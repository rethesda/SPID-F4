#include "LookupNPC.h"
#include "ExclusiveGroups.h"

namespace NPC
{
	Data::ID::ID(const RE::TESForm* a_base) :
		formID(a_base->GetFormID()),
		editorID(a_base->GetFormEditorID())
	{}

	bool Data::ID::contains(const std::string& a_str) const
	{
		return string::icontains(editorID, a_str);
	}

	bool Data::ID::operator==(const RE::TESFile* a_mod) const
	{
		return a_mod->IsFormInMod(formID);
	}

	bool Data::ID::operator==(const std::string& a_str) const
	{
		return string::iequals(editorID, a_str);
	}

	bool Data::ID::operator==(RE::FormID a_formID) const
	{
		return formID == a_formID;
	}

	Data::Data(RE::Actor* a_actor, RE::TESNPC* a_npc) :
		npc(a_npc),
		actor(a_actor),
		race(a_actor->race),
		name(a_npc->GetFullName()),
		level(a_npc->GetLevel()),
		child(a_actor->IsChild() || race && race->formEditorID.contains("RaceChild")),
		leveled(a_actor->IsLeveledCreature())
	{
		npc->ForEachKeyword([&](const RE::BGSKeyword* a_keyword) {
			keywords.emplace(a_keyword->GetFormEditorID());
			return RE::BSContainer::ForEachResult::kContinue;
		});

		if (npc->baseTemplateForm) {
			IDs.emplace_back(npc->baseTemplateForm);
		}

		if (const auto extraLvlCreature = a_actor->extraList->GetByType<RE::ExtraLeveledCreature>()) {
			if (const auto originalBase = extraLvlCreature->originalBase) {
				IDs.emplace_back(originalBase);
			}
			for (std::uint32_t i = 0; i < 13; i++) {
				if (auto templateBase = extraLvlCreature->templates[i]) {
					IDs.emplace_back(templateBase);
				}
			}
		} else {
			IDs.emplace_back(npc);
		}

		if (race) {
			race->ForEachKeyword([&](const RE::BGSKeyword* a_keyword) {
				keywords.emplace(a_keyword->GetFormEditorID());
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	RE::Actor* Data::GetActor() const
	{
		return actor;
	}

	bool Data::has_keyword_string(const std::string& a_string) const
	{
		return std::any_of(keywords.begin(), keywords.end(), [&](const auto& keyword) {
			return string::iequals(keyword, a_string);
		});
	}

	bool Data::HasStringFilter(const StringVec& a_strings, bool a_all) const
	{
		if (a_all) {
			return std::ranges::all_of(a_strings, [&](const auto& str) {
				return has_keyword_string(str) || string::iequals(name, str) || std::ranges::any_of(IDs, [&](const auto& ID) { return ID == str; });
			});
		} else {
			return std::ranges::any_of(a_strings, [&](const auto& str) {
				return has_keyword_string(str) || string::iequals(name, str) || std::ranges::any_of(IDs, [&](const auto& ID) { return ID == str; });
			});
		}
	}

	bool Data::ContainsStringFilter(const StringVec& a_strings) const
	{
		return std::ranges::any_of(a_strings, [&](const auto& str) {
			return string::icontains(name, str) ||
			       std::ranges::any_of(IDs, [&](const auto& ID) { return ID.contains(str); }) ||
			       std::any_of(keywords.begin(), keywords.end(), [&](const auto& keyword) {
					   return string::icontains(keyword, str);
				   });
		});
	}

	bool Data::InsertKeyword(const char* a_keyword)
	{
		return keywords.emplace(a_keyword).second;
	}

	bool Data::has_form(RE::TESForm* a_form) const
	{
		switch (a_form->GetFormType()) {
		case RE::FormType::kCSTY:
			return npc->GetCombatStyle() == a_form;
		case RE::FormType::kCLAS:
			return npc->cl == a_form;
		case RE::FormType::kFACT:
			{
				const auto faction = a_form->As<RE::TESFaction>();
				return npc->IsInFaction(faction);
			}
		case RE::FormType::kRACE:
			return GetRace() == a_form;
		case RE::FormType::kOTFT:
			return npc->defOutfit == a_form;
		case RE::FormType::kNPC_:
			return npc == a_form || std::ranges::any_of(IDs, [&](const auto& ID) { return ID == a_form->GetFormID(); });
		case RE::FormType::kVTYP:
			return npc->voiceType == a_form;
		case RE::FormType::kSPEL:
			{
				const auto spell = a_form->As<RE::SpellItem>();
				return npc->GetSpellList()->GetIndex(spell).has_value();
			}
		case RE::FormType::kARMO:
			return npc->formSkin == a_form;
		case RE::FormType::kLCTN:
			{
				const auto location = a_form->As<RE::BGSLocation>();
				return actor->GetEditorLocation() == location;
			}
		case RE::FormType::kPERK:
			{
				const auto perk = a_form->As<RE::BGSPerk>();
				return actor->GetPerkRank(perk) > 0;
			}
		case RE::FormType::kFLST:
			{
				bool result = false;

				const auto list = a_form->As<RE::BGSListForm>();
				list->ForEachForm([&](RE::TESForm* a_formInList) {
					if (result = has_form(a_formInList); result) {
						return RE::BSContainer::ForEachResult::kStop;
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});

				return result;
			}
		default:
			return false;
		}
	}

	bool Data::HasFormFilter(const FormVec& a_forms, bool all) const
	{
		const auto has_form_or_file = [&](const std::variant<RE::TESForm*, const RE::TESFile*>& a_formFile) {
			bool result = false;
			std::visit(overload{
						   [&](RE::TESForm* a_form) {
							   result = has_form(a_form);
						   },
						   [&](const RE::TESFile* a_file) {
							   result = std::ranges::any_of(IDs, [&](const auto& ID) { return ID == a_file; });
						   } },
				a_formFile);
			return result;
		};

		if (all) {
			return std::ranges::all_of(a_forms, has_form_or_file);
		} else {
			return std::ranges::any_of(a_forms, has_form_or_file);
		}
	}

	bool Data::HasMutuallyExclusiveForm(RE::TESForm* a_form) const
	{
		auto excludedForms = ExclusiveGroups::Manager::GetSingleton()->MutuallyExclusiveFormsForForm(a_form);
		if (excludedForms.empty()) {
			return false;
		}
		return std::ranges::any_of(excludedForms, [&](auto form) {
			if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
				return has_keyword_string(keyword->GetFormEditorID());
			}
			return has_form(form);
		});
	}

	std::uint16_t Data::GetLevel() const
	{
		return level;
	}

	bool Data::IsChild() const
	{
		return child;
	}

	bool Data::IsLeveled() const
	{
		return leveled;
	}

	RE::TESRace* Data::GetRace() const
	{
		return race;
	}
}

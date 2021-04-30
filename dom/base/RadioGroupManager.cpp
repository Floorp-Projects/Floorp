/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RadioGroupManager.h"
#include "nsIRadioVisitor.h"
#include "mozilla/dom/HTMLInputElement.h"

namespace mozilla::dom {

/**
 * A struct that holds all the information about a radio group.
 */
struct nsRadioGroupStruct {
  nsRadioGroupStruct()
      : mRequiredRadioCount(0), mGroupSuffersFromValueMissing(false) {}

  /**
   * A strong pointer to the currently selected radio button.
   */
  RefPtr<HTMLInputElement> mSelectedRadioButton;
  nsCOMArray<nsIFormControl> mRadioButtons;
  uint32_t mRequiredRadioCount;
  bool mGroupSuffersFromValueMissing;
};

RadioGroupManager::RadioGroupManager() = default;

void RadioGroupManager::Traverse(RadioGroupManager* tmp,
                                 nsCycleCollectionTraversalCallback& cb) {
  for (const auto& entry : tmp->mRadioGroups) {
    nsRadioGroupStruct* radioGroup = entry.GetWeak();
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
        cb, "mRadioGroups entry->mSelectedRadioButton");
    cb.NoteXPCOMChild(ToSupports(radioGroup->mSelectedRadioButton));

    uint32_t i, count = radioGroup->mRadioButtons.Count();
    for (i = 0; i < count; ++i) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
          cb, "mRadioGroups entry->mRadioButtons[i]");
      cb.NoteXPCOMChild(radioGroup->mRadioButtons[i]);
    }
  }
}

void RadioGroupManager::Unlink(RadioGroupManager* tmp) {
  tmp->mRadioGroups.Clear();
}

nsresult RadioGroupManager::WalkRadioGroup(const nsAString& aName,
                                           nsIRadioVisitor* aVisitor) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  for (int i = 0; i < radioGroup->mRadioButtons.Count(); i++) {
    if (!aVisitor->Visit(radioGroup->mRadioButtons[i])) {
      return NS_OK;
    }
  }

  return NS_OK;
}

void RadioGroupManager::SetCurrentRadioButton(const nsAString& aName,
                                              HTMLInputElement* aRadio) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mSelectedRadioButton = aRadio;
}

HTMLInputElement* RadioGroupManager::GetCurrentRadioButton(
    const nsAString& aName) {
  return GetOrCreateRadioGroup(aName)->mSelectedRadioButton;
}

nsresult RadioGroupManager::GetNextRadioButton(const nsAString& aName,
                                               const bool aPrevious,
                                               HTMLInputElement* aFocusedRadio,
                                               HTMLInputElement** aRadioOut) {
  *aRadioOut = nullptr;

  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  // Return the radio button relative to the focused radio button.
  // If no radio is focused, get the radio relative to the selected one.
  RefPtr<HTMLInputElement> currentRadio;
  if (aFocusedRadio) {
    currentRadio = aFocusedRadio;
  } else {
    currentRadio = radioGroup->mSelectedRadioButton;
    if (!currentRadio) {
      return NS_ERROR_FAILURE;
    }
  }
  int32_t index = radioGroup->mRadioButtons.IndexOf(currentRadio);
  if (index < 0) {
    return NS_ERROR_FAILURE;
  }

  int32_t numRadios = radioGroup->mRadioButtons.Count();
  RefPtr<HTMLInputElement> radio;
  do {
    if (aPrevious) {
      if (--index < 0) {
        index = numRadios - 1;
      }
    } else if (++index >= numRadios) {
      index = 0;
    }
    MOZ_ASSERT(
        static_cast<nsGenericHTMLFormElement*>(radioGroup->mRadioButtons[index])
            ->IsHTMLElement(nsGkAtoms::input),
        "mRadioButtons holding a non-radio button");
    radio = static_cast<HTMLInputElement*>(radioGroup->mRadioButtons[index]);
  } while (radio->Disabled() && radio != currentRadio);

  radio.forget(aRadioOut);
  return NS_OK;
}

void RadioGroupManager::AddToRadioGroup(const nsAString& aName,
                                        HTMLInputElement* aRadio) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mRadioButtons.AppendObject(aRadio);

  if (aRadio->IsRequired()) {
    radioGroup->mRequiredRadioCount++;
  }
}

void RadioGroupManager::RemoveFromRadioGroup(const nsAString& aName,
                                             HTMLInputElement* aRadio) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mRadioButtons.RemoveObject(aRadio);

  if (aRadio->IsRequired()) {
    MOZ_ASSERT(radioGroup->mRequiredRadioCount != 0,
               "mRequiredRadioCount about to wrap below 0!");
    radioGroup->mRequiredRadioCount--;
  }
}

uint32_t RadioGroupManager::GetRequiredRadioCount(
    const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = GetRadioGroup(aName);
  return radioGroup ? radioGroup->mRequiredRadioCount : 0;
}

void RadioGroupManager::RadioRequiredWillChange(const nsAString& aName,
                                                bool aRequiredAdded) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  if (aRequiredAdded) {
    radioGroup->mRequiredRadioCount++;
  } else {
    MOZ_ASSERT(radioGroup->mRequiredRadioCount != 0,
               "mRequiredRadioCount about to wrap below 0!");
    radioGroup->mRequiredRadioCount--;
  }
}

bool RadioGroupManager::GetValueMissingState(const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = GetRadioGroup(aName);
  return radioGroup && radioGroup->mGroupSuffersFromValueMissing;
}

void RadioGroupManager::SetValueMissingState(const nsAString& aName,
                                             bool aValue) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mGroupSuffersFromValueMissing = aValue;
}

nsRadioGroupStruct* RadioGroupManager::GetRadioGroup(
    const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = nullptr;
  mRadioGroups.Get(aName, &radioGroup);
  return radioGroup;
}

nsRadioGroupStruct* RadioGroupManager::GetOrCreateRadioGroup(
    const nsAString& aName) {
  return mRadioGroups.GetOrInsertNew(aName);
}

}  // namespace mozilla::dom

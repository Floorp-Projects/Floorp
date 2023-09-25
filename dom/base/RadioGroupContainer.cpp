/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/RadioGroupContainer.h"
#include "mozilla/Assertions.h"
#include "nsIRadioVisitor.h"
#include "nsRadioVisitor.h"

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
  nsTArray<RefPtr<HTMLInputElement>> mRadioButtons;
  uint32_t mRequiredRadioCount;
  bool mGroupSuffersFromValueMissing;
};

RadioGroupContainer::RadioGroupContainer() = default;

RadioGroupContainer::~RadioGroupContainer() {
  for (const auto& group : mRadioGroups) {
    for (const auto& button : group.GetData()->mRadioButtons) {
      // When the radio group container is being cycle-collected, any remaining
      // connected buttons will also be in the process of being cycle-collected.
      // Here, we unset the button's reference to the container so that when it
      // is collected it does not attempt to remove itself from a potentially
      // already deleted radio group.
      button->DisconnectRadioGroupContainer();
    }
  }
}

/* static */
void RadioGroupContainer::Traverse(RadioGroupContainer* tmp,
                                   nsCycleCollectionTraversalCallback& cb) {
  for (const auto& entry : tmp->mRadioGroups) {
    nsRadioGroupStruct* radioGroup = entry.GetWeak();
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
        cb, "mRadioGroups entry->mSelectedRadioButton");
    cb.NoteXPCOMChild(ToSupports(radioGroup->mSelectedRadioButton));

    uint32_t i, count = radioGroup->mRadioButtons.Length();
    for (i = 0; i < count; ++i) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
          cb, "mRadioGroups entry->mRadioButtons[i]");
      cb.NoteXPCOMChild(ToSupports(radioGroup->mRadioButtons[i]));
    }
  }
}

size_t RadioGroupContainer::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return mRadioGroups.SizeOfIncludingThis(aMallocSizeOf);
}

nsresult RadioGroupContainer::WalkRadioGroup(const nsAString& aName,
                                             nsIRadioVisitor* aVisitor) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  for (size_t i = 0; i < radioGroup->mRadioButtons.Length(); i++) {
    if (!aVisitor->Visit(radioGroup->mRadioButtons[i])) {
      return NS_OK;
    }
  }

  return NS_OK;
}

void RadioGroupContainer::SetCurrentRadioButton(const nsAString& aName,
                                                HTMLInputElement* aRadio) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mSelectedRadioButton = aRadio;
}

HTMLInputElement* RadioGroupContainer::GetCurrentRadioButton(
    const nsAString& aName) {
  return GetOrCreateRadioGroup(aName)->mSelectedRadioButton;
}

nsresult RadioGroupContainer::GetNextRadioButton(
    const nsAString& aName, const bool aPrevious,
    HTMLInputElement* aFocusedRadio, HTMLInputElement** aRadioOut) {
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

  int32_t numRadios = static_cast<int32_t>(radioGroup->mRadioButtons.Length());
  RefPtr<HTMLInputElement> radio;
  do {
    if (aPrevious) {
      if (--index < 0) {
        index = numRadios - 1;
      }
    } else if (++index >= numRadios) {
      index = 0;
    }
    radio = radioGroup->mRadioButtons[index];
  } while (radio->Disabled() && radio != currentRadio);

  radio.forget(aRadioOut);
  return NS_OK;
}

void RadioGroupContainer::AddToRadioGroup(const nsAString& aName,
                                          HTMLInputElement* aRadio,
                                          nsIContent* aAncestor) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  nsContentUtils::AddElementToListByTreeOrder(radioGroup->mRadioButtons, aRadio,
                                              aAncestor);

  if (aRadio->IsRequired()) {
    radioGroup->mRequiredRadioCount++;
  }
}

void RadioGroupContainer::RemoveFromRadioGroup(const nsAString& aName,
                                               HTMLInputElement* aRadio) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  MOZ_ASSERT(
      radioGroup->mRadioButtons.Contains(aRadio),
      "Attempting to remove radio button from group it is not a part of!");

  radioGroup->mRadioButtons.RemoveElement(aRadio);

  if (aRadio->IsRequired()) {
    MOZ_ASSERT(radioGroup->mRequiredRadioCount != 0,
               "mRequiredRadioCount about to wrap below 0!");
    radioGroup->mRequiredRadioCount--;
  }
}

uint32_t RadioGroupContainer::GetRequiredRadioCount(
    const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = GetRadioGroup(aName);
  return radioGroup ? radioGroup->mRequiredRadioCount : 0;
}

void RadioGroupContainer::RadioRequiredWillChange(const nsAString& aName,
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

bool RadioGroupContainer::GetValueMissingState(const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = GetRadioGroup(aName);
  return radioGroup && radioGroup->mGroupSuffersFromValueMissing;
}

void RadioGroupContainer::SetValueMissingState(const nsAString& aName,
                                               bool aValue) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mGroupSuffersFromValueMissing = aValue;
}

nsRadioGroupStruct* RadioGroupContainer::GetRadioGroup(
    const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = nullptr;
  mRadioGroups.Get(aName, &radioGroup);
  return radioGroup;
}

nsRadioGroupStruct* RadioGroupContainer::GetOrCreateRadioGroup(
    const nsAString& aName) {
  return mRadioGroups.GetOrInsertNew(aName);
}

}  // namespace mozilla::dom

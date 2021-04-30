/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsRadioGroupStruct_h
#define mozilla_dom_nsRadioGroupStruct_h

#include "nsCOMArray.h"
#include "nsIFormControl.h"
#include "nsIRadioGroupContainer.h"
#include "nsClassHashtable.h"

namespace mozilla {

namespace html {
class nsIRadioVisitor;
}

namespace dom {
class HTMLInputElement;
struct nsRadioGroupStruct;

class RadioGroupManager {
 public:
  RadioGroupManager();

  static void Traverse(RadioGroupManager* tmp,
                       nsCycleCollectionTraversalCallback& cb);
  static void Unlink(RadioGroupManager* tmp);

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor);
  void SetCurrentRadioButton(const nsAString& aName, HTMLInputElement* aRadio);
  HTMLInputElement* GetCurrentRadioButton(const nsAString& aName);
  nsresult GetNextRadioButton(const nsAString& aName, const bool aPrevious,
                              HTMLInputElement* aFocusedRadio,
                              HTMLInputElement** aRadioOut);
  void AddToRadioGroup(const nsAString& aName, HTMLInputElement* aRadio);
  void RemoveFromRadioGroup(const nsAString& aName, HTMLInputElement* aRadio);
  uint32_t GetRequiredRadioCount(const nsAString& aName) const;
  void RadioRequiredWillChange(const nsAString& aName, bool aRequiredAdded);
  bool GetValueMissingState(const nsAString& aName) const;
  void SetValueMissingState(const nsAString& aName, bool aValue);

  // for radio group
  nsRadioGroupStruct* GetRadioGroup(const nsAString& aName) const;
  nsRadioGroupStruct* GetOrCreateRadioGroup(const nsAString& aName);

 private:
  nsClassHashtable<nsStringHashKey, nsRadioGroupStruct> mRadioGroups;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_nsRadioGroupStruct_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ACCESSIBLE_WINDOWS_IA2_ACCESSIBLETEXTSELECTIONCONTAINER_H_
#define ACCESSIBLE_WINDOWS_IA2_ACCESSIBLETEXTSELECTIONCONTAINER_H_

#include "AccessibleTextSelectionContainer.h"
#include "mozilla/Attributes.h"

template <class T>
class RefPtr;

namespace mozilla::a11y {
class Accessible;
class HyperTextAccessibleBase;
class TextLeafPoint;

class ia2AccessibleTextSelectionContainer
    : public IAccessibleTextSelectionContainer {
 public:
  // IAccessibleTextSelectionContainer
  /* [propget] */ HRESULT STDMETHODCALLTYPE get_selections(
      /* [out, size_is(,*nSelections)] */ IA2TextSelection** selections,
      /* [out, retval] */ long* nSelections) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  HRESULT STDMETHODCALLTYPE setSelections(
      /* [in] */ long nSelections,
      /* [in, size_is(nSelections)] */ IA2TextSelection* selections) override;

 private:
  HyperTextAccessibleBase* TextAcc();
  static RefPtr<IAccessibleText> GetIATextFrom(Accessible* aAcc);
  static TextLeafPoint GetTextLeafPointFrom(IAccessibleText* aText,
                                            long aOffset, bool aDescendToEnd);
};

}  // namespace mozilla::a11y

#endif

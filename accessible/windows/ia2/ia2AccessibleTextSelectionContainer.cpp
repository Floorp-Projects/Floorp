/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleTextSelectionContainer.h"

#include "AccessibleTextSelectionContainer_i.c"
#include "ia2AccessibleHypertext.h"
#include "mozilla/a11y/HyperTextAccessibleBase.h"
#include "TextRange.h"
#include "TextLeafRange.h"

using namespace mozilla::a11y;

// IAccessibleTextSelectionContainer

STDMETHODIMP
ia2AccessibleTextSelectionContainer::get_selections(
    IA2TextSelection** selections, long* nSelections) {
  if (!selections || !nSelections) {
    return E_INVALIDARG;
  }
  *nSelections = 0;
  HyperTextAccessibleBase* text = TextAcc();
  if (!text) {
    return CO_E_OBJNOTCONNECTED;
  }
  AutoTArray<TextRange, 1> ranges;
  text->CroppedSelectionRanges(ranges);
  *nSelections = ranges.Length();
  *selections = static_cast<IA2TextSelection*>(
      ::CoTaskMemAlloc(sizeof(IA2TextSelection) * *nSelections));
  if (!*selections) {
    return E_OUTOFMEMORY;
  }
  for (uint32_t idx = 0; idx < static_cast<uint32_t>(*nSelections); idx++) {
    RefPtr<IAccessibleText> startObj =
        GetIATextFrom(ranges[idx].StartContainer());
    startObj.forget(&(*selections)[idx].startObj);
    (*selections)[idx].startOffset = ranges[idx].StartOffset();
    RefPtr<IAccessibleText> endObj = GetIATextFrom(ranges[idx].EndContainer());
    endObj.forget(&(*selections)[idx].endObj);
    (*selections)[idx].endOffset = ranges[idx].EndOffset();
    // XXX Expose this properly somehow.
    (*selections)[idx].startIsActive = true;
  }
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTextSelectionContainer::setSelections(
    long nSelections, IA2TextSelection* selections) {
  if (nSelections < 0 || (nSelections > 0 && !selections)) {
    return E_INVALIDARG;
  }
  HyperTextAccessibleBase* text = TextAcc();
  if (!text) {
    return CO_E_OBJNOTCONNECTED;
  }
  // Build and validate new selection ranges.
  AutoTArray<TextLeafRange, 1> newRanges;
  newRanges.SetCapacity(nSelections);
  for (long r = 0; r < nSelections; ++r) {
    TextLeafRange range(GetTextLeafPointFrom(selections[r].startObj,
                                             selections[r].startOffset, false),
                        GetTextLeafPointFrom(selections[r].endObj,
                                             selections[r].endOffset, true));
    if (!range) {
      return E_INVALIDARG;
    }
    newRanges.AppendElement(range);
  }
  // Get the number of existing selections. We use SelectionRanges rather than
  // SelectionCount because SelectionCount is restricted to this Accessible,
  // whereas we want all selections within the control/document.
  AutoTArray<TextRange, 1> oldRanges;
  text->SelectionRanges(&oldRanges);
  // Set the new selections.
  for (long r = 0; r < nSelections; ++r) {
    newRanges[r].SetSelection(r);
  }
  // Remove any remaining old selections if there were more than nSelections.
  for (long r = nSelections; r < static_cast<long>(oldRanges.Length()); ++r) {
    text->RemoveFromSelection(r);
  }
  return S_OK;
}

// ia2AccessibleTextSelectionContainer

HyperTextAccessibleBase* ia2AccessibleTextSelectionContainer::TextAcc() {
  auto hyp = static_cast<ia2AccessibleHypertext*>(this);
  Accessible* acc = hyp->Acc();
  return acc ? acc->AsHyperTextBase() : nullptr;
}

/* static */
RefPtr<IAccessibleText> ia2AccessibleTextSelectionContainer::GetIATextFrom(
    Accessible* aAcc) {
  MsaaAccessible* msaa = MsaaAccessible::GetFrom(aAcc);
  MOZ_ASSERT(msaa);
  RefPtr<IAccessibleText> text;
  msaa->QueryInterface(IID_IAccessibleText, getter_AddRefs(text));
  MOZ_ASSERT(text);
  return text;
}

/* static */
TextLeafPoint ia2AccessibleTextSelectionContainer::GetTextLeafPointFrom(
    IAccessibleText* aText, long aOffset, bool aDescendToEnd) {
  if (!aText) {
    return TextLeafPoint();
  }
  Accessible* acc = MsaaAccessible::GetAccessibleFrom(aText);
  if (!acc) {
    return TextLeafPoint();
  }
  HyperTextAccessibleBase* hyp = acc->AsHyperTextBase();
  if (!hyp) {
    return TextLeafPoint();
  }
  return hyp->ToTextLeafPoint(aOffset, aDescendToEnd);
}

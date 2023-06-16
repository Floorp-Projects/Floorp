/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleEditableText.h"
#include "ia2AccessibleHypertext.h"

#include "AccessibleEditableText_i.c"
#include "mozilla/a11y/HyperTextAccessibleBase.h"

#include "nsCOMPtr.h"
#include "nsString.h"

using namespace mozilla::a11y;

HyperTextAccessibleBase* ia2AccessibleEditableText::TextAcc() {
  auto hyp = static_cast<ia2AccessibleHypertext*>(this);
  Accessible* acc = hyp->Acc();
  return acc ? acc->AsHyperTextBase() : nullptr;
}

// IAccessibleEditableText

STDMETHODIMP
ia2AccessibleEditableText::copyText(long aStartOffset, long aEndOffset) {
  HyperTextAccessibleBase* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset)) return E_INVALIDARG;

  textAcc->CopyText(aStartOffset, aEndOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::deleteText(long aStartOffset, long aEndOffset) {
  HyperTextAccessibleBase* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset)) return E_INVALIDARG;

  textAcc->DeleteText(aStartOffset, aEndOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::insertText(long aOffset, BSTR* aText) {
  uint32_t length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);

  HyperTextAccessibleBase* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidOffset(aOffset)) return E_INVALIDARG;

  textAcc->InsertText(text, aOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::cutText(long aStartOffset, long aEndOffset) {
  HyperTextAccessibleBase* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset)) return E_INVALIDARG;

  textAcc->CutText(aStartOffset, aEndOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::pasteText(long aOffset) {
  HyperTextAccessibleBase* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidOffset(aOffset)) return E_INVALIDARG;

  textAcc->PasteText(aOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::replaceText(long aStartOffset, long aEndOffset,
                                       BSTR* aText) {
  HyperTextAccessibleBase* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset)) return E_INVALIDARG;

  textAcc->DeleteText(aStartOffset, aEndOffset);

  uint32_t length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);
  textAcc->InsertText(text, aStartOffset);

  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::setAttributes(long aStartOffset, long aEndOffset,
                                         BSTR* aAttributes) {
  return E_NOTIMPL;
}

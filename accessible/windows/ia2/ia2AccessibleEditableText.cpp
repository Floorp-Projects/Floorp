/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleEditableText.h"
#include "ia2AccessibleHypertext.h"

#include "AccessibleEditableText_i.c"
#include "HyperTextAccessible-inl.h"
#include "HyperTextAccessibleWrap.h"
#include "ProxyWrappers.h"

#include "nsCOMPtr.h"
#include "nsString.h"

using namespace mozilla::a11y;

HyperTextAccessibleWrap* ia2AccessibleEditableText::TextAcc() {
  auto hyp = static_cast<ia2AccessibleHypertext*>(this);
  AccessibleWrap* acc = static_cast<MsaaAccessible*>(hyp)->LocalAcc();
  return static_cast<HyperTextAccessibleWrap*>(acc);
}

// IAccessibleEditableText

STDMETHODIMP
ia2AccessibleEditableText::copyText(long aStartOffset, long aEndOffset) {
  HyperTextAccessible* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;
  MOZ_ASSERT(!textAcc->IsProxy());

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset)) return E_INVALIDARG;

  textAcc->CopyText(aStartOffset, aEndOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::deleteText(long aStartOffset, long aEndOffset) {
  HyperTextAccessible* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;
  MOZ_ASSERT(!textAcc->IsProxy());

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset)) return E_INVALIDARG;

  textAcc->DeleteText(aStartOffset, aEndOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::insertText(long aOffset, BSTR* aText) {
  uint32_t length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);

  HyperTextAccessible* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;
  MOZ_ASSERT(!textAcc->IsProxy());

  if (!textAcc->IsValidOffset(aOffset)) return E_INVALIDARG;

  textAcc->InsertText(text, aOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::cutText(long aStartOffset, long aEndOffset) {
  HyperTextAccessible* textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;
  MOZ_ASSERT(!textAcc->IsProxy());

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset)) return E_INVALIDARG;

  textAcc->CutText(aStartOffset, aEndOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::pasteText(long aOffset) {
  RefPtr<HyperTextAccessible> textAcc = TextAcc();
  if (!textAcc) return CO_E_OBJNOTCONNECTED;
  MOZ_ASSERT(!textAcc->IsProxy());

  if (!textAcc->IsValidOffset(aOffset)) return E_INVALIDARG;

  textAcc->PasteText(aOffset);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleEditableText::replaceText(long aStartOffset, long aEndOffset,
                                       BSTR* aText) {
  HyperTextAccessible* textAcc = TextAcc();
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

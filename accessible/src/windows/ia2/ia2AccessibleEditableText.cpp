/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleEditableText.h"

#include "AccessibleEditableText_i.c"
#include "HyperTextAccessible-inl.h"
#include "HyperTextAccessibleWrap.h"

#include "nsCOMPtr.h"
#include "nsString.h"

using namespace mozilla::a11y;

// IAccessibleEditableText

STDMETHODIMP
ia2AccessibleEditableText::copyText(long aStartOffset, long aEndOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset))
    return E_INVALIDARG;

  textAcc->CopyText(aStartOffset, aEndOffset);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::deleteText(long aStartOffset, long aEndOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset))
    return E_INVALIDARG;

  textAcc->DeleteText(aStartOffset, aEndOffset);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::insertText(long aOffset, BSTR *aText)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidOffset(aOffset))
    return E_INVALIDARG;

  uint32_t length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);

  textAcc->InsertText(text, aOffset);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::cutText(long aStartOffset, long aEndOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset))
    return E_INVALIDARG;

  textAcc->CutText(aStartOffset, aEndOffset);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::pasteText(long aOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidOffset(aOffset))
    return E_INVALIDARG;

  textAcc->PasteText(aOffset);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::replaceText(long aStartOffset, long aEndOffset,
                                       BSTR *aText)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!textAcc->IsValidRange(aStartOffset, aEndOffset))
    return E_INVALIDARG;

  textAcc->DeleteText(aStartOffset, aEndOffset);

  uint32_t length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);
  textAcc->InsertText(text, aStartOffset);

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::setAttributes(long aStartOffset, long aEndOffset,
                                         BSTR *aAttributes)
{
  return E_NOTIMPL;
}

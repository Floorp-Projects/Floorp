/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleEditableText.h"

#include "AccessibleEditableText_i.c"
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

  nsresult rv = textAcc->CopyText(aStartOffset, aEndOffset);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::deleteText(long aStartOffset, long aEndOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->DeleteText(aStartOffset, aEndOffset);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::insertText(long aOffset, BSTR *aText)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  uint32_t length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);

  nsresult rv = textAcc->InsertText(text, aOffset);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::cutText(long aStartOffset, long aEndOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->CutText(aStartOffset, aEndOffset);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::pasteText(long aOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->PasteText(aOffset);
  return GetHRESULT(rv);

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

  nsresult rv = textAcc->DeleteText(aStartOffset, aEndOffset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  uint32_t length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);

  rv = textAcc->InsertText(text, aStartOffset);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleEditableText::setAttributes(long aStartOffset, long aEndOffset,
                                         BSTR *aAttributes)
{
  return E_NOTIMPL;
}

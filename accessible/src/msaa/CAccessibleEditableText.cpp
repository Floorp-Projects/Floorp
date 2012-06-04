/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CAccessibleEditableText.h"

#include "AccessibleEditableText_i.c"
#include "HyperTextAccessible.h"

#include "nsCOMPtr.h"
#include "nsString.h"

// IUnknown

STDMETHODIMP
CAccessibleEditableText::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleEditableText == iid) {
    nsCOMPtr<nsIAccessibleEditableText> editTextAcc(do_QueryObject(this));
    if (!editTextAcc)
      return E_NOINTERFACE;
    *ppv = static_cast<IAccessibleEditableText*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef(); 
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleEditableText

STDMETHODIMP
CAccessibleEditableText::copyText(long aStartOffset, long aEndOffset)
{
__try {
  nsRefPtr<HyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->CopyText(aStartOffset, aEndOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleEditableText::deleteText(long aStartOffset, long aEndOffset)
{
__try {
  nsRefPtr<HyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->DeleteText(aStartOffset, aEndOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleEditableText::insertText(long aOffset, BSTR *aText)
{
__try {
  nsRefPtr<HyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRUint32 length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);

  nsresult rv = textAcc->InsertText(text, aOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleEditableText::cutText(long aStartOffset, long aEndOffset)
{
__try {
  nsRefPtr<HyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->CutText(aStartOffset, aEndOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleEditableText::pasteText(long aOffset)
{
__try {
  nsRefPtr<HyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->PasteText(aOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleEditableText::replaceText(long aStartOffset, long aEndOffset,
                                     BSTR *aText)
{
__try {
  nsRefPtr<HyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->DeleteText(aStartOffset, aEndOffset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  PRUint32 length = ::SysStringLen(*aText);
  nsAutoString text(*aText, length);

  rv = textAcc->InsertText(text, aStartOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleEditableText::setAttributes(long aStartOffset, long aEndOffset,
                                       BSTR *aAttributes)
{
__try {

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_NOTIMPL;
}

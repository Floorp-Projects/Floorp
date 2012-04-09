/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CAccessibleEditableText.h"

#include "AccessibleEditableText_i.c"
#include "nsHyperTextAccessible.h"

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
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
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
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
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
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
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
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
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
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
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
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
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

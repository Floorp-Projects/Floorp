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

#include "CAccessibleHypertext.h"

#include "AccessibleHypertext_i.c"

#include "nsIAccessibleHypertext.h"
#include "nsIWinAccessNode.h"
#include "nsAccessNodeWrap.h"

#include "nsCOMPtr.h"

// IUnknown

STDMETHODIMP
CAccessibleHypertext::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;
  if (IID_IAccessibleHypertext == iid) {
    nsCOMPtr<nsIAccessibleHyperText> hyperAcc(do_QueryInterface(this));
    if (!hyperAcc)
      return E_NOINTERFACE;

    *ppv = static_cast<IAccessibleHypertext*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return CAccessibleText::QueryInterface(iid, ppv);
}

// IAccessibleHypertext

STDMETHODIMP
CAccessibleHypertext::get_nHyperlinks(long *aHyperlinkCount)
{
__try {
  *aHyperlinkCount = 0;

  nsCOMPtr<nsIAccessibleHyperText> hyperAcc(do_QueryInterface(this));
  if (!hyperAcc)
    return E_FAIL;

  PRInt32 count = 0;
  nsresult rv = hyperAcc->GetLinkCount(&count);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aHyperlinkCount = count;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleHypertext::get_hyperlink(long aLinkIndex,
                                    IAccessibleHyperlink **aHyperlink)
{
__try {
  *aHyperlink = NULL;

  nsCOMPtr<nsIAccessibleHyperText> hyperAcc(do_QueryInterface(this));
  if (!hyperAcc)
    return E_FAIL;

  nsCOMPtr<nsIAccessibleHyperLink> hyperLink;
  nsresult rv = hyperAcc->GetLink(aLinkIndex, getter_AddRefs(hyperLink));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(hyperLink));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  rv =  winAccessNode->QueryNativeInterface(IID_IAccessibleHyperlink,
                                            &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aHyperlink = static_cast<IAccessibleHyperlink*>(instancePtr);
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleHypertext::get_hyperlinkIndex(long aCharIndex, long *aHyperlinkIndex)
{
__try {
  *aHyperlinkIndex = 0;

  nsCOMPtr<nsIAccessibleHyperText> hyperAcc(do_QueryInterface(this));
  if (!hyperAcc)
    return E_FAIL;

  PRInt32 index = 0;
  nsresult rv = hyperAcc->GetLinkIndex(aCharIndex, &index);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aHyperlinkIndex = index;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}


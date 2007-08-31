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

#include "CAccessibleHyperlink.h"

#include "Accessible2.h"
#include "AccessibleHyperlink.h"
#include "AccessibleHyperlink_i.c"

#include "nsIAccessible.h"
#include "nsIAccessibleHyperlink.h"
#include "nsIWinAccessNode.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIURI.h"

// IUnknown

STDMETHODIMP
CAccessibleHyperlink::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleHyperlink == iid) {
    nsCOMPtr<nsIAccessibleHyperLink> acc(do_QueryInterface(this));
    if (!acc)
      return E_NOINTERFACE;

    *ppv = static_cast<IAccessibleHyperlink*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return CAccessibleAction::QueryInterface(iid, ppv);
}

// IAccessibleHyperlink

STDMETHODIMP
CAccessibleHyperlink::get_anchor(long aIndex, VARIANT *aAnchor)
{
  VariantInit(aAnchor);

  nsCOMPtr<nsIAccessibleHyperLink> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  nsCOMPtr<nsIAccessible> anchor;
  acc->GetObject(aIndex, getter_AddRefs(anchor));
  if (!anchor)
    return E_FAIL;

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(anchor));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  nsresult rv =  winAccessNode->QueryNativeInterface(IID_IUnknown,
                                                     &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  IUnknown *unknownPtr = static_cast<IUnknown*>(instancePtr);
  aAnchor->ppunkVal = &unknownPtr;
  aAnchor->vt = VT_UNKNOWN;

  return S_OK;
}

STDMETHODIMP
CAccessibleHyperlink::get_anchorTarget(long aIndex, VARIANT *aAnchorTarget)
{
  VariantInit(aAnchorTarget);

  nsCOMPtr<nsIAccessibleHyperLink> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = acc->GetURI(aIndex, getter_AddRefs(uri));
  if (NS_SUCCEEDED(rv))
    return E_FAIL;

  nsCAutoString prePath;
  rv = uri->GetPrePath(prePath);
  if (NS_SUCCEEDED(rv))
    return E_FAIL;

  nsCAutoString path;
  rv = uri->GetPath(path);
  if (NS_SUCCEEDED(rv))
    return E_FAIL;

  nsAutoString stringURI;
  AppendUTF8toUTF16(prePath, stringURI);
  AppendUTF8toUTF16(path, stringURI);

  aAnchorTarget->vt = VT_BSTR;
  INT result = ::SysReAllocStringLen(&aAnchorTarget->bstrVal, stringURI.get(),
                                     stringURI.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
CAccessibleHyperlink::get_startIndex(long *aIndex)
{
  *aIndex = 0;

  nsCOMPtr<nsIAccessibleHyperLink> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  PRInt32 index = 0;
  nsresult rv = acc->GetStartIndex(&index);
  *aIndex = index;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleHyperlink::get_endIndex(long *aIndex)
{
  *aIndex = 0;

  nsCOMPtr<nsIAccessibleHyperLink> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  PRInt32 index = 0;
  nsresult rv = acc->GetEndIndex(&index);
  *aIndex = index;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleHyperlink::get_valid(boolean *aValid)
{
  nsCOMPtr<nsIAccessibleHyperLink> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  PRBool isValid = PR_FALSE;
  nsresult rv = acc->IsValid(&isValid);
  *aValid = isValid;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDocAccessibleWrap.h"
#include "ISimpleDOMDocument_i.c"
#include "nsIAccessibilityService.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeNode.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISelectionController.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIViewManager.h"
#include "nsIWebNavigation.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

////////////////////////////////////////////////////////////////////////////////
// nsDocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

nsDocAccessibleWrap::
  nsDocAccessibleWrap(nsIDocument *aDocument, nsIContent *aRootContent,
                      nsIWeakReference *aShell) :
  nsDocAccessible(aDocument, aRootContent, aShell)
{
}

nsDocAccessibleWrap::~nsDocAccessibleWrap()
{
}

//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------
STDMETHODIMP_(ULONG) nsDocAccessibleWrap::AddRef()
{
  return nsAccessNode::AddRef();
}

STDMETHODIMP_(ULONG) nsDocAccessibleWrap::Release()
{
  return nsAccessNode::Release();
}

// Microsoft COM QueryInterface
STDMETHODIMP nsDocAccessibleWrap::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_ISimpleDOMDocument == iid)
    *ppv = static_cast<ISimpleDOMDocument*>(this);

  if (NULL == *ppv)
    return nsHyperTextAccessibleWrap::QueryInterface(iid, ppv);
    
  (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
  return S_OK;
}

nsAccessible*
nsDocAccessibleWrap::GetXPAccessibleFor(const VARIANT& aVarChild)
{
  // If lVal negative then it is treated as child ID and we should look for
  // accessible through whole accessible subtree including subdocuments.
  // Otherwise we treat lVal as index in parent.

  if (aVarChild.vt == VT_I4 && aVarChild.lVal < 0) {
    // Convert child ID to unique ID.
    void* uniqueID = reinterpret_cast<void*>(-aVarChild.lVal);
    return GetCachedAccessibleInSubtree(uniqueID);
  }

  return nsAccessibleWrap::GetXPAccessibleFor(aVarChild);
}

STDMETHODIMP nsDocAccessibleWrap::get_URL(/* [out] */ BSTR __RPC_FAR *aURL)
{
__try {
  *aURL = NULL;

  nsAutoString URL;
  nsresult rv = GetURL(URL);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (URL.IsEmpty())
    return S_FALSE;

  *aURL = ::SysAllocStringLen(URL.get(), URL.Length());
  return *aURL ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::get_title( /* [out] */ BSTR __RPC_FAR *aTitle)
{
__try {
  *aTitle = NULL;

  nsAutoString title;
  nsresult rv = GetTitle(title);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aTitle = ::SysAllocStringLen(title.get(), title.Length());
  return *aTitle ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::get_mimeType(/* [out] */ BSTR __RPC_FAR *aMimeType)
{
__try {
  *aMimeType = NULL;

  nsAutoString mimeType;
  nsresult rv = GetMimeType(mimeType);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (mimeType.IsEmpty())
    return S_FALSE;

  *aMimeType = ::SysAllocStringLen(mimeType.get(), mimeType.Length());
  return *aMimeType ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::get_docType(/* [out] */ BSTR __RPC_FAR *aDocType)
{
__try {
  *aDocType = NULL;

  nsAutoString docType;
  nsresult rv = GetDocType(docType);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (docType.IsEmpty())
    return S_FALSE;

  *aDocType = ::SysAllocStringLen(docType.get(), docType.Length());
  return *aDocType ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::get_nameSpaceURIForID(/* [in] */  short aNameSpaceID,
  /* [out] */ BSTR __RPC_FAR *aNameSpaceURI)
{
__try {
  *aNameSpaceURI = NULL;

  if (aNameSpaceID < 0)
    return E_INVALIDARG;  // -1 is kNameSpaceID_Unknown

  nsAutoString nameSpaceURI;
  nsresult rv = GetNameSpaceURIForID(aNameSpaceID, nameSpaceURI);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (nameSpaceURI.IsEmpty())
    return S_FALSE;

  *aNameSpaceURI = ::SysAllocStringLen(nameSpaceURI.get(),
                                       nameSpaceURI.Length());

  return *aNameSpaceURI ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsDocAccessibleWrap::put_alternateViewMediaTypes( /* [in] */ BSTR __RPC_FAR *aCommaSeparatedMediaTypes)
{
__try {
  *aCommaSeparatedMediaTypes = NULL;
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_NOTIMPL;
}

STDMETHODIMP nsDocAccessibleWrap::get_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue)
{
  // For backwards-compat, we still support old MSAA hack to provide URL in accValue
  *pszValue = NULL;
  // Check for real value first
  HRESULT hr = nsAccessibleWrap::get_accValue(varChild, pszValue);
  if (FAILED(hr) || *pszValue || varChild.lVal != CHILDID_SELF)
    return hr;
  // If document is being used to create a widget, don't use the URL hack
  PRUint32 role = Role();
  if (role != nsIAccessibleRole::ROLE_DOCUMENT &&
      role != nsIAccessibleRole::ROLE_APPLICATION &&
      role != nsIAccessibleRole::ROLE_DIALOG &&
      role != nsIAccessibleRole::ROLE_ALERT)
    return hr;

  return get_URL(pszValue);
}

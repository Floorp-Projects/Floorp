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
#include "nsIPresShell.h"
#include "nsISelectionController.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIViewManager.h"
#include "nsIWebNavigation.h"
#include "nsIWidget.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

//----- nsDocAccessibleWrap -----

nsDocAccessibleWrap::nsDocAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell): 
  nsDocAccessible(aDOMNode, aShell)
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
    *ppv = NS_STATIC_CAST(ISimpleDOMDocument*, this);

  if (NULL == *ppv)
    return nsAccessibleWrap::QueryInterface(iid, ppv);
    
  (NS_REINTERPRET_CAST(IUnknown*, *ppv))->AddRef();
  return S_OK;
}

void nsDocAccessibleWrap::GetXPAccessibleFor(const VARIANT& aVarChild, nsIAccessible **aXPAccessible)
{
  *aXPAccessible = nsnull;
  if (!mWeakShell)
    return; // This document has been shut down

  if (aVarChild.lVal < 0) {
    // Get from hash table
    void *uniqueID = (void*)(-aVarChild.lVal);  // Convert child ID back to unique ID
    nsCOMPtr<nsIAccessNode> accessNode;
    GetCachedAccessNode(uniqueID, getter_AddRefs(accessNode));
    nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(accessNode));
    NS_IF_ADDREF(*aXPAccessible = accessible);
    return;
  }

  nsDocAccessible::GetXPAccessibleFor(aVarChild, aXPAccessible);
}

STDMETHODIMP nsDocAccessibleWrap::get_accChild( 
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild)
{
  *ppdispChild = NULL;

  if (varChild.vt == VT_I4 && varChild.lVal < 0) {
    // AccessibleObjectFromEvent() being called
    // that's why the lVal < 0
    nsCOMPtr<nsIAccessible> xpAccessible;
    GetXPAccessibleFor(varChild, getter_AddRefs(xpAccessible));
    if (xpAccessible) {
      IAccessible *msaaAccessible;
      xpAccessible->GetNativeInterface((void**)&msaaAccessible);
      *ppdispChild = NS_STATIC_CAST(IDispatch*, msaaAccessible);
      return S_OK;
    }
    else if (mDocument) {
      // If child ID from event can't be found in this window, ask parent.
      // This is especially relevant for times when a xul menu item
      // has focus, but the system thinks the content window has focus.
      nsIDocument* parentDoc = mDocument->GetParentDocument();
      if (parentDoc) {
        nsIPresShell *parentShell = parentDoc->GetShellAt(0);
        nsCOMPtr<nsIWeakReference> weakParentShell(do_GetWeakReference(parentShell));
        if (weakParentShell) {
          nsCOMPtr<nsIAccessibleDocument> parentDocAccessible = 
            nsAccessNode::GetDocAccessibleFor(weakParentShell);
          nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(parentDocAccessible));
          IAccessible *msaaParentDoc;
          if (accessible) {
            accessible->GetNativeInterface((void**)&msaaParentDoc);
            HRESULT rv = msaaParentDoc->get_accChild(varChild, ppdispChild);
            msaaParentDoc->Release();
            return rv;
          }
        }
      }
    }
    return E_FAIL;
  }

  // Otherwise, the normal get_accChild() will do
  return nsAccessibleWrap::get_accChild(varChild, ppdispChild);
}

NS_IMETHODIMP nsDocAccessibleWrap::Shutdown()
{
  return nsDocAccessible::Shutdown();
}

NS_IMETHODIMP nsDocAccessibleWrap::FireAnchorJumpEvent()
{
  // Staying on the same page, jumping to a named anchor
  // Fire EVENT_SCROLLING_START on first leaf accessible -- because some
  // assistive technologies only cache the child numbers for leaf accessibles
  // the can only relate events back to their internal model if it's a leaf.
  // There is usually an accessible for the focus node, but if it's an empty text node
  // we have to move forward in the document to get one
  nsDocAccessible::FireAnchorJumpEvent();
  if (!mIsAnchorJumped)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> focusNode;
  if (mIsAnchor) {
    nsCOMPtr<nsISelectionController> selCon(do_QueryReferent(mWeakShell));
    if (!selCon) {
      return NS_OK;
    }
    nsCOMPtr<nsISelection> domSel;
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
    if (!domSel) {
      return NS_OK;
    }
    domSel->GetFocusNode(getter_AddRefs(focusNode));
  }
  else {
    focusNode = mDOMNode; // Moved to top, so event is for 1st leaf after root
  }

  nsCOMPtr<nsIAccessible> accessible = GetFirstAvailableAccessible(focusNode, PR_TRUE);
  nsCOMPtr<nsPIAccessible> privateAccessible = do_QueryInterface(accessible);
  if (privateAccessible) {
    privateAccessible->FireToolkitEvent(nsIAccessibleEvent::EVENT_SCROLLING_START,
                                        accessible, nsnull);
  }
  return NS_OK;
}

STDMETHODIMP nsDocAccessibleWrap::get_URL(/* [out] */ BSTR __RPC_FAR *aURL)
{
  *aURL = NULL;
  nsAutoString URL;
  if (NS_SUCCEEDED(GetURL(URL))) {
    *aURL= ::SysAllocString(URL.get());
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::get_title( /* [out] */ BSTR __RPC_FAR *aTitle)
{
  *aTitle = NULL;
  nsAutoString title;
  if (NS_SUCCEEDED(GetTitle(title))) { // getter_Copies(pszTitle)))) {
    *aTitle= ::SysAllocString(title.get());
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::get_mimeType(/* [out] */ BSTR __RPC_FAR *aMimeType)
{
  *aMimeType = NULL;
  nsAutoString mimeType;
  if (NS_SUCCEEDED(GetMimeType(mimeType))) {
    *aMimeType= ::SysAllocString(mimeType.get());
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::get_docType(/* [out] */ BSTR __RPC_FAR *aDocType)
{
  *aDocType = NULL;
  nsAutoString docType;
  if (NS_SUCCEEDED(GetDocType(docType))) {
    *aDocType= ::SysAllocString(docType.get());
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::get_nameSpaceURIForID(/* [in] */  short aNameSpaceID,
  /* [out] */ BSTR __RPC_FAR *aNameSpaceURI)
{
  if (aNameSpaceID < 0) {
    return E_FAIL;  // -1 is kNameSpaceID_Unknown
  }
  *aNameSpaceURI = NULL;
  nsAutoString nameSpaceURI;
  if (NS_SUCCEEDED(GetNameSpaceURIForID(aNameSpaceID, nameSpaceURI))) {
    *aNameSpaceURI = ::SysAllocString(nameSpaceURI.get());
    return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP nsDocAccessibleWrap::put_alternateViewMediaTypes( /* [in] */ BSTR __RPC_FAR *commaSeparatedMediaTypes)
{
  return E_NOTIMPL;
}


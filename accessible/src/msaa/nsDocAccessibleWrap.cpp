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
#include "nsIAccessibleEvent.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
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
          nsCOMPtr<nsIAccessibleDocument> parentDocAccessible;
          nsAccessNode::GetDocAccessibleFor(weakParentShell, 
                                            getter_AddRefs(parentDocAccessible));
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

STDMETHODIMP nsDocAccessibleWrap::get_accParent( IDispatch __RPC_FAR *__RPC_FAR *ppdispParent)
{
  // MSAA expects that client area accessibles return the native accessible for 
  // their containing window, otherwise WindowFromAccessibleObject() doesn't work.

  void* wnd;
  GetWindowHandle(&wnd);

  HWND pWnd = ::GetParent(NS_REINTERPRET_CAST(HWND, wnd));
  if (pWnd) {
    // get the accessible.
    void* ptr = nsnull;
    HRESULT result = AccessibleObjectFromWindow(pWnd, OBJID_WINDOW, IID_IAccessible, &ptr);
    if (SUCCEEDED(result)) {
      IAccessible* msaaParentAccessible = (IAccessible*)ptr;
      // got one? return it.
      if (msaaParentAccessible) {
        *ppdispParent = msaaParentAccessible;
        return S_OK;
      }
    }
  }

  return E_FAIL;
}

NS_IMETHODIMP nsDocAccessibleWrap::FireToolkitEvent(PRUint32 aEvent, nsIAccessible* aAccessible, void* aData)
{
  if (!mWeakShell) {   // Means we're not active
    return NS_ERROR_FAILURE;
  }

  nsDocAccessible::FireToolkitEvent(aEvent, aAccessible, aData); // Fire nsIObserver message

#ifdef SWALLOW_DOC_FOCUS_EVENTS
  // Remove this until we can figure out which focus events are coming at
  // the same time as native window focus events, although
  // perhaps 2 duplicate focus events on the window isn't really a problem
  if (aEvent == nsIAccessibleEvent::EVENT_FOCUS) {
    // Don't fire accessible focus event for documents, 
    // Microsoft Windows will generate those from native window focus events
    nsCOMPtr<nsIAccessibleDocument> accessibleDoc(do_QueryInterface(aAccessible));
    if (accessibleDoc)
      return NS_OK;
  }
#endif

  PRInt32 childID, worldID = OBJID_CLIENT;
  PRUint32 role = ROLE_SYSTEM_TEXT; // Default value

  if (NS_SUCCEEDED(aAccessible->GetRole(&role)) && role == ROLE_SYSTEM_CARET) {
    childID = CHILDID_SELF;
    worldID = OBJID_CARET;
  }
  else 
    childID = GetChildIDFor(aAccessible); // get the id for the accessible

  if (role == ROLE_SYSTEM_PANE && aEvent == nsIAccessibleEvent::EVENT_STATE_CHANGE) {
    // Something on the document has changed
    // Clear out the cache in this subtree
  }

  HWND hWnd = NS_REINTERPRET_CAST(HWND, mWnd);
  if (gmGetGUIThreadInfo && (aEvent == nsIAccessibleEvent::EVENT_FOCUS || 
      aEvent == nsIAccessibleEvent::EVENT_MENUPOPUPSTART ||
      aEvent == nsIAccessibleEvent::EVENT_MENUPOPUPEND)) {
    GUITHREADINFO guiInfo;
    guiInfo.cbSize = sizeof(GUITHREADINFO);
    if (gmGetGUIThreadInfo(NULL, &guiInfo)) {
      hWnd = guiInfo.hwndFocus;
    }
  }

  // notify the window system
  NotifyWinEvent(aEvent, hWnd, worldID, childID);

  return NS_OK;
}

PRInt32 nsDocAccessibleWrap::GetChildIDFor(nsIAccessible* aAccessible)
{
  // A child ID of the window is required, when we use NotifyWinEvent, so that the 3rd party application
  // can call back and get the IAccessible the event occured on.
  // We use the unique ID exposed through nsIContent::GetContentID()

  void *uniqueID;
  nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(aAccessible));
  accessNode->GetUniqueID(&uniqueID);

  // Yes, this means we're only compatibible with 32 bit
  // MSAA is only available for 32 bit windows, so it's okay
  return - NS_PTR_TO_INT32(uniqueID);
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


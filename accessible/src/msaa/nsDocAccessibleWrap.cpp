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
#include "nsIAccessibleEvent.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMTreeWalker.h"
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
  nsDocAccessible(aDOMNode, aShell), mWasAnchor(PR_FALSE)
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

STDMETHODIMP nsDocAccessibleWrap::get_accFocus( 
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  // Return the current IAccessible child that has focus
  VariantInit(pvarChild);
  pvarChild->vt = VT_EMPTY;

  nsCOMPtr<nsIAccessible> accessible;
  if (NS_SUCCEEDED(GetFocusedChild(getter_AddRefs(accessible)))) {
    if (accessible == (nsIAccessible*)this) {
      pvarChild->vt = VT_I4;
      pvarChild->lVal = CHILDID_SELF;
    }
    else {
      pvarChild->vt = VT_DISPATCH;
      pvarChild->pdispVal = NativeAccessible(accessible);
    }
  }
  return S_OK;
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

already_AddRefed<nsIAccessible>
nsDocAccessibleWrap::GetFirstLeafAccessible(nsIDOMNode *aStartNode)
{
  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  nsCOMPtr<nsIAccessible> accessible;
  nsCOMPtr<nsIDOMTreeWalker> walker; 
  nsCOMPtr<nsIDOMNode> currentNode(aStartNode);

  while (currentNode) {
    accService->GetAccessibleInWeakShell(currentNode, mWeakShell, getter_AddRefs(accessible)); // AddRef'd
    if (accessible) {
      PRInt32 numChildren;
      accessible->GetChildCount(&numChildren);
      if (numChildren == 0) {
        nsIAccessible *leafAccessible = accessible;
        NS_ADDREF(leafAccessible);
        return leafAccessible;  // It's a leaf accessible, return it
      }
    }
    if (!walker) {
      // Instantiate walker lazily since we won't need it in 90% of the cases
      // where the first DOM node we're given provides an accessible
      nsCOMPtr<nsIDOMDocumentTraversal> trav = do_QueryInterface(mDocument);
      NS_ASSERTION(trav, "No DOM document traversal for document");
      trav->CreateTreeWalker(mDOMNode, 
                            nsIDOMNodeFilter::SHOW_ELEMENT | nsIDOMNodeFilter::SHOW_TEXT,
                            nsnull, PR_FALSE, getter_AddRefs(walker));
      NS_ENSURE_TRUE(walker, nsnull);
      walker->SetCurrentNode(currentNode);
    }

    walker->NextNode(getter_AddRefs(currentNode));
  }

  return nsnull;
}

void nsDocAccessibleWrap::FireAnchorJumpEvent()
{
  // Staying on the same page, jumping to a named anchor
  // Fire EVENT_SELECTION_WITHIN on first leaf accessible -- because some
  // assistive technologies only cache the child numbers for leaf accessibles
  // the can only relate events back to their internal model if it's a leaf.
  // There is usually an accessible for the focus node, but if it's an empty text node
  // we have to move forward in the document to get one
  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(container));
  nsCAutoString theURL;
  if (webNav) {
    nsCOMPtr<nsIURI> pURI;
    webNav->GetCurrentURI(getter_AddRefs(pURI));
    if (pURI) {
      pURI->GetSpec(theURL);
    }
  }
  const char kHash = '#';
  PRBool hasAnchor = PR_FALSE;
  if (theURL.FindChar(kHash) > 0) {
    hasAnchor = PR_TRUE;
  }

  // mWasAnchor is set when the previous URL included a named anchor.
  // This way we still know to fire the SELECTION_WITHIN event when we
  // move from a named anchor back to the top.
  if (!mWasAnchor && !hasAnchor) {
    return;
  }
  mWasAnchor = hasAnchor;

  nsCOMPtr<nsIDOMNode> focusNode;
  if (hasAnchor) {
    nsCOMPtr<nsISelectionController> selCon(do_QueryReferent(mWeakShell));
    if (!selCon) {
      return;
    }
    nsCOMPtr<nsISelection> domSel;
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
    if (!domSel) {
      return;
    }
    domSel->GetFocusNode(getter_AddRefs(focusNode));
  }
  else {
    focusNode = mDOMNode; // Moved to top, so event is for 1st leaf after root
  }

  nsCOMPtr<nsIAccessible> accessible = GetFirstLeafAccessible(focusNode);
  nsCOMPtr<nsPIAccessible> privateAccessible = do_QueryInterface(accessible);
  if (privateAccessible) {
    privateAccessible->FireToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN,
                                        accessible, nsnull);
  }
}

void nsDocAccessibleWrap::FireDocLoadFinished()
{
  if (!mDocument || !mWeakShell)
    return;  // Document has been shut down

  PRUint32 state;
  GetState(&state);
  if ((state & STATE_INVISIBLE) != 0) {
    return; // Don't consider load finished until window unhidden
  }

  if (mIsNewDocument) {
    mIsNewDocument = PR_FALSE;

    if (mBusy != eBusyStateDone) {
      mBusy = eBusyStateDone; // before event callback so STATE_BUSY is not reported
      FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, this, nsnull);
      FireAnchorJumpEvent();
    }
  }

  mBusy = eBusyStateDone;
}

NS_IMETHODIMP nsDocAccessibleWrap::OnLocationChange(nsIWebProgress *aWebProgress,
                                                    nsIRequest *aRequest,
                                                    nsIURI *location)
{
  if (!mWeakShell || !mDocument) {
    return NS_OK;
  }
  
  PRBool isLoadingDocument;
  aWebProgress->GetIsLoadingDocument(&isLoadingDocument);
  if (isLoadingDocument) {
    return nsDocAccessible::OnLocationChange(aWebProgress, aRequest, location);
  }

  FireAnchorJumpEvent();
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


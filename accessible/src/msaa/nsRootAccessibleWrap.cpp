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
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsRootAccessibleWrap.h"
#include "nsIAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessibleEventReceiver.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

//----- nsRootAccessibleWrap -----

nsRootAccessibleWrap::nsRootAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell): 
  nsRootAccessible(aDOMNode, aShell)
{
  // XXX aaronl: we should be able to simplify event handling 
  // now that we have 1 accessible class hierarchy.
  // Why should we have to call Add/Remove accesible event listener anymore?

  static PRBool prefsInitialized;

  if (!prefsInitialized) {
    nsCOMPtr<nsIPref> prefService(do_GetService(kPrefCID));
    if (prefService) {
      //prefService->GetBoolPref("accessibility.disablecache", &gIsCacheDisabled);
      prefService->GetBoolPref("accessibility.disableenumvariant", &gIsEnumVariantSupportDisabled);
    }
    prefsInitialized = PR_TRUE;
  }

  if (!gmUserLib) {
    gmUserLib =::LoadLibrary("USER32.DLL");
  }

  if (gmUserLib) {
    if (!gmNotifyWinEvent)
      gmNotifyWinEvent = (LPFNNOTIFYWINEVENT)GetProcAddress(gmUserLib,"NotifyWinEvent");
    if (!gmGetGUIThreadInfo)
      gmGetGUIThreadInfo = (LPFNGETGUITHREADINFO)GetProcAddress(gmUserLib,"GetGUIThreadInfo");
  }
}

nsRootAccessibleWrap::~nsRootAccessibleWrap()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsRootAccessibleWrap, nsRootAccessible, nsIAccessibleEventListener)


void nsRootAccessibleWrap::GetXPAccessibleFor(VARIANT varChild, nsIAccessible **aXPAccessible)
{
  *aXPAccessible = nsnull;

  if (varChild.lVal < 0) {
    // Get from hash table
    // Not implemented yet
    NS_IF_ADDREF(*aXPAccessible);
    return;
  }

  nsRootAccessible::GetXPAccessibleFor(varChild, aXPAccessible);
}

STDMETHODIMP nsRootAccessibleWrap::get_accChild( 
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
      varChild.pdispVal = NS_STATIC_CAST(IDispatch*, msaaAccessible);
      return S_OK;
    }
    return E_FAIL;
  }

  // Otherwise, the normal get_accChild() will do
  return nsAccessibleWrap::get_accChild(varChild, ppdispChild);
}

NS_IMETHODIMP nsRootAccessibleWrap::HandleEvent(PRUint32 aEvent, nsIAccessible* aAccessible, void* aData)
{
  if (!mDOMNode) {   // Means we're not active
    return NS_ERROR_FAILURE;
  }

#ifdef SWALLOW_DOC_FOCUS_EVENTS
  // Remove this until we can figure out which focus events are coming at
  // the same time as native window focus events, although
  // perhaps 2 duplicate focus events on the window isn't really a problem
  if (aEvent == EVENT_FOCUS) {
    // Don't fire accessible focus event for documents, 
    // Microsoft Windows will generate those from native window focus events
    nsCOMPtr<nsIAccessibleDocument> accessibleDoc(do_QueryInterface(aAccessible));
    if (accessibleDoc)
      return NS_OK;
  }
#endif

  PRInt32 childID, worldID = OBJID_CLIENT;
  PRUint32 role = ROLE_SYSTEM_TEXT; // Default value

  if (NS_SUCCEEDED(aAccessible->GetAccRole(&role)) && role == ROLE_SYSTEM_CARET) {
    childID = CHILDID_SELF;
    worldID = OBJID_CARET;
  }
  else 
    childID = GetIdFor(aAccessible); // get the id for the accessible

  if (role == ROLE_SYSTEM_PANE && aEvent == EVENT_STATE_CHANGE) {
    // Something on the document has changed
    // Clear out the cache in this subtree
  }

  if (role == EVENT_OBJECT_REORDER) {
    // Probably need to do this somewhere else so simple dom nodes get shutdown
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(aAccessible));
    accessNode->Shutdown();
  }

  HWND hWnd; //= mWnd;  // aaronl we're a sibling now, how to we get that data
  if (gmGetGUIThreadInfo && (aEvent == EVENT_FOCUS || 
      aEvent == EVENT_MENUPOPUPSTART ||
      aEvent == EVENT_MENUPOPUPEND)) {
    GUITHREADINFO guiInfo;
    guiInfo.cbSize = sizeof(GUITHREADINFO);
    if (gmGetGUIThreadInfo(NULL, &guiInfo)) {
      hWnd = guiInfo.hwndFocus;
    }
  }
  else {
    void *wnd;
    GetOwnerWindow(&wnd);
    hWnd = NS_REINTERPRET_CAST(HWND, wnd);
  }

  // notify the window system
  //NotifyWinEvent(aEvent, hWnd, worldID, childID);

  return NS_OK;
}

PRUint32 nsRootAccessibleWrap::GetIdFor(nsIAccessible* aAccessible)
{
  // A child ID of the window is required, when we use NotifyWinEvent, so that the 3rd party application
  // can call back and get the IAccessible the event occured on.
  // We use the unique ID exposed through nsIContent::GetContentID()

  PRInt32 uniqueID;
  aAccessible->GetAccId(&uniqueID);

  return uniqueID;
}


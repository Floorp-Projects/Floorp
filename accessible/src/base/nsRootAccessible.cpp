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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// NOTE: alphabetically ordered
#include "nsAccessibilityService.h"
#include "nsAccessibleEventData.h"
#include "nsCaretAccessible.h"
#include "nsHTMLSelectAccessible.h"
#include "nsIAccessibleCaret.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIHTMLDocument.h"
#include "nsIFocusController.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScrollableView.h"
#include "nsIServiceManager.h"
#include "nsIViewManager.h"
#include "nsLayoutAtoms.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsRootAccessible.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"

#ifdef MOZ_XUL
#include "nsXULTreeAccessible.h"
#include "nsIXULDocument.h"
#endif
#include "nsAccessibilityService.h"
#include "nsISelectionPrivate.h"
#include "nsICaret.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsAccessibleEventData.h"
#include "nsIDOMDocument.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsIAccessibleHyperText.h"
#endif

NS_INTERFACE_MAP_BEGIN(nsRootAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFormListener)
NS_INTERFACE_MAP_END_INHERITING(nsDocAccessible)

NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsDocAccessible)
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsDocAccessible)


//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsDocAccessibleWrap(aDOMNode, aShell), 
  mAccService(do_GetService("@mozilla.org/accessibilityService;1")),
  mIsInDHTMLMenu(PR_FALSE)
{
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsRootAccessible::~nsRootAccessible()
{
}

// helpers
/* readonly attribute AString name; */
NS_IMETHODIMP nsRootAccessible::GetName(nsAString& aName)
{
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  if (mRoleMapEntry) {
    nsAccessible::GetName(aName);
    if (!aName.IsEmpty()) {
      return NS_OK;
    }
  }

  nsIScriptGlobalObject *globalScript = mDocument->GetScriptGlobalObject();
  nsIDocShell *docShell = nsnull;
  if (globalScript) {
    docShell = globalScript->GetDocShell();
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
  if(!docShellAsItem)
     return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));

  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(treeOwner));
  if (baseWindow) {
    nsXPIDLString title;
    baseWindow->GetTitle(getter_Copies(title));
    aName.Assign(title);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

/* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsRootAccessible::GetParent(nsIAccessible * *aParent) 
{ 
  *aParent = nsnull;
  return NS_OK;
}

/* readonly attribute unsigned long accRole; */
NS_IMETHODIMP nsRootAccessible::GetRole(PRUint32 *aRole) 
{ 
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  // If it's a <dialog> or <wizard>, use ROLE_DIALOG instead
  nsIContent *rootContent = mDocument->GetRootContent();
  if (rootContent) {
    nsCOMPtr<nsIDOMElement> rootElement(do_QueryInterface(rootContent));
    if (rootElement) {
      nsAutoString name;
      rootElement->GetLocalName(name);
      if (name.EqualsLiteral("dialog") || name.EqualsLiteral("wizard")) {
        *aRole = ROLE_DIALOG; // Always at the root
        return NS_OK;
      }
    }
  }

  return nsDocAccessibleWrap::GetRole(aRole);
}

NS_IMETHODIMP nsRootAccessible::GetState(PRUint32 *aState) 
{
  nsresult rv = NS_ERROR_FAILURE;
  if (mDOMNode) {
    rv = nsDocAccessibleWrap::GetState(aState);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(mDocument, "mDocument should not be null unless mDOMNode is");
  if (gLastFocusedNode) {
    nsCOMPtr<nsIDOMDocument> rootAccessibleDoc(do_QueryInterface(mDocument));
    nsCOMPtr<nsIDOMDocument> focusedDoc;
    gLastFocusedNode->GetOwnerDocument(getter_AddRefs(focusedDoc));
    if (rootAccessibleDoc == focusedDoc) {
      *aState |= STATE_FOCUSED;
    }
  }
  return NS_OK;
}

void
nsRootAccessible::GetChromeEventHandler(nsIDOMEventTarget **aChromeTarget)
{
  nsCOMPtr<nsIDOMWindow> domWin;
  GetWindow(getter_AddRefs(domWin));
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(domWin));
  nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
  if (privateDOMWindow) {
    chromeEventHandler = privateDOMWindow->GetChromeEventHandler();
  }

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(chromeEventHandler));

  *aChromeTarget = target;
  NS_IF_ADDREF(*aChromeTarget);
}

nsresult nsRootAccessible::AddEventListeners()
{
  // use AddEventListener from the nsIDOMEventTarget interface
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mDocument));
  if (target) { 
    // capture DOM focus events 
    nsresult rv = target->AddEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // capture Form change events 
    rv = target->AddEventListener(NS_LITERAL_STRING("select"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // capture NameChange events (fired whenever name changes, immediately after, whether focus moves or not)
    rv = target->AddEventListener(NS_LITERAL_STRING("NameChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // capture ValueChange events (fired whenever value changes, immediately after, whether focus moves or not)
    rv = target->AddEventListener(NS_LITERAL_STRING("ValueChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // capture AlertActive events (fired whenever alert pops up)
    rv = target->AddEventListener(NS_LITERAL_STRING("AlertActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // add ourself as a OpenStateChange listener (custom event fired in tree.xml)
    rv = target->AddEventListener(NS_LITERAL_STRING("OpenStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // add ourself as a CheckboxStateChange listener (custom event fired in nsHTMLInputElement.cpp)
    rv = target->AddEventListener(NS_LITERAL_STRING("CheckboxStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // add ourself as a RadioStateChange Listener ( custom event fired in in nsHTMLInputElement.cpp  & radio.xml)
    rv = target->AddEventListener(NS_LITERAL_STRING("RadioStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = target->AddEventListener(NS_LITERAL_STRING("popupshown"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = target->AddEventListener(NS_LITERAL_STRING("popuphiding"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = target->AddEventListener(NS_LITERAL_STRING("DOMMenuInactive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = target->AddEventListener(NS_LITERAL_STRING("DOMMenuItemActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = target->AddEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = target->AddEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = target->AddEventListener(NS_LITERAL_STRING("DOMContentLoaded"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  GetChromeEventHandler(getter_AddRefs(target));
  NS_ASSERTION(target, "No chrome event handler for document");
  if (target) {
    nsresult rv = target->AddEventListener(NS_LITERAL_STRING("pagehide"), 
                                           NS_STATIC_CAST(nsIDOMXULListener*, this), 
                                           PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
    target->AddEventListener(NS_LITERAL_STRING("pageshow"), 
                             NS_STATIC_CAST(nsIDOMXULListener*, this), 
                             PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mCaretAccessible)
    mCaretAccessible = new nsCaretAccessible(mDOMNode, mWeakShell, this);

  // Fire accessible focus event for pre-existing focus, but wait until all internal
  // focus events are finished for window initialization.
  mFireFocusTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mFireFocusTimer) {
    mFireFocusTimer->InitWithFuncCallback(FireFocusCallback, this,
                                          0, nsITimer::TYPE_ONE_SHOT);
  }

  return nsDocAccessible::AddEventListeners();
}

nsresult nsRootAccessible::RemoveEventListeners()
{
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mDocument));
  if (target) { 
    target->RemoveEventListener(NS_LITERAL_STRING("focus"), NS_STATIC_CAST(nsIDOMFocusListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("select"), NS_STATIC_CAST(nsIDOMFormListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("NameChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("ValueChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("AlertActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("OpenStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("CheckboxStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("RadioStateChange"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("popupshown"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("popuphiding"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuInactive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuItemActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarActive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMMenuBarInactive"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMContentLoaded"), NS_STATIC_CAST(nsIDOMXULListener*, this), PR_TRUE);
  }

  GetChromeEventHandler(getter_AddRefs(target));
  if (target) {
    target->RemoveEventListener(NS_LITERAL_STRING("pagehide"), 
                                NS_STATIC_CAST(nsIDOMXULListener*, this), 
                                PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("pageshow"), 
                                NS_STATIC_CAST(nsIDOMXULListener*, this), 
                                PR_TRUE);
  }

  if (mCaretAccessible) {
    mCaretAccessible->RemoveSelectionListener();
    mCaretAccessible = nsnull;
  }

  mAccService = nsnull;

  return nsDocAccessible::RemoveEventListeners();
}

NS_IMETHODIMP nsRootAccessible::GetCaretAccessible(nsIAccessible **aCaretAccessible)
{
  *aCaretAccessible = nsnull;
  if (mCaretAccessible) {
    CallQueryInterface(mCaretAccessible, aCaretAccessible);
  }

  return NS_OK;
}

void nsRootAccessible::TryFireEarlyLoadEvent(nsIAccessible *aAccessible, nsIDOMNode *aDocNode)
{
  // We can fire an early load event based on DOMContentLoaded unless we 
  // have subdocuments. For that we wait until WebProgressListener
  // STATE_STOP handling in nsAccessibilityService.

  // Note, we don't fire any page load finished events for chrome or for
  // frame/iframe page loads during the initial complete page load -- that page
  // load event for the entire content pane needs to stand alone.

  // This also works for firing events for error pages

  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    GetDocShellTreeItemFor(aDocNode);
  NS_ASSERTION(treeItem, "No docshelltreeitem for aDocNode");
  if (!treeItem) {
    return;
  }
  PRInt32 itemType;
  treeItem->GetItemType(&itemType);
  if (itemType != nsIDocShellTreeItem::typeContent) {
    return;
  }
  nsCOMPtr<nsIDocShellTreeNode> treeNode(do_QueryInterface(treeItem));
  if (treeNode) {
    PRInt32 subDocuments;
    treeNode->GetChildCount(&subDocuments);
    if (subDocuments) {
      return;
    }
  }
  nsCOMPtr<nsIDocShellTreeItem> rootContentTreeItem;
  treeItem->GetSameTypeRootTreeItem(getter_AddRefs(rootContentTreeItem));
  NS_ASSERTION(rootContentTreeItem, "No root content tree item");
  if (!rootContentTreeItem) { // Not at root of content
    return;
  }
  if (rootContentTreeItem != treeItem) {
    nsCOMPtr<nsIAccessibleDocument> rootContentDocAccessible =
      GetDocAccessibleFor(rootContentTreeItem);
    nsCOMPtr<nsIAccessible> rootContentAccessible =
      do_QueryInterface(rootContentDocAccessible);
    if (!rootContentAccessible) {
      return;
    }
    PRUint32 state;
    rootContentAccessible->GetFinalState(&state);
    if (state & STATE_BUSY) {
      // Don't fire page load events on subdocuments for initial page load of entire page
      return;
    }
  }

  // No frames or iframes, so we can fire the doc load finished event early
  nsCOMPtr<nsPIAccessibleDocument> docAccessible =
    do_QueryInterface(aAccessible);
  NS_ASSERTION(docAccessible, "No doc aAccessible for DOMContentLoaded");
  if (docAccessible) {
    docAccessible->FireDocLoadingEvent(PR_TRUE);
  }
}

void nsRootAccessible::FireAccessibleFocusEvent(nsIAccessible *aAccessible,
                                                nsIDOMNode *aNode,
                                                PRBool aForceEvent)
{
  NS_ASSERTION(aAccessible, "Attempted to fire focus event for no accessible");

  // Fire focus only if it changes, but always fire focus events for menu items
  // Also always fire for XUL tree items, because they always use the same node for
  // the tree container itself
  // XXX use optional aForceIt bool param
  if (gLastFocusedNode == aNode && !aForceEvent) {
    return;
  }

  nsCOMPtr<nsPIAccessible> privateAccessible =
    do_QueryInterface(aAccessible);
  NS_ASSERTION(privateAccessible , "No nsPIAccessible for nsIAccessible");

  // Use focus events on DHTML menuitems to indicate when to fire menustart and menuend
  // Special dynamic content handling
  PRUint32 role = ROLE_NOTHING;
  aAccessible->GetFinalRole(&role);
  if (role == ROLE_MENUITEM) {
    if (!mIsInDHTMLMenu) {  // Entering menus
      PRUint32 naturalRole; // The natural role is the role that this type of element normally has
      aAccessible->GetRole(&naturalRole);
      if (role != naturalRole) { // Must be a DHTML menuitem
         FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUSTART, this, nsnull);
         mIsInDHTMLMenu = ROLE_MENUITEM;
      }
    }
  }
  else if (mIsInDHTMLMenu) {
    FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUEND, this, nsnull);
    mIsInDHTMLMenu = PR_FALSE;
  }

  NS_IF_RELEASE(gLastFocusedNode);
  gLastFocusedNode = aNode;
  NS_IF_ADDREF(gLastFocusedNode);

  privateAccessible->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS,
                                      aAccessible, nsnull);

  if (mCaretAccessible)
    mCaretAccessible->AttachNewSelectionListener(aNode);
}

void nsRootAccessible::FireCurrentFocusEvent()
{
  nsCOMPtr<nsIDOMWindow> domWin;
  GetWindow(getter_AddRefs(domWin));
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(domWin));
  if (!privateDOMWindow) {
    return;
  }
  nsIFocusController *focusController = privateDOMWindow->GetRootFocusController();
  if (!focusController) {
    return;
  }
  nsCOMPtr<nsIDOMElement> focusedElement;
  focusController->GetFocusedElement(getter_AddRefs(focusedElement));
  nsCOMPtr<nsIDOMNode> focusedNode(do_QueryInterface(focusedElement));
  if (!focusedNode) {
    // Document itself may have focus
    nsCOMPtr<nsIDOMWindowInternal> focusedWinInternal;
    focusController->GetFocusedWindow(getter_AddRefs(focusedWinInternal));
    if (focusedWinInternal) {
      nsCOMPtr<nsIDOMDocument> focusedDOMDocument;
      focusedWinInternal->GetDocument(getter_AddRefs(focusedDOMDocument));
      focusedNode = do_QueryInterface(focusedDOMDocument);
    }
    if (!focusedNode) {
      return;  // Could not get a focused document either
    }
  }

  // Simulate a focus event so that we can reuse code that fires focus for container children like treeitems
  nsIContent *rootContent = mDocument->GetRootContent();
  nsPresContext *presContext = GetPresContext();
  if (rootContent && presContext) {
    nsCOMPtr<nsIDOMEvent> event;
    nsCOMPtr<nsIEventListenerManager> manager;
    rootContent->GetListenerManager(getter_AddRefs(manager));
    if (manager && NS_SUCCEEDED(manager->CreateEvent(presContext, nsnull,
                                                     NS_LITERAL_STRING("Events"),
                                                     getter_AddRefs(event))) &&
        NS_SUCCEEDED(event->InitEvent(NS_LITERAL_STRING("focus"), PR_TRUE, PR_TRUE))) {
      HandleEvent(event);
    }
  }
}

// --------------- nsIDOMEventListener Methods (3) ------------------------

NS_IMETHODIMP nsRootAccessible::HandleEvent(nsIDOMEvent* aEvent)
{
  // Turn DOM events in accessibility events

  // Get info about event and target
  nsCOMPtr<nsIDOMNode> targetNode; 
  GetTargetNode(aEvent, getter_AddRefs(targetNode));
  if (!targetNode)
    return NS_ERROR_FAILURE;

  nsAutoString eventType;
  aEvent->GetType(eventType);
  nsAutoString localName;
  targetNode->GetLocalName(localName);
#ifdef DEBUG_aleventhal
  // Very useful for debugging, please leave this here.
  if (eventType.LowerCaseEqualsLiteral("dommenuitemactive")) {
    printf("\ndebugging dommenuitemactive events for %s", NS_ConvertUCS2toUTF8(localName).get());
  }
  if (localName.EqualsIgnoreCase("tree")) {
    printf("\ndebugging events in tree, event is %s", NS_ConvertUCS2toUTF8(eventType).get());
  }
  if (localName.EqualsIgnoreCase("select")) {
    printf("\ndebugging events in select, event is %s", NS_ConvertUCS2toUTF8(eventType).get());
  }
#endif

  nsCOMPtr<nsIPresShell> eventShell = GetPresShellFor(targetNode);

#ifdef MOZ_ACCESSIBILITY_ATK
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement(do_QueryInterface(targetNode));
  if (anchorElement) {
    nsCOMPtr<nsIDOMNode> blockNode;
    // For ATK, we don't create any individual object for hyperlink, use its parent who has block frame instead
    if (NS_SUCCEEDED(nsAccessible::GetParentBlockNode(eventShell, targetNode, getter_AddRefs(blockNode))))
      targetNode = blockNode;
  }
#endif

  if (!eventShell) {
    return NS_OK;
  }
      
  if (eventType.LowerCaseEqualsLiteral("unload")) {
    // Only get cached accessible for pagehide -- so that we don't create it
    // just to destroy it.
    nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(eventShell));
    nsCOMPtr<nsIAccessibleDocument> accessibleDoc =
      nsAccessNode::GetDocAccessibleFor(weakShell);
    nsCOMPtr<nsPIAccessibleDocument> privateAccDoc = do_QueryInterface(accessibleDoc);
    if (privateAccDoc) {
      privateAccDoc->Destroy();
    }
    return NS_OK;
  }

  if (eventType.LowerCaseEqualsLiteral("popupshown")) {
    // Fire menupopupstart events after a delay so that ancestor views
    // are visible, otherwise an accessible cannot be created for the
    // popup and the accessibility toolkit event can't be fired.
    nsCOMPtr<nsIDOMXULPopupElement> popup(do_QueryInterface(targetNode));
    if (popup) {
      return FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_MENUPOPUPSTART,
                                    targetNode, nsnull);
    }
  }

  nsCOMPtr<nsIAccessible> accessible;
  if (NS_FAILED(mAccService->GetAccessibleInShell(targetNode, eventShell,
                                                  getter_AddRefs(accessible))))
    return NS_OK;
  
#ifdef MOZ_XUL
  // If it's a tree element, need the currently selected item
  nsCOMPtr<nsIAccessible> treeItemAccessible;
  if (localName.EqualsLiteral("tree")) {
    nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
      do_QueryInterface(targetNode);
    if (multiSelect) {
      PRInt32 treeIndex = -1;
      multiSelect->GetCurrentIndex(&treeIndex);
      if (treeIndex >= 0) {
        nsCOMPtr<nsIAccessibleTreeCache> treeCache(do_QueryInterface(accessible));
        if (!treeCache ||
            NS_FAILED(treeCache->GetCachedTreeitemAccessible(
                      treeIndex,
                      nsnull,
                      getter_AddRefs(treeItemAccessible))) ||
                      !treeItemAccessible) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        accessible = treeItemAccessible;
      }
    }
  }
#endif

  nsCOMPtr<nsPIAccessible> privAcc(do_QueryInterface(accessible));

#ifndef MOZ_ACCESSIBILITY_ATK
#ifdef MOZ_XUL
  // tree event
  if (eventType.LowerCaseEqualsLiteral("checkboxstatechange") ||
           eventType.LowerCaseEqualsLiteral("openstatechange")) {
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, 
                                accessible, nsnull);
      return NS_OK;
    }
  else if (treeItemAccessible) {
    if (eventType.LowerCaseEqualsLiteral("focus")) {
      FireAccessibleFocusEvent(accessible, targetNode); // Tree has focus
    }
    else if (eventType.LowerCaseEqualsLiteral("dommenuitemactive")) {
      FireAccessibleFocusEvent(treeItemAccessible, targetNode, PR_TRUE);
    }
    else if (eventType.LowerCaseEqualsLiteral("namechange")) {
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, 
                                accessible, nsnull);
    }
    else if (eventType.LowerCaseEqualsLiteral("select")) {
      // If multiselect tree, we should fire selectionadd or selection removed
      if (gLastFocusedNode == targetNode) {
        nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSel =
          do_QueryInterface(targetNode);
        nsAutoString selType;
        multiSel->GetSelType(selType);
        if (selType.IsEmpty() || !selType.EqualsLiteral("single")) {
          privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN, 
                                  accessible, nsnull);
          // XXX We need to fire EVENT_SELECTION_ADD and EVENT_SELECTION_REMOVE
          //     for each tree item. Perhaps each tree item will need to
          //     cache its selection state and fire an event after a DOM "select"
          //     event when that state changes.
          // nsXULTreeAccessible::UpdateTreeSelection();
        }
        else {
          privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION, 
                                  treeItemAccessible, nsnull);
        }
      }
    }
    return NS_OK;
  }
  else 
#endif
  if (eventType.LowerCaseEqualsLiteral("dommenuitemactive")) {
    nsCOMPtr<nsIAccessible> containerAccessible = accessible;
    PRUint32 containerState = 0;
    do {
      nsIAccessible *tempAccessible = containerAccessible;
      tempAccessible->GetParent(getter_AddRefs(containerAccessible));
      if (!containerAccessible) {
        break;
      }
      containerAccessible->GetFinalState(&containerState);
    }
    while ((containerState & STATE_HASPOPUP) == 0);

    // Only fire focus event for DOMMenuItemActive is not inside collapsed popup
    if (0 == (containerState & STATE_COLLAPSED)) {
      FireAccessibleFocusEvent(accessible, targetNode, PR_TRUE);
    }
  }
  else if (eventType.LowerCaseEqualsLiteral("focus")) {
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
      do_QueryInterface(targetNode);
    // Send focus to individual radio button or selected item
    if (selectControl) {
      nsCOMPtr<nsIDOMXULMenuListElement> menuList =
        do_QueryInterface(targetNode);
      if (!menuList) {
        // Don't do this for menu lists, the items only get focused
        // when the list is open, based on DOMMenuitemActive events
        nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
        selectControl->GetSelectedItem(getter_AddRefs(selectedItem));
        if (selectedItem) {
          targetNode = do_QueryInterface(selectedItem);
        }

        if (!targetNode ||
            NS_FAILED(mAccService->GetAccessibleInShell(targetNode, eventShell,
                      getter_AddRefs(accessible)))) {
          return NS_OK;
        }
      }
    }
    if (accessible == this) {
      // Top level window focus events already automatically fired by MSAA
      // based on HWND activities. Don't fire the extra focus event.
      NS_IF_RELEASE(gLastFocusedNode);
      gLastFocusedNode = mDOMNode;
      NS_IF_ADDREF(gLastFocusedNode);
      return NS_OK;
    }
    FireAccessibleFocusEvent(accessible, targetNode);
  }
  else if (eventType.LowerCaseEqualsLiteral("namechange")) {
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE,
                              accessible, nsnull);
  }
  else if (eventType.EqualsLiteral("ValueChange")) {
    PRUint32 role;
    accessible->GetFinalRole(&role);
    if (role == ROLE_PROGRESSBAR) {
      // For progressmeter, fire EVENT_SHOW on 1st value change
      nsAutoString value;
      accessible->GetFinalValue(value);
      if (value.EqualsLiteral("0%")) {
        privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_SHOW, 
                                  accessible, nsnull);
      }
    }
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, 
                              accessible, nsnull);
  }
  else if (eventType.EqualsLiteral("AlertActive")) { 
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_ALERT, 
                              accessible, nsnull);
  }
  else if (eventType.LowerCaseEqualsLiteral("radiostatechange") ) {
    // first the XUL radio buttons
    if (targetNode &&
        NS_SUCCEEDED(mAccService->GetAccessibleInShell(targetNode, eventShell,
                                                       getter_AddRefs(accessible)))) {
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, 
                                accessible, nsnull);
      FireAccessibleFocusEvent(accessible, targetNode);
    }
    else { // for the html radio buttons -- apparently the focus code just works. :-)
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, 
                                accessible, nsnull);
    }
  }
  else if (eventType.LowerCaseEqualsLiteral("dommenubaractive"))
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUSTART, accessible, nsnull);
  else if (eventType.LowerCaseEqualsLiteral("dommenubarinactive")) {
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUEND, accessible, nsnull);
    FireCurrentFocusEvent();
  }
  else if (eventType.LowerCaseEqualsLiteral("popuphiding")) {
    // If accessible focus was inside popup that closes,
    // then restore it to true current focus.
    // This is the case when we've been getting DOMMenuItemActive events
    // inside of a combo box that closes. The real focus is on the combo box.
    if (!gLastFocusedNode) {
      return NS_OK;
    }
    nsCOMPtr<nsIDOMNode> parentOfFocus;
    gLastFocusedNode->GetParentNode(getter_AddRefs(parentOfFocus));
    if (parentOfFocus != targetNode) {
      return NS_OK;
    }
    // Focus was inside of popup that's being hidden
    FireCurrentFocusEvent();
  }
  else if (eventType.LowerCaseEqualsLiteral("domcontentloaded")) {
    TryFireEarlyLoadEvent(accessible, targetNode);
  }
  else if (eventType.EqualsLiteral("DOMMenuInactive")) {
    nsCOMPtr<nsIDOMXULPopupElement> popup(do_QueryInterface(targetNode));
    if (popup) {
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUPOPUPEND,
                                accessible, nsnull);
    }
  }
#else
  AtkStateChange stateData;
  if (eventType.EqualsIgnoreCase("pageshow")) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(targetNode));
    if (htmlDoc) {
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_REORDER, 
                                accessible, nsnull);
    }
  }
  else if (eventType.LowerCaseEqualsLiteral("focus") || 
      eventType.LowerCaseEqualsLiteral("dommenuitemactive")) {
    if (treeItemAccessible) { // use focused treeitem
      privAcc = do_QueryInterface(treeItemAccessible);
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, 
                                treeItemAccessible, nsnull);
    }
    else if (anchorElement) {
      nsCOMPtr<nsIAccessibleHyperText> hyperText(do_QueryInterface(accessible));
      if (hyperText) {
        PRInt32 selectedLink;
        hyperText->GetSelectedLinkIndex(&selectedLink);
        privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_ATK_LINK_SELECTED, accessible, &selectedLink);
      }
    }
    else
      FireAccessibleFocusEvent(accessible, targetNode);
  }
  else if (eventType.LowerCaseEqualsLiteral("select")) {
    if (treeItemAccessible) { // it's a XUL <tree>
      // use EVENT_FOCUS instead of EVENT_ATK_SELECTION_CHANGE
      privAcc = do_QueryInterface(treeItemAccessible);
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, 
                                treeItemAccessible, nsnull);
    }
  }
#if 0
  // XXX todo: value change events for ATK are done with 
  // AtkPropertyChange, PROP_VALUE. Need the old and new value.
  // Not sure how we'll get the old value.
  // Aaron: I think this is a problem with the ATK API -- its much harder to
  // grab the old value for all the application developers than it is for
  // AT's to cache old values when they need to (when would that be!?)
  else if (eventType.LowerCaseEqualsLiteral("valuechange")) { 
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, 
                              accessible, nsnull);
  }
#endif
  else if (eventType.LowerCaseEqualsLiteral("checkboxstatechange") || // it's a XUL <checkbox>
           eventType.LowerCaseEqualsLiteral("radiostatechange")) { // it's a XUL <radio>
    accessible->GetFinalState(&stateData.state);
    stateData.enable = (stateData.state & STATE_CHECKED) != 0;
    stateData.state = STATE_CHECKED;
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, accessible, &stateData);
    if (eventType.LowerCaseEqualsLiteral("radiostatechange")) {
      FireAccessibleFocusEvent(accessible, targetNode);
    }
  }
  else if (eventType.LowerCaseEqualsLiteral("openstatechange")) { // collapsed/expanded changed
    accessible->GetFinalState(&stateData.state);
    stateData.enable = (stateData.state & STATE_EXPANDED) != 0;
    stateData.state = STATE_EXPANDED;
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, accessible, &stateData);
  }
  else if (eventType.LowerCaseEqualsLiteral("popuphiding")) {
    // If accessible focus was inside popup that closes,
    // then restore it to true current focus.
    // This is the case when we've been getting DOMMenuItemActive events
    // inside of a combo box that closes. The real focus is on the combo box.
    if (!gLastFocusedNode) {
      return NS_OK;
    }
    nsCOMPtr<nsIDOMNode> parentOfFocus;
    gLastFocusedNode->GetParentNode(getter_AddRefs(parentOfFocus));
    if (parentOfFocus != targetNode) {
      return NS_OK;
    }
    // Focus was inside of popup that's being hidden
    FireCurrentFocusEvent();
  }
  else if (eventType.LowerCaseEqualsLiteral("popupshown")) {
    FireAccessibleFocusEvent(accessible, targetNode);
  }
  else if (eventType.EqualsLiteral("DOMMenuInactive")) {
    //FireAccessibleFocusEvent(accessible, targetNode);  // Not yet used in ATK
  }
#endif
  return NS_OK;
}

void nsRootAccessible::GetTargetNode(nsIDOMEvent *aEvent, nsIDOMNode **aTargetNode)
{
  *aTargetNode = nsnull;

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));

  if (nsevent) {
    nsCOMPtr<nsIDOMEventTarget> domEventTarget;
    nsevent->GetOriginalTarget(getter_AddRefs(domEventTarget));
    nsCOMPtr<nsIContent> content(do_QueryInterface(domEventTarget));
    nsIContent *bindingParent;
    if (content && content->IsContentOfType(nsIContent::eHTML) &&
      (bindingParent = content->GetBindingParent()) != nsnull) {
      // Use binding parent when the event occurs in 
      // anonymous HTML content.
      // This gets the following important cases correct:
      // 1. Inserted <dialog> buttons like OK, Cancel, Help.
      // 2. XUL menulists and comboboxes.
      // 3. The focused radio button in a group.
      CallQueryInterface(bindingParent, aTargetNode);
      NS_ASSERTION(*aTargetNode, "No target node for binding parent of anonymous event target");
      return;
    }
    if (domEventTarget) {
      CallQueryInterface(domEventTarget, aTargetNode);
    }
  }
}

void nsRootAccessible::FireFocusCallback(nsITimer *aTimer, void *aClosure)
{
  nsRootAccessible *rootAccessible = NS_STATIC_CAST(nsRootAccessible*, aClosure);
  NS_ASSERTION(rootAccessible, "How did we get here without a root accessible?");
  rootAccessible->FireCurrentFocusEvent();
}

// ------- nsIDOMFocusListener Methods (1) -------------

NS_IMETHODIMP nsRootAccessible::Focus(nsIDOMEvent* aEvent) 
{ 
  return HandleEvent(aEvent);
}

NS_IMETHODIMP nsRootAccessible::Blur(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
}

// ------- nsIDOMFormListener Methods (5) -------------

NS_IMETHODIMP nsRootAccessible::Submit(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsRootAccessible::Reset(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsRootAccessible::Change(nsIDOMEvent* aEvent)
{
  // get change events when the form elements changes its state, checked->not,
  // deleted text, new text, change in selection for list/combo boxes
  // this may be the event that we have the individual Accessible objects
  // handle themselves -- have list/combos figure out the change in selection
  // have textareas and inputs fire a change of state etc...
  return NS_OK;   // Ignore form change events in MSAA
}

// gets Select events when text is selected in a textarea or input
NS_IMETHODIMP nsRootAccessible::Select(nsIDOMEvent* aEvent) 
{
  return HandleEvent(aEvent);
}

// gets Input events when text is entered or deleted in a textarea or input
NS_IMETHODIMP nsRootAccessible::Input(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
}

// ------- nsIDOMXULListener Methods (8) ---------------

NS_IMETHODIMP nsRootAccessible::PopupShowing(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::PopupShown(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::PopupHiding(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::PopupHidden(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Close(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Command(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Broadcast(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::CommandUpdate(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Shutdown()
{
  // Called manually or by nsAccessNode::~nsAccessNode()
  if (!mWeakShell) {
    return NS_OK;  // Already shutdown
  }
  mCaretAccessible = nsnull;
  mAccService = nsnull;
  if (mFireFocusTimer) {
    mFireFocusTimer->Cancel();
    mFireFocusTimer = nsnull;
  }

  return nsDocAccessibleWrap::Shutdown();
}


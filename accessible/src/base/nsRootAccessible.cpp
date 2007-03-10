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
#include "nsIBaseWindow.h"
#include "nsICaret.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMNSEvent.h"
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
#include "nsIScrollableView.h"
#include "nsISelectionPrivate.h"
#include "nsIServiceManager.h"
#include "nsIViewManager.h"
#include "nsPIDOMWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsReadableUtils.h"
#include "nsRootAccessible.h"
#include "nsIDOMNSEventTarget.h"
#include "nsIDOMDocumentEvent.h"

#ifdef MOZ_XUL
#include "nsXULTreeAccessible.h"
#include "nsIXULDocument.h"
#include "nsIXULWindow.h"
#endif

// Expanded version of NS_IMPL_ISUPPORTS_INHERITED2 
// so we can QI directly to concrete nsRootAccessible
NS_IMPL_QUERY_HEAD(nsRootAccessible)
NS_IMPL_QUERY_BODY(nsIDOMEventListener)
if (aIID.Equals(NS_GET_IID(nsRootAccessible)))
  foundInterface = NS_REINTERPRET_CAST(nsISupports*, this);
else
NS_IMPL_QUERY_TAIL_INHERITING(nsDocAccessible)

NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsDocAccessible) 
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsDocAccessible)

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsDocAccessibleWrap(aDOMNode, aShell),
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

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
    GetDocShellTreeItemFor(mDOMNode);
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

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

#ifdef MOZ_XUL
PRUint32 nsRootAccessible::GetChromeFlags()
{
  // Return the flag set for the top level window as defined 
  // by nsIWebBrowserChrome::CHROME_WINDOW_[FLAGNAME]
  // Not simple: nsIXULWindow is not just a QI from nsIDOMWindow
  nsCOMPtr<nsIDocShellTreeItem> treeItem = GetDocShellTreeItemFor(mDOMNode);
  NS_ENSURE_TRUE(treeItem, 0);
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, 0);
  nsCOMPtr<nsIXULWindow> xulWin(do_GetInterface(treeOwner));
  if (!xulWin) {
    return 0;
  }
  PRUint32 chromeFlags;
  xulWin->GetChromeFlags(&chromeFlags);
  return chromeFlags;
}
#endif

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

#ifdef MOZ_XUL
  PRUint32 chromeFlags = GetChromeFlags();
  if (chromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) {
    *aState |= STATE_SIZEABLE;
  }
  if (chromeFlags & nsIWebBrowserChrome::CHROME_TITLEBAR) {
    // If it has a titlebar it's movable
    // XXX unless it's minimized or maximized, but not sure
    //     how to detect that
    *aState |= STATE_MOVEABLE;
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::GetExtState(PRUint32 *aExtState)
{
  nsDocAccessibleWrap::GetExtState(aExtState);

  nsCOMPtr<nsIDOMWindow> domWin;
  GetWindow(getter_AddRefs(domWin));
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(domWin));
  if (privateDOMWindow) {
    nsIFocusController *focusController =
      privateDOMWindow->GetRootFocusController();
    PRBool isActive = PR_FALSE;
    focusController->GetActive(&isActive);
    if (isActive) {
      *aExtState |= EXT_STATE_ACTIVE;
    }
  }
#ifdef MOZ_XUL
  if (GetChromeFlags() & nsIWebBrowserChrome::CHROME_MODAL) {
    *aExtState |= EXT_STATE_MODAL;
  }
#endif

  return NS_OK;
}

void
nsRootAccessible::GetChromeEventHandler(nsIDOMEventTarget **aChromeTarget)
{
  nsCOMPtr<nsIDOMWindow> domWin;
  GetWindow(getter_AddRefs(domWin));
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(domWin));
  nsCOMPtr<nsPIDOMEventTarget> chromeEventHandler;
  if (privateDOMWindow) {
    chromeEventHandler = privateDOMWindow->GetChromeEventHandler();
  }

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(chromeEventHandler));

  *aChromeTarget = target;
  NS_IF_ADDREF(*aChromeTarget);
}

const char* const docEvents[] = {
  // capture DOM focus events 
  "focus",
  // capture Form change events 
  "select",
  // capture NameChange events (fired whenever name changes, immediately after, whether focus moves or not)
  "NameChange",
  // capture ValueChange events (fired whenever value changes, immediately after, whether focus moves or not)
  "ValueChange",
  // capture AlertActive events (fired whenever alert pops up)
  "AlertActive",
  // add ourself as a TreeViewChanged listener (custom event fired in nsTreeBodyFrame.cpp)
  "TreeViewChanged",
  // add ourself as a OpenStateChange listener (custom event fired in tree.xml)
  "OpenStateChange",
  // add ourself as a CheckboxStateChange listener (custom event fired in nsHTMLInputElement.cpp)
  "CheckboxStateChange",
  // add ourself as a RadioStateChange Listener ( custom event fired in in nsHTMLInputElement.cpp  & radio.xml)
  "RadioStateChange",
  "popupshown",
  "popuphiding",
  "DOMMenuInactive",
  "DOMMenuItemActive",
  "DOMMenuBarActive",
  "DOMMenuBarInactive",
  "DOMContentLoaded"
};

nsresult nsRootAccessible::AddEventListeners()
{
  // nsIDOMNSEventTarget interface allows to register event listeners to
  // receive untrusted events (synthetic events generated by untrusted code).
  // For example, XBL bindings implementations for elements that are hosted in
  // non chrome document fire untrusted events.
  nsCOMPtr<nsIDOMNSEventTarget> nstarget(do_QueryInterface(mDocument));

  if (nstarget) {
    for (const char* const* e = docEvents,
                   * const* e_end = docEvents + NS_ARRAY_LENGTH(docEvents);
         e < e_end; ++e) {
      nsresult rv = nstarget->AddEventListener(NS_ConvertASCIItoUTF16(*e),
                                               this, PR_TRUE, PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsCOMPtr<nsIDOMEventTarget> target;
  GetChromeEventHandler(getter_AddRefs(target));
  if (target) {
    target->AddEventListener(NS_LITERAL_STRING("pagehide"), this, PR_TRUE);
  }

  if (!mCaretAccessible) {
    mCaretAccessible = new nsCaretAccessible(mDOMNode, mWeakShell, this);
    nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(mCaretAccessible));
    if (accessNode && NS_FAILED(accessNode->Init())) {
      mCaretAccessible = nsnull;
    }
  }

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
    for (const char* const* e = docEvents,
                   * const* e_end = docEvents + NS_ARRAY_LENGTH(docEvents);
         e < e_end; ++e) {
      nsresult rv = target->RemoveEventListener(NS_ConvertASCIItoUTF16(*e), this, PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  GetChromeEventHandler(getter_AddRefs(target));
  if (target) {
    target->RemoveEventListener(NS_LITERAL_STRING("pagehide"), this, PR_TRUE);
  }

  if (mCaretAccessible) {
    mCaretAccessible->RemoveSelectionListener();
    mCaretAccessible = nsnull;
  }

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

void nsRootAccessible::TryFireEarlyLoadEvent(nsIDOMNode *aDocNode)
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
  FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_INTERNAL_LOAD, aDocNode,
                          nsnull, PR_FALSE);
}

void nsRootAccessible::FireAccessibleFocusEvent(nsIAccessible *aAccessible,
                                                nsIDOMNode *aNode,
                                                nsIDOMEvent *aFocusEvent,
                                                PRBool aForceEvent)
{
  if (mCaretAccessible) {
    nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aFocusEvent));
    if (nsevent) {
      // Use the originally focused node where the selection lives.
      // For example, use the anonymous HTML:input instead of the containing
      // XUL:textbox. In this case, sometimes it is a later focus event
      // which points to the actual anonymous child with focus, so to be safe 
      // we need to reset the selection listener every time.
      // This happens because when some bindings handle focus, they retarget
      // focus to the appropriate child inside of themselves, but DOM focus
      // stays outside on that binding parent.
      nsCOMPtr<nsIDOMEventTarget> domEventTarget;
      nsevent->GetOriginalTarget(getter_AddRefs(domEventTarget));
      nsCOMPtr<nsIDOMNode> realFocusedNode(do_QueryInterface(domEventTarget));
      if (realFocusedNode) {
        mCaretAccessible->AttachNewSelectionListener(realFocusedNode);
      }
    }
  }

  // Check for aaa:activedescendant, which changes which element has focus
  nsCOMPtr<nsIDOMNode> finalFocusNode = aNode;
  nsCOMPtr<nsIAccessible> finalFocusAccessible = aAccessible;
  nsCOMPtr<nsIContent> finalFocusContent  = do_QueryInterface(aNode);
  if (finalFocusContent) {
    nsAutoString id;
    if (finalFocusContent->GetAttr(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::activedescendant, id)) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      aNode->GetOwnerDocument(getter_AddRefs(domDoc));
      if (!domDoc) {
        return;
      }
      nsCOMPtr<nsIDOMElement> relatedEl;
      domDoc->GetElementById(id, getter_AddRefs(relatedEl));
      finalFocusContent = do_QueryInterface(relatedEl);
      if (!finalFocusContent) {
        return;
      }
      GetAccService()->GetAccessibleFor(finalFocusNode, getter_AddRefs(finalFocusAccessible));      
      // XXX Deal with case where finalFocusNode is not in parent chain of original true focus (aNode).
      // We need direction from ARIA spec -- should we just return because it's not valid?
      // Or should we put focus on the original container node?
    }
  }

  // Fire focus only if it changes, but always fire focus events when aForceEvent == PR_TRUE
  if (gLastFocusedNode == finalFocusNode && !aForceEvent) {
    return;
  }

  nsCOMPtr<nsPIAccessible> privateAccessible =
    do_QueryInterface(finalFocusAccessible);
  NS_ASSERTION(privateAccessible , "No nsPIAccessible for nsIAccessible");
  if (!privateAccessible) {
    return;
  }

  // Use focus events on DHTML menuitems to indicate when to fire menustart and menuend
  // Special dynamic content handling
  PRUint32 role = ROLE_NOTHING;
  finalFocusAccessible->GetFinalRole(&role);
  if (role == ROLE_MENUITEM) {
    if (!mIsInDHTMLMenu) {  // Entering menus
      PRUint32 naturalRole; // The natural role is the role that this type of element normally has
      finalFocusAccessible->GetRole(&naturalRole);
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
  gLastFocusedNode = finalFocusNode;
  NS_IF_ADDREF(gLastFocusedNode);

  privateAccessible->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS,
                                      finalFocusAccessible, nsnull);
}

void nsRootAccessible::FireCurrentFocusEvent()
{
  nsCOMPtr<nsIDOMNode> focusedNode = GetCurrentFocus();

  // Simulate a focus event so that we can reuse code that fires focus for container children like treeitems
  nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(mDocument);
  if (docEvent) {
    nsCOMPtr<nsIDOMEvent> event;
    if (NS_SUCCEEDED(docEvent->CreateEvent(NS_LITERAL_STRING("Events"),
                                           getter_AddRefs(event))) &&
        NS_SUCCEEDED(event->InitEvent(NS_LITERAL_STRING("focus"), PR_TRUE, PR_TRUE))) {
      // Get the target node we really want for the event.
      nsIAccessibilityService* accService = GetAccService();
      if (accService) {
        nsCOMPtr<nsIDOMNode> targetNode;
        accService->GetRelevantContentNodeFor(focusedNode,
                                            getter_AddRefs(targetNode));
        if (targetNode) {
          HandleEventWithTarget(event, targetNode);
        }
      }
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
  
  return HandleEventWithTarget(aEvent, targetNode);
}

/* virtual */
nsresult nsRootAccessible::HandleEventWithTarget(nsIDOMEvent* aEvent,
                                                 nsIDOMNode* aTargetNode)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  nsAutoString localName;
  aTargetNode->GetLocalName(localName);
#ifdef DEBUG_A11Y
  // Very useful for debugging, please leave this here.
  if (eventType.LowerCaseEqualsLiteral("alertactive")) {
    printf("\ndebugging %s events for %s", NS_ConvertUTF16toUTF8(eventType).get(), NS_ConvertUTF16toUTF8(localName).get());
  }
  if (localName.LowerCaseEqualsLiteral("textbox")) {
    printf("\ndebugging %s events for %s", NS_ConvertUTF16toUTF8(eventType).get(), NS_ConvertUTF16toUTF8(localName).get());
  }
#endif

  nsCOMPtr<nsIPresShell> eventShell = GetPresShellFor(aTargetNode);
  if (!eventShell) {
    return NS_OK;
  }
      
  nsIAccessibilityService *accService = GetAccService();
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  if (eventType.LowerCaseEqualsLiteral("pagehide")) {
    // pagehide event can be fired under several conditions, such as HTML
    // document going away, closing a window/dialog, and wizard page changing.
    // We only destroy the accessible object when it's a document accessible,
    // so that we don't destroy something still in use, like wizard page. 
    // And we only get cached document accessible to destroy, so that we don't
    // create it just to destroy it.
    nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(eventShell));
    nsCOMPtr<nsIAccessible> accessible;
    accService->GetCachedAccessible(aTargetNode, weakShell, getter_AddRefs(accessible));
    nsCOMPtr<nsPIAccessibleDocument> privateAccDoc = do_QueryInterface(accessible);
    if (privateAccDoc) {
      privateAccDoc->Destroy();
    }
    return NS_OK;
  }

  if (eventType.LowerCaseEqualsLiteral("popupshown")) {
    // Fire menupopupstart events after a delay so that ancestor views
    // are visible, otherwise an accessible cannot be created for the
    // popup and the accessibility toolkit event can't be fired.
    nsCOMPtr<nsIDOMXULPopupElement> popup(do_QueryInterface(aTargetNode));
    if (popup) {
      PRInt32 event = nsIAccessibleEvent::EVENT_MENUPOPUPSTART;
      nsCOMPtr<nsIContent> content(do_QueryInterface(aTargetNode));
      if (content->NodeInfo()->Equals(nsAccessibilityAtoms::tooltip, kNameSpaceID_XUL)) {
        // There is a single <xul:tooltip> node which Mozilla moves around.
        // The accessible for it stays the same no matter where it moves. 
        // AT's expect to get an EVENT_SHOW for the tooltip. 
        // In event callback the tooltip's accessible will be ready.
        event = nsIAccessibleEvent::EVENT_SHOW;
      }
      return FireDelayedToolkitEvent(event, aTargetNode, nsnull);
    }
  }

  if (eventType.LowerCaseEqualsLiteral("domcontentloaded")) {
    // Don't create the doc accessible until load scripts have a chance to set
    // role attribute for <body> or <html> element, because the value of 
    // role attribute will be cached when the doc accessible is Init()'d
    TryFireEarlyLoadEvent(aTargetNode);
    return NS_OK;
  }

  if (eventType.EqualsLiteral("TreeViewChanged")) {
    NS_ENSURE_TRUE(localName.EqualsLiteral("tree"), NS_OK);
    nsCOMPtr<nsIContent> treeContent = do_QueryInterface(aTargetNode);

    return accService->InvalidateSubtreeFor(eventShell, treeContent,
                                            nsIAccessibleEvent::EVENT_REORDER);
  }

  nsCOMPtr<nsIAccessible> accessible;
  accService->GetAccessibleInShell(aTargetNode, eventShell,
                                   getter_AddRefs(accessible));
  if (!accessible)
    return NS_OK;

#ifdef MOZ_XUL
  // If it's a tree element, need the currently selected item
  nsCOMPtr<nsIAccessible> treeItemAccessible;
  if (localName.EqualsLiteral("tree")) {
    nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
      do_QueryInterface(aTargetNode);
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
      FireAccessibleFocusEvent(accessible, aTargetNode, aEvent); // Tree has focus
    }
    else if (eventType.LowerCaseEqualsLiteral("dommenuitemactive")) {
      FireAccessibleFocusEvent(treeItemAccessible, aTargetNode, aEvent, PR_TRUE);
    }
    else if (eventType.LowerCaseEqualsLiteral("namechange")) {
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, 
                                accessible, nsnull);
    }
    else if (eventType.LowerCaseEqualsLiteral("select")) {
      // If multiselect tree, we should fire selectionadd or selection removed
      if (gLastFocusedNode == aTargetNode) {
        nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSel =
          do_QueryInterface(aTargetNode);
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
  if (eventType.LowerCaseEqualsLiteral("focus")) {
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
      do_QueryInterface(aTargetNode);
    // Keep a reference to the target node. We might want to change
    // it to the individual radio button or selected item, and send
    // the focus event to that.
    nsCOMPtr<nsIDOMNode> focusedItem(aTargetNode);
    if (selectControl) {
      nsCOMPtr<nsIDOMXULMenuListElement> menuList =
        do_QueryInterface(aTargetNode);
      if (!menuList) {
        // Don't do this for menu lists, the items only get focused
        // when the list is open, based on DOMMenuitemActive events
        nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
        selectControl->GetSelectedItem(getter_AddRefs(selectedItem));
        if (selectedItem) {
          focusedItem = do_QueryInterface(selectedItem);
        }

        if (!focusedItem)
          return NS_OK;

        accService->GetAccessibleInShell(focusedItem, eventShell,
                                         getter_AddRefs(accessible));
        if (!accessible)
          return NS_OK;
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
    FireAccessibleFocusEvent(accessible, focusedItem, aEvent);
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
      accessible->GetValue(value);
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
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, 
                              accessible, nsnull);
    PRUint32 finalState;
    accessible->GetFinalState(&finalState);
    if (finalState & (STATE_CHECKED | STATE_SELECTED)) {
      FireAccessibleFocusEvent(accessible, aTargetNode, aEvent);
    }
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
    if (parentOfFocus != aTargetNode) {
      return NS_OK;
    }
    // Focus was inside of popup that's being hidden
    FireCurrentFocusEvent();
  }
  else if (eventType.EqualsLiteral("DOMMenuInactive")) {
    nsCOMPtr<nsIDOMXULPopupElement> popup(do_QueryInterface(aTargetNode));
    if (popup) {
      privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUPOPUPEND,
                                accessible, nsnull);
    }
  }
  else
#endif
  if (eventType.LowerCaseEqualsLiteral("dommenuitemactive")) {
    nsCOMPtr<nsIAccessible> containerAccessible;
    accessible->GetParent(getter_AddRefs(containerAccessible));
    NS_ENSURE_TRUE(containerAccessible, NS_OK);
    if (Role(containerAccessible) == ROLE_MENUBAR) {
      // It is top level menuitem
      // Only fire focus event if it is not collapsed
      if (State(accessible) & STATE_COLLAPSED)
        return NS_OK;
    }
    else {
      // It is not top level menuitem
      // Only fire focus event if it is not inside collapsed popup
      if (State(containerAccessible) & STATE_COLLAPSED)
        return NS_OK;
    }
    FireAccessibleFocusEvent(accessible, aTargetNode, aEvent, PR_TRUE);
  }
  else if (eventType.LowerCaseEqualsLiteral("dommenubaractive")) {
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUSTART,
                              accessible, nsnull);
  }
  else if (eventType.LowerCaseEqualsLiteral("dommenubarinactive")) {
    privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_MENUEND,
                              accessible, nsnull);
    FireCurrentFocusEvent();
  }
  return NS_OK;
}

void nsRootAccessible::GetTargetNode(nsIDOMEvent *aEvent, nsIDOMNode **aTargetNode)
{
  *aTargetNode = nsnull;

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));

  if (!nsevent)
    return;

  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  nsevent->GetOriginalTarget(getter_AddRefs(domEventTarget));
  nsCOMPtr<nsIDOMNode> eventTarget(do_QueryInterface(domEventTarget));
  if (!eventTarget)
    return;

  nsIAccessibilityService* accService = GetAccService();
  if (accService) {
    nsresult rv = accService->GetRelevantContentNodeFor(eventTarget,
                                                        aTargetNode);
    if (NS_SUCCEEDED(rv) && *aTargetNode)
      return;
  }

  NS_ADDREF(*aTargetNode = eventTarget);
}

void nsRootAccessible::FireFocusCallback(nsITimer *aTimer, void *aClosure)
{
  nsRootAccessible *rootAccessible = NS_STATIC_CAST(nsRootAccessible*, aClosure);
  NS_ASSERTION(rootAccessible, "How did we get here without a root accessible?");
  rootAccessible->FireCurrentFocusEvent();
}

NS_IMETHODIMP nsRootAccessible::Shutdown()
{
  // Called manually or by nsAccessNode::~nsAccessNode()
  if (!mWeakShell) {
    return NS_OK;  // Already shutdown
  }
  mCaretAccessible = nsnull;
  if (mFireFocusTimer) {
    mFireFocusTimer->Cancel();
    mFireFocusTimer = nsnull;
  }

  return nsDocAccessibleWrap::Shutdown();
}

already_AddRefed<nsIDocShellTreeItem>
nsRootAccessible::GetContentDocShell(nsIDocShellTreeItem *aStart)
{
  if (!aStart) {
    return nsnull;
  }

  PRInt32 itemType;
  aStart->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeContent) {
    nsCOMPtr<nsIAccessibleDocument> accDoc =
      GetDocAccessibleFor(aStart, PR_TRUE);
    nsCOMPtr<nsIAccessible> accessible = do_QueryInterface(accDoc);
    // If ancestor chain of accessibles is not completely visible,
    // don't use this one. This happens for example if it's inside
    // a background tab (tabbed browsing)
    while (accessible) {
      if (State(accessible) & STATE_INVISIBLE) {
        return nsnull;
      }
      nsCOMPtr<nsIAccessible> ancestor;
      accessible->GetParent(getter_AddRefs(ancestor));
      accessible.swap(ancestor);
    }

    NS_ADDREF(aStart);
    return aStart;
  }
  nsCOMPtr<nsIDocShellTreeNode> treeNode(do_QueryInterface(aStart));
  if (treeNode) {
    PRInt32 subDocuments;
    treeNode->GetChildCount(&subDocuments);
    for (PRInt32 count = 0; count < subDocuments; count ++) {
      nsCOMPtr<nsIDocShellTreeItem> treeItemChild, contentTreeItem;
      treeNode->GetChildAt(count, getter_AddRefs(treeItemChild));
      NS_ENSURE_TRUE(treeItemChild, nsnull);
      contentTreeItem = GetContentDocShell(treeItemChild);
      if (contentTreeItem) {
        NS_ADDREF(aStart = contentTreeItem);
        return aStart;
      }
    }
  }
  return nsnull;
}

NS_IMETHODIMP nsRootAccessible::GetAccessibleRelated(PRUint32 aRelationType,
                                                     nsIAccessible **aRelated)
{
  *aRelated = nsnull;

  if (!mDOMNode || aRelationType != RELATION_EMBEDS) {
    return nsDocAccessibleWrap::GetAccessibleRelated(aRelationType, aRelated);
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem = GetDocShellTreeItemFor(mDOMNode);   
  nsCOMPtr<nsIDocShellTreeItem> contentTreeItem = GetContentDocShell(treeItem);
  // there may be no content area, so we need a null check
  if (contentTreeItem) {
    nsCOMPtr<nsIAccessibleDocument> accDoc =
      GetDocAccessibleFor(contentTreeItem, PR_TRUE);
    NS_ASSERTION(accDoc, "No EMBEDS document");
    if (accDoc) {
      accDoc->QueryInterface(NS_GET_IID(nsIAccessible), (void**)aRelated);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::FireDocLoadEvents(PRUint32 aEventType)
{
  if (!mDocument || !mWeakShell) {
    return NS_OK;  // Document has been shut down
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsAccessNode::GetDocShellTreeItemFor(mDOMNode);
  NS_ASSERTION(docShellTreeItem, "No doc shell tree item for document");
  NS_ENSURE_TRUE(docShellTreeItem, NS_ERROR_FAILURE);
  PRInt32 contentType;
  docShellTreeItem->GetItemType(&contentType);
  if (contentType == nsIDocShellTreeItem::typeContent) {
    return nsDocAccessibleWrap::FireDocLoadEvents(aEventType); // Content might need to fire event
  }

  // Root chrome: don't fire event
  mIsContentLoaded = (aEventType == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE ||
                      aEventType == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED);

  return NS_OK;
}

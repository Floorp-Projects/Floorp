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
#include "nsHTMLSelectAccessible.h"
#include "nsIBaseWindow.h"
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
#include "nsIDOMDataContainerEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIFocusController.h"
#include "nsIFrame.h"
#include "nsIMenuFrame.h"
#include "nsIHTMLDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMenuParent.h"
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

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsAppRootAccessible.h"
#else
#include "nsApplicationAccessibleWrap.h"
#endif

// Expanded version of NS_IMPL_ISUPPORTS_INHERITED2 
// so we can QI directly to concrete nsRootAccessible
NS_IMPL_QUERY_HEAD(nsRootAccessible)
NS_IMPL_QUERY_BODY(nsIDOMEventListener)
if (aIID.Equals(NS_GET_IID(nsRootAccessible)))
  foundInterface = reinterpret_cast<nsISupports*>(this);
else
NS_IMPL_QUERY_TAIL_INHERITING(nsDocAccessible)

NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsDocAccessible) 
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsDocAccessible)

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsDocAccessibleWrap(aDOMNode, aShell)
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
NS_IMETHODIMP
nsRootAccessible::GetName(nsAString& aName)
{
  aName.Truncate();

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
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
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
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nsnull;

  nsRefPtr<nsApplicationAccessibleWrap> root = GetApplicationAccessible();
  NS_IF_ADDREF(*aParent = root);

  return NS_OK;
}

/* readonly attribute unsigned long accRole; */
NS_IMETHODIMP nsRootAccessible::GetRole(PRUint32 *aRole) 
{ 
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  // If it's a <dialog> or <wizard>, use nsIAccessibleRole::ROLE_DIALOG instead
  nsIContent *rootContent = mDocument->GetRootContent();
  if (rootContent) {
    nsCOMPtr<nsIDOMElement> rootElement(do_QueryInterface(rootContent));
    if (rootElement) {
      nsAutoString name;
      rootElement->GetLocalName(name);
      if (name.EqualsLiteral("dialog") || name.EqualsLiteral("wizard")) {
        *aRole = nsIAccessibleRole::ROLE_DIALOG; // Always at the root
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
  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
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

nsresult
nsRootAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsDocAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!mDOMNode)
    return NS_OK;

#ifdef MOZ_XUL
  PRUint32 chromeFlags = GetChromeFlags();
  if (chromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) {
    *aState |= nsIAccessibleStates::STATE_SIZEABLE;
  }
  if (chromeFlags & nsIWebBrowserChrome::CHROME_TITLEBAR) {
    // If it has a titlebar it's movable
    // XXX unless it's minimized or maximized, but not sure
    //     how to detect that
    *aState |= nsIAccessibleStates::STATE_MOVEABLE;
  }
#endif

  if (!aExtraState)
    return NS_OK;

  nsCOMPtr<nsIDOMWindow> domWin;
  GetWindow(getter_AddRefs(domWin));
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow(do_QueryInterface(domWin));
  if (privateDOMWindow) {
    nsIFocusController *focusController =
      privateDOMWindow->GetRootFocusController();
    if (focusController) {
      PRBool isActive = PR_FALSE;
      focusController->GetActive(&isActive);
      if (isActive) {
        *aExtraState |= nsIAccessibleStates::EXT_STATE_ACTIVE;
      }
    }
  }
#ifdef MOZ_XUL
  if (GetChromeFlags() & nsIWebBrowserChrome::CHROME_MODAL) {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_MODAL;
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
#ifdef DEBUG
  // Capture mouse over events and fire fake DRAGDROPSTART event to simplify
  // debugging a11y objects with event viewers
  "mouseover",
#endif
  // capture DOM focus events 
  "focus",
  // capture Form change events 
  "select",
  // capture ValueChange events (fired whenever value changes, immediately after, whether focus moves or not)
  "ValueChange",
  // capture AlertActive events (fired whenever alert pops up)
  "AlertActive",
  // add ourself as a TreeViewChanged listener (custom event fired in nsTreeBodyFrame.cpp)
  "TreeViewChanged",
  "TreeRowCountChanged",
  "TreeInvalidated",
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
    mCaretAccessible = new nsCaretAccessible(this);
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

  // Do this before removing clearing caret accessible, so that it can use
  // shutdown the caret accessible's selection listener
  nsDocAccessible::RemoveEventListeners();

  if (mCaretAccessible) {
    mCaretAccessible->Shutdown();
    mCaretAccessible = nsnull;
  }

  return NS_OK;
}

nsCaretAccessible*
nsRootAccessible::GetCaretAccessible()
{
  return mCaretAccessible;
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
    nsCoreUtils::GetDocShellTreeItemFor(aDocNode);
  NS_ASSERTION(treeItem, "No docshelltreeitem for aDocNode");
  if (!treeItem) {
    return;
  }
  PRInt32 itemType;
  treeItem->GetItemType(&itemType);
  if (itemType != nsIDocShellTreeItem::typeContent) {
    return;
  }

  // The doc accessible should already be created as a result of the
  // OnStateChange() for the initiation of page loading
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
  if (rootContentTreeItem == treeItem) {
    // No frames or iframes, so we can fire the doc load finished event early
    FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_INTERNAL_LOAD, aDocNode);
  }
}

PRBool nsRootAccessible::FireAccessibleFocusEvent(nsIAccessible *aAccessible,
                                                  nsIDOMNode *aNode,
                                                  nsIDOMEvent *aFocusEvent,
                                                  PRBool aForceEvent,
                                                  PRBool aIsAsynch)
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
      if (!realFocusedNode) {
        // When FireCurrentFocusEvent() synthesizes a focus event,
        // the orignal target does not exist, so use the passed-in node
        // which is the relevant focused node
        realFocusedNode = aNode;
      }
      if (realFocusedNode) {
        mCaretAccessible->SetControlSelectionListener(realFocusedNode);
      }
    }
  }

  // Check for aria-activedescendant, which changes which element has focus
  nsCOMPtr<nsIDOMNode> finalFocusNode = aNode;
  nsCOMPtr<nsIAccessible> finalFocusAccessible = aAccessible;
  nsCOMPtr<nsIContent> finalFocusContent =
    nsCoreUtils::GetRoleContent(finalFocusNode);

  if (finalFocusContent) {
    nsAutoString id;
    if (finalFocusContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_activedescendant, id)) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      aNode->GetOwnerDocument(getter_AddRefs(domDoc));
      if (!domDoc) {  // Maybe the passed-in node actually is a doc
        domDoc = do_QueryInterface(aNode);
      }
      if (!domDoc) {
        return PR_FALSE;
      }
      nsCOMPtr<nsIDOMElement> relatedEl;
      domDoc->GetElementById(id, getter_AddRefs(relatedEl));
      finalFocusNode = do_QueryInterface(relatedEl);
      if (!finalFocusNode) {
        // If aria-activedescendant is set to nonextistant ID, then treat as focus
        // on the activedescendant container (which has real DOM focus)
        finalFocusNode = aNode;
      }
      finalFocusAccessible = nsnull;
    }
  }

  // Fire focus only if it changes, but always fire focus events when aForceEvent == PR_TRUE
  if (gLastFocusedNode == finalFocusNode && !aForceEvent) {
    return PR_FALSE;
  }

  if (!finalFocusAccessible) {
    GetAccService()->GetAccessibleFor(finalFocusNode, getter_AddRefs(finalFocusAccessible));      
    // For activedescendant, the ARIA spec does not require that the user agent
    // checks whether finalFocusNode is actually a descendant of the element with
    // the activedescendant attribute.
    if (!finalFocusAccessible) {
      return PR_FALSE;
    }
  }

  gLastFocusedAccessiblesState = nsAccUtils::State(finalFocusAccessible);
  PRUint32 role = nsAccUtils::Role(finalFocusAccessible);
  if (role == nsIAccessibleRole::ROLE_MENUITEM) {
    if (!mCurrentARIAMenubar) {  // Entering menus
      PRUint32 naturalRole; // The natural role is the role that this type of element normally has
      finalFocusAccessible->GetRole(&naturalRole);
      if (role != naturalRole) { // Must be a DHTML menuitem
        nsCOMPtr<nsIAccessible> menuBarAccessible =
          nsAccUtils::GetAncestorWithRole(finalFocusAccessible,
                                          nsIAccessibleRole::ROLE_MENUBAR);
        nsCOMPtr<nsIAccessNode> menuBarAccessNode = do_QueryInterface(menuBarAccessible);
        if (menuBarAccessNode) {
          menuBarAccessNode->GetDOMNode(getter_AddRefs(mCurrentARIAMenubar));
          if (mCurrentARIAMenubar) {
            nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_MENU_START,
                                     menuBarAccessible);
          }
        }
      }
    }
  }
  else if (mCurrentARIAMenubar) {
    nsCOMPtr<nsIAccessibleEvent> menuEndEvent =
      new nsAccEvent(nsIAccessibleEvent::EVENT_MENU_END, mCurrentARIAMenubar,
                     PR_FALSE, nsAccEvent::eAllowDupes);
    if (menuEndEvent) {
      FireDelayedAccessibleEvent(menuEndEvent);
    }
    mCurrentARIAMenubar = nsnull;
  }

  NS_IF_RELEASE(gLastFocusedNode);
  gLastFocusedNode = finalFocusNode;
  NS_IF_ADDREF(gLastFocusedNode);

  nsCOMPtr<nsIContent> focusContent = do_QueryInterface(gLastFocusedNode);
  nsIFrame *focusFrame = nsnull;
  if (focusContent) {
    nsCOMPtr<nsIPresShell> shell =
      nsCoreUtils::GetPresShellFor(gLastFocusedNode);
    focusFrame = shell->GetRealPrimaryFrameFor(focusContent);
  }
  gLastFocusedFrameType = (focusFrame && focusFrame->GetStyleVisibility()->IsVisible()) ? focusFrame->GetType() : 0;

  nsCOMPtr<nsIAccessibleDocument> docAccessible = do_QueryInterface(finalFocusAccessible);
  if (docAccessible) {
    // Doc is gaining focus, but actual focus may be on an element within document
    nsCOMPtr<nsIDOMNode> realFocusedNode = GetCurrentFocus();
    if (realFocusedNode != aNode || realFocusedNode == mDOMNode) {
      // Suppress document focus, because real DOM focus will be fired next,
      // and that's what we care about
      // Make sure we never fire focus for the nsRootAccessible (mDOMNode)
      
return PR_FALSE;
    }
  }

  FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS,
                          finalFocusNode, nsAccEvent::eRemoveDupes,
                          aIsAsynch);

  return PR_TRUE;
}

void nsRootAccessible::FireCurrentFocusEvent()
{
  nsCOMPtr<nsIDOMNode> focusedNode = GetCurrentFocus();
  if (!focusedNode) {
    return; // No current focus
  }

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

nsresult nsRootAccessible::HandleEventWithTarget(nsIDOMEvent* aEvent,
                                                 nsIDOMNode* aTargetNode)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  nsAutoString localName;
  aTargetNode->GetLocalName(localName);
#ifdef MOZ_XUL
  PRBool isTree = localName.EqualsLiteral("tree");
#endif
#ifdef DEBUG_A11Y
  // Very useful for debugging, please leave this here.
  if (eventType.EqualsLiteral("AlertActive")) {
    printf("\ndebugging %s events for %s", NS_ConvertUTF16toUTF8(eventType).get(), NS_ConvertUTF16toUTF8(localName).get());
  }
  if (localName.LowerCaseEqualsLiteral("textbox")) {
    printf("\ndebugging %s events for %s", NS_ConvertUTF16toUTF8(eventType).get(), NS_ConvertUTF16toUTF8(localName).get());
  }
#endif

  nsIAccessibilityService *accService = GetAccService();
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  if (eventType.EqualsLiteral("pagehide")) {
    // pagehide event can be fired under several conditions, such as HTML
    // document going away, closing a window/dialog, and wizard page changing.
    // We only destroy the accessible object when it's a document accessible,
    // so that we don't destroy something still in use, like wizard page. 
    // And we only get cached document accessible to destroy, so that we don't
    // create it just to destroy it.
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aTargetNode));
    nsCOMPtr<nsIAccessibleDocument> accDoc = GetDocAccessibleFor(doc);
    if (accDoc) {
      nsRefPtr<nsAccessNode> docAccNode = nsAccUtils::QueryAccessNode(accDoc);
      docAccNode->Shutdown();
    }

    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> eventShell = nsCoreUtils::GetPresShellFor(aTargetNode);
  if (!eventShell) {
    return NS_OK;
  }

  if (eventType.EqualsLiteral("DOMContentLoaded")) {
    // Don't create the doc accessible until load scripts have a chance to set
    // role attribute for <body> or <html> element, because the value of 
    // role attribute will be cached when the doc accessible is Init()'d
    TryFireEarlyLoadEvent(aTargetNode);
    return NS_OK;
  }

  if (eventType.EqualsLiteral("popuphiding")) {
    // If accessible focus was on or inside popup that closes,
    // then restore it to true current focus.
    // This is the case when we've been getting DOMMenuItemActive events
    // inside of a combo box that closes. The real focus is on the combo box.
    // It's also the case when a popup gets focus in ATK -- when it closes
    // we need to fire an event to restore focus to where it was
    if (!gLastFocusedNode ||
        !nsCoreUtils::IsAncestorOf(aTargetNode, gLastFocusedNode)) {
      return NS_OK;  // And was not focused on an item inside the popup
    }
    // Focus was on or inside of a popup that's being hidden
    FireCurrentFocusEvent();
  }

  nsCOMPtr<nsIAccessible> accessible;
  accService->GetAccessibleInShell(aTargetNode, eventShell,
                                   getter_AddRefs(accessible));
  nsCOMPtr<nsPIAccessible> privAcc(do_QueryInterface(accessible));
  if (!privAcc)
    return NS_OK;

#ifdef MOZ_XUL
  if (isTree) {
    nsCOMPtr<nsIAccessibleTreeCache> treeAcc(do_QueryInterface(accessible));
    NS_ASSERTION(treeAcc,
                 "Accessible for xul:tree doesn't implement nsIAccessibleTreeCache interface.");

    if (treeAcc) {
      if (eventType.EqualsLiteral("TreeViewChanged"))
        return treeAcc->TreeViewChanged();

      if (eventType.EqualsLiteral("TreeRowCountChanged"))
        return HandleTreeRowCountChangedEvent(aEvent, treeAcc);
      
      if (eventType.EqualsLiteral("TreeInvalidated"))
        return HandleTreeInvalidatedEvent(aEvent, treeAcc);
    }
  }
#endif

  if (eventType.EqualsLiteral("RadioStateChange")) {
    PRUint32 state = nsAccUtils::State(accessible);

    // radiogroup in prefWindow is exposed as a list,
    // and panebutton is exposed as XULListitem in A11y.
    // nsXULListitemAccessible::GetStateInternal uses STATE_SELECTED in this case,
    // so we need to check nsIAccessibleStates::STATE_SELECTED also.
    PRBool isEnabled = (state & (nsIAccessibleStates::STATE_CHECKED |
                        nsIAccessibleStates::STATE_SELECTED)) != 0;

    nsCOMPtr<nsIAccessibleStateChangeEvent> accEvent =
      new nsAccStateChangeEvent(accessible, nsIAccessibleStates::STATE_CHECKED,
                                PR_FALSE, isEnabled);
    privAcc->FireAccessibleEvent(accEvent);

    if (isEnabled)
      FireAccessibleFocusEvent(accessible, aTargetNode, aEvent);

    return NS_OK;
  }

  if (eventType.EqualsLiteral("CheckboxStateChange")) {
    PRUint32 state = nsAccUtils::State(accessible);

    PRBool isEnabled = !!(state & nsIAccessibleStates::STATE_CHECKED);

    nsCOMPtr<nsIAccessibleStateChangeEvent> accEvent =
      new nsAccStateChangeEvent(accessible,
                                nsIAccessibleStates::STATE_CHECKED,
                                PR_FALSE, isEnabled);

    return privAcc->FireAccessibleEvent(accEvent);
  }

  nsCOMPtr<nsIAccessible> treeItemAccessible;
#ifdef MOZ_XUL
  // If it's a tree element, need the currently selected item
  if (isTree) {
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

#ifdef MOZ_XUL
  if (treeItemAccessible && eventType.EqualsLiteral("OpenStateChange")) {
    PRUint32 state = nsAccUtils::State(accessible); // collapsed/expanded changed
    PRBool isEnabled = (state & nsIAccessibleStates::STATE_EXPANDED) != 0;

    nsCOMPtr<nsIAccessibleStateChangeEvent> accEvent =
      new nsAccStateChangeEvent(accessible, nsIAccessibleStates::STATE_EXPANDED,
                                PR_FALSE, isEnabled);
    return FireAccessibleEvent(accEvent);
  }

  if (treeItemAccessible && eventType.EqualsLiteral("select")) {
    // If multiselect tree, we should fire selectionadd or selection removed
    if (gLastFocusedNode == aTargetNode) {
      nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSel =
        do_QueryInterface(aTargetNode);
      nsAutoString selType;
      multiSel->GetSelType(selType);
      if (selType.IsEmpty() || !selType.EqualsLiteral("single")) {
        // XXX: We need to fire EVENT_SELECTION_ADD and EVENT_SELECTION_REMOVE
        // for each tree item. Perhaps each tree item will need to cache its
        // selection state and fire an event after a DOM "select" event when
        // that state changes. nsXULTreeAccessible::UpdateTreeSelection();
        return nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN,
                                        accessible);
      }

      return nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_SELECTION,
                                      treeItemAccessible);
    }
  }
  else
#endif
  if (eventType.EqualsLiteral("focus")) {
    if (aTargetNode == mDOMNode && mDOMNode != gLastFocusedNode) {
      // Got focus event for the window, we will make sure that an accessible
      // focus event for initial focus is fired. We do this on a short timer
      // because the initial focus may not have been set yet.
      if (!mFireFocusTimer) {
        mFireFocusTimer = do_CreateInstance("@mozilla.org/timer;1");
      }
      if (mFireFocusTimer) {
        mFireFocusTimer->InitWithFuncCallback(FireFocusCallback, this,
                                              0, nsITimer::TYPE_ONE_SHOT);
      }
    }

    // Keep a reference to the target node. We might want to change
    // it to the individual radio button or selected item, and send
    // the focus event to that.
    nsCOMPtr<nsIDOMNode> focusedItem(aTargetNode);

    if (!treeItemAccessible) {
      nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
        do_QueryInterface(aTargetNode);
      if (selectControl) {
        nsCOMPtr<nsIDOMXULMenuListElement> menuList =
          do_QueryInterface(aTargetNode);
        if (!menuList) {
          // Don't do this for menu lists, the items only get focused
          // when the list is open, based on DOMMenuitemActive events
          nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
          selectControl->GetSelectedItem(getter_AddRefs(selectedItem));
          if (selectedItem)
            focusedItem = do_QueryInterface(selectedItem);

          if (!focusedItem)
            return NS_OK;

          accService->GetAccessibleInShell(focusedItem, eventShell,
                                           getter_AddRefs(accessible));
          if (!accessible)
            return NS_OK;
        }
      }
    }
    FireAccessibleFocusEvent(accessible, focusedItem, aEvent);
  }
  else if (eventType.EqualsLiteral("AlertActive")) { 
    nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_ALERT, accessible);
  }
  else if (eventType.EqualsLiteral("popupshown")) {
    // Don't fire menupopup events for combobox and autocomplete lists
    PRUint32 role = nsAccUtils::Role(accessible);
    PRInt32 event = 0;
    if (role == nsIAccessibleRole::ROLE_MENUPOPUP) {
      event = nsIAccessibleEvent::EVENT_MENUPOPUP_START;
    }
    else if (role == nsIAccessibleRole::ROLE_TOOLTIP) {
      // There is a single <xul:tooltip> node which Mozilla moves around.
      // The accessible for it stays the same no matter where it moves. 
      // AT's expect to get an EVENT_SHOW for the tooltip. 
      // In event callback the tooltip's accessible will be ready.
      event = nsIAccessibleEvent::EVENT_ASYNCH_SHOW;
    }
    if (event) {
      nsAccUtils::FireAccEvent(event, accessible);
    }
  }
  else if (eventType.EqualsLiteral("DOMMenuInactive")) {
    if (nsAccUtils::Role(accessible) == nsIAccessibleRole::ROLE_MENUPOPUP) {
      nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_END,
                               accessible);
    }
  }
  else if (eventType.EqualsLiteral("DOMMenuItemActive")) {
    PRBool fireFocus = PR_FALSE;
    if (!treeItemAccessible) {
#ifdef MOZ_XUL
      if (isTree) {
        return NS_OK; // Tree with nothing selected
      }
#endif
      nsRefPtr<nsAccessNode> menuAccessNode =
        nsAccUtils::QueryAccessNode(accessible);
  
      nsIFrame* menuFrame = menuAccessNode->GetFrame();
      NS_ENSURE_TRUE(menuFrame, NS_ERROR_FAILURE);

      nsIMenuFrame* imenuFrame;
      CallQueryInterface(menuFrame, &imenuFrame);
      if (imenuFrame)
        fireFocus = PR_TRUE;
      // QI failed for nsIMenuFrame means it's not on menu bar
      if (imenuFrame && imenuFrame->IsOnMenuBar() &&
                       !imenuFrame->IsOnActiveMenuBar()) {
        // It is a top level menuitem. Only fire a focus event when the menu bar
        // is active.
        return NS_OK;
      } else {
        nsCOMPtr<nsIAccessible> containerAccessible;
        accessible->GetParent(getter_AddRefs(containerAccessible));
        NS_ENSURE_TRUE(containerAccessible, NS_ERROR_FAILURE);
        // It is not top level menuitem
        // Only fire focus event if it is not inside collapsed popup
        // and not a listitem of a combo box
        if (nsAccUtils::State(containerAccessible) & nsIAccessibleStates::STATE_COLLAPSED) {
          nsCOMPtr<nsIAccessible> containerParent;
          containerAccessible->GetParent(getter_AddRefs(containerParent));
          NS_ENSURE_TRUE(containerParent, NS_ERROR_FAILURE);
          if (nsAccUtils::Role(containerParent) != nsIAccessibleRole::ROLE_COMBOBOX) {
            return NS_OK;
          }
        }
      }
    }
    if (!fireFocus) {
      nsCOMPtr<nsIDOMNode> realFocusedNode = GetCurrentFocus();
      nsCOMPtr<nsIContent> realFocusedContent = do_QueryInterface(realFocusedNode);
      nsCOMPtr<nsIContent> targetContent = do_QueryInterface(aTargetNode);
      nsIContent *containerContent = targetContent;
      while (containerContent) {
        nsCOMPtr<nsIDOMXULPopupElement> popup = do_QueryInterface(containerContent);
        if (popup || containerContent == realFocusedContent) { 
          // If we're inside the focus or a popup we can fire focus events
          // for the changed active item
          fireFocus = PR_TRUE;
          break;
        }
        containerContent = containerContent->GetParent();
      }
    }
    if (fireFocus) {
      nsAccEvent::PrepareForEvent(aTargetNode, PR_TRUE);  // Always asynch, always from user input
      FireAccessibleFocusEvent(accessible, aTargetNode, aEvent, PR_TRUE, PR_TRUE);
    }
  }
  else if (eventType.EqualsLiteral("DOMMenuBarActive")) {  // Always asynch, always from user input
    nsAccEvent::PrepareForEvent(aTargetNode, PR_TRUE);
    nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_MENU_START,
                             accessible, PR_TRUE);
  }
  else if (eventType.EqualsLiteral("DOMMenuBarInactive")) {  // Always asynch, always from user input
    nsAccEvent::PrepareForEvent(aTargetNode, PR_TRUE);
    nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_MENU_END,
                             accessible, PR_TRUE);
    FireCurrentFocusEvent();
  }
  else if (eventType.EqualsLiteral("ValueChange")) {
    FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, aTargetNode, nsAccEvent::eRemoveDupes);
  }
#ifdef DEBUG
  else if (eventType.EqualsLiteral("mouseover")) {
    nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_DRAGDROP_START,
                             accessible);
  }
#endif
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
  nsRootAccessible *rootAccessible = static_cast<nsRootAccessible*>(aClosure);
  NS_ASSERTION(rootAccessible, "How did we get here without a root accessible?");
  rootAccessible->FireCurrentFocusEvent();
}

nsresult
nsRootAccessible::Init()
{
  nsresult rv = nsDocAccessibleWrap::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsApplicationAccessibleWrap> root = GetApplicationAccessible();
  NS_ENSURE_STATE(root);

  root->AddRootAccessible(this);
  return NS_OK;
}

nsresult
nsRootAccessible::Shutdown()
{
  // Called manually or by nsAccessNode::LastRelease()
  if (!mWeakShell) {
    return NS_OK;  // Already shutdown
  }

  nsRefPtr<nsApplicationAccessibleWrap> root = GetApplicationAccessible();
  NS_ENSURE_STATE(root);

  root->RemoveRootAccessible(this);

  mCurrentARIAMenubar = nsnull;

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
      if (nsAccUtils::State(accessible) & nsIAccessibleStates::STATE_INVISIBLE)
        return nsnull;

      nsCOMPtr<nsIAccessible> ancestor;
      accessible->GetParent(getter_AddRefs(ancestor));
      if (ancestor == this) {
        break; // Don't check past original root accessible we started with
      }
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

  if (!mDOMNode || aRelationType != nsIAccessibleRelation::RELATION_EMBEDS) {
    return nsDocAccessibleWrap::GetAccessibleRelated(aRelationType, aRelated);
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
  nsCOMPtr<nsIDocShellTreeItem> contentTreeItem = GetContentDocShell(treeItem);
  // there may be no content area, so we need a null check
  if (contentTreeItem) {
    nsCOMPtr<nsIAccessibleDocument> accDoc =
      GetDocAccessibleFor(contentTreeItem, PR_TRUE);

    if (accDoc)
      CallQueryInterface(accDoc, aRelated);
  }
  return NS_OK;
}

NS_IMETHODIMP nsRootAccessible::FireDocLoadEvents(PRUint32 aEventType)
{
  if (!mDocument || !mWeakShell) {
    return NS_OK;  // Document has been shut down
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
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

#ifdef MOZ_XUL
nsresult
nsRootAccessible::HandleTreeRowCountChangedEvent(nsIDOMEvent *aEvent,
                                                 nsIAccessibleTreeCache *aAccessible)
{
  nsCOMPtr<nsIDOMDataContainerEvent> dataEvent(do_QueryInterface(aEvent));
  if (!dataEvent)
    return NS_OK;

  nsCOMPtr<nsIVariant> indexVariant;
  dataEvent->GetData(NS_LITERAL_STRING("index"),
                     getter_AddRefs(indexVariant));
  if (!indexVariant)
    return NS_OK;

  nsCOMPtr<nsIVariant> countVariant;
  dataEvent->GetData(NS_LITERAL_STRING("count"),
                     getter_AddRefs(countVariant));
  if (!countVariant)
    return NS_OK;

  PRInt32 index, count;
  indexVariant->GetAsInt32(&index);
  countVariant->GetAsInt32(&count);

  return aAccessible->InvalidateCache(index, count);
}

nsresult
nsRootAccessible::HandleTreeInvalidatedEvent(nsIDOMEvent *aEvent,
                                             nsIAccessibleTreeCache *aAccessible)
{
  nsCOMPtr<nsIDOMDataContainerEvent> dataEvent(do_QueryInterface(aEvent));
  if (!dataEvent)
    return NS_OK;

  PRInt32 startRow = 0, endRow = -1, startCol = 0, endCol = -1;

  nsCOMPtr<nsIVariant> startRowVariant;
  dataEvent->GetData(NS_LITERAL_STRING("startrow"),
                     getter_AddRefs(startRowVariant));
  if (startRowVariant)
    startRowVariant->GetAsInt32(&startRow);

  nsCOMPtr<nsIVariant> endRowVariant;
  dataEvent->GetData(NS_LITERAL_STRING("endrow"),
                     getter_AddRefs(endRowVariant));
  if (endRowVariant)
    endRowVariant->GetAsInt32(&endRow);

  nsCOMPtr<nsIVariant> startColVariant;
  dataEvent->GetData(NS_LITERAL_STRING("startcolumn"),
                     getter_AddRefs(startColVariant));
  if (startColVariant)
    startColVariant->GetAsInt32(&startCol);

  nsCOMPtr<nsIVariant> endColVariant;
  dataEvent->GetData(NS_LITERAL_STRING("endcolumn"),
                     getter_AddRefs(endColVariant));
  if (endColVariant)
    endColVariant->GetAsInt32(&endCol);

  return aAccessible->TreeViewInvalidated(startRow, endRow, startCol, endCol);
}
#endif


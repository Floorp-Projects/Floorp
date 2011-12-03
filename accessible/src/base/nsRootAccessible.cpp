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

#include "mozilla/Util.h"

#define CreateEvent CreateEventA
#include "nsIDOMDocument.h"

#include "nsAccessibilityService.h"
#include "nsApplicationAccessibleWrap.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "Relation.h"
#include "States.h"

#include "mozilla/dom/Element.h"
#include "nsHTMLSelectAccessible.h"
#include "nsIAccessibleRelation.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMDataContainerEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIDocument.h"
#include "nsEventListenerManager.h"
#include "nsIFrame.h"
#include "nsIHTMLDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISelectionPrivate.h"
#include "nsIServiceManager.h"
#include "nsPIDOMWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsReadableUtils.h"
#include "nsRootAccessible.h"
#include "nsIPrivateDOMEvent.h"
#include "nsFocusManager.h"

#ifdef MOZ_XUL
#include "nsXULTreeAccessible.h"
#include "nsIXULDocument.h"
#include "nsIXULWindow.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

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

////////////////////////////////////////////////////////////////////////////////
// Constructor/desctructor

nsRootAccessible::
  nsRootAccessible(nsIDocument *aDocument, nsIContent *aRootContent,
                   nsIWeakReference *aShell) :
  nsDocAccessibleWrap(aDocument, aRootContent, aShell)
{
  mFlags |= eRootAccessible;
}

nsRootAccessible::~nsRootAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

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

  nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(mDocument);
  return document->GetTitle(aName);
}

PRUint32
nsRootAccessible::NativeRole()
{
  // If it's a <dialog> or <wizard>, use nsIAccessibleRole::ROLE_DIALOG instead
  dom::Element *root = mDocument->GetRootElement();
  if (root) {
    nsCOMPtr<nsIDOMElement> rootElement(do_QueryInterface(root));
    if (rootElement) {
      nsAutoString name;
      rootElement->GetLocalName(name);
      if (name.EqualsLiteral("dialog") || name.EqualsLiteral("wizard")) {
        return nsIAccessibleRole::ROLE_DIALOG; // Always at the root
      }
    }
  }

  return nsDocAccessibleWrap::NativeRole();
}

// nsRootAccessible protected member
#ifdef MOZ_XUL
PRUint32 nsRootAccessible::GetChromeFlags()
{
  // Return the flag set for the top level window as defined 
  // by nsIWebBrowserChrome::CHROME_WINDOW_[FLAGNAME]
  // Not simple: nsIXULWindow is not just a QI from nsIDOMWindow
  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDocument);
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

PRUint64
nsRootAccessible::NativeState()
{
  PRUint64 states = nsDocAccessibleWrap::NativeState();

#ifdef MOZ_XUL
  PRUint32 chromeFlags = GetChromeFlags();
  if (chromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE)
    states |= states::SIZEABLE;
    // If it has a titlebar it's movable
    // XXX unless it's minimized or maximized, but not sure
    //     how to detect that
  if (chromeFlags & nsIWebBrowserChrome::CHROME_TITLEBAR)
    states |= states::MOVEABLE;
  if (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL)
    states |= states::MODAL;
#endif

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    nsCOMPtr<nsIDOMWindow> rootWindow;
    GetWindow(getter_AddRefs(rootWindow));

    nsCOMPtr<nsIDOMWindow> activeWindow;
    fm->GetActiveWindow(getter_AddRefs(activeWindow));
    if (activeWindow == rootWindow)
      states |= states::ACTIVE;
  }

  return states;
}

const char* const docEvents[] = {
#ifdef DEBUG_DRAGDROPSTART
  // Capture mouse over events and fire fake DRAGDROPSTART event to simplify
  // debugging a11y objects with event viewers
  "mouseover",
#endif
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
  "DOMMenuItemInactive",
  "DOMMenuBarActive",
  "DOMMenuBarInactive"
};

nsresult nsRootAccessible::AddEventListeners()
{
  // nsIDOMEventTarget interface allows to register event listeners to
  // receive untrusted events (synthetic events generated by untrusted code).
  // For example, XBL bindings implementations for elements that are hosted in
  // non chrome document fire untrusted events.
  nsCOMPtr<nsIDOMEventTarget> nstarget(do_QueryInterface(mDocument));

  if (nstarget) {
    for (const char* const* e = docEvents,
                   * const* e_end = ArrayEnd(docEvents);
         e < e_end; ++e) {
      nsresult rv = nstarget->AddEventListener(NS_ConvertASCIItoUTF16(*e),
                                               this, true, true, 2);
      NS_ENSURE_SUCCESS(rv, rv);
    }
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
                   * const* e_end = ArrayEnd(docEvents);
         e < e_end; ++e) {
      nsresult rv = target->RemoveEventListener(NS_ConvertASCIItoUTF16(*e), this, true);
      NS_ENSURE_SUCCESS(rv, rv);
    }
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

////////////////////////////////////////////////////////////////////////////////
// public

nsCaretAccessible*
nsRootAccessible::GetCaretAccessible()
{
  return mCaretAccessible;
}

void
nsRootAccessible::DocumentActivated(nsDocAccessible* aDocument)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsIDOMEventListener

NS_IMETHODIMP
nsRootAccessible::HandleEvent(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMNSEvent> DOMNSEvent(do_QueryInterface(aDOMEvent));
  nsCOMPtr<nsIDOMEventTarget> DOMEventTarget;
  DOMNSEvent->GetOriginalTarget(getter_AddRefs(DOMEventTarget));
  nsCOMPtr<nsINode> origTargetNode(do_QueryInterface(DOMEventTarget));
  if (!origTargetNode)
    return NS_OK;

  nsDocAccessible* document =
    GetAccService()->GetDocAccessible(origTargetNode->OwnerDoc());

  if (document) {
#ifdef DEBUG_NOTIFICATIONS
    if (origTargetNode->IsElement()) {
      nsIContent* elm = origTargetNode->AsElement();

      nsAutoString tag;
      elm->Tag()->ToString(tag);

      nsIAtom* atomid = elm->GetID();
      nsCAutoString id;
      if (atomid)
        atomid->ToUTF8String(id);

      nsAutoString eventType;
      aDOMEvent->GetType(eventType);

      printf("\nPend DOM event processing for %s@id='%s', type: %s\n\n",
             NS_ConvertUTF16toUTF8(tag).get(), id.get(),
             NS_ConvertUTF16toUTF8(eventType).get());
    }
#endif

    // Root accessible exists longer than any of its descendant documents so
    // that we are guaranteed notification is processed before root accessible
    // is destroyed.
    document->HandleNotification<nsRootAccessible, nsIDOMEvent>
      (this, &nsRootAccessible::ProcessDOMEvent, aDOMEvent);
  }

  return NS_OK;
}

// nsRootAccessible protected
void
nsRootAccessible::ProcessDOMEvent(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMNSEvent> DOMNSEvent(do_QueryInterface(aDOMEvent));
  nsCOMPtr<nsIDOMEventTarget> DOMEventTarget;
  DOMNSEvent->GetOriginalTarget(getter_AddRefs(DOMEventTarget));
  nsCOMPtr<nsINode> origTargetNode(do_QueryInterface(DOMEventTarget));

  nsAutoString eventType;
  aDOMEvent->GetType(eventType);

  nsCOMPtr<nsIWeakReference> weakShell =
    nsCoreUtils::GetWeakShellFor(origTargetNode);
  if (!weakShell)
    return;

  if (eventType.EqualsLiteral("popuphiding")) {
    HandlePopupHidingEvent(origTargetNode);
    return;
  }

  nsAccessible* accessible =
    GetAccService()->GetAccessibleOrContainer(origTargetNode, weakShell);
  if (!accessible)
    return;

  nsDocAccessible* targetDocument = accessible->GetDocAccessible();
  NS_ASSERTION(targetDocument, "No document while accessible is in document?!");

  nsINode* targetNode = accessible->GetNode();

#ifdef MOZ_XUL
  nsRefPtr<nsXULTreeAccessible> treeAcc;
  if (targetNode->IsElement() &&
      targetNode->AsElement()->NodeInfo()->Equals(nsGkAtoms::tree,
                                                  kNameSpaceID_XUL)) {
    treeAcc = do_QueryObject(accessible);
    if (treeAcc) {
      if (eventType.EqualsLiteral("TreeViewChanged")) {
        treeAcc->TreeViewChanged();
        return;
      }

      if (eventType.EqualsLiteral("TreeRowCountChanged")) {
        HandleTreeRowCountChangedEvent(aDOMEvent, treeAcc);
        return;
      }

      if (eventType.EqualsLiteral("TreeInvalidated")) {
        HandleTreeInvalidatedEvent(aDOMEvent, treeAcc);
        return;
      }
    }
  }
#endif

  if (eventType.EqualsLiteral("RadioStateChange")) {
    PRUint64 state = accessible->State();

    // radiogroup in prefWindow is exposed as a list,
    // and panebutton is exposed as XULListitem in A11y.
    // nsXULListitemAccessible::GetStateInternal uses STATE_SELECTED in this case,
    // so we need to check states::SELECTED also.
    bool isEnabled = (state & (states::CHECKED | states::SELECTED)) != 0;

    nsRefPtr<AccEvent> accEvent =
      new AccStateChangeEvent(accessible, states::CHECKED, isEnabled);
    nsEventShell::FireEvent(accEvent);

    if (isEnabled) {
      FocusMgr()->ActiveItemChanged(accessible);
      A11YDEBUG_FOCUS_ACTIVEITEMCHANGE_CAUSE("RadioStateChange", accessible)
    }

    return;
  }

  if (eventType.EqualsLiteral("CheckboxStateChange")) {
    PRUint64 state = accessible->State();

    bool isEnabled = !!(state & states::CHECKED);

    nsRefPtr<AccEvent> accEvent =
      new AccStateChangeEvent(accessible, states::CHECKED, isEnabled);

    nsEventShell::FireEvent(accEvent);
    return;
  }

  nsAccessible* treeItemAcc = nsnull;
#ifdef MOZ_XUL
  // If it's a tree element, need the currently selected item.
  if (treeAcc) {
    treeItemAcc = accessible->CurrentItem();
    if (treeItemAcc)
      accessible = treeItemAcc;
  }

  if (treeItemAcc && eventType.EqualsLiteral("OpenStateChange")) {
    PRUint64 state = accessible->State();
    bool isEnabled = (state & states::EXPANDED) != 0;

    nsRefPtr<AccEvent> accEvent =
      new AccStateChangeEvent(accessible, states::EXPANDED, isEnabled);
    nsEventShell::FireEvent(accEvent);
    return;
  }

  if (treeItemAcc && eventType.EqualsLiteral("select")) {
    // XXX: We shouldn't be based on DOM select event which doesn't provide us
    // any context info. We should integrate into nsTreeSelection instead.
    // If multiselect tree, we should fire selectionadd or selection removed
    if (FocusMgr()->HasDOMFocus(targetNode)) {
      nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSel =
        do_QueryInterface(targetNode);
      nsAutoString selType;
      multiSel->GetSelType(selType);
      if (selType.IsEmpty() || !selType.EqualsLiteral("single")) {
        // XXX: We need to fire EVENT_SELECTION_ADD and EVENT_SELECTION_REMOVE
        // for each tree item. Perhaps each tree item will need to cache its
        // selection state and fire an event after a DOM "select" event when
        // that state changes. nsXULTreeAccessible::UpdateTreeSelection();
        nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN,
                                accessible);
        return;
      }

      nsRefPtr<AccSelChangeEvent> selChangeEvent =
        new AccSelChangeEvent(treeAcc, treeItemAcc,
                              AccSelChangeEvent::eSelectionAdd);
      nsEventShell::FireEvent(selChangeEvent);
      return;
    }
  }
  else
#endif
  if (eventType.EqualsLiteral("AlertActive")) {
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_ALERT, accessible);
  }
  else if (eventType.EqualsLiteral("popupshown")) {
    HandlePopupShownEvent(accessible);
  }
  else if (eventType.EqualsLiteral("DOMMenuInactive")) {
    if (accessible->Role() == nsIAccessibleRole::ROLE_MENUPOPUP) {
      nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_END,
                              accessible);
    }
  }
  else if (eventType.EqualsLiteral("DOMMenuItemActive")) {
    FocusMgr()->ActiveItemChanged(accessible);
    A11YDEBUG_FOCUS_ACTIVEITEMCHANGE_CAUSE("DOMMenuItemActive", accessible)
  }
  else if (eventType.EqualsLiteral("DOMMenuItemInactive")) {
    // Process DOMMenuItemInactive event for autocomplete only because this is
    // unique widget that may acquire focus from autocomplete popup while popup
    // stays open and has no active item. In case of XUL tree autocomplete
    // popup this event is fired for tree accessible.
    nsAccessible* widget =
      accessible->IsWidget() ? accessible : accessible->ContainerWidget();
    if (widget && widget->IsAutoCompletePopup()) {
      FocusMgr()->ActiveItemChanged(nsnull);
      A11YDEBUG_FOCUS_ACTIVEITEMCHANGE_CAUSE("DOMMenuItemInactive", accessible)
    }
  }
  else if (eventType.EqualsLiteral("DOMMenuBarActive")) {  // Always from user input
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENU_START,
                            accessible, eFromUserInput);

    // Notify of active item change when menubar gets active and if it has
    // current item. This is a case of mouseover (set current menuitem) and
    // mouse click (activate the menubar). If menubar doesn't have current item
    // (can be a case of menubar activation from keyboard) then ignore this
    // notification because later we'll receive DOMMenuItemActive event after
    // current menuitem is set.
    nsAccessible* activeItem = accessible->CurrentItem();
    if (activeItem) {
      FocusMgr()->ActiveItemChanged(activeItem);
      A11YDEBUG_FOCUS_ACTIVEITEMCHANGE_CAUSE("DOMMenuBarActive", accessible)
    }
  }
  else if (eventType.EqualsLiteral("DOMMenuBarInactive")) {  // Always from user input
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENU_END,
                            accessible, eFromUserInput);

    FocusMgr()->ActiveItemChanged(nsnull);
    A11YDEBUG_FOCUS_ACTIVEITEMCHANGE_CAUSE("DOMMenuBarInactive", accessible)
  }
  else if (eventType.EqualsLiteral("ValueChange")) {
    targetDocument->
      FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE,
                                 targetNode, AccEvent::eRemoveDupes);
  }
#ifdef DEBUG_DRAGDROPSTART
  else if (eventType.EqualsLiteral("mouseover")) {
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_DRAGDROP_START,
                            accessible);
  }
#endif
}


////////////////////////////////////////////////////////////////////////////////
// nsAccessNode

void
nsRootAccessible::Shutdown()
{
  // Called manually or by nsAccessNode::LastRelease()
  if (!mWeakShell)
    return;  // Already shutdown

  nsDocAccessibleWrap::Shutdown();
}

// nsRootAccessible protected member
already_AddRefed<nsIDocShellTreeItem>
nsRootAccessible::GetContentDocShell(nsIDocShellTreeItem *aStart)
{
  if (!aStart) {
    return nsnull;
  }

  PRInt32 itemType;
  aStart->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeContent) {
    nsDocAccessible *accDoc = nsAccUtils::GetDocAccessibleFor(aStart);

    // Hidden documents don't have accessibles (like SeaMonkey's sidebar),
    // they are of no interest for a11y.
    if (!accDoc)
      return nsnull;

    // If ancestor chain of accessibles is not completely visible,
    // don't use this one. This happens for example if it's inside
    // a background tab (tabbed browsing)
    nsAccessible* parent = accDoc->Parent();
    while (parent) {
      if (parent->State() & states::INVISIBLE)
        return nsnull;

      if (parent == this)
        break; // Don't check past original root accessible we started with

      parent = parent->Parent();
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

// nsIAccessible method
Relation
nsRootAccessible::RelationByType(PRUint32 aType)
{
  if (!mDocument || aType != nsIAccessibleRelation::RELATION_EMBEDS)
    return nsDocAccessibleWrap::RelationByType(aType);

  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDocument);
  nsCOMPtr<nsIDocShellTreeItem> contentTreeItem = GetContentDocShell(treeItem);
  // there may be no content area, so we need a null check
  if (!contentTreeItem)
    return Relation();

  return Relation(nsAccUtils::GetDocAccessibleFor(contentTreeItem));
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

void
nsRootAccessible::HandlePopupShownEvent(nsAccessible* aAccessible)
{
  PRUint32 role = aAccessible->Role();

  if (role == nsIAccessibleRole::ROLE_MENUPOPUP) {
    // Don't fire menupopup events for combobox and autocomplete lists.
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START,
                            aAccessible);
    return;
  }

  if (role == nsIAccessibleRole::ROLE_TOOLTIP) {
    // There is a single <xul:tooltip> node which Mozilla moves around.
    // The accessible for it stays the same no matter where it moves. 
    // AT's expect to get an EVENT_SHOW for the tooltip. 
    // In event callback the tooltip's accessible will be ready.
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_SHOW, aAccessible);
    return;
  }

  if (role == nsIAccessibleRole::ROLE_COMBOBOX_LIST) {
    // Fire expanded state change event for comboboxes and autocompeletes.
    nsAccessible* combobox = aAccessible->Parent();
    if (!combobox)
      return;

    PRUint32 comboboxRole = combobox->Role();
    if (comboboxRole == nsIAccessibleRole::ROLE_COMBOBOX ||
        comboboxRole == nsIAccessibleRole::ROLE_AUTOCOMPLETE) {
      nsRefPtr<AccEvent> event =
        new AccStateChangeEvent(combobox, states::EXPANDED, true);
      if (event)
        nsEventShell::FireEvent(event);
    }
  }
}

void
nsRootAccessible::HandlePopupHidingEvent(nsINode* aPopupNode)
{
  // Get popup accessible. There are cases when popup element isn't accessible
  // but an underlying widget is and behaves like popup, an example is
  // autocomplete popups.
  nsDocAccessible* document = nsAccUtils::GetDocAccessibleFor(aPopupNode);
  if (!document)
    return;

  nsAccessible* popup = document->GetAccessible(aPopupNode);
  if (!popup) {
    nsAccessible* popupContainer = document->GetContainerAccessible(aPopupNode);
    if (!popupContainer)
      return;

    PRInt32 childCount = popupContainer->GetChildCount();
    for (PRInt32 idx = 0; idx < childCount; idx++) {
      nsAccessible* child = popupContainer->GetChildAt(idx);
      if (child->IsAutoCompletePopup()) {
        popup = child;
        break;
      }
    }

    // No popup no events. Focus is managed by DOM. This is a case for
    // menupopups of menus on Linux since there are no accessible for popups.
    if (!popup)
      return;
  }

  // In case of autocompletes and comboboxes fire state change event for
  // expanded state. Note, HTML form autocomplete isn't a subject of state
  // change event because they aren't autocompletes strictly speaking.
  // When popup closes (except nested popups and menus) then fire focus event to
  // where it was. The focus event is expected even if popup didn't take a focus.

  static const PRUint32 kNotifyOfFocus = 1;
  static const PRUint32 kNotifyOfState = 2;
  PRUint32 notifyOf = 0;

  // HTML select is target of popuphidding event. Otherwise get container
  // widget. No container widget means this is either tooltip or menupopup.
  // No events in the former case.
  nsAccessible* widget = nsnull;
  if (popup->IsCombobox()) {
    widget = popup;
  } else {
    widget = popup->ContainerWidget();
    if (!widget) {
      if (!popup->IsMenuPopup())
        return;

      widget = popup;
    }
  }

  if (popup->IsAutoCompletePopup()) {
    // No focus event for autocomplete because it's managed by
    // DOMMenuItemInactive events.
    if (widget->IsAutoComplete())
      notifyOf = kNotifyOfState;

  } else if (widget->IsCombobox()) {
    // Fire focus for active combobox, otherwise the focus is managed by DOM
    // focus notifications. Always fire state change event.
    if (widget->IsActiveWidget())
      notifyOf = kNotifyOfFocus;
    notifyOf |= kNotifyOfState;

  } else if (widget->IsMenuButton()) {
    // Can be a part of autocomplete.
    nsAccessible* compositeWidget = widget->ContainerWidget();
    if (compositeWidget && compositeWidget->IsAutoComplete()) {
      widget = compositeWidget;
      notifyOf = kNotifyOfState;
    }

    // Autocomplete (like searchbar) can be inactive when popup hiddens
    notifyOf |= kNotifyOfFocus;

  } else if (widget == popup) {
    // Top level context menus and alerts.
    // Ignore submenus and menubar. When submenu is closed then sumbenu
    // container menuitem takes a focus via DOMMenuItemActive notification.
    // For menubars processing we listen DOMMenubarActive/Inactive
    // notifications.
    notifyOf = kNotifyOfFocus;
  }

  // Restore focus to where it was.
  if (notifyOf & kNotifyOfFocus) {
    FocusMgr()->ActiveItemChanged(nsnull);
    A11YDEBUG_FOCUS_ACTIVEITEMCHANGE_CAUSE("popuphiding", popup)
  }

  // Fire expanded state change event.
  if (notifyOf & kNotifyOfState) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(widget, states::EXPANDED, false);
    document->FireDelayedAccessibleEvent(event);
  }
}

#ifdef MOZ_XUL
void
nsRootAccessible::HandleTreeRowCountChangedEvent(nsIDOMEvent* aEvent,
                                                 nsXULTreeAccessible* aAccessible)
{
  nsCOMPtr<nsIDOMDataContainerEvent> dataEvent(do_QueryInterface(aEvent));
  if (!dataEvent)
    return;

  nsCOMPtr<nsIVariant> indexVariant;
  dataEvent->GetData(NS_LITERAL_STRING("index"),
                     getter_AddRefs(indexVariant));
  if (!indexVariant)
    return;

  nsCOMPtr<nsIVariant> countVariant;
  dataEvent->GetData(NS_LITERAL_STRING("count"),
                     getter_AddRefs(countVariant));
  if (!countVariant)
    return;

  PRInt32 index, count;
  indexVariant->GetAsInt32(&index);
  countVariant->GetAsInt32(&count);

  aAccessible->InvalidateCache(index, count);
}

void
nsRootAccessible::HandleTreeInvalidatedEvent(nsIDOMEvent* aEvent,
                                             nsXULTreeAccessible* aAccessible)
{
  nsCOMPtr<nsIDOMDataContainerEvent> dataEvent(do_QueryInterface(aEvent));
  if (!dataEvent)
    return;

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

  aAccessible->TreeViewInvalidated(startRow, endRow, startCol, endCol);
}
#endif

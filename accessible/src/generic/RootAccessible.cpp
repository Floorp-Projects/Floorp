/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessible.h"

#include "mozilla/Util.h"

#define CreateEvent CreateEventA
#include "nsIDOMDocument.h"

#include "Accessible-inl.h"
#include "DocAccessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#ifdef MOZ_XUL
#include "XULTreeAccessible.h"
#endif

#include "mozilla/dom/Element.h"

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
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsPIDOMWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsReadableUtils.h"
#include "nsFocusManager.h"

#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#include "nsIXULWindow.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(RootAccessible, DocAccessible, nsIAccessibleDocument)

////////////////////////////////////////////////////////////////////////////////
// Constructor/destructor

RootAccessible::
  RootAccessible(nsIDocument* aDocument, nsIContent* aRootContent,
                 nsIPresShell* aPresShell) :
  DocAccessibleWrap(aDocument, aRootContent, aPresShell)
{
  mFlags |= eRootAccessible;
}

RootAccessible::~RootAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

ENameValueFlag
RootAccessible::Name(nsString& aName)
{
  aName.Truncate();

  if (mRoleMapEntry) {
    Accessible::Name(aName);
    if (!aName.IsEmpty())
      return eNameOK;
  }

  nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(mDocument);
  NS_ENSURE_TRUE(document, eNameOK);
  document->GetTitle(aName);
  return eNameOK;
}

role
RootAccessible::NativeRole()
{
  // If it's a <dialog> or <wizard>, use roles::DIALOG instead
  dom::Element* rootElm = mDocument->GetRootElement();
  if (rootElm && (rootElm->Tag() == nsGkAtoms::dialog ||
                  rootElm->Tag() == nsGkAtoms::wizard))
    return roles::DIALOG;

  return DocAccessibleWrap::NativeRole();
}

// RootAccessible protected member
#ifdef MOZ_XUL
PRUint32
RootAccessible::GetChromeFlags()
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
RootAccessible::NativeState()
{
  PRUint64 state = DocAccessibleWrap::NativeState();
  if (state & states::DEFUNCT)
    return state;

#ifdef MOZ_XUL
  PRUint32 chromeFlags = GetChromeFlags();
  if (chromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE)
    state |= states::SIZEABLE;
    // If it has a titlebar it's movable
    // XXX unless it's minimized or maximized, but not sure
    //     how to detect that
  if (chromeFlags & nsIWebBrowserChrome::CHROME_TITLEBAR)
    state |= states::MOVEABLE;
  if (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL)
    state |= states::MODAL;
#endif

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm && fm->GetActiveWindow() == mDocument->GetWindow())
    state |= states::ACTIVE;

  return state;
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

nsresult
RootAccessible::AddEventListeners()
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

  return DocAccessible::AddEventListeners();
}

nsresult
RootAccessible::RemoveEventListeners()
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
  DocAccessible::RemoveEventListeners();

  if (mCaretAccessible) {
    mCaretAccessible->Shutdown();
    mCaretAccessible = nsnull;
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// public

nsCaretAccessible*
RootAccessible::GetCaretAccessible()
{
  return mCaretAccessible;
}

void
RootAccessible::DocumentActivated(DocAccessible* aDocument)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsIDOMEventListener

NS_IMETHODIMP
RootAccessible::HandleEvent(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMNSEvent> DOMNSEvent(do_QueryInterface(aDOMEvent));
  nsCOMPtr<nsIDOMEventTarget> DOMEventTarget;
  DOMNSEvent->GetOriginalTarget(getter_AddRefs(DOMEventTarget));
  nsCOMPtr<nsINode> origTargetNode(do_QueryInterface(DOMEventTarget));
  if (!origTargetNode)
    return NS_OK;

  DocAccessible* document =
    GetAccService()->GetDocAccessible(origTargetNode->OwnerDoc());

  if (document) {
#ifdef DEBUG
    if (logging::IsEnabled(logging::eDOMEvents)) {
      nsAutoString eventType;
      aDOMEvent->GetType(eventType);

      logging::MsgBegin("DOMEvents", "event '%s' handled",
                        NS_ConvertUTF16toUTF8(eventType).get());
      logging::Node("target", origTargetNode);
      logging::MsgEnd();
    }
#endif

    // Root accessible exists longer than any of its descendant documents so
    // that we are guaranteed notification is processed before root accessible
    // is destroyed.
    document->HandleNotification<RootAccessible, nsIDOMEvent>
      (this, &RootAccessible::ProcessDOMEvent, aDOMEvent);
  }

  return NS_OK;
}

// RootAccessible protected
void
RootAccessible::ProcessDOMEvent(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMNSEvent> DOMNSEvent(do_QueryInterface(aDOMEvent));
  nsCOMPtr<nsIDOMEventTarget> DOMEventTarget;
  DOMNSEvent->GetOriginalTarget(getter_AddRefs(DOMEventTarget));
  nsCOMPtr<nsINode> origTargetNode(do_QueryInterface(DOMEventTarget));

  nsAutoString eventType;
  aDOMEvent->GetType(eventType);

  if (eventType.EqualsLiteral("popuphiding")) {
    HandlePopupHidingEvent(origTargetNode);
    return;
  }

  DocAccessible* targetDocument = GetAccService()->
    GetDocAccessible(origTargetNode->OwnerDoc());
  NS_ASSERTION(targetDocument, "No document while accessible is in document?!");

  Accessible* accessible = 
    targetDocument->GetAccessibleOrContainer(origTargetNode);
  if (!accessible)
    return;

  nsINode* targetNode = accessible->GetNode();

#ifdef MOZ_XUL
  XULTreeAccessible* treeAcc = accessible->AsXULTree();
  if (treeAcc) {
    if (eventType.EqualsLiteral("TreeRowCountChanged")) {
      HandleTreeRowCountChangedEvent(aDOMEvent, treeAcc);
      return;
    }

    if (eventType.EqualsLiteral("TreeInvalidated")) {
      HandleTreeInvalidatedEvent(aDOMEvent, treeAcc);
      return;
    }
  }
#endif

  if (eventType.EqualsLiteral("RadioStateChange")) {
    PRUint64 state = accessible->State();

    // radiogroup in prefWindow is exposed as a list,
    // and panebutton is exposed as XULListitem in A11y.
    // XULListitemAccessible::GetStateInternal uses STATE_SELECTED in this case,
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

  Accessible* treeItemAcc = nsnull;
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
        // that state changes. XULTreeAccessible::UpdateTreeSelection();
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
    if (accessible->Role() == roles::MENUPOPUP) {
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
    Accessible* widget =
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
    Accessible* activeItem = accessible->CurrentItem();
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
RootAccessible::Shutdown()
{
  // Called manually or by nsAccessNode::LastRelease()
  if (!PresShell())
    return;  // Already shutdown

  DocAccessibleWrap::Shutdown();
}

// nsIAccessible method
Relation
RootAccessible::RelationByType(PRUint32 aType)
{
  if (!mDocument || aType != nsIAccessibleRelation::RELATION_EMBEDS)
    return DocAccessibleWrap::RelationByType(aType);

  nsIDOMWindow* rootWindow = mDocument->GetWindow();
  if (rootWindow) {
    nsCOMPtr<nsIDOMWindow> contentWindow;
    rootWindow->GetContent(getter_AddRefs(contentWindow));
    if (contentWindow) {
      nsCOMPtr<nsIDOMDocument> contentDOMDocument;
      contentWindow->GetDocument(getter_AddRefs(contentDOMDocument));
      nsCOMPtr<nsIDocument> contentDocumentNode =
        do_QueryInterface(contentDOMDocument);
      if (contentDocumentNode) {
        DocAccessible* contentDocument =
          GetAccService()->GetDocAccessible(contentDocumentNode);
        if (contentDocument)
          return Relation(contentDocument);
      }
    }
  }

  return Relation();
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

void
RootAccessible::HandlePopupShownEvent(Accessible* aAccessible)
{
  roles::Role role = aAccessible->Role();

  if (role == roles::MENUPOPUP) {
    // Don't fire menupopup events for combobox and autocomplete lists.
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START,
                            aAccessible);
    return;
  }

  if (role == roles::TOOLTIP) {
    // There is a single <xul:tooltip> node which Mozilla moves around.
    // The accessible for it stays the same no matter where it moves. 
    // AT's expect to get an EVENT_SHOW for the tooltip. 
    // In event callback the tooltip's accessible will be ready.
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_SHOW, aAccessible);
    return;
  }

  if (role == roles::COMBOBOX_LIST) {
    // Fire expanded state change event for comboboxes and autocompeletes.
    Accessible* combobox = aAccessible->Parent();
    if (!combobox)
      return;

    roles::Role comboboxRole = combobox->Role();
    if (comboboxRole == roles::COMBOBOX || 
	comboboxRole == roles::AUTOCOMPLETE) {
      nsRefPtr<AccEvent> event =
        new AccStateChangeEvent(combobox, states::EXPANDED, true);
      if (event)
        nsEventShell::FireEvent(event);
    }
  }
}

void
RootAccessible::HandlePopupHidingEvent(nsINode* aPopupNode)
{
  // Get popup accessible. There are cases when popup element isn't accessible
  // but an underlying widget is and behaves like popup, an example is
  // autocomplete popups.
  DocAccessible* document = nsAccUtils::GetDocAccessibleFor(aPopupNode);
  if (!document)
    return;

  Accessible* popup = document->GetAccessible(aPopupNode);
  if (!popup) {
    Accessible* popupContainer = document->GetContainerAccessible(aPopupNode);
    if (!popupContainer)
      return;

    PRUint32 childCount = popupContainer->ChildCount();
    for (PRUint32 idx = 0; idx < childCount; idx++) {
      Accessible* child = popupContainer->GetChildAt(idx);
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
  Accessible* widget = nsnull;
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
    Accessible* compositeWidget = widget->ContainerWidget();
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
RootAccessible::HandleTreeRowCountChangedEvent(nsIDOMEvent* aEvent,
                                               XULTreeAccessible* aAccessible)
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
RootAccessible::HandleTreeInvalidatedEvent(nsIDOMEvent* aEvent,
                                           XULTreeAccessible* aAccessible)
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

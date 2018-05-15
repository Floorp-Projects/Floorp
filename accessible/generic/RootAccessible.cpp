/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessible.h"

#include "mozilla/ArrayUtils.h"

#define CreateEvent CreateEventA

#include "Accessible-inl.h"
#include "DocAccessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsEventShell.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#ifdef MOZ_XUL
#include "XULTreeAccessible.h"
#endif

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"

#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPropertyBag2.h"
#include "nsIServiceManager.h"
#include "nsPIDOMWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsReadableUtils.h"
#include "nsFocusManager.h"
#include "nsGlobalWindow.h"

#ifdef MOZ_XUL
#include "nsIXULWindow.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED(RootAccessible, DocAccessible, nsIDOMEventListener)

////////////////////////////////////////////////////////////////////////////////
// Constructor/destructor

RootAccessible::
  RootAccessible(nsIDocument* aDocument, nsIPresShell* aPresShell) :
  DocAccessibleWrap(aDocument, aPresShell)
{
  mType = eRootType;
}

RootAccessible::~RootAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

ENameValueFlag
RootAccessible::Name(nsString& aName) const
{
  aName.Truncate();

  if (ARIARoleMap()) {
    Accessible::Name(aName);
    if (!aName.IsEmpty())
      return eNameOK;
  }

  mDocumentNode->GetTitle(aName);
  return eNameOK;
}

role
RootAccessible::NativeRole() const
{
  // If it's a <dialog> or <wizard>, use roles::DIALOG instead
  dom::Element* rootElm = mDocumentNode->GetRootElement();
  if (rootElm && rootElm->IsAnyOfXULElements(nsGkAtoms::dialog,
                                             nsGkAtoms::wizard))
    return roles::DIALOG;

  return DocAccessibleWrap::NativeRole();
}

// RootAccessible protected member
#ifdef MOZ_XUL
uint32_t
RootAccessible::GetChromeFlags() const
{
  // Return the flag set for the top level window as defined
  // by nsIWebBrowserChrome::CHROME_WINDOW_[FLAGNAME]
  // Not simple: nsIXULWindow is not just a QI from nsIDOMWindow
  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(mDocumentNode);
  NS_ENSURE_TRUE(docShell, 0);
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, 0);
  nsCOMPtr<nsIXULWindow> xulWin(do_GetInterface(treeOwner));
  if (!xulWin) {
    return 0;
  }
  uint32_t chromeFlags;
  xulWin->GetChromeFlags(&chromeFlags);
  return chromeFlags;
}
#endif

uint64_t
RootAccessible::NativeState() const
{
  uint64_t state = DocAccessibleWrap::NativeState();
  if (state & states::DEFUNCT)
    return state;

#ifdef MOZ_XUL
  uint32_t chromeFlags = GetChromeFlags();
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
  if (fm && fm->GetActiveWindow() == mDocumentNode->GetWindow())
    state |= states::ACTIVE;

  return state;
}

const char* const kEventTypes[] = {
#ifdef DEBUG_DRAGDROPSTART
  // Capture mouse over events and fire fake DRAGDROPSTART event to simplify
  // debugging a11y objects with event viewers.
  "mouseover",
#endif
  // Fired when list or tree selection changes.
  "select",
  // Fired when value changes immediately, wether or not focused changed.
  "ValueChange",
  "AlertActive",
  "TreeRowCountChanged",
  "TreeInvalidated",
  // add ourself as a OpenStateChange listener (custom event fired in tree.xml)
  "OpenStateChange",
  // add ourself as a CheckboxStateChange listener (custom event fired in HTMLInputElement.cpp)
  "CheckboxStateChange",
  // add ourself as a RadioStateChange Listener ( custom event fired in in HTMLInputElement.cpp  & radio.xml)
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
  // EventTarget interface allows to register event listeners to
  // receive untrusted events (synthetic events generated by untrusted code).
  // For example, XBL bindings implementations for elements that are hosted in
  // non chrome document fire untrusted events.
  nsCOMPtr<EventTarget> nstarget = mDocumentNode;

  if (nstarget) {
    for (const char* const* e = kEventTypes,
                   * const* e_end = ArrayEnd(kEventTypes);
         e < e_end; ++e) {
      nsresult rv = nstarget->AddEventListener(NS_ConvertASCIItoUTF16(*e),
                                               this, true, true);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return DocAccessible::AddEventListeners();
}

nsresult
RootAccessible::RemoveEventListeners()
{
  nsCOMPtr<EventTarget> target = mDocumentNode;
  if (target) {
    for (const char* const* e = kEventTypes,
                   * const* e_end = ArrayEnd(kEventTypes);
         e < e_end; ++e) {
      target->RemoveEventListener(NS_ConvertASCIItoUTF16(*e), this, true);
    }
  }

  // Do this before removing clearing caret accessible, so that it can use
  // shutdown the caret accessible's selection listener
  DocAccessible::RemoveEventListeners();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// public

void
RootAccessible::DocumentActivated(DocAccessible* aDocument)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsIDOMEventListener

NS_IMETHODIMP
RootAccessible::HandleEvent(Event* aDOMEvent)
{
  MOZ_ASSERT(aDOMEvent);
  nsCOMPtr<nsINode> origTargetNode = do_QueryInterface(aDOMEvent->GetOriginalTarget());
  if (!origTargetNode)
    return NS_OK;

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDOMEvents)) {
    nsAutoString eventType;
    aDOMEvent->GetType(eventType);
    logging::DOMEvent("handled", origTargetNode, eventType);
  }
#endif

  DocAccessible* document =
    GetAccService()->GetDocAccessible(origTargetNode->OwnerDoc());

  if (document) {
    // Root accessible exists longer than any of its descendant documents so
    // that we are guaranteed notification is processed before root accessible
    // is destroyed.
    document->HandleNotification<RootAccessible, Event>
      (this, &RootAccessible::ProcessDOMEvent, aDOMEvent);
  }

  return NS_OK;
}

// RootAccessible protected
void
RootAccessible::ProcessDOMEvent(Event* aDOMEvent)
{
  MOZ_ASSERT(aDOMEvent);
  nsCOMPtr<nsINode> origTargetNode =
    do_QueryInterface(aDOMEvent->GetOriginalTarget());

  nsAutoString eventType;
  aDOMEvent->GetType(eventType);

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDOMEvents))
    logging::DOMEvent("processed", origTargetNode, eventType);
#endif

  if (eventType.EqualsLiteral("popuphiding")) {
    HandlePopupHidingEvent(origTargetNode);
    return;
  }

  DocAccessible* targetDocument = GetAccService()->
    GetDocAccessible(origTargetNode->OwnerDoc());
  if (!targetDocument) {
    // Document has ceased to exist.
    return;
  }

  Accessible* accessible =
    targetDocument->GetAccessibleOrContainer(origTargetNode);
  if (!accessible)
    return;

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
    uint64_t state = accessible->State();
    bool isEnabled = (state & (states::CHECKED | states::SELECTED)) != 0;

    if (accessible->NeedsDOMUIEvent()) {
      RefPtr<AccEvent> accEvent =
        new AccStateChangeEvent(accessible, states::CHECKED, isEnabled);
      nsEventShell::FireEvent(accEvent);
    }

    if (isEnabled) {
      FocusMgr()->ActiveItemChanged(accessible);
#ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eFocus))
        logging::ActiveItemChangeCausedBy("RadioStateChange", accessible);
#endif
    }

    return;
  }

  if (eventType.EqualsLiteral("CheckboxStateChange")) {
    if (accessible->NeedsDOMUIEvent()) {
      uint64_t state = accessible->State();
      bool isEnabled = !!(state & states::CHECKED);

      RefPtr<AccEvent> accEvent =
        new AccStateChangeEvent(accessible, states::CHECKED, isEnabled);
      nsEventShell::FireEvent(accEvent);
    }
    return;
  }

  Accessible* treeItemAcc = nullptr;
#ifdef MOZ_XUL
  // If it's a tree element, need the currently selected item.
  if (treeAcc) {
    treeItemAcc = accessible->CurrentItem();
    if (treeItemAcc)
      accessible = treeItemAcc;
  }

  if (treeItemAcc && eventType.EqualsLiteral("OpenStateChange")) {
    uint64_t state = accessible->State();
    bool isEnabled = (state & states::EXPANDED) != 0;

    RefPtr<AccEvent> accEvent =
      new AccStateChangeEvent(accessible, states::EXPANDED, isEnabled);
    nsEventShell::FireEvent(accEvent);
    return;
  }

  nsINode* targetNode = accessible->GetNode();
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

      RefPtr<AccSelChangeEvent> selChangeEvent =
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
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eFocus))
      logging::ActiveItemChangeCausedBy("DOMMenuItemActive", accessible);
#endif
  }
  else if (eventType.EqualsLiteral("DOMMenuItemInactive")) {
    // Process DOMMenuItemInactive event for autocomplete only because this is
    // unique widget that may acquire focus from autocomplete popup while popup
    // stays open and has no active item. In case of XUL tree autocomplete
    // popup this event is fired for tree accessible.
    Accessible* widget =
      accessible->IsWidget() ? accessible : accessible->ContainerWidget();
    if (widget && widget->IsAutoCompletePopup()) {
      FocusMgr()->ActiveItemChanged(nullptr);
#ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eFocus))
        logging::ActiveItemChangeCausedBy("DOMMenuItemInactive", accessible);
#endif
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
#ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eFocus))
        logging::ActiveItemChangeCausedBy("DOMMenuBarActive", accessible);
#endif
    }
  }
  else if (eventType.EqualsLiteral("DOMMenuBarInactive")) {  // Always from user input
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENU_END,
                            accessible, eFromUserInput);

    FocusMgr()->ActiveItemChanged(nullptr);
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eFocus))
      logging::ActiveItemChangeCausedBy("DOMMenuBarInactive", accessible);
#endif
  }
  else if (accessible->NeedsDOMUIEvent() &&
           eventType.EqualsLiteral("ValueChange")) {
    uint32_t event = accessible->HasNumericValue()
      ? nsIAccessibleEvent::EVENT_VALUE_CHANGE
      : nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE;
     targetDocument->FireDelayedEvent(event, accessible);
  }
#ifdef DEBUG_DRAGDROPSTART
  else if (eventType.EqualsLiteral("mouseover")) {
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_DRAGDROP_START,
                            accessible);
  }
#endif
}


////////////////////////////////////////////////////////////////////////////////
// Accessible

void
RootAccessible::Shutdown()
{
  // Called manually or by Accessible::LastRelease()
  if (!PresShell())
    return;  // Already shutdown

  DocAccessibleWrap::Shutdown();
}

Relation
RootAccessible::RelationByType(RelationType aType) const
{
  if (!mDocumentNode || aType != RelationType::EMBEDS)
    return DocAccessibleWrap::RelationByType(aType);

  if (nsPIDOMWindowOuter* rootWindow = mDocumentNode->GetWindow()) {
    nsCOMPtr<nsPIDOMWindowOuter> contentWindow =
      nsGlobalWindowOuter::Cast(rootWindow)->GetContent();
    if (contentWindow) {
      nsCOMPtr<nsIDocument> contentDocumentNode = contentWindow->GetDoc();
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

    if (combobox->IsCombobox() || combobox->IsAutoComplete()) {
      RefPtr<AccEvent> event =
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

    uint32_t childCount = popupContainer->ChildCount();
    for (uint32_t idx = 0; idx < childCount; idx++) {
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

  static const uint32_t kNotifyOfFocus = 1;
  static const uint32_t kNotifyOfState = 2;
  uint32_t notifyOf = 0;

  // HTML select is target of popuphidding event. Otherwise get container
  // widget. No container widget means this is either tooltip or menupopup.
  // No events in the former case.
  Accessible* widget = nullptr;
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
    FocusMgr()->ActiveItemChanged(nullptr);
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eFocus))
      logging::ActiveItemChangeCausedBy("popuphiding", popup);
#endif
  }

  // Fire expanded state change event.
  if (notifyOf & kNotifyOfState) {
    RefPtr<AccEvent> event =
      new AccStateChangeEvent(widget, states::EXPANDED, false);
    document->FireDelayedEvent(event);
  }
}

#ifdef MOZ_XUL
static void
GetPropertyBagFromEvent(Event* aEvent, nsIPropertyBag2** aPropertyBag)
{
  *aPropertyBag = nullptr;

  CustomEvent* customEvent = aEvent->AsCustomEvent();
  if (!customEvent)
    return;

  AutoJSAPI jsapi;
  if (!jsapi.Init(customEvent->GetParentObject()))
    return;

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> detail(cx);
  customEvent->GetDetail(cx, &detail);
  if (!detail.isObject())
    return;

  JS::Rooted<JSObject*> detailObj(cx, &detail.toObject());

  nsresult rv;
  nsCOMPtr<nsIPropertyBag2> propBag;
  rv = UnwrapArg<nsIPropertyBag2>(cx, detailObj, getter_AddRefs(propBag));
  if (NS_FAILED(rv))
    return;

  propBag.forget(aPropertyBag);
}

void
RootAccessible::HandleTreeRowCountChangedEvent(Event* aEvent,
                                               XULTreeAccessible* aAccessible)
{
  nsCOMPtr<nsIPropertyBag2> propBag;
  GetPropertyBagFromEvent(aEvent, getter_AddRefs(propBag));
  if (!propBag)
    return;

  nsresult rv;
  int32_t index, count;
  rv = propBag->GetPropertyAsInt32(NS_LITERAL_STRING("index"), &index);
  if (NS_FAILED(rv))
    return;

  rv = propBag->GetPropertyAsInt32(NS_LITERAL_STRING("count"), &count);
  if (NS_FAILED(rv))
    return;

  aAccessible->InvalidateCache(index, count);
}

void
RootAccessible::HandleTreeInvalidatedEvent(Event* aEvent,
                                           XULTreeAccessible* aAccessible)
{
  nsCOMPtr<nsIPropertyBag2> propBag;
  GetPropertyBagFromEvent(aEvent, getter_AddRefs(propBag));
  if (!propBag)
    return;

  int32_t startRow = 0, endRow = -1, startCol = 0, endCol = -1;
  propBag->GetPropertyAsInt32(NS_LITERAL_STRING("startrow"),
                              &startRow);
  propBag->GetPropertyAsInt32(NS_LITERAL_STRING("endrow"),
                              &endRow);
  propBag->GetPropertyAsInt32(NS_LITERAL_STRING("startcolumn"),
                              &startCol);
  propBag->GetPropertyAsInt32(NS_LITERAL_STRING("endcolumn"),
                              &endCol);

  aAccessible->TreeViewInvalidated(startRow, endRow, startCol, endCol);
}
#endif

ProxyAccessible*
RootAccessible::GetPrimaryRemoteTopLevelContentDoc() const
{
  nsCOMPtr<nsIDocShellTreeOwner> owner;
  mDocumentNode->GetDocShell()->GetTreeOwner(getter_AddRefs(owner));
  NS_ENSURE_TRUE(owner, nullptr);

  nsCOMPtr<nsITabParent> tabParent;
  owner->GetPrimaryTabParent(getter_AddRefs(tabParent));
  if (!tabParent) {
    return nullptr;
  }

  auto tab = static_cast<dom::TabParent*>(tabParent.get());
  return tab->GetTopLevelDocAccessible();
}

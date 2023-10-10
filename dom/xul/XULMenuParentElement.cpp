/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULMenuParentElement.h"
#include "XULButtonElement.h"
#include "XULMenuBarElement.h"
#include "XULPopupElement.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/StaticAnalysisFunctions.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/EventDispatcher.h"
#include "nsDebug.h"
#include "nsMenuPopupFrame.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsUTF8Utils.h"
#include "nsXULElement.h"
#include "nsXULPopupManager.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(XULMenuParentElement,
                                               nsXULElement)
NS_IMPL_CYCLE_COLLECTION_INHERITED(XULMenuParentElement, nsXULElement,
                                   mActiveItem)

XULMenuParentElement::XULMenuParentElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsXULElement(std::move(aNodeInfo)) {}

XULMenuParentElement::~XULMenuParentElement() = default;

class MenuActivateEvent final : public Runnable {
 public:
  MenuActivateEvent(Element* aMenu, bool aIsActivate)
      : Runnable("MenuActivateEvent"), mMenu(aMenu), mIsActivate(aIsActivate) {}

  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230, bug 1535398)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    nsAutoString domEventToFire;
    if (mIsActivate) {
      // Highlight the menu.
      mMenu->SetAttr(kNameSpaceID_None, nsGkAtoms::menuactive, u"true"_ns,
                     true);
      // The menuactivated event is used by accessibility to track the user's
      // movements through menus
      domEventToFire.AssignLiteral("DOMMenuItemActive");
    } else {
      // Unhighlight the menu.
      mMenu->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menuactive, true);
      domEventToFire.AssignLiteral("DOMMenuItemInactive");
    }

    RefPtr<nsPresContext> pc = mMenu->OwnerDoc()->GetPresContext();
    RefPtr<dom::Event> event = NS_NewDOMEvent(mMenu, pc, nullptr);
    event->InitEvent(domEventToFire, true, true);

    event->SetTrusted(true);

    EventDispatcher::DispatchDOMEvent(mMenu, nullptr, event, pc, nullptr);
    return NS_OK;
  }

 private:
  const RefPtr<Element> mMenu;
  bool mIsActivate;
};

static void ActivateOrDeactivate(XULButtonElement& aButton, bool aActivate) {
  if (!aButton.IsMenu()) {
    return;
  }

  if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
    if (aActivate) {
      // Cancel the close timer if selecting a menu within the popup to be
      // closed.
      pm->CancelMenuTimer(aButton.GetContainingPopupWithoutFlushing());
    } else if (auto* popup = aButton.GetMenuPopupWithoutFlushing()) {
      if (popup->IsOpen()) {
        // Set up the close timer if deselecting an open sub-menu.
        pm->HidePopupAfterDelay(popup, aButton.MenuOpenCloseDelay());
      }
    }
  }

  nsCOMPtr<nsIRunnable> event = new MenuActivateEvent(&aButton, aActivate);
  aButton.OwnerDoc()->Dispatch(event.forget());
}

XULButtonElement* XULMenuParentElement::GetContainingMenu() const {
  if (IsMenuBar()) {
    return nullptr;
  }
  auto* button = XULButtonElement::FromNodeOrNull(GetParent());
  if (!button || !button->IsMenu()) {
    return nullptr;
  }
  return button;
}

void XULMenuParentElement::LockMenuUntilClosed(bool aLock) {
  if (IsMenuBar()) {
    return;
  }
  mLocked = aLock;
  // Lock/Unlock the parent menu too.
  if (XULButtonElement* menu = GetContainingMenu()) {
    if (XULMenuParentElement* parent = menu->GetMenuParent()) {
      parent->LockMenuUntilClosed(aLock);
    }
  }
}

void XULMenuParentElement::SetActiveMenuChild(XULButtonElement* aChild,
                                              ByKey aByKey) {
  if (aChild == mActiveItem) {
    return;
  }

  if (mActiveItem) {
    ActivateOrDeactivate(*mActiveItem, false);
  }
  mActiveItem = nullptr;

  if (auto* menuBar = XULMenuBarElement::FromNode(*this)) {
    // KnownLive because `this` is known-live by definition.
    MOZ_KnownLive(menuBar)->SetActive(!!aChild);
  }

  if (!aChild) {
    return;
  }

  // When a menu opens a submenu, the mouse will often be moved onto a sibling
  // before moving onto an item within the submenu, causing the parent to become
  // deselected. We need to ensure that the parent menu is reselected when an
  // item in the submenu is selected.
  if (RefPtr menu = GetContainingMenu()) {
    if (RefPtr parent = menu->GetMenuParent()) {
      parent->SetActiveMenuChild(menu, aByKey);
    }
  }

  mActiveItem = aChild;
  ActivateOrDeactivate(*mActiveItem, true);

  if (IsInMenuList()) {
    if (nsMenuPopupFrame* f = do_QueryFrame(GetPrimaryFrame())) {
      f->EnsureActiveMenuListItemIsVisible();
#ifdef XP_WIN
      // On Windows, a menulist should update its value whenever navigation was
      // done by the keyboard.
      //
      // NOTE(emilio): This is a rather odd per-platform behavior difference,
      // but other browsers also do this.
      if (aByKey == ByKey::Yes && f->IsOpen()) {
        // Fire a command event as the new item, but we don't want to close the
        // menu, blink it, or update any other state of the menuitem. The
        // command event will cause the item to be selected.
        RefPtr<mozilla::PresShell> presShell = OwnerDoc()->GetPresShell();
        nsContentUtils::DispatchXULCommand(aChild, /* aTrusted = */ true,
                                           nullptr, presShell, false, false,
                                           false, false);
      }
#endif
    }
  }
}

static bool IsValidMenuItem(const XULMenuParentElement& aMenuParent,
                            const nsIContent& aContent) {
  const auto* button = XULButtonElement::FromNode(aContent);
  if (!button || !button->IsMenu()) {
    return false;
  }
  if (!button->GetPrimaryFrame()) {
    // Hidden buttons are not focusable/activatable.
    return false;
  }
  if (!button->IsDisabled()) {
    return true;
  }
  // In the menubar or a menulist disabled items are always skipped.
  const bool skipDisabled =
      LookAndFeel::GetInt(LookAndFeel::IntID::SkipNavigatingDisabledMenuItem) ||
      aMenuParent.IsMenuBar() || aMenuParent.IsInMenuList();
  return !skipDisabled;
}

enum class StartKind { Parent, Item };

template <bool aForward>
static XULButtonElement* DoGetNextMenuItem(
    const XULMenuParentElement& aMenuParent, const nsIContent& aStart,
    StartKind aStartKind) {
  nsIContent* start =
      aStartKind == StartKind::Item
          ? (aForward ? aStart.GetNextSibling() : aStart.GetPreviousSibling())
          : (aForward ? aStart.GetFirstChild() : aStart.GetLastChild());
  for (nsIContent* node = start; node;
       node = aForward ? node->GetNextSibling() : node->GetPreviousSibling()) {
    if (IsValidMenuItem(aMenuParent, *node)) {
      return static_cast<XULButtonElement*>(node);
    }
    if (node->IsXULElement(nsGkAtoms::menugroup)) {
      if (XULButtonElement* child = DoGetNextMenuItem<aForward>(
              aMenuParent, *node, StartKind::Parent)) {
        return child;
      }
    }
  }
  if (aStartKind == StartKind::Item && aStart.GetParent() &&
      aStart.GetParent()->IsXULElement(nsGkAtoms::menugroup)) {
    // We haven't found anything in aStart's sibling list, but if we're in a
    // group we need to keep looking.
    return DoGetNextMenuItem<aForward>(aMenuParent, *aStart.GetParent(),
                                       StartKind::Item);
  }
  return nullptr;
}

XULButtonElement* XULMenuParentElement::GetFirstMenuItem() const {
  return DoGetNextMenuItem<true>(*this, *this, StartKind::Parent);
}

XULButtonElement* XULMenuParentElement::GetLastMenuItem() const {
  return DoGetNextMenuItem<false>(*this, *this, StartKind::Parent);
}

XULButtonElement* XULMenuParentElement::GetNextMenuItemFrom(
    const XULButtonElement& aStartingItem) const {
  return DoGetNextMenuItem<true>(*this, aStartingItem, StartKind::Item);
}

XULButtonElement* XULMenuParentElement::GetPrevMenuItemFrom(
    const XULButtonElement& aStartingItem) const {
  return DoGetNextMenuItem<false>(*this, aStartingItem, StartKind::Item);
}

XULButtonElement* XULMenuParentElement::GetNextMenuItem(Wrap aWrap) const {
  if (mActiveItem) {
    if (auto* next = GetNextMenuItemFrom(*mActiveItem)) {
      return next;
    }
    if (aWrap == Wrap::No) {
      return nullptr;
    }
  }
  return GetFirstMenuItem();
}

XULButtonElement* XULMenuParentElement::GetPrevMenuItem(Wrap aWrap) const {
  if (mActiveItem) {
    if (auto* prev = GetPrevMenuItemFrom(*mActiveItem)) {
      return prev;
    }
    if (aWrap == Wrap::No) {
      return nullptr;
    }
  }
  return GetLastMenuItem();
}

void XULMenuParentElement::SelectFirstItem() {
  if (RefPtr firstItem = GetFirstMenuItem()) {
    SetActiveMenuChild(firstItem);
  }
}

XULButtonElement* XULMenuParentElement::FindMenuWithShortcut(
    KeyboardEvent& aKeyEvent) const {
  using AccessKeyArray = AutoTArray<uint32_t, 10>;
  AccessKeyArray accessKeys;
  WidgetKeyboardEvent* nativeKeyEvent =
      aKeyEvent.WidgetEventPtr()->AsKeyboardEvent();
  if (nativeKeyEvent) {
    nativeKeyEvent->GetAccessKeyCandidates(accessKeys);
  }
  const uint32_t charCode = aKeyEvent.CharCode();
  if (accessKeys.IsEmpty() && charCode) {
    accessKeys.AppendElement(charCode);
  }
  if (accessKeys.IsEmpty()) {
    return nullptr;  // no character was pressed so just return
  }
  XULButtonElement* foundMenu = nullptr;
  size_t foundIndex = AccessKeyArray::NoIndex;
  for (auto* item = GetFirstMenuItem(); item;
       item = GetNextMenuItemFrom(*item)) {
    nsAutoString shortcutKey;
    item->GetAttr(nsGkAtoms::accesskey, shortcutKey);
    if (shortcutKey.IsEmpty()) {
      continue;
    }

    ToLowerCase(shortcutKey);
    const char16_t* start = shortcutKey.BeginReading();
    const char16_t* end = shortcutKey.EndReading();
    uint32_t ch = UTF16CharEnumerator::NextChar(&start, end);
    size_t index = accessKeys.IndexOf(ch);
    if (index == AccessKeyArray::NoIndex) {
      continue;
    }
    if (foundIndex == AccessKeyArray::NoIndex || index < foundIndex) {
      foundMenu = item;
      foundIndex = index;
    }
  }
  return foundMenu;
}

XULButtonElement* XULMenuParentElement::FindMenuWithShortcut(
    const nsAString& aString, bool& aDoAction) const {
  aDoAction = false;
  uint32_t accessKeyMatchCount = 0;
  uint32_t matchCount = 0;

  XULButtonElement* foundAccessKeyMenu = nullptr;
  XULButtonElement* foundMenuBeforeCurrent = nullptr;
  XULButtonElement* foundMenuAfterCurrent = nullptr;

  bool foundActive = false;
  for (auto* item = GetFirstMenuItem(); item;
       item = GetNextMenuItemFrom(*item)) {
    nsAutoString textKey;
    // Get the shortcut attribute.
    item->GetAttr(nsGkAtoms::accesskey, textKey);
    const bool isAccessKey = !textKey.IsEmpty();
    if (textKey.IsEmpty()) {  // No shortcut, try first letter
      item->GetAttr(nsGkAtoms::label, textKey);
      if (textKey.IsEmpty()) {  // No label, try another attribute (value)
        item->GetAttr(nsGkAtoms::value, textKey);
      }
    }

    const bool isActive = item == GetActiveMenuChild();
    foundActive |= isActive;

    if (!StringBeginsWith(
            nsContentUtils::TrimWhitespace<
                nsContentUtils::IsHTMLWhitespaceOrNBSP>(textKey, false),
            aString, nsCaseInsensitiveStringComparator)) {
      continue;
    }
    // There is one match
    matchCount++;
    if (isAccessKey) {
      // There is one shortcut-key match
      accessKeyMatchCount++;
      // Record the matched item. If there is only one matched shortcut
      // item, do it
      foundAccessKeyMenu = item;
    }
    // Get the active status
    if (isActive && aString.Length() > 1 && !foundMenuBeforeCurrent) {
      // If there is more than one char typed and the current item matches, the
      // current item has highest priority, otherwise the item next to current
      // has highest priority.
      return item;
    }
    if (!foundActive || isActive) {
      // It's a first candidate item located before/on the current item
      if (!foundMenuBeforeCurrent) {
        foundMenuBeforeCurrent = item;
      }
    } else {
      if (!foundMenuAfterCurrent) {
        foundMenuAfterCurrent = item;
      }
    }
  }

  aDoAction = !IsInMenuList() && (matchCount == 1 || accessKeyMatchCount == 1);

  if (accessKeyMatchCount == 1) {
    // We have one matched accesskey item
    return foundAccessKeyMenu;
  }
  // If we have matched an item after the current, use it.
  if (foundMenuAfterCurrent) {
    return foundMenuAfterCurrent;
  }
  // If we haven't, use the item before the current, if any.
  return foundMenuBeforeCurrent;
}

void XULMenuParentElement::HandleEnterKeyPress(WidgetEvent& aEvent) {
  if (RefPtr child = GetActiveMenuChild()) {
    child->HandleEnterKeyPress(aEvent);
  }
}

}  // namespace mozilla::dom

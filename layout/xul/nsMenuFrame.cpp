/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsMenuFrame.h"
#include "nsBoxFrame.h"
#include "nsIContent.h"
#include "nsAtom.h"
#include "nsPresContext.h"
#include "mozilla/ComputedStyle.h"
#include "nsCSSRendering.h"
#include "nsNameSpaceManager.h"
#include "nsMenuPopupFrame.h"
#include "nsMenuBarFrame.h"
#include "mozilla/dom/Document.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIStringBundle.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsIReflowCallback.h"
#include "nsISound.h"
#include "nsLayoutUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Likely.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/Services.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/UserActivation.h"
#include <algorithm>

using namespace mozilla;
using dom::Element;

#define NS_MENU_POPUP_LIST_INDEX 0

#if defined(XP_WIN)
#  define NSCONTEXTMENUISMOUSEUP 1
#endif

NS_DECLARE_FRAME_PROPERTY_FRAMELIST(PopupListProperty)

// This global flag indicates that a menu just opened or closed and is used
// to ignore the mousemove and mouseup events that would fire on the menu after
// the mousedown occurred.
static int32_t gMenuJustOpenedOrClosed = false;

const int32_t kBlinkDelay = 67;  // milliseconds

// this class is used for dispatching menu activation events asynchronously.
class nsMenuActivateEvent : public Runnable {
 public:
  nsMenuActivateEvent(Element* aMenu, nsPresContext* aPresContext,
                      bool aIsActivate)
      : mozilla::Runnable("nsMenuActivateEvent"),
        mMenu(aMenu),
        mPresContext(aPresContext),
        mIsActivate(aIsActivate) {}

  NS_IMETHOD Run() override {
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

    RefPtr<dom::Event> event = NS_NewDOMEvent(mMenu, mPresContext, nullptr);
    event->InitEvent(domEventToFire, true, true);

    event->SetTrusted(true);

    EventDispatcher::DispatchDOMEvent(mMenu, nullptr, event, mPresContext,
                                      nullptr);

    return NS_OK;
  }

 private:
  RefPtr<Element> mMenu;
  RefPtr<nsPresContext> mPresContext;
  bool mIsActivate;
};

class nsMenuAttributeChangedEvent : public Runnable {
 public:
  nsMenuAttributeChangedEvent(nsIFrame* aFrame, nsAtom* aAttr)
      : mozilla::Runnable("nsMenuAttributeChangedEvent"),
        mFrame(aFrame),
        mAttr(aAttr) {}

  NS_IMETHOD Run() override {
    nsMenuFrame* frame = static_cast<nsMenuFrame*>(mFrame.GetFrame());
    NS_ENSURE_STATE(frame);
    if (mAttr == nsGkAtoms::checked) {
      frame->UpdateMenuSpecialState();
    } else if (mAttr == nsGkAtoms::type || mAttr == nsGkAtoms::name) {
      frame->UpdateMenuType();
    }
    return NS_OK;
  }

 protected:
  WeakFrame mFrame;
  RefPtr<nsAtom> mAttr;
};

//
// NS_NewMenuFrame and NS_NewMenuItemFrame
//
// Wrappers for creating a new menu popup container
//
nsIFrame* NS_NewMenuFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  nsMenuFrame* it =
      new (aPresShell) nsMenuFrame(aStyle, aPresShell->GetPresContext());
  it->SetIsMenu(true);
  return it;
}

nsIFrame* NS_NewMenuItemFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  nsMenuFrame* it =
      new (aPresShell) nsMenuFrame(aStyle, aPresShell->GetPresContext());
  it->SetIsMenu(false);
  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsMenuFrame)

NS_QUERYFRAME_HEAD(nsMenuFrame)
  NS_QUERYFRAME_ENTRY(nsMenuFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

nsMenuFrame::nsMenuFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
    : nsBoxFrame(aStyle, aPresContext, kClassID),
      mIsMenu(false),
      mChecked(false),
      mReflowCallbackPosted(false),
      mType(eMenuType_Normal),
      mBlinkState(0) {}

nsMenuParent* nsMenuFrame::GetMenuParent() const {
  nsContainerFrame* parent = GetParent();
  for (; parent; parent = parent->GetParent()) {
    nsMenuPopupFrame* popup = do_QueryFrame(parent);
    if (popup) {
      return popup;
    }
    nsMenuBarFrame* menubar = do_QueryFrame(parent);
    if (menubar) {
      return menubar;
    }
  }
  return nullptr;
}

bool nsMenuFrame::ReflowFinished() {
  mReflowCallbackPosted = false;

  UpdateMenuType();
  return true;
}

void nsMenuFrame::ReflowCallbackCanceled() { mReflowCallbackPosted = false; }

void nsMenuFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                       nsIFrame* aPrevInFlow) {
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Set up a mediator which can be used for callbacks on this frame.
  mTimerMediator = new nsMenuTimerMediator(this);

  if (!mReflowCallbackPosted) {
    mReflowCallbackPosted = true;
    PresShell()->PostReflowCallback(this);
  }
}

const nsFrameList& nsMenuFrame::GetChildList(ChildListID aListID) const {
  if (kPopupList == aListID) {
    nsFrameList* list = GetPopupList();
    return list ? *list : nsFrameList::EmptyList();
  }
  return nsBoxFrame::GetChildList(aListID);
}

void nsMenuFrame::GetChildLists(nsTArray<ChildList>* aLists) const {
  nsBoxFrame::GetChildLists(aLists);
  nsFrameList* list = GetPopupList();
  if (list) {
    list->AppendIfNonempty(aLists, kPopupList);
  }
}

nsMenuPopupFrame* nsMenuFrame::GetPopup() const {
  nsFrameList* popupList = GetPopupList();
  return popupList ? static_cast<nsMenuPopupFrame*>(popupList->FirstChild())
                   : nullptr;
}

nsFrameList* nsMenuFrame::GetPopupList() const {
  if (!HasPopup()) {
    return nullptr;
  }
  nsFrameList* prop = GetProperty(PopupListProperty());
  NS_ASSERTION(
      prop && prop->GetLength() == 1 && prop->FirstChild()->IsMenuPopupFrame(),
      "popup list should have exactly one nsMenuPopupFrame");
  return prop;
}

void nsMenuFrame::DestroyPopupList() {
  NS_ASSERTION(HasPopup(), "huh?");
  nsFrameList* prop = TakeProperty(PopupListProperty());
  NS_ASSERTION(prop && prop->IsEmpty(),
               "popup list must exist and be empty when destroying");
  RemoveStateBits(NS_STATE_MENU_HAS_POPUP_LIST);
  prop->Delete(PresShell());
}

void nsMenuFrame::SetPopupFrame(nsFrameList& aFrameList) {
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    nsMenuPopupFrame* popupFrame = do_QueryFrame(e.get());
    if (popupFrame) {
      // Remove the frame from the list and store it in a nsFrameList* property.
      aFrameList.RemoveFrame(popupFrame);
      nsFrameList* popupList =
          new (PresShell()) nsFrameList(popupFrame, popupFrame);
      SetProperty(PopupListProperty(), popupList);
      AddStateBits(NS_STATE_MENU_HAS_POPUP_LIST);
      break;
    }
  }
}

void nsMenuFrame::SetInitialChildList(ChildListID aListID,
                                      nsFrameList& aChildList) {
  if (aListID == kPrincipalList || aListID == kPopupList) {
    NS_ASSERTION(!HasPopup(), "SetInitialChildList called twice?");
#ifdef DEBUG
    for (nsIFrame* f : aChildList) {
      MOZ_ASSERT(f->GetParent() == this, "Unexpected parent");
    }
#endif
    SetPopupFrame(aChildList);
  }
  nsBoxFrame::SetInitialChildList(aListID, aChildList);
}

void nsMenuFrame::DestroyFrom(nsIFrame* aDestructRoot,
                              PostDestroyData& aPostDestroyData) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());

  if (mReflowCallbackPosted) {
    PresShell()->CancelReflowCallback(this);
    mReflowCallbackPosted = false;
  }

  // Kill our timer if one is active. This is not strictly necessary as
  // the pointer to this frame will be cleared from the mediator, but
  // this is done for added safety.
  if (mOpenTimer) {
    mOpenTimer->Cancel();
  }

  StopBlinking();

  // Null out the pointer to this frame in the mediator wrapper so that it
  // doesn't try to interact with a deallocated frame.
  mTimerMediator->ClearFrame();

  // if the menu content is just being hidden, it may be made visible again
  // later, so make sure to clear the highlighting.
  nsContentUtils::AddScriptRunner(
      new nsUnsetAttrRunnable(mContent->AsElement(), nsGkAtoms::menuactive));

  // are we our menu parent's current menu item?
  nsMenuParent* menuParent = GetMenuParent();
  if (menuParent && menuParent->GetCurrentMenuItem() == this) {
    // yes; tell it that we're going away
    menuParent->CurrentMenuIsBeingDestroyed();
  }

  nsFrameList* popupList = GetPopupList();
  if (popupList) {
    popupList->DestroyFramesFrom(aDestructRoot, aPostDestroyData);
    DestroyPopupList();
  }

  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsMenuFrame::BuildDisplayListForChildren(nsDisplayListBuilder* aBuilder,
                                              const nsDisplayListSet& aLists) {
  if (!aBuilder->IsForEventDelivery()) {
    nsBoxFrame::BuildDisplayListForChildren(aBuilder, aLists);
    return;
  }

  nsDisplayListCollection set(aBuilder);
  nsBoxFrame::BuildDisplayListForChildren(aBuilder, set);

  WrapListsInRedirector(aBuilder, set, aLists);
}

nsresult nsMenuFrame::HandleEvent(nsPresContext* aPresContext,
                                  WidgetGUIEvent* aEvent,
                                  nsEventStatus* aEventStatus) {
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }
  nsMenuParent* menuParent = GetMenuParent();
  if (menuParent && menuParent->IsMenuLocked()) {
    return NS_OK;
  }

  AutoWeakFrame weakFrame(this);
  if (*aEventStatus == nsEventStatus_eIgnore)
    *aEventStatus = nsEventStatus_eConsumeDoDefault;

  // If a menu just opened, ignore the mouseup event that might occur after a
  // the mousedown event that opened it. However, if a different mousedown
  // event occurs, just clear this flag.
  if (gMenuJustOpenedOrClosed) {
    if (aEvent->mMessage == eMouseDown) {
      gMenuJustOpenedOrClosed = false;
    } else if (aEvent->mMessage == eMouseUp) {
      return NS_OK;
    }
  }

  bool onmenu = IsOnMenu();

  if (aEvent->mMessage == eKeyPress && !IsDisabled()) {
    WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
    uint32_t keyCode = keyEvent->mKeyCode;
#ifdef XP_MACOSX
    // On mac, open menulist on either up/down arrow or space (w/o Cmd pressed)
    if (!IsOpen() && ((keyEvent->mCharCode == ' ' && !keyEvent->IsMeta()) ||
                      (keyCode == NS_VK_UP || keyCode == NS_VK_DOWN))) {
      // When pressing space, don't open the menu if performing an incremental
      // search.
      if (keyEvent->mCharCode != ' ' ||
          !nsMenuPopupFrame::IsWithinIncrementalTime(keyEvent->mTime)) {
        *aEventStatus = nsEventStatus_eConsumeNoDefault;
        OpenMenu(false);
      }
    }
#else
    // On other platforms, toggle menulist on unmodified F4 or Alt arrow
    if ((keyCode == NS_VK_F4 && !keyEvent->IsAlt()) ||
        ((keyCode == NS_VK_UP || keyCode == NS_VK_DOWN) && keyEvent->IsAlt())) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
      ToggleMenuState();
    }
#endif
  } else if (aEvent->mMessage == eMouseDown &&
             aEvent->AsMouseEvent()->mButton == MouseButton::ePrimary &&
#ifdef XP_MACOSX
             // On mac, ctrl-click will send a context menu event from the
             // widget, so we don't want to bring up the menu.
             !aEvent->AsMouseEvent()->IsControl() &&
#endif
             !IsDisabled() && IsMenu()) {
    // The menu item was selected. Bring up the menu.
    // We have children.
    // Don't prevent the default action here, since that will also cancel
    // potential drag starts.
    if (!menuParent || menuParent->IsMenuBar()) {
      ToggleMenuState();
    } else {
      if (!IsOpen()) {
        menuParent->ChangeMenuItem(this, false, false);
        OpenMenu(false);
      }
    }
  } else if (aEvent->mMessage == eMouseUp && !IsMenu() && !IsDisabled()) {
    // We accept left and middle clicks on all menu items to activate the item.
    // On context menus we also accept right click to activate the item, because
    // right-clicking on an item in a context menu cannot open another context
    // menu.
    bool isMacCtrlClick = false;
#ifdef XP_MACOSX
    isMacCtrlClick = aEvent->AsMouseEvent()->mButton == MouseButton::ePrimary &&
                     aEvent->AsMouseEvent()->IsControl();
#endif
    bool clickMightOpenContextMenu =
        aEvent->AsMouseEvent()->mButton == MouseButton::eSecondary ||
        isMacCtrlClick;
    if (!clickMightOpenContextMenu || (onmenu && menuParent->IsContextMenu())) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
      Execute(aEvent);
    }
  } else if (aEvent->mMessage == eContextMenu && onmenu && !IsMenu() &&
             !IsDisabled() && menuParent->IsContextMenu()) {
    // Make sure we cancel default processing of the context menu event so
    // that it doesn't bubble and get seen again by the popuplistener and show
    // another context menu.
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  } else if (aEvent->mMessage == eMouseOut) {
    // Kill our timer if one is active.
    if (mOpenTimer) {
      mOpenTimer->Cancel();
      mOpenTimer = nullptr;
    }

    // Deactivate the menu.
    if (menuParent) {
      bool onmenubar = menuParent->IsMenuBar();
      if (!(onmenubar && menuParent->IsActive())) {
        if (IsMenu() && !onmenubar && IsOpen()) {
          // Submenus don't get closed up immediately.
        } else if (this == menuParent->GetCurrentMenuItem()
#ifdef XP_WIN
                   && !IsParentMenuList()
#endif
        ) {
          menuParent->ChangeMenuItem(nullptr, false, false);
        }
      }
    }
  } else if (aEvent->mMessage == eMouseMove &&
             (onmenu || (menuParent && menuParent->IsMenuBar()))) {
    if (gMenuJustOpenedOrClosed) {
      gMenuJustOpenedOrClosed = false;
      return NS_OK;
    }

    if (IsDisabled() && IsParentMenuList()) {
      return NS_OK;
    }

    // Let the menu parent know we're the new item.
    menuParent->ChangeMenuItem(this, false, false);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    NS_ENSURE_TRUE(menuParent, NS_OK);

    // we need to check if we really became the current menu
    // item or not
    nsMenuFrame* realCurrentItem = menuParent->GetCurrentMenuItem();
    if (realCurrentItem != this) {
      // we didn't (presumably because a context menu was active)
      return NS_OK;
    }

    // Hovering over a menu in a popup should open it without a need for a
    // click. A timer is used so that it doesn't open if the user moves the
    // mouse quickly past the menu. This conditional check ensures that only
    // menus have this behaviour
    if (!IsDisabled() && IsMenu() && !IsOpen() && !mOpenTimer &&
        !menuParent->IsMenuBar()) {
      int32_t menuDelay =
          LookAndFeel::GetInt(LookAndFeel::IntID::SubmenuDelay, 300);  // ms

      // We're a menu, we're built, we're closed, and no timer has been kicked
      // off.
      NS_NewTimerWithCallback(
          getter_AddRefs(mOpenTimer), mTimerMediator, menuDelay,
          nsITimer::TYPE_ONE_SHOT,
          mContent->OwnerDoc()->EventTargetFor(TaskCategory::Other));
    }
  }

  return NS_OK;
}

void nsMenuFrame::ToggleMenuState() {
  if (IsOpen())
    CloseMenu(false);
  else
    OpenMenu(false);
}

void nsMenuFrame::PopupOpened() {
  gMenuJustOpenedOrClosed = true;

  AutoWeakFrame weakFrame(this);
  mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::open, u"true"_ns,
                                 true);
  if (!weakFrame.IsAlive()) return;

  nsMenuParent* menuParent = GetMenuParent();
  if (menuParent) {
    menuParent->SetActive(true);
    // Make sure the current menu which is being toggled on
    // the menubar is highlighted
    menuParent->SetCurrentMenuItem(this);
  }
}

void nsMenuFrame::PopupClosed(bool aDeselectMenu) {
  AutoWeakFrame weakFrame(this);
  nsContentUtils::AddScriptRunner(
      new nsUnsetAttrRunnable(mContent->AsElement(), nsGkAtoms::open));
  if (!weakFrame.IsAlive()) return;

  // if the popup is for a menu on a menubar, inform menubar to deactivate
  nsMenuParent* menuParent = GetMenuParent();
  if (menuParent && menuParent->MenuClosed()) {
    if (aDeselectMenu) {
      SelectMenu(false);
    } else {
      // We are not deselecting the parent menu while closing the popup, so send
      // a DOMMenuItemActive event to the menu to indicate that the menu is
      // becoming active again.
      nsMenuFrame* current = menuParent->GetCurrentMenuItem();
      if (current) {
        // However, if the menu is a descendant on a menubar, and the menubar
        // has the 'stay active' flag set, it means that the menubar is
        // switching to another toplevel menu entirely (for example from Edit to
        // View), so don't fire the DOMMenuItemActive event or else we'll send
        // extraneous events for submenus. nsMenuBarFrame::ChangeMenuItem has
        // already deselected the old menu, so it doesn't need to happen again
        // here, and the new menu can be selected right away.
        nsIFrame* parent = current;
        while (parent) {
          nsMenuBarFrame* menubar = do_QueryFrame(parent);
          if (menubar && menubar->GetStayActive()) return;

          parent = parent->GetParent();
        }

        nsCOMPtr<nsIRunnable> event = new nsMenuActivateEvent(
            current->GetContent()->AsElement(), PresContext(), true);
        mContent->OwnerDoc()->Dispatch(TaskCategory::Other, event.forget());
      }
    }
  }
}

NS_IMETHODIMP
nsMenuFrame::SelectMenu(bool aActivateFlag) {
  if (mContent) {
    // When a menu opens a submenu, the mouse will often be moved onto a
    // sibling before moving onto an item within the submenu, causing the
    // parent to become deselected. We need to ensure that the parent menu
    // is reselected when an item in the submenu is selected, so navigate up
    // from the item to its popup, and then to the popup above that.
    if (aActivateFlag) {
      nsIFrame* parent = GetParent();
      while (parent) {
        nsMenuPopupFrame* menupopup = do_QueryFrame(parent);
        if (menupopup) {
          // a menu is always the direct parent of a menupopup
          nsMenuFrame* menu = do_QueryFrame(menupopup->GetParent());
          if (menu) {
            // a popup however is not necessarily the direct parent of a menu
            nsIFrame* popupParent = menu->GetParent();
            while (popupParent) {
              menupopup = do_QueryFrame(popupParent);
              if (menupopup) {
                menupopup->SetCurrentMenuItem(menu);
                break;
              }
              popupParent = popupParent->GetParent();
            }
          }
          break;
        }
        parent = parent->GetParent();
      }
    }

    // cancel the close timer if selecting a menu within the popup to be closed
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      nsMenuParent* menuParent = GetMenuParent();
      pm->CancelMenuTimer(menuParent);
    }

    nsCOMPtr<nsIRunnable> event = new nsMenuActivateEvent(
        mContent->AsElement(), PresContext(), aActivateFlag);
    mContent->OwnerDoc()->Dispatch(TaskCategory::Other, event.forget());
  }

  return NS_OK;
}

nsresult nsMenuFrame::AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                       int32_t aModType) {
  if (aAttribute == nsGkAtoms::checked || aAttribute == nsGkAtoms::acceltext ||
      aAttribute == nsGkAtoms::key || aAttribute == nsGkAtoms::type ||
      aAttribute == nsGkAtoms::name) {
    nsCOMPtr<nsIRunnable> event =
        new nsMenuAttributeChangedEvent(this, aAttribute);
    nsContentUtils::AddScriptRunner(event);
  }
  return NS_OK;
}

void nsMenuFrame::OpenMenu(bool aSelectFirstItem) {
  if (!mContent) return;

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    pm->KillMenuTimer();
    // This opens the menu asynchronously
    pm->ShowMenu(mContent, aSelectFirstItem, true);
  }
}

void nsMenuFrame::CloseMenu(bool aDeselectMenu) {
  gMenuJustOpenedOrClosed = true;

  // Close the menu asynchronously
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && HasPopup())
    pm->HidePopup(GetPopup()->GetContent(), false, aDeselectMenu, true, false);
}

bool nsMenuFrame::IsSizedToPopup(nsIContent* aContent, bool aRequireAlways) {
  MOZ_ASSERT(aContent->IsElement());
  nsAutoString sizedToPopup;
  aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::sizetopopup,
                                 sizedToPopup);
  bool sizedToPopupSetToPref =
      sizedToPopup.EqualsLiteral("pref") ||
      (sizedToPopup.IsEmpty() && aContent->IsXULElement(nsGkAtoms::menulist));
  return sizedToPopup.EqualsLiteral("always") ||
         (!aRequireAlways && sizedToPopupSetToPref);
}

nsSize nsMenuFrame::GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) {
  nsSize size = nsBoxFrame::GetXULMinSize(aBoxLayoutState);
  DISPLAY_MIN_SIZE(this, size);

  if (IsSizedToPopup(mContent, true)) SizeToPopup(aBoxLayoutState, size);

  return size;
}

NS_IMETHODIMP
nsMenuFrame::DoXULLayout(nsBoxLayoutState& aState) {
  // lay us out
  nsresult rv = nsBoxFrame::DoXULLayout(aState);

  nsMenuPopupFrame* popupFrame = GetPopup();
  if (popupFrame) {
    bool sizeToPopup = IsSizedToPopup(mContent, false);
    popupFrame->LayoutPopup(aState, this, sizeToPopup);
  }

  return rv;
}

//
// Enter
//
// Called when the user hits the <Enter>/<Return> keys or presses the
// shortcut key. If this is a leaf item, the item's action will be executed.
// In either case, do nothing if the item is disabled.
//
nsMenuFrame* nsMenuFrame::Enter(WidgetGUIEvent* aEvent) {
  if (IsDisabled()) {
#ifdef XP_WIN
    // behavior on Windows - close the popup chain
    nsMenuParent* menuParent = GetMenuParent();
    if (menuParent) {
      nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
      if (pm) {
        nsIFrame* popup = pm->GetTopPopup(ePopupTypeAny);
        if (popup) pm->HidePopup(popup->GetContent(), true, true, true, false);
      }
    }
#endif  // #ifdef XP_WIN
    // this menu item was disabled - exit
    return nullptr;
  }

  if (!IsOpen()) {
    // The enter key press applies to us.
    nsMenuParent* menuParent = GetMenuParent();
    if (!IsMenu() && menuParent)
      Execute(aEvent);  // Execute our event handler
    else
      return this;
  }

  return nullptr;
}

bool nsMenuFrame::IsOpen() {
  nsMenuPopupFrame* popupFrame = GetPopup();
  return popupFrame && popupFrame->IsOpen();
}

bool nsMenuFrame::IsMenu() { return mIsMenu; }

bool nsMenuFrame::IsParentMenuList() {
  nsMenuParent* menuParent = GetMenuParent();
  if (menuParent && menuParent->IsMenu()) {
    nsMenuPopupFrame* popupFrame = static_cast<nsMenuPopupFrame*>(menuParent);
    return popupFrame->IsMenuList();
  }
  return false;
}

nsresult nsMenuFrame::Notify(nsITimer* aTimer) {
  // Our timer has fired.
  if (aTimer == mOpenTimer.get()) {
    mOpenTimer = nullptr;

    nsMenuParent* menuParent = GetMenuParent();
    if (!IsOpen() && menuParent) {
      // make sure we didn't open a context menu in the meantime
      // (i.e. the user right-clicked while hovering over a submenu).
      nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
      if (pm) {
        if ((!pm->HasContextMenu(nullptr) || menuParent->IsContextMenu()) &&
            mContent->AsElement()->AttrValueIs(
                kNameSpaceID_None, nsGkAtoms::menuactive, nsGkAtoms::_true,
                eCaseMatters)) {
          OpenMenu(false);
        }
      }
    }
  } else if (aTimer == mBlinkTimer) {
    switch (mBlinkState++) {
      case 0:
        NS_ASSERTION(false, "Blink timer fired while not blinking");
        StopBlinking();
        break;
      case 1: {
        // Turn the highlight back on and wait for a while before closing the
        // menu.
        AutoWeakFrame weakFrame(this);
        mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::menuactive,
                                       u"true"_ns, true);
        if (weakFrame.IsAlive()) {
          aTimer->InitWithCallback(mTimerMediator, kBlinkDelay,
                                   nsITimer::TYPE_ONE_SHOT);
        }
      } break;
      default: {
        nsMenuParent* menuParent = GetMenuParent();
        if (menuParent) {
          menuParent->LockMenuUntilClosed(false);
        }
        PassMenuCommandEventToPopupManager();
        StopBlinking();
        break;
      }
    }
  }

  return NS_OK;
}

bool nsMenuFrame::IsDisabled() {
  return mContent->AsElement()->AttrValueIs(
      kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters);
}

void nsMenuFrame::UpdateMenuType() {
  static Element::AttrValuesArray strings[] = {nsGkAtoms::checkbox,
                                               nsGkAtoms::radio, nullptr};
  switch (mContent->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::type, strings, eCaseMatters)) {
    case 0:
      mType = eMenuType_Checkbox;
      break;
    case 1:
      mType = eMenuType_Radio;
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::name,
                                     mGroupName);
      break;

    default:
      if (mType != eMenuType_Normal) {
        AutoWeakFrame weakFrame(this);
        mContent->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                                         true);
        NS_ENSURE_TRUE_VOID(weakFrame.IsAlive());
      }
      mType = eMenuType_Normal;
      break;
  }
  UpdateMenuSpecialState();
}

/* update checked-ness for type="checkbox" and type="radio" */
void nsMenuFrame::UpdateMenuSpecialState() {
  bool newChecked = mContent->AsElement()->AttrValueIs(
      kNameSpaceID_None, nsGkAtoms::checked, nsGkAtoms::_true, eCaseMatters);
  if (newChecked == mChecked) {
    /* checked state didn't change */

    if (mType != eMenuType_Radio)
      return;  // only Radio possibly cares about other kinds of change

    if (!mChecked || mGroupName.IsEmpty()) return;  // no interesting change
  } else {
    mChecked = newChecked;
    if (mType != eMenuType_Radio || !mChecked)
      /*
       * Unchecking something requires no further changes, and only
       * menuRadio has to do additional work when checked.
       */
      return;
  }

  /*
   * If we get this far, we're type=radio, and:
   * - our name= changed, or
   * - we went from checked="false" to checked="true"
   */

  /*
   * Behavioural note:
   * If we're checked and renamed _into_ an existing radio group, we are
   * made the new checked item, and we unselect the previous one.
   *
   * The only other reasonable behaviour would be to check for another selected
   * item in that group.  If found, unselect ourselves, otherwise we're the
   * selected item.  That, however, would be a lot more work, and I don't think
   * it's better at all.
   */

  /* walk siblings, looking for the other checked item with the same name */
  // get the first sibling in this menu popup. This frame may be it, and if
  // we're being called at creation time, this frame isn't yet in the parent's
  // child list. All I'm saying is that this may fail, but it's most likely
  // alright.
  nsIFrame* firstMenuItem =
      nsXULPopupManager::GetNextMenuItem(GetParent(), nullptr, true, false);
  nsIFrame* sib = firstMenuItem;
  while (sib) {
    nsMenuFrame* menu = do_QueryFrame(sib);
    if (sib != this) {
      if (menu && menu->GetMenuType() == eMenuType_Radio && menu->IsChecked() &&
          menu->GetRadioGroupName() == mGroupName) {
        /* uncheck the old item */
        sib->GetContent()->AsElement()->UnsetAttr(kNameSpaceID_None,
                                                  nsGkAtoms::checked, true);
        // XXX in DEBUG, check to make sure that there aren't two checked items
        return;
      }
    }
    sib = nsXULPopupManager::GetNextMenuItem(GetParent(), menu, true, true);
    if (sib == firstMenuItem) {
      break;
    }
  }
}

void nsMenuFrame::Execute(WidgetGUIEvent* aEvent) {
  nsCOMPtr<nsISound> sound(do_CreateInstance("@mozilla.org/sound;1"));
  if (sound) sound->PlayEventSound(nsISound::EVENT_MENU_EXECUTE);

  // Create a trusted event if the triggering event was trusted, or if
  // we're called from chrome code (since at least one of our caller
  // passes in a null event).
  bool isTrusted =
      aEvent ? aEvent->IsTrusted() : nsContentUtils::IsCallerChrome();

  mozilla::Modifiers modifiers = 0;
  WidgetInputEvent* inputEvent = aEvent ? aEvent->AsInputEvent() : nullptr;
  if (inputEvent) {
    modifiers = inputEvent->mModifiers;
  }

  int16_t button = 0;
  WidgetMouseEventBase* mouseEvent =
      aEvent ? aEvent->AsMouseEventBase() : nullptr;
  if (mouseEvent) {
    button = mouseEvent->mButton;
  }

  StopBlinking();
  CreateMenuCommandEvent(isTrusted, modifiers, button);
  StartBlinking();
}

void nsMenuFrame::ActivateItem(Modifiers aModifiers, int16_t aButton) {
  StopBlinking();
  CreateMenuCommandEvent(nsContentUtils::IsCallerChrome(), aModifiers, aButton);
  StartBlinking();
}

bool nsMenuFrame::ShouldBlink() {
  int32_t shouldBlink =
      LookAndFeel::GetInt(LookAndFeel::IntID::ChosenMenuItemsShouldBlink, 0);
  if (!shouldBlink) return false;

  return true;
}

void nsMenuFrame::StartBlinking() {
  if (!ShouldBlink()) {
    PassMenuCommandEventToPopupManager();
    return;
  }

  // Blink off.
  AutoWeakFrame weakFrame(this);
  mContent->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menuactive,
                                   true);
  if (!weakFrame.IsAlive()) return;

  nsMenuParent* menuParent = GetMenuParent();
  if (menuParent) {
    // Make this menu ignore events from now on.
    menuParent->LockMenuUntilClosed(true);
  }

  // Set up a timer to blink back on.
  NS_NewTimerWithCallback(
      getter_AddRefs(mBlinkTimer), mTimerMediator, kBlinkDelay,
      nsITimer::TYPE_ONE_SHOT,
      mContent->OwnerDoc()->EventTargetFor(TaskCategory::Other));
  mBlinkState = 1;
}

void nsMenuFrame::StopBlinking() {
  mBlinkState = 0;
  if (mBlinkTimer) {
    mBlinkTimer->Cancel();
    mBlinkTimer = nullptr;
  }
  mDelayedMenuCommandEvent = nullptr;
}

void nsMenuFrame::CreateMenuCommandEvent(bool aIsTrusted,
                                         mozilla::Modifiers aModifiers,
                                         int16_t aButton) {
  // Because the command event is firing asynchronously, a flag is needed to
  // indicate whether user input is being handled. This ensures that a popup
  // window won't get blocked.
  bool userinput = dom::UserActivation::IsHandlingUserInput();

  // Flip "checked" state if we're a checkbox menu, or an un-checked radio menu.
  bool needToFlipChecked = false;
  if (mType == eMenuType_Checkbox || (mType == eMenuType_Radio && !mChecked)) {
    needToFlipChecked = !mContent->AsElement()->AttrValueIs(
        kNameSpaceID_None, nsGkAtoms::autocheck, nsGkAtoms::_false,
        eCaseMatters);
  }

  mDelayedMenuCommandEvent =
      new nsXULMenuCommandEvent(mContent->AsElement(), aIsTrusted, aModifiers,
                                userinput, needToFlipChecked, aButton);
}

void nsMenuFrame::PassMenuCommandEventToPopupManager() {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  nsMenuParent* menuParent = GetMenuParent();
  if (pm && menuParent && mDelayedMenuCommandEvent) {
    nsCOMPtr<nsIContent> content = mContent;
    RefPtr<nsXULMenuCommandEvent> event = mDelayedMenuCommandEvent;
    pm->ExecuteMenu(content, event);
  }
  mDelayedMenuCommandEvent = nullptr;
}

void nsMenuFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  nsFrameList* popupList = GetPopupList();
  if (popupList && popupList->FirstChild() == aOldFrame) {
    popupList->RemoveFirstChild();
    aOldFrame->Destroy();
    DestroyPopupList();
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);
    return;
  }
  nsBoxFrame::RemoveFrame(aListID, aOldFrame);
}

void nsMenuFrame::InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                               const nsLineList::iterator* aPrevFrameLine,
                               nsFrameList& aFrameList) {
  if (!HasPopup() && (aListID == kPrincipalList || aListID == kPopupList)) {
    SetPopupFrame(aFrameList);
    if (HasPopup()) {
      PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                    NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }

  if (aFrameList.IsEmpty()) return;

  if (MOZ_UNLIKELY(aPrevFrame && aPrevFrame == GetPopup())) {
    aPrevFrame = nullptr;
  }

  nsBoxFrame::InsertFrames(aListID, aPrevFrame, aPrevFrameLine, aFrameList);
}

void nsMenuFrame::AppendFrames(ChildListID aListID, nsFrameList& aFrameList) {
  if (!HasPopup() && (aListID == kPrincipalList || aListID == kPopupList)) {
    SetPopupFrame(aFrameList);
    if (HasPopup()) {
      PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                    NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }

  if (aFrameList.IsEmpty()) return;

  nsBoxFrame::AppendFrames(aListID, aFrameList);
}

bool nsMenuFrame::SizeToPopup(nsBoxLayoutState& aState, nsSize& aSize) {
  if (!IsXULCollapsed()) {
    bool widthSet, heightSet;
    nsSize tmpSize(-1, 0);
    nsIFrame::AddXULPrefSize(this, tmpSize, widthSet, heightSet);
    if (!widthSet && GetXULFlex() == 0) {
      nsMenuPopupFrame* popupFrame = GetPopup();
      if (!popupFrame) return false;
      tmpSize = popupFrame->GetXULPrefSize(aState);

      // Produce a size such that:
      //  (1) the menu and its popup can be the same width
      //  (2) there's enough room in the menu for the content and its
      //      border-padding
      //  (3) there's enough room in the popup for the content and its
      //      scrollbar
      nsMargin borderPadding;
      GetXULBorderAndPadding(borderPadding);

      // if there is a scroll frame, add the desired width of the scrollbar as
      // well
      nsIScrollableFrame* scrollFrame = popupFrame->GetScrollFrame(popupFrame);
      nscoord scrollbarWidth = 0;
      if (scrollFrame) {
        scrollbarWidth =
            scrollFrame->GetDesiredScrollbarSizes(&aState).LeftRight();
      }

      aSize.width =
          tmpSize.width + std::max(borderPadding.LeftRight(), scrollbarWidth);

      return true;
    }
  }

  return false;
}

nsSize nsMenuFrame::GetXULPrefSize(nsBoxLayoutState& aState) {
  nsSize size = nsBoxFrame::GetXULPrefSize(aState);
  DISPLAY_PREF_SIZE(this, size);

  // If we are using sizetopopup="always" then
  // nsBoxFrame will already have enforced the minimum size
  if (!IsSizedToPopup(mContent, true) && IsSizedToPopup(mContent, false) &&
      SizeToPopup(aState, size)) {
    // We now need to ensure that size is within the min - max range.
    nsSize minSize = nsBoxFrame::GetXULMinSize(aState);
    nsSize maxSize = GetXULMaxSize(aState);
    size = XULBoundsCheck(minSize, size, maxSize);
  }

  return size;
}

NS_IMETHODIMP
nsMenuFrame::GetActiveChild(dom::Element** aResult) {
  nsMenuPopupFrame* popupFrame = GetPopup();
  if (!popupFrame) return NS_ERROR_FAILURE;

  nsMenuFrame* menuFrame = popupFrame->GetCurrentMenuItem();
  if (!menuFrame) {
    *aResult = nullptr;
  } else {
    RefPtr<dom::Element> elt = menuFrame->GetContent()->AsElement();
    elt.forget(aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::SetActiveChild(dom::Element* aChild) {
  nsMenuPopupFrame* popupFrame = GetPopup();
  if (!popupFrame) return NS_ERROR_FAILURE;

  // Force the child frames within the popup to be generated.
  AutoWeakFrame weakFrame(popupFrame);
  popupFrame->GenerateFrames();
  if (!weakFrame.IsAlive()) {
    return NS_OK;
  }

  if (!aChild) {
    // Remove the current selection
    popupFrame->ChangeMenuItem(nullptr, false, false);
    return NS_OK;
  }

  nsMenuFrame* menu = do_QueryFrame(aChild->GetPrimaryFrame());
  if (menu) popupFrame->ChangeMenuItem(menu, false, false);
  return NS_OK;
}

nsIScrollableFrame* nsMenuFrame::GetScrollTargetFrame() const {
  nsMenuPopupFrame* popupFrame = GetPopup();
  if (!popupFrame) return nullptr;
  nsIFrame* childFrame = popupFrame->PrincipalChildList().FirstChild();
  if (childFrame) return popupFrame->GetScrollFrame(childFrame);
  return nullptr;
}

// nsMenuTimerMediator implementation.
NS_IMPL_ISUPPORTS(nsMenuTimerMediator, nsITimerCallback)

/**
 * Constructs a wrapper around an nsMenuFrame.
 * @param aFrame nsMenuFrame to create a wrapper around.
 */
nsMenuTimerMediator::nsMenuTimerMediator(nsMenuFrame* aFrame) : mFrame(aFrame) {
  NS_ASSERTION(mFrame, "Must have frame");
}

nsMenuTimerMediator::~nsMenuTimerMediator() = default;

/**
 * Delegates the notification to the contained frame if it has not been
 * destroyed.
 * @param aTimer Timer which initiated the callback.
 * @return NS_ERROR_FAILURE if the frame has been destroyed.
 */
NS_IMETHODIMP nsMenuTimerMediator::Notify(nsITimer* aTimer) {
  if (!mFrame) return NS_ERROR_FAILURE;

  return mFrame->Notify(aTimer);
}

/**
 * Clear the pointer to the contained nsMenuFrame. This should be called
 * when the contained nsMenuFrame is destroyed.
 */
void nsMenuTimerMediator::ClearFrame() { mFrame = nullptr; }

/**
 * Get the name of this timer callback.
 * @param aName the name to return
 */
NS_IMETHODIMP
nsMenuTimerMediator::GetName(nsACString& aName) {
  aName.AssignLiteral("nsMenuTimerMediator");
  return NS_OK;
}

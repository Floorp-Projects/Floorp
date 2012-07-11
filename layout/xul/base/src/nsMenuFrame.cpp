/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsMenuFrame.h"
#include "nsBoxFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsMenuPopupFrame.h"
#include "nsMenuBarFrame.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIComponentManager.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsBindingManager.h"
#include "nsIServiceManager.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDOMKeyEvent.h"
#include "nsEventDispatcher.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIStringBundle.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsIReflowCallback.h"
#include "nsISound.h"
#include "nsEventStateManager.h"
#include "nsIDOMXULMenuListElement.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Attributes.h"

using namespace mozilla;

#define NS_MENU_POPUP_LIST_INDEX 0

#if defined(XP_WIN) || defined(XP_OS2)
#define NSCONTEXTMENUISMOUSEUP 1
#endif

static void
AssertNotCalled(void* aPropertyValue)
{
  NS_ERROR("popup list should never be destroyed by the FramePropertyTable");
}
NS_DECLARE_FRAME_PROPERTY(PopupListProperty, AssertNotCalled)

static PRInt32 gEatMouseMove = false;

const PRInt32 kBlinkDelay = 67; // milliseconds

// this class is used for dispatching menu activation events asynchronously.
class nsMenuActivateEvent : public nsRunnable
{
public:
  nsMenuActivateEvent(nsIContent *aMenu,
                      nsPresContext* aPresContext,
                      bool aIsActivate)
    : mMenu(aMenu), mPresContext(aPresContext), mIsActivate(aIsActivate)
  {
  }

  NS_IMETHOD Run()
  {
    nsAutoString domEventToFire;

    if (mIsActivate) {
      // Highlight the menu.
      mMenu->SetAttr(kNameSpaceID_None, nsGkAtoms::menuactive,
                     NS_LITERAL_STRING("true"), true);
      // The menuactivated event is used by accessibility to track the user's
      // movements through menus
      domEventToFire.AssignLiteral("DOMMenuItemActive");
    }
    else {
      // Unhighlight the menu.
      mMenu->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menuactive, true);
      domEventToFire.AssignLiteral("DOMMenuItemInactive");
    }

    nsCOMPtr<nsIDOMEvent> event;
    if (NS_SUCCEEDED(nsEventDispatcher::CreateEvent(mPresContext, nsnull,
                                                    NS_LITERAL_STRING("Events"),
                                                    getter_AddRefs(event)))) {
      event->InitEvent(domEventToFire, true, true);

      event->SetTrusted(true);

      nsEventDispatcher::DispatchDOMEvent(mMenu, nsnull, event,
                                          mPresContext, nsnull);
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsIContent> mMenu;
  nsRefPtr<nsPresContext> mPresContext;
  bool mIsActivate;
};

class nsMenuAttributeChangedEvent : public nsRunnable
{
public:
  nsMenuAttributeChangedEvent(nsIFrame* aFrame, nsIAtom* aAttr)
  : mFrame(aFrame), mAttr(aAttr)
  {
  }

  NS_IMETHOD Run()
  {
    nsMenuFrame* frame = static_cast<nsMenuFrame*>(mFrame.GetFrame());
    NS_ENSURE_STATE(frame);
    if (mAttr == nsGkAtoms::checked) {
      frame->UpdateMenuSpecialState(frame->PresContext());
    } else if (mAttr == nsGkAtoms::acceltext) {
      // someone reset the accelText attribute,
      // so clear the bit that says *we* set it
      frame->RemoveStateBits(NS_STATE_ACCELTEXT_IS_DERIVED);
      frame->BuildAcceleratorText(true);
    }
    else if (mAttr == nsGkAtoms::key) {
      frame->BuildAcceleratorText(true);
    } else if (mAttr == nsGkAtoms::type || mAttr == nsGkAtoms::name) {
      frame->UpdateMenuType(frame->PresContext());
    }
    return NS_OK;
  }
protected:
  nsWeakFrame       mFrame;
  nsCOMPtr<nsIAtom> mAttr;
};

//
// NS_NewMenuFrame and NS_NewMenuItemFrame
//
// Wrappers for creating a new menu popup container
//
nsIFrame*
NS_NewMenuFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  nsMenuFrame* it = new (aPresShell) nsMenuFrame (aPresShell, aContext);
  
  if (it)
    it->SetIsMenu(true);

  return it;
}

nsIFrame*
NS_NewMenuItemFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  nsMenuFrame* it = new (aPresShell) nsMenuFrame (aPresShell, aContext);

  if (it)
    it->SetIsMenu(false);

  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsMenuFrame)

NS_QUERYFRAME_HEAD(nsMenuFrame)
  NS_QUERYFRAME_ENTRY(nsMenuFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

nsMenuFrame::nsMenuFrame(nsIPresShell* aShell, nsStyleContext* aContext):
  nsBoxFrame(aShell, aContext),
    mIsMenu(false),
    mChecked(false),
    mIgnoreAccelTextChange(false),
    mType(eMenuType_Normal),
    mMenuParent(nsnull),
    mBlinkState(0)
{
}

void
nsMenuFrame::SetParent(nsIFrame* aParent)
{
  nsBoxFrame::SetParent(aParent);
  InitMenuParent(aParent);
}

void
nsMenuFrame::InitMenuParent(nsIFrame* aParent)
{
  while (aParent) {
    nsIAtom* type = aParent->GetType();
    if (type == nsGkAtoms::menuPopupFrame) {
      mMenuParent = static_cast<nsMenuPopupFrame *>(aParent);
      break;
    }
    else if (type == nsGkAtoms::menuBarFrame) {
      mMenuParent = static_cast<nsMenuBarFrame *>(aParent);
      break;
    }
    aParent = aParent->GetParent();
  }
}

class nsASyncMenuInitialization MOZ_FINAL : public nsIReflowCallback
{
public:
  nsASyncMenuInitialization(nsIFrame* aFrame)
    : mWeakFrame(aFrame)
  {
  }

  virtual bool ReflowFinished()
  {
    bool shouldFlush = false;
    if (mWeakFrame.IsAlive()) {
      if (mWeakFrame.GetFrame()->GetType() == nsGkAtoms::menuFrame) {
        nsMenuFrame* menu = static_cast<nsMenuFrame*>(mWeakFrame.GetFrame());
        menu->UpdateMenuType(menu->PresContext());
        shouldFlush = true;
      }
    }
    delete this;
    return shouldFlush;
  }

  virtual void ReflowCallbackCanceled()
  {
    delete this;
  }

  nsWeakFrame mWeakFrame;
};

NS_IMETHODIMP
nsMenuFrame::Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Set up a mediator which can be used for callbacks on this frame.
  mTimerMediator = new nsMenuTimerMediator(this);
  if (NS_UNLIKELY(!mTimerMediator))
    return NS_ERROR_OUT_OF_MEMORY;

  InitMenuParent(aParent);

  BuildAcceleratorText(false);
  nsIReflowCallback* cb = new nsASyncMenuInitialization(this);
  NS_ENSURE_TRUE(cb, NS_ERROR_OUT_OF_MEMORY);
  PresContext()->PresShell()->PostReflowCallback(cb);
  return rv;
}

const nsFrameList&
nsMenuFrame::GetChildList(ChildListID aListID) const
{
  if (kPopupList == aListID) {
    nsFrameList* list = GetPopupList();
    return list ? *list : nsFrameList::EmptyList();
  }
  return nsBoxFrame::GetChildList(aListID);
}

void
nsMenuFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  nsBoxFrame::GetChildLists(aLists);
  nsFrameList* list = GetPopupList();
  if (list) {
    list->AppendIfNonempty(aLists, kPopupList);
  }
}

nsMenuPopupFrame*
nsMenuFrame::GetPopup()
{
  nsFrameList* popupList = GetPopupList();
  return popupList ? static_cast<nsMenuPopupFrame*>(popupList->FirstChild()) :
                     nsnull;
}

nsFrameList*
nsMenuFrame::GetPopupList() const
{
  if (!HasPopup()) {
    return nsnull;
  }
  nsFrameList* prop =
    static_cast<nsFrameList*>(Properties().Get(PopupListProperty()));
  NS_ASSERTION(prop && prop->GetLength() == 1 &&
               prop->FirstChild()->GetType() == nsGkAtoms::menuPopupFrame,
               "popup list should have exactly one nsMenuPopupFrame");
  return prop;
}

void
nsMenuFrame::DestroyPopupList()
{
  NS_ASSERTION(HasPopup(), "huh?");
  nsFrameList* prop =
    static_cast<nsFrameList*>(Properties().Remove(PopupListProperty()));
  NS_ASSERTION(prop && prop->IsEmpty(),
               "popup list must exist and be empty when destroying");
  RemoveStateBits(NS_STATE_MENU_HAS_POPUP_LIST);
  delete prop;
}

void
nsMenuFrame::SetPopupFrame(nsFrameList& aFrameList)
{
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    if (e.get()->GetType() == nsGkAtoms::menuPopupFrame) {
      // Remove the frame from the list and store it in a nsFrameList* property.
      nsIFrame* popupFrame = e.get();
      aFrameList.RemoveFrame(popupFrame);
      nsFrameList* popupList = new nsFrameList(popupFrame, popupFrame);
      Properties().Set(PopupListProperty(), popupList);
      AddStateBits(NS_STATE_MENU_HAS_POPUP_LIST);
      break;
    }
  }
}

NS_IMETHODIMP
nsMenuFrame::SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList)
{
  NS_ASSERTION(!HasPopup(), "SetInitialChildList called twice?");
  if (aListID == kPrincipalList || aListID == kPopupList) {
    SetPopupFrame(aChildList);
  }
  return nsBoxFrame::SetInitialChildList(aListID, aChildList);
}

void
nsMenuFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
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
  mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menuactive, false);

  // are we our menu parent's current menu item?
  if (mMenuParent && mMenuParent->GetCurrentMenuItem() == this) {
    // yes; tell it that we're going away
    mMenuParent->CurrentMenuIsBeingDestroyed();
  }

  nsFrameList* popupList = GetPopupList();
  if (popupList) {
    popupList->DestroyFramesFrom(aDestructRoot);
    DestroyPopupList();
  }

  nsBoxFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
nsMenuFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists)
{
  if (!aBuilder->IsForEventDelivery())
    return nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
    
  nsDisplayListCollection set;
  nsresult rv = nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return WrapListsInRedirector(aBuilder, set, aLists);
}

NS_IMETHODIMP
nsMenuFrame::HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus ||
      (mMenuParent && mMenuParent->IsMenuLocked())) {
    return NS_OK;
  }

  nsWeakFrame weakFrame(this);
  if (*aEventStatus == nsEventStatus_eIgnore)
    *aEventStatus = nsEventStatus_eConsumeDoDefault;

  bool onmenu = IsOnMenu();

  if (aEvent->message == NS_KEY_PRESS && !IsDisabled()) {
    nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
    PRUint32 keyCode = keyEvent->keyCode;
#ifdef XP_MACOSX
    // On mac, open menulist on either up/down arrow or space (w/o Cmd pressed)
    if (!IsOpen() && ((keyEvent->charCode == NS_VK_SPACE && !keyEvent->IsMeta()) ||
        (keyCode == NS_VK_UP || keyCode == NS_VK_DOWN))) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
      OpenMenu(false);
    }
#else
    // On other platforms, toggle menulist on unmodified F4 or Alt arrow
    if ((keyCode == NS_VK_F4 && !keyEvent->IsAlt()) ||
        ((keyCode == NS_VK_UP || keyCode == NS_VK_DOWN) && keyEvent->IsAlt())) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
      ToggleMenuState();
    }
#endif
  }
  else if (aEvent->eventStructType == NS_MOUSE_EVENT &&
           aEvent->message == NS_MOUSE_BUTTON_DOWN &&
           static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton &&
           !IsDisabled() && IsMenu()) {
    // The menu item was selected. Bring up the menu.
    // We have children.
    // Don't prevent the default action here, since that will also cancel
    // potential drag starts.
    if (!mMenuParent || mMenuParent->IsMenuBar()) {
      ToggleMenuState();
    }
    else {
      if (!IsOpen()) {
        OpenMenu(false);
      }
    }
  }
  else if (
#ifndef NSCONTEXTMENUISMOUSEUP
           (aEvent->eventStructType == NS_MOUSE_EVENT &&
            aEvent->message == NS_MOUSE_BUTTON_UP &&
            static_cast<nsMouseEvent*>(aEvent)->button ==
              nsMouseEvent::eRightButton) &&
#else
            aEvent->message == NS_CONTEXTMENU &&
#endif
            onmenu && !IsMenu() && !IsDisabled()) {
    // if this menu is a context menu it accepts right-clicks...fire away!
    // Make sure we cancel default processing of the context menu event so
    // that it doesn't bubble and get seen again by the popuplistener and show
    // another context menu.
    //
    // Furthermore (there's always more, isn't there?), on some platforms (win32
    // being one of them) we get the context menu event on a mouse up while
    // on others we get it on a mouse down. For the ones where we get it on a
    // mouse down, we must continue listening for the right button up event to
    // dismiss the menu.
    if (mMenuParent->IsContextMenu()) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
      Execute(aEvent);
    }
  }
  else if (aEvent->eventStructType == NS_MOUSE_EVENT &&
           aEvent->message == NS_MOUSE_BUTTON_UP &&
           static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton &&
           !IsMenu() && !IsDisabled()) {
    // Execute the execute event handler.
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
    Execute(aEvent);
  }
  else if (aEvent->message == NS_MOUSE_EXIT_SYNTH) {
    // Kill our timer if one is active.
    if (mOpenTimer) {
      mOpenTimer->Cancel();
      mOpenTimer = nsnull;
    }

    // Deactivate the menu.
    if (mMenuParent) {
      bool onmenubar = mMenuParent->IsMenuBar();
      if (!(onmenubar && mMenuParent->IsActive())) {
        if (IsMenu() && !onmenubar && IsOpen()) {
          // Submenus don't get closed up immediately.
        }
        else if (this == mMenuParent->GetCurrentMenuItem()) {
          mMenuParent->ChangeMenuItem(nsnull, false);
        }
      }
    }
  }
  else if (aEvent->message == NS_MOUSE_MOVE &&
           (onmenu || (mMenuParent && mMenuParent->IsMenuBar()))) {
    if (gEatMouseMove) {
      gEatMouseMove = false;
      return NS_OK;
    }

    // Let the menu parent know we're the new item.
    mMenuParent->ChangeMenuItem(this, false);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    NS_ENSURE_TRUE(mMenuParent, NS_OK);

    // we need to check if we really became the current menu
    // item or not
    nsMenuFrame *realCurrentItem = mMenuParent->GetCurrentMenuItem();
    if (realCurrentItem != this) {
      // we didn't (presumably because a context menu was active)
      return NS_OK;
    }

    // Hovering over a menu in a popup should open it without a need for a click.
    // A timer is used so that it doesn't open if the user moves the mouse quickly
    // past the menu. This conditional check ensures that only menus have this
    // behaviour
    if (!IsDisabled() && IsMenu() && !IsOpen() && !mOpenTimer && !mMenuParent->IsMenuBar()) {
      PRInt32 menuDelay =
        LookAndFeel::GetInt(LookAndFeel::eIntID_SubmenuDelay, 300); // ms

      // We're a menu, we're built, we're closed, and no timer has been kicked off.
      mOpenTimer = do_CreateInstance("@mozilla.org/timer;1");
      mOpenTimer->InitWithCallback(mTimerMediator, menuDelay, nsITimer::TYPE_ONE_SHOT);
    }
  }
  
  return NS_OK;
}

void
nsMenuFrame::ToggleMenuState()
{
  if (IsOpen())
    CloseMenu(false);
  else
    OpenMenu(false);
}

void
nsMenuFrame::PopupOpened()
{
  nsWeakFrame weakFrame(this);
  mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::open,
                    NS_LITERAL_STRING("true"), true);
  if (!weakFrame.IsAlive())
    return;

  if (mMenuParent) {
    mMenuParent->SetActive(true);
    // Make sure the current menu which is being toggled on
    // the menubar is highlighted
    mMenuParent->SetCurrentMenuItem(this);
  }
}

void
nsMenuFrame::PopupClosed(bool aDeselectMenu)
{
  nsWeakFrame weakFrame(this);
  nsContentUtils::AddScriptRunner(
    new nsUnsetAttrRunnable(mContent, nsGkAtoms::open));
  if (!weakFrame.IsAlive())
    return;

  // if the popup is for a menu on a menubar, inform menubar to deactivate
  if (mMenuParent && mMenuParent->MenuClosed()) {
    if (aDeselectMenu) {
      SelectMenu(false);
    } else {
      // We are not deselecting the parent menu while closing the popup, so send
      // a DOMMenuItemActive event to the menu to indicate that the menu is
      // becoming active again.
      nsMenuFrame *current = mMenuParent->GetCurrentMenuItem();
      if (current) {
        nsCOMPtr<nsIRunnable> event =
          new nsMenuActivateEvent(current->GetContent(),
                                  PresContext(), true);
        NS_DispatchToCurrentThread(event);
      }
    }
  }
}

NS_IMETHODIMP
nsMenuFrame::SelectMenu(bool aActivateFlag)
{
  if (mContent) {
    // When a menu opens a submenu, the mouse will often be moved onto a
    // sibling before moving onto an item within the submenu, causing the
    // parent to become deselected. We need to ensure that the parent menu
    // is reselected when an item in the submenu is selected, so navigate up
    // from the item to its popup, and then to the popup above that.
    if (aActivateFlag) {
      nsIFrame* parent = GetParent();
      while (parent) {
        if (parent->GetType() == nsGkAtoms::menuPopupFrame) {
          // a menu is always the direct parent of a menupopup
          parent = parent->GetParent();
          if (parent && parent->GetType() == nsGkAtoms::menuFrame) {
            // a popup however is not necessarily the direct parent of a menu
            nsIFrame* popupParent = parent->GetParent();
            while (popupParent) {
              if (popupParent->GetType() == nsGkAtoms::menuPopupFrame) {
                nsMenuPopupFrame* popup = static_cast<nsMenuPopupFrame *>(popupParent);
                popup->SetCurrentMenuItem(static_cast<nsMenuFrame *>(parent));
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
    if (pm)
      pm->CancelMenuTimer(mMenuParent);

    nsCOMPtr<nsIRunnable> event =
      new nsMenuActivateEvent(mContent, PresContext(), aActivateFlag);
    NS_DispatchToCurrentThread(event);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType)
{
  if (aAttribute == nsGkAtoms::acceltext && mIgnoreAccelTextChange) {
    // Reset the flag so that only one change is ignored.
    mIgnoreAccelTextChange = false;
    return NS_OK;
  }

  if (aAttribute == nsGkAtoms::checked ||
      aAttribute == nsGkAtoms::acceltext ||
      aAttribute == nsGkAtoms::key ||
      aAttribute == nsGkAtoms::type ||
      aAttribute == nsGkAtoms::name) {
    nsCOMPtr<nsIRunnable> event =
      new nsMenuAttributeChangedEvent(this, aAttribute);
    nsContentUtils::AddScriptRunner(event);
  }
  return NS_OK;
}

void
nsMenuFrame::OpenMenu(bool aSelectFirstItem)
{
  if (!mContent)
    return;

  gEatMouseMove = true;

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    pm->KillMenuTimer();
    // This opens the menu asynchronously
    pm->ShowMenu(mContent, aSelectFirstItem, true);
  }
}

void
nsMenuFrame::CloseMenu(bool aDeselectMenu)
{
  gEatMouseMove = true;

  // Close the menu asynchronously
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && HasPopup())
    pm->HidePopup(GetPopup()->GetContent(), false, aDeselectMenu, true);
}

bool
nsMenuFrame::IsSizedToPopup(nsIContent* aContent, bool aRequireAlways)
{
  bool sizeToPopup;
  if (aContent->Tag() == nsGkAtoms::select)
    sizeToPopup = true;
  else {
    nsAutoString sizedToPopup;
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::sizetopopup, sizedToPopup);
    sizeToPopup = sizedToPopup.EqualsLiteral("always") ||
                  (!aRequireAlways && sizedToPopup.EqualsLiteral("pref"));
  }
  
  return sizeToPopup;
}

nsSize
nsMenuFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size = nsBoxFrame::GetMinSize(aBoxLayoutState);
  DISPLAY_MIN_SIZE(this, size);

  if (IsSizedToPopup(mContent, true))
    SizeToPopup(aBoxLayoutState, size);

  return size;
}

NS_IMETHODIMP
nsMenuFrame::DoLayout(nsBoxLayoutState& aState)
{
  // lay us out
  nsresult rv = nsBoxFrame::DoLayout(aState);

  nsMenuPopupFrame* popupFrame = GetPopup();
  if (popupFrame) {
    bool sizeToPopup = IsSizedToPopup(mContent, false);
    popupFrame->LayoutPopup(aState, this, sizeToPopup);
  }

  return rv;
}

#ifdef DEBUG_LAYOUT
NS_IMETHODIMP
nsMenuFrame::SetDebug(nsBoxLayoutState& aState, bool aDebug)
{
  // see if our state matches the given debug state
  bool debugSet = mState & NS_STATE_CURRENTLY_IN_DEBUG;
  bool debugChanged = (!aDebug && debugSet) || (aDebug && !debugSet);

  // if it doesn't then tell each child below us the new debug state
  if (debugChanged)
  {
      nsBoxFrame::SetDebug(aState, aDebug);
      nsMenuPopupFrame* popupFrame = GetPopup();
      if (popupFrame)
        SetDebug(aState, popupFrame, aDebug);
  }

  return NS_OK;
}

nsresult
nsMenuFrame::SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, bool aDebug)
{
      if (!aList)
          return NS_OK;

      while (aList) {
        if (aList->IsBoxFrame())
          aList->SetDebug(aState, aDebug);

        aList = aList->GetNextSibling();
      }

      return NS_OK;
}
#endif

//
// Enter
//
// Called when the user hits the <Enter>/<Return> keys or presses the
// shortcut key. If this is a leaf item, the item's action will be executed.
// In either case, do nothing if the item is disabled.
//
nsMenuFrame*
nsMenuFrame::Enter(nsGUIEvent *aEvent)
{
  if (IsDisabled()) {
#ifdef XP_WIN
    // behavior on Windows - close the popup chain
    if (mMenuParent) {
      nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
      if (pm) {
        nsIFrame* popup = pm->GetTopPopup(ePopupTypeAny);
        if (popup)
          pm->HidePopup(popup->GetContent(), true, true, true);
      }
    }
#endif   // #ifdef XP_WIN
    // this menu item was disabled - exit
    return nsnull;
  }

  if (!IsOpen()) {
    // The enter key press applies to us.
    if (!IsMenu() && mMenuParent)
      Execute(aEvent);          // Execute our event handler
    else
      return this;
  }

  return nsnull;
}

bool
nsMenuFrame::IsOpen()
{
  nsMenuPopupFrame* popupFrame = GetPopup();
  return popupFrame && popupFrame->IsOpen();
}

bool
nsMenuFrame::IsMenu()
{
  return mIsMenu;
}

nsMenuListType
nsMenuFrame::GetParentMenuListType()
{
  if (mMenuParent && mMenuParent->IsMenu()) {
    nsMenuPopupFrame* popupFrame = static_cast<nsMenuPopupFrame*>(mMenuParent);
    nsIFrame* parentMenu = popupFrame->GetParent();
    if (parentMenu) {
      nsCOMPtr<nsIDOMXULMenuListElement> menulist = do_QueryInterface(parentMenu->GetContent());
      if (menulist) {
        bool isEditable = false;
        menulist->GetEditable(&isEditable);
        return isEditable ? eEditableMenuList : eReadonlyMenuList;
      }
    }
  }
  return eNotMenuList;
}

nsresult
nsMenuFrame::Notify(nsITimer* aTimer)
{
  // Our timer has fired.
  if (aTimer == mOpenTimer.get()) {
    mOpenTimer = nsnull;

    if (!IsOpen() && mMenuParent) {
      // make sure we didn't open a context menu in the meantime
      // (i.e. the user right-clicked while hovering over a submenu).
      nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
      if (pm) {
        if ((!pm->HasContextMenu(nsnull) || mMenuParent->IsContextMenu()) &&
            mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menuactive,
                                  nsGkAtoms::_true, eCaseMatters)) {
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
      case 1:
        {
          // Turn the highlight back on and wait for a while before closing the menu.
          nsWeakFrame weakFrame(this);
          mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::menuactive,
                            NS_LITERAL_STRING("true"), true);
          if (weakFrame.IsAlive()) {
            aTimer->InitWithCallback(mTimerMediator, kBlinkDelay, nsITimer::TYPE_ONE_SHOT);
          }
        }
        break;
      default:
        if (mMenuParent) {
          mMenuParent->LockMenuUntilClosed(false);
        }
        PassMenuCommandEventToPopupManager();
        StopBlinking();
        break;
    }
  }

  return NS_OK;
}

bool 
nsMenuFrame::IsDisabled()
{
  return mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                               nsGkAtoms::_true, eCaseMatters);
}

void
nsMenuFrame::UpdateMenuType(nsPresContext* aPresContext)
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::checkbox, &nsGkAtoms::radio, nsnull};
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                    strings, eCaseMatters)) {
    case 0: mType = eMenuType_Checkbox; break;
    case 1:
      mType = eMenuType_Radio;
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, mGroupName);
      break;

    default:
      if (mType != eMenuType_Normal) {
        nsWeakFrame weakFrame(this);
        mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                            true);
        ENSURE_TRUE(weakFrame.IsAlive());
      }
      mType = eMenuType_Normal;
      break;
  }
  UpdateMenuSpecialState(aPresContext);
}

/* update checked-ness for type="checkbox" and type="radio" */
void
nsMenuFrame::UpdateMenuSpecialState(nsPresContext* aPresContext)
{
  bool newChecked =
    mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                          nsGkAtoms::_true, eCaseMatters); 
  if (newChecked == mChecked) {
    /* checked state didn't change */

    if (mType != eMenuType_Radio)
      return; // only Radio possibly cares about other kinds of change

    if (!mChecked || mGroupName.IsEmpty())
      return;                   // no interesting change
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
  // get the first sibling in this menu popup. This frame may be it, and if we're
  // being called at creation time, this frame isn't yet in the parent's child list.
  // All I'm saying is that this may fail, but it's most likely alright.
  nsIFrame* sib = GetParent()->GetFirstPrincipalChild();

  while (sib) {
    if (sib != this && sib->GetType() == nsGkAtoms::menuFrame) {
      nsMenuFrame* menu = static_cast<nsMenuFrame*>(sib);
      if (menu->GetMenuType() == eMenuType_Radio &&
          menu->IsChecked() &&
          (menu->GetRadioGroupName() == mGroupName)) {      
        /* uncheck the old item */
        sib->GetContent()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                                     true);
        /* XXX in DEBUG, check to make sure that there aren't two checked items */
        return;
      }
    }

    sib = sib->GetNextSibling();
  } 
}

void 
nsMenuFrame::BuildAcceleratorText(bool aNotify)
{
  nsAutoString accelText;

  if ((GetStateBits() & NS_STATE_ACCELTEXT_IS_DERIVED) == 0) {
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::acceltext, accelText);
    if (!accelText.IsEmpty())
      return;
  }
  // accelText is definitely empty here.

  // Now we're going to compute the accelerator text, so remember that we did.
  AddStateBits(NS_STATE_ACCELTEXT_IS_DERIVED);

  // If anything below fails, just leave the accelerator text blank.
  nsWeakFrame weakFrame(this);
  mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::acceltext, aNotify);
  ENSURE_TRUE(weakFrame.IsAlive());

  // See if we have a key node and use that instead.
  nsAutoString keyValue;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyValue);
  if (keyValue.IsEmpty())
    return;

  // Turn the document into a DOM document so we can use getElementById
  nsIDocument *document = mContent->GetDocument();
  if (!document)
    return;

  nsIContent *keyElement = document->GetElementById(keyValue);
  if (!keyElement) {
#ifdef DEBUG
    nsAutoString label;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, label);
    nsAutoString msg = NS_LITERAL_STRING("Key '") +
                       keyValue +
                       NS_LITERAL_STRING("' of menu item '") +
                       label +
                       NS_LITERAL_STRING("' could not be found");
    NS_WARNING(NS_ConvertUTF16toUTF8(msg).get());
#endif
    return;
  }

  // get the string to display as accelerator text
  // check the key element's attributes in this order:
  // |keytext|, |key|, |keycode|
  nsAutoString accelString;
  keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::keytext, accelString);

  if (accelString.IsEmpty()) {
    keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::key, accelString);

    if (!accelString.IsEmpty()) {
      ToUpperCase(accelString);
    } else {
      nsAutoString keyCode;
      keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, keyCode);
      ToUpperCase(keyCode);

      nsresult rv;
      nsCOMPtr<nsIStringBundleService> bundleService =
        mozilla::services::GetStringBundleService();
      if (bundleService) {
        nsCOMPtr<nsIStringBundle> bundle;
        rv = bundleService->CreateBundle("chrome://global/locale/keys.properties",
                                         getter_AddRefs(bundle));

        if (NS_SUCCEEDED(rv) && bundle) {
          nsXPIDLString keyName;
          rv = bundle->GetStringFromName(keyCode.get(), getter_Copies(keyName));
          if (keyName)
            accelString = keyName;
        }
      }

      // nothing usable found, bail
      if (accelString.IsEmpty())
        return;
    }
  }

  static PRInt32 accelKey = 0;

  if (!accelKey)
  {
    // Compiled-in defaults, in case we can't get LookAndFeel --
    // command for mac, control for all other platforms.
#ifdef XP_MACOSX
    accelKey = nsIDOMKeyEvent::DOM_VK_META;
#else
    accelKey = nsIDOMKeyEvent::DOM_VK_CONTROL;
#endif

    // Get the accelerator key value from prefs, overriding the default:
    accelKey = Preferences::GetInt("ui.key.accelKey", accelKey);
  }

  nsAutoString modifiers;
  keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::modifiers, modifiers);
  
  char* str = ToNewCString(modifiers);
  char* newStr;
  char* token = nsCRT::strtok(str, ", \t", &newStr);

  nsAutoString shiftText;
  nsAutoString altText;
  nsAutoString metaText;
  nsAutoString controlText;
  nsAutoString modifierSeparator;

  nsContentUtils::GetShiftText(shiftText);
  nsContentUtils::GetAltText(altText);
  nsContentUtils::GetMetaText(metaText);
  nsContentUtils::GetControlText(controlText);
  nsContentUtils::GetModifierSeparatorText(modifierSeparator);

  while (token) {
      
    if (PL_strcmp(token, "shift") == 0)
      accelText += shiftText;
    else if (PL_strcmp(token, "alt") == 0) 
      accelText += altText; 
    else if (PL_strcmp(token, "meta") == 0) 
      accelText += metaText; 
    else if (PL_strcmp(token, "control") == 0) 
      accelText += controlText; 
    else if (PL_strcmp(token, "accel") == 0) {
      switch (accelKey)
      {
        case nsIDOMKeyEvent::DOM_VK_META:
          accelText += metaText;
          break;

        case nsIDOMKeyEvent::DOM_VK_ALT:
          accelText += altText;
          break;

        case nsIDOMKeyEvent::DOM_VK_CONTROL:
        default:
          accelText += controlText;
          break;
      }
    }
    
    accelText += modifierSeparator;

    token = nsCRT::strtok(newStr, ", \t", &newStr);
  }

  nsMemory::Free(str);

  accelText += accelString;

  mIgnoreAccelTextChange = true;
  mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::acceltext, accelText, aNotify);
  ENSURE_TRUE(weakFrame.IsAlive());

  mIgnoreAccelTextChange = false;
}

void
nsMenuFrame::Execute(nsGUIEvent *aEvent)
{
  // flip "checked" state if we're a checkbox menu, or an un-checked radio menu
  bool needToFlipChecked = false;
  if (mType == eMenuType_Checkbox || (mType == eMenuType_Radio && !mChecked)) {
    needToFlipChecked = !mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::autocheck,
                                               nsGkAtoms::_false, eCaseMatters);
  }

  nsCOMPtr<nsISound> sound(do_CreateInstance("@mozilla.org/sound;1"));
  if (sound)
    sound->PlayEventSound(nsISound::EVENT_MENU_EXECUTE);

  StartBlinking(aEvent, needToFlipChecked);
}

bool
nsMenuFrame::ShouldBlink()
{
  PRInt32 shouldBlink =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ChosenMenuItemsShouldBlink, 0);
  if (!shouldBlink)
    return false;

  // Don't blink in editable menulists.
  if (GetParentMenuListType() == eEditableMenuList)
    return false;

  return true;
}

void
nsMenuFrame::StartBlinking(nsGUIEvent *aEvent, bool aFlipChecked)
{
  StopBlinking();
  CreateMenuCommandEvent(aEvent, aFlipChecked);

  if (!ShouldBlink()) {
    PassMenuCommandEventToPopupManager();
    return;
  }

  // Blink off.
  nsWeakFrame weakFrame(this);
  mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menuactive, true);
  if (!weakFrame.IsAlive())
    return;

  if (mMenuParent) {
    // Make this menu ignore events from now on.
    mMenuParent->LockMenuUntilClosed(true);
  }

  // Set up a timer to blink back on.
  mBlinkTimer = do_CreateInstance("@mozilla.org/timer;1");
  mBlinkTimer->InitWithCallback(mTimerMediator, kBlinkDelay, nsITimer::TYPE_ONE_SHOT);
  mBlinkState = 1;
}

void
nsMenuFrame::StopBlinking()
{
  mBlinkState = 0;
  if (mBlinkTimer) {
    mBlinkTimer->Cancel();
    mBlinkTimer = nsnull;
  }
  mDelayedMenuCommandEvent = nsnull;
}

void
nsMenuFrame::CreateMenuCommandEvent(nsGUIEvent *aEvent, bool aFlipChecked)
{
  // Create a trusted event if the triggering event was trusted, or if
  // we're called from chrome code (since at least one of our caller
  // passes in a null event).
  bool isTrusted = aEvent ? NS_IS_TRUSTED_EVENT(aEvent) :
                              nsContentUtils::IsCallerChrome();

  bool shift = false, control = false, alt = false, meta = false;
  if (aEvent && (aEvent->eventStructType == NS_MOUSE_EVENT ||
                 aEvent->eventStructType == NS_KEY_EVENT)) {
    shift = static_cast<nsInputEvent *>(aEvent)->IsShift();
    control = static_cast<nsInputEvent *>(aEvent)->IsControl();
    alt = static_cast<nsInputEvent *>(aEvent)->IsAlt();
    meta = static_cast<nsInputEvent *>(aEvent)->IsMeta();
  }

  // Because the command event is firing asynchronously, a flag is needed to
  // indicate whether user input is being handled. This ensures that a popup
  // window won't get blocked.
  bool userinput = nsEventStateManager::IsHandlingUserInput();

  mDelayedMenuCommandEvent =
    new nsXULMenuCommandEvent(mContent, isTrusted, shift, control, alt, meta,
                              userinput, aFlipChecked);
}

void
nsMenuFrame::PassMenuCommandEventToPopupManager()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mMenuParent && mDelayedMenuCommandEvent) {
    pm->ExecuteMenu(mContent, mDelayedMenuCommandEvent);
  }
  mDelayedMenuCommandEvent = nsnull;
}

NS_IMETHODIMP
nsMenuFrame::RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame)
{
  nsFrameList* popupList = GetPopupList();
  if (popupList && popupList->DestroyFrameIfPresent(aOldFrame)) {
    DestroyPopupList();
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
    return NS_OK;
  }
  return nsBoxFrame::RemoveFrame(aListID, aOldFrame);
}

NS_IMETHODIMP
nsMenuFrame::InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList)
{
  if (!HasPopup() && (aListID == kPrincipalList || aListID == kPopupList)) {
    SetPopupFrame(aFrameList);
    if (HasPopup()) {
#ifdef DEBUG_LAYOUT
      nsBoxLayoutState state(PresContext());
      SetDebug(state, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
#endif

      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }

  if (aFrameList.IsEmpty())
    return NS_OK;

  if (NS_UNLIKELY(aPrevFrame && aPrevFrame == GetPopup())) {
    aPrevFrame = nsnull;
  }

  return nsBoxFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
}

NS_IMETHODIMP
nsMenuFrame::AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList)
{
  if (!HasPopup() && (aListID == kPrincipalList || aListID == kPopupList)) {
    SetPopupFrame(aFrameList);
    if (HasPopup()) {

#ifdef DEBUG_LAYOUT
      nsBoxLayoutState state(PresContext());
      SetDebug(state, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
#endif
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }

  if (aFrameList.IsEmpty())
    return NS_OK;

  return nsBoxFrame::AppendFrames(aListID, aFrameList); 
}

bool
nsMenuFrame::SizeToPopup(nsBoxLayoutState& aState, nsSize& aSize)
{
  if (!IsCollapsed()) {
    bool widthSet, heightSet;
    nsSize tmpSize(-1, 0);
    nsIBox::AddCSSPrefSize(this, tmpSize, widthSet, heightSet);
    if (!widthSet && GetFlex(aState) == 0) {
      nsMenuPopupFrame* popupFrame = GetPopup();
      if (!popupFrame)
        return false;
      tmpSize = popupFrame->GetPrefSize(aState);

      // Produce a size such that:
      //  (1) the menu and its popup can be the same width
      //  (2) there's enough room in the menu for the content and its
      //      border-padding
      //  (3) there's enough room in the popup for the content and its
      //      scrollbar
      nsMargin borderPadding;
      GetBorderAndPadding(borderPadding);

      // if there is a scroll frame, add the desired width of the scrollbar as well
      nsIScrollableFrame* scrollFrame = do_QueryFrame(popupFrame->GetFirstPrincipalChild());
      nscoord scrollbarWidth = 0;
      if (scrollFrame) {
        scrollbarWidth =
          scrollFrame->GetDesiredScrollbarSizes(&aState).LeftRight();
      }

      aSize.width =
        tmpSize.width + NS_MAX(borderPadding.LeftRight(), scrollbarWidth);

      return true;
    }
  }

  return false;
}

nsSize
nsMenuFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize size = nsBoxFrame::GetPrefSize(aState);
  DISPLAY_PREF_SIZE(this, size);

  // If we are using sizetopopup="always" then
  // nsBoxFrame will already have enforced the minimum size
  if (!IsSizedToPopup(mContent, true) &&
      IsSizedToPopup(mContent, false) &&
      SizeToPopup(aState, size)) {
    // We now need to ensure that size is within the min - max range.
    nsSize minSize = nsBoxFrame::GetMinSize(aState);
    nsSize maxSize = GetMaxSize(aState);
    size = BoundsCheck(minSize, size, maxSize);
  }

  return size;
}

NS_IMETHODIMP
nsMenuFrame::GetActiveChild(nsIDOMElement** aResult)
{
  nsMenuPopupFrame* popupFrame = GetPopup();
  if (!popupFrame)
    return NS_ERROR_FAILURE;

  nsMenuFrame* menuFrame = popupFrame->GetCurrentMenuItem();
  if (!menuFrame) {
    *aResult = nsnull;
  }
  else {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(menuFrame->GetContent()));
    *aResult = elt;
    NS_IF_ADDREF(*aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::SetActiveChild(nsIDOMElement* aChild)
{
  nsMenuPopupFrame* popupFrame = GetPopup();
  if (!popupFrame)
    return NS_ERROR_FAILURE;

  if (!aChild) {
    // Remove the current selection
    popupFrame->ChangeMenuItem(nsnull, false);
    return NS_OK;
  }

  nsCOMPtr<nsIContent> child(do_QueryInterface(aChild));

  nsIFrame* kid = child->GetPrimaryFrame();
  if (kid && kid->GetType() == nsGkAtoms::menuFrame)
    popupFrame->ChangeMenuItem(static_cast<nsMenuFrame *>(kid), false);
  return NS_OK;
}

nsIScrollableFrame* nsMenuFrame::GetScrollTargetFrame()
{
  nsMenuPopupFrame* popupFrame = GetPopup();
  if (!popupFrame)
    return nsnull;
  nsIFrame* childFrame = popupFrame->GetFirstPrincipalChild();
  if (childFrame)
    return popupFrame->GetScrollFrame(childFrame);
  return nsnull;
}

// nsMenuTimerMediator implementation.
NS_IMPL_ISUPPORTS1(nsMenuTimerMediator, nsITimerCallback)

/**
 * Constructs a wrapper around an nsMenuFrame.
 * @param aFrame nsMenuFrame to create a wrapper around.
 */
nsMenuTimerMediator::nsMenuTimerMediator(nsMenuFrame *aFrame) :
  mFrame(aFrame)
{
  NS_ASSERTION(mFrame, "Must have frame");
}

nsMenuTimerMediator::~nsMenuTimerMediator()
{
}

/**
 * Delegates the notification to the contained frame if it has not been destroyed.
 * @param aTimer Timer which initiated the callback.
 * @return NS_ERROR_FAILURE if the frame has been destroyed.
 */
NS_IMETHODIMP nsMenuTimerMediator::Notify(nsITimer* aTimer)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->Notify(aTimer);
}

/**
 * Clear the pointer to the contained nsMenuFrame. This should be called
 * when the contained nsMenuFrame is destroyed.
 */
void nsMenuTimerMediator::ClearFrame()
{
  mFrame = nsnull;
}

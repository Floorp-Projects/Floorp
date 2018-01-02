/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuBarFrame.h"
#include "nsIServiceManager.h"
#include "nsIContent.h"
#include "nsAtom.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsCSSRendering.h"
#include "nsNameSpaceManager.h"
#include "nsIDocument.h"
#include "nsGkAtoms.h"
#include "nsMenuFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsUnicharUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCSSFrameConstructor.h"
#ifdef XP_WIN
#include "nsISound.h"
#include "nsWidgetsCID.h"
#endif
#include "nsContentUtils.h"
#include "nsUTF8Utils.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Event.h"

using namespace mozilla;

//
// NS_NewMenuBarFrame
//
// Wrapper for creating a new menu Bar container
//
nsIFrame*
NS_NewMenuBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMenuBarFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMenuBarFrame)

NS_QUERYFRAME_HEAD(nsMenuBarFrame)
  NS_QUERYFRAME_ENTRY(nsMenuBarFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

//
// nsMenuBarFrame cntr
//
nsMenuBarFrame::nsMenuBarFrame(nsStyleContext* aContext)
  : nsBoxFrame(aContext, kClassID)
  , mStayActive(false)
  , mIsActive(false)
  , mActiveByKeyboard(false)
  , mCurrentMenu(nullptr)
{
} // cntr

void
nsMenuBarFrame::Init(nsIContent*       aContent,
                     nsContainerFrame* aParent,
                     nsIFrame*         aPrevInFlow)
{
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Create the menu bar listener.
  mMenuBarListener = new nsMenuBarListener(this, aContent);
}

NS_IMETHODIMP
nsMenuBarFrame::SetActive(bool aActiveFlag)
{
  // If the activity is not changed, there is nothing to do.
  if (mIsActive == aActiveFlag)
    return NS_OK;

  if (!aActiveFlag) {
    // Don't deactivate when switching between menus on the menubar.
    if (mStayActive)
      return NS_OK;

    // if there is a request to deactivate the menu bar, check to see whether
    // there is a menu popup open for the menu bar. In this case, don't
    // deactivate the menu bar.
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm && pm->IsPopupOpenForMenuParent(this))
      return NS_OK;
  }

  mIsActive = aActiveFlag;
  if (mIsActive) {
    InstallKeyboardNavigator();
  }
  else {
    mActiveByKeyboard = false;
    RemoveKeyboardNavigator();
  }

  NS_NAMED_LITERAL_STRING(active, "DOMMenuBarActive");
  NS_NAMED_LITERAL_STRING(inactive, "DOMMenuBarInactive");

  FireDOMEvent(mIsActive ? active : inactive, mContent);

  return NS_OK;
}

nsMenuFrame*
nsMenuBarFrame::ToggleMenuActiveState()
{
  if (mIsActive) {
    // Deactivate the menu bar
    SetActive(false);
    if (mCurrentMenu) {
      nsMenuFrame* closeframe = mCurrentMenu;
      closeframe->SelectMenu(false);
      mCurrentMenu = nullptr;
      return closeframe;
    }
  }
  else {
    // if the menu bar is already selected (eg. mouseover), deselect it
    if (mCurrentMenu)
      mCurrentMenu->SelectMenu(false);

    // Set the active menu to be the top left item (e.g., the File menu).
    // We use an attribute called "menuactive" to track the current
    // active menu.
    nsMenuFrame* firstFrame = nsXULPopupManager::GetNextMenuItem(this, nullptr, false, false);
    if (firstFrame) {
      // Activate the menu bar
      SetActive(true);
      firstFrame->SelectMenu(true);

      // Track this item for keyboard navigation.
      mCurrentMenu = firstFrame;
    }
  }

  return nullptr;
}

nsMenuFrame*
nsMenuBarFrame::FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent)
{
  uint32_t charCode;
  aKeyEvent->GetCharCode(&charCode);

  AutoTArray<uint32_t, 10> accessKeys;
  WidgetKeyboardEvent* nativeKeyEvent =
    aKeyEvent->AsEvent()->WidgetEventPtr()->AsKeyboardEvent();
  if (nativeKeyEvent) {
    nativeKeyEvent->GetAccessKeyCandidates(accessKeys);
  }
  if (accessKeys.IsEmpty() && charCode)
    accessKeys.AppendElement(charCode);

  if (accessKeys.IsEmpty())
    return nullptr; // no character was pressed so just return

  // Enumerate over our list of frames.
  nsContainerFrame* immediateParent =
    nsXULPopupManager::ImmediateParentFrame(this);

  // Find a most preferred accesskey which should be returned.
  nsIFrame* foundMenu = nullptr;
  size_t foundIndex = accessKeys.NoIndex;
  nsIFrame* currFrame = immediateParent->PrincipalChildList().FirstChild();

  while (currFrame) {
    nsIContent* current = currFrame->GetContent();

    // See if it's a menu item.
    if (nsXULPopupManager::IsValidMenuItem(current, false)) {
      // Get the shortcut attribute.
      nsAutoString shortcutKey;
      if (current->IsElement()) {
        current->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey,
                                      shortcutKey);
      }
      if (!shortcutKey.IsEmpty()) {
        ToLowerCase(shortcutKey);
        const char16_t* start = shortcutKey.BeginReading();
        const char16_t* end = shortcutKey.EndReading();
        uint32_t ch = UTF16CharEnumerator::NextChar(&start, end);
        size_t index = accessKeys.IndexOf(ch);
        if (index != accessKeys.NoIndex &&
            (foundIndex == accessKeys.NoIndex || index < foundIndex)) {
          foundMenu = currFrame;
          foundIndex = index;
        }
      }
    }
    currFrame = currFrame->GetNextSibling();
  }
  if (foundMenu) {
    return do_QueryFrame(foundMenu);
  }

  // didn't find a matching menu item
#ifdef XP_WIN
  // behavior on Windows - this item is on the menu bar, beep and deactivate the menu bar
  if (mIsActive) {
    nsCOMPtr<nsISound> soundInterface = do_CreateInstance("@mozilla.org/sound;1");
    if (soundInterface)
      soundInterface->Beep();
  }

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsIFrame* popup = pm->GetTopPopup(ePopupTypeAny);
    if (popup)
      pm->HidePopup(popup->GetContent(), true, true, true, false);
  }

  SetCurrentMenuItem(nullptr);
  SetActive(false);

#endif  // #ifdef XP_WIN

  return nullptr;
}

/* virtual */ nsMenuFrame*
nsMenuBarFrame::GetCurrentMenuItem()
{
  return mCurrentMenu;
}

NS_IMETHODIMP
nsMenuBarFrame::SetCurrentMenuItem(nsMenuFrame* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  if (mCurrentMenu)
    mCurrentMenu->SelectMenu(false);

  if (aMenuItem)
    aMenuItem->SelectMenu(true);

  mCurrentMenu = aMenuItem;

  return NS_OK;
}

void
nsMenuBarFrame::CurrentMenuIsBeingDestroyed()
{
  mCurrentMenu->SelectMenu(false);
  mCurrentMenu = nullptr;
}

class nsMenuBarSwitchMenu : public Runnable
{
public:
  nsMenuBarSwitchMenu(nsIContent* aMenuBar,
                      nsIContent* aOldMenu,
                      nsIContent* aNewMenu,
                      bool aSelectFirstItem)
    : mozilla::Runnable("nsMenuBarSwitchMenu")
    , mMenuBar(aMenuBar)
    , mOldMenu(aOldMenu)
    , mNewMenu(aNewMenu)
    , mSelectFirstItem(aSelectFirstItem)
  {
  }

  NS_IMETHOD Run() override
  {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (!pm)
      return NS_ERROR_UNEXPECTED;

    // if switching from one menu to another, set a flag so that the call to
    // HidePopup doesn't deactivate the menubar when the first menu closes.
    nsMenuBarFrame* menubar = nullptr;
    if (mOldMenu && mNewMenu) {
      menubar = do_QueryFrame(mMenuBar->GetPrimaryFrame());
      if (menubar)
        menubar->SetStayActive(true);
    }

    if (mOldMenu) {
      AutoWeakFrame weakMenuBar(menubar);
      pm->HidePopup(mOldMenu, false, false, false, false);
      // clear the flag again
      if (mNewMenu && weakMenuBar.IsAlive())
        menubar->SetStayActive(false);
    }

    if (mNewMenu)
      pm->ShowMenu(mNewMenu, mSelectFirstItem, false);

    return NS_OK;
  }

private:
  nsCOMPtr<nsIContent> mMenuBar;
  nsCOMPtr<nsIContent> mOldMenu;
  nsCOMPtr<nsIContent> mNewMenu;
  bool mSelectFirstItem;
};

NS_IMETHODIMP
nsMenuBarFrame::ChangeMenuItem(nsMenuFrame* aMenuItem,
                               bool aSelectFirstItem,
                               bool aFromKey)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  // check if there's an open context menu, we ignore this
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && pm->HasContextMenu(nullptr))
    return NS_OK;

  nsIContent* aOldMenu = nullptr;
  nsIContent* aNewMenu = nullptr;

  // Unset the current child.
  bool wasOpen = false;
  if (mCurrentMenu) {
    wasOpen = mCurrentMenu->IsOpen();
    mCurrentMenu->SelectMenu(false);
    if (wasOpen) {
      nsMenuPopupFrame* popupFrame = mCurrentMenu->GetPopup();
      if (popupFrame)
        aOldMenu = popupFrame->GetContent();
    }
  }

  // set to null first in case the IsAlive check below returns false
  mCurrentMenu = nullptr;

  // Set the new child.
  if (aMenuItem) {
    nsCOMPtr<nsIContent> content = aMenuItem->GetContent();
    aMenuItem->SelectMenu(true);
    mCurrentMenu = aMenuItem;
    if (wasOpen && !aMenuItem->IsDisabled())
      aNewMenu = content;
  }

  // use an event so that hiding and showing can be done synchronously, which
  // avoids flickering
  nsCOMPtr<nsIRunnable> event =
    new nsMenuBarSwitchMenu(GetContent(), aOldMenu, aNewMenu, aSelectFirstItem);
  return mContent->OwnerDoc()->Dispatch(TaskCategory::Other,
                                        event.forget());
}

nsMenuFrame*
nsMenuBarFrame::Enter(WidgetGUIEvent* aEvent)
{
  if (!mCurrentMenu)
    return nullptr;

  if (mCurrentMenu->IsOpen())
    return mCurrentMenu->Enter(aEvent);

  return mCurrentMenu;
}

bool
nsMenuBarFrame::MenuClosed()
{
  SetActive(false);
  if (!mIsActive && mCurrentMenu) {
    mCurrentMenu->SelectMenu(false);
    mCurrentMenu = nullptr;
    return true;
  }
  return false;
}

void
nsMenuBarFrame::InstallKeyboardNavigator()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm)
    pm->SetActiveMenuBar(this, true);
}

void
nsMenuBarFrame::RemoveKeyboardNavigator()
{
  if (!mIsActive) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm)
      pm->SetActiveMenuBar(this, false);
  }
}

void
nsMenuBarFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm)
    pm->SetActiveMenuBar(this, false);

  mMenuBarListener->OnDestroyMenuBarFrame();
  mMenuBarListener = nullptr;

  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

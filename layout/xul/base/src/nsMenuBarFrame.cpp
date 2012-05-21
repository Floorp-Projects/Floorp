/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuBarFrame.h"
#include "nsIServiceManager.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsGkAtoms.h"
#include "nsMenuFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsGUIEvent.h"
#include "nsUnicharUtils.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCSSFrameConstructor.h"
#ifdef XP_WIN
#include "nsISound.h"
#include "nsWidgetsCID.h"
#endif
#include "nsContentUtils.h"
#include "nsUTF8Utils.h"


//
// NS_NewMenuBarFrame
//
// Wrapper for creating a new menu Bar container
//
nsIFrame*
NS_NewMenuBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMenuBarFrame (aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMenuBarFrame)

NS_QUERYFRAME_HEAD(nsMenuBarFrame)
  NS_QUERYFRAME_ENTRY(nsMenuBarFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

//
// nsMenuBarFrame cntr
//
nsMenuBarFrame::nsMenuBarFrame(nsIPresShell* aShell, nsStyleContext* aContext):
  nsBoxFrame(aShell, aContext),
    mMenuBarListener(nsnull),
    mStayActive(false),
    mIsActive(false),
    mCurrentMenu(nsnull),
    mTarget(nsnull)
{
} // cntr

NS_IMETHODIMP
nsMenuBarFrame::Init(nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Create the menu bar listener.
  mMenuBarListener = new nsMenuBarListener(this);
  NS_IF_ADDREF(mMenuBarListener);
  if (! mMenuBarListener)
    return NS_ERROR_OUT_OF_MEMORY;

  // Hook up the menu bar as a key listener on the whole document.  It will see every
  // key press that occurs, but after everyone else does.
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(aContent->GetDocument());
  
  mTarget = target;

  // Also hook up the listener to the window listening for focus events. This is so we can keep proper
  // state as the user alt-tabs through processes.
  
  target->AddEventListener(NS_LITERAL_STRING("keypress"), mMenuBarListener, false); 
  target->AddEventListener(NS_LITERAL_STRING("keydown"), mMenuBarListener, false);  
  target->AddEventListener(NS_LITERAL_STRING("keyup"), mMenuBarListener, false);   

  // mousedown event should be handled in all phase
  target->AddEventListener(NS_LITERAL_STRING("mousedown"), mMenuBarListener, true);
  target->AddEventListener(NS_LITERAL_STRING("mousedown"), mMenuBarListener, false);
  target->AddEventListener(NS_LITERAL_STRING("blur"), mMenuBarListener, true);   

  return rv;
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
      mCurrentMenu = nsnull;
      return closeframe;
    }
  }
  else {
    // if the menu bar is already selected (eg. mouseover), deselect it
    if (mCurrentMenu)
      mCurrentMenu->SelectMenu(false);
    
    // Activate the menu bar
    SetActive(true);

    // Set the active menu to be the top left item (e.g., the File menu).
    // We use an attribute called "menuactive" to track the current 
    // active menu.
    nsMenuFrame* firstFrame = nsXULPopupManager::GetNextMenuItem(this, nsnull, false);
    if (firstFrame) {
      firstFrame->SelectMenu(true);
      
      // Track this item for keyboard navigation.
      mCurrentMenu = firstFrame;
    }
  }

  return nsnull;
}

static void
GetInsertionPoint(nsIPresShell* aShell, nsIFrame* aFrame, nsIFrame* aChild,
                  nsIFrame** aResult)
{
  nsIContent* child = nsnull;
  if (aChild)
    child = aChild->GetContent();
  aShell->FrameConstructor()->GetInsertionPoint(aFrame, child, aResult);
}

nsMenuFrame*
nsMenuBarFrame::FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent)
{
  PRUint32 charCode;
  aKeyEvent->GetCharCode(&charCode);

  nsAutoTArray<PRUint32, 10> accessKeys;
  nsEvent* nativeEvent = nsContentUtils::GetNativeEvent(aKeyEvent);
  nsKeyEvent* nativeKeyEvent = static_cast<nsKeyEvent*>(nativeEvent);
  if (nativeKeyEvent)
    nsContentUtils::GetAccessKeyCandidates(nativeKeyEvent, accessKeys);
  if (accessKeys.IsEmpty() && charCode)
    accessKeys.AppendElement(charCode);

  if (accessKeys.IsEmpty())
    return nsnull; // no character was pressed so just return

  // Enumerate over our list of frames.
  nsIFrame* immediateParent = nsnull;
  GetInsertionPoint(PresContext()->PresShell(), this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  // Find a most preferred accesskey which should be returned.
  nsIFrame* foundMenu = nsnull;
  PRUint32 foundIndex = accessKeys.NoIndex;
  nsIFrame* currFrame = immediateParent->GetFirstPrincipalChild();

  while (currFrame) {
    nsIContent* current = currFrame->GetContent();

    // See if it's a menu item.
    if (nsXULPopupManager::IsValidMenuItem(PresContext(), current, false)) {
      // Get the shortcut attribute.
      nsAutoString shortcutKey;
      current->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, shortcutKey);
      if (!shortcutKey.IsEmpty()) {
        ToLowerCase(shortcutKey);
        const PRUnichar* start = shortcutKey.BeginReading();
        const PRUnichar* end = shortcutKey.EndReading();
        PRUint32 ch = UTF16CharEnumerator::NextChar(&start, end);
        PRUint32 index = accessKeys.IndexOf(ch);
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
    return (foundMenu->GetType() == nsGkAtoms::menuFrame) ?
           static_cast<nsMenuFrame *>(foundMenu) : nsnull;
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
      pm->HidePopup(popup->GetContent(), true, true, true);
  }

  SetCurrentMenuItem(nsnull);
  SetActive(false);

#endif  // #ifdef XP_WIN

  return nsnull;
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
  mCurrentMenu = nsnull;
}

class nsMenuBarSwitchMenu : public nsRunnable
{
public:
  nsMenuBarSwitchMenu(nsIContent* aMenuBar,
                      nsIContent *aOldMenu,
                      nsIContent *aNewMenu,
                      bool aSelectFirstItem)
    : mMenuBar(aMenuBar), mOldMenu(aOldMenu), mNewMenu(aNewMenu),
      mSelectFirstItem(aSelectFirstItem)
  {
  }

  NS_IMETHOD Run()
  {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (!pm)
      return NS_ERROR_UNEXPECTED;

    // if switching from one menu to another, set a flag so that the call to
    // HidePopup doesn't deactivate the menubar when the first menu closes.
    nsMenuBarFrame* menubar = nsnull;
    if (mOldMenu && mNewMenu) {
      menubar = static_cast<nsMenuBarFrame *>
        (pm->GetFrameOfTypeForContent(mMenuBar, nsGkAtoms::menuBarFrame, false));
      if (menubar)
        menubar->SetStayActive(true);
    }

    if (mOldMenu) {
      nsWeakFrame weakMenuBar(menubar);
      pm->HidePopup(mOldMenu, false, false, false);
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
                               bool aSelectFirstItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  // check if there's an open context menu, we ignore this
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && pm->HasContextMenu(nsnull))
    return NS_OK;

  nsIContent* aOldMenu = nsnull, *aNewMenu = nsnull;
  
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
  mCurrentMenu = nsnull;

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
  return NS_DispatchToCurrentThread(event);
}

nsMenuFrame*
nsMenuBarFrame::Enter(nsGUIEvent* aEvent)
{
  if (!mCurrentMenu)
    return nsnull;

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
    mCurrentMenu = nsnull;
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
nsMenuBarFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm)
    pm->SetActiveMenuBar(this, false);

  mTarget->RemoveEventListener(NS_LITERAL_STRING("keypress"), mMenuBarListener, false); 
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), mMenuBarListener, false);  
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keyup"), mMenuBarListener, false);

  mTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), mMenuBarListener, true);
  mTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), mMenuBarListener, false);
  mTarget->RemoveEventListener(NS_LITERAL_STRING("blur"), mMenuBarListener, true);

  NS_IF_RELEASE(mMenuBarListener);

  nsBoxFrame::DestroyFrom(aDestructRoot);
}

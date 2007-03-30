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
 *   Dan Rosen <dr@netscape.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
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

#include "nsMenuListener.h"
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
#include "nsIDOMEventReceiver.h"
#include "nsGkAtoms.h"
#include "nsMenuFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsGUIEvent.h"
#include "nsUnicharUtils.h"
#include "nsICaret.h"
#include "nsIFocusController.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCSSFrameConstructor.h"
#ifdef XP_WIN
#include "nsISound.h"
#include "nsWidgetsCID.h"
#endif


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

NS_IMETHODIMP_(nsrefcnt) 
nsMenuBarFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuBarFrame::Release(void)
{
    return NS_OK;
}


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsMenuBarFrame)
  NS_INTERFACE_MAP_ENTRY(nsIMenuParent)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


//
// nsMenuBarFrame cntr
//
nsMenuBarFrame::nsMenuBarFrame(nsIPresShell* aShell, nsStyleContext* aContext):
  nsBoxFrame(aShell, aContext),
    mMenuBarListener(nsnull),
    mKeyboardNavigator(nsnull),
    mIsActive(PR_FALSE),
    mTarget(nsnull),
    mCaretWasVisible(PR_FALSE)
{
} // cntr

nsMenuBarFrame::~nsMenuBarFrame()
{
  /* The menubar can still be active at this point under unusual circumstances.
     (say, while switching skins (which tears down all frames including
     this one) after having made a menu selection (say, Edit->Preferences,
     to get to the skin switching UI)). SetActive(PR_FALSE) releases
     mKeyboardNavigator, which is by now pointing to a deleted frame.
  */
  SetActive(PR_FALSE);
}

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
  nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(aContent->GetDocument());
  
  mTarget = target;

  // Also hook up the listener to the window listening for focus events. This is so we can keep proper
  // state as the user alt-tabs through processes.
  
  target->AddEventListener(NS_LITERAL_STRING("keypress"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE); 
  target->AddEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);  
  target->AddEventListener(NS_LITERAL_STRING("keyup"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);   

  target->AddEventListener(NS_LITERAL_STRING("mousedown"), (nsIDOMMouseListener*)mMenuBarListener, PR_FALSE);   
  target->AddEventListener(NS_LITERAL_STRING("blur"), (nsIDOMFocusListener*)mMenuBarListener, PR_TRUE);   

  return rv;
}

NS_IMETHODIMP
nsMenuBarFrame::IsOpen()
{
  PRBool isOpen = PR_FALSE;
  if(mCurrentMenu) {
    mCurrentMenu->MenuIsOpen(isOpen);
    if (isOpen) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


NS_IMETHODIMP
nsMenuBarFrame::SetActive(PRBool aActiveFlag)
{
  // If the activity is not changed, there is nothing to do.
  if (mIsActive == aActiveFlag)
    return NS_OK;

  mIsActive = aActiveFlag;
  if (mIsActive) {
    InstallKeyboardNavigator();
  }
  else {
    RemoveKeyboardNavigator();
  }
  
  // We don't want the caret to blink while the menus are active
  // The caret distracts screen readers and other assistive technologies from the menu selection
  // There is 1 caret per document, we need to find the focused document and toggle its caret 
  do {
    nsIPresShell *presShell = PresContext()->GetPresShell();
    if (!presShell)
      break;

    nsIDocument *document = presShell->GetDocument();
    if (!document)
      break;

    nsCOMPtr<nsISupports> container = document->GetContainer();
    nsCOMPtr<nsPIDOMWindow> windowPrivate = do_GetInterface(container);
    if (!windowPrivate)
      break;

    nsIFocusController *focusController =
      windowPrivate->GetRootFocusController();
    if (!focusController)
      break;

    nsCOMPtr<nsIDOMWindowInternal> windowInternal;
    focusController->GetFocusedWindow(getter_AddRefs(windowInternal));
    if (!windowInternal)
      break;

    nsCOMPtr<nsIDOMDocument> domDoc;
    nsCOMPtr<nsIDocument> focusedDoc;
    windowInternal->GetDocument(getter_AddRefs(domDoc));
    focusedDoc = do_QueryInterface(domDoc);
    if (!focusedDoc)
      break;

    presShell = focusedDoc->GetShellAt(0);
    nsCOMPtr<nsISelectionController> selCon(do_QueryInterface(presShell));
    // there is no selection controller for full page plugins
    if (!selCon)
      break;

    if (mIsActive) {// store whether caret was visible so that we can restore that state when menu is closed
      PRBool isCaretVisible;
      selCon->GetCaretEnabled(&isCaretVisible);
      mCaretWasVisible |= isCaretVisible;
    }
    selCon->SetCaretEnabled(!mIsActive && mCaretWasVisible);
    if (!mIsActive) {
      mCaretWasVisible = PR_FALSE;
    }
  } while (0);

  NS_NAMED_LITERAL_STRING(active, "DOMMenuBarActive");
  NS_NAMED_LITERAL_STRING(inactive, "DOMMenuBarInactive");
  
  FireDOMEventSynch(mIsActive ? active : inactive);

  return NS_OK;
}

void
nsMenuBarFrame::ToggleMenuActiveState()
{
  if (mIsActive) {
    // Deactivate the menu bar
    SetActive(PR_FALSE);
    if (mCurrentMenu) {
      // Deactivate the menu.
      mCurrentMenu->OpenMenu(PR_FALSE);
      mCurrentMenu->SelectMenu(PR_FALSE);
      mCurrentMenu = nsnull;
    }
  }
  else {
    // if the menu bar is already selected (eg. mouseover), deselect it
    if (mCurrentMenu)
      mCurrentMenu->SelectMenu(PR_FALSE);
    
    // Activate the menu bar
    SetActive(PR_TRUE);

    // Set the active menu to be the top left item (e.g., the File menu).
    // We use an attribute called "menuactive" to track the current 
    // active menu.
    nsIMenuFrame* firstFrame = GetNextMenuItem(nsnull);
    if (firstFrame) {
      firstFrame->SelectMenu(PR_TRUE);
      
      // Track this item for keyboard navigation.
      mCurrentMenu = firstFrame;
    }
  }
}

static void GetInsertionPoint(nsIPresShell* aShell, nsIFrame* aFrame, nsIFrame* aChild,
                              nsIFrame** aResult)
{
  nsIContent* child = nsnull;
  if (aChild)
    child = aChild->GetContent();
  aShell->FrameConstructor()->GetInsertionPoint(aFrame, child, aResult);
}

nsIMenuFrame*
nsMenuBarFrame::FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent)
{
  PRUint32 charCode;
  aKeyEvent->GetCharCode(&charCode);

  // Enumerate over our list of frames.
  nsIFrame* immediateParent = nsnull;
  GetInsertionPoint(PresContext()->PresShell(), this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsIFrame* currFrame = immediateParent->GetFirstChild(nsnull);

  while (currFrame) {
    nsIContent* current = currFrame->GetContent();
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      // Get the shortcut attribute.
      nsAutoString shortcutKey;
      current->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, shortcutKey);
      if (!shortcutKey.IsEmpty()) {
        // We've got something.
        PRUnichar letter = PRUnichar(charCode); // throw away the high-zero-fill
        if ( shortcutKey.Equals(Substring(&letter, &letter+1),
                                nsCaseInsensitiveStringComparator()) )  {
          // We match!
          nsIMenuFrame *menuFrame;
          if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame))) {
            menuFrame = nsnull;
          }
          return menuFrame;
        }
      }
    }
    currFrame = currFrame->GetNextSibling();
  }

  // didn't find a matching menu item
#ifdef XP_WIN
  // behavior on Windows - this item is on the menu bar, beep and deactivate the menu bar
  if (mIsActive) {
    nsCOMPtr<nsISound> soundInterface = do_CreateInstance("@mozilla.org/sound;1");
    if (soundInterface)
      soundInterface->Beep();
  }

  DismissChain();
#endif  // #ifdef XP_WIN

  return nsnull;
}

NS_IMETHODIMP 
nsMenuBarFrame::ShortcutNavigation(nsIDOMKeyEvent* aKeyEvent, PRBool& aHandledFlag)
{
  if (mCurrentMenu) {
    PRBool isOpen = PR_FALSE;
    mCurrentMenu->MenuIsOpen(isOpen);
    if (isOpen) {
      // No way this applies to us. Give it to our child.
      mCurrentMenu->ShortcutNavigation(aKeyEvent, aHandledFlag);
      return NS_OK;
    }
  }

  // This applies to us. Let's see if one of the shortcuts applies
  nsIMenuFrame* result = FindMenuWithShortcut(aKeyEvent);
  if (result) {
    // We got one!
    nsWeakFrame weakFrame(this);
    nsIFrame* frame = nsnull;
    CallQueryInterface(result, &frame);
    nsWeakFrame weakResult(frame);
    aHandledFlag = PR_TRUE;
    SetActive(PR_TRUE);
    if (weakFrame.IsAlive()) {
      SetCurrentMenuItem(result);
    }
    if (weakResult.IsAlive()) {
      result->OpenMenu(PR_TRUE);
      if (weakResult.IsAlive()) {
        result->SelectFirstItem();
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::KeyboardNavigation(PRUint32 aKeyCode, PRBool& aHandledFlag)
{
  nsNavigationDirection theDirection;
  NS_DIRECTION_FROM_KEY_CODE(theDirection, aKeyCode);
  if (!mCurrentMenu)
    return NS_OK;

  nsWeakFrame weakFrame(this);
  PRBool isContainer = PR_FALSE;
  PRBool isOpen = PR_FALSE;
  mCurrentMenu->MenuIsContainer(isContainer);
  mCurrentMenu->MenuIsOpen(isOpen);

  aHandledFlag = PR_FALSE;
  
  if (isOpen) {
    // Let the child menu try to handle it.
    mCurrentMenu->KeyboardNavigation(aKeyCode, aHandledFlag);
  }

  if (aHandledFlag)
    return NS_OK;

  if NS_DIRECTION_IS_INLINE(theDirection) {
    
    nsIMenuFrame* nextItem = (theDirection == eNavigationDirection_End) ?
                             GetNextMenuItem(mCurrentMenu) : 
                             GetPreviousMenuItem(mCurrentMenu);

    nsIFrame* nextFrame = nsnull;
    if (nextItem) {
      CallQueryInterface(nextItem, &nextFrame);
    }
    nsWeakFrame weakNext(nextFrame);
    SetCurrentMenuItem(nextItem);
    if (weakNext.IsAlive()) {
      PRBool nextIsOpen;
      nextItem->MenuIsOpen(nextIsOpen);
      if (nextIsOpen) {
        // Select the first item.
        nextItem->SelectFirstItem();
      }
    }
  }
  else if NS_DIRECTION_IS_BLOCK(theDirection) {
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    nsIFrame* frame = nsnull;
    CallQueryInterface(mCurrentMenu, &frame);
    nsWeakFrame weakCurrentMenu(frame);
    nsIMenuFrame* currentMenu = mCurrentMenu;
     // Open the menu and select its first item.
    currentMenu->OpenMenu(PR_TRUE);
    if (weakCurrentMenu.IsAlive()) {
      currentMenu->SelectFirstItem();
    }
  }

  return NS_OK;
}

/* virtual */ nsIMenuFrame*
nsMenuBarFrame::GetNextMenuItem(nsIMenuFrame* aStart)
{
  nsIFrame* immediateParent = nsnull;
  GetInsertionPoint(PresContext()->PresShell(), this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsIFrame* currFrame = nsnull;
  nsIFrame* startFrame = nsnull;
  if (aStart) {
    aStart->QueryInterface(NS_GET_IID(nsIFrame), (void**)&currFrame); 
    if (currFrame) {
      startFrame = currFrame;
      currFrame = currFrame->GetNextSibling();
    }
  }
  else 
    currFrame = immediateParent->GetFirstChild(nsnull);

  while (currFrame) {
    // See if it's a menu item.
    if (IsValidItem(currFrame->GetContent())) {
      nsIMenuFrame *menuFrame;
      if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame)))
        menuFrame = nsnull;
      return menuFrame;
    }
    currFrame = currFrame->GetNextSibling();
  }

  currFrame = immediateParent->GetFirstChild(nsnull);

  // Still don't have anything. Try cycling from the beginning.
  while (currFrame && currFrame != startFrame) {
    // See if it's a menu item.
    if (IsValidItem(currFrame->GetContent())) {
      nsIMenuFrame *menuFrame;
      if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame)))
        menuFrame = nsnull;
      return menuFrame;
    }

    currFrame = currFrame->GetNextSibling();
  }

  // No luck. Just return our start value.
  return aStart;
}

/* virtual */ nsIMenuFrame*
nsMenuBarFrame::GetPreviousMenuItem(nsIMenuFrame* aStart)
{
  nsIFrame* immediateParent = nsnull;
  GetInsertionPoint(PresContext()->PresShell(), this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsFrameList frames(immediateParent->GetFirstChild(nsnull));
                              
  nsIFrame* currFrame = nsnull;
  nsIFrame* startFrame = nsnull;
  if (aStart) {
    aStart->QueryInterface(NS_GET_IID(nsIFrame), (void**)&currFrame);
    if (currFrame) {
      startFrame = currFrame;
      currFrame = frames.GetPrevSiblingFor(currFrame);
    }
  }
  else currFrame = frames.LastChild();

  while (currFrame) {
    // See if it's a menu item.
    if (IsValidItem(currFrame->GetContent())) {
      nsIMenuFrame *menuFrame;
      if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame)))
        menuFrame = nsnull;
      return menuFrame;
    }
    currFrame = frames.GetPrevSiblingFor(currFrame);
  }

  currFrame = frames.LastChild();

  // Still don't have anything. Try cycling from the end.
  while (currFrame && currFrame != startFrame) {
    // See if it's a menu item.
    if (IsValidItem(currFrame->GetContent())) {
      nsIMenuFrame *menuFrame;
      if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame)))
        menuFrame = nsnull;
      return menuFrame;
    }

    currFrame = frames.GetPrevSiblingFor(currFrame);
  }

  // No luck. Just return our start value.
  return aStart;
}

/* virtual */ nsIMenuFrame*
nsMenuBarFrame::GetCurrentMenuItem()
{
  return mCurrentMenu;
}


NS_IMETHODIMP nsMenuBarFrame::SetCurrentMenuItem(nsIMenuFrame* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  PRBool wasOpen = PR_FALSE;
  
  // check if there's an open context menu, we ignore this
  if (nsMenuFrame::GetContextMenu())
    return NS_OK;

  nsWeakFrame weakFrame(this);

  // Unset the current child.
  if (mCurrentMenu) {
    nsIFrame* frame = nsnull;
    CallQueryInterface(mCurrentMenu, &frame);
    nsWeakFrame weakCurrentMenu(frame);
    nsIMenuFrame* currentMenu = mCurrentMenu;
    currentMenu->MenuIsOpen(wasOpen);
    currentMenu->SelectMenu(PR_FALSE);
    if (wasOpen && weakCurrentMenu.IsAlive()) {
      currentMenu->OpenMenu(PR_FALSE);
    }
  }

  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);


  // Set the new child.
  if (aMenuItem) {
    nsIFrame* newMenu = nsnull;
    CallQueryInterface(aMenuItem, &newMenu);
    nsWeakFrame weakNewMenu(newMenu);
    aMenuItem->SelectMenu(PR_TRUE);
    NS_ENSURE_TRUE(weakNewMenu.IsAlive(), NS_OK);
    aMenuItem->MarkAsGenerated(); // Have the menu building. Get it ready to be shown.
    NS_ENSURE_TRUE(weakNewMenu.IsAlive(), NS_OK);

    PRBool isDisabled = PR_FALSE;
    aMenuItem->MenuIsDisabled(isDisabled);
    if (wasOpen&&!isDisabled)
      aMenuItem->OpenMenu(PR_TRUE);
    ClearRecentlyRolledUp();
  }

  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
  mCurrentMenu = aMenuItem;

  return NS_OK;
}


NS_IMETHODIMP 
nsMenuBarFrame::Escape(PRBool& aHandledFlag)
{
  if (!mCurrentMenu)
    return NS_OK;

  nsWeakFrame weakFrame(this);
  // See if our menu is open.
  PRBool isOpen = PR_FALSE;
  mCurrentMenu->MenuIsOpen(isOpen);
  if (isOpen) {
    // Let the child menu handle this.
    aHandledFlag = PR_FALSE;
    mCurrentMenu->Escape(aHandledFlag);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    if (!aHandledFlag) {
      // Close up this menu but keep our current menu item
      // designation.
      mCurrentMenu->OpenMenu(PR_FALSE);
      NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    }
    nsMenuDismissalListener::Shutdown();
    return NS_OK;
  }

  // Clear our current menu item if we've got one.
  SetCurrentMenuItem(nsnull);
  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);

  SetActive(PR_FALSE);

  // Clear out our dismissal listener
  nsMenuDismissalListener::Shutdown();
  return NS_OK;
}

NS_IMETHODIMP 
nsMenuBarFrame::Enter()
{
  if (!mCurrentMenu)
    return NS_OK;

  ClearRecentlyRolledUp();

  // See if our menu is open.
  PRBool isOpen = PR_FALSE;
  mCurrentMenu->MenuIsOpen(isOpen);
  if (isOpen) {
    // Let the child menu handle this.
    mCurrentMenu->Enter();
    return NS_OK;
  }

  // It's us. Open the current menu.
  mCurrentMenu->OpenMenu(PR_TRUE);
  mCurrentMenu->SelectFirstItem();

  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::ClearRecentlyRolledUp()
{
  // We're no longer in danger of popping down a menu from the same 
  // click on the menubar, which was supposed to toggle the menu closed
  mRecentRollupMenu = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::RecentlyRolledUp(nsIMenuFrame *aMenuFrame, PRBool *aJustRolledUp)
{
  // Don't let a click reopen a menu that was just rolled up
  // from the same click. Otherwise, the user can't click on
  // a menubar item to toggle its submenu closed.
  *aJustRolledUp = (mRecentRollupMenu == aMenuFrame);

  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::HideChain()
{
  // XXX hack if a context menu is active, do an Escape, which is
  // currently bugged and destroys everything.  We need to close
  // the context menu first, otherwise SetCurrentMenuItem above
  // would get blocked.
  if (nsMenuFrame::GetContextMenu()) {
    PRBool dummy;
    mCurrentMenu->Escape(dummy);
  }

  // Stop capturing rollups
  // (must do this during Hide, which happens before the menu item is executed,
  // since this reinstates normal event handling.)
  nsMenuDismissalListener::Shutdown();

  ClearRecentlyRolledUp();
  if (mCurrentMenu) {
    mCurrentMenu->ActivateMenu(PR_FALSE);
    mCurrentMenu->SelectMenu(PR_FALSE);
    mRecentRollupMenu = mCurrentMenu;
  }

  if (mIsActive) {
    ToggleMenuActiveState();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::DismissChain()
{
  // Stop capturing rollups
  nsMenuDismissalListener::Shutdown();
  nsWeakFrame weakFrame(this);
  SetCurrentMenuItem(nsnull);
  if (weakFrame.IsAlive()) {
    SetActive(PR_FALSE);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsMenuBarFrame::KillPendingTimers ( )
{
  return NS_OK;

} // KillPendingTimers


NS_IMETHODIMP
nsMenuBarFrame::GetWidget(nsIWidget **aWidget)
{
  // (pinkerton/hyatt)
  // since the menubar is a menuparent but not a menuItem, the win32 rollup code
  // would erroneously add the entire top-level window to the widget list built up for
  // determining if a click is in a submenu's menu chain. To get around this, we just 
  // don't let the menubar have a widget. Things seem to work because the dismissal
  // listener is registered when a new menu is popped up, which is the only real reason
  // why we need a widget at all.
  *aWidget = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::InstallKeyboardNavigator()
{
  if (mKeyboardNavigator)
    return NS_OK;

  mKeyboardNavigator = new nsMenuListener(this);
  NS_IF_ADDREF(mKeyboardNavigator);

  mTarget->AddEventListener(NS_LITERAL_STRING("keypress"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE); 
  mTarget->AddEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);  
  mTarget->AddEventListener(NS_LITERAL_STRING("keyup"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);   
  
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::RemoveKeyboardNavigator()
{
  if (!mKeyboardNavigator || mIsActive)
    return NS_OK;

  mTarget->RemoveEventListener(NS_LITERAL_STRING("keypress"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keyup"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  
  NS_IF_RELEASE(mKeyboardNavigator);

  return NS_OK;
}

// helpers ///////////////////////////////////////////////////////////

PRBool 
nsMenuBarFrame::IsValidItem(nsIContent* aContent)
{
  nsIAtom *tag = aContent->Tag();

  return ((tag == nsGkAtoms::menu ||
           tag == nsGkAtoms::menuitem) &&
          !IsDisabled(aContent));
}

PRBool 
nsMenuBarFrame::IsDisabled(nsIContent* aContent)
{
  return aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                               nsGkAtoms::_true, eCaseMatters);
}

void
nsMenuBarFrame::Destroy()
{
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keypress"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE); 
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);  
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keyup"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);

  mTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), (nsIDOMMouseListener*)mMenuBarListener, PR_FALSE);
  mTarget->RemoveEventListener(NS_LITERAL_STRING("blur"), (nsIDOMFocusListener*)mMenuBarListener, PR_TRUE);

  NS_IF_RELEASE(mMenuBarListener);

  nsBoxFrame::Destroy();
}


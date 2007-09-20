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

//
// nsMenuBarFrame cntr
//
nsMenuBarFrame::nsMenuBarFrame(nsIPresShell* aShell, nsStyleContext* aContext):
  nsBoxFrame(aShell, aContext),
    mMenuBarListener(nsnull),
    mIsActive(PR_FALSE),
    mTarget(nsnull),
    mCaretWasVisible(PR_FALSE)
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
  
  target->AddEventListener(NS_LITERAL_STRING("keypress"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE); 
  target->AddEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);  
  target->AddEventListener(NS_LITERAL_STRING("keyup"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);   

  target->AddEventListener(NS_LITERAL_STRING("mousedown"), (nsIDOMMouseListener*)mMenuBarListener, PR_FALSE);   
  target->AddEventListener(NS_LITERAL_STRING("blur"), (nsIDOMFocusListener*)mMenuBarListener, PR_TRUE);   

  return rv;
}

NS_IMETHODIMP
nsMenuBarFrame::SetActive(PRBool aActiveFlag)
{
  // If the activity is not changed, there is nothing to do.
  if (mIsActive == aActiveFlag)
    return NS_OK;

  if (!aActiveFlag) {
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

    presShell = focusedDoc->GetPrimaryShell();
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
  
  FireDOMEvent(mIsActive ? active : inactive, mContent);

  return NS_OK;
}

nsMenuFrame*
nsMenuBarFrame::ToggleMenuActiveState()
{
  if (mIsActive) {
    // Deactivate the menu bar
    SetActive(PR_FALSE);
    if (mCurrentMenu) {
      nsMenuFrame* closeframe = mCurrentMenu;
      closeframe->SelectMenu(PR_FALSE);
      mCurrentMenu = nsnull;
      return closeframe;
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
    nsMenuFrame* firstFrame = nsXULPopupManager::GetNextMenuItem(this, nsnull, PR_FALSE);
    if (firstFrame) {
      firstFrame->SelectMenu(PR_TRUE);
      
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
  if (!charCode) // no character was pressed so just return  
    return nsnull;

  // Enumerate over our list of frames.
  nsIFrame* immediateParent = nsnull;
  GetInsertionPoint(PresContext()->PresShell(), this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsIFrame* currFrame = immediateParent->GetFirstChild(nsnull);

  while (currFrame) {
    nsIContent* current = currFrame->GetContent();
    
    // See if it's a menu item.
    if (nsXULPopupManager::IsValidMenuItem(PresContext(), current, PR_FALSE)) {
      // Get the shortcut attribute.
      nsAutoString shortcutKey;
      current->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, shortcutKey);
      if (!shortcutKey.IsEmpty()) {
        // We've got something.
        PRUnichar letter = PRUnichar(charCode); // throw away the high-zero-fill
        if ( shortcutKey.Equals(Substring(&letter, &letter+1),
                                nsCaseInsensitiveStringComparator()) )  {
          // We match!
          return (currFrame->GetType() == nsGkAtoms::menuFrame) ?
                 static_cast<nsMenuFrame *>(currFrame) : nsnull;
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

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsIFrame* popup = pm->GetTopPopup(ePopupTypeAny);
    if (popup)
      pm->HidePopup(popup->GetContent(), PR_TRUE, PR_TRUE, PR_TRUE);
  }

  SetCurrentMenuItem(nsnull);
  SetActive(PR_FALSE);

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

  nsWeakFrame weakFrame(this);
  if (mCurrentMenu)
    mCurrentMenu->SelectMenu(PR_FALSE);

  if (aMenuItem)
    aMenuItem->SelectMenu(PR_TRUE);

  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
  mCurrentMenu = aMenuItem;

  return NS_OK;
}

void
nsMenuBarFrame::CurrentMenuIsBeingDestroyed()
{
  mCurrentMenu->SelectMenu(PR_FALSE);
  mCurrentMenu = nsnull;
}

class nsMenuBarSwitchMenu : public nsRunnable
{
public:
  nsMenuBarSwitchMenu(nsIContent *aOldMenu,
                      nsIContent *aNewMenu,
                      PRBool aSelectFirstItem)
    : mOldMenu(aOldMenu), mNewMenu(aNewMenu), mSelectFirstItem(aSelectFirstItem)
  {
  }

  NS_IMETHOD Run()
  {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (!pm)
      return NS_ERROR_UNEXPECTED;

    if (mOldMenu)
      pm->HidePopup(mOldMenu, PR_FALSE, PR_FALSE, PR_FALSE);
    if (mNewMenu)
      pm->ShowMenu(mNewMenu, mSelectFirstItem, PR_FALSE);

    return NS_OK;
  }

private:
  nsCOMPtr<nsIContent> mOldMenu;
  nsCOMPtr<nsIContent> mNewMenu;
  PRBool mSelectFirstItem;
};

NS_IMETHODIMP
nsMenuBarFrame::ChangeMenuItem(nsMenuFrame* aMenuItem,
                               PRBool aSelectFirstItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  // check if there's an open context menu, we ignore this
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && pm->HasContextMenu(nsnull))
    return NS_OK;

  nsIContent* aOldMenu = nsnull, *aNewMenu = nsnull;
  
  // Unset the current child.
  PRBool wasOpen = PR_FALSE;
  if (mCurrentMenu) {
    wasOpen = mCurrentMenu->IsOpen();
    mCurrentMenu->SelectMenu(PR_FALSE);
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
    nsWeakFrame weakNewMenu(aMenuItem);
    aMenuItem->SelectMenu(PR_TRUE);
    NS_ENSURE_TRUE(weakNewMenu.IsAlive(), NS_OK);
    mCurrentMenu = aMenuItem;
    if (wasOpen && !aMenuItem->IsDisabled())
      aNewMenu = content;
  }

  // use an event so that hiding and showing can be done synchronously, which
  // avoids flickering
  nsCOMPtr<nsIRunnable> event =
    new nsMenuBarSwitchMenu(aOldMenu, aNewMenu, aSelectFirstItem);
  return NS_DispatchToCurrentThread(event);
}

nsMenuFrame*
nsMenuBarFrame::Enter()
{
  if (!mCurrentMenu)
    return nsnull;

  if (mCurrentMenu->IsOpen())
    return mCurrentMenu->Enter();

  return mCurrentMenu;
}

PRBool
nsMenuBarFrame::MenuClosed()
{
  SetActive(PR_FALSE);
  if (!mIsActive && mCurrentMenu) {
    mCurrentMenu->SelectMenu(PR_FALSE);
    mCurrentMenu = nsnull;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsMenuBarFrame::InstallKeyboardNavigator()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm)
    pm->SetActiveMenuBar(this, PR_TRUE);
}

void
nsMenuBarFrame::RemoveKeyboardNavigator()
{
  if (!mIsActive) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm)
      pm->SetActiveMenuBar(this, PR_FALSE);
  }
}

void
nsMenuBarFrame::Destroy()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm)
    pm->SetActiveMenuBar(this, PR_FALSE);

  mTarget->RemoveEventListener(NS_LITERAL_STRING("keypress"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE); 
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);  
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keyup"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);

  mTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), (nsIDOMMouseListener*)mMenuBarListener, PR_FALSE);
  mTarget->RemoveEventListener(NS_LITERAL_STRING("blur"), (nsIDOMFocusListener*)mMenuBarListener, PR_TRUE);

  NS_IF_RELEASE(mMenuBarListener);

  nsBoxFrame::Destroy();
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 * Contributor(s): Dan Rosen <dr@netscape.com>
 *                 Dean Tessman <dean_tessman@hotmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMenuListener.h"
#include "nsMenuBarFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsMenuFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIFrameManager.h"
#include "nsMenuPopupFrame.h"
#include "nsGUIEvent.h"
#include "nsUnicharUtils.h"


//
// NS_NewMenuBarFrame
//
// Wrapper for creating a new menu Bar container
//
nsresult
NS_NewMenuBarFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMenuBarFrame* it = new (aPresShell) nsMenuBarFrame (aPresShell);
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
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
nsMenuBarFrame::nsMenuBarFrame(nsIPresShell* aShell):nsBoxFrame(aShell),
mMenuBarListener(nsnull), mKeyboardNavigator(nsnull),
mIsActive(PR_FALSE), mTarget(nsnull)
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
nsMenuBarFrame::Init(nsIPresContext*  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIStyleContext* aContext,
                     nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // XXX hack
  mPresContext = aPresContext;

  // Create the menu bar listener.
  mMenuBarListener = new nsMenuBarListener(this);
  NS_IF_ADDREF(mMenuBarListener);
  if (! mMenuBarListener)
    return NS_ERROR_OUT_OF_MEMORY;

  // Hook up the menu bar as a key listener on the whole document.  It will see every
  // key press that occurs, but after everyone else does.
  nsCOMPtr<nsIDocument> doc;
  aContent->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(doc);
  
  mTarget = target;

  // Also hook up the listener to the window listening for focus events. This is so we can keep proper
  // state as the user alt-tabs through processes.
  
  target->AddEventListener(NS_ConvertASCIItoUCS2("keypress"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE); 
  target->AddEventListener(NS_ConvertASCIItoUCS2("keydown"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);  
  target->AddEventListener(NS_ConvertASCIItoUCS2("keyup"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);   

  target->AddEventListener(NS_ConvertASCIItoUCS2("mousedown"), (nsIDOMMouseListener*)mMenuBarListener, PR_FALSE);   
  target->AddEventListener(NS_ConvertASCIItoUCS2("blur"), (nsIDOMFocusListener*)mMenuBarListener, PR_TRUE);   

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
  mIsActive = aActiveFlag;
  if (mIsActive) {
    InstallKeyboardNavigator();
  }
  else if (mKeyboardNavigator) {
    mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keypress"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
    mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keydown"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
    mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keyup"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  
    NS_IF_RELEASE(mKeyboardNavigator);
  }

  return NS_OK;
}

void
nsMenuBarFrame::ToggleMenuActiveState()
{
  if (mIsActive) {
    // Deactivate the menu bar
    mIsActive = PR_FALSE;
    if (mCurrentMenu) {
      // Deactivate the menu.
      mCurrentMenu->OpenMenu(PR_FALSE);
      mCurrentMenu->SelectMenu(PR_FALSE);
      mCurrentMenu = nsnull;
      RemoveKeyboardNavigator();
    }
  }
  else {
    // if the menu bar is already selected (eg. mouseover), deselect it
    if (mCurrentMenu)
      mCurrentMenu->SelectMenu(PR_FALSE);
    
    // Activate the menu bar
    SetActive(PR_TRUE);

    InstallKeyboardNavigator();

    // Set the active menu to be the top left item (e.g., the File menu).
    // We use an attribute called "active" to track the current active menu.
    nsCOMPtr<nsIContent> firstMenuItem;
    nsIMenuFrame* firstFrame;
    GetNextMenuItem(nsnull, &firstFrame);
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
  nsCOMPtr<nsIStyleSet> styleSet;
  aShell->GetStyleSet(getter_AddRefs(styleSet));
  nsCOMPtr<nsIContent> child;
  if (aChild)
    aChild->GetContent(getter_AddRefs(child));
  styleSet->GetInsertionPoint(aShell, aFrame, child, aResult);
}

nsIMenuFrame*
nsMenuBarFrame::FindMenuWithShortcut(PRUint32 aLetter)
{
  // Enumerate over our list of frames.
  nsIFrame* immediateParent = nsnull;
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  GetInsertionPoint(shell, this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsIFrame* currFrame;
  immediateParent->FirstChild(mPresContext, nsnull, &currFrame);

  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      // Get the shortcut attribute.
      nsAutoString shortcutKey;
      current->GetAttr(kNameSpaceID_None, nsXULAtoms::accesskey, shortcutKey);
      if (!shortcutKey.IsEmpty()) {
        // We've got something.
        PRUnichar letter = PRUnichar(aLetter); // throw away the high-zero-fill
        if ( Compare(shortcutKey, Substring(&letter, &letter+1), nsCaseInsensitiveStringComparator())==0 )  {
          // We match!
          nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(currFrame);
          if (menuFrame)
            return menuFrame.get();
          return nsnull;
        }
      }
    }
    currFrame->GetNextSibling(&currFrame);
  }
  return nsnull;
}

NS_IMETHODIMP 
nsMenuBarFrame::ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag)
{
  if (mCurrentMenu) {
    PRBool isOpen = PR_FALSE;
    mCurrentMenu->MenuIsOpen(isOpen);
    if (isOpen) {
      // No way this applies to us. Give it to our child.
      mCurrentMenu->ShortcutNavigation(aLetter, aHandledFlag);
      return NS_OK;
    }
  }

  // This applies to us. Let's see if one of the shortcuts applies
  nsIMenuFrame* result = FindMenuWithShortcut(aLetter);
  if (result) {
    // We got one!
    aHandledFlag = PR_TRUE;
    mIsActive = PR_TRUE;
    SetCurrentMenuItem(result);
    result->OpenMenu(PR_TRUE);
    result->SelectFirstItem();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag)
{
  if (!mCurrentMenu)
    return NS_OK;
  
  PRBool isContainer = PR_FALSE;
  PRBool isOpen = PR_FALSE;
  mCurrentMenu->MenuIsContainer(isContainer);
  mCurrentMenu->MenuIsOpen(isOpen);

  aHandledFlag = PR_FALSE;
  
  if (isOpen) {
    // Let the child menu try to handle it.
    mCurrentMenu->KeyboardNavigation(aDirection, aHandledFlag);
  }

  if (aHandledFlag)
    return NS_OK;

  if (aDirection == NS_VK_RIGHT || aDirection == NS_VK_LEFT) {
    
    nsIMenuFrame* nextItem;
    
    if (aDirection == NS_VK_RIGHT)
      GetNextMenuItem(mCurrentMenu, &nextItem);
    else GetPreviousMenuItem(mCurrentMenu, &nextItem);

    SetCurrentMenuItem(nextItem);
    if (nextItem) {
      PRBool nextIsOpen;
      nextItem->MenuIsOpen(nextIsOpen);
      if (nextIsOpen) {
        // Select the first item.
        nextItem->SelectFirstItem();
      }
    }
  }
  else if (aDirection == NS_VK_UP || aDirection == NS_VK_DOWN) {
    // Open the menu and select its first item.
    mCurrentMenu->OpenMenu(PR_TRUE);
    mCurrentMenu->SelectFirstItem();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::GetNextMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult)
{
  nsIFrame* immediateParent = nsnull;
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  GetInsertionPoint(shell, this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsIFrame* currFrame = nsnull;
  nsIFrame* startFrame = nsnull;
  if (aStart) {
    aStart->QueryInterface(NS_GET_IID(nsIFrame), (void**)&currFrame); 
    if (currFrame) {
      startFrame = currFrame;
      currFrame->GetNextSibling(&currFrame);
    }
  }
  else 
    immediateParent->FirstChild(mPresContext,
                                nsnull,
                                &currFrame);

  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));

    // See if it's a menu item.
    if (IsValidItem(current)) {
      nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(currFrame);
      *aResult = menuFrame.get();
      NS_IF_ADDREF(*aResult);
      return NS_OK;
    }
    currFrame->GetNextSibling(&currFrame);
  }

  immediateParent->FirstChild(mPresContext,
                              nsnull,
                              &currFrame);

  // Still don't have anything. Try cycling from the beginning.
  while (currFrame && currFrame != startFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(currFrame);
      *aResult = menuFrame.get();
      NS_IF_ADDREF(*aResult);
      return NS_OK;
    }

    currFrame->GetNextSibling(&currFrame);
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::GetPreviousMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult)
{
  nsIFrame* immediateParent = nsnull;
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  GetInsertionPoint(shell, this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsIFrame* first;
  immediateParent->FirstChild(mPresContext,
                              nsnull, &first);
  nsFrameList frames(first);
                              
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
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));

    // See if it's a menu item.
    if (IsValidItem(current)) {
      nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(currFrame);
      *aResult = menuFrame.get();
      NS_IF_ADDREF(*aResult);
      return NS_OK;
    }
    currFrame = frames.GetPrevSiblingFor(currFrame);
  }

  currFrame = frames.LastChild();

  // Still don't have anything. Try cycling from the end.
  while (currFrame && currFrame != startFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(currFrame);
      *aResult = menuFrame.get();
      NS_IF_ADDREF(*aResult);
      return NS_OK;
    }

    currFrame = frames.GetPrevSiblingFor(currFrame);
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  return NS_OK;
}

NS_IMETHODIMP nsMenuBarFrame::GetCurrentMenuItem(nsIMenuFrame** aResult)
{
  *aResult = mCurrentMenu;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


NS_IMETHODIMP nsMenuBarFrame::SetCurrentMenuItem(nsIMenuFrame* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  PRBool wasOpen = PR_FALSE;
  
  // Unset the current child.
  if (mCurrentMenu) {
    mCurrentMenu->MenuIsOpen(wasOpen);
    mCurrentMenu->SelectMenu(PR_FALSE);
    if (wasOpen)
      mCurrentMenu->OpenMenu(PR_FALSE);
  }

  // Set the new child.
  if (aMenuItem) {
    aMenuItem->SelectMenu(PR_TRUE);
    aMenuItem->MarkAsGenerated(); // Have the menu building. Get it ready to be shown.

    if (wasOpen)
      aMenuItem->OpenMenu(PR_TRUE);
  }

  mCurrentMenu = aMenuItem;

  return NS_OK;
}


NS_IMETHODIMP 
nsMenuBarFrame::Escape(PRBool& aHandledFlag)
{
  if (!mCurrentMenu)
    return NS_OK;

  // See if our menu is open.
  PRBool isOpen = PR_FALSE;
  mCurrentMenu->MenuIsOpen(isOpen);
  if (isOpen) {
    // Let the child menu handle this.
    aHandledFlag = PR_FALSE;
    mCurrentMenu->Escape(aHandledFlag);
    if (!aHandledFlag) {
      // Close up this menu but keep our current menu item
      // designation.
      mCurrentMenu->OpenMenu(PR_FALSE);
    }
	if (nsMenuFrame::sDismissalListener)
      nsMenuFrame::sDismissalListener->Unregister();
    return NS_OK;
  }

  // It's us. Just set our active flag to false.
  mIsActive = PR_FALSE;

  // Clear our current menu item if we've got one.
  SetCurrentMenuItem(nsnull);

  // Remove our keyboard navigator
  RemoveKeyboardNavigator();

  // Clear out our dismissal listener
  if (nsMenuFrame::sDismissalListener)
    nsMenuFrame::sDismissalListener->Unregister();

  return NS_OK;
}

NS_IMETHODIMP 
nsMenuBarFrame::Enter()
{
  if (!mCurrentMenu)
    return NS_OK;

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
nsMenuBarFrame::HideChain()
{
  // Stop capturing rollups
  // (must do this during Hide, which happens before the menu item is executed,
  // since this reinstates normal event handling.)
  if (nsMenuFrame::sDismissalListener)
    nsMenuFrame::sDismissalListener->Unregister();

  if (mCurrentMenu) {
    mCurrentMenu->ActivateMenu(PR_FALSE);
    mCurrentMenu->SelectMenu(PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::DismissChain()
{
  // Stop capturing rollups
  if (nsMenuFrame::sDismissalListener)
    nsMenuFrame::sDismissalListener->Unregister();
  
  SetCurrentMenuItem(nsnull);
  SetActive(PR_FALSE);
  return NS_OK;
}


NS_IMETHODIMP
nsMenuBarFrame :: KillPendingTimers ( )
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

#if DONT_WANT_TO_DO_THIS
  // Get parent view
  nsIView * view = nsnull;
  nsMenuPopupFrame::GetNearestEnclosingView(mPresContext, this, &view);
  if (!view)
    return NS_OK;

  view->GetWidget(*aWidget);
#endif
}

NS_IMETHODIMP
nsMenuBarFrame::CreateDismissalListener()
{
  NS_ADDREF(nsMenuFrame::sDismissalListener = new nsMenuDismissalListener());
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::InstallKeyboardNavigator()
{
  if (mKeyboardNavigator)
    return NS_OK;

  mKeyboardNavigator = new nsMenuListener(this);
  NS_IF_ADDREF(mKeyboardNavigator);

  mTarget->AddEventListener(NS_ConvertASCIItoUCS2("keypress"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE); 
  mTarget->AddEventListener(NS_ConvertASCIItoUCS2("keydown"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);  
  mTarget->AddEventListener(NS_ConvertASCIItoUCS2("keyup"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);   
  
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::RemoveKeyboardNavigator()
{
  if (!mKeyboardNavigator || mIsActive)
    return NS_OK;

  mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keypress"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keydown"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keyup"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  
  NS_IF_RELEASE(mKeyboardNavigator);

  return NS_OK;
}

// helpers ///////////////////////////////////////////////////////////

PRBool 
nsMenuBarFrame::IsValidItem(nsIContent* aContent)
{
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));
  if (tag && (tag.get() == nsXULAtoms::menu ||
              tag.get() == nsXULAtoms::menuitem) &&
      !IsDisabled(aContent))
      return PR_TRUE;

  return PR_FALSE;
}

PRBool 
nsMenuBarFrame::IsDisabled(nsIContent* aContent)
{
  nsString disabled;
  aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled);
  if (disabled.Equals(NS_LITERAL_STRING("true")))
    return PR_TRUE;
  return PR_FALSE;
}

NS_IMETHODIMP
nsMenuBarFrame::Destroy(nsIPresContext* aPresContext)
{
  mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keypress"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE); 
  mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keydown"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);  
  mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("keyup"), (nsIDOMKeyListener*)mMenuBarListener, PR_FALSE);

  mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("mousedown"), (nsIDOMMouseListener*)mMenuBarListener, PR_FALSE);
  mTarget->RemoveEventListener(NS_ConvertASCIItoUCS2("blur"), (nsIDOMFocusListener*)mMenuBarListener, PR_TRUE);

  NS_IF_RELEASE(mMenuBarListener);

  return nsBoxFrame::Destroy(aPresContext);
}


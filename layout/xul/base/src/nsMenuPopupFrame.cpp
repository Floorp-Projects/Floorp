/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nsMenuPopupFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsMenuFrame.h"
#include "nsIPopupSetFrame.h"

const PRInt32 kMaxZ = 0x7fffffff; //XXX: Shouldn't there be a define somewhere for MaxInt for PRInt32

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

// NS_NewMenuPopupFrame
//
// Wrapper for creating a new menu popup container
//
nsresult
NS_NewMenuPopupFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMenuPopupFrame* it = new nsMenuPopupFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuPopupFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuPopupFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP nsMenuPopupFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(nsIMenuParent::GetIID())) {                                         
    *aInstancePtr = (void*)(nsIMenuParent*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);                                     
}


//
// nsMenuPopupFrame cntr
//
nsMenuPopupFrame::nsMenuPopupFrame()
:mCurrentMenu(nsnull)
{

} // cntr

NS_IMETHODIMP
nsMenuPopupFrame::Init(nsIPresContext&  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIStyleContext* aContext,
                       nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // We default to being vertical.
  nsString value;
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value);
  mHorizontal = PR_FALSE;
  if (value.EqualsIgnoreCase("vertical"))
    mHorizontal = PR_FALSE;
  else if (value.EqualsIgnoreCase("horizontal"))
    mHorizontal = PR_TRUE;

  CreateViewForFrame(aPresContext, this, aContext, PR_TRUE);

  // Now that we've made a view, remove it and insert it at the correct
  // position in the view hierarchy (as the root view).  We do this so that we
  // can draw the menus outside the confines of the window.
  nsIView* ourView;
  GetView(&ourView);

  nsIFrame* parent;
  aParent->GetParentWithView(&parent);
  nsIView* parentView;
  parent->GetView(&parentView);

  nsCOMPtr<nsIViewManager> viewManager;
  parentView->GetViewManager(*getter_AddRefs(viewManager));

  // Remove the view from its old position.
  viewManager->RemoveChild(parentView, ourView);

  // Reinsert ourselves as the root view with a maximum z-index.
  nsIView* rootView;
  viewManager->GetRootView(rootView);
  viewManager->InsertChild(rootView, ourView, kMaxZ);

  // XXX Hack. Change our transparency to be non-transparent
  // until the bug related to update of transparency on show/hide
  // is fixed.
  viewManager->SetViewContentTransparency(ourView, PR_FALSE);

  // Create a widget for ourselves.
  nsWidgetInitData widgetData;
  ourView->SetZIndex(kMaxZ);
  widgetData.mWindowType = eWindowType_popup;
  widgetData.mBorderStyle = eBorderStyle_default;
  static NS_DEFINE_IID(kCChildCID,  NS_CHILD_CID);
  ourView->CreateWidget(kCChildCID,
                     &widgetData,
                     nsnull);

  // Register a listener so that we know when the mouse enters
  // the menu (have to use moves, since enter is completely flaky).
  
  // Create the menu bar listener.
  /*mMenuPopupEntryListener = new nsMenuPopupEntryListener(this);

  nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(mContent);
  nsIDOMEventListener* domEventListener = (nsIDOMMouseMotionListener*)mMenuPopupEntryListener;
  target->AddEventListener("mousemove", domEventListener, PR_FALSE); 
	*/
  
  return rv;
}

void
nsMenuPopupFrame::GetViewOffset(nsIViewManager* aManager, nsIView* aView, 
  nsPoint& aPoint)
{
  aPoint.x = 0;
  aPoint.y = 0;
 
  nsIView *parent;
  nsRect bounds;

  parent = aView;
  while (nsnull != parent) {
    parent->GetBounds(bounds);
    aPoint.x += bounds.x;
    aPoint.y += bounds.y;
    parent->GetParent(parent);
  }
}

void
nsMenuPopupFrame::GetNearestEnclosingView(nsIFrame* aStartFrame, nsIView** aResult)
{
  *aResult = nsnull;
  aStartFrame->GetView(aResult);
  if (!*aResult) {
    nsIFrame* parent;
    aStartFrame->GetParentWithView(&parent);
    if (parent)
      parent->GetView(aResult);
  }
}

nsresult 
nsMenuPopupFrame::SyncViewWithFrame(nsIPresContext& aPresContext,
                                    PRBool aOnMenuBar, 
                                    nsIFrame* aFrame, 
                                    PRInt32 aXPos, PRInt32 aYPos)
{
  nsPoint parentPos;
  nsCOMPtr<nsIViewManager> viewManager;

  //Get the nearest enclosing parent view to aFrame.
  nsIView* parentView = nsnull;
  GetNearestEnclosingView(aFrame, &parentView);
  if (!parentView)
    return NS_OK;

  parentView->GetViewManager(*getter_AddRefs(viewManager));
  GetViewOffset(viewManager, parentView, parentPos);
  nsIView* view = nsnull;
  GetView(&view);

  nsIView* containingView = nsnull;
  nsPoint offset;
  aFrame->GetOffsetFromView(offset, &containingView);
  
  nsRect parentRect;
  aFrame->GetRect(parentRect);

  viewManager->ResizeView(view, mRect.width, mRect.height);
  if (aXPos != -1 || aYPos != -1) {
    // Convert the screen coords to twips
    float p2t;
    aPresContext.GetScaledPixelsToTwips(&p2t);
    PRInt32 xpos = NSIntPixelsToTwips(aXPos, p2t);
    PRInt32 ypos = NSIntPixelsToTwips(aYPos, p2t);
    viewManager->MoveViewTo(view, xpos, ypos);
  }

  else if (aOnMenuBar)
    viewManager->MoveViewTo(view, parentPos.x + offset.x, parentPos.y + parentRect.height + offset.y );
  else viewManager->MoveViewTo(view, parentPos.x + parentRect.width + offset.x, parentPos.y + offset.y );

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::DidReflow(nsIPresContext& aPresContext,
                            nsDidReflowStatus aStatus)
{
  nsresult rv = nsBoxFrame::DidReflow(aPresContext, aStatus);
  return rv;
}

NS_IMETHODIMP
nsMenuPopupFrame::GetNextMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult)
{
  nsIFrame* currFrame = nsnull;
  nsIFrame* startFrame = nsnull;
  if (aStart) {
    aStart->QueryInterface(kIFrameIID, (void**)&currFrame); 
    if (currFrame) {
      startFrame = currFrame;
      currFrame->GetNextSibling(&currFrame);
    }
  }
  else currFrame = mFrames.FirstChild();
  
  nsIFrame* start = currFrame;
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

  currFrame = mFrames.FirstChild();

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
nsMenuPopupFrame::GetPreviousMenuItem(nsIMenuFrame* aStart, nsIMenuFrame** aResult)
{
  nsIFrame* currFrame = nsnull;
  nsIFrame* startFrame = nsnull;
  if (aStart) {
    aStart->QueryInterface(kIFrameIID, (void**)&currFrame);
    if (currFrame) {
      startFrame = currFrame;
      currFrame = mFrames.GetPrevSiblingFor(currFrame);
    }
  }
  else currFrame = mFrames.LastChild();

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
    currFrame = mFrames.GetPrevSiblingFor(currFrame);
  }

  currFrame = mFrames.LastChild();

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

    currFrame = mFrames.GetPrevSiblingFor(currFrame);
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  return NS_OK;
}

NS_IMETHODIMP nsMenuPopupFrame::SetCurrentMenuItem(nsIMenuFrame* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;
  
  // Unset the current child.
  if (mCurrentMenu) {
    PRBool isOpen = PR_FALSE;
    mCurrentMenu->MenuIsOpen(isOpen);
    if (isOpen)
      mCurrentMenu->OpenMenu(PR_FALSE);
    mCurrentMenu->SelectMenu(PR_FALSE);
  }

  // Set the new child.
  if (aMenuItem) {
    aMenuItem->SelectMenu(PR_TRUE);
  }

  mCurrentMenu = aMenuItem;

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::CaptureMouseEvents(PRBool aGrabMouseEvents)
{
  // get its view
  nsIView* view = nsnull;
  GetView(&view);
  nsCOMPtr<nsIViewManager> viewMan;
  PRBool result;

  nsCOMPtr<nsIWidget> widget;

  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));
    if (viewMan) {
      view->GetWidget(*getter_AddRefs(widget));
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
        mIsCapturingMouseEvents = PR_TRUE;
        //widget->CaptureMouse(PR_TRUE);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
        mIsCapturingMouseEvents = PR_FALSE;
        //widget->CaptureMouse(PR_FALSE);
      }
    }
  }

  return NS_OK;
}

void
nsMenuPopupFrame::Escape(PRBool& aHandledFlag)
{
  if (!mCurrentMenu)
    return;

  // See if our menu is open.
  PRBool isOpen = PR_FALSE;
  mCurrentMenu->MenuIsOpen(isOpen);
  if (isOpen) {
    // Let the child menu handle this.
    mCurrentMenu->Escape(aHandledFlag);
    if (!aHandledFlag) {
      // We should close up.
      mCurrentMenu->OpenMenu(PR_FALSE);
      aHandledFlag = PR_TRUE;
    }
    return;
  }
}

void
nsMenuPopupFrame::Enter()
{
  // Give it to the child.
  if (mCurrentMenu)
    mCurrentMenu->Enter();
}

nsIMenuFrame*
nsMenuPopupFrame::FindMenuWithShortcut(PRUint32 aLetter)
{
  // Enumerate over our list of frames.
  nsIFrame* currFrame = mFrames.FirstChild();
  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      // Get the shortcut attribute.
      nsString shortcutKey = "";
      current->GetAttribute(kNameSpaceID_None, nsXULAtoms::accesskey, shortcutKey);
      shortcutKey.ToUpperCase();
      if (shortcutKey.Length() > 0) {
        // We've got something.
        PRUnichar shortcutChar = shortcutKey.CharAt(0);
        if (shortcutChar == aLetter) {
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

void 
nsMenuPopupFrame::ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag)
{
  if (mCurrentMenu) {
    PRBool isOpen = PR_FALSE;
    mCurrentMenu->MenuIsOpen(isOpen);
    if (isOpen) {
      // No way this applies to us. Give it to our child.
      mCurrentMenu->ShortcutNavigation(aLetter, aHandledFlag);
      return;
    }
  }

  // This applies to us. Let's see if one of the shortcuts applies
  nsIMenuFrame* result = FindMenuWithShortcut(aLetter);
  if (result) {
    // We got one!
    aHandledFlag = PR_TRUE;
    SetCurrentMenuItem(result);
    result->OpenMenu(PR_TRUE);
    result->SelectFirstItem();

    // XXX For menu items, do an execution of the oncommand handler!
    // Still needed or did I do this already? I'm going senile. - Dave
  }
}

void
nsMenuPopupFrame::KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag)
{
  // This method only gets called if we're open.
  if (!mCurrentMenu && (aDirection == NS_VK_RIGHT || aDirection == NS_VK_LEFT)) {
    // We've been opened, but we haven't had anything selected.
    // We can handle RIGHT, but our parent handles LEFT.
    if (aDirection == NS_VK_RIGHT) {
      nsIMenuFrame* nextItem;
      GetNextMenuItem(nsnull, &nextItem);
      if (nextItem) {
        aHandledFlag = PR_TRUE;
        SetCurrentMenuItem(nextItem);
      }
    }
    return;
  }

  PRBool isContainer = PR_FALSE;
  PRBool isOpen = PR_FALSE;
  if (mCurrentMenu) {
    mCurrentMenu->MenuIsContainer(isContainer);
    mCurrentMenu->MenuIsOpen(isOpen);

    if (isOpen) {
      // Give our child a shot.
      mCurrentMenu->KeyboardNavigation(aDirection, aHandledFlag);
    }
    else if (aDirection == NS_VK_RIGHT && isContainer) {
      // The menu is not yet open. Open it and select the first item.
      aHandledFlag = PR_TRUE;
      mCurrentMenu->OpenMenu(PR_TRUE);
      mCurrentMenu->SelectFirstItem();
    }
  }

  if (aHandledFlag)
    return; // The child menu took it for us.

  // For the vertical direction, we can move up or down.
  if (aDirection == NS_VK_UP || aDirection == NS_VK_DOWN) {
    
    nsIMenuFrame* nextItem;
    
    if (aDirection == NS_VK_DOWN)
      GetNextMenuItem(mCurrentMenu, &nextItem);
    else GetPreviousMenuItem(mCurrentMenu, &nextItem);

    SetCurrentMenuItem(nextItem);

    aHandledFlag = PR_TRUE;
  }
  else if (mCurrentMenu && isContainer && isOpen) {
    if (aDirection == NS_VK_LEFT) {
      // Close it up.
      mCurrentMenu->OpenMenu(PR_FALSE);
      aHandledFlag = PR_TRUE;
    }
  }
}

NS_IMETHODIMP
nsMenuPopupFrame::GetParentPopup(nsIMenuParent** aMenuParent)
{
  *aMenuParent = nsnull;
  nsIFrame* frame;
  GetParent(&frame);
  if (frame) {
    nsIFrame* grandparent;
    frame->GetParent(&grandparent);
    if (grandparent) {
      nsCOMPtr<nsIMenuParent> menuParent = do_QueryInterface(grandparent);
      if (menuParent) {
        *aMenuParent = menuParent.get();
        NS_ADDREF(*aMenuParent);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::HideChain()
{
  nsIFrame* frame;
  GetParent(&frame);
  if (frame) {
    nsCOMPtr<nsIPopupSetFrame> popupSetFrame = do_QueryInterface(frame);
    if (popupSetFrame) {
      // Destroy the popup.
      popupSetFrame->HidePopup();
      return NS_OK;
    }
    
    nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(frame);
    if (!menuFrame)
      return NS_OK;

    menuFrame->ActivateMenu(PR_FALSE);
    menuFrame->SelectMenu(PR_FALSE);

    // Get the parent.
    nsCOMPtr<nsIMenuParent> menuParent;
    menuFrame->GetMenuParent(getter_AddRefs(menuParent));
    if (menuParent)
      menuParent->HideChain();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::DismissChain()
{
  // Get our menu parent.
  nsIFrame* frame;
  GetParent(&frame);
  if (frame) {
    nsCOMPtr<nsIPopupSetFrame> popupSetFrame = do_QueryInterface(frame);
    if (popupSetFrame) {
      // Destroy the popup.
      popupSetFrame->DestroyPopup();
      return NS_OK;
    }

    nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(frame);
    if (!menuFrame)
      return NS_OK;
    
    menuFrame->OpenMenu(PR_FALSE);

    // Get the parent.
    nsCOMPtr<nsIMenuParent> menuParent;
    menuFrame->GetMenuParent(getter_AddRefs(menuParent));
    if (menuParent)
      menuParent->DismissChain();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::CreateDismissalListener()
{
  return NS_OK;
}

PRBool 
nsMenuPopupFrame::IsValidItem(nsIContent* aContent)
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
nsMenuPopupFrame::IsDisabled(nsIContent* aContent)
{
  nsString disabled = "";
  aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled);
  if (disabled == "true")
    return PR_TRUE;
  return PR_FALSE;
}

NS_IMETHODIMP 
nsMenuPopupFrame::HandleEvent(nsIPresContext& aPresContext, 
                              nsGUIEvent*     aEvent,
                              nsEventStatus&  aEventStatus)
{
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_IMETHODIMP
nsMenuPopupFrame::Destroy(nsIPresContext& aPresContext)
{
  //nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(mContent);
  //target->RemoveEventListener("mousemove", mMenuPopupEntryListener, PR_TRUE);
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsMenuPopupFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  nsRect rect;
  GetRect(rect);
  if (rect.Contains(aPoint)) {
    return nsBoxFrame::GetFrameForPoint(aPoint, aFrame);
  }
  
  *aFrame = this;
  return NS_OK;
}

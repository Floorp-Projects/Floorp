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

const PRInt32 kMaxZ = 0x7fffffff; //XXX: Shouldn't there be a define somewhere for MaxInt for PRInt32


//
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

  // XXX: Don't need? 
  // ourView->SetViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);

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

nsresult 
nsMenuPopupFrame::SyncViewWithFrame(PRBool aOnMenuBar)
{
  nsPoint parentPos;
  nsCOMPtr<nsIViewManager> viewManager;

     //Get parent frame
  nsIFrame* parent;
  GetParentWithView(&parent);
  NS_ASSERTION(parent, "GetParentWithView failed");

  // Get parent view
  nsIView* parentView = nsnull;
  parent->GetView(&parentView);

  parentView->GetViewManager(*getter_AddRefs(viewManager));
  GetViewOffset(viewManager, parentView, parentPos);
  nsIView* view = nsnull;
  GetView(&view);

  nsIView* containingView = nsnull;
  nsPoint offset;
  GetOffsetFromView(offset, &containingView);
  nsSize size;
  GetSize(size);
  
  nsIFrame* parentFrame;
  GetParent(&parentFrame);
  
  nsRect parentRect;
  parentFrame->GetRect(parentRect);

  viewManager->ResizeView(view, mRect.width, mRect.height);
  if (aOnMenuBar)
    viewManager->MoveViewTo(view, parentPos.x + offset.x, parentPos.y + parentRect.height + offset.y );
  else viewManager->MoveViewTo(view, parentPos.x + parentRect.width + offset.x, parentPos.y + offset.y );

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::DidReflow(nsIPresContext& aPresContext,
                            nsDidReflowStatus aStatus)
{
  nsresult rv = nsBoxFrame::DidReflow(aPresContext, aStatus);
  //SyncViewWithFrame();
  return rv;
}

NS_IMETHODIMP
nsMenuPopupFrame::GetNextMenuItem(nsIFrame* aStart, nsIFrame** aResult)
{
  nsIFrame* currFrame;
  if (aStart) {
    currFrame = aStart; 
    if (currFrame)
      currFrame->GetNextSibling(&currFrame);
  }
  else currFrame = mFrames.FirstChild();
  
  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));

    // See if it's a menu item.
    if (IsValidItem(current)) {
      *aResult = currFrame;
      return NS_OK;
    }
    currFrame->GetNextSibling(&currFrame);
  }

  currFrame = mFrames.FirstChild();

  // Still don't have anything. Try cycling from the beginning.
  while (currFrame && currFrame != aStart) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      *aResult = currFrame;
      return NS_OK;
    }

    currFrame->GetNextSibling(&currFrame);
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::GetPreviousMenuItem(nsIFrame* aStart, nsIFrame** aResult)
{
  nsIFrame* currFrame;
  if (aStart) {
    currFrame = aStart;
    if (currFrame)
      currFrame = mFrames.GetPrevSiblingFor(currFrame);
  }
  else currFrame = mFrames.LastChild();

  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));

    // See if it's a menu item.
    if (IsValidItem(current)) {
      *aResult = currFrame;
      return NS_OK;
    }
    currFrame = mFrames.GetPrevSiblingFor(currFrame);
  }

  currFrame = mFrames.LastChild();

  // Still don't have anything. Try cycling from the end.
  while (currFrame && currFrame != aStart) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      *aResult = currFrame;
      return NS_OK;
    }

    currFrame = mFrames.GetPrevSiblingFor(currFrame);
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  return NS_OK;
}

NS_IMETHODIMP nsMenuPopupFrame::SetCurrentMenuItem(nsIFrame* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
    
  // Unset the current child.
  if (mCurrentMenu) {
    if (menuFrame->IsOpen())
      menuFrame->OpenMenu(PR_FALSE);
    menuFrame->SelectMenu(PR_FALSE);
  }

  // Set the new child.
  if (aMenuItem) {
    nsMenuFrame* newFrame = (nsMenuFrame*)aMenuItem;
    newFrame->SelectMenu(PR_TRUE);
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

  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));
    if (viewMan) {
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
        mIsCapturingMouseEvents = PR_TRUE;
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
        mIsCapturingMouseEvents = PR_FALSE;
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
  nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
  if (menuFrame->IsOpen()) {
    // Let the child menu handle this.
    menuFrame->Escape(aHandledFlag);
    if (!aHandledFlag) {
      // We should close up.
      menuFrame->OpenMenu(PR_FALSE);
      aHandledFlag = PR_TRUE;
    }
    return;
  }
}

void
nsMenuPopupFrame::Enter()
{
  if (!mCurrentMenu)
    return;

  // Give it to the child.
  nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
  menuFrame->Enter();
}

nsIFrame*
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
          return currFrame;
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
    nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
    if (menuFrame->IsOpen()) {
      // No way this applies to us. Give it to our child.
      menuFrame->ShortcutNavigation(aLetter, aHandledFlag);
      return;
    }
  }

  // This applies to us. Let's see if one of the shortcuts applies
  nsIFrame* result = FindMenuWithShortcut(aLetter);
  if (result) {
    // We got one!
    aHandledFlag = PR_TRUE;
    nsMenuFrame* menuFrame = (nsMenuFrame*)result;
    SetCurrentMenuItem(result);
    menuFrame->OpenMenu(PR_TRUE);
    menuFrame->SelectFirstItem();

    // XXX For menu items, do an execution of the onclick handler!
  }
}

void
nsMenuPopupFrame::KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag)
{
  // This method only gets called if we're open.
  nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
  
  if (!mCurrentMenu && (aDirection == NS_VK_RIGHT || aDirection == NS_VK_LEFT)) {
    // We've been opened, but we haven't had anything selected.
    // We can handle RIGHT, but our parent handles LEFT.
    if (aDirection == NS_VK_RIGHT) {
      nsIFrame* nextItem;
      GetNextMenuItem(nsnull, &nextItem);
      if (nextItem) {
        aHandledFlag = PR_TRUE;
        SetCurrentMenuItem(nextItem);
      }
    }
    return;
  }

  if (menuFrame) {
    if (menuFrame->IsOpen()) {
      // Give our child a shot.
      menuFrame->KeyboardNavigation(aDirection, aHandledFlag);
    }
    else if (aDirection == NS_VK_RIGHT && menuFrame->IsMenu()) {
      // The menu is not yet open. Open it and select the first item.
      aHandledFlag = PR_TRUE;
      menuFrame->OpenMenu(PR_TRUE);
      menuFrame->SelectFirstItem();
    }
  }

  if (aHandledFlag)
    return; // The child menu took it for us.

  // For the vertical direction, we can move up or down.
  if (aDirection == NS_VK_UP || aDirection == NS_VK_DOWN) {
    
    nsIFrame* nextItem;
    
    if (aDirection == NS_VK_DOWN)
      GetNextMenuItem(mCurrentMenu, &nextItem);
    else GetPreviousMenuItem(mCurrentMenu, &nextItem);

    SetCurrentMenuItem(nextItem);

    aHandledFlag = PR_TRUE;
  }
  else if (mCurrentMenu && menuFrame->IsMenu() && menuFrame->IsOpen()) {
    if (aDirection == NS_VK_LEFT) {
      // Close it up.
      menuFrame->OpenMenu(PR_FALSE);
      aHandledFlag = PR_TRUE;
    }
  }
}

NS_IMETHODIMP
nsMenuPopupFrame::DismissChain()
{
  // Get our menu parent.
  nsIFrame* frame;
  GetParent(&frame);
  if (frame) {
    nsMenuFrame* menuFrame = (nsMenuFrame*)frame;
    menuFrame->OpenMenu(PR_FALSE);

    // Get the parent.
    nsCOMPtr<nsIMenuParent> menuParent;
    menuFrame->GetMenuParent(getter_AddRefs(menuParent));
    if (menuParent)
      menuParent->DismissChain();
  }

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

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

  // Create a widget for ourselves.
  nsWidgetInitData widgetData;
  ourView->SetZIndex(kMaxZ);
  widgetData.mBorderStyle = eBorderStyle_BorderlessTopLevel;
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
nsMenuPopupFrame::GetNextMenuItem(nsIContent* aStart, nsIContent** aResult)
{
  PRInt32 index = 0;
  if (aStart) {
    // Determine the index of start.
    mContent->IndexOf(aStart, index);
    index++;
  }

  PRInt32 count;
  mContent->ChildCount(count);

  // Begin the search from index.
  PRInt32 i;
  for (i = index; i < count; i++) {
    nsCOMPtr<nsIContent> current;
    mContent->ChildAt(i, *getter_AddRefs(current));
    
    // See if it's a menu item.
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::xpmenu) {
      *aResult = current;
      NS_IF_ADDREF(*aResult);
      return NS_OK;
    }
  }

  // Still don't have anything. Try cycling from the beginning.
  for (i = 0; i <= index; i++) {
    nsCOMPtr<nsIContent> current;
    mContent->ChildAt(i, *getter_AddRefs(current));
    
    // See if it's a menu item.
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::xpmenu) {
      *aResult = current;
      NS_IF_ADDREF(*aResult);
      return NS_OK;
    }
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  NS_IF_ADDREF(aStart);

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::GetPreviousMenuItem(nsIContent* aStart, nsIContent** aResult)
{
  PRInt32 count;
  mContent->ChildCount(count);

  PRInt32 index = count-1;
  if (aStart) {
    // Determine the index of start.
    mContent->IndexOf(aStart, index);
    index--;
  }

  
  // Begin the search from index.
  PRInt32 i;
  for (i = index; i >= 0; i--) {
    nsCOMPtr<nsIContent> current;
    mContent->ChildAt(i, *getter_AddRefs(current));
    
    // See if it's a menu item.
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::xpmenu) {
      *aResult = current;
      NS_IF_ADDREF(*aResult);
      return NS_OK;
    }
  }

  // Still don't have anything. Try cycling from the beginning.
  for (i = count-1; i >= index; i--) {
    nsCOMPtr<nsIContent> current;
    mContent->ChildAt(i, *getter_AddRefs(current));
    
    // See if it's a menu item.
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::xpmenu) {
      *aResult = current;
      NS_IF_ADDREF(*aResult);
      return NS_OK;
    }
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  NS_IF_ADDREF(aStart);

  return NS_OK;
}

NS_IMETHODIMP nsMenuPopupFrame::SetCurrentMenuItem(nsIContent* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  // Unset the current child.
  if (mCurrentMenu) {
    //printf("Unsetting current child.\n");
    mCurrentMenu->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
  }

  // Set the new child.
  if (aMenuItem) {
    //printf("Setting new child.\n");
    aMenuItem->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, "true", PR_TRUE);
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


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 */

#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsPopupSetFrame.h"
#include "nsIMenuParent.h"
#include "nsMenuFrame.h"
#include "nsBoxFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsMenuBarFrame.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"
#include "nsISupportsArray.h"
#include "nsIDOMText.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsCSSFrameConstructor.h"

#define NS_MENU_POPUP_LIST_INDEX   0

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

//
// NS_NewPopupSetFrame
//
// Wrapper for creating a new menu popup container
//
nsresult
NS_NewPopupSetFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsPopupSetFrame* it = new (aPresShell) nsPopupSetFrame (aPresShell);
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsPopupSetFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsPopupSetFrame::Release(void)
{
    return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsPopupSetFrame)
  NS_INTERFACE_MAP_ENTRY(nsIPopupSetFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


//
// nsPopupSetFrame cntr
//
nsPopupSetFrame::nsPopupSetFrame(nsIPresShell* aShell):nsBoxFrame(aShell),
mPresContext(nsnull), 
mElementContent(nsnull), 
mCreateHandlerSucceeded(PR_FALSE),
mLastPref(-1,-1),
mFrameConstructor(nsnull)
{

} // cntr

nsIFrame*
nsPopupSetFrame::GetActiveChild()
{
  return mPopupFrames.FirstChild();
}

NS_IMETHODIMP
nsPopupSetFrame::Init(nsIPresContext*  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIStyleContext* aContext,
                     nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext; // Don't addref it.  Our lifetime is shorter.
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  return rv;
}

// The following methods are all overridden to ensure that the menupopup frames
// are placed in the appropriate list.
NS_IMETHODIMP
nsPopupSetFrame::FirstChild(nsIPresContext* aPresContext,
                            nsIAtom*        aListName,
                            nsIFrame**      aFirstChild) const
{
  if (nsLayoutAtoms::popupList == aListName) {
    *aFirstChild = mPopupFrames.FirstChild();
  } else {
    nsBoxFrame::FirstChild(aPresContext, aListName, aFirstChild);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;
  if (nsLayoutAtoms::popupList == aListName) {
    mPopupFrames.SetFrames(aChildList);
  } else {

    nsFrameList frames(aChildList);

    // We may have menupopups in here. Get them out, and move them into
    // the popup frame list.
    nsIFrame* frame = frames.FirstChild();
    while (frame) {
      nsCOMPtr<nsIContent> content;
      frame->GetContent(getter_AddRefs(content));
      nsCOMPtr<nsIAtom> tag;
      content->GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsXULAtoms::popup) {
        // Remove this frame from the list and place it in the other list.
        frames.RemoveFrame(frame);
        mPopupFrames.AppendFrame(this, frame);
        nsIFrame* first = frames.FirstChild();
        rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, first);
        return rv;
      }
      frame->GetNextSibling(&frame);
    }

    // Didn't find it.
    rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }
  return rv;
}

NS_IMETHODIMP
nsPopupSetFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const
{
  // Maintain a separate child list for the menu contents.
  // This is necessary because we don't want the menu contents to be included in the layout
  // of the menu's single item because it would take up space, when it is supposed to
  // be floating above the display.

  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  
  *aListName = nsnull;

  // don't expose the child frame list, it slows things down
#if 0
  if (NS_MENU_POPUP_LIST_INDEX == aIndex) {
    *aListName = nsLayoutAtoms::popupList;
    NS_ADDREF(*aListName);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::Destroy(nsIPresContext* aPresContext)
{
  // Remove our frame mappings
  if (mFrameConstructor) {
    nsIFrame* curFrame = mPopupFrames.FirstChild();
    while (curFrame) {
      mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, curFrame, nsnull);
      curFrame->GetNextSibling(&curFrame);
    }
  }

   // Cleanup frames in popup child list
  mPopupFrames.DestroyFrames(aPresContext);
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsPopupSetFrame::DoLayout(nsBoxLayoutState& aState)
{
  // lay us out
  nsresult rv = nsBoxFrame::DoLayout(aState);

  // layout the popup. First we need to get it.
  nsIFrame* popupChild = GetActiveChild();
  if (popupChild) {
    nsIBox* ibox = nsnull;
    nsresult rv2 = popupChild->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox);
    NS_ASSERTION(NS_SUCCEEDED(rv2) && ibox,"popupChild is not box!!");

    // then get its preferred size
    nsSize prefSize(0,0);
    nsSize minSize(0,0);
    nsSize maxSize(0,0);

    ibox->GetPrefSize(aState, prefSize);
    ibox->GetMinSize(aState, minSize);
    ibox->GetMaxSize(aState, maxSize);

    BoundsCheck(minSize, prefSize, maxSize);

 // if the pref size changed then set bounds to be the pref size
    // and sync the view. And set new pref size.
    if (mLastPref != prefSize) {
      ibox->SetBounds(aState, nsRect(0,0,prefSize.width, prefSize.height));
      RePositionPopup(aState);
      mLastPref = prefSize;
    }

    // is the new size too small? Make sure we handle scrollbars correctly
    nsIBox* child;
    ibox->GetChildBox(&child);

    nsRect bounds(0,0,0,0);
    ibox->GetBounds(bounds);

    nsCOMPtr<nsIScrollableFrame> scrollframe = do_QueryInterface(child);
    if (scrollframe) {
      nsIScrollableFrame::nsScrollPref pref;
      scrollframe->GetScrollPreference(aState.GetPresContext(), &pref);

      if (pref == nsIScrollableFrame::Auto)  
      {
        // if our pref height
        if (bounds.height < prefSize.height) {
           // layout the child
           ibox->Layout(aState);

           nscoord width;
           nscoord height;
           scrollframe->GetScrollbarSizes(aState.GetPresContext(), &width, &height);
           if (bounds.width < prefSize.width + width)
           {
             bounds.width += width;
             //printf("Width=%d\n",width);
             ibox->SetBounds(aState, bounds);
           }
        }
      }
    }
    
    // layout the child
    ibox->Layout(aState);

    // only size popup if open
    if (mCreateHandlerSucceeded) {
      nsIView* view = nsnull;
      popupChild->GetView(aState.GetPresContext(), &view);
      nsCOMPtr<nsIViewManager> viewManager;
      view->GetViewManager(*getter_AddRefs(viewManager));
      viewManager->ResizeView(view, bounds.width, bounds.height);
      viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
    }
  }

  SyncLayout(aState);

  return rv;
}


NS_IMETHODIMP
nsPopupSetFrame::SetDebug(nsBoxLayoutState& aState, PRBool aDebug)
{
  // see if our state matches the given debug state
  PRBool debugSet = mState & NS_STATE_CURRENTLY_IN_DEBUG;
  PRBool debugChanged = (!aDebug && debugSet) || (aDebug && !debugSet);

  // if it doesn't then tell each child below us the new debug state
  if (debugChanged)
  {
      nsBoxFrame::SetDebug(aState, aDebug);
      SetDebug(aState, mPopupFrames.FirstChild(), aDebug);
  }

  return NS_OK;
}

nsresult
nsPopupSetFrame::SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, PRBool aDebug)
{
      if (!aList)
          return NS_OK;

      while (aList) {
          nsIBox* ibox = nsnull;
          if (NS_SUCCEEDED(aList->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) {
              ibox->SetDebug(aState, aDebug);
          }

          aList->GetNextSibling(&aList);
      }

      return NS_OK;
}



void
nsPopupSetFrame::RePositionPopup(nsBoxLayoutState& aState)
{
  // Sync up the view.
  nsIFrame* activeChild = GetActiveChild();
  if (activeChild && mElementContent) {

    nsCOMPtr<nsIContent> menuPopupContent;
    activeChild->GetContent(getter_AddRefs(menuPopupContent));
    nsAutoString popupAnchor, popupAlign;
      
    menuPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::popupanchor, popupAnchor);
    menuPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::popupalign, popupAlign);

    if (popupAnchor.IsEmpty())
      popupAnchor.AssignWithConversion("bottomleft");
    if (popupAlign.IsEmpty())
      popupAlign.AssignWithConversion("topleft");
   
    nsIFrame* frameToSyncTo = nsnull;
    nsCOMPtr<nsIPresShell> presShell;
    nsIPresContext* presContext = aState.GetPresContext();
    presContext->GetShell(getter_AddRefs(presShell));
    presShell->GetPrimaryFrameFor ( mElementContent, &frameToSyncTo );
    ((nsMenuPopupFrame*)activeChild)->SyncViewWithFrame(presContext, popupAnchor, popupAlign, frameToSyncTo, mXPos, mYPos);
  }
}

NS_IMETHODIMP
nsPopupSetFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
  nsresult  rv;
  
  if (mPopupFrames.ContainsFrame(aOldFrame)) {
    // Go ahead and remove this frame.
    mPopupFrames.DestroyFrame(aPresContext, aOldFrame);
    nsBoxLayoutState state(aPresContext);
    rv = MarkDirtyChildren(state);
  } else {
    rv = nsBoxFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsPopupSetFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{

  nsCOMPtr<nsIContent> frameChild;
  aFrameList->GetContent(getter_AddRefs(frameChild));
  nsCOMPtr<nsIAtom> tag;
  nsresult          rv;
  frameChild->GetTag(*getter_AddRefs(tag));
  if (tag && tag.get() == nsXULAtoms::popup) {
    mPopupFrames.InsertFrames(nsnull, nsnull, aFrameList);
    nsBoxLayoutState state(aPresContext);
    SetDebug(state, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
    rv = MarkDirtyChildren(state);
  } else {
    rv = nsBoxFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList);  
  }

  return rv;
}

NS_IMETHODIMP
nsPopupSetFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  if (!aFrameList)
    return NS_OK;

  nsCOMPtr<nsIContent> frameChild;
  aFrameList->GetContent(getter_AddRefs(frameChild));

  nsCOMPtr<nsIAtom> tag;
  nsresult          rv;
  
  frameChild->GetTag(*getter_AddRefs(tag));
  if (tag && tag.get() == nsXULAtoms::popup) {
    mPopupFrames.AppendFrames(nsnull, aFrameList);
    nsBoxLayoutState state(aPresContext);
    SetDebug(state, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
    rv = MarkDirtyChildren(state);
  } else {
    rv = nsBoxFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList); 
  }

  return rv;
}

NS_IMETHODIMP
nsPopupSetFrame::CreatePopup(nsIContent* aElementContent, nsIContent* aPopupContent, 
                             PRInt32 aXPos, PRInt32 aYPos, 
                             const nsString& aPopupType, const nsString& anAnchorAlignment,
                             const nsString& aPopupAlignment)
{
  // Cache the element content we're supposed to sync to
  mPopupType = aPopupType;
  mElementContent = aElementContent;
  
  // Show the popup at the specified position.
  mXPos = aXPos;
  mYPos = aYPos;

  if (!OnCreate(aPopupContent))
    return NS_OK;

#ifdef DEBUG_PINK
  printf("X Pos: %d\n", mXPos);
  printf("Y Pos: %d\n", mYPos);
#endif

  // Generate the popup.
  mCreateHandlerSucceeded = PR_TRUE;
  MarkAsGenerated(aPopupContent);

  // determine if this menu is a context menu and flag it
  nsIFrame* activeChild = GetActiveChild();
  nsCOMPtr<nsIMenuParent> childPopup ( do_QueryInterface(activeChild) );
  if ( childPopup && aPopupType == NS_LITERAL_STRING("context") )
    childPopup->SetIsContextMenu(PR_TRUE);

  // Now we'll have it in our child frame list.
  
  // Now open the popup.
  OpenPopup(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::HidePopup()
{
  if ( mCreateHandlerSucceeded )
    ActivatePopup(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::DestroyPopup()
{
  if ( mCreateHandlerSucceeded ) {    // ensure the popup was created before we try to destroy it
    OpenPopup(PR_FALSE);
    mPopupType.SetLength(0);
  }

  // clear things out for next time
  mCreateHandlerSucceeded = PR_FALSE;
  mElementContent = nsnull;
  mXPos = mYPos = 0;
  mLastPref.width = -1;
  mLastPref.height = -1;

  return NS_OK;
}

void
nsPopupSetFrame::MarkAsGenerated(nsIContent* aPopupContent)
{
  // Ungenerate all other popups in the set. No more than one can exist
  // at any point in time.
  PRInt32 childCount;
  mContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> childContent;
    mContent->ChildAt(i, *getter_AddRefs(childContent));

    // Retrieve the menugenerated attribute.
    nsAutoString value;
    childContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::menugenerated, 
                               value);
    if (value == NS_LITERAL_STRING("true")) {
      // Ungenerate this element.
      childContent->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menugenerated,
                                   PR_TRUE);
    }
  }

  // Set our attribute, but only if we aren't already generated.
  // Retrieve the menugenerated attribute.
  nsAutoString value;
  aPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::menugenerated, 
                              value);
  if (value != NS_LITERAL_STRING("true")) {
    // Generate this element.
    aPopupContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::menugenerated, NS_LITERAL_STRING("true"),
                                PR_TRUE);
  }
}

void
nsPopupSetFrame::OpenPopup(PRBool aActivateFlag)
{
  if (aActivateFlag) {
    ActivatePopup(PR_TRUE);

    // register the rollup listeners, etc, but not if we're a tooltip
    nsIFrame* activeChild = GetActiveChild();
    nsCOMPtr<nsIMenuParent> childPopup = do_QueryInterface(activeChild);
    if ( mPopupType != NS_LITERAL_STRING("tooltip") )
      UpdateDismissalListener(childPopup);
    
    // First check and make sure this popup wants keyboard navigation
  
    nsCOMPtr<nsIContent> content;
    GetContent(getter_AddRefs(content));
    nsAutoString property;    
    // Tooltips don't get keyboard navigation
    content->GetAttribute(kNameSpaceID_None, nsXULAtoms::ignorekeys, property);
    if ( property != NS_LITERAL_STRING("true") && 
           childPopup &&
          mPopupType != NS_LITERAL_STRING("tooltip") )
      childPopup->InstallKeyboardNavigator();

  }
  else {
    if (mCreateHandlerSucceeded && !OnDestroy())
      return;

    // Unregister, but not if we're a tooltip
    if ( mPopupType != NS_LITERAL_STRING("tooltip") ) {
      if (nsMenuFrame::mDismissalListener)
        nsMenuFrame::mDismissalListener->Unregister();
    }
    
    // Remove any keyboard navigators
    nsIFrame* activeChild = GetActiveChild();
    nsCOMPtr<nsIMenuParent> childPopup = do_QueryInterface(activeChild);
    if ( childPopup )
      childPopup->RemoveKeyboardNavigator();

    ActivatePopup(PR_FALSE);
  }
}

void
nsPopupSetFrame::ActivatePopup(PRBool aActivateFlag)
{
  nsCOMPtr<nsIContent> content;
  GetActiveChildElement(getter_AddRefs(content));
  if (content) {
    // When we sync the popup view with the frame, we'll show the popup if |menutobedisplayed|
    // is set by setting the |menuactive| attribute. This used to trip css into showing the menu
    // but now we do it ourselves. 
    if (aActivateFlag)
      content->SetAttribute(kNameSpaceID_None, nsXULAtoms::menutobedisplayed, NS_LITERAL_STRING("true"), PR_TRUE);
    else {
      content->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
      content->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menutobedisplayed, PR_TRUE);

      // make sure we hide the popup.
      nsIFrame* activeChild = GetActiveChild();
      nsIView* view = nsnull;
      activeChild->GetView(mPresContext, &view);
      nsCOMPtr<nsIViewManager> viewManager;
      view->GetViewManager(*getter_AddRefs(viewManager));
      viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
      viewManager->ResizeView(view, 0, 0);
    }
  }
}

PRBool
nsPopupSetFrame::OnCreate(nsIContent* aPopupContent)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_CREATE;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;

  if (aPopupContent) {
    nsCOMPtr<nsIPresShell> shell;
    nsresult rv = mPresContext->GetShell(getter_AddRefs(shell));
    if (NS_SUCCEEDED(rv) && shell) {
      rv = shell->HandleDOMEventWithTarget(aPopupContent, &event, &status);
    }
    if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
      return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool
nsPopupSetFrame::OnDestroy()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_DESTROY;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;

  nsCOMPtr<nsIContent> content;
  GetActiveChildElement(getter_AddRefs(content));
  
  if (content) {
    nsCOMPtr<nsIPresShell> shell;
    nsresult rv = mPresContext->GetShell(getter_AddRefs(shell));
    if (NS_SUCCEEDED(rv) && shell) {
      rv = shell->HandleDOMEventWithTarget(content, &event, &status);
    }
    if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
      return PR_FALSE;
  }
  return PR_TRUE;
}

void
nsPopupSetFrame::GetActiveChildElement(nsIContent** aResult)
{
  *aResult = nsnull;
  nsIFrame* child = GetActiveChild();
  if (child) {
    child->GetContent(aResult);
  }
}

void
nsPopupSetFrame::UpdateDismissalListener(nsIMenuParent* aMenuParent)
{
  if (!nsMenuFrame::mDismissalListener) {
    if (!aMenuParent)
       return;
    // Create the listener and attach it to the outermost window.
    aMenuParent->CreateDismissalListener();
  }
  
  // Make sure the menu dismissal listener knows what the current
  // innermost menu popup frame is.
  nsMenuFrame::mDismissalListener->SetCurrentMenuParent(aMenuParent);
}

NS_IMETHODIMP
nsPopupSetFrame::GetActiveChild(nsIDOMElement** aResult)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  if (!frame)
    return NS_ERROR_FAILURE;

  nsIMenuFrame* menuFrame;
  menuPopup->GetCurrentMenuItem(&menuFrame);
  
  if (!menuFrame) {
    *aResult = nsnull;
  }
  else {
    nsIFrame* f;
    menuFrame->QueryInterface(kIFrameIID, (void**)&f);
    nsCOMPtr<nsIContent> c;
    f->GetContent(getter_AddRefs(c));
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(c));
    *aResult = elt;
    NS_IF_ADDREF(*aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::SetActiveChild(nsIDOMElement* aChild)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  if (!frame)
    return NS_ERROR_FAILURE;

  if (!aChild) {
    // Remove the current selection
    menuPopup->SetCurrentMenuItem(nsnull);
    return NS_OK;
  }

  nsCOMPtr<nsIContent> child(do_QueryInterface(aChild));
  
  nsCOMPtr<nsIContent> par;
  child->GetParent(*getter_AddRefs(par));
  
  nsCOMPtr<nsIContent> menuPopupContent;
  menuPopup->GetContent(getter_AddRefs(menuPopupContent));
  if (menuPopupContent != par)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  nsIFrame* kid;
  shell->GetPrimaryFrameFor(child, &kid);
  if (!kid)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(kid));
  if (!menuFrame)
    return NS_ERROR_FAILURE;
  menuPopup->SetCurrentMenuItem(menuFrame);
  return NS_OK;
}

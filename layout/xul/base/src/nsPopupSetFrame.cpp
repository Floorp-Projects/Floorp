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

#define NS_MENU_POPUP_LIST_INDEX   (NS_AREA_FRAME_ABSOLUTE_LIST_INDEX + 1)

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
mPresContext(nsnull), mElementFrame(nsnull)
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
  /*NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  
  *aListName = nsnull;
  if (NS_MENU_POPUP_LIST_INDEX == aIndex) {
    *aListName = nsLayoutAtoms::popupList;
    NS_ADDREF(*aListName);
    return NS_OK;
  }*/
  return nsBoxFrame::GetAdditionalChildListName(aIndex, aListName);
}

NS_IMETHODIMP
nsPopupSetFrame::Destroy(nsIPresContext* aPresContext)
{
   // Cleanup frames in popup child list
  mPopupFrames.DestroyFrames(aPresContext);
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsPopupSetFrame::Reflow(nsIPresContext*   aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
    nsIFrame* popupChild = GetActiveChild();

    nsHTMLReflowState boxState(aReflowState);

    if (aReflowState.reason == eReflowReason_Incremental) {

        nsIFrame* incrementalChild;

        // get the child but don't pull it off
        aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
        
        // see if it is in the mPopupFrames list
        nsIFrame* child = mPopupFrames.FirstChild();
        popupChild = nsnull;

        while (nsnull != child) 
        { 
            // if it is then flow the popup incrementally then flow
            // us with a resize just to get our correct desired size.
            if (child == incrementalChild) {
                // pull it off now
                aReflowState.reflowCommand->GetNext(incrementalChild);

                // we know what child
                popupChild = child;

                // relow the box with resize just to get the
                // aDesiredSize set correctly
                boxState.reason = eReflowReason_Resize;
                break;
            }

            nsresult rv = child->GetNextSibling(&child);
            NS_ASSERTION(rv == NS_OK,"failed to get next child");
        }   
    } else if (aReflowState.reason == eReflowReason_Dirty) {
    // sometimes incrementals are converted to dirty. This is done in the case just above this. So lets check
    // to see if this was converted. If it was it will still have a reflow state.
    if (aReflowState.reflowCommand) {
        // it was converted so lets see if the next child is this one. If it is then convert it back and
        // pass it down.
        nsIFrame* incrementalChild = nsnull;
        aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
        if (incrementalChild == popupChild) 
        {
            nsHTMLReflowState state(aReflowState);
            state.reason = eReflowReason_Incremental;
            return Reflow(aPresContext, aDesiredSize, state, aStatus);
            
        } 
    }
  }

  // Handle reflowing our subordinate popup
  if (popupChild) {
      // Constrain the child's width and height to aAvailableWidth and aAvailableHeight
      nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, popupChild,
                                       availSize);
      kidReflowState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
      kidReflowState.mComputedHeight = NS_UNCONSTRAINEDSIZE;

       // Reflow child
      nsHTMLReflowMetrics kidDesiredSize(aDesiredSize);

      nsRect rect;
      popupChild->GetRect(rect);
      nsresult rv = ReflowChild(popupChild, aPresContext, kidDesiredSize, kidReflowState,
                                  rect.x, rect.y, NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_MOVE_CHILD_VIEWS, aStatus);

       // Set the child's width and height to its desired size
      FinishReflowChild(popupChild, aPresContext, kidDesiredSize, rect.x, rect.y,
                          NS_FRAME_NO_SIZE_VIEW |NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_MOVE_CHILD_VIEWS);
  }

  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, boxState, aStatus);

  return rv;
}

NS_IMETHODIMP
nsPopupSetFrame::SetDebug(nsIPresContext* aPresContext, PRBool aDebug)
{
  // see if our state matches the given debug state
  PRBool debugSet = mState & NS_STATE_CURRENTLY_IN_DEBUG;
  PRBool debugChanged = (!aDebug && debugSet) || (aDebug && !debugSet);

  // if it doesn't then tell each child below us the new debug state
  if (debugChanged)
  {
      nsBoxFrame::SetDebug(aPresContext, aDebug);
      SetDebug(aPresContext, mPopupFrames.FirstChild(), aDebug);
  }

  return NS_OK;
}

nsresult
nsPopupSetFrame::SetDebug(nsIPresContext* aPresContext, nsIFrame* aList, PRBool aDebug)
{
      if (!aList)
          return NS_OK;

      while (aList) {
          nsIBox* ibox = nsnull;
          if (NS_SUCCEEDED(aList->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) {
              ibox->SetDebug(aPresContext, aDebug);
          }

          aList->GetNextSibling(&aList);
      }

      return NS_OK;
}



NS_IMETHODIMP
nsPopupSetFrame::DidReflow(nsIPresContext* aPresContext,
                            nsDidReflowStatus aStatus)
{
  // Sync up the view.
  nsIFrame* activeChild = GetActiveChild();
  if (activeChild) {

    nsCOMPtr<nsIContent> menuPopupContent;
    activeChild->GetContent(getter_AddRefs(menuPopupContent));
    nsAutoString popupAnchor, popupAlign;
      
    menuPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::popupanchor, popupAnchor);
    menuPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::popupalign, popupAlign);

    if (popupAnchor.IsEmpty())
      popupAnchor = "bottomleft";
    if (popupAlign.IsEmpty())
      popupAlign = "topleft";
   
    ((nsMenuPopupFrame*)activeChild)->SyncViewWithFrame(aPresContext, popupAnchor, popupAlign, mElementFrame, mXPos, mYPos);
  }

  return nsBoxFrame::DidReflow(aPresContext, aStatus);
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
    rv = GenerateDirtyReflowCommand(aPresContext, aPresShell);
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
    SetDebug(aPresContext, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
    rv = GenerateDirtyReflowCommand(aPresContext, aPresShell);
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
    SetDebug(aPresContext, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
    rv = GenerateDirtyReflowCommand(aPresContext, aPresShell);
  } else {
    rv = nsBoxFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList); 
  }

  return rv;
}

NS_IMETHODIMP
nsPopupSetFrame::CreatePopup(nsIFrame* aElementFrame, nsIContent* aPopupContent, 
                             PRInt32 aXPos, PRInt32 aYPos, 
                             const nsString& aPopupType, const nsString& anAnchorAlignment,
                             const nsString& aPopupAlignment)
{
  // Cache the element frame.
  mElementFrame = aElementFrame;

  // Show the popup at the specified position.
  mXPos = aXPos;
  mYPos = aYPos;

  printf("X Pos: %d\n", mXPos);
  printf("Y Pos: %d\n", mYPos);

  if (!OnCreate(aPopupContent))
    return NS_OK;

  // Generate the popup.
  MarkAsGenerated(aPopupContent);

  // determine if this menu is a context menu and flag it
  nsIFrame* activeChild = GetActiveChild();
  nsCOMPtr<nsIMenuParent> childPopup ( do_QueryInterface(activeChild) );
  if ( childPopup && aPopupType.Equals("context") )
    childPopup->SetIsContextMenu(PR_TRUE);

  // Now we'll have it in our child frame list.
  
  // Now open the popup.
  OpenPopup(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::HidePopup()
{
  ActivatePopup(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::DestroyPopup()
{
  OpenPopup(PR_FALSE);
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
    if (value.Equals("true")) {
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
  if (!value.Equals("true")) {
    // Generate this element.
    aPopupContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::menugenerated, "true",
                                PR_TRUE);
  }
}

void
nsPopupSetFrame::OpenPopup(PRBool aActivateFlag)
{
  if (aActivateFlag) {
    ActivatePopup(PR_TRUE);

    nsIFrame* activeChild = GetActiveChild();
    nsCOMPtr<nsIMenuParent> childPopup = do_QueryInterface(activeChild);
    UpdateDismissalListener(childPopup);
    childPopup->InstallKeyboardNavigator();
  }
  else {
    if (!OnDestroy())
      return;

    // Unregister.
    if (nsMenuFrame::mDismissalListener) {
      nsMenuFrame::mDismissalListener->Unregister();
    }

    // Remove any keyboard navigators
    nsIFrame* activeChild = GetActiveChild();
    nsCOMPtr<nsIMenuParent> childPopup = do_QueryInterface(activeChild);
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
    // is set by setting the |menuactive| attribute. This trips CSS to make the view visible.
    // We wait until the last possible moment to show to avoid flashing, but we can just go
    // ahead and hide it here if we're told to (no additional stages necessary).
    if (aActivateFlag)
      content->SetAttribute(kNameSpaceID_None, nsXULAtoms::menutobedisplayed, "true", PR_TRUE);
    else {
      content->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
      content->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menutobedisplayed, PR_TRUE);
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
    nsresult rv = aPopupContent->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
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
    nsresult rv = content->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
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

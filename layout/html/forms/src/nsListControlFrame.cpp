/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsCOMPtr.h"
#include "nsListControlFrame.h"
#include "nsFormControlHelper.h"
#include "nsFormControlFrame.h"
#include "nsBlockReflowContext.h"
#include "nsBlockBandData.h"
#include "nsBulletFrame.h"
#include "nsLineBox.h"

#include "nsLineLayout.h"
#include "nsPlaceholderFrame.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIReflowCommand.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIFontMetrics.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLValue.h"
#include "nsDOMEvent.h"
#include "nsIHTMLContent.h"
#include "prprf.h"
#include "nsLayoutAtoms.h"

#include "nsIFormControl.h"
#include "nsStyleUtil.h"
#include "nsINameSpaceManager.h"
#include "nsIDeviceContext.h" //rods
#include "nsIHTMLContent.h" //rods
#include "nsIDOMHTMLCollection.h" //rods
#include "nsIDOMHTMLSelectElement.h" //rods
//#include "nsIDOMHTMLOListElement.h" //rods
#include "nsIDOMHTMLOptionElement.h" //rods
#include "nsIDOMNode.h" //rods
#include "nsHTMLAtoms.h" //rods
#include "nsIAtom.h" //rods
#include "nsIDOMEventReceiver.h"
#include "nsIComboboxControlFrame.h"
#include "nsComboboxControlFrame.h" // for the static helper function
#include "nsIListControlFrame.h"
#include "nsIViewManager.h"
#include "nsFormFrame.h"

#define CSS_NOTSET -1
#define ATTR_NOTSET -1

// Constants
const char * kNormal        = "";
const char * kSelected      = "SELECTED";
const char * kSelectedFocus = "SELECTEDFOCUS";

const char * kMozSelected = "-moz-option-selected";


static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);

#ifdef PLUGGABLE_EVENTS
static NS_DEFINE_IID(kIPluggableEventListenerIID, NS_IPLUGGABLEEVENTLISTENER_IID);
#endif
static NS_DEFINE_IID(kIListControlFrameIID, NS_ILISTCONTROLFRAME_IID);

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID,   NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID,   NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);

//----------------------------------------------------------------------

nsresult
NS_NewListControlFrame(nsIFrame*& aNewFrame)
{
  nsListControlFrame* it = new nsListControlFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aNewFrame = it;
  return NS_OK;
}

//----------------------------------------------------------------------
nsListControlFrame::nsListControlFrame()
{
  mHitFrame         = nsnull;
  mCurrentHitFrame  = nsnull;
  mSelectedFrame    = nsnull;
  mSelectedContent  = nsnull;
  mSelectedIndex    = -1;
  mNumRows          = 0;
  mMaxNumSelections = 64; 
  mIsInitializedFromContent = PR_FALSE;

  PRInt32 i;
  for (i=0;i<mMaxNumSelections;i++) {
    mIsFrameSelected[i] = PR_FALSE;
  }
  mNumSelections  = 0;
  mInDropDownMode = PR_FALSE;
  mComboboxFrame  = nsnull;

}

//----------------------------------------------------------------------
nsListControlFrame::~nsListControlFrame()
{
  NS_IF_RELEASE(mComboboxFrame);
}


//----------------------------------------------------------------------
NS_IMPL_ADDREF(nsListControlFrame)
NS_IMPL_RELEASE(nsListControlFrame)

//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  if (aIID.Equals(kIListControlFrameIID)) {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtr = (void *)((nsIListControlFrame*)this);
    return NS_OK;
  }
#ifdef PLUGGABLE_EVENTS
  if (aIID.Equals(kIPluggableEventListenerIID)) {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtr = (void *)((nsIPluggableEventListener*)this);
    return NS_OK;
  }
#endif
  return nsScrollFrame::QueryInterface(aIID, aInstancePtr);
}


//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  //nsresult rv = nsScrollFrame::GetFrameForPoint(aPoint, aFrame);
  nsresult rv = GetFrameForPointUsing(aPoint, nsnull, aFrame);
  if (NS_OK == rv) {
    if (*aFrame != this) {
      mHitFrame = *aFrame;
      *aFrame = this;
    }
    return NS_OK;
  }
  *aFrame = this;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::GetFrameForPointUsing(const nsPoint& aPoint,
                                        nsIAtom*       aList,
                                        nsIFrame**     aFrame)
{
  nsIFrame* kid;
  nsRect kidRect;
  nsPoint tmp;
  *aFrame = nsnull;

  mContentFrame->FirstChild(aList, &kid);
  while (nsnull != kid) {
    kid->GetRect(kidRect);
    if (kidRect.Contains(aPoint)) {
      tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);

      nsIContent * content;
      kid->GetContent(&content);
      static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
      nsIDOMHTMLOptionElement* oe;
      if (content && (NS_OK == content->QueryInterface(kIDOMHTMLOptionElementIID, (void**) &oe))) {
        *aFrame = kid;
        NS_RELEASE(content);
        return NS_OK;
      }
      NS_RELEASE(content);
      return kid->GetFrameForPoint(tmp, aFrame);
      //*aFrame = kid;
      //return NS_OK;
    }
    kid->GetNextSibling(&kid);
  }

  mContentFrame->FirstChild(aList, &kid);
  while (nsnull != kid) {
    nsFrameState state;
    kid->GetFrameState(&state);
    if (NS_FRAME_OUTSIDE_CHILDREN & state) {
      kid->GetRect(kidRect);
      tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);
      if (NS_OK == kid->GetFrameForPoint(tmp, aFrame)) {
        return NS_OK;
      }
    }
    kid->GetNextSibling(&kid);
  }
  *aFrame = this;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
void
nsListControlFrame::PaintChildren(nsIPresContext&      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer)
{

  nsScrollFrame::PaintChildren(aPresContext,
                              aRenderingContext,
                              aDirtyRect, aWhichLayer);
}

//----------------------------------------------------------------------
// XXX this needs to properly deal with max element size. percentage based unconstrained widths cannot
// be allowed to return large max element sizes or tables will get too large.
NS_IMETHODIMP 
nsListControlFrame::Reflow(nsIPresContext&          aPresContext, 
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState, 
                    nsReflowStatus&          aStatus)
{
  if (!mIsInitializedFromContent) {
    mIsInitializedFromContent = PR_TRUE;
    InitializeFromContent(PR_TRUE);
  }

  nsIFrame* childFrame;

  nscoord scrollbarWidth  = 0;
  nscoord scrollbarHeight = 0;
  GetScrollBarDimensions(aPresContext, scrollbarWidth, scrollbarHeight);

  nsFont font(aPresContext.GetDefaultFixedFontDeprecated());
  GetFont(&aPresContext, font);

  nsSize maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
  nsHTMLReflowMetrics desiredSize = aDesiredSize;
  aDesiredSize.width  = 0;
  aDesiredSize.height = 0;

  // Calculate the amount of space needed for borders
  const nsStyleSpacing*  spacing = (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  if (!spacing->GetBorder(border)) {
    border.SizeTo(0, 0, 0, 0);
  }

  nsSize desiredLineSize;
  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, desiredLineSize);

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nscoord lineEndPadding = NSIntPixelsToTwips(10, p2t);//GetHorizontalInsidePadding(aPresContext, p2t, 0, 0);
  aDesiredSize.width += lineEndPadding;

  desiredSize = aDesiredSize;
  nsIFrame * firstChild = mFrames.FirstChild();

  nsHTMLReflowState   reflowState(aPresContext, aReflowState, firstChild, maxSize);
  nsIHTMLReflow*      htmlReflow;
  if (NS_OK == firstChild->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
    htmlReflow->WillReflow(aPresContext);
    htmlReflow->Reflow(aPresContext, desiredSize, reflowState, aStatus);
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  }

  mContentFrame->FirstChild(nsnull, &childFrame);
  PRInt32 numChildren = LengthOf(childFrame);

  aDesiredSize.width  += border.left+border.right;
  aDesiredSize.height += border.top+border.bottom;

  nscoord insideWidth = aDesiredSize.width-border.left-border.right-1;
  if (mInDropDownMode) {
    aDesiredSize.width += scrollbarWidth;
    insideWidth        += scrollbarWidth;
  } else {
    if (mNumRows < numChildren) {
      aDesiredSize.width += scrollbarWidth;
    }
  }

  nsRect rect(0,0, insideWidth, desiredLineSize.height*numChildren);
  mContentFrame->SetRect(rect);


  mContentFrame->FirstChild(nsnull, &childFrame);
  nsPoint offset(0,0);
  while (nsnull != childFrame) {  // reflow, place, size the children
    nsHTMLReflowState   reflowState(aPresContext, aReflowState, childFrame, maxSize);
    nsIHTMLReflow*      htmlReflow;

    if (NS_OK == childFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->WillReflow(aPresContext);
      htmlReflow->Reflow(aPresContext, desiredSize, reflowState, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
      //??nsRect rect(offset.x, offset.y, desiredLineSize.width+lineEndPadding, desiredLineSize.height);
      nsRect rect(offset.x, offset.y, insideWidth, desiredLineSize.height);
      childFrame->SetRect(rect);
      childFrame->GetNextSibling(&childFrame);
      offset.x = 0;
      offset.y += desiredLineSize.height;
    }
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    //XXX    aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;

  return NS_OK;
}

NS_IMETHODIMP nsListControlFrame::SetRect(const nsRect& aRect)
{
  nsScrollFrame::SetRect(aRect);
  return NS_OK;
}
NS_IMETHODIMP nsListControlFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  nsScrollFrame::SizeTo(aWidth, aHeight);
  return NS_OK;
}

//----------------------------------------------------------------------
nsIFrame *
nsListControlFrame::GetOptionFromChild(nsIFrame* aParentFrame)
{

  nsIFrame* kid;

  aParentFrame->FirstChild(nsnull, &kid);
  while (nsnull != kid) {
    nsIContent * content;
    kid->GetContent(&content);
    static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
    nsIDOMHTMLOptionElement* element;
    if (content && (NS_OK == content->QueryInterface(kIDOMHTMLOptionElementIID, (void**) &element))) {
      NS_RELEASE(content);
      NS_RELEASE(element);
      return kid;
    }
    NS_RELEASE(content);
    nsIFrame * frame = GetOptionFromChild(kid);
    if (nsnull != frame) {
      return frame;
    }
    kid->GetNextSibling(&kid);
  }
  return nsnull;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsListControlFrame::PluggableEventHandler(nsIPresContext& aPresContext, 
                                                 nsGUIEvent*     aEvent,
                                                 nsEventStatus&  aEventStatus)
{
  HandleEvent(aPresContext, aEvent, aEventStatus);
  //aEventStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsListControlFrame::PluggableGetFrameForPoint(const nsPoint& aPoint, 
                                                     nsIFrame**     aFrame)
{
  return GetFrameForPoint(aPoint, aFrame);
}

//----------------------------------------------------------------------
PRInt32 nsListControlFrame::SetContentSelected(nsIFrame *    aHitFrame, 
                                        nsIContent *& aHitContent, 
                                        PRBool        aIsSelected) 
{
  PRInt32 index   = 0;
  nsIFrame* kid;
  mContentFrame->FirstChild(nsnull, &kid);
  while (nsnull != kid) {
    if (kid == aHitFrame) {
      NS_IF_RELEASE(aHitContent);
      nsIContent* content;
      kid->GetContent(&content);
      aHitContent = content;
      nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)) );
      aHitContent->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_TRUE);
      return index;
    }
    kid->GetNextSibling(&kid);
    index++;
  }
  return -1;
}

//----------------------------------------------------------------------
void nsListControlFrame::ClearSelection()
{
  PRInt32 i = 0;
  nsIFrame* kid;
  mContentFrame->FirstChild(nsnull, &kid);
  while (nsnull != kid) {
    nsIContent * content;
    kid->GetContent(&content);
    if (mIsFrameSelected[i]) {
      if (i != mSelectedIndex) {
        content->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, kNormal, PR_TRUE);
        mIsFrameSelected[i] = PR_FALSE;
      }
    }
    NS_RELEASE(content);
    kid->GetNextSibling(&kid);
    i++;
  }
}

//----------------------------------------------------------------------
void nsListControlFrame::ExtendedSelection(PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aDoInvert, PRBool aSetValue)
{
  PRInt32 startInx;
  PRInt32 endInx;

  if (aStartIndex < aEndIndex) {
    startInx = aStartIndex;
    endInx   = aEndIndex;
  } else {
    startInx = aEndIndex;
    endInx   = aStartIndex;
  }

  PRInt32 i = 0;
  nsIFrame* kid;
  PRBool startInverting = PR_FALSE;
  mContentFrame->FirstChild(nsnull, &kid);
  while (nsnull != kid) {
    nsIContent * content;
    kid->GetContent(&content);
    if (i == startInx) {
      startInverting = PR_TRUE;
    }
    if (startInverting && ((i != mStartExtendedIndex && aDoInvert) || !aDoInvert)) {
      if (aDoInvert) {
        mIsFrameSelected[i] = !mIsFrameSelected[i];
      } else {
        mIsFrameSelected[i] = aSetValue;
      }
      if (i != mSelectedIndex) {
        content->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, (mIsFrameSelected[i]?kSelected:kNormal), PR_TRUE);
      }
    }
    if (i == endInx) {
      startInverting = PR_FALSE;
    }
    NS_RELEASE(content);
    kid->GetNextSibling(&kid);
    i++;
  }
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsListControlFrame::HandleLikeListEvent(nsIPresContext& aPresContext, 
                                               nsGUIEvent*     aEvent,
                                               nsEventStatus&  aEventStatus)
{
  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    PRInt32 oldSelectedIndex = mSelectedIndex;
    mSelectedIndex = (PRInt32)SetContentSelected(mHitFrame, mHitContent, PR_TRUE);
    if (-1 < mSelectedIndex) {
      PRBool selected = mIsFrameSelected[mSelectedIndex];
      if (mMultipleSelections) {

        if (((nsMouseEvent *)aEvent)->isShift) {
          selected = PR_TRUE;
          if (mEndExtendedIndex == -1) {
            mEndExtendedIndex = mSelectedIndex;
            ExtendedSelection(mStartExtendedIndex, mEndExtendedIndex, PR_FALSE, PR_TRUE);
          } else {
            if (mStartExtendedIndex < mEndExtendedIndex) {
              if (mSelectedIndex < mStartExtendedIndex) {
                ExtendedSelection(mSelectedIndex, mEndExtendedIndex, PR_TRUE, PR_TRUE);
                mEndExtendedIndex   = mSelectedIndex;
              } else if (mSelectedIndex > mEndExtendedIndex) {
                ExtendedSelection(mEndExtendedIndex+1, mSelectedIndex, PR_FALSE, PR_TRUE);
                mEndExtendedIndex = mSelectedIndex;
              } else if (mSelectedIndex < mEndExtendedIndex) {
                ExtendedSelection(mSelectedIndex+1, mEndExtendedIndex, PR_FALSE, PR_FALSE);
                mEndExtendedIndex = mSelectedIndex;
              }
            } else if (mStartExtendedIndex > mEndExtendedIndex) {
              if (mSelectedIndex > mStartExtendedIndex) {
                ExtendedSelection(mEndExtendedIndex, mSelectedIndex, PR_TRUE, PR_TRUE);
                mEndExtendedIndex   = mSelectedIndex;
              } else if (mSelectedIndex < mEndExtendedIndex) {
                ExtendedSelection(mStartExtendedIndex+1, mEndExtendedIndex-1, PR_FALSE, PR_TRUE);
                mEndExtendedIndex = mSelectedIndex;
              } else if (mSelectedIndex > mEndExtendedIndex) {
                ExtendedSelection(mEndExtendedIndex, mSelectedIndex-1, PR_FALSE, PR_FALSE);
                mEndExtendedIndex = mSelectedIndex;
              }
            }
          }
        } else if (((nsMouseEvent *)aEvent)->isControl) {
          selected = !selected;
          if (nsnull != mSelectedContent) {
            if (mHitContent != mSelectedContent) {
              mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, (mIsFrameSelected[oldSelectedIndex]?kSelected:kNormal), PR_TRUE);
            } else {
              mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, (selected?kSelectedFocus:kNormal), PR_TRUE);
            }
          }
        } else {
          mStartExtendedIndex = mSelectedIndex;
          mEndExtendedIndex   = -1;
          ClearSelection();
          selected = PR_TRUE;
        }
      } else {
        if (nsnull != mSelectedContent) {
        nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)) );
        mSelectedContent->UnsetAttribute(kNameSpaceID_None, selectedAtom, PR_TRUE);
 //XXX: This needs to a set a psuedo attribute instead         mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, kNormal, PR_TRUE);
          NS_RELEASE(mSelectedContent);
        }
      }
      mIsFrameSelected[mSelectedIndex] = selected;
      //XXX mHitContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, (selected?kSelectedFocus:kNormal), PR_TRUE);
      mSelectedContent = mHitContent;
      mSelectedFrame   = mHitFrame;
      aEventStatus = nsEventStatus_eConsumeNoDefault;
    }
  } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
    aEventStatus = nsEventStatus_eConsumeNoDefault;
  }
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsListControlFrame::HandleLikeDropDownListEvent(nsIPresContext& aPresContext, 
                                                       nsGUIEvent*     aEvent,
                                                       nsEventStatus&  aEventStatus)
{
  // Mouse Move behavior is as follows:
  // When the DropDown occurs, if an item is selected it displayed as being selected.
  // It may or may not be currently visible, when the mouse is moved across any item 
  // in the the list it is displayed as the currently selected item. 
  // (Selection doesn't actually occur until the mouse is released, or lciked and released)
  if (aEvent->message == NS_MOUSE_MOVE) {

    // If the DropDown is currently has a selected item 
    // then clear the selected item
    if (nsnull != mSelectedContent) {
      mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, kNormal, PR_TRUE);
      NS_RELEASE(mSelectedContent);
    }

    // Now check to see if the newly hit frame is different
    // from the currently hit frame
    if (nsnull != mHitFrame && mHitFrame != mCurrentHitFrame) {

      // Since the new frame is different then clear the selection on the old frame (the current frame)
      if (nsnull != mCurrentHitContent) {
        mCurrentHitContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, kNormal, PR_TRUE);
        NS_RELEASE(mCurrentHitContent);
      }

      // Find the new hit "content" from the hit frame and select it
      SetContentSelected(mHitFrame, mHitContent, PR_TRUE);

      // Now cache the the newly hit frame and content as the "current" values
      mCurrentHitFrame   = mHitFrame;
      mCurrentHitContent = mHitContent;

    }

  } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
  
    // Start by finding the newly "hit" content from the hit frame
    PRInt32 index = SetContentSelected(mHitFrame, mHitContent, PR_FALSE);
    if (-1 < index) {

      nsIDOMHTMLOptionElement* option = nsnull;
      nsresult rv = mHitContent->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
      if (NS_OK == rv) {
        nsAutoString text;
        if (NS_CONTENT_ATTR_HAS_VALUE == option->GetText(text)) {
          mSelectionStr = text;
        }

        NS_IF_RELEASE(mSelectedContent);
        mSelectedIndex = index;

        // Clean up frames before disappearing
        if (nsnull != mSelectedContent) {
          mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, kNormal, PR_TRUE);
          NS_RELEASE(mSelectedContent);
        }
        if (nsnull != mHitContent) {
          mHitContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, kNormal, PR_TRUE);
          NS_RELEASE(mHitContent);
        }
        if (nsnull != mCurrentHitContent) {
          mCurrentHitContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, kNormal, PR_TRUE);
          NS_RELEASE(mCurrentHitContent);
        }
        NS_RELEASE(option);
      }

      if (mComboboxFrame) {
        mComboboxFrame->ListWasSelected(&aPresContext);
      }

    }

  }
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsListControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus&  aEventStatus)
{
/*  const char * desc[] = {"NS_MOUSE_MOVE", 
                          "NS_MOUSE_LEFT_BUTTON_UP",
                          "NS_MOUSE_LEFT_BUTTON_DOWN",
                          "<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>",
                          "NS_MOUSE_MIDDLE_BUTTON_UP",
                          "NS_MOUSE_MIDDLE_BUTTON_DOWN",
                          "<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>",
                          "NS_MOUSE_RIGHT_BUTTON_UP",
                          "NS_MOUSE_RIGHT_BUTTON_DOWN",
                          "NS_MOUSE_ENTER",
                          "NS_MOUSE_EXIT",
                          "NS_MOUSE_LEFT_DOUBLECLICK",
                          "NS_MOUSE_MIDDLE_DOUBLECLICK",
                          "NS_MOUSE_RIGHT_DOUBLECLICK",
                          "NS_MOUSE_LEFT_CLICK",
                          "NS_MOUSE_MIDDLE_CLICK",
                          "NS_MOUSE_RIGHT_CLICK"};
  int inx = aEvent->message-NS_MOUSE_MESSAGE_START;
  if (inx >= 0 && inx <= (NS_MOUSE_RIGHT_CLICK-NS_MOUSE_MESSAGE_START)) {
    printf("Mouse in ListFrame %s [%d]\n", desc[inx], aEvent->message);
  } else {
    printf("Mouse in ListFrame <UNKNOWN> [%d]\n", aEvent->message);
  }*/

  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->mVisible) {
    return NS_OK;
  }

  if(nsEventStatus_eConsumeNoDefault != aEventStatus) {

    aEventStatus = nsEventStatus_eConsumeNoDefault;
    if (mInDropDownMode) {
      HandleLikeDropDownListEvent(aPresContext, aEvent, aEventStatus);
    } else {
      HandleLikeListEvent(aPresContext, aEvent, aEventStatus);
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  mContentFrame = aChildList;
#ifdef PLUGGABLE_EVENTS
  mContentFrame->SetPluggableEventListener(this);
#endif
  if (!mInDropDownMode) {
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }
  //aChildList->FirstChild(nsnull, mContentFrame);

  if (!mIsInitializedFromContent) {
    InitializeFromContent();
  }
 
  return nsScrollFrame::SetInitialChildList(aPresContext, aListName, aChildList);

}
//----------------------------------------------------------------------
/*nsresult
nsListControlFrame::AppendNewFrames(nsIPresContext& aPresContext,
                               nsIFrame* aNewFrame)
{
  return nsScrollFrame::AppendNewFrames(aPresContext, aNewFrame);
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::InsertNewFrame(nsIPresContext& aPresContext,
                              nsBaseIBFrame*   aParentFrame,
                              nsIFrame*       aNewFrame,
                              nsIFrame*       aPrevSibling)
{
  return nsScrollFrame::InsertNewFrame(aPresContext, aParentFrame, aNewFrame, aPrevSibling);
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::DoRemoveFrame(nsBlockReflowState& aState,
                             nsBaseIBFrame* aParentFrame,
                             nsIFrame* aDeletedFrame,
                             nsIFrame* aPrevSibling)
{
  return nsScrollFrame::DoRemoveFrame(aState, aParentFrame, aDeletedFrame, aPrevSibling);
}*/

//----------------------------------------------------------------------
NS_IMETHODIMP  
nsListControlFrame::Init(nsIPresContext&  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  nsresult result = nsScrollFrame::Init(aPresContext, aContent, aParent, aContext,
                                        aPrevInFlow);
  /*if (NS_OK == result) {
    nsIDOMNode* node;
    if (mContent && (NS_OK == mContent->QueryInterface(kIDOMNodeIID, (void**) &node))) {
      AddEventListener(node);
      NS_RELEASE(node);
    }
  }*/
  if (NS_OK == result) {
    nsIDOMHTMLSelectElement* select;
    if (mContent && (NS_OK == mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**) &select))) {
      select->GetMultiple(&mMultipleSelections);
      select->GetSize(&mNumRows);

      NS_RELEASE(select);
    }
  }
  
  return result;

}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetMaxLength(PRInt32* aSize)
{
  *aSize = -1;
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::maxlength, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  return result;
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::GetSizeFromContent(PRInt32* aSize) const
{
  *aSize = -1;
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::size, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  return result;
}
//----------------------------------------------------------------------
void 
nsListControlFrame::GetScrollBarDimensions(nsIPresContext& aPresContext,
                                    nscoord &aWidth, nscoord &aHeight)
{

  aWidth  = 0;
  aHeight = 0;
  float   scale;
  nsCOMPtr<nsIDeviceContext> dx;
  aPresContext.GetDeviceContext(getter_AddRefs(dx));
  if (dx) { 
    float sbWidth;
    float sbHeight;
    dx->GetCanonicalPixelScale(scale);
    dx->GetScrollBarDimensions(sbWidth, sbHeight);
    aWidth  = PRInt32(sbWidth * scale);
    aHeight = PRInt32(sbHeight * scale);
  } //else {
    //aWidth  = 19 * sp2t;
    //aHeight = scrollbarWidth;
  //}

  
}

//----------------------------------------------------------------------
nscoord 
nsListControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(3, aPixToTwip);
}

//----------------------------------------------------------------------
nscoord 
nsListControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

//----------------------------------------------------------------------
nscoord 
nsListControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                      nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(0, aPixToTwip); // XXX this is really 
}

//----------------------------------------------------------------------
nscoord 
nsListControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                        float aPixToTwip, 
                                        nscoord aInnerWidth,
                                        nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPixToTwip, aInnerWidth);
}


//----------------------------------------------------------------------

void 
nsListControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredLayoutSize,
                             nsSize& aDesiredWidgetSize)
{
  nsIDOMHTMLSelectElement* select = GetSelect(mContent);
  if (!select) {
    return;
  }
  nsIDOMHTMLCollection* options = GetOptions(mContent, select);
  if (!options) {
    NS_RELEASE(select);
    return;
  }

  // get the css size 
  nsSize styleSize;
  nsFormControlFrame::GetStyleSize(*aPresContext, aReflowState, styleSize);

  // get the size of the longest option 
  PRInt32  maxWidth = 1;
  PRUint32 numOptions;
  options->GetLength(&numOptions);
  for (PRUint32 i = 0; i < numOptions; i++) {
    nsIDOMHTMLOptionElement* option = GetOption(*options, i);
    if (option) {
      //option->CompressContent();
       nsAutoString text;
      if (NS_CONTENT_ATTR_HAS_VALUE != option->GetText(text)) {
        continue;
      }
      nsSize textSize;
      // use the style for the select rather that the option, since widgets don't support it
      nsFormControlHelper::GetTextSize(*aPresContext, this, text, textSize, aReflowState.rendContext); 
      if (textSize.width > maxWidth) {
        maxWidth = textSize.width;
      }
      NS_RELEASE(option);
    }
  }

  PRInt32 rowHeight = 0;
  nsSize desiredSize;
  nsSize minSize;
  PRBool widthExplicit, heightExplicit;
  nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull, nsnull,
                                maxWidth, PR_TRUE, nsHTMLAtoms::size, 1);
  // XXX fix CalculateSize to return PRUint32
  mNumRows = (PRUint32)nsFormControlHelper::CalculateSize (aPresContext, aReflowState.rendContext, this, styleSize, 
                                                           textSpec, desiredSize, minSize, widthExplicit, 
                                                           heightExplicit, rowHeight);

  float sp2t;
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  aPresContext->GetScaledPixelsToTwips(&sp2t);

  //nscoord scrollbarWidth  = 0;
  //nscoord scrollbarHeight = 0;
  //GetScrollBarDimensions(*aPresContext, scrollbarWidth, scrollbarHeight);

  if (mInDropDownMode) {
    //PRUint32 numOptions;
    //options->GetLength(&numOptions);
    nscoord extra = desiredSize.height - (rowHeight * mNumRows);

    mNumRows = (numOptions > 20 ? 20 : numOptions);
    desiredSize.height = (mNumRows * rowHeight) + extra;
    if (mNumRows < 21) {
      //calcSize.width += scrollbarWidth;
    }
  }

  aDesiredLayoutSize.width = desiredSize.width;
  // account for vertical scrollbar, if present  
  //if (!widthExplicit && ((mNumRows < numOptions) || mIsComboBox)) {
  //if (!widthExplicit && (mNumRows < (PRInt32)numOptions)) {
  //  aDesiredLayoutSize.width += scrollbarWidth;
 // }

  // XXX put this in widget library, combo boxes are fixed height (visible part)
  //aDesiredLayoutSize.height = (mIsComboBox)
  //  ? rowHeight + (2 * GetVerticalInsidePadding(p2t, rowHeight))
  //  : calcSize.height; 
  aDesiredLayoutSize.height = desiredSize.height; 
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;

  // XXX When this code is working, max element size needs to be set properly from CalculateSize 
  // above. look at some of the other form control to see how to do it.

  aDesiredWidgetSize.width  = maxWidth;
  aDesiredWidgetSize.height = rowHeight;
  //if (mIsComboBox) {  // add in pull down size
  //  PRInt32 extra = NSIntPixelsToTwips(10, p2t*scale);
  //  aDesiredWidgetSize.height += (rowHeight * (numOptions > 20 ? 20 : numOptions)) + extra;
  //}

  // override the width and height for a combo box that has already got a widget
  //if (mWidget && mIsComboBox) {
  //  nscoord ignore;
  //  GetWidgetSize(*aPresContext, ignore, aDesiredLayoutSize.height);
  //  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  //}

  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->width  = minSize.width;
    aDesiredLayoutSize.maxElementSize->height = minSize.height;
  }

  NS_RELEASE(select);
  NS_RELEASE(options);

}


//----------------------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetMultiple(PRBool* aMultiple, nsIDOMHTMLSelectElement* aSelect)
{
  if (!aSelect) {
    nsIDOMHTMLSelectElement* select = nsnull;
    nsresult result = mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&select);
    if ((NS_OK == result) && select) {
      result = select->GetMultiple(aMultiple);
      NS_RELEASE(select);
    } 
    return result;
  } else {
    return aSelect->GetMultiple(aMultiple);
  }
}

//----------------------------------------------------------------------
nsIDOMHTMLSelectElement* 
nsListControlFrame::GetSelect(nsIContent * aContent)
{
  nsIDOMHTMLSelectElement* select = nsnull;
  nsresult result = aContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&select);
  if ((NS_OK == result) && select) {
    return select;
  } else {
    return nsnull;
  }
}

//----------------------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::AboutToDropDown()
{
  PRInt32 i = 0;
  nsIFrame* kid;
  mContentFrame->FirstChild(nsnull, &kid);
  while (nsnull != kid) {
    nsIContent * content;
    kid->GetContent(&content);
    if (i == mSelectedIndex) {
      mSelectedContent = content;
      mSelectedFrame   = kid;
      mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, kSelected, PR_TRUE);
      return NS_OK;
    }
    NS_RELEASE(content);
    kid->GetNextSibling(&kid);
    i++;
  }
  return NS_OK;
}

//----------------------------------------------------------------------
void 
nsListControlFrame::InitializeFromContent(PRBool aDoDisplay)
{
  PRInt32 i;
  for (i=0;i<mMaxNumSelections;i++) {
    mIsFrameSelected[i] = PR_FALSE;
  }

  i = 0;
  nsIFrame* kid;
  mContentFrame->FirstChild(nsnull, &kid);
  while (nsnull != kid) {
    nsIContent * content;
    kid->GetContent(&content);

    nsIDOMHTMLOptionElement* option = nsnull;
    nsresult result = content->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
    if (result == NS_OK) {
      PRBool selected;
      option->GetDefaultSelected(&selected);
 //XXX: Remove, why is selected queried twice here?  option->GetSelected(&selected);
      mIsFrameSelected[i] = selected;

      if (mInDropDownMode) {
        if (selected) {
          nsAutoString text;
          if (NS_CONTENT_ATTR_HAS_VALUE == option->GetText(text)) {
            mSelectionStr = text;
          }
          mSelectedIndex = i;
          NS_RELEASE(option);
          mSelectedContent = content;
          mSelectedFrame   = kid;
          return;
        }
      } else {
        if (selected && aDoDisplay) {
           // XXX: Here we introduce a new -moz-option-selected attribute so a attribute
           // selecitor n the ua.css can change the style when the option is selected.
          nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)));
          content->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_TRUE);
        }
      }
      NS_RELEASE(option);
    }
    NS_RELEASE(content);
    kid->GetNextSibling(&kid);
    i++;
  }

}

//----------------------------------------------------------------------
nsIDOMHTMLCollection* 
nsListControlFrame::GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect)
{
  nsIDOMHTMLCollection* options = nsnull;
  if (!aSelect) {
    nsIDOMHTMLSelectElement* select = GetSelect(aContent);
    if (select) {
      select->GetOptions(&options);
      NS_RELEASE(select);
      return options;
    } else {
      return nsnull;
    }
  } else {
    aSelect->GetOptions(&options);
    return options;
  }
}

//----------------------------------------------------------------------
nsIDOMHTMLOptionElement* 
nsListControlFrame::GetOption(nsIDOMHTMLCollection& aCollection, PRUint32 aIndex)
{
  nsIDOMNode* node = nsnull;
  if ((NS_OK == aCollection.Item(aIndex, &node)) && node) {
    nsIDOMHTMLOptionElement* option = nsnull;
    node->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
    NS_RELEASE(node);
    return option;
  }
  return nsnull;
}


//----------------------------------------------------------------------
PRBool
nsListControlFrame::GetOptionValue(nsIDOMHTMLCollection& aCollection, PRUint32 aIndex, nsString& aValue)
{
  PRBool status = PR_FALSE;
  nsIDOMHTMLOptionElement* option = GetOption(aCollection, aIndex);
  if (option) {
    nsresult result = option->GetValue(aValue);
    if (aValue.Length() > 0) {
      status = PR_TRUE;
    } else {
      result = option->GetText(aValue);
      if (aValue.Length() > 0) {
        status = PR_TRUE;
      }
    }
    NS_RELEASE(option);
  }
  return status;
}



//----------------------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::Deselect() 
{
  PRInt32 i;
  for (i=0;i<mMaxNumSelections;i++) {
    mIsFrameSelected[i] = PR_FALSE;
    // update visual here
  }
  mNumSelections = 0;
  return NS_OK;
}

#if 0
//------------------------------------------------------------------
nscoord 
nsListControlFrame::GetTextSize(nsIPresContext& aPresContext, nsListControlFrame* aFrame,
                         const nsString& aString, nsSize& aSize,
                         nsIRenderingContext *aRendContext)
{
  nsFont font(aPresContext.GetDefaultFixedFont());
  aFrame->GetFont(&aPresContext, font);
  nsIDeviceContext* deviceContext = aPresContext.GetDeviceContext();

  nsIFontMetrics* fontMet;
  deviceContext->GetMetricsFor(font, fontMet);
  aRendContext->SetFont(fontMet);
  aRendContext->GetWidth(aString, aSize.width);
  fontMet->GetHeight(aSize.height);

  char char1, char2;
  nsCompatibility mode = nsFormControlHelper::GetRepChars(aPresContext, char1, char2);
  nscoord char1Width, char2Width;
  aRendContext->GetWidth(char1, char1Width);
  aRendContext->GetWidth(char2, char2Width);

  NS_RELEASE(fontMet);
  NS_RELEASE(deviceContext);

  if (eCompatibility_Standard == mode) {
    return ((char1Width + char2Width) / 2) + 1;
  } else {
    return char1Width;
  }
}  
  
//------------------------------------------------------------------
nscoord
nsListControlFrame::GetTextSize(nsIPresContext& aPresContext, nsListControlFrame* aFrame,
                                PRInt32 aNumChars, nsSize& aSize,
                                nsIRenderingContext *aRendContext)
{
  nsAutoString val;
  char char1, char2;
  nsFormControlHelper::GetRepChars(aPresContext, char1, char2);
  int i;
  for (i = 0; i < aNumChars; i+=2) {
    val += char1;  
  }
  for (i = 1; i < aNumChars; i+=2) {
    val += char2;  
  }
  return GetTextSize(aPresContext, aFrame, val, aSize, aRendContext);
}
  
//------------------------------------------------------------------
PRInt32
nsListControlFrame::CalculateSize (nsIPresContext* aPresContext, nsListControlFrame* aFrame,
                            const nsSize& aCSSSize, nsInputDimensionSpec& aSpec, 
                            nsSize& aBounds, PRBool& aWidthExplicit, 
                            PRBool& aHeightExplicit, nscoord& aRowHeight,
                            nsIRenderingContext *aRendContext) 
{
  nscoord charWidth   = 0;
  PRInt32 numRows     = ATTR_NOTSET;
  aWidthExplicit      = PR_FALSE;
  aHeightExplicit     = PR_FALSE;

  aBounds.width  = CSS_NOTSET;
  aBounds.height = CSS_NOTSET;
  nsSize textSize(0,0);

  nsIContent* iContent = nsnull;
  aFrame->GetContent((nsIContent*&) iContent);
  if (!iContent) {
    return 0;
  }
  nsIHTMLContent* hContent = nsnull;
  nsresult result = iContent->QueryInterface(kIHTMLContentIID, (void**)&hContent);
  if ((NS_OK != result) || !hContent) {
    NS_RELEASE(iContent);
    return 0;
  }
  nsAutoString valAttr;
  nsresult valStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mColValueAttr) {
    valStatus = hContent->GetAttribute(kNameSpaceID_HTML, aSpec.mColValueAttr, valAttr);
  }
  nsHTMLValue colAttr;
  nsresult colStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mColSizeAttr) {
    colStatus = hContent->GetHTMLAttribute(aSpec.mColSizeAttr, colAttr);
  }
  float p2t;
  aPresContext->GetScaledPixelsToTwips(p2t);

  // determine the width
  nscoord adjSize;
  if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
    PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
    if (aSpec.mColSizeAttrInPixels) {
      col = (col <= 0) ? 15 : col; 
      aBounds.width = NSIntPixelsToTwips(col, p2t);
    }
    else {
      col = (col <= 0) ? 1 : col;
      charWidth = GetTextSize(*aPresContext, aFrame, col, aBounds, aRendContext);
      aRowHeight = aBounds.height;  // XXX aBounds.height has CSS_NOTSET
    }
    if (aSpec.mColSizeAttrInPixels) {
      aWidthExplicit = PR_TRUE;
    }
  }
  else {
    if (CSS_NOTSET != aCSSSize.width) {  // css provides width
      aBounds.width = (aCSSSize.width > 0) ? aCSSSize.width : 1;
      aWidthExplicit = PR_TRUE;
    } 
    else {                       
      if (NS_CONTENT_ATTR_HAS_VALUE == valStatus) { // use width of initial value 
        charWidth = GetTextSize(*aPresContext, aFrame, valAttr, aBounds, aRendContext);
      }
      else if (aSpec.mColDefaultValue) { // use default value
        charWidth = GetTextSize(*aPresContext, aFrame, *aSpec.mColDefaultValue, aBounds, aRendContext);
      }
      else if (aSpec.mColDefaultSizeInPixels) {    // use default width in pixels
        charWidth = GetTextSize(*aPresContext, aFrame, 1, aBounds, aRendContext);
        aBounds.width = aSpec.mColDefaultSize;
      }
      else  {                                    // use default width in num characters
        charWidth = GetTextSize(*aPresContext, aFrame, aSpec.mColDefaultSize, aBounds, aRendContext); 
      }
      aRowHeight = aBounds.height; // XXX aBounds.height has CSS_NOTSET
    }
  }

  // determine the height
  nsHTMLValue rowAttr;
  nsresult rowStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mRowSizeAttr) {
    rowStatus = hContent->GetHTMLAttribute(aSpec.mRowSizeAttr, rowAttr);
  }
  if (NS_CONTENT_ATTR_HAS_VALUE == rowStatus) { // row attr will provide height
    PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
    adjSize = (rowAttrInt > 0) ? rowAttrInt : 1;
    if (0 == charWidth) {
      charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize, aRendContext);
      aBounds.height = textSize.height * adjSize;
      aRowHeight = textSize.height;
      numRows = adjSize;
    }
    else {
      aBounds.height = aBounds.height * adjSize;
    }
  }
  else if (CSS_NOTSET != aCSSSize.height) {  // css provides height
    aBounds.height = (aCSSSize.height > 0) ? aCSSSize.height : 1;
    aHeightExplicit = PR_TRUE;
  } 
  else {         // use default height in num lines
    if (0 == charWidth) {  
      charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize, aRendContext);
      aBounds.height = textSize.height * aSpec.mRowDefaultSize;
      aRowHeight = textSize.height;
    } 
    else {
      aBounds.height = aBounds.height * aSpec.mRowDefaultSize;
    }
  }

  if ((0 == charWidth) || (0 == textSize.width)) {
    charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize, aRendContext);
    aRowHeight = textSize.height;
  }

  // add inside padding if necessary
  if (!aWidthExplicit) {
    aBounds.width += (2 * aFrame->GetHorizontalInsidePadding(*aPresContext, p2t, aBounds.width, charWidth));
  }
  if (!aHeightExplicit) {
    aBounds.height += (2 * aFrame->GetVerticalInsidePadding(p2t, textSize.height)); 
  }

  NS_RELEASE(hContent);
  if (ATTR_NOTSET == numRows) {
    numRows = aBounds.height / aRowHeight;
  }

  NS_RELEASE(iContent);
  return numRows;
}

#endif

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// nsHTMLContainerFrame
//----------------------------------------------------------------------
PRIntn
nsListControlFrame::GetSkipSides() const
{
  return 0;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// nsIFormControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetType(PRInt32* aType) const
{
  /*nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIFormControl* formControl = nsnull;
    result = mContent->QueryInterface(kIFormControlIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      result = formControl->GetType(aType);
      NS_RELEASE(formControl);
    }
  }
  return result;
  */
  *aType = NS_FORM_SELECT;
  return NS_OK;
}

void 
nsListControlFrame::SetFormFrame(nsFormFrame* aFormFrame) 
{ 
  mFormFrame = aFormFrame; 
}

//----------------------------------------------------------------------
PRBool
nsListControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}

//----------------------------------------------------------------------
void 
nsListControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  /*nsIRadioButton* radioWidget;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID, (void**)&radioWidget))) {
	  radioWidget->SetState(PR_TRUE);
    NS_RELEASE(radioWidget);
    if (mFormFrame) {
      mFormFrame->OnRadioChecked(*this);
    } 
  }*/
}
//----------------------------------------------------------------------
PRInt32 
nsListControlFrame::GetMaxNumValues()
{
  PRBool multiple;
  GetMultiple(&multiple);
  if (multiple) {
    PRUint32 length = 0;
    nsIDOMHTMLCollection* options = GetOptions(mContent);
    if (options) {
      options->GetLength(&length);
    }
    return (PRInt32)length; // XXX fix return on GetMaxNumValues
  } else {
    return 1;
  }
}

//----------------------------------------------------------------------
void 
nsListControlFrame::Reset() 
{
  SetFocus(PR_TRUE, PR_TRUE);

  nsIDOMHTMLCollection* options = GetOptions(mContent);
  if (!options) {
    return;
  }

  PRUint32 numOptions;
  options->GetLength(&numOptions);

  Deselect();
  for (PRUint32 i = 0; i < numOptions; i++) {
    nsIDOMHTMLOptionElement* option = GetOption(*options, i);
    if (option) {
      PRBool selected = PR_FALSE;
      option->GetSelected(&selected);
      if (selected) {
        mIsFrameSelected[i] = PR_TRUE;
      }
      NS_RELEASE(option);
    }
  }

  // update visual here
  NS_RELEASE(options);
} 
//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetName(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      nsHTMLValue value;
      result = formControl->GetHTMLAttribute(nsHTMLAtoms::name, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}


//----------------------------------------------------------------------
/*PRBool
nsListControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, 
                                   nsString* aValues, nsString* aNames)
{
  aNumValues = 0;
  return PR_FALSE;
}*/
 
//----------------------------------------------------------------------
PRBool
nsListControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                     nsString* aValues, nsString* aNames)
{
  PRBool status = PR_FALSE;

  aNumValues = 0;
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_NOT_THERE == result)) {
    return PR_FALSE;
  }

  nsIDOMHTMLCollection* options = GetOptions(mContent);
  if (!options) {
    return PR_FALSE;
  }

  NS_ASSERTION(aMaxNumValues >= mNumSelections, "invalid max num values");
  if (mNumSelections >= 0) {
    PRInt32* selections = new PRInt32[mNumSelections];
    PRInt32 i = 0;
    PRInt32 inx;
    for (inx=0;i<mNumSelections;i++) {
      if (mIsFrameSelected[inx]) {
        selections[i++] = inx;
      }
    }
    aNumValues = 0;
    for (i = 0; i < mNumSelections; i++) {
      nsAutoString value;
      GetOptionValue(*options, selections[i], value);
      aNames[i]  = name;
      aValues[i] = value;
      aNumValues++;
    }
    delete[] selections;
    status = PR_TRUE;
  }

  NS_RELEASE(options);

  return status;
}


//----------------------------------------------------------------------
// native widgets don't unset focus explicitly and don't need to repaint
void 
nsListControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  // XXX I don't know whether to use the aRepaint flag as last param
  // in this call?
  if (mSelectedContent) {
    mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, (aOn?kSelectedFocus:kNormal), PR_TRUE);
  }

}

//----------------------------------------------------------------------
// nsIListControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::SetComboboxFrame(nsIFrame* aComboboxFrame)
{
  static NS_DEFINE_IID(kIComboboxIID, NS_ICOMBOBOXCONTROLFRAME_IID);
  if (aComboboxFrame && (NS_OK == aComboboxFrame->QueryInterface(kIComboboxIID, (void**) &mComboboxFrame))) {
    mInDropDownMode = PR_TRUE;
  }
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetSelectedItem(nsString & aStr)
{
  aStr = mSelectionStr;
  return NS_OK;
}

NS_IMETHODIMP nsListControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsListControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}

nsresult nsListControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}




#if 0
//----------------------------------------------------------------------
NS_IMETHODIMP  
nsListControlFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  nsIDOMNode* node;
  if (mContent && (NS_OK == mContent->QueryInterface(kIDOMNodeIID, (void**) &node))) {
    RemoveEventListener(node);
    NS_RELEASE(node);
  }
  return nsScrollFrame::DeleteFrame(aPresContext);

}
//-----------------------------------------------------------------
nsresult nsListControlFrame::Focus(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsListControlFrame::Blur(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsListControlFrame::ProcessEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsListControlFrame::AddEventListener(nsIDOMNode * aNode)
{
  nsIDOMEventReceiver * receiver;

  if (NS_OK == aNode->QueryInterface(kIDOMEventReceiverIID, (void**) &receiver)) {
    receiver->AddEventListener((nsIDOMFocusListener*)this, kIDOMFocusListenerIID);
    NS_RELEASE(receiver);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------
NS_IMETHODIMP nsListControlFrame::RemoveEventListener(nsIDOMNode * aNode)
{
  nsIDOMEventReceiver * receiver;

  if (NS_OK == aNode->QueryInterface(kIDOMEventReceiverIID, (void**) &receiver)) {
    receiver->RemoveEventListener(this, kIDOMFocusListenerIID);
    NS_RELEASE(receiver);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}
#endif

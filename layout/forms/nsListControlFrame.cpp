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
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsIDeviceContext.h" 
#include "nsIDOMHTMLCollection.h" 
#include "nsIDOMHTMLSelectElement.h" 
#include "nsIDOMHTMLOptionElement.h" 
#include "nsIComboboxControlFrame.h"
#include "nsIViewManager.h"
#include "nsFormFrame.h"
#include "nsIScrollableView.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsWidgetsCID.h"
#include "nsIReflowCommand.h"

// Constants
const nscoord kMaxDropDownRows =  20; // This matches the setting for 4.x browsers
const PRInt32 kDefaultMultiselectHeight = 4; // This is compatible with 4.x browsers
const PRInt32 kNothingSelected = -1;
const PRInt32 kMaxZ= 0x7fffffff; //XXX: Shouldn't there be a define somewhere for MaxInt for PRInt32
const PRInt32 kNoSizeSpecified = -1;

//XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
// -moz-option-selected in the ua.css style sheet. This will not be needed when
//The event state manager is functional. KMM
const char * kMozSelected = "-moz-option-selected";

static NS_DEFINE_IID(kIContentIID,              NS_ICONTENT_IID);
static NS_DEFINE_IID(kIFormControlFrameIID,     NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kIScrollableViewIID,       NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIListControlFrameIID,     NS_ILISTCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIFrameIID,                NS_IFRAME_IID);


nsresult
NS_NewListControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsListControlFrame* it = new nsListControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}


nsListControlFrame::nsListControlFrame()
{
  mHitFrame         = nsnull;
  mSelectedIndex    = kNothingSelected;
  mComboboxFrame  = nsnull;
  mFormFrame      = nsnull;
  mDisplayed      = PR_FALSE;
  mButtonDown     = PR_FALSE;
  mLastFrame      = nsnull;
  mMaxWidth       = 0;
  mMaxHeight      = 0;
  mIsCapturingMouseEvents = PR_FALSE;
}

nsListControlFrame::~nsListControlFrame()
{
  mComboboxFrame = nsnull;
  mFormFrame = nsnull;
}

NS_IMPL_ADDREF(nsListControlFrame)
NS_IMPL_RELEASE(nsListControlFrame)

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
    *aInstancePtr = (void *)((nsIListControlFrame*)this);
    return NS_OK;
  }

  return nsScrollFrame::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP
nsListControlFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
   // Save the result of GetFrameForPointUsing in the mHitFrame member variable.
   // mHitFrame is used later in the HandleLikeListEvent to determine what was clicked on.
   // XXX: This is kludgy, but there doesn't seem to be a way to get what was just clicked
   // on in the HandleEvent. The GetFrameForPointUsing is always called before the HandleEvent.
   //
  nsresult rv;
  mHitFrame = nsnull;  
  nsIFrame *childFrame;
  FirstChild(nsnull, &childFrame);
  rv = GetFrameForPointUsing(aPoint, nsnull, aFrame);
  if (NS_OK == rv) {
    if (*aFrame != this) {
      *aFrame = GetSelectableFrame(*aFrame);
      if (nsnull == *aFrame) {
        mHitFrame = this;
      } else {
        mHitFrame = *aFrame;
      }
      *aFrame = this;
    }
    return NS_OK;
  }
  *aFrame = this;
  return NS_ERROR_FAILURE;

}


nsresult
nsListControlFrame::GetFrameForPointUsing(const nsPoint& aPoint,
                                        nsIAtom*       aList,
                                        nsIFrame**     aFrame)
{
  nsIFrame* kid = nsnull;
  nsRect kidRect;
  nsPoint tmp;
  nsRect offset(0, 0, 0, 0); 
  *aFrame = nsnull;
  nsIFrame* firstKid = nsnull;
     
    // Get the scrolled offset from the view.
    //
    // XXX:Hack. This should not be necessary. It is only required because we interpose on the
    // normal event flow by redirecting to the listbox using the SetVFlags(NS_DONT_CHECK_CHILDREN).
    // What we should really do is have a manager which the option items report there events to.
    // The listbox frame would be the manager for the option items. KMM.
    //
    //
    // This is needed because:
    // The scrolled view is below the nsListControlFrame's ScrollingView in 
    // the view hierarchy. This presents a problem for event translation when the events 
    // are re-directed with the  nsView::SetVFlags(NS_DONT_CHECK_CHILDREN). Since 
    // the handling of the view event is short-circuited at the nsListControlFrame's 
    // scrolling view level mouse click events do not have the scolled offset added to them. 
    // The view containing the offset is below the nsListControlFrame's scrolling 
    // view, so the short-circuiting of the event processing provided by nsView:SetVFlags(NS_DONT_CHECK_CHILDREN) 
    // prevents the scrolled view's offset from ever being added to the event's offset.
    // When nsListControlFrame::GetFrameForPoint is invoked the nsPoint passed in does not include the scrolled view offset, 
    // so the frame returned will always be wrong when the listbox is scrolled.
    // As a hack we adjust for missing view offset by adding code to the nsListControlFrame::GetFrameForPoint which first 
    // gets the ScrollableView associated with the nsListControlFrame and asks for it's GetScrollPosition offsets. These are 
    // added to the point that is passed in so the correct frame is returned from nsListControlFrame::GetFrameForPoint.
  
   tmp.MoveTo(aPoint.x, aPoint.y);
   nsIView *view = nsnull;
   GetView(&view);
   if (nsnull != view) {
     nscoord offsetx;
     nscoord offsety;
     nsIScrollableView* scrollableView;
     if (NS_SUCCEEDED(view->QueryInterface(kIScrollableViewIID, (void **)&scrollableView))) {
       scrollableView->GetScrollPosition(offsetx, offsety);
       tmp.x += offsetx;
       tmp.y += offsety;  
        // Note: scrollableView is not ref counted 
     }
   }

  // Remove the top border from the offset.
  nsMargin border;
  tmp.y -= mBorderOffsetY;

  mContentFrame->FirstChild(aList, &kid);
  while (nsnull != kid) {
    kid->GetRect(kidRect);
    if (kidRect.Contains(tmp)) {
      tmp.MoveTo(tmp.x - kidRect.x, tmp.y - kidRect.y);

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
    }
    kid->GetNextSibling(&kid);
  }

  mContentFrame->FirstChild(aList, &kid);
  while (nsnull != kid) {
    nsFrameState state;
    kid->GetFrameState(&state);
    if (NS_FRAME_OUTSIDE_CHILDREN & state) {
      kid->GetRect(kidRect);
      tmp.MoveTo(tmp.x - kidRect.x, tmp.y - kidRect.y);
      if (NS_OK == kid->GetFrameForPoint(tmp, aFrame)) {
        return NS_OK;
      }
    }
    kid->GetNextSibling(&kid);
  }
  *aFrame = this;
  return NS_ERROR_FAILURE;
  
}


// Reflow is overriden to constrain the listbox height to the number of rows and columns
// specified. 

NS_IMETHODIMP 
nsListControlFrame::Reflow(nsIPresContext&   aPresContext, 
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState, 
                    nsReflowStatus&          aStatus)
{
   // Strategy: Let the inherited reflow happen as though the width and height of the
   // ScrollFrame are big enough to allow the listbox to
   // shrink to fit the longest option element line in the list.
   // The desired width and height returned by the inherited reflow is returned, 
   // unless one of the following has been specified.
   // 1. A css width has been specified.
   // 2. The size has been specified.
   // 3. The css height has been specified, but the number of rows has not.
   //  The size attribute overrides the height setting but the height setting
   // should be honored if there isn't a size specified.

    // Determine the desired width + height for the listbox + 
  aDesiredSize.width  = 0;
  aDesiredSize.height = 0;

  //--Calculate a width just big enough for the scrollframe to shrink around the
  //longest element in the list
  nsHTMLReflowState secondPassState(aReflowState);
  nsHTMLReflowState firstPassState(aReflowState);

   // Get the size of option elements inside the listbox
   // Compute the width based on the longest line in the listbox.
  
  firstPassState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
  firstPassState.mComputedHeight = NS_UNCONSTRAINEDSIZE;
  firstPassState.availableWidth = NS_UNCONSTRAINEDSIZE;
  firstPassState.availableHeight = NS_UNCONSTRAINEDSIZE;
 
  nsSize scrolledAreaSize(0,0);
  nsHTMLReflowMetrics  scrolledAreaDesiredSize(&scrolledAreaSize);


  if (eReflowReason_Incremental == firstPassState.reason) {
    nsIFrame* targetFrame;
    firstPassState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIReflowCommand::ReflowType type;
      aReflowState.reflowCommand->GetType(type);
      firstPassState.reason = eReflowReason_StyleChange;
      firstPassState.reflowCommand = nsnull;
    }
  }

  nsScrollFrame::Reflow(aPresContext, 
                    scrolledAreaDesiredSize,
                    firstPassState, 
                    aStatus);

    // Compute the bounding box of the contents of the list using the area 
    // calculated by the first reflow as a starting point.

  nscoord scrolledAreaWidth = scrolledAreaDesiredSize.maxElementSize->width;
  nscoord scrolledAreaHeight = scrolledAreaDesiredSize.height;
  mMaxWidth = scrolledAreaWidth;
  mMaxHeight = scrolledAreaDesiredSize.maxElementSize->height;

    // The first reflow produces a box with the scrollbar width and borders
    // added in so we need to subtract them out.

    // Retrieve the scrollbar's width and height
  float sbWidth = 0.0;
  float sbHeight = 0.0;;
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext.GetDeviceContext(getter_AddRefs(dc));
  dc->GetScrollBarDimensions(sbWidth, sbHeight);
   // Convert to nscoord's by rounding
  nscoord scrollbarWidth = NSToCoordRound(sbWidth);
  nscoord scrollbarHeight = NSToCoordRound(sbHeight);

    // Subtract out the scrollbar width
  scrolledAreaWidth -= scrollbarWidth;

    // Subtract out the borders
  nsMargin border;
  if (!aReflowState.mStyleSpacing->GetBorder(border)) {
    NS_NOTYETIMPLEMENTED("percentage border");
    border.SizeTo(0, 0, 0, 0);
  }
  //XXX: Should just get the border.top when needed instead of
  //caching it.
  mBorderOffsetY = border.top;

  scrolledAreaWidth -= (border.left + border.right);
  scrolledAreaHeight -= (border.top + border.bottom);

    // Now the scrolledAreaWidth and scrolledAreaHeight are exactly 
    // wide and high enough to enclose their contents

  nscoord visibleWidth = 0;
  if (IsInDropDownMode() == PR_TRUE) {
     // Calculate visible width for dropdown
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
      visibleWidth = scrolledAreaWidth;
    } else {
      visibleWidth = aReflowState.mComputedWidth - (border.left + border.right);
    }
  } else {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
      visibleWidth = scrolledAreaWidth;
    } else {
      visibleWidth = aReflowState.mComputedWidth;
    }
  }
  
   // Determine if a scrollbar will be needed, If so we need to add
   // enough the width to allow for the scrollbar.
   // The scrollbar will be needed under two conditions:
   // (size * heightOfaRow) < scrolledAreaHeight or
   // the height set through Style < scrolledAreaHeight.

   // Calculate the height of a single row in the listbox or dropdown list
   // Note: It is calculated based on what layout returns for the maxElement
   // size, rather than trying to take the scrolledAreaHeight and dividing by the number 
   // of option elements. The reason is that their may be option groups in addition to
   //  option elements. Either of which may be visible or invisible.
  PRInt32 heightOfARow = scrolledAreaDesiredSize.maxElementSize->height;
  heightOfARow -= (border.top + border.bottom);

  nscoord visibleHeight = 0;
  if (IsInDropDownMode() == PR_TRUE) {
    // Compute the visible height of the drop-down list
    // The dropdown list height is the smaller of it's height setting or the height
    // of the smallest box that can drawn around it's contents.
    visibleHeight = scrolledAreaHeight;

    if (visibleHeight > (kMaxDropDownRows * heightOfARow)) {
       visibleHeight = (kMaxDropDownRows * heightOfARow);
    }
   
  } else {
      // Calculate the visible height of the listbox
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
      visibleHeight = aReflowState.mComputedHeight;
    } else {
      PRInt32 numRows = 1;
      GetSizeAttribute(&numRows);
      if (numRows == kNoSizeSpecified) {
        visibleHeight = aReflowState.mComputedHeight;
      }
      else {
        visibleHeight = numRows * heightOfARow;
      }
    }
  }

  PRBool needsVerticalScrollbar = PR_FALSE;
  if (visibleHeight < scrolledAreaHeight) {
    needsVerticalScrollbar = PR_TRUE; 
  }

  if ((needsVerticalScrollbar) && (IsInDropDownMode() == PR_FALSE)) {
    visibleWidth += scrollbarWidth;
  }

   // Do a second reflow with the adjusted width and height settings
   // This sets up all of the frames with the correct width and height.
  secondPassState.mComputedWidth = visibleWidth;
  secondPassState.mComputedHeight = visibleHeight;
  secondPassState.reason = eReflowReason_Resize;
  nsScrollFrame::Reflow(aPresContext, 
                    aDesiredSize,
                    secondPassState, 
                    aStatus);

    // Set the max element size to be the same as the desired element size.
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  mDisplayed = PR_TRUE;
  
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}


NS_IMETHODIMP
nsListControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}


PRBool 
nsListControlFrame::IsOptionElement(nsIContent* aContent)
{
  PRBool result = PR_FALSE;
 
  nsIDOMHTMLOptionElement* oe = nsnull;
  if (NS_SUCCEEDED(aContent->QueryInterface(kIDOMHTMLOptionElementIID, (void**) &oe))) {      
    if (oe != nsnull) {
      result = PR_TRUE;
      NS_RELEASE(oe);
    }
  }
 
  return result;
}


PRBool 
nsListControlFrame::IsOptionElementFrame(nsIFrame *aFrame)
{
  nsIContent *content = nsnull;
  aFrame->GetContent(&content);
  PRBool result = PR_FALSE;
  if (nsnull != content) {
    result = IsOptionElement(content);
    NS_RELEASE(content);
  }
  return(result);
}


// Go up the frame tree looking for the first ancestor that has content
// which is selectable

nsIFrame *
nsListControlFrame::GetSelectableFrame(nsIFrame *aFrame)
{
  nsIFrame* selectedFrame = aFrame;
  
  while ((nsnull != selectedFrame) && 
         (PR_FALSE ==IsOptionElementFrame(selectedFrame))) {
      selectedFrame->GetParent(&selectedFrame);
  }  

  return(selectedFrame);  
}

void 
nsListControlFrame::ForceRedraw() 
{
  //XXX: Hack. This should not be needed. The problem is DisplaySelected
  //and DisplayDeselected set and unset an attribute on generic HTML content
  //which does not know that it should repaint as the result of the attribute
  //being set. This should not be needed once the event state manager handles
  //selection.
  nsFormControlHelper::ForceDrawFrame(this);
}


// XXX: Here we introduce a new -moz-option-selected attribute so a attribute
// selecitor n the ua.css can change the style when the option is selected.

void 
nsListControlFrame::DisplaySelected(nsIContent* aContent) 
{
   //XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
   // -moz-option-selected in the ua.css style sheet. This will not be needed when
   // The event state manager supports selected states. KMM
  
  nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)));
  if (PR_TRUE == mDisplayed) {
    aContent->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_TRUE);
    ForceRedraw();
  } else {
    aContent->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_FALSE);
  }
}

void 
nsListControlFrame::DisplayDeselected(nsIContent* aContent) 
{
   //XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
   // -moz-option-selected in the ua.css style sheet. This will not be needed when
   // The event state manager is functional. KMM

  nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)));
 
  if (PR_TRUE == mDisplayed) {
    aContent->UnsetAttribute(kNameSpaceID_None, selectedAtom, PR_TRUE);
    ForceRedraw();
  } else {
    aContent->UnsetAttribute(kNameSpaceID_None, selectedAtom, PR_FALSE);
  }

}


PRInt32 
nsListControlFrame::GetSelectedIndex(nsIFrame *aHitFrame) 
{
  PRInt32 index   = kNothingSelected;
  // Get the content of the frame that was selected
  nsIContent* selectedContent = nsnull;
  NS_ASSERTION(aHitFrame, "No frame for html <select> element\n");  
  if (aHitFrame) {
    aHitFrame->GetContent(&selectedContent);

    // Search the list of option elements looking for a match
 
    PRInt32 length = 0;
    GetNumberOfOptions(&length);
    nsIDOMHTMLCollection* options = GetOptions(mContent);
    if (nsnull != options) {
      PRUint32 numOptions;
      options->GetLength(&numOptions);
      for (PRUint32 optionX = 0; optionX < numOptions; optionX++) {
        nsIContent* option = nsnull;
        option = GetOptionAsContent(options, optionX);
        if (nsnull != option) {
          if (option == selectedContent) {
            index = optionX;
            break;
          }
         NS_RELEASE(option);
        }
      }
      NS_RELEASE(options);
    }
  }
  return index;
}


void 
nsListControlFrame::ClearSelection()
{
  PRInt32 length = 0;
  GetNumberOfOptions(&length);
  for (PRInt32 i = 0; i < length; i++) {
    if (i != mSelectedIndex) {
      SetFrameSelected(i, PR_FALSE);
     } 
  }
}

void 
nsListControlFrame::ExtendedSelection(PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aDoInvert, PRBool aSetValue)
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
  PRBool startInverting = PR_FALSE;

  PRInt32 length = 0;
  GetNumberOfOptions(&length);
  for (i = 0; i < length; i++) {
    if (i == startInx) {
      startInverting = PR_TRUE;
    }
    if (startInverting && ((i != mStartExtendedIndex && aDoInvert) || !aDoInvert)) {
      if (aDoInvert) {
        SetFrameSelected(i, !IsFrameSelected(i));
      } else {
        SetFrameSelected(i, aSetValue);
      }
    }
    if (i == endInx) {
      startInverting = PR_FALSE;
    }
  }
}

void 
nsListControlFrame::SingleSelection()
{
  if (nsnull != mHitFrame) {
     // Store previous selection
    PRInt32 oldSelectedIndex = mSelectedIndex;
     // Get Current selection
    PRInt32 newSelectedIndex = (PRInt32)GetSelectedIndex(mHitFrame);
    if (newSelectedIndex != kNothingSelected) {
      if (oldSelectedIndex != newSelectedIndex) {
          // Deselect the previous selection if there is one
        if (oldSelectedIndex != kNothingSelected) {
          SetFrameSelected(oldSelectedIndex, PR_FALSE);
        }
          // Display the new selection
        SetFrameSelected(newSelectedIndex, PR_TRUE);
        mSelectedIndex = newSelectedIndex;
      } else {
        // Selecting the currently selected item so do nothing.
      }
    }
  }
}

void 
nsListControlFrame::MultipleSelection(PRBool aIsShift, PRBool aIsControl)
{
  if (nsnull != mHitFrame) {
  mSelectedIndex = (PRInt32)GetSelectedIndex(mHitFrame);
  if (kNothingSelected != mSelectedIndex) {
      if ((aIsShift) || (mButtonDown && (!aIsControl))) {
          // Shift is held down
        SetFrameSelected(mSelectedIndex, PR_TRUE);
        if (mEndExtendedIndex == kNothingSelected) {
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
      } else if (aIsControl) {
         // Control is held down
        if (IsFrameSelected(mSelectedIndex)) {
          SetFrameSelected(mSelectedIndex, PR_FALSE);
        } else {
          SetFrameSelected(mSelectedIndex, PR_TRUE);
        }
 
      } else {
         // Neither control nor shift is held down
        SetFrameSelected(mSelectedIndex, PR_TRUE);
        mStartExtendedIndex = mSelectedIndex;
        mEndExtendedIndex   = kNothingSelected;
        ClearSelection();
      }
     }
   }
}

void 
nsListControlFrame::HandleListSelection(nsIPresContext& aPresContext, 
                                        nsGUIEvent*     aEvent,
                                        nsEventStatus&  aEventStatus)
{
  PRBool multipleSelections = PR_FALSE;
  GetMultiple(&multipleSelections);
  if (multipleSelections) {
    MultipleSelection(((nsMouseEvent *)aEvent)->isShift, ((nsMouseEvent *)aEvent)->isControl);
  } else {
    SingleSelection();
  }
}

PRBool 
nsListControlFrame::HasSameContent(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
   // Quick check, if the frames are equal they must have
   // the same content
  if (aFrame1 == aFrame2)
    return PR_TRUE;

  PRBool result = PR_FALSE;
  nsIContent* content1 = nsnull;
  nsIContent* content2 = nsnull;
  aFrame1->GetContent(&content1);
  aFrame2->GetContent(&content2);
  if (aFrame1 == aFrame2) {
    result = PR_TRUE;
  }
  
  NS_IF_RELEASE(content1);
  NS_IF_RELEASE(content2);
  return(result);
}


nsresult
nsListControlFrame::HandleLikeListEvent(nsIPresContext& aPresContext, 
                                               nsGUIEvent*     aEvent,
                                               nsEventStatus&  aEventStatus)
{
  if (nsnull != mHitFrame) {

    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
      HandleListSelection(aPresContext, aEvent, aEventStatus);
      mButtonDown = PR_TRUE;
      CaptureMouseEvents(PR_TRUE);
      mLastFrame = mHitFrame;
    } else if (aEvent->message == NS_MOUSE_MOVE) {
      if ((PR_TRUE == mButtonDown) && (! HasSameContent(mLastFrame, mHitFrame))) {
        HandleListSelection(aPresContext, aEvent, aEventStatus);
        mLastFrame = mHitFrame;
      }
    } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      mButtonDown = PR_FALSE;
      CaptureMouseEvents(PR_FALSE);
    }

  }

  aEventStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame :: CaptureMouseEvents(PRBool aGrabMouseEvents)
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

nsIView* 
nsListControlFrame::GetViewFor(nsIWidget* aWidget)
{           
  nsIView*  view = nsnull;
  void*     clientData;

  NS_PRECONDITION(nsnull != aWidget, "null widget ptr");
	
  // The widget's client data points back to the owning view
  if (aWidget && NS_SUCCEEDED(aWidget->GetClientData(clientData))) {
    view = (nsIView*)clientData;
  }

  return view;
}

// Determine if a view is an ancestor of another view.

PRBool 
nsListControlFrame::IsAncestor(nsIView* aAncestor, nsIView* aChild)
{
  nsIView* view = aChild;
  while (nsnull != view) {
    if (view == aAncestor)
       // Is an ancestor
      return(PR_TRUE);
    else {
      view->GetParent(view);
    }
  }
   // Not an ancestor
  return(PR_FALSE);
}


nsresult
nsListControlFrame::HandleLikeDropDownListEvent(nsIPresContext& aPresContext, 
                                                nsGUIEvent*     aEvent,
                                                nsEventStatus&  aEventStatus)
{
  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    if (nsnull == mHitFrame) {
        // Button down without hitting a frame in the drop down list.
     
         // May need to give the scrollbars a chance at the event.
         // The drop down list's scrollbar view will be a descendant
         // of the drop down list's view. So check to see if the view
         // that associated with event's widget is a descendant.
         // If so, then we do not pop the drop down list back up.
        nsIView* eventView = GetViewFor(aEvent->widget);
        nsIView* view=nsnull;
        GetView(&view);
        if (PR_TRUE == IsAncestor(view, eventView)) {
          return NS_OK;
        }
         // Roll the drop-down list back up.
      mComboboxFrame->ListWasSelected(&aPresContext);     
      return NS_OK;
      }
  } 
  
  // Mouse Move behavior is as follows:
  // When the DropDown occurs, if an item is selected it displayed as being selected.
  // It may or may not be currently visible, when the mouse is moved across any item 
  // in the the list it is displayed as the currently selected item. 
  // (Selection doesn't actually occur until the mouse is released, or clicked and released)
  if (aEvent->message == NS_MOUSE_MOVE) {

    // If the DropDown is currently has a selected item 
    // then clear the selected item
   
    if (nsnull != mHitFrame) {
      PRInt32 oldSelectedIndex = mSelectedIndex;
      PRInt32 newSelectedIndex = GetSelectedIndex(mHitFrame);
      if (kNothingSelected != newSelectedIndex) {
        if (oldSelectedIndex != newSelectedIndex) {
          if (oldSelectedIndex != kNothingSelected) {
            SetFrameSelected(oldSelectedIndex, PR_FALSE);
          }
          SetFrameSelected(newSelectedIndex, PR_TRUE);
          mSelectedIndex = newSelectedIndex;
        }
      }
    }       
  } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
    // Start by finding the newly "hit" content from the hit frame
      if (nsnull != mHitFrame) {
        PRInt32 index = GetSelectedIndex(mHitFrame);
        if (kNothingSelected != index) {
          SetFrameSelected(index, PR_TRUE);  
          mSelectedIndex = index;
        }
    
      if (mComboboxFrame) {
        mComboboxFrame->ListWasSelected(&aPresContext); 
      } 
    } 

  } 

  aEventStatus = nsEventStatus_eConsumeNoDefault; 
  return NS_OK;
}


NS_IMETHODIMP 
nsListControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus&  aEventStatus)
{
/*
  const char * desc[] = {"NS_MOUSE_MOVE", 
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
  }
*/
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->mVisible) {
    return NS_OK;
  }

  if (IsInDropDownMode() == PR_TRUE) {
    HandleLikeDropDownListEvent(aPresContext, aEvent, aEventStatus);
  } else {
    HandleLikeListEvent(aPresContext, aEvent, aEventStatus);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsListControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  mContentFrame = aChildList;

  if (IsInDropDownMode() == PR_FALSE) {
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }

  return nsScrollFrame::SetInitialChildList(aPresContext, aListName, aChildList);
}

nsresult
nsListControlFrame::GetSizeAttribute(PRInt32 *aSize) {
  nsresult rv = NS_OK;
  nsIDOMHTMLSelectElement* select;
  rv = mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**) &select);
  if (mContent && (NS_SUCCEEDED(rv))) {
    rv = select->GetSize(aSize);
    NS_RELEASE(select);
  }
  return rv;
}


NS_IMETHODIMP  
nsListControlFrame::Init(nsIPresContext&  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  nsresult result = nsScrollFrame::Init(aPresContext, aContent, aParent, aContext,
                                        aPrevInFlow);

   // Initialize the current selected and not selected state's for
   // the listbox items from the content. This is done here because
   // The selected content sets an attribute that must be on the content
   // before the option element's frames are constructed so the frames will
   // get the proper style based on attribute selectors which refer to the
   // selected attribute.
  if (!mIsInitializedFromContent) {
    InitializeFromContent();
  }
  
  return result;
}


nscoord 
nsListControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                      nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(0, aPixToTwip); 
}


nscoord 
nsListControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                        float aPixToTwip, 
                                        nscoord aInnerWidth,
                                        nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPixToTwip, aInnerWidth);
}


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


NS_IMETHODIMP 
nsListControlFrame::AboutToDropDown()
{
    // Resync the view's position with the frame.
  return(SyncViewWithFrame());
}

nsIContent* 
nsListControlFrame::GetOptionAsContent(nsIDOMHTMLCollection* aCollection,PRUint32 aIndex) 
{
  nsIContent *content = nsnull;
  nsIDOMHTMLOptionElement* optionElement = GetOption(*aCollection, aIndex);
  if (nsnull != optionElement) {
    content = nsnull;
    nsresult result = optionElement->QueryInterface(kIContentIID, (void**) &content);
    NS_RELEASE(optionElement);
  }
 
  return content;
}

nsIContent* 
nsListControlFrame::GetOptionContent(PRUint32 aIndex)
  
{
  nsIContent* content = nsnull;
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  if (nsnull != options) {
    content = GetOptionAsContent(options, aIndex);
    NS_RELEASE(options);
  }
  return(content);
}

PRBool 
nsListControlFrame::IsContentSelected(nsIContent* aContent)
{
  nsString value; 
  nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)));
  nsresult result = aContent->GetAttribute(kNameSpaceID_None, selectedAtom, value);
 
  if (NS_CONTENT_ATTR_NOT_THERE == result) {
    return(PR_FALSE);
  }
  else {
    return(PR_TRUE);
  }
}


PRBool 
nsListControlFrame::IsFrameSelected(PRUint32 aIndex) 
{
  nsIContent* content = GetOptionContent(aIndex);
  NS_ASSERTION(nsnull != content, "Failed to retrieve option content");
  PRBool result = IsContentSelected(content);
  NS_RELEASE(content);
  return result;
}

void 
nsListControlFrame::SetFrameSelected(PRUint32 aIndex, PRBool aSelected)
{
  nsIContent* content = GetOptionContent(aIndex);
  NS_ASSERTION(nsnull != content, "Failed to retrieve option content");
  if (aSelected) {
    DisplaySelected(content);
  } else {
    DisplayDeselected(content);
  }
  NS_IF_RELEASE(content);
}

void 
nsListControlFrame::InitializeFromContent()
{
  PRInt32 length = 0;
  GetNumberOfOptions(&length);
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  nsresult result = NS_OK;
  if (nsnull != options) {
    for (PRInt32 i = 0; i < length; i++) {
      nsIDOMHTMLOptionElement* optionElement = nsnull;
      optionElement = GetOption(*options, i);
      if (nsnull != optionElement) {
        PRBool selected;
        optionElement->GetDefaultSelected(&selected);
          // Set the selected index for single selection list boxes.
        if (selected) {
          ToggleSelected(i); 
        }
        NS_RELEASE(optionElement);
     }
   }
   NS_RELEASE(options);
  }

}


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

nsIDOMHTMLOptionElement* 
nsListControlFrame::GetOption(nsIDOMHTMLCollection& aCollection, PRUint32 aIndex)
{
  nsIDOMNode* node = nsnull;
  if (NS_SUCCEEDED(aCollection.Item(aIndex, &node))) {
    if (nsnull != node) {
      nsIDOMHTMLOptionElement* option = nsnull;
      node->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
      NS_RELEASE(node);
      return option;
    }
  }
  return nsnull;
}


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

nsresult 
nsListControlFrame::Deselect() 
{
  PRInt32 i;
  PRInt32 max = 0;
  GetNumberOfOptions(&max);
  for (i=0;i<max;i++) {
    SetFrameSelected(i, PR_FALSE);
  }
  mSelectedIndex = kNothingSelected;
  
  return NS_OK;
}

PRIntn
nsListControlFrame::GetSkipSides() const
{    
    // Don't skip any sides during border rendering
  return 0;
}

NS_IMETHODIMP
nsListControlFrame::GetType(PRInt32* aType) const
{
  *aType = NS_FORM_SELECT;
  return NS_OK;
}

void 
nsListControlFrame::SetFormFrame(nsFormFrame* aFormFrame) 
{ 
  mFormFrame = aFormFrame; 
}


PRBool
nsListControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}


void 
nsListControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
}


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


void 
nsListControlFrame::Reset() 
{
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
        SetFrameSelected(i, PR_TRUE);
      }
      NS_RELEASE(option);
    }
  }
  NS_RELEASE(options);
} 

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
 

PRInt32 
nsListControlFrame::GetNumberOfSelections()
{
  PRInt32 count = 0;
  PRInt32 length = 0;
  GetNumberOfOptions(&length);
  PRInt32 i = 0;
  for (i = 0; i < length; i++) {
    if (IsFrameSelected(i)) {
      count++;
    }
  }
  return(count);
}


PRBool
nsListControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                     nsString* aValues, nsString* aNames)
{
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

  PRBool status = PR_FALSE;
  PRBool multiple;
  GetMultiple(&multiple);
  if (!multiple) {
    if (mSelectedIndex >= 0) {
      nsAutoString value;
      GetOptionValue(*options, mSelectedIndex, value);
      aNumValues = 1;
      aNames[0]  = name;
      aValues[0] = value;
      status = PR_TRUE;
    }
  } 
  else {
    aNumValues = 0;
    PRInt32 length = 0;
    GetNumberOfOptions(&length);
    for (int i = 0; i < length; i++) {
      if (PR_TRUE == IsFrameSelected(i)) {
        nsAutoString value;
        GetOptionValue(*options, i, value);
        aNames[aNumValues]  = name;
        aValues[aNumValues] = value;
        aNumValues++;
      }
    }
    status = PR_TRUE;
  } 
  NS_RELEASE(options);

  return status;
}


void 
nsListControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  // XXX:TODO Make set focus work
}


NS_IMETHODIMP 
nsListControlFrame::SetComboboxFrame(nsIFrame* aComboboxFrame)
{
  nsresult rv = NS_OK;
  static NS_DEFINE_IID(kIComboboxIID, NS_ICOMBOBOXCONTROLFRAME_IID);
  if (nsnull != aComboboxFrame) {
    rv = aComboboxFrame->QueryInterface(kIComboboxIID, (void**) &mComboboxFrame); 
  }
  return rv;
}


NS_IMETHODIMP 
nsListControlFrame::GetSelectedItem(nsString & aStr)
{
  aStr = "";
  nsresult rv = NS_ERROR_FAILURE; 
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  if (nsnull != options) {
    nsIDOMHTMLOptionElement* optionElement = GetOption(*options, mSelectedIndex);
    if (nsnull != optionElement) {
      nsAutoString text;
      if (NS_CONTENT_ATTR_HAS_VALUE == optionElement->GetText(text)) {
        aStr = text;
        rv = NS_OK;
      }
      NS_RELEASE(optionElement);
    }
    NS_RELEASE(options);
  }

  return rv;
}

PRBool nsListControlFrame::IsInDropDownMode()
{
  return((nsnull == mComboboxFrame) ? PR_FALSE : PR_TRUE);
}

nsresult nsListControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsListControlFrame::GetNumberOfOptions(PRInt32* aNumOptions) 
{
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  if (nsnull == options) {
    *aNumOptions = 0;
  } else {
    PRUint32 length = 0;
    options->GetLength(&length);
    *aNumOptions = (PRInt32)length;
    NS_RELEASE(options);
  }

  return NS_OK;
}

// Select the specified item in the listbox using control logic.
// If it a single selection listbox the previous selection will be
// de-selected. 

void 
nsListControlFrame::ToggleSelected(PRInt32 aIndex)
{
  PRBool multiple;
  GetMultiple(&multiple);

  if (PR_TRUE == multiple) {
    SetFrameSelected(aIndex, PR_TRUE);
  } else {
    if (mSelectedIndex != kNothingSelected) {
      SetFrameSelected(mSelectedIndex, PR_FALSE);
    }
    SetFrameSelected(aIndex, PR_TRUE);
    mSelectedIndex = aIndex;
  }
}


NS_IMETHODIMP 
nsListControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::selected == aName) {
    return NS_ERROR_INVALID_ARG; // Selected is readonly according to spec.
  } else if (nsHTMLAtoms::selectedindex == aName) {
    PRInt32 error = 0;
    PRInt32 selectedIndex = aValue.ToInteger(&error, 10); // Get index from aValue
    if (error) {
      return NS_ERROR_INVALID_ARG; // Couldn't convert to integer
    } else {
       // Select the specified item in the list box. 
      ToggleSelected(selectedIndex);
    }
  }
 
  return NS_OK;
}

NS_IMETHODIMP 
nsListControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Get the selected value of option from local cache (optimization vs. widget)
  if (nsHTMLAtoms::selected == aName) {
    PRInt32 error = 0;
    PRBool selected = PR_FALSE;
    PRInt32 index = aValue.ToInteger(&error, 10); // Get index from aValue
    if (error == 0)
       selected = IsFrameSelected(index); 
  
    nsFormControlHelper::GetBoolString(selected, aValue);
    
  // For selectedIndex, get the value from the widget
  } else  if (nsHTMLAtoms::selectedindex == aName) {
    aValue.Append(mSelectedIndex, 10);
  }

  return NS_OK;
}


// Create a Borderless top level widget for drop-down lists.
nsresult 
nsListControlFrame::CreateScrollingViewWidget(nsIView* aView, const nsStylePosition* aPosition)
{
  if (IsInDropDownMode() == PR_TRUE) {
    nsWidgetInitData widgetData;
    aView->SetZIndex(kMaxZ);
    widgetData.mWindowType = eWindowType_popup;
    widgetData.mBorderStyle = eBorderStyle_default;
    static NS_DEFINE_IID(kCChildCID,  NS_CHILD_CID);
    aView->CreateWidget(kCChildCID,
                       &widgetData,
                       nsnull);
    return NS_OK;
  } else {
    return nsScrollFrame::CreateScrollingViewWidget(aView, aPosition);
  }
}

void
nsListControlFrame::GetViewOffset(nsIViewManager* aManager, nsIView* aView, 
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
nsListControlFrame::SyncViewWithFrame()
{
    // Resync the view's position with the frame.
    // The problem is the dropdown's view is attached directly under
    // the root view. This means it's view needs to have it's coordinates calculated
    // as if it were in it's normal position in the view hierarchy.

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
  
  viewManager->ResizeView(view, mRect.width, mRect.height);
  viewManager->MoveViewTo(view, parentPos.x + offset.x, parentPos.y + offset.y );

  return NS_OK;
}

nsresult
nsListControlFrame::GetScrollingParentView(nsIFrame* aParent, nsIView** aParentView)
{
  if (IsInDropDownMode() == PR_TRUE) {
     // Use the parent frame to get the view manager
    nsIView* parentView = nsnull;
    nsresult rv = aParent->GetView(&parentView);
    NS_ASSERTION(parentView, "GetView failed");
    nsCOMPtr<nsIViewManager> viewManager;
    parentView->GetViewManager(*getter_AddRefs(viewManager));
    NS_ASSERTION(viewManager, "GetViewManager failed");

     // Ask the view manager for the root view and
     // use it as the parent for popup scrolling lists.
     // Using the normal view as the parent causes the 
     // drop-down list to be clipped to a parent view. 
     // Using the root view as the parent
     // prevents this from happening. 
    viewManager->GetRootView(*aParentView);
    NS_ASSERTION(aParentView, "GetRootView failed");  
    return rv;
   } else {
     return nsScrollFrame::GetScrollingParentView(aParent, aParentView);
   }
}

NS_IMETHODIMP
nsListControlFrame::DidReflow(nsIPresContext& aPresContext,
                   nsDidReflowStatus aStatus)
{
  if (PR_TRUE == IsInDropDownMode()) 
  {
    nsresult rv = nsScrollFrame::DidReflow(aPresContext, aStatus);
    SyncViewWithFrame();
    return rv;
  } else {
    return nsScrollFrame::DidReflow(aPresContext, aStatus);
  }
}

NS_IMETHODIMP 
nsListControlFrame::GetMaximumSize(nsSize &aSize)
{
  aSize.width = mMaxWidth;
  aSize.height = mMaxHeight;
  return NS_OK;
}


NS_IMETHODIMP 
nsListControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}


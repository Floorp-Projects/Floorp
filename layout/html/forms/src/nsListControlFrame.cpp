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
#include "nsIDeviceContext.h" 
#include "nsIHTMLContent.h"
#include "nsIDOMHTMLCollection.h" 
#include "nsIDOMHTMLSelectElement.h" 
#include "nsIDOMHTMLOptionElement.h" 
#include "nsIDOMNode.h" 
#include "nsHTMLAtoms.h" 
#include "nsIAtom.h" 
#include "nsIDOMEventReceiver.h"
#include "nsIComboboxControlFrame.h"
#include "nsComboboxControlFrame.h" // for the static helper function
#include "nsIListControlFrame.h"
#include "nsIViewManager.h"
#include "nsFormFrame.h"
#include "nsIScrollableView.h"
#include "nsVoidArray.h"
#include "nsIDOMHTMLOptGroupElement.h"



// Constants
const char * kNormal        = "";
const char * kSelected      = "SELECTED";
const char * kSelectedFocus = "SELECTEDFOCUS";
const nscoord kMaxDropDownRows = 3;
const PRInt32 kNothingSelected = -1;

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
static NS_DEFINE_IID(kIDOMNodeIID,              NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMElementIID,           NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIFrameIID,                NS_IFRAME_IID);
static NS_DEFINE_IID(kIDOMHTMLOptGroupElementIID, NS_IDOMHTMLOPTGROUPELEMENT_IID);

//----------------------------------------------------------------------

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

//----------------------------------------------------------------------
nsListControlFrame::nsListControlFrame()
{
  mHitFrame         = nsnull;
  mCurrentHitFrame  = nsnull;
  mSelectedContent  = nsnull;
  mSelectedIndex    = kNothingSelected;
  mNumRows          = 0;
  mIsInitializedFromContent = PR_FALSE;
  mNumSelections  = 0;
  mInDropDownMode = PR_FALSE;
  mComboboxFrame  = nsnull;
  mFormFrame      = nsnull;
  mSelectableFrames = new nsVoidArray;
}

//----------------------------------------------------------------------
nsListControlFrame::~nsListControlFrame()
{
  mComboboxFrame = nsnull;
  mFormFrame = nsnull;
  delete mSelectableFrames;
  mSelectableFrames = nsnull;
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

  return nsScrollFrame::QueryInterface(aIID, aInstancePtr);
}

PRBool nsListControlFrame::IsOptionGroup(nsIFrame* aFrame)
{
  PRBool result = PR_FALSE;
  nsIContent*content = nsnull;
  aFrame->GetContent(&content);
  nsIDOMHTMLOptGroupElement* og = nsnull;
  if (content && (NS_OK == content->QueryInterface(kIDOMHTMLOptGroupElementIID, (void**) &og))) {      
    if (og != nsnull) {
      result = PR_TRUE;
      NS_RELEASE(og);
    }
    NS_RELEASE(content);
  }
 
  return result;
}

void nsListControlFrame::ConstructSelectableList(nsIFrame* aFrame, nsVoidArray* aList)
{
  nsIFrame* kid = nsnull;
  aFrame->FirstChild(nsnull, &kid);
  while (nsnull != kid) {
    if (IsOptionGroup(kid)) {
      ConstructSelectableList(kid, aList);
    } else {
      aList->AppendElement(kid);
      kid->GetNextSibling(&kid);
    }
  }
}

nsIFrame* nsListControlFrame::GetFirstSelectableFrame(nsVoidArray *aList, PRInt32& aPos)
{
  aPos = 1;
  return((nsIFrame*)aList->ElementAt(0));
}

nsIFrame* nsListControlFrame::GetNextSelectableFrame(nsVoidArray *aList, PRInt32& aPos)
{
  return((nsIFrame*)aList->ElementAt(aPos++));
}


//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
   // Save the result of GetFrameForPointUsing in the mHitFrame member variable.
   // mHitFrame is used later in the HandleLikeListEvent to determine what was clicked on.
   // XXX: This is kludgy, but there doesn't seem to be a way to get what was just clicked
   // on in the HandleEvent. The GetFrameForPointUsing is always called before the HandleEvent.
   //
  nsresult rv;
  
  nsIFrame *childFrame;
  FirstChild(nsnull, &childFrame);
  rv = GetFrameForPointUsing(aPoint, nsnull, aFrame);
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
        // Note: scrollableView is not ref counted so it's interface is not NS_RELEASE'd 
     }
   }

#ifdef NS_OPTGROUPSUPPORT
  nsIFrame* selectedFrame = nsnull;
  nsresult rv = nsScrollFrame::GetFrameForPoint(tmp, &selectedFrame);


  // Walk up the frames looking for a selectable frame.
  // A selectable frame is defined as a frame sitting under the ScrollArea's 
  // Area Frame or the first optiongroup ancestor.
  
  if (nsnull != selectedFrame) {
    PRBool done = PR_FALSE;
    while (! done) {
      nsIFrame* parent = nsnull;
      selectedFrame->GetParent(&parent);
      if (parent != nsnull) {
        if ((parent == mContentFrame) || (IsOptionGroup(parent))) {
          done = PR_TRUE;
        } else {
            // Continue looking at ancestors
          selectedFrame = parent;
        }
      } else {
         // Parent is null so stop looking
        done = PR_TRUE;
      }
    }
  }

  *aFrame = selectedFrame;

  return rv;

#else

//XXX: Remove the following code when the code above works

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
#endif
}


//----------------------------------------------------------------------
// Reflow is overriden to constrain the listbox height to the number of rows and columns
// specified. 
//
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

  // Initialize the current selected and not selected state's for
   // the listbox items from the content
  if (!mIsInitializedFromContent) {
    mIsInitializedFromContent = PR_TRUE;
    InitializeFromContent(PR_TRUE);
  }
 
 
  //--Calculate a width just big enough for the scrollframe to shrink around the
  //longest element in the list
  nsHTMLReflowState secondPassState(aReflowState);
  nsHTMLReflowState firstPassState(aReflowState);
 
   // Get the size of option elements inside the listbox
   // Compute the width based on the longest line in the listbox.
  
  firstPassState.computedWidth = NS_UNCONSTRAINEDSIZE;
  firstPassState.computedHeight = NS_UNCONSTRAINEDSIZE;
  firstPassState.availableWidth = NS_UNCONSTRAINEDSIZE;
  firstPassState.availableHeight = NS_UNCONSTRAINEDSIZE;

  
  nsSize scrolledAreaSize(0,0);
  nsHTMLReflowMetrics  scrolledAreaDesiredSize(&scrolledAreaSize);
  nsScrollFrame::Reflow(aPresContext, 
                    scrolledAreaDesiredSize,
                    firstPassState, 
                    aStatus);

    // Compute the bounding box of the contents of the list using the area 
    // calculated by the first reflow as a starting point.

  nscoord scrolledAreaWidth = scrolledAreaDesiredSize.maxElementSize->width;
  nscoord scrolledAreaHeight = scrolledAreaDesiredSize.height;

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
  if (mInDropDownMode) {
    // Always set the width to be
    // XXX: TODO The width of the list should be passed in so it can
    // be set to the same as the text box + dropdown arrow.
    visibleWidth = scrolledAreaWidth;
    if (nsnull != mComboboxFrame) {
      nsRect comboRect;
      nsIFrame *comboFrame = nsnull;
      if (NS_SUCCEEDED(mComboboxFrame->QueryInterface(kIFrameIID, (void **)&comboFrame))) {
        comboFrame->GetRect(comboRect);
        visibleWidth = comboRect.width;
        visibleWidth -= (border.left + border.right);
      }
    } 

  } else {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.computedWidth) {
      visibleWidth = scrolledAreaWidth;
    } else {
      visibleWidth = aReflowState.computedWidth;
    }
  }
  
    // Determine if a scrollbar will be needed, If so we need to add
    // enough the width to allow for the scrollbar.
    // The scrollbar will be needed under two conditions:
    // (size * heightOfaRow) < scrolledAreaHeight or
    // the height set through Style < scrolledAreaHeight.

    // Calculate the height of a row. 
    // XXX: Fix this for optgroup's. 
  nsIFrame* childFrame = nsnull;
  mContentFrame->FirstChild(nsnull, &childFrame);
  PRInt32 numChildren = LengthOf(childFrame); 
  PRInt32 heightOfARow = scrolledAreaHeight / numChildren;

  nscoord visibleHeight = 0;
  if (mInDropDownMode) {
      // Compute the visible height of the drop-down list
      // The dropdown list height is the smaller of it's height setting or the height
      // of the smallest box that can drawn around it's contents.
      visibleHeight = scrolledAreaHeight;

      if (visibleHeight > (kMaxDropDownRows * heightOfARow))
         visibleHeight = (kMaxDropDownRows * heightOfARow);
   
  } else {
      // Calculate the visible height of the listbox
    if (NS_UNCONSTRAINEDSIZE != aReflowState.computedHeight) {
      visibleHeight = aReflowState.computedHeight;
    } else {
      if (numChildren == mNumRows) {
          // Make sure scrollbars go away when the number of rows
          // matchs the number of items to scroll.
        visibleHeight = aReflowState.computedHeight;
      } else {
        visibleHeight = mNumRows * heightOfARow;
      }
    }
  }

  PRBool needsVerticalScrollbar = PR_FALSE;
  if (visibleHeight < scrolledAreaHeight) {
    needsVerticalScrollbar = PR_TRUE; 
  }

  if ((needsVerticalScrollbar) && (! mInDropDownMode)) {
    visibleWidth += scrollbarWidth;
  }

   // Do a second reflow with the adjusted width and height settings
   // This sets up all of the frames with the correct width and height.
  secondPassState.computedWidth = visibleWidth;
  secondPassState.computedHeight = visibleHeight;
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


void nsListControlFrame::ForceRedraw(nsIContent* aContent) 
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

void nsListControlFrame::DisplaySelected(nsIContent* aContent) 
{
   //XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
   // -moz-option-selected in the ua.css style sheet. This will not be needed when
   // The event state manager is functional. KMM
  
  nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)));
  aContent->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_TRUE);
  ForceRedraw(aContent);
 
}

void nsListControlFrame::DisplayDeselected(nsIContent* aContent) 
{
   //XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
   // -moz-option-selected in the ua.css style sheet. This will not be needed when
   // The event state manager is functional. KMM
 
  nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)));
  aContent->UnsetAttribute(kNameSpaceID_None, selectedAtom, PR_TRUE);
  ForceRedraw(aContent);

}

void nsListControlFrame::UpdateItem(nsIContent* aContent, PRBool aSelected)
{
  if (aSelected) {
    DisplaySelected(aContent);
  } else {
    DisplayDeselected(aContent);
  }
}

//----------------------------------------------------------------------
PRInt32 nsListControlFrame::SetContentSelected(nsIFrame *    aHitFrame, 
                                        nsIContent *& aHitContent, 
                                        PRBool        aDisplaySelected) 
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
      if (PR_TRUE == aDisplaySelected)
        DisplaySelected(aHitContent);
      return index;
    }
    kid->GetNextSibling(&kid);
    index++;
  }
  return kNothingSelected;
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
    if (IsFrameSelected(i)) {
      if (i != mSelectedIndex) {
        DisplayDeselected(content);
        SetFrameSelected(i, PR_FALSE);
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
        SetFrameSelected(i, !IsFrameSelected(i));
      } else {
        SetFrameSelected(i, aSetValue);
      }
      if (i != mSelectedIndex) {
        UpdateItem(content, IsFrameSelected(i));
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

void nsListControlFrame::SingleSelection()
{
   // Store previous selection
  PRInt32 oldSelectedIndex = mSelectedIndex;
   // Get Current selection
  mSelectedIndex = (PRInt32)SetContentSelected(mHitFrame, mHitContent, PR_FALSE);
  if (mSelectedIndex != kNothingSelected) {
    if (oldSelectedIndex != mSelectedIndex) {
        // Deselect the previous selection if there is one
      if (oldSelectedIndex != kNothingSelected) {
        SetFrameSelected(oldSelectedIndex, PR_FALSE);
      }
        // Dipslay the new selection
      SetFrameSelected(mSelectedIndex, PR_TRUE);
      mSelectedContent = mHitContent;
    } else {
      // Selecting the currently selected item so do nothing.
    }
  }
}

void nsListControlFrame::MultipleSelection(PRBool aIsShift, PRBool aIsControl)
{
  PRInt32 oldSelectedIndex = mSelectedIndex;

  mSelectedIndex = (PRInt32)SetContentSelected(mHitFrame, mHitContent, PR_FALSE);
  if (kNothingSelected != mSelectedIndex) {
    PRBool wasSelected = IsFrameSelected(mSelectedIndex);
    SetFrameSelected(mSelectedIndex, PR_TRUE);
    PRBool selected = wasSelected; 
      
    if (aIsShift) {
      selected = PR_TRUE;
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
      selected = !selected;
      if (nsnull != mSelectedContent) {
          if (mHitContent != mSelectedContent) {
             // Toggle the selection
            UpdateItem(mHitContent, !wasSelected);  
        } else {
//XXX: Fix this    mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, (selected?kSelectedFocus:kNormal), PR_TRUE);
        }
      }
    } else {
      mStartExtendedIndex = mSelectedIndex;
      mEndExtendedIndex   = kNothingSelected;
      ClearSelection();
      selected = PR_TRUE;
    }

    //XXX Fix this mHitContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, (selected?kSelectedFocus:kNormal), PR_TRUE);
    mSelectedContent = mHitContent;
 }

}

//----------------------------------------------------------------------
NS_IMETHODIMP nsListControlFrame::HandleLikeListEvent(nsIPresContext& aPresContext, 
                                               nsGUIEvent*     aEvent,
                                               nsEventStatus&  aEventStatus)
{
  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    if (mMultipleSelections) {
      MultipleSelection(((nsMouseEvent *)aEvent)->isShift, ((nsMouseEvent *)aEvent)->isControl);
    } else {
      SingleSelection();
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
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
      }
    }
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
  // (Selection doesn't actually occur until the mouse is released, or clicked and released)
  if (aEvent->message == NS_MOUSE_MOVE) {

    // If the DropDown is currently has a selected item 
    // then clear the selected item
    if (nsnull != mSelectedContent) {
      DisplayDeselected(mSelectedContent);
      NS_RELEASE(mSelectedContent);
    }

    // Now check to see if the newly hit frame is different
    // from the currently hit frame
    if (nsnull != mHitFrame && mHitFrame != mCurrentHitFrame) {

      // Since the new frame is different then clear the selection on the old frame (the current frame)
      if (nsnull != mCurrentHitContent) {
        DisplayDeselected(mCurrentHitContent);
        NS_RELEASE(mCurrentHitContent);
      }

      // Find the new hit "content" from the hit frame and select it
      SetContentSelected(mHitFrame, mHitContent, PR_TRUE);

      // Now cache the the newly hit frame and content as the "current" values
      mCurrentHitFrame   = mHitFrame;
      mCurrentHitContent = mHitContent;
    }

  } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
      // Initiate mouse capture
     CaptureMouseEvents(PR_TRUE);
      
  } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
     // Stop grabbing mouse events
     CaptureMouseEvents(PR_FALSE);
  
    // Start by finding the newly "hit" content from the hit frame
    PRInt32 index = SetContentSelected(mHitFrame, mHitContent, PR_TRUE);
    if (kNothingSelected != index) {

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
          DisplayDeselected(mSelectedContent);
          NS_RELEASE(mSelectedContent);
        }
        if (nsnull != mHitContent) {
          DisplayDeselected(mHitContent);
          NS_RELEASE(mHitContent);
        }
        if (nsnull != mCurrentHitContent) {
          DisplayDeselected(mCurrentHitContent);
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

  if (!mInDropDownMode) {
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }

  if (!mIsInitializedFromContent) {
    InitializeFromContent();
  }
 
  return nsScrollFrame::SetInitialChildList(aPresContext, aListName, aChildList);

}

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

// XXX: This following methods are not referenced. They should be removed if they
//   are not needed 
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

//XXX: End of the un-referenced methods.

//----------------------------------------------------------------------
nscoord 
nsListControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                      nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(0, aPixToTwip); 
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
      DisplaySelected(mSelectedContent);
      return NS_OK;
    }
    NS_RELEASE(content);
    kid->GetNextSibling(&kid);
    i++;
  }
  return NS_OK;
}


nsIContent* 
nsListControlFrame::GetOptionContent(PRUint32 aIndex) 
{
  nsresult result = NS_OK;
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  nsIDOMHTMLOptionElement* optionElement = GetOption(*options, aIndex);

  nsIContent *content = nsnull;
  result = optionElement->QueryInterface(kIContentIID, (void**) &content);

  NS_RELEASE(optionElement);
  NS_RELEASE(options);

  if (NS_SUCCEEDED(result)) {
    return(content);
  } else {
    return(nsnull);
  }
}

PRBool 
nsListControlFrame::IsFrameSelected(PRUint32 aIndex) 
{
  nsIContent* content = GetOptionContent(aIndex);
  NS_ASSERTION(nsnull != content, "Failed to retrieve option content");
  nsString value; 
  nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)));
  nsresult result = content->GetAttribute(kNameSpaceID_None, selectedAtom, value);
  NS_IF_RELEASE(content);
 
  if (NS_CONTENT_ATTR_NOT_THERE == result) {
    return(PR_FALSE);
  }
  else {
    return(PR_TRUE);
  }
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

//----------------------------------------------------------------------
void 
nsListControlFrame::InitializeFromContent(PRBool aDoDisplay)
{
  PRInt32 i;

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
      SetFrameSelected(i, selected);
        // Set the selected index for single selection list boxes.
      if (selected) {
       mSelectedIndex = i;
      }

      if (mInDropDownMode) {
        if (selected) {
          nsAutoString text;
          if (NS_CONTENT_ATTR_HAS_VALUE == option->GetText(text)) {
            mSelectionStr = text;
          }
          mSelectedIndex = i;
          NS_RELEASE(option);
          mSelectedContent = content;
          return;
        }
      } else {
        if (selected && aDoDisplay) {      
          DisplaySelected(content);
          mSelectedContent = content;
          NS_ADDREF(content);
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
    SetFrameSelected(i, PR_FALSE);
    // update visual here
  }
  mNumSelections = 0;
  return NS_OK;
}



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
        SetFrameSelected(i, PR_TRUE);
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
      if (IsFrameSelected(inx)) {
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
//XXX: Need correct attribute here   
//    mSelectedContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, (aOn?kSelectedFocus:kNormal), PR_TRUE);
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

nsresult nsListControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


PRInt32 nsListControlFrame::GetNumberOfOptions() 
{
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  if (!options) {
    return 0;
  }
  PRUint32 numOptions;
  options->GetLength(&numOptions);
  NS_RELEASE(options);
  return(numOptions);
}

// Set the selected attribute for an item in the listbox. If the listbox has
// been initialized the result of the setting the attribute is displayed otherwise
// it is not.

void 
nsListControlFrame::SetContentSelectedAttribute(PRUint32 aIndex, PRBool aSelected)
{
   // Only display the result of the attribute changed if the listbox has been
   // initialized.
  if (mIsInitializedFromContent) {
      // Update the attribute and display it
     SetFrameSelected(aIndex, aSelected);
  } else {
      // Update the attribute without displaying it
    nsIContent* content = GetOptionContent(aIndex);
    NS_ASSERTION(nsnull != content, "Failed to retrieve option content");
    nsCOMPtr<nsIAtom> selectedAtom ( dont_QueryInterface(NS_NewAtom(kMozSelected)));
    if (aSelected) {
      content->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_FALSE);
    } else {
      content->UnsetAttribute(kNameSpaceID_None, selectedAtom, PR_FALSE);
    }
    NS_IF_RELEASE(content);
  }
}

// Set a single item in the listbox to selected.

void 
nsListControlFrame::SetSelectedIndex(PRInt32 aIndex, nsIContent *aContent)
{
  SetContentSelectedAttribute(aIndex, PR_TRUE);
  mSelectedContent = aContent;
  mSelectedIndex = aIndex;

   // Get the selected string from the content
  nsIDOMHTMLOptionElement* option = nsnull;
  nsresult rv = aContent->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
  if (NS_SUCCEEDED(rv)) {
    nsAutoString text;
    if (NS_CONTENT_ATTR_HAS_VALUE == option->GetText(text)) {
       mSelectionStr = text;
    }
    NS_RELEASE(option);
  }
}


// Select the specified item in the listbox using control logic.
// If it a single selection listbox the previous selection will be
// de-selected. 

void 
nsListControlFrame::ToggleSelected(PRInt32 aIndex, nsIContent *aContent)
{
  PRBool multiple;
  GetMultiple(&multiple);

  if (PR_TRUE == multiple) {
    SetSelectedIndex(aIndex, aContent);
  } else {
    if (mSelectedIndex != kNothingSelected) {
      SetContentSelectedAttribute(aIndex, PR_FALSE);
    }
    SetSelectedIndex(aIndex, aContent);
  }
}

// Select the specified item in the listbox using 

void 
nsListControlFrame::SelectIndex(PRInt32 aIndex) 
{
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  nsIDOMHTMLOptionElement* optionElement = GetOption(*options, aIndex);
  nsIContent *content = nsnull;
  nsresult result = optionElement->QueryInterface(kIContentIID, (void**) &content);
  ToggleSelected(aIndex, content);
  NS_RELEASE(content);
  NS_RELEASE(optionElement);
  NS_RELEASE(options);
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
      SelectIndex(selectedIndex);
    }
  } else {
   //XXX: TODO What about all othe other properties that form elements can set here?
   // return nsFormControlFrame::SetProperty(aName, aValue);
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
     // Spin through loooking for the first selection in the list
    PRInt32 index = 0;
    PRInt32 maxOptions = GetNumberOfOptions();
    PRInt32 selectedIndex = kNothingSelected;
    for (index = 0; index < maxOptions; index++) {
      PRBool isSelected = PR_FALSE;
     
      if (PR_TRUE == IsFrameSelected(index)) {
        selectedIndex = isSelected;
        break;
      }
    }
    aValue.Append(selectedIndex, 10);
  } else {
    //XXX: TODO: What about all of the other form properties??? 
    //return nsFormControlFrame::GetProperty(aName, aValue);
  }

  return NS_OK;
}




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
#include "nsIPresShell.h"
#include "nsHTMLParts.h"

#include "nsIDOMEventReceiver.h"
#include "nsIEventStateManager.h"
#include "nsIDOMUIEvent.h"

static NS_DEFINE_IID(kIDOMMouseListenerIID,       NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseMotionListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);

// Constants
const nscoord kMaxDropDownRows          = 20; // This matches the setting for 4.x browsers
const PRInt32 kDefaultMultiselectHeight = 4; // This is compatible with 4.x browsers
const PRInt32 kNothingSelected          = -1;
const PRInt32 kMaxZ                     = 0x7fffffff; //XXX: Shouldn't there be a define somewhere for MaxInt for PRInt32
const PRInt32 kNoSizeSpecified          = -1;

//XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
// -moz-option-selected in the ua.css style sheet. This will not be needed when
//The event state manager is functional. KMM
const char * kMozSelected = "-moz-option-selected";

//---------------------------------------------------------
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


//---------------------------------------------------------
nsListControlFrame::nsListControlFrame()
{
  mHitFrame           = nsnull;
  mSelectedIndex      = kNothingSelected;
  mComboboxFrame      = nsnull;
  mFormFrame          = nsnull;
  mDisplayed          = PR_FALSE;
  mButtonDown         = PR_FALSE;
  mLastFrame          = nsnull;
  mMaxWidth           = 0;
  mMaxHeight          = 0;
  mPresContext        = nsnull;
  mEndExtendedIndex   = kNothingSelected;
  mStartExtendedIndex = kNothingSelected;
  mIgnoreMouseUp      = PR_FALSE;
  mIsCapturingMouseEvents = PR_FALSE;
}

//---------------------------------------------------------
nsListControlFrame::~nsListControlFrame()
{
  mComboboxFrame = nsnull;
  mFormFrame = nsnull;
  NS_IF_RELEASE(mPresContext);
}

//---------------------------------------------------------
NS_IMPL_ADDREF(nsListControlFrame)
NS_IMPL_RELEASE(nsListControlFrame)

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIFormControlFrame>::GetIID())) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIListControlFrame>::GetIID())) {
    *aInstancePtr = (void *)((nsIListControlFrame*)this);
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISelectControlFrame>::GetIID())) {
    *aInstancePtr = (void *)((nsISelectControlFrame*)this);
    return NS_OK;
  }
  if (aIID.Equals(kIDOMMouseListenerIID)) {                                         
    *aInstancePtr = (void*)(nsIDOMMouseListener*) this;                                        
    NS_ADDREF_THIS();
    return NS_OK;                                                        
  }
  if (aIID.Equals(kIDOMMouseMotionListenerIID)) {                                         
    *aInstancePtr = (void*)(nsIDOMMouseMotionListener*) this;                                        
    NS_ADDREF_THIS();
    return NS_OK;                                                        
  }
  return nsScrollFrame::QueryInterface(aIID, aInstancePtr);
}

//---------------------------------------------------------
// Reflow is overriden to constrain the listbox height to the number of rows and columns
// specified. 

NS_IMETHODIMP 
nsListControlFrame::Reflow(nsIPresContext&          aPresContext, 
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
  /*printf("nsListControlFrame::Reflow   Reason: ");
  switch (aReflowState.reason) {
    case eReflowReason_Initial:printf("eReflowReason_Initial\n");break;
    case eReflowReason_Incremental:printf("eReflowReason_Incremental\n");break;
    case eReflowReason_Resize:printf("eReflowReason_Resize\n");break;
    case eReflowReason_StyleChange:printf("eReflowReason_StyleChange\n");break;
  }*/
  aDesiredSize.width  = 0;
  aDesiredSize.height = 0;

  // Add the list frame as a child of the form
  if (IsInDropDownMode() == PR_FALSE && !mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }

  //--Calculate a width just big enough for the scrollframe to shrink around the
  //longest element in the list
  nsHTMLReflowState secondPassState(aReflowState);
  nsHTMLReflowState firstPassState(aReflowState);

   // Get the size of option elements inside the listbox
   // Compute the width based on the longest line in the listbox.
  
  firstPassState.mComputedWidth  = NS_UNCONSTRAINEDSIZE;
  firstPassState.mComputedHeight = NS_UNCONSTRAINEDSIZE;
  firstPassState.availableWidth  = NS_UNCONSTRAINEDSIZE;
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
  //nscoord scrollbarHeight = NSToCoordRound(sbHeight);

    // Subtract out the scrollbar width
  scrolledAreaWidth -= scrollbarWidth;

    // Subtract out the borders
  nsMargin border;
  if (!aReflowState.mStyleSpacing->GetBorder(border)) {
    NS_NOTYETIMPLEMENTED("percentage border");
    border.SizeTo(0, 0, 0, 0);
  }

  mBorderOffsetY = border.top;

  scrolledAreaWidth -= (border.left + border.right);
  scrolledAreaHeight -= (border.top + border.bottom);

  // Now the scrolledAreaWidth and scrolledAreaHeight are exactly 
  // wide and high enough to enclose their contents

  PRBool isInDropDownMode = IsInDropDownMode();

  nscoord visibleWidth = 0;
  if (isInDropDownMode) {
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

  // Check to see if we have zero item and
  // whether we have no width and height
  // The following code measures the width and height 
  // of a bogus string so the list actually displays
  PRInt32 length = 0;
  GetNumberOfOptions(&length);
  /*if (!isInDropDownMode && (0 == length || (0 == visibleWidth && 0 == heightOfARow))) {
    nsCOMPtr<nsIPresShell> presShell;
    nsresult rv = aPresContext.GetShell(getter_AddRefs(presShell));
    if (NS_SUCCEEDED(rv) && presShell) {
      nsCOMPtr<nsIRenderingContext> renderContext;
      rv = presShell->CreateRenderingContext(this, getter_AddRefs(renderContext));
      if (NS_SUCCEEDED(rv) && renderContext) {


        nsSize size;
        rv = presShell->CreateRenderingContext(this, getter_AddRefs(renderContext));
	      const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
        renderContext->SetFont(fontStyle->mFont);
        nsFormControlHelper::GetTextSize(aPresContext, this, nsAutoString("XX"), size, renderContext);
        visibleWidth  = size.width;
        heightOfARow = size.height;
      }
    }
  }*/

  nscoord visibleHeight = 0;
  if (isInDropDownMode) {
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


  // There are no items in the list
  // but we want to include space for the scrollbars
  // So fake like we will need scrollbars also
  if (!isInDropDownMode && 0 == length) {
    scrolledAreaHeight = visibleHeight+1;
  }

  PRBool needsVerticalScrollbar = PR_FALSE;
  if (visibleHeight < scrolledAreaHeight) {
    needsVerticalScrollbar = PR_TRUE; 
  }

  if (needsVerticalScrollbar && !isInDropDownMode) {
    visibleWidth += scrollbarWidth;
    mIsScrollbarVisible = PR_TRUE; // XXX temp code
  } else {
    mIsScrollbarVisible = PR_FALSE; // XXX temp code
  }

   // Do a second reflow with the adjusted width and height settings
   // This sets up all of the frames with the correct width and height.
  secondPassState.mComputedWidth = visibleWidth;
  secondPassState.mComputedHeight = visibleHeight;
  secondPassState.reason = eReflowReason_Resize;
  nsScrollFrame::Reflow(aPresContext, aDesiredSize, secondPassState, aStatus);

    // Set the max element size to be the same as the desired element size.
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  mDisplayed = PR_TRUE;
  
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}


//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}


//---------------------------------------------------------
PRBool 
nsListControlFrame::IsOptionElement(nsIContent* aContent)
{
  PRBool result = PR_FALSE;
 
  nsCOMPtr<nsIDOMHTMLOptionElement> optElem;
  if (NS_SUCCEEDED(aContent->QueryInterface(nsCOMTypeInfo<nsIDOMHTMLOptionElement>::GetIID(),(void**) getter_AddRefs(optElem)))) {      
    if (optElem != nsnull) {
      result = PR_TRUE;
    }
  }
 
  return result;
}


//---------------------------------------------------------
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


//---------------------------------------------------------
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

//---------------------------------------------------------
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


//---------------------------------------------------------
// XXX: Here we introduce a new -moz-option-selected attribute so a attribute
// selecitor n the ua.css can change the style when the option is selected.

void 
nsListControlFrame::DisplaySelected(nsIContent* aContent) 
{
   //XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
   // -moz-option-selected in the ua.css style sheet. This will not be needed when
   // The event state manager supports selected states. KMM
  
  nsIAtom * selectedAtom = NS_NewAtom(kMozSelected);
  if (PR_TRUE == mDisplayed) {
    aContent->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_TRUE);
    ForceRedraw();
  } else {
    aContent->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_FALSE);
  }
  NS_RELEASE(selectedAtom);
}

//---------------------------------------------------------
void 
nsListControlFrame::DisplayDeselected(nsIContent* aContent) 
{
   //XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
   // -moz-option-selected in the ua.css style sheet. This will not be needed when
   // The event state manager is functional. KMM

  nsIAtom * selectedAtom = NS_NewAtom(kMozSelected);
  if (PR_TRUE == mDisplayed) {
    aContent->UnsetAttribute(kNameSpaceID_None, selectedAtom, PR_TRUE);
    ForceRedraw();
  } else {
    aContent->UnsetAttribute(kNameSpaceID_None, selectedAtom, PR_FALSE);
  }
  NS_RELEASE(selectedAtom);

}


//---------------------------------------------------------
// Starts at the passed in content object and walks up the 
// parent heierarchy looking for the nsIDOMHTMLOptionElement
//---------------------------------------------------------
nsIContent *
nsListControlFrame::GetOptionFromContent(nsIContent *aContent) 
{
  nsIContent * content = aContent;
  NS_ADDREF(content);
  while (nsnull != content) {
    if (IsOptionElement(content)) {
      return content;
    }
    nsIContent * node = content;
    node->GetParent(content); // this add refs
    NS_RELEASE(node);
  }
  return nsnull;
}

//---------------------------------------------------------
// Finds the index of the hit frame's content in the list
// of option elements
//---------------------------------------------------------
PRInt32 
nsListControlFrame::GetSelectedIndexFromContent(nsIContent *aContent) 
{
  // Search the list of option elements looking for a match
  // between the hit frame's content and the content of an option element

  // get the collection of option items
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  if (nsnull != options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);

    PRUint32 inx;
    for (inx = 0; inx < numOptions; inx++) {
      nsIContent* option = nsnull;
      option = GetOptionAsContent(options, inx);
      if (nsnull != option) {
        if (option == aContent) {
          NS_RELEASE(option);
          NS_RELEASE(options);
          return inx;
        }
       NS_RELEASE(option);
      }
    }
    NS_RELEASE(options);
  }
  return kNothingSelected;
}

//---------------------------------------------------------
// Finds the index of the hit frame's content in the list
// of option elements
//---------------------------------------------------------
PRInt32 
nsListControlFrame::GetSelectedIndexFromFrame(nsIFrame *aHitFrame) 
{
  PRInt32 index = kNothingSelected;
  // Get the content of the frame that was selected
  nsIContent* selectedContent = nsnull;
  NS_ASSERTION(aHitFrame, "No frame for html <select> element\n");  
  if (aHitFrame) {
    aHitFrame->GetContent(&selectedContent);
    index = GetSelectedIndexFromContent(selectedContent);
    NS_RELEASE(selectedContent);
  }
  return index;
}


//---------------------------------------------------------
void 
nsListControlFrame::ClearSelection()
{
  PRInt32 length = 0;
  GetNumberOfOptions(&length);
  for (PRInt32 i = 0; i < length; i++) {
    if (i != mSelectedIndex) {
      SetContentSelected(i, PR_FALSE);
    } 
  }
}

//---------------------------------------------------------
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
        SetContentSelected(i, !IsContentSelectedByIndex(i));
      } else {
        SetContentSelected(i, aSetValue);
      }
    }
    if (i == endInx) {
      startInverting = PR_FALSE;
    }
  }
}

//---------------------------------------------------------
void 
nsListControlFrame::SingleSelection()
{
   // Get Current selection
  if (mSelectedIndex != kNothingSelected) {
    if (mOldSelectedIndex != mSelectedIndex) {
        // Deselect the previous selection if there is one
      if (mOldSelectedIndex != kNothingSelected) {
        SetContentSelected(mOldSelectedIndex, PR_FALSE);
      }
        // Display the new selection
      SetContentSelected(mSelectedIndex, PR_TRUE);
    } else {
      // Selecting the currently selected item so do nothing.
    }
  }
}

//---------------------------------------------------------
void 
nsListControlFrame::MultipleSelection(PRBool aIsShift, PRBool aIsControl)
{
  if (kNothingSelected != mSelectedIndex) {
    if ((aIsShift) || (mButtonDown && (!aIsControl))) {
        // Shift is held down
      SetContentSelected(mSelectedIndex, PR_TRUE);
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
      if (IsContentSelectedByIndex(mSelectedIndex)) {
        SetContentSelected(mSelectedIndex, PR_FALSE);
      } else {
        SetContentSelected(mSelectedIndex, PR_TRUE);
      }

    } else {
       // Neither control nor shift is held down
      SetContentSelected(mSelectedIndex, PR_TRUE);
      mStartExtendedIndex = mSelectedIndex;
      mEndExtendedIndex   = kNothingSelected;
      ClearSelection();
    }
  }

}

//---------------------------------------------------------
void 
nsListControlFrame::HandleListSelection(nsIDOMEvent* aEvent)
{

  PRBool multipleSelections = PR_FALSE;
  GetMultiple(&multipleSelections);
  if (multipleSelections) {
    nsCOMPtr<nsIDOMUIEvent> uiEvent = do_QueryInterface(aEvent);
    PRBool isShift;
    PRBool isControl;
    uiEvent->GetCtrlKey(&isControl);
    uiEvent->GetShiftKey(&isShift);
    MultipleSelection(isShift, isControl);
  } else {
    SingleSelection();
  }
}

//---------------------------------------------------------
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

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::CaptureMouseEvents(PRBool aGrabMouseEvents)
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
      // XXX this is temp code
      if (!mIsScrollbarVisible) {
        nsIWidget * widget;
        view->GetWidget(widget);
        if (nsnull != widget) {
          widget->CaptureMouse(aGrabMouseEvents);
          NS_RELEASE(widget);
        }
      }
    }
  }

  return NS_OK;
}

//---------------------------------------------------------
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

//---------------------------------------------------------
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


//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus&  aEventStatus)
{

  /*const char * desc[] = {"NS_MOUSE_MOVE", 
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

  /*if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  if (nsFormFrame::GetDisabled(this)) { 
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
  }*/

  return NS_OK;
}


//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  mContentFrame = aChildList;
  return nsScrollFrame::SetInitialChildList(aPresContext, aListName, aChildList);
}

//---------------------------------------------------------
nsresult
nsListControlFrame::GetSizeAttribute(PRInt32 *aSize) {
  nsresult rv = NS_OK;
  nsIDOMHTMLSelectElement* selectElement;
  rv = mContent->QueryInterface(nsCOMTypeInfo<nsIDOMHTMLSelectElement>::GetIID(),(void**) &selectElement);
  if (mContent && NS_SUCCEEDED(rv)) {
    rv = selectElement->GetSize(aSize);
    NS_RELEASE(selectElement);
  }
  return rv;
}


//---------------------------------------------------------
NS_IMETHODIMP  
nsListControlFrame::Init(nsIPresContext&  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  mPresContext = &aPresContext;
  NS_ADDREF(mPresContext);
  nsresult result = nsScrollFrame::Init(aPresContext, aContent, aParent, aContext,
                                        aPrevInFlow);
   // Initialize the current selected and not selected state's for
   // the listbox items from the content. This is done here because
   // The selected content sets an attribute that must be on the content
   // before the option element's frames are constructed so the frames will
   // get the proper style based on attribute selectors which refer to the
   // selected attribute.
  if (!mIsInitializedFromContent) {
    Reset();
  }

  // get the reciever interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mContent));

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  reciever->AddEventListenerByIID((nsIDOMMouseListener *)this, kIDOMMouseListenerIID);
  reciever->AddEventListenerByIID((nsIDOMMouseMotionListener *)this, kIDOMMouseMotionListenerIID);

  return result;
}


//---------------------------------------------------------
nscoord 
nsListControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                      nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(0, aPixToTwip); 
}


//---------------------------------------------------------
nscoord 
nsListControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                        float aPixToTwip, 
                                        nscoord aInnerWidth,
                                        nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPixToTwip, aInnerWidth);
}


//---------------------------------------------------------
// Returns whether the nsIDOMHTMLSelectElement supports 
// mulitple selection
//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetMultiple(PRBool* aMultiple, nsIDOMHTMLSelectElement* aSelect)
{
  if (!aSelect) {
    nsIDOMHTMLSelectElement* selectElement = nsnull;
    nsresult result = mContent->QueryInterface(nsCOMTypeInfo<nsIDOMHTMLSelectElement>::GetIID(),
                                               (void**)&selectElement);
    if (NS_SUCCEEDED(result) && selectElement) {
      result = selectElement->GetMultiple(aMultiple);
      NS_RELEASE(selectElement);
    } 
    return result;
  } else {
    return aSelect->GetMultiple(aMultiple);
  }
}


//---------------------------------------------------------
// for a given piece of content it returns nsIDOMHTMLSelectElement object
// or null 
//---------------------------------------------------------
nsIDOMHTMLSelectElement* 
nsListControlFrame::GetSelect(nsIContent * aContent)
{
  nsIDOMHTMLSelectElement* selectElement = nsnull;
  nsresult result = aContent->QueryInterface(nsCOMTypeInfo<nsIDOMHTMLSelectElement>::GetIID(),
                                             (void**)&selectElement);
  if (NS_SUCCEEDED(result) && selectElement) {
    return selectElement;
  } else {
    return nsnull;
  }
}


//---------------------------------------------------------
// Returns the nsIContent object in the collection 
// for a given index
//---------------------------------------------------------
nsIContent* 
nsListControlFrame::GetOptionAsContent(nsIDOMHTMLCollection* aCollection, PRUint32 aIndex) 
{
  nsIContent *             content       = nsnull;
  nsIDOMHTMLOptionElement* optionElement = GetOption(*aCollection, aIndex);
  if (nsnull != optionElement) {
    optionElement->QueryInterface(nsCOMTypeInfo<nsIContent>::GetIID(),(void**) &content);
    NS_RELEASE(optionElement);
  }
 
  return content;
}

//---------------------------------------------------------
// for a given index it returns the nsIContent object 
// from the select
//---------------------------------------------------------
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

//---------------------------------------------------------
// This returns the collection for nsIDOMHTMLSelectElement or
// the nsIContent object is the select is null 
//---------------------------------------------------------
nsIDOMHTMLCollection* 
nsListControlFrame::GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect)
{
  nsIDOMHTMLCollection* options = nsnull;
  if (!aSelect) {
    nsIDOMHTMLSelectElement* selectElement = GetSelect(aContent);
    if (selectElement) {
      selectElement->GetOptions(&options);
      NS_RELEASE(selectElement);
      return options;
    } else {
      return nsnull;
    }
  } else {
    aSelect->GetOptions(&options);
    return options;
  }
}

//---------------------------------------------------------
// Returns the nsIDOMHTMLOptionElement for a given index 
// in the select's collection
//---------------------------------------------------------
nsIDOMHTMLOptionElement* 
nsListControlFrame::GetOption(nsIDOMHTMLCollection& aCollection, PRUint32 aIndex)
{
  nsIDOMNode* node = nsnull;
  if (NS_SUCCEEDED(aCollection.Item(aIndex, &node))) {
    if (nsnull != node) {
      nsIDOMHTMLOptionElement* option = nsnull;
      node->QueryInterface(nsCOMTypeInfo<nsIDOMHTMLOptionElement>::GetIID(), (void**)&option);
      NS_RELEASE(node);
      return option;
    }
  }
  return nsnull;
}


//---------------------------------------------------------
// For a given index in the collection it sets the aValue 
// parameter with the "value" ATTRIBUTE from the DOM element
// This method return PR_TRUE if successful
//---------------------------------------------------------
PRBool
nsListControlFrame::GetOptionValue(nsIDOMHTMLCollection& aCollection, 
                                   PRUint32              aIndex, 
                                   nsString&             aValue)
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

//---------------------------------------------------------
// For a given piece of content, it determines whether the 
// content (an option) is selected or not
// return PR_TRUE if it is, PR_FALSE if it is NOT
//---------------------------------------------------------
PRBool 
nsListControlFrame::IsContentSelected(nsIContent* aContent)
{
  nsString value; 
  nsIAtom * selectedAtom = NS_NewAtom(kMozSelected);
  nsresult result = aContent->GetAttribute(kNameSpaceID_None, selectedAtom, value);
  NS_RELEASE(selectedAtom);

  return (NS_CONTENT_ATTR_NOT_THERE == result ? PR_FALSE : PR_TRUE);
}


//---------------------------------------------------------
// For a given index is return whether the content is selected
//---------------------------------------------------------
PRBool 
nsListControlFrame::IsContentSelectedByIndex(PRUint32 aIndex) 
{
  nsIContent* content = GetOptionContent(aIndex);
  NS_ASSERTION(nsnull != content, "Failed to retrieve option content");

  PRBool result = IsContentSelected(content);
  NS_RELEASE(content);
  return result;
}

//---------------------------------------------------------
// gets the content (an option) by index and then set it as
// being selected or not selected
//---------------------------------------------------------
void 
nsListControlFrame::SetContentSelected(PRUint32 aIndex, PRBool aSelected)
{
  if (aIndex == kNothingSelected) {
    return;
  }
  nsIContent* content = GetOptionContent(aIndex);
  NS_ASSERTION(nsnull != content, "Failed to retrieve option content");
  if (aSelected) {
    DisplaySelected(content);
  } else {
    DisplayDeselected(content);
  }
  NS_IF_RELEASE(content);
}

//---------------------------------------------------------
// Deselects all the content items in the select
//---------------------------------------------------------
nsresult 
nsListControlFrame::Deselect() 
{
  PRInt32 i;
  PRInt32 max = 0;
  GetNumberOfOptions(&max);
  for (i=0;i<max;i++) {
    SetContentSelected(i, PR_FALSE);
  }
  mSelectedIndex = kNothingSelected;
  
  return NS_OK;
}

//---------------------------------------------------------
PRIntn
nsListControlFrame::GetSkipSides() const
{    
    // Don't skip any sides during border rendering
  return 0;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetType(PRInt32* aType) const
{
  *aType = NS_FORM_SELECT;
  return NS_OK;
}

//---------------------------------------------------------
void 
nsListControlFrame::SetFormFrame(nsFormFrame* aFormFrame) 
{ 
  mFormFrame = aFormFrame; 
}


//---------------------------------------------------------
PRBool
nsListControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}


//---------------------------------------------------------
void 
nsListControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
}


//---------------------------------------------------------
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


//---------------------------------------------------------
// Resets the select back to it's original default values;
// those values as determined by the original HTML
//---------------------------------------------------------
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
  PRUint32 i;
  for (i = 0; i < numOptions; i++) {
    nsIDOMHTMLOptionElement* option = GetOption(*options, i);
    if (option) {
      PRBool selected = PR_FALSE;
      option->GetDefaultSelected(&selected);
      if (selected) {
        mSelectedIndex = i;
        SetContentSelected(i, PR_TRUE);
      }
      NS_RELEASE(option);
    }
  }
  NS_RELEASE(options);
} 

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetName(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(nsCOMTypeInfo<nsIHTMLContent>::GetIID(),(void**)&formControl);
    if (NS_SUCCEEDED(result) && formControl) {
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
 

//---------------------------------------------------------
PRInt32 
nsListControlFrame::GetNumberOfSelections()
{
  PRInt32 count = 0;
  PRInt32 length = 0;
  GetNumberOfOptions(&length);
  PRInt32 i = 0;
  for (i = 0; i < length; i++) {
    if (IsContentSelectedByIndex(i)) {
      count++;
    }
  }
  return(count);
}


//---------------------------------------------------------
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
      if (PR_TRUE == IsContentSelectedByIndex(i)) {
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


//---------------------------------------------------------
void 
nsListControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  // XXX:TODO Make set focus work
}

//---------------------------------------------------------
void 
nsListControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_ANYWHERE,NS_PRESSHELL_SCROLL_ANYWHERE);

  }
}


//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::SetComboboxFrame(nsIFrame* aComboboxFrame)
{
  nsresult rv = NS_OK;
  if (nsnull != aComboboxFrame) {
    rv = aComboboxFrame->QueryInterface(nsCOMTypeInfo<nsIComboboxControlFrame>::GetIID(),(void**) &mComboboxFrame); 
  }
  return rv;
}


//---------------------------------------------------------
// Gets the text of the currently selected item
// if the there are zero items then an empty string is returned
// if there is nothing selected, then the 0th item's text is returned
//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetSelectedItem(nsString & aStr)
{
  aStr = "";
  nsresult rv = NS_ERROR_FAILURE; 
  nsIDOMHTMLCollection* options = GetOptions(mContent);

  if (nsnull != options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);

    if (numOptions == 0) {
      rv = NS_OK;
    } else {
      PRInt32 selectedIndex = (mSelectedIndex == kNothingSelected ? 0 : mSelectedIndex);

      nsIDOMHTMLOptionElement* optionElement = GetOption(*options, selectedIndex);
      if (nsnull != optionElement) {
        nsAutoString text;
        if (NS_CONTENT_ATTR_HAS_VALUE == optionElement->GetText(text)) {
          aStr = text;
          rv = NS_OK;
        }
        NS_RELEASE(optionElement);
      }
    }
    NS_RELEASE(options);
  }

  return rv;
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetSelectedIndex(PRInt32 * aIndex)
{
  *aIndex = mSelectedIndex;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetSelectedIndexFromDOM(PRInt32 * aIndex)
{
  // figure out which item is selected by looking at which
  // option are selected
  *aIndex = kNothingSelected;
  nsIDOMHTMLCollection* options = GetOptions(mContent);
  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);

    PRUint32 inx;
    for (inx = 0; inx < numOptions && (*aIndex == kNothingSelected); inx++) {
      nsIContent* content = GetOptionAsContent(options, inx);
      if (nsnull != content) {
        if (IsContentSelected(content)) {
          *aIndex = (PRInt32)inx;
        }
        NS_RELEASE(content);
      }
    }
    NS_RELEASE(options);
  }
  return NS_ERROR_FAILURE;
}

//---------------------------------------------------------
PRBool 
nsListControlFrame::IsInDropDownMode()
{
  return((nsnull == mComboboxFrame) ? PR_FALSE : PR_TRUE);
}

//---------------------------------------------------------
nsresult 
nsListControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

//---------------------------------------------------------
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

//---------------------------------------------------------
// Select the specified item in the listbox using control logic.
// If it a single selection listbox the previous selection will be
// de-selected. 
void 
nsListControlFrame::ToggleSelected(PRInt32 aIndex)
{
  PRBool multiple;
  GetMultiple(&multiple);

  if (PR_TRUE == multiple) {
    SetContentSelected(aIndex, PR_TRUE);
  } else {
    SetContentSelected(mSelectedIndex, PR_FALSE);
    SetContentSelected(aIndex, PR_TRUE);
    mSelectedIndex = aIndex;
  }
}

//----------------------------------------------------------------------
// nsISelectControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::AddOption(PRInt32 aIndex)
{
  PRInt32 numOptions;
  GetNumberOfOptions(&numOptions);

  PRInt32 selectedIndex;
  GetSelectedIndexFromDOM(&selectedIndex); // comes from the DOM

  if (1 == numOptions || mSelectedIndex == kNothingSelected || selectedIndex != mSelectedIndex) {
    mSelectedIndex = selectedIndex;
    if (nsnull != mComboboxFrame) {
      mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, selectedIndex); // don't dispatch event
    }
  } else {
    mSelectedIndex = selectedIndex;
  }
  return NS_OK;
}
  

NS_IMETHODIMP
nsListControlFrame::RemoveOption(PRInt32 aIndex)
{
  PRInt32 numOptions;
  GetNumberOfOptions(&numOptions);

  PRInt32 selectedIndex;
  GetSelectedIndexFromDOM(&selectedIndex); // comes from the DOM

  if (aIndex == mSelectedIndex) {
    ToggleSelected(selectedIndex); // sets mSelectedIndex
  } else {
    mSelectedIndex = selectedIndex;
  }
  if (nsnull != mComboboxFrame) {
    mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, selectedIndex); // don't dispatch event
  }

  return NS_OK;
}
//----------------------------------------------------------------------
// End nsISelectControlFrame
//----------------------------------------------------------------------

//---------------------------------------------------------
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
      if (mSelectedIndex != selectedIndex) {
        ToggleSelected(selectedIndex);// sets mSelectedIndex
        if (nsnull != mComboboxFrame) {
          mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, selectedIndex); // don't dispatch event
        }
      }
    }
  }
 
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Get the selected value of option from local cache (optimization vs. widget)
  if (nsHTMLAtoms::selected == aName) {
    PRInt32 error = 0;
    PRBool selected = PR_FALSE;
    PRInt32 index = aValue.ToInteger(&error, 10); // Get index from aValue
    if (error == 0)
       selected = IsContentSelectedByIndex(index); 
  
    nsFormControlHelper::GetBoolString(selected, aValue);
    
  // For selectedIndex, get the value from the widget
  } else  if (nsHTMLAtoms::selectedindex == aName) {
    // figures out the first selected item from the content
    PRInt32 selectedIndex;
    GetSelectedIndexFromDOM(&selectedIndex);
    aValue.Append(selectedIndex, 10);
    //aValue.Append(mSelectedIndex, 10);

  }

  return NS_OK;
}


//---------------------------------------------------------
// Create a Borderless top level widget for drop-down lists.
nsresult 
nsListControlFrame::CreateScrollingViewWidget(nsIView* aView, const nsStylePosition* aPosition)
{
  if (IsInDropDownMode() == PR_TRUE) {
    nsWidgetInitData widgetData;
    aView->SetZIndex(kMaxZ);
    widgetData.mWindowType  = eWindowType_popup;
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

//---------------------------------------------------------
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
 

//---------------------------------------------------------
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

////////////////////////////////////
    const nsStyleColor* color;
    const nsStyleDisplay* disp; 
   GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&) color);
   GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) disp);

    view->SetVisibility(NS_STYLE_VISIBILITY_HIDDEN == disp->mVisible ?nsViewVisibility_kHide:nsViewVisibility_kShow); 

    /*viewManager->SetViewOpacity(view, color->mOpacity);
    PRInt32 num;
    view->GetChildCount(num);
    for (PRInt32 cnt = 0; cnt < num; cnt++){
      nsIView *kid;
      view->GetChild(cnt, kid);
      if (nsnull != kid) {
        kid->SetVisibility(NS_STYLE_VISIBILITY_HIDDEN == disp->mVisible ?nsViewVisibility_kHide:nsViewVisibility_kShow); 
      }
    }*/

/////////////////////////////

  return NS_OK;
}

//---------------------------------------------------------
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

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::DidReflow(nsIPresContext& aPresContext,
                              nsDidReflowStatus aStatus)
{
  if (PR_TRUE == IsInDropDownMode()) 
  {
    SyncViewWithFrame();
    //mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
    nsresult rv = nsScrollFrame::DidReflow(aPresContext, aStatus);
    //mState |= NS_FRAME_SYNC_FRAME_AND_VIEW;
    SyncViewWithFrame();
    return rv;
  } else {
    return nsScrollFrame::DidReflow(aPresContext, aStatus);
  }
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetMaximumSize(nsSize &aSize)
{
  aSize.width  = mMaxWidth;
  aSize.height = mMaxHeight;
  return NS_OK;
}


//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMMouseListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseUp(nsIDOMEvent* aMouseEvent)
{
  //if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
  //  return NS_OK;
  //}

  if (nsFormFrame::GetDisabled(this)) { 
    return NS_OK;
  }

  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->mVisible) {
    return NS_OK;
  }

  if (IsInDropDownMode() == PR_TRUE) {
    if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent))) {
      if (kNothingSelected != mSelectedIndex) {
        SetContentSelected(mSelectedIndex, PR_TRUE);  
      }
      if (mComboboxFrame) {
        mComboboxFrame->ListWasSelected(mPresContext); 
      } 
    }
  } else {
    mButtonDown = PR_FALSE;
    CaptureMouseEvents(PR_FALSE);
  }

  return NS_OK;
}


//----------------------------------------------------------------------
// Sets the mSelectedIndex and mOldSelectedIndex from figuring out what 
// item was selected using content
// Returns NS_OK if it successfully found the selection
//----------------------------------------------------------------------
nsresult
nsListControlFrame::GetIndexFromDOMEvent(nsIDOMEvent* aMouseEvent)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIEventStateManager *stateManager;
  if (NS_OK == mPresContext->GetEventStateManager(&stateManager)) {
    nsIContent * content;
    stateManager->GetEventTargetContent(&content);

    nsIContent * optionContent = GetOptionFromContent(content);
    NS_RELEASE(content);
    if (nsnull != optionContent) {
      mOldSelectedIndex = mSelectedIndex;
      mSelectedIndex    = GetSelectedIndexFromContent(optionContent);
      NS_RELEASE(optionContent);
      rv = NS_OK;
    }
    NS_RELEASE(stateManager);
  }

  return rv;
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if (nsFormFrame::GetDisabled(this)) { 
    return NS_OK;
  }

  if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent))) {
    if (IsInDropDownMode() == PR_TRUE) {
      // Do nothing
    } else {
      // Handle Like List
      mButtonDown = PR_TRUE;
      CaptureMouseEvents(PR_TRUE);
      HandleListSelection(aMouseEvent);
    }
  } else {
    if (mComboboxFrame) {
      PRBool isDroppedDown;
      mComboboxFrame->IsDroppedDown(&isDroppedDown);
      mComboboxFrame->ShowDropDown(!isDroppedDown);
      if (isDroppedDown) {
        CaptureMouseEvents(PR_FALSE);
      }
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMMouseMotionListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseMove(nsIDOMEvent* aMouseEvent)
{
  if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent))) {
    if (IsInDropDownMode() == PR_TRUE) {
      if (kNothingSelected != mSelectedIndex) {
        if (mOldSelectedIndex != mSelectedIndex) {
          if (mOldSelectedIndex != kNothingSelected) {
            SetContentSelected(mOldSelectedIndex, PR_FALSE);
          }
          SetContentSelected(mSelectedIndex, PR_TRUE);
        }
      }
    }
  }
  return NS_OK;
}



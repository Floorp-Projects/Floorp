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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsListControlFrame.h"
#include "nsFormControlFrame.h" // for COMPARE macro
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
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIStatefulFrame.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsILookAndFeel.h"

#include "nsISelectElement.h"

static NS_DEFINE_IID(kIDOMMouseListenerIID,       NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseMotionListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,         NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMNodeIID,                NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIFrameIID,                  NS_IFRAME_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID,        NS_IPRIVATEDOMEVENT_IID);
//static NS_DEFINE_IID(kBlockFrameCID,              NS_BLOCK_FRAME_CID);

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
  mButtonDown         = PR_FALSE;
  mLastFrame          = nsnull;
  mMaxWidth           = 0;
  mMaxHeight          = 0;
  mPresContext        = nsnull;
  mEndExtendedIndex   = kNothingSelected;
  mStartExtendedIndex = kNothingSelected;
  mIgnoreMouseUp      = PR_FALSE;
  mIsCapturingMouseEvents = PR_FALSE;
  mSelectionCache     = nsnull;
  mSelectionCacheLength = -1;

  mIsAllContentHere   = PR_FALSE;
  mIsAllFramesHere    = PR_FALSE;
  mHasBeenInitialized = PR_FALSE;
}

//---------------------------------------------------------
nsListControlFrame::~nsListControlFrame()
{
  // if list is dropped down 
  // make sure it gets rolled up
  if (IsInDropDownMode()) {
    PRBool isDown;
    mComboboxFrame->IsDroppedDown(&isDown);
    if (isDown) {
      mComboboxFrame->ShowDropDown(PR_FALSE);
    }
  }

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mContent));

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  reciever->RemoveEventListenerByIID((nsIDOMMouseListener *)this, kIDOMMouseListenerIID);
  reciever->RemoveEventListenerByIID((nsIDOMMouseMotionListener *)this, kIDOMMouseMotionListenerIID);
  reciever->RemoveEventListenerByIID((nsIDOMKeyListener *)this, kIDOMKeyListenerIID);

  mComboboxFrame = nsnull;
  if (mFormFrame) {
    mFormFrame->RemoveFormControlFrame(*this);
    mFormFrame = nsnull;
  }
  NS_IF_RELEASE(mPresContext);
  if (mSelectionCache) {
    delete[] mSelectionCache;
  }
}

//---------------------------------------------------------
//NS_IMPL_ADDREF(nsListControlFrame)
//NS_IMPL_RELEASE(nsListControlFrame)

//---------------------------------------------------------
// Frames are not refcounted, no need to AddRef
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
    return NS_OK;                                                        
  }
  if (aIID.Equals(kIDOMMouseMotionListenerIID)) {                                         
    *aInstancePtr = (void*)(nsIDOMMouseMotionListener*) this;                                        
    return NS_OK;                                                        
  }
  if (aIID.Equals(kIDOMKeyListenerIID)) {                                         
    *aInstancePtr = (void*)(nsIDOMKeyListener*) this;                                        
    return NS_OK;                                                        
  }
  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
    return NS_OK;
  }
  return nsScrollFrame::QueryInterface(aIID, aInstancePtr);
}

//---------------------------------------------------------
nsresult nsListControlFrame::CountAllChild(nsIDOMNode * aNode, PRInt32& aCount) 
{
  nsresult status = NS_OK;
  nsIDOMNode * child;
  aNode->GetFirstChild(&child);
  while (child) {

    // note: both optgroup and option elements can have DOM child
    // option elements have text nodes as COM child, but they don't have too
    nsCOMPtr<nsIDOMHTMLOptGroupElement> optGroup;
    child->QueryInterface(nsCOMTypeInfo<nsIDOMHTMLOptGroupElement>::GetIID(),(void**) &optGroup);
    if (optGroup) {
      aCount++;

      // check for children
      PRBool hasChildren;
      status = child->HasChildNodes(&hasChildren);
      if (NS_FAILED(status)) {
        NS_RELEASE(child);
        return status;
      }
      if (hasChildren) {
        status = CountAllChild(child, aCount);
        if (NS_FAILED(status)) {
          NS_RELEASE(child);
          return status;
        }
      }
    } else {
      // don't query interface againa if it was an optgroup
      nsCOMPtr<nsIDOMHTMLOptionElement> option;
      child->QueryInterface(nsCOMTypeInfo<nsIDOMHTMLOptionElement>::GetIID(),(void**) &option);
      if (option) {
        aCount++;
      }
    }

    nsIDOMNode * tmp = child;
    status = child->GetNextSibling(&child);
    if (NS_FAILED(status)) {
      NS_RELEASE(tmp);
      return status;
    }
    NS_RELEASE(tmp);
  }
  return status;
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
#ifdef DEBUG_rodsXXX
  printf("nsListControlFrame::Reflow    Reason: ");
  switch (aReflowState.reason) {
    case eReflowReason_Initial:printf("eReflowReason_Initial\n");break;
    case eReflowReason_Incremental:printf("eReflowReason_Incremental\n");break;
    case eReflowReason_Resize:printf("eReflowReason_Resize\n");break;
    case eReflowReason_StyleChange:printf("eReflowReason_StyleChange\n");break;
  }
#endif // DEBUG_rodsXXX

#if 0
    // reflow optimization - why reflow if all the contents 
    // and frames aren't there yet
    if (!mIsAllContentHere || !mIsAllFramesHere) {
      aDesiredSize.width  = 30*15;
      aDesiredSize.height = 30*15;
      if (nsnull != aDesiredSize.maxElementSize) {
        aDesiredSize.maxElementSize->width  = aDesiredSize.width;
	      aDesiredSize.maxElementSize->height = aDesiredSize.height;
      }
      aStatus = NS_FRAME_COMPLETE;
      printf("--------------------------> Skipping reflow\n");
      return NS_OK;
    }
#endif

  // If all the content and frames are here 
  // then initialize it before reflow
    if (mIsAllContentHere && !mHasBeenInitialized) {
      if (PR_FALSE == mIsAllFramesHere) {
        printf("**********************************************************\n");
        printf("**********************************************************\n");
        printf("**********************************************************\n");
        CheckIfAllFramesHere();
      }
      if (mIsAllFramesHere && !mHasBeenInitialized) {
        mHasBeenInitialized = PR_TRUE;
        InitSelectionCache(-1); // Reset sel cache so as not to send event
        Reset(mPresContext);
      }
    }
  
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

  // Add the list frame as a child of the form
  if (IsInDropDownMode() == PR_FALSE && !mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }

  //--Calculate a width just big enough for the scrollframe to shrink around the
  //longest element in the list
  nsHTMLReflowState secondPassState(aReflowState);
  nsHTMLReflowState firstPassState(aReflowState);
  //nsHTMLReflowState   firstPassState(aPresContext, nsnull,
  //                                   this, aDesiredSize);

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
  //
   // The nsScrollFrame::REflow adds in the scrollbar width and border dimensions
  // to the maxElementSize, so these need to be subtracted
  nscoord scrolledAreaWidth  = scrolledAreaDesiredSize.maxElementSize->width;
  nscoord scrolledAreaHeight = scrolledAreaDesiredSize.height;

  // Keep the oringal values
  mMaxWidth  = scrolledAreaWidth;
  mMaxHeight = scrolledAreaDesiredSize.maxElementSize->height;

  // The first reflow produces a box with the scrollbar width and borders
  // added in so we need to subtract them out.

  // Retrieve the scrollbar's width and height
  float sbWidth  = 0.0;
  float sbHeight = 0.0;;
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext.GetDeviceContext(getter_AddRefs(dc));
  dc->GetScrollBarDimensions(sbWidth, sbHeight);
  // Convert to nscoord's by rounding
  nscoord scrollbarWidth  = NSToCoordRound(sbWidth);
  nscoord scrollbarHeight = NSToCoordRound(sbHeight);

    // Subtract out the scrollbar width
  scrolledAreaWidth -= scrollbarWidth;

    // Subtract out the borders
  nsMargin border;
  if (!aReflowState.mStyleSpacing->GetBorder(border)) {
    NS_NOTYETIMPLEMENTED("percentage border");
    border.SizeTo(0, 0, 0, 0);
  }

  nsMargin padding;
  if (!aReflowState.mStyleSpacing->GetPadding(padding)) {
    NS_NOTYETIMPLEMENTED("percentage padding");
    padding.SizeTo(0, 0, 0, 0);
  }

  mBorderOffsetY = border.top;

  scrolledAreaWidth  -= (border.left + border.right + padding.left + padding.right);
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
      if (visibleWidth < scrolledAreaWidth) {
        visibleWidth = scrolledAreaWidth;
      }
    }
  } else {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
      visibleWidth = scrolledAreaWidth;
    } else {
      visibleWidth = aReflowState.mComputedWidth - scrollbarWidth;
      visibleWidth  -= (border.left + border.right + padding.left + padding.right);
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
  heightOfARow -= (border.top + border.bottom + padding.top + padding.bottom);

  // Check to see if we have zero item and
  // whether we have no width and height
  // The following code measures the width and height 
  // of a bogus string so the list actually displays
  PRInt32 length = 0;
  GetNumberOfOptions(&length);

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
      visibleHeight -= (border.top + border.bottom + padding.top + padding.bottom);
    } else {
      PRInt32 numRows = 1;
      GetSizeAttribute(&numRows);
      if (numRows == kNoSizeSpecified) {
        nsIDOMNode* node;
        nsresult rv = mContent->QueryInterface(nsCOMTypeInfo<nsIDOMNode>::GetIID(),(void**) &node);
        if (node && NS_SUCCEEDED(rv)) {
          numRows = 0;
          CountAllChild(node, numRows);
          NS_RELEASE(node);
        }
      }
      visibleHeight = numRows * heightOfARow;
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
    if (!isInDropDownMode) {
      visibleWidth += scrollbarWidth;
    }
    mIsScrollbarVisible = PR_FALSE; // XXX temp code
  }

  if (visibleHeight < scrollbarHeight) {
    visibleHeight  = scrollbarHeight;
    mMaxHeight     = scrollbarHeight;
  }

   // Do a second reflow with the adjusted width and height settings
   // This sets up all of the frames with the correct width and height.
  secondPassState.mComputedWidth  = visibleWidth;
  secondPassState.mComputedHeight = visibleHeight;
  secondPassState.reason = eReflowReason_Resize;
  //printf("List: visible      %d %d\n", visibleWidth, visibleHeight);
  nsScrollFrame::Reflow(aPresContext, aDesiredSize, secondPassState, aStatus);

    // Set the max element size to be the same as the desired element size.
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  //printf("List: aDesiredSize %d %d\n", aDesiredSize.width, aDesiredSize.height);

  aStatus = NS_FRAME_COMPLETE;

#ifdef DEBUG_rodsXXX
  if (!isInDropDownMode) {
    PRInt32 numRows = 1;
    GetSizeAttribute(&numRows);
    printf("%d ", numRows);
    if (numRows == 2) {
      COMPARE_QUIRK_SIZE("nsListControlFrame", 56, 38) 
    } if (numRows == 3) {
      COMPARE_QUIRK_SIZE("nsListControlFrame", 56, 54) 
    } if (numRows == 4) {
      COMPARE_QUIRK_SIZE("nsListControlFrame", 56, 70) 
    }
  }
#endif

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
nsListControlFrame::ForceRedraw(nsIPresContext* aPresContext) 
{
  //XXX: Hack. This should not be needed. The problem is DisplaySelected
  //and DisplayDeselected set and unset an attribute on generic HTML content
  //which does not know that it should repaint as the result of the attribute
  //being set. This should not be needed once the event state manager handles
  //selection.
  nsFormControlHelper::ForceDrawFrame(aPresContext, this);
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
  if (PR_TRUE == mIsAllFramesHere) {
    aContent->SetAttribute(kNameSpaceID_None, selectedAtom, "", PR_TRUE);
    //ForceRedraw();
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
  if (PR_TRUE == mIsAllFramesHere) {
    aContent->UnsetAttribute(kNameSpaceID_None, selectedAtom, PR_TRUE);
    //ForceRedraw();
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
  PRInt32 indx = kNothingSelected;
  // Get the content of the frame that was selected
  nsIContent* selectedContent = nsnull;
  NS_ASSERTION(aHitFrame, "No frame for html <select> element\n");  
  if (aHitFrame) {
    aHitFrame->GetContent(&selectedContent);
    indx = GetSelectedIndexFromContent(selectedContent);
    NS_RELEASE(selectedContent);
  }
  return indx;
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
      nsCOMPtr<nsIContent> content = getter_AddRefs(GetOptionContent(mSelectedIndex));
      if (content) {
        ScrollToFrame(content);
      }
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
    //if ((aIsShift) || (mButtonDown && (!aIsControl))) {
    if (aIsShift) {
        // Shift is held down
      SetContentSelected(mSelectedIndex, PR_TRUE);
      if (mEndExtendedIndex == kNothingSelected) {
        mEndExtendedIndex = mSelectedIndex;
        ExtendedSelection(mStartExtendedIndex, mEndExtendedIndex, PR_FALSE, PR_TRUE);
      } else {
        if (mStartExtendedIndex < mEndExtendedIndex) {
          if (mSelectedIndex < mStartExtendedIndex) {
            ExtendedSelection(mSelectedIndex+1, mEndExtendedIndex, PR_TRUE, PR_TRUE);
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
            ExtendedSelection(mEndExtendedIndex, mSelectedIndex-1, PR_TRUE, PR_TRUE);
            mEndExtendedIndex   = mSelectedIndex;
          } else if (mSelectedIndex < mEndExtendedIndex) {
            ExtendedSelection(mStartExtendedIndex, mEndExtendedIndex-1, PR_FALSE, PR_TRUE);
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
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aEvent);
    PRBool isShift;
    PRBool isControl;
    mouseEvent->GetCtrlKey(&isControl);
    mouseEvent->GetShiftKey(&isShift);
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
nsListControlFrame::CaptureMouseEvents(nsIPresContext* aPresContext, PRBool aGrabMouseEvents)
{

    // get its view
  nsIView* view = nsnull;
  GetView(aPresContext, &view);
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
#if 0
      if (!mIsScrollbarVisible) {
        nsIWidget * widget;
        view->GetWidget(widget);
        if (nsnull != widget) {
          widget->CaptureMouse(aGrabMouseEvents);
          NS_RELEASE(widget);
        }
      }
#endif
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

  if (nsEventStatus_eConsumeNoDefault == aEventStatus)
    return NS_OK;

  if (nsFormFrame::GetDisabled(this))
    return NS_OK;

  switch(aEvent->message) {
    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        printf("---> %d %c\n", keyEvent->keyCode, keyEvent->keyCode);
        //if (NS_VK_SPACE == keyEvent->keyCode || NS_VK_RETURN == keyEvent->keyCode) {
        //  MouseClicked(&aPresContext);
        //}
      }
      break;
    default:
      break;
  }

  return(nsScrollFrame::HandleEvent(aPresContext, aEvent, aEventStatus));

}


//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  // First check to see if all the content has been added
  nsCOMPtr<nsISelectElement> element(do_QueryInterface(mContent));
  if (element) {
    element->IsDoneAddingContent(&mIsAllContentHere);
    if (!mIsAllContentHere) {
      mIsAllFramesHere    = PR_FALSE;
      mHasBeenInitialized = PR_FALSE;
    }
  }
  mContentFrame = aChildList;
  nsresult rv = nsScrollFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // If all the content is here now check
  // to see if all the frames have been created
  if (mIsAllContentHere) {
    // If all content and frames are here
    // the reset/initialize
    if (CheckIfAllFramesHere()) {
      InitSelectionCache(-1);
      Reset(&aPresContext);
      mHasBeenInitialized = PR_TRUE;
    }
  }

  return rv;
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
    Reset(&aPresContext);
  } else {
    InitSelectionCache(-1);
  }

  // get the reciever interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mContent));

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  reciever->AddEventListenerByIID((nsIDOMMouseListener *)this, kIDOMMouseListenerIID);
  reciever->AddEventListenerByIID((nsIDOMMouseMotionListener *)this, kIDOMMouseMotionListenerIID);
  reciever->AddEventListenerByIID((nsIDOMKeyListener *)this, kIDOMKeyListenerIID);

#if 0
  nsIFrame* parent;
  GetParentWithView(&parent);
  NS_ASSERTION(parent, "GetParentWithView failed");

  // Get parent view
  nsIView* parentView = nsnull;
  while (1) {
    parent->GetView(&parentView);
  }
#endif
  return result;
}


//---------------------------------------------------------
nscoord 
nsListControlFrame::GetVerticalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
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
  return GetVerticalInsidePadding(aPresContext, aPixToTwip, aInnerWidth);
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
nsListControlFrame::GetOptionAsContent(nsIDOMHTMLCollection* aCollection, PRInt32 aIndex) 
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
nsListControlFrame::GetOptionContent(PRInt32 aIndex)
  
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
nsListControlFrame::GetOption(nsIDOMHTMLCollection& aCollection, PRInt32 aIndex)
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
                                   PRInt32               aIndex, 
                                   nsString&             aValue)
{
  PRBool status = PR_FALSE;
  nsresult result = NS_OK;
  nsIDOMHTMLOptionElement* option = GetOption(aCollection, aIndex);
  if (option) {
     // Check for value attribute on the option element.
     // Can not use the return value from option->GetValue() because it
     // always returns NS_OK even when the value attribute does not
     // exist.
   nsCOMPtr<nsIHTMLContent> optionContent;
   result = option->QueryInterface(kIHTMLContentIID, getter_AddRefs(optionContent));
   if (NS_SUCCEEDED(result)) {
     nsHTMLValue value;
      result = optionContent->GetHTMLAttribute(nsHTMLAtoms::value, value);

      if (NS_CONTENT_ATTR_NOT_THERE == result) {
        result = option->GetText(aValue);
        if (aValue.Length() > 0) {
          status = PR_TRUE;
        }
      } else {
          // Need to use the options GetValue to get the real value because
          // it does extra processing on the return value. (i.e it trims it.)
         result = option->GetValue(aValue);
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
  //nsIAtom * selectedAtom = NS_NewAtom("selected");
  nsresult result = aContent->GetAttribute(kNameSpaceID_None, selectedAtom, value);
  NS_RELEASE(selectedAtom);

  return (NS_CONTENT_ATTR_NOT_THERE == result ? PR_FALSE : PR_TRUE);
}


//---------------------------------------------------------
// For a given index is return whether the content is selected
//---------------------------------------------------------
PRBool 
nsListControlFrame::IsContentSelectedByIndex(PRInt32 aIndex) 
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
nsListControlFrame::SetContentSelected(PRInt32 aIndex, PRBool aSelected)
{
  if (aIndex == kNothingSelected) {
    return;
  }
  nsIContent* content = GetOptionContent(aIndex);
  //NS_ASSERTION(nsnull != content && aIndex == 0, "Failed to retrieve option content");
  if (nsnull != content) {
    if (aSelected) {
      DisplaySelected(content);
    } else {
      DisplayDeselected(content);
    }
    NS_RELEASE(content);
  }
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
      NS_RELEASE(options);
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
nsListControlFrame::Reset(nsIPresContext* aPresContext)
{
  // if all the frames aren't here 
  // don't bother reseting
  if (!mIsAllFramesHere) {
    return;
  }

  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (!options) {
    return;
  }

  PRUint32 numOptions;
  options->GetLength(&numOptions);

  mSelectedIndex = kNothingSelected;

  Deselect();
  PRUint32 i;
  for (i = 0; i < numOptions; i++) {
    nsCOMPtr<nsIDOMHTMLOptionElement> option = getter_AddRefs(GetOption(*options, i));
    if (option) {
      PRBool selected = PR_FALSE;
      option->GetDefaultSelected(&selected);
      if (selected) {
        mSelectedIndex = i;
        SetContentSelected(i, PR_TRUE);

        // Now that it is selected scroll to it
        nsCOMPtr<nsIContent> content(do_QueryInterface(option));
        if (content) {
          ScrollToFrame(content);
        }
        if (mComboboxFrame) {
          mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, mSelectedIndex); // don't dispatch event
        }
      }
    }
  }

  InitSelectionCache(numOptions);

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
      nsIDOMHTMLOptionElement* optionElement = GetOption(*options, mSelectedIndex);
      if (nsnull != optionElement) {
        nsAutoString text;
        rv = optionElement->GetLabel(text);
        if (NS_CONTENT_ATTR_HAS_VALUE != rv || 0 == text.Length()) {
          if (NS_CONTENT_ATTR_HAS_VALUE == optionElement->GetText(text)) {
            aStr = text;
            rv = NS_OK;
          }
        } else {
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
PRBool nsListControlFrame::CheckIfAllFramesHere()
{
  // Get the number of optgroups and options
  PRInt32 numContentItems = 0;
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  if (node) {
    CountAllChild(node, numContentItems);
  }
  //printf("number of items %d ", numContentItems);

  // now count the number of block frames
  // each option is in a block frame
  PRInt32 numFrames = 0;
  nsIFrame * areaFrame = nsnull;
  FirstChild(nsnull, &areaFrame);
  if (areaFrame) {
    nsIFrame * child = nsnull;
    areaFrame->FirstChild(nsnull, &child);
    while (child) {
      nsIFrame * blkFrame;
      if (NS_SUCCEEDED(child->QueryInterface(kBlockFrameCID,(void**)&blkFrame))) {
        numFrames++;
      }
      child->GetNextSibling(&child);
    }
  }
  // now make sure we have a frame each piece of content
  mIsAllFramesHere = numFrames == numContentItems;

  return mIsAllFramesHere;
}

NS_IMETHODIMP
nsListControlFrame::DoneAddingContent()
{
  mIsAllContentHere = PR_TRUE;

  // Here we check to see if all the frames have been created 
  // for all the content.
  // If so, then we can initialize;
  if (mIsAllFramesHere == PR_FALSE) {
    // if all the frames are now present we can initalize
    if (CheckIfAllFramesHere() && mPresContext) {
      mHasBeenInitialized = PR_TRUE;
      InitSelectionCache(-1); // Reset select cache so as not to send event
      Reset(mPresContext);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::AddOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
  
  if (!mIsAllContentHere) {
    nsCOMPtr<nsISelectElement> element(do_QueryInterface(mContent));
    if (element) {
      element->IsDoneAddingContent(&mIsAllContentHere);
      if (!mIsAllContentHere) {
        mIsAllFramesHere    = PR_FALSE;
        mHasBeenInitialized = PR_FALSE;
      } else {
        PRInt32 numOptions;
        GetNumberOfOptions(&numOptions);
        mIsAllFramesHere = aIndex == numOptions-1;
      }
    }
  }
  
  if (!mHasBeenInitialized) {
    return NS_OK;
  }

  PRInt32 oldSelection = mSelectedIndex;

  // Adding an option to the select can cause a change in selection
  // if the new option has it's selected attribute set.
  // this code checks to see if it does
  // if so then it resets the entire selection of listbox
  PRBool wasReset = PR_FALSE;
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (options) {
    nsCOMPtr<nsIDOMHTMLOptionElement> option = getter_AddRefs(GetOption(*options, aIndex));
    if (option) {
      PRBool selected = PR_FALSE;
      option->GetDefaultSelected(&selected);
      if (selected) {
        Reset(aPresContext);             // this sets mSelectedIndex to the defaulted selection
        wasReset = PR_TRUE;
      }
    }
  }

  if (!wasReset) {
    GetSelectedIndexFromDOM(&mSelectedIndex); // comes from the DOM
  }

  // if selection changed because of the new option being added 
  // notify the combox if necessary
  if (nsnull != mComboboxFrame && oldSelection != mSelectedIndex) {
    mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, mSelectedIndex); // don't dispatch event
  }

  // selectionChanged = (1 == numOptions ||
  // kNothingSelected == oldSelectedIndex || mSelectedIndex != oldSelectedIndex)

  // For now, we don't care because we're not dispatching an event:
  InitSelectionCache(-1); // Reset sel cache so as not to send event
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::RemoveOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
  PRInt32 numOptions;
  GetNumberOfOptions(&numOptions);

//  PRInt32 oldSelectedIndex = mSelectedIndex;
  GetSelectedIndexFromDOM(&mSelectedIndex); // comes from the DOM

  // Select the new selectedIndex
  // Don't need to deselect option as it is being removed anyway.
  if (mSelectedIndex >= 0) {
    SetContentSelected(mSelectedIndex, PR_TRUE);
  }

  // Only if aIndex != -1 can we determine if there was a change in selection
  // selectionChanged = (aIndex == mSelectedIndex) || (mSelectedIndex ==
  // oldSelectedIndex - (oldSelectedIndex > aIndex)?1:0);
  // Call SelectionChanged to dispatch an event if so!

  // For now, we don't care because we're not dispatching an event:
  InitSelectionCache(-1); // Reset cache to not send event
  return NS_OK;
}

//---------------------------------------------------------
// Select the specified item in the listbox using control logic.
// If it a single selection listbox the previous selection will be
// de-selected. 
NS_IMETHODIMP
nsListControlFrame::SetOptionSelected(PRInt32 aIndex, PRBool aValue)
{
  PRBool multiple;
  nsresult rv = GetMultiple(&multiple);
  if (NS_SUCCEEDED(rv)) {
    if (aValue) {
      ToggleSelected(aIndex); // sets mSelectedIndex
    } else {
      SetContentSelected(aIndex, aValue);
      if (!multiple) {
        // Get the new selIndex from the DOM (may have changed)
        PRInt32 selectedIndex;
        GetSelectedIndexFromDOM(&selectedIndex);
        if (mSelectedIndex != selectedIndex) {
          ToggleSelected(selectedIndex); // sets mSelectedIndex
        }
      }
    }
    // Should we send an event here or not?
    if (nsnull != mComboboxFrame && mIsAllFramesHere) {
      rv = mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, aIndex); // don't dispatch event
    } else {
      InitSelectionCache(-1);
    }
  }
  return rv;
}

// Initialize selection cache from content state;
nsresult
nsListControlFrame::InitSelectionCache(PRInt32 aLength)
{
  nsresult rv = NS_OK;
  
  // Get number of options from content if needed
  if (0 > aLength) {
    rv = GetNumberOfOptions(&aLength);
  }

  // Allocate a new array or realloc if the length has changed
  if (NS_SUCCEEDED(rv) && (mSelectionCacheLength != aLength)) {
    if (mSelectionCache) {
      delete[] mSelectionCache;
      mSelectionCacheLength = 0;
    }
    mSelectionCache = new PRBool[aLength];
    if (!mSelectionCache) return NS_ERROR_OUT_OF_MEMORY;
    mSelectionCacheLength = aLength;
  }

  // Sync the cache to the content
  if (NS_SUCCEEDED(rv)) {
    for (PRInt32 i = 0; i < mSelectionCacheLength; i++) {
      mSelectionCache[i] = IsContentSelectedByIndex(i);
    }
  }
  return rv;
}

// Compare content state with local cache of last known state
// If there was a change, call SelectionChanged()
NS_IMETHODIMP
nsListControlFrame::UpdateSelection(PRBool aDoDispatchEvent, PRBool aForceUpdate, nsIContent* aContent)
{
  if (!mIsAllFramesHere || !mIsAllContentHere) {
    return NS_OK;
  }
  nsresult rv = NS_OK;
  PRBool changed = PR_FALSE;

  // Paranoia: check if cache is up to date with content
  PRInt32 length = 0;
  GetNumberOfOptions(&length);
  if (mSelectionCacheLength != length) {
    NS_ASSERTION(0,"nsListControlFrame: Cache sync'd with content!\n");
    rv = InitSelectionCache(length);
    changed = PR_TRUE; // Assume the worst, there was a change.
  }

  // Step through content looking for change in selection
  if (NS_SUCCEEDED(rv)) {
    if (!changed) {
      PRBool selected;
      for (PRInt32 i = 0; i < length; i++) {
        selected = IsContentSelectedByIndex(i);
        if (selected != mSelectionCache[i]) {
          mSelectionCache[i] = selected;
          changed = PR_TRUE;
        }
      }
    }

    if (changed && aDoDispatchEvent) {
      rv = SelectionChanged(aContent); // Dispatch event
    }
    if ((changed || aForceUpdate) && mComboboxFrame) {
      rv = mComboboxFrame->SelectionChanged(); // Update view
    }
  }
  return rv;
}

// Send out an onchange notification.
nsresult
nsListControlFrame::SelectionChanged(nsIContent* aContent)
{
  // Dispatch the NS_FORM_CHANGE event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  event.widget = nsnull;
  event.message = NS_FORM_CHANGE;
  event.flags = NS_EVENT_FLAG_NONE;

  // Here we create our own DOM event and set the target to the Select
  // We'll pass this DOM event in, in hopes that the target is used.
  nsIDOMEvent* DOMEvent = nsnull;
  nsresult res = NS_NewDOMUIEvent(&DOMEvent, *mPresContext, &event);
  if (NS_SUCCEEDED(res) && DOMEvent && mContent) {
    nsIDOMNode* node = nsnull;
    res = mContent->QueryInterface(kIDOMNodeIID, (void**)&node);
    if (NS_SUCCEEDED(res) && node) {
      nsIPrivateDOMEvent* pDOMEvent = nsnull;
      res = DOMEvent->QueryInterface(kIPrivateDOMEventIID, (void**)&pDOMEvent);
      if (NS_SUCCEEDED(res) && pDOMEvent) {
        res = pDOMEvent->SetTarget(node);
	if (NS_SUCCEEDED(res)) {
          // Have the content handle the event.
          res = mContent->HandleDOMEvent(*mPresContext, &event, &DOMEvent, NS_EVENT_FLAG_BUBBLE, status);
        }
        NS_RELEASE(pDOMEvent);
      }
      NS_RELEASE(node);
    }
    NS_RELEASE(DOMEvent);
  }

  // Now have the frame handle the event
  if (NS_SUCCEEDED(res)) {
    if (this) {
      nsIFrame* frame = nsnull;
      res = this->QueryInterface(kIFrameIID, (void**)&frame);
      if ((NS_SUCCEEDED(res)) && (nsnull != frame)) {
        res = frame->HandleEvent(*mPresContext, &event, status);
        // NS_RELEASE(frame);
      }
    }
  }
  return res;
}

//---------------------------------------------------------
// Determine if the specified item in the listbox is selected.
NS_IMETHODIMP
nsListControlFrame::GetOptionSelected(PRInt32 aIndex, PRBool* aValue)
{
  *aValue = IsContentSelectedByIndex(aIndex);
  return NS_OK;
}

//----------------------------------------------------------------------
// End nsISelectControlFrame
//----------------------------------------------------------------------

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue)
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
	// Should we send an event here?
        if (nsnull != mComboboxFrame && mIsAllFramesHere) {
          mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, selectedIndex); // don't dispatch event
        } else {
	        InitSelectionCache(-1);
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
    PRInt32 indx = aValue.ToInteger(&error, 10); // Get index from aValue
    if (error == 0)
       selected = IsContentSelectedByIndex(indx); 
  
    nsFormControlHelper::GetBoolString(selected, aValue);
    
  // For selectedIndex, get the value from the widget
  } else  if (nsHTMLAtoms::selectedindex == aName) {
    // figures out the first selected item from the content
    PRInt32 selectedIndex;
    GetSelectedIndexFromDOM(&selectedIndex);
    if ((kNothingSelected == selectedIndex) && (mComboboxFrame)) {
      selectedIndex = 0;
    }
    aValue.Append(selectedIndex, 10);
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
    
    
#ifdef XP_MAC
    static NS_DEFINE_IID(kCPopUpCID,  NS_POPUP_CID);
    aView->CreateWidget(kCPopUpCID, &widgetData, nsnull);
#else
   static NS_DEFINE_IID(kCChildCID,  NS_CHILD_CID);
   aView->CreateWidget(kCChildCID, &widgetData, nsnull);
#endif   
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
NS_IMETHODIMP 
nsListControlFrame::SyncViewWithFrame(nsIPresContext* aPresContext)
{
    // Resync the view's position with the frame.
    // The problem is the dropdown's view is attached directly under
    // the root view. This means it's view needs to have it's coordinates calculated
    // as if it were in it's normal position in the view hierarchy.
  mComboboxFrame->AbsolutelyPositionDropDown();

  nsPoint parentPos;
  nsCOMPtr<nsIViewManager> viewManager;

     //Get parent frame
  nsIFrame* parent;
  GetParentWithView(aPresContext, &parent);
  NS_ASSERTION(parent, "GetParentWithView failed");

  // Get parent view
  nsIView* parentView = nsnull;
  parent->GetView(aPresContext, &parentView);

  parentView->GetViewManager(*getter_AddRefs(viewManager));
  GetViewOffset(viewManager, parentView, parentPos);
  nsIView* view = nsnull;
  GetView(aPresContext, &view);

  nsIView* containingView = nsnull;
  nsPoint offset;
  GetOffsetFromView(aPresContext, offset, &containingView);
  //nsSize size;
  //GetSize(size);

  nscoord width;
  nscoord height;
  view->GetDimensions(&width, &height);

  if (width != mRect.width || height != mRect.height) {
    viewManager->ResizeView(view, mRect.width, mRect.height);
  }
  nscoord x;
  nscoord y;
  view->GetPosition(&x, &y);

  nscoord newX = parentPos.x + offset.x;
  nscoord newY = parentPos.y + offset.y;

  //if (newX != x || newY != y) {
    viewManager->MoveViewTo(view, newX, newY);
  //}

  nsViewVisibility visibility;

  view->GetVisibility(visibility);
  const nsStyleDisplay* disp; 
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) disp);

  if (visibility != disp->mVisible) {
    view->SetVisibility(NS_STYLE_VISIBILITY_VISIBLE == disp->mVisible ?nsViewVisibility_kShow:nsViewVisibility_kHide); 
  }

  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::AboutToDropDown()
{
  mSelectedIndexWhenPoppedDown = mSelectedIndex;
  if (mIsAllContentHere && mIsAllFramesHere && mHasBeenInitialized) {
    if (mSelectedIndex != kNothingSelected) {
      // make sure we scroll to the correct item before it drops down
      nsCOMPtr<nsIContent> content = getter_AddRefs(GetOptionContent(mSelectedIndex));
      if (content) {
        ScrollToFrame(content);
      }
    } else {
      ScrollToFrame(nsnull); // this means it scrolls to 0,0
    }
  }

  return NS_OK;
}

//---------------------------------------------------------
// We are about to be rolledup from the outside (ComboboxFrame)
// this is a "cancelled" action not a selected action
NS_IMETHODIMP 
nsListControlFrame::AboutToRollup()
{
  ResetSelectedItem();
  return NS_OK;
}

//---------------------------------------------------------
nsresult
nsListControlFrame::GetScrollingParentView(nsIPresContext* aPresContext,
                                           nsIFrame* aParent,
                                           nsIView** aParentView)
{
  if (IsInDropDownMode() == PR_TRUE) {
     // Use the parent frame to get the view manager
    nsIView* parentView = nsnull;
    nsresult rv = aParent->GetView(aPresContext, &parentView);
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
     return nsScrollFrame::GetScrollingParentView(aPresContext, aParent, aParentView);
   }
}

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::DidReflow(nsIPresContext& aPresContext,
                              nsDidReflowStatus aStatus)
{
  if (PR_TRUE == IsInDropDownMode()) 
  {
    //SyncViewWithFrame();
    mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
    nsresult rv = nsScrollFrame::DidReflow(aPresContext, aStatus);
    mState |= NS_FRAME_SYNC_FRAME_AND_VIEW;
    SyncViewWithFrame(&aPresContext);
    return rv;
  } else {
    return nsScrollFrame::DidReflow(aPresContext, aStatus);
  }
}

NS_IMETHODIMP nsListControlFrame::MoveTo(nsIPresContext* aPresContext, nscoord aX, nscoord aY)
{
  if (PR_TRUE == IsInDropDownMode()) 
  {
    //SyncViewWithFrame();
    mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
    nsresult rv = nsScrollFrame::MoveTo(aPresContext, aX, aY);
    mState |= NS_FRAME_SYNC_FRAME_AND_VIEW;
    //SyncViewWithFrame();
    return rv;
  } else {
    return nsScrollFrame::MoveTo(aPresContext, aX, aY);
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

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::IsTargetOptionDisabled(PRBool &aIsDisabled)
{
  nsresult rv = NS_ERROR_FAILURE;
  aIsDisabled = PR_FALSE;

  nsIEventStateManager *stateManager;
  rv = mPresContext->GetEventStateManager(&stateManager);
  if (NS_OK == rv) {
    nsIContent * content;
    rv = stateManager->GetEventTargetContent(&content);
    if (NS_OK == rv && nsnull != content) {
      if (IsOptionElement(content)) {
        aIsDisabled = nsFormFrame::GetDisabled(this, content);
      } else {
        rv = NS_ERROR_FAILURE; // return error when it is not an option
      }
      NS_RELEASE(content);
    }
    NS_RELEASE(stateManager);
  }
  return rv;
}

//----------------------------------------------------------------------
// This is used to reset the the list and it's selection because the 
// selection was cancelled and the list rolled up.
void nsListControlFrame::ResetSelectedItem()
{  
  if (mIsAllFramesHere) {
    ToggleSelected(mSelectedIndexWhenPoppedDown);  
  }
}

//----------------------------------------------------------------------
// nsIDOMMouseListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseUp(nsIDOMEvent* aMouseEvent)
{

  if (nsFormFrame::GetDisabled(this)) { 
    return NS_OK;
  }

  // Check to see if the disabled option was clicked on
  // NS_ERROR_FAILURE is returned is it isn't over an option
  PRBool optionIsDisabled;
  if (NS_OK == IsTargetOptionDisabled(optionIsDisabled)) {
    if (optionIsDisabled) {
      if (IsInDropDownMode() == PR_TRUE && mComboboxFrame) {
        ResetSelectedItem();
        mComboboxFrame->ListWasSelected(mPresContext); 
      } 
      return NS_OK;
    }
  }

  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->mVisible) {
    return NS_OK;
  }

  if (IsInDropDownMode() == PR_TRUE) {
    if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, mOldSelectedIndex, mSelectedIndex))) {
      if (kNothingSelected != mSelectedIndex) {
        SetContentSelected(mSelectedIndex, PR_TRUE);  
      }
      if (mComboboxFrame) {
        mComboboxFrame->ListWasSelected(mPresContext); 
      } 
    }
  } else {
    mButtonDown = PR_FALSE;
    CaptureMouseEvents(mPresContext, PR_FALSE);
    UpdateSelection(PR_TRUE, PR_FALSE, mContent);
  }

  return NS_OK;
}


//----------------------------------------------------------------------
// Sets the mSelectedIndex and mOldSelectedIndex from figuring out what 
// item was selected using content
// Returns NS_OK if it successfully found the selection
//----------------------------------------------------------------------
nsresult
nsListControlFrame::GetIndexFromDOMEvent(nsIDOMEvent* aMouseEvent, 
                                         PRInt32&     aOldIndex, 
                                         PRInt32&     aCurIndex)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIEventStateManager *stateManager;
  if (NS_OK == mPresContext->GetEventStateManager(&stateManager)) {
    nsIContent * content;
    stateManager->GetEventTargetContent(&content);

    nsIContent * optionContent = GetOptionFromContent(content);
    NS_RELEASE(content);
    if (nsnull != optionContent) {
      aOldIndex = aCurIndex;
      aCurIndex = GetSelectedIndexFromContent(optionContent);
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

  // Check to see if the disabled option was clicked on
  // NS_ERROR_FAILURE is returned is it isn't over an option
  PRBool optionIsDisabled;
  if (NS_OK == IsTargetOptionDisabled(optionIsDisabled)) {
    if (optionIsDisabled) {
      if (IsInDropDownMode() == PR_TRUE) {
        ResetSelectedItem();
      }
      return NS_OK;
    }
  }

  PRInt32 oldIndex;
  PRInt32 curIndex = mSelectedIndex;

  if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, oldIndex, curIndex))) {
    if (IsInDropDownMode() == PR_TRUE) {
      // the pop up stole focus away from the webshell
      // now I am giving it back
      nsIFrame * parentFrame;
      GetParentWithView(mPresContext, &parentFrame);
      if (nsnull != parentFrame) {
        nsIView * pView;
        parentFrame->GetView(mPresContext, &pView);
        if (nsnull != pView) {
          nsIWidget *window = nsnull;

          nsIView *ancestor = pView;
          while (nsnull != ancestor) {
            ancestor->GetWidget(window); // addrefs
            if (nsnull != window) {
              window->SetFocus();
              NS_IF_RELEASE(window);
	            break;
	          }
	          ancestor->GetParent(ancestor);
          }
        }
      }
      // turn back on focus events
      nsIEventStateManager *stateManager;
      if (NS_OK == mPresContext->GetEventStateManager(&stateManager)) {
        stateManager->ConsumeFocusEvents(PR_TRUE);
        NS_RELEASE(stateManager);
      }
    } else {
      mSelectedIndex    = curIndex;
      mOldSelectedIndex = oldIndex;
      // Handle Like List
      mButtonDown = PR_TRUE;
      CaptureMouseEvents(mPresContext, PR_TRUE);
      HandleListSelection(aMouseEvent);
    }
  } else {
    // NOTE: the combo box is responsible for dropping it down
    if (mComboboxFrame) {
      nsIEventStateManager *stateManager;
      if (NS_OK == mPresContext->GetEventStateManager(&stateManager)) {
        nsIFrame * frame;
        stateManager->GetEventTarget(&frame);
        nsCOMPtr<nsIListControlFrame> listFrame(do_QueryInterface(frame));
        if (listFrame) {
          nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));
          PRInt32 scrX;
          PRInt32 scrY;
          mouseEvent->GetScreenX(&scrX);
          mouseEvent->GetScreenY(&scrY);
          nsRect rect;
          mComboboxFrame->GetAbsoluteRect(&rect);
          if (!rect.Contains(scrX, scrY)) {
            return NS_OK;
          }
        }
        // This will consume the focus event we get from the clicking on the dropdown
        //stateManager->ConsumeFocusEvents(PR_TRUE);


        PRBool isDroppedDown;
        mComboboxFrame->IsDroppedDown(&isDroppedDown);
        mComboboxFrame->ShowDropDown(!isDroppedDown);
        // Reset focus on main webshell here
        //stateManager->SetContentState(mContent, NS_EVENT_STATE_FOCUS);

        if (isDroppedDown) {
          CaptureMouseEvents(mPresContext, PR_FALSE);
        }

        NS_RELEASE(stateManager);
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
  if (mComboboxFrame) { // Synonym for IsInDropDownMode()
    PRBool isDroppedDown = PR_FALSE;
    mComboboxFrame->IsDroppedDown(&isDroppedDown);
    if (isDroppedDown) {
      PRInt32 oldIndex;
      PRInt32 curIndex = mSelectedIndex;
      if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, oldIndex, curIndex))) {
        mSelectedIndex    = curIndex;
        mOldSelectedIndex = oldIndex;
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
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMKeyListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}


nsresult
nsListControlFrame::ScrollToFrame(nsIContent* aOptElement)
{
  nsIView * scrollView;
  GetView(mPresContext, &scrollView);
  nsIScrollableView * scrollableView = nsnull;
  nsresult rv = scrollView->QueryInterface(nsIScrollableView::GetIID(), (void**)&scrollableView);

  // if null is passed in we scroll to 0,0
  if (nsnull == aOptElement) {
    if (NS_SUCCEEDED(rv) && scrollableView) {
      scrollableView->ScrollTo(0, 0, PR_TRUE);
    }
    return NS_OK;
  }
  
  // otherwise we find the content's frame and scroll to it
  nsIFrame * childframe;
  nsresult result;
  if (aOptElement) {
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    result = presShell->GetPrimaryFrameFor(aOptElement, &childframe);
  } else {
    return NS_ERROR_FAILURE;
  }

  if (childframe) {
    if (NS_SUCCEEDED(rv) && scrollableView) {
      const nsIView * clippedView;
      scrollableView->GetClipView(&clippedView);
      nscoord x;
      nscoord y;
      scrollableView->GetScrollPosition(x,y);
      // get the clipped rect
      nsRect rect;
      clippedView->GetBounds(rect);
      // now move it by the offset of the scroll position
      rect.x = 0;
      rect.y = 0;
      rect.MoveBy(x,y);

      // get the child
      nsRect fRect;
      childframe->GetRect(fRect);
      nsPoint pnt;
      nsIView * view;
      childframe->GetOffsetFromView(mPresContext, pnt, &view);

      // see if the selected frame is inside the scrolled area
      if (!rect.Contains(fRect)) {
        // figure out which direction we are going
        if (fRect.y+fRect.height >= rect.y+rect.height) {
          y = fRect.y-(rect.height-fRect.height);
        } else {
          y = fRect.y;
        }
        scrollableView->ScrollTo(pnt.x, y, PR_TRUE);
      }

    }
  }
  return NS_OK;
}

nsresult
nsListControlFrame::KeyDown(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (keyEvent) {
    PRUint32 code;
    //uiEvent->GetCharCode(&code);
    //printf("%c %d   ", code, code);
    keyEvent->GetKeyCode(&code);
    //printf("%c %d\n", code, code);

    nsresult rv = NS_ERROR_FAILURE; 
    nsIDOMHTMLCollection* options = GetOptions(mContent);

    if (nsnull != options) {
      PRUint32 numOptions;
      options->GetLength(&numOptions);

      if (numOptions == 0) {
        rv = NS_OK;
      } else {
        if (code == nsIDOMKeyEvent::DOM_VK_UP || code == nsIDOMKeyEvent::DOM_VK_LEFT) {
          printf("DOM_VK_UP   mSelectedIndex: %d ", mSelectedIndex);
          if (mSelectedIndex > 0) {
            mOldSelectedIndex = mSelectedIndex;
            mSelectedIndex--;
            SingleSelection();
            if (nsnull != mComboboxFrame && mIsAllFramesHere) {
              mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, mSelectedIndex); // don't dispatch event
            }
          }
          printf("  After: %d\n", mSelectedIndex);
        } if (code == nsIDOMKeyEvent::DOM_VK_DOWN || code == nsIDOMKeyEvent::DOM_VK_RIGHT) {
          printf("DOM_VK_DOWN mSelectedIndex: %d ", mSelectedIndex);
          if ((mSelectedIndex+1) < (PRInt32)numOptions) {
            mOldSelectedIndex = mSelectedIndex;
            mSelectedIndex++;
            SingleSelection();
            if (nsnull != mComboboxFrame && mIsAllFramesHere) {
              mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, mSelectedIndex); // don't dispatch event
            }
          }
          printf("  After: %d\n", mSelectedIndex);
        } if (code == nsIDOMKeyEvent::DOM_VK_RETURN) {
          if (IsInDropDownMode() == PR_TRUE && mComboboxFrame) {
            mComboboxFrame->ListWasSelected(mPresContext);
          } else {
	          UpdateSelection(PR_TRUE, PR_FALSE, mContent);
	        }
        } if (code == nsIDOMKeyEvent::DOM_VK_ESCAPE) {
          if (IsInDropDownMode() == PR_TRUE && mComboboxFrame) {
            ResetSelectedItem();
            mComboboxFrame->ListWasSelected(mPresContext); 
          } 
        } else { // Select option with this as the first character
          // XXX Not I18N compliant
          PRInt32 selectedIndex = (mSelectedIndex == kNothingSelected ? 0 : mSelectedIndex+1) % numOptions;
          PRInt32 startedAtIndex    = selectedIndex;
          PRBool  loopedAround  = PR_FALSE;
          while ((selectedIndex < startedAtIndex && loopedAround) || !loopedAround) {
            nsIDOMHTMLOptionElement* optionElement = GetOption(*options, selectedIndex);
            if (nsnull != optionElement) {
              nsAutoString text;
              if (NS_CONTENT_ATTR_HAS_VALUE == optionElement->GetText(text)) {
                char * buf = text.ToNewCString();
                char c = buf[0];
                delete [] buf;
                if (c == (char)code) {
                  mOldSelectedIndex = mSelectedIndex;
                  mSelectedIndex    = selectedIndex;
                  SingleSelection();
                  if (nsnull != mComboboxFrame && mIsAllFramesHere) {
                    mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, mSelectedIndex); // don't dispatch event
                  }
                  break;
                }
              }
              NS_RELEASE(optionElement);
            }
            selectedIndex++;
            if (selectedIndex == (PRInt32)numOptions) {
              selectedIndex = 0;
              loopedAround = PR_TRUE;
            }

          } // while
        }
      }
      NS_RELEASE(options);
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetStateType(nsIPresContext* aPresContext,
                                 nsIStatefulFrame::StateType* aStateType)
{
  *aStateType = nsIStatefulFrame::eSelectType;
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::SaveState(nsIPresContext* aPresContext,
                              nsISupports** aState)
{
  nsISupportsArray* value = nsnull;
  nsresult res = NS_NewISupportsArray(&value);
  if (NS_SUCCEEDED(res) && value) {
    PRInt32 j=0;
    PRInt32 length = 0;
    GetNumberOfOptions(&length);
    PRInt32 i;
    for (i=0; i<length; i++) {
      PRBool selected = PR_FALSE;
      res = GetOptionSelected(i, &selected);
      if (NS_SUCCEEDED(res) && selected) {
        nsISupportsPRInt32* thisVal = nsnull;
        res = nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_PROGID,
	                       nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)&thisVal);
        if (NS_SUCCEEDED(res) && thisVal) {
          res = thisVal->SetData(i);
	  if (NS_SUCCEEDED(res)) {
            PRBool okay = value->InsertElementAt((nsISupports *)thisVal, j++);
	    if (!okay) res = NS_ERROR_OUT_OF_MEMORY; // Most likely cause;
	  }
	  if (!NS_SUCCEEDED(res)) NS_RELEASE(thisVal);
	}
      }
      if (!NS_SUCCEEDED(res)) break;
    }
    if (i<length)
      NS_RELEASE(value);
  }
  *aState = (nsISupports*)value;  // Set to null if not successful
  return res;
}

NS_IMETHODIMP
nsListControlFrame::RestoreState(nsIPresContext* aPresContext,
                                 nsISupports* aState)
{
  nsISupportsArray* value = (nsISupportsArray *)aState;
  nsresult res = NS_ERROR_NULL_POINTER;
  if (value) {
    res = Deselect();
    if (NS_SUCCEEDED(res)) {
      PRUint32 count = 0;
      res = value->Count(&count);
      if (NS_SUCCEEDED(res)) {
        nsISupportsPRInt32* thisVal = nsnull;
        PRInt32 j=0;
        for (PRUint32 i=0; i<count; i++) {
          thisVal = (nsISupportsPRInt32*) value->ElementAt(i);
          if (thisVal) {
            res = thisVal->GetData(&j);
            if (NS_SUCCEEDED(res)) {
              res = SetOptionSelected(j, PR_TRUE);
            }
          } else {
            res = NS_ERROR_UNEXPECTED;
          }
          if (!NS_SUCCEEDED(res)) break;
        }
      }
    }
  }
  return res;
}

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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIDOMNSHTMLOptionCollection.h"
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
#include "nsLayoutAtoms.h"
#include "nsIFontMetrics.h"
#include "nsVoidArray.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMEventTarget.h"

#include "nsISelectElement.h"
#include "nsIPrivateDOMEvent.h"

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
//const char * kMozSelected = "-moz-option-selected";
// it is now using "nsLayoutAtoms::optionSelectedPseudo"

//---------------------------------------------------------
nsresult
NS_NewListControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsListControlFrame* it = new (aPresShell) nsListControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
#if 0
  // set the state flags (if any are provided)
  nsFrameState state;
  it->GetFrameState( &state );
  state |= NS_BLOCK_SPACE_MGR;
  it->SetFrameState( state );
#endif
  *aNewFrame = it;
  return NS_OK;
}

//-----------------------------------------------------------
// Reflow Debugging Macros
// These let us "see" how many reflow counts are happening
//-----------------------------------------------------------
#ifdef DO_REFLOW_COUNTER

#define MAX_REFLOW_CNT 1024
static PRInt32 gTotalReqs    = 0;;
static PRInt32 gTotalReflows = 0;;
static PRInt32 gReflowControlCntRQ[MAX_REFLOW_CNT];
static PRInt32 gReflowControlCnt[MAX_REFLOW_CNT];
static PRInt32 gReflowInx = -1;

#define REFLOW_COUNTER() \
  if (mReflowId > -1) \
    gReflowControlCnt[mReflowId]++;

#define REFLOW_COUNTER_REQUEST() \
  if (mReflowId > -1) \
    gReflowControlCntRQ[mReflowId]++;

#define REFLOW_COUNTER_DUMP(__desc) \
  if (mReflowId > -1) {\
    gTotalReqs    += gReflowControlCntRQ[mReflowId];\
    gTotalReflows += gReflowControlCnt[mReflowId];\
    printf("** Id:%5d %s RF: %d RQ: %d   %d/%d  %5.2f\n", \
           mReflowId, (__desc), \
           gReflowControlCnt[mReflowId], \
           gReflowControlCntRQ[mReflowId],\
           gTotalReflows, gTotalReqs, float(gTotalReflows)/float(gTotalReqs)*100.0f);\
  }

#define REFLOW_COUNTER_INIT() \
  if (gReflowInx < MAX_REFLOW_CNT) { \
    gReflowInx++; \
    mReflowId = gReflowInx; \
    gReflowControlCnt[mReflowId] = 0; \
    gReflowControlCntRQ[mReflowId] = 0; \
  } else { \
    mReflowId = -1; \
  }

// reflow messages
#define REFLOW_DEBUG_MSG(_msg1) printf((_msg1))
#define REFLOW_DEBUG_MSG2(_msg1, _msg2) printf((_msg1), (_msg2))
#define REFLOW_DEBUG_MSG3(_msg1, _msg2, _msg3) printf((_msg1), (_msg2), (_msg3))
#define REFLOW_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) printf((_msg1), (_msg2), (_msg3), (_msg4))

#else //-------------

#define REFLOW_COUNTER_REQUEST() 
#define REFLOW_COUNTER() 
#define REFLOW_COUNTER_DUMP(__desc) 
#define REFLOW_COUNTER_INIT() 

#define REFLOW_DEBUG_MSG(_msg) 
#define REFLOW_DEBUG_MSG2(_msg1, _msg2) 
#define REFLOW_DEBUG_MSG3(_msg1, _msg2, _msg3) 
#define REFLOW_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) 


#endif

//------------------------------------------
// This is for being VERY noisy
//------------------------------------------
#ifdef DO_VERY_NOISY
#define REFLOW_NOISY_MSG(_msg1) printf((_msg1))
#define REFLOW_NOISY_MSG2(_msg1, _msg2) printf((_msg1), (_msg2))
#define REFLOW_NOISY_MSG3(_msg1, _msg2, _msg3) printf((_msg1), (_msg2), (_msg3))
#define REFLOW_NOISY_MSG4(_msg1, _msg2, _msg3, _msg4) printf((_msg1), (_msg2), (_msg3), (_msg4))
#else
#define REFLOW_NOISY_MSG(_msg) 
#define REFLOW_NOISY_MSG2(_msg1, _msg2) 
#define REFLOW_NOISY_MSG3(_msg1, _msg2, _msg3) 
#define REFLOW_NOISY_MSG4(_msg1, _msg2, _msg3, _msg4) 
#endif

//------------------------------------------
// Displays value in pixels or twips
//------------------------------------------
#ifdef DO_PIXELS
#define PX(__v) __v / 15
#else
#define PX(__v) __v 
#endif

//------------------------------------------
// Asserts if we return a desired size that 
// doesn't correctly match the mComputedWidth
//------------------------------------------
#ifdef DO_UNCONSTRAINED_CHECK
#define UNCONSTRAINED_CHECK() \
if (aReflowState.mComputedWidth != NS_UNCONSTRAINEDSIZE) { \
  nscoord width = aDesiredSize.width - borderPadding.left - borderPadding.right; \
  if (width != aReflowState.mComputedWidth) { \
    printf("aDesiredSize.width %d %d != aReflowState.mComputedWidth %d\n", aDesiredSize.width, width, aReflowState.mComputedWidth); \
  } \
  NS_ASSERTION(width == aReflowState.mComputedWidth, "Returning bad value when constrained!"); \
}
#else
#define UNCONSTRAINED_CHECK()
#endif
//------------------------------------------------------
//-- Done with macros
//------------------------------------------------------


//---------------------------------------------------------
nsListControlFrame::nsListControlFrame()
:   mWeakReferent(this)
{
  mSelectedIndex      = kNothingSelected;
  mComboboxFrame      = nsnull;
  mFormFrame          = nsnull;
  mButtonDown         = PR_FALSE;
  mMaxWidth           = 0;
  mMaxHeight          = 0;
  mPresContext        = nsnull;
  mEndExtendedIndex   = kNothingSelected;
  mStartExtendedIndex = kNothingSelected;
  mIsCapturingMouseEvents = PR_FALSE;
  mDelayedIndexSetting = kNothingSelected;
  mDelayedValueSetting = PR_FALSE;

  mSelectionCache        = new nsVoidArray();
  mSelectionCacheLength     = 0;

  mIsAllContentHere   = PR_FALSE;
  mIsAllFramesHere    = PR_FALSE;
  mHasBeenInitialized = PR_FALSE;

  mCacheSize.width             = -1;
  mCacheSize.height            = -1;
  mCachedMaxElementSize.width  = -1;
  mCachedMaxElementSize.height = -1;
  mCachedAvailableSize.width   = -1;
  mCachedAvailableSize.height  = -1;
  mCachedUnconstrainedSize.width  = -1;
  mCachedUnconstrainedSize.height = -1;
  
  mOverrideReflowOpt           = PR_FALSE;

  REFLOW_COUNTER_INIT()
}

//---------------------------------------------------------
nsListControlFrame::~nsListControlFrame()
{
  REFLOW_COUNTER_DUMP("nsLCF");

  mComboboxFrame = nsnull;
  if (mFormFrame) {
    mFormFrame->RemoveFormControlFrame(*this);
    mFormFrame = nsnull;
  }
  NS_IF_RELEASE(mPresContext);
  if (mSelectionCache) {
    delete mSelectionCache;
  }
}

// for Bug 47302 (remove this comment later)
NS_IMETHODIMP
nsListControlFrame::Destroy(nsIPresContext *aPresContext)
{
  // get the reciever interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mContent));

  nsCOMPtr<nsIDOMMouseListener> mouseListener = do_QueryInterface(mEventListener);
  if (!mouseListener) { return NS_ERROR_NO_INTERFACE; }
  reciever->RemoveEventListenerByIID(mouseListener, NS_GET_IID(nsIDOMMouseListener));

  nsCOMPtr<nsIDOMMouseMotionListener> mouseMotionListener = do_QueryInterface(mEventListener);
  if (!mouseMotionListener) { return NS_ERROR_NO_INTERFACE; }
  reciever->RemoveEventListenerByIID(mouseMotionListener, NS_GET_IID(nsIDOMMouseMotionListener));

  nsCOMPtr<nsIDOMKeyListener> keyListener = do_QueryInterface(mEventListener);
  if (!keyListener) { return NS_ERROR_NO_INTERFACE; }
  reciever->RemoveEventListenerByIID(keyListener, NS_GET_IID(nsIDOMKeyListener));

  if (IsInDropDownMode() == PR_FALSE) {
    nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  }
  return nsScrollFrame::Destroy(aPresContext);
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
  if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIListControlFrame))) {
    *aInstancePtr = (void *)((nsIListControlFrame*)this);
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISelectControlFrame))) {
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
// Reflow is overriden to constrain the listbox height to the number of rows and columns
// specified. 
#ifdef DO_REFLOW_DEBUG
static int myCounter = 0;

static void printSize(char * aDesc, nscoord aSize) 
{
  printf(" %s: ", aDesc);
  if (aSize == NS_UNCONSTRAINEDSIZE) {
    printf("UNC");
  } else {
    printf("%d", aSize);
  }
}
#endif

//-----------------------------------------------------------------
// Main Reflow for ListBox/Dropdown
//-----------------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::Reflow(nsIPresContext*          aPresContext, 
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState, 
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsListControlFrame", aReflowState.reason);
  REFLOW_COUNTER_REQUEST();

  aStatus = NS_FRAME_COMPLETE;

#ifdef DO_REFLOW_DEBUG
  printf("%p ** Id: %d nsLCF::Reflow %d R: ", this, mReflowId, myCounter++);
  switch (aReflowState.reason) {
    case eReflowReason_Initial:
      printf("Initia");break;
    case eReflowReason_Incremental:
      printf("Increm");break;
    case eReflowReason_Resize:
      printf("Resize");break;
    case eReflowReason_StyleChange:
      printf("StyleC");break;
    case eReflowReason_Dirty:
      printf("Dirty ");break;
    default:printf("<unknown>%d", aReflowState.reason);break;
  }
  
  printSize("AW", aReflowState.availableWidth);
  printSize("AH", aReflowState.availableHeight);
  printSize("CW", aReflowState.mComputedWidth);
  printSize("CH", aReflowState.mComputedHeight);
  printf("\n");
#if 0
    {
      const nsStyleDisplay* display;
      GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
      printf("+++++++++++++++++++++++++++++++++ ");
      switch (display->mVisible) {
        case NS_STYLE_VISIBILITY_COLLAPSE: printf("NS_STYLE_VISIBILITY_COLLAPSE\n");break;
        case NS_STYLE_VISIBILITY_HIDDEN:   printf("NS_STYLE_VISIBILITY_HIDDEN\n");break;
        case NS_STYLE_VISIBILITY_VISIBLE:  printf("NS_STYLE_VISIBILITY_VISIBLE\n");break;
      }
    }
#endif
#endif // DEBUG_rodsXXX

  PRBool bailOnWidth;
  PRBool bailOnHeight;
  // This ifdef is for turning off the optimization
  // so we can check timings against the old version
#if 1

  nsFormControlFrame::SkipResizeReflow(mCacheSize, 
                                       mCachedMaxElementSize, 
                                       mCachedAvailableSize, 
                                       aDesiredSize, aReflowState, 
                                       aStatus, 
                                       bailOnWidth, bailOnHeight);

  // Here we bail if both the height and the width haven't changed
  // also we see if we should override the optimization
  //
  // The optimization can get overridden by the combobox 
  // sometime the combobox knows that the list MUST do a full reflow
  // no matter what
  if (!mOverrideReflowOpt && bailOnWidth && bailOnHeight) {
    REFLOW_DEBUG_MSG3("*** Done nsLCF - Bailing on DW: %d  DH: %d ", PX(aDesiredSize.width), PX(aDesiredSize.height));
    REFLOW_DEBUG_MSG3("bailOnWidth %d  bailOnHeight %d\n", PX(bailOnWidth), PX(bailOnHeight));
    NS_ASSERTION(aDesiredSize.width < 100000, "Width is still NS_UNCONSTRAINEDSIZE");
    NS_ASSERTION(aDesiredSize.height < 100000, "Height is still NS_UNCONSTRAINEDSIZE");
    return NS_OK;
  } else if (mOverrideReflowOpt) {
    mOverrideReflowOpt = PR_FALSE;
  }

#else
  bailOnWidth  = PR_FALSE;
  bailOnHeight = PR_FALSE;
#endif

#ifdef DEBUG_rodsXXX
  // Lists out all the options
  {
  nsresult rv = NS_ERROR_FAILURE; 
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    printf("--- Num of Items %d ---\n", numOptions);
    for (PRUint32 i=0;i<numOptions;i++) {
      nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = getter_AddRefs(GetOption(*options, i));
      if (optionElement) {
        nsAutoString text;
        rv = optionElement->GetLabel(text);
        if (NS_CONTENT_ATTR_HAS_VALUE != rv || 0 == text.Length()) {
          if (NS_OK != optionElement->GetText(text)) {
            text = "No Value";
          }
        } else {
            text = "No Value";
        }          
        printf("[%d] - %s\n", i, text.ToNewCString());
      }
    }
  }
  }
#endif // DEBUG_rodsXXX


  if (mDelayedIndexSetting != kNothingSelected) {
    SetOptionSelected(mDelayedIndexSetting, mDelayedValueSetting);
    mDelayedIndexSetting = kNothingSelected;
  }

  // If all the content and frames are here 
  // then initialize it before reflow
    if (mIsAllContentHere && !mHasBeenInitialized) {
      if (PR_FALSE == mIsAllFramesHere) {
        CheckIfAllFramesHere();
      }
      if (mIsAllFramesHere && !mHasBeenInitialized) {
        mHasBeenInitialized = PR_TRUE;
        Reset(aPresContext);
      }
    }


    if (eReflowReason_Incremental == aReflowState.reason) {
      nsIFrame* targetFrame;
      aReflowState.reflowCommand->GetTarget(targetFrame);
      if (targetFrame == this) {
        // XXX So this may do it too often
        // the side effect of this is if the user has scrolled to some other place in the list and
        // an incremental reflow comes through the list gets scrolled to the first selected item
        // I haven't been able to make it do it, but it will do it
        // basically the real solution is to know when all the reframes are there.
        nsCOMPtr<nsIContent> content = getter_AddRefs(GetOptionContent(mSelectedIndex));
        if (content) {
          ScrollToFrame(content);
        }
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
  if (eReflowReason_Initial == aReflowState.reason) {
    if (mPresState) {
      RestoreState(aPresContext, mPresState);
      mPresState = do_QueryInterface(nsnull);
    }
    if (IsInDropDownMode() == PR_FALSE && !mFormFrame) {
      nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
      nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*, this));
    }
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


  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIFrame* targetFrame;
    firstPassState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIReflowCommand::ReflowType type;
      aReflowState.reflowCommand->GetType(type);
      firstPassState.reason = eReflowReason_StyleChange;
      firstPassState.reflowCommand = nsnull;
    } else {
      nsresult res = nsScrollFrame::Reflow(aPresContext, 
                                           scrolledAreaDesiredSize,
                                           aReflowState, 
                                           aStatus);
      if (NS_FAILED(res)) {
        NS_ASSERTION(aDesiredSize.width < 100000, "Width is still NS_UNCONSTRAINEDSIZE");
        NS_ASSERTION(aDesiredSize.height < 100000, "Height is still NS_UNCONSTRAINEDSIZE");
        return res;
      }
      nsIReflowCommand::ReflowType type;
      aReflowState.reflowCommand->GetType(type);
      firstPassState.reason = eReflowReason_StyleChange;
      firstPassState.reflowCommand = nsnull;
    }
  }

  nsresult res = nsScrollFrame::Reflow(aPresContext, 
                                       scrolledAreaDesiredSize,
                                       firstPassState, 
                                       aStatus);
  if (NS_FAILED(res)) {
    NS_ASSERTION(aDesiredSize.width < 100000, "Width is still NS_UNCONSTRAINEDSIZE");
    NS_ASSERTION(aDesiredSize.height < 100000, "Height is still NS_UNCONSTRAINEDSIZE");
    return res;
  }

  nsIScrollableFrame * scrollableFrame = nsnull;
  nsRect scrolledRect;
  if (NS_SUCCEEDED(QueryInterface(NS_GET_IID(nsIScrollableFrame), (void**)&scrollableFrame))) {
    nsIFrame * scrolledFrame;
    scrollableFrame->GetScrolledFrame(aPresContext, scrolledFrame);
    NS_ASSERTION(scrolledFrame != nsnull, "Must have scrollable frame");
    scrolledFrame->GetRect(scrolledRect);
  } else {
    NS_ASSERTION(scrollableFrame != nsnull, "Must have scrollableFrame frame");
  }
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
  aPresContext->GetDeviceContext(getter_AddRefs(dc));
  dc->GetScrollBarDimensions(sbWidth, sbHeight);
  // Convert to nscoord's by rounding
  nscoord scrollbarWidth  = NSToCoordRound(sbWidth);
  //nscoord scrollbarHeight = NSToCoordRound(sbHeight);

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

  mMaxWidth  -= (border.left + border.right + padding.left + padding.right);
  mMaxHeight -= (border.top + border.bottom + padding.top + padding.bottom);

  // Now the scrolledAreaWidth and scrolledAreaHeight are exactly 
  // wide and high enough to enclose their contents

  PRBool isInDropDownMode = IsInDropDownMode();

  scrolledAreaWidth  -= (border.left + border.right + padding.left + padding.right);
  scrolledAreaHeight -= (border.top + border.bottom + padding.top + padding.bottom);

  nscoord visibleWidth = 0;
  if (isInDropDownMode) {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
      visibleWidth = scrolledAreaWidth;
    } else {
      visibleWidth = aReflowState.mComputedWidth;
      visibleWidth -= (border.left + border.right + padding.left + padding.right);
      //scrolledAreaHeight -= (border.top + border.bottom + padding.top + padding.bottom);
    }
  } else {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
      visibleWidth = scrolledAreaWidth;
    } else {
      visibleWidth = aReflowState.mComputedWidth;// - scrollbarWidth;
      // XXX rods - this hould not be subtracted in
      //visibleWidth  -= (border.left + border.right + padding.left + padding.right);
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

  // Check to see if we have zero items 
  PRInt32 length = 0;
  GetNumberOfOptions(&length);

  // If there is only one option and that option's content is empty
  // then heightOfARow is zero, so we need to go measure 
  // the height of the option as if it had some text.
  if (heightOfARow == 0 && length > 0) {
    nsIContent * option = GetOptionContent(0);
    if (option != nsnull) {
      nsIFrame * optFrame;
      nsCOMPtr<nsIPresShell> presShell;
      mPresContext->GetShell(getter_AddRefs(presShell));
      nsresult result = presShell->GetPrimaryFrameFor(option, &optFrame);
      if (NS_SUCCEEDED(result) && optFrame != nsnull) {
        nsCOMPtr<nsIStyleContext> optStyle;
        optFrame->GetStyleContext(getter_AddRefs(optStyle));
        if (optStyle) {
          const nsStyleFont* styleFont = (const nsStyleFont*)optStyle->GetStyleData(eStyleStruct_Font);
          nsCOMPtr<nsIDeviceContext> deviceContext;
          aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));
          NS_ASSERTION(deviceContext, "Couldn't get the device context"); 
          nsIFontMetrics * fontMet;
          result = deviceContext->GetMetricsFor(styleFont->mFont, fontMet);
          if (NS_SUCCEEDED(result) && fontMet != nsnull) {
            if (fontMet) {
              fontMet->GetHeight(heightOfARow);
              mMaxHeight = heightOfARow;
            }
            NS_RELEASE(fontMet);
          }
        }
      }
      NS_RELEASE(option);
    }
  }

  // Check to see if we have no width and height
  // The following code measures the width and height 
  // of a bogus string so the list actually displays
  nscoord visibleHeight = 0;
  if (isInDropDownMode) {
    // Compute the visible height of the drop-down list
    // The dropdown list height is the smaller of it's height setting or the height
    // of the smallest box that can drawn around it's contents.
    visibleHeight = scrolledAreaHeight;

    mNumDisplayRows = kMaxDropDownRows;
    if (visibleHeight > (mNumDisplayRows * heightOfARow)) {
      visibleHeight = (mNumDisplayRows * heightOfARow);
      // This is an adaptive algorithm for figuring out how many rows 
      // should be displayed in the drop down. The standard size is 20 rows, 
      // but on 640x480 it is typically too big.
      // This takes the height of the screen divides it by two and then subtracts off 
      // an estimated height of the combobox. I estimate it by taking the max element size
      // of the drop down and multiplying it by 2 (this is arbitrary) then subtract off
      // the border and padding of the drop down (again rather arbitrary)
      // This all breaks down if the font of the combobox is a lot larger then the option items
      // or CSS style has set the height of the combobox to be rather large.
      // We can fix these cases later if they actually happen.
      if (isInDropDownMode) {
        nscoord screenHeightInPixels = 0;
        if (NS_SUCCEEDED(nsFormControlFrame::GetScreenHeight(aPresContext, screenHeightInPixels))) {
          float   p2t;
          aPresContext->GetPixelsToTwips(&p2t);
          nscoord screenHeight = NSIntPixelsToTwips(screenHeightInPixels, p2t);

          nscoord availDropHgt = (screenHeight / 2) - (heightOfARow*2); // approx half screen minus combo size
          availDropHgt -= (border.top + border.bottom + padding.top + padding.bottom);

          nscoord hgt = visibleHeight + border.top + border.bottom + padding.top + padding.bottom;
          if (hgt > availDropHgt) {
            visibleHeight = (availDropHgt / heightOfARow) * heightOfARow;
          }
          mNumDisplayRows = visibleHeight / heightOfARow;
        }
      }
    }
   
  } else {
      // Calculate the visible height of the listbox
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
      visibleHeight = aReflowState.mComputedHeight;
      visibleHeight -= (border.top + border.bottom + padding.top + padding.bottom);
    } else {
      mNumDisplayRows = 1;
      GetSizeAttribute(&mNumDisplayRows);
      // because we are not a drop down 
      // we will always have 2 or more rows
      if (mNumDisplayRows >= 1) {
        visibleHeight = mNumDisplayRows * heightOfARow;
      } else {
        PRBool multipleSelections = PR_FALSE;
        GetMultiple(&multipleSelections);
        if (multipleSelections) {
          visibleHeight = PR_MIN(length, kMaxDropDownRows) * heightOfARow;
        } else {
          visibleHeight = 2 * heightOfARow;
        }
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

  if (needsVerticalScrollbar) {
    mIsScrollbarVisible = PR_TRUE; // XXX temp code
  } else {
    mIsScrollbarVisible = PR_FALSE; // XXX temp code
  }

  // The visible height is zero, this could be a select with no options
  // or a select with a single option that has no content or no label
  //
  // So this may not be the best solution, but we get the height of the font
  // for the list frame and use that as the max/minimum size for the contents
  if (visibleHeight == 0) {
    nsCOMPtr<nsIFontMetrics> fontMet;
    nsresult rvv = nsFormControlHelper::GetFrameFontFM(aPresContext, this, getter_AddRefs(fontMet));
    if (NS_SUCCEEDED(rvv) && fontMet) {
      aReflowState.rendContext->SetFont(fontMet);
      fontMet->GetHeight(visibleHeight);
      mMaxHeight = visibleHeight;
    }
  }

  if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
    visibleWidth += (border.left + border.right + padding.left + padding.right);
  }

   // Do a second reflow with the adjusted width and height settings
   // This sets up all of the frames with the correct width and height.
  secondPassState.mComputedWidth  = visibleWidth;
  secondPassState.mComputedHeight = visibleHeight;
  secondPassState.reason = eReflowReason_Resize;

  nsScrollFrame::Reflow(aPresContext, aDesiredSize, secondPassState, aStatus);

    // Set the max element size to be the same as the desired element size.
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;

#ifdef DEBUG_rods
  if (!isInDropDownMode) {
    PRInt32 numRows = 1;
    GetSizeAttribute(&numRows);
    printf("%d ", numRows);
    if (numRows == 2) {
      COMPARE_QUIRK_SIZE("nsListControlFrame", 56, 38) 
    } else if (numRows == 3) {
      COMPARE_QUIRK_SIZE("nsListControlFrame", 56, 54) 
    } else if (numRows == 4) {
      COMPARE_QUIRK_SIZE("nsListControlFrame", 56, 70) 
    } else {
      COMPARE_QUIRK_SIZE("nsListControlFrame", 127, 118) 
    }
  }
#endif

  if (aReflowState.availableWidth != NS_UNCONSTRAINEDSIZE) {
    mCachedAvailableSize.width  = aDesiredSize.width - (border.left + border.right + padding.left + padding.right);
    REFLOW_DEBUG_MSG2("** nsLCF Caching AW: %d\n", PX(mCachedAvailableSize.width));
  }
  if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
    mCachedAvailableSize.height = aDesiredSize.height - (border.top + border.bottom + padding.top + padding.bottom);
    REFLOW_DEBUG_MSG2("** nsLCF Caching AH: %d\n", PX(mCachedAvailableSize.height));
  }

  //REFLOW_DEBUG_MSG3("** nsLCF Caching AW: %d  AH: %d\n", PX(mCachedAvailableSize.width), PX(mCachedAvailableSize.height));

  nsFormControlFrame::SetupCachedSizes(mCacheSize, mCachedMaxElementSize, aDesiredSize);

  REFLOW_DEBUG_MSG3("** Done nsLCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));

  REFLOW_COUNTER();

#ifdef DO_REFLOW_COUNTER
  if (gReflowControlCnt[mReflowId] > 50) {
    REFLOW_DEBUG_MSG3("** Id: %d Cnt: %d ", mReflowId, gReflowControlCnt[mReflowId]);
    REFLOW_DEBUG_MSG3("Done nsLCF DW: %d  DH: %d\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
  }
#endif

  NS_ASSERTION(aDesiredSize.width < 100000, "Width is still NS_UNCONSTRAINEDSIZE");
  NS_ASSERTION(aDesiredSize.height < 100000, "Height is still NS_UNCONSTRAINEDSIZE");
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
nsListControlFrame::GetFont(nsIPresContext* aPresContext, 
                            const nsFont*&  aFont)
{
  return nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
}


//---------------------------------------------------------
PRBool 
nsListControlFrame::IsOptionElement(nsIContent* aContent)
{
  PRBool result = PR_FALSE;
 
  nsCOMPtr<nsIDOMHTMLOptionElement> optElem;
  if (NS_SUCCEEDED(aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLOptionElement),(void**) getter_AddRefs(optElem)))) {      
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
  PRBool result = PR_FALSE;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  if (content) {
    result = IsOptionElement(content);
  }
  return result;
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
  
  if (PR_TRUE == mIsAllFramesHere) {
    aContent->SetAttribute(kNameSpaceID_None, nsLayoutAtoms::optionSelectedPseudo, nsAutoString(), PR_TRUE);
    //ForceRedraw();
  } else {
    aContent->SetAttribute(kNameSpaceID_None, nsLayoutAtoms::optionSelectedPseudo, nsAutoString(), PR_FALSE);
  }
}

//---------------------------------------------------------
void 
nsListControlFrame::DisplayDeselected(nsIContent* aContent) 
{
   //XXX: This is temporary. It simulates psuedo states by using a attribute selector on 
   // -moz-option-selected in the ua.css style sheet. This will not be needed when
   // The event state manager is functional. KMM
  if (PR_TRUE == mIsAllFramesHere) {
    aContent->UnsetAttribute(kNameSpaceID_None, nsLayoutAtoms::optionSelectedPseudo, PR_TRUE);
    //ForceRedraw();
  } else {
    aContent->UnsetAttribute(kNameSpaceID_None, nsLayoutAtoms::optionSelectedPseudo, PR_FALSE);
  }

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
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);

    PRUint32 inx;
    for (inx = 0; inx < numOptions; inx++) {
      nsIContent * option = GetOptionAsContent(options, inx);
      if (option != nsnull) {
        if (option == aContent) {
          NS_RELEASE(option);
          return inx;
        }
        NS_RELEASE(option);
      }
    }
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
  NS_ASSERTION(aHitFrame, "No frame for html <select> element\n");  

  PRInt32 indx = kNothingSelected;
  // Get the content of the frame that was selected
  if (aHitFrame) {
    nsCOMPtr<nsIContent> selectedContent;
    aHitFrame->GetContent(getter_AddRefs(selectedContent));
    if (selectedContent) {
      indx = GetSelectedIndexFromContent(selectedContent);
    }
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
#ifdef XP_MAC
    mouseEvent->GetMetaKey(&isControl);
#else
    mouseEvent->GetCtrlKey(&isControl);
#endif
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
nsListControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  /*const char * desc[] = {"NS_MOUSE_MOVE", 
                          "NS_MOUSE_LEFT_BUTTON_UP",
                          "NS_MOUSE_LEFT_BUTTON_DOWN",
                          "<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>",
                          "NS_MOUSE_MIDDLE_BUTTON_UP",
                          "NS_MOUSE_MIDDLE_BUTTON_DOWN",
                          "<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>",
                          "NS_MOUSE_RIGHT_BUTTON_UP",
                          "NS_MOUSE_RIGHT_BUTTON_DOWN",
                          "NS_MOUSE_ENTER_SYNTH",
                          "NS_MOUSE_EXIT_SYNTH",
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

  if (nsEventStatus_eConsumeNoDefault == *aEventStatus)
    return NS_OK;

  // do we have style that affects how we are selected?
  // do we have user-input style?
  const nsStyleUserInterface* uiStyle;
  GetStyleData(eStyleStruct_UserInterface,  (const nsStyleStruct *&)uiStyle);
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  if (nsFormFrame::GetDisabled(this))
    return NS_OK;

  switch(aEvent->message) {
    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
#ifdef DEBUG_rodsXXX
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        printf("---> %d %c\n", keyEvent->keyCode, keyEvent->keyCode);
#endif
        //if (NS_VK_SPACE == keyEvent->keyCode || NS_VK_RETURN == keyEvent->keyCode) {
        //  MouseClicked(aPresContext);
        //}
      }
      break;
    default:
      break;
  }

  return nsScrollFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SetInitialChildList(nsIPresContext* aPresContext,
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
  nsresult rv = nsScrollFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // If all the content is here now check
  // to see if all the frames have been created
  /*if (mIsAllContentHere) {
    // If all content and frames are here
    // the reset/initialize
    if (CheckIfAllFramesHere()) {
      Reset(aPresContext);
      mHasBeenInitialized = PR_TRUE;
    }
  }*/

  return rv;
}

//---------------------------------------------------------
nsresult
nsListControlFrame::GetSizeAttribute(PRInt32 *aSize) {
  nsresult rv = NS_OK;
  nsIDOMHTMLSelectElement* selectElement;
  rv = mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),(void**) &selectElement);
  if (mContent && NS_SUCCEEDED(rv)) {
    rv = selectElement->GetSize(aSize);
    NS_RELEASE(selectElement);
  }
  return rv;
}


//---------------------------------------------------------
NS_IMETHODIMP  
nsListControlFrame::Init(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);
  nsresult result = nsScrollFrame::Init(aPresContext, aContent, aParent, aContext,
                                        aPrevInFlow);

  // get the reciever interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mContent));

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  // we need to hook up our listeners before the editor is initialized
  result = NS_NewListEventListener(getter_AddRefs(mEventListener));
  if (NS_FAILED(result)) { return result ; }
  if (!mEventListener) { return NS_ERROR_NULL_POINTER; }

  mEventListener->SetFrame(this);

  nsCOMPtr<nsIDOMMouseListener> mouseListener = do_QueryInterface(mEventListener);
  if (!mouseListener) { return NS_ERROR_NO_INTERFACE; }
  reciever->AddEventListenerByIID(mouseListener, NS_GET_IID(nsIDOMMouseListener));

  nsCOMPtr<nsIDOMMouseMotionListener> mouseMotionListener = do_QueryInterface(mEventListener);
  if (!mouseMotionListener) { return NS_ERROR_NO_INTERFACE; }
  reciever->AddEventListenerByIID(mouseMotionListener, NS_GET_IID(nsIDOMMouseMotionListener));

  nsCOMPtr<nsIDOMKeyListener> keyListener = do_QueryInterface(mEventListener);
  if (!keyListener) { return NS_ERROR_NO_INTERFACE; }
  reciever->AddEventListenerByIID(keyListener, NS_GET_IID(nsIDOMKeyListener));

  return result;
}


//---------------------------------------------------------
nscoord 
nsListControlFrame::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(0, aPixToTwip); 
}


//---------------------------------------------------------
nscoord 
nsListControlFrame::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
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
    nsresult result = mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
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
  nsresult result = aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                             (void**)&selectElement);
  if (NS_SUCCEEDED(result) && selectElement) {
    return selectElement;
  } else {
    return nsnull;
  }
}


//---------------------------------------------------------
// Returns the nsIContent object in the collection 
// for a given index (AddRefs)
//---------------------------------------------------------
nsIContent* 
nsListControlFrame::GetOptionAsContent(nsIDOMHTMLCollection* aCollection, PRInt32 aIndex) 
{
  nsIContent * content = nsnull;
  nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = getter_AddRefs(GetOption(*aCollection, aIndex));
  if (optionElement) {
    optionElement->QueryInterface(NS_GET_IID(nsIContent),(void**) &content);
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
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (options) {
    content = GetOptionAsContent(options, aIndex);
  }
  return(content);
}

//---------------------------------------------------------
// This returns the collection for nsIDOMHTMLSelectElement or
// the nsIContent object is the select is null  (AddRefs)
//---------------------------------------------------------
nsIDOMHTMLCollection* 
nsListControlFrame::GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect)
{
  nsIDOMNSHTMLOptionCollection* optCol = nsnull;
  nsIDOMHTMLCollection* options = nsnull;
  if (!aSelect) {
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = getter_AddRefs(GetSelect(aContent));
    if (selectElement) {
      selectElement->GetOptions(&optCol);  // AddRefs (1)
    }
  } else {
    aSelect->GetOptions(&optCol); // AddRefs (1)
  }
  if (optCol) {
    nsresult res = optCol->QueryInterface(NS_GET_IID(nsIDOMHTMLCollection), (void **)&options); // AddRefs (2)
    NS_RELEASE(optCol); // Release (1)
  }
  return options;
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
      node->QueryInterface(NS_GET_IID(nsIDOMHTMLOptionElement), (void**)&option);
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
  //nsIAtom * selectedAtom = NS_NewAtom("selected");
  nsresult result = aContent->GetAttribute(kNameSpaceID_None, nsLayoutAtoms::optionSelectedPseudo, value);

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

  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));
  nsIFrame * childframe;
  nsresult result = presShell->GetPrimaryFrameFor(content, &childframe);
  if (NS_SUCCEEDED(result) && childframe != nsnull) {
    //NS_ASSERTION(nsnull != content && aIndex == 0, "Failed to retrieve option content");
    if (nsnull != content) {
      if (aSelected) {
        DisplaySelected(content);
        // Now that it is selected scroll to it
        ScrollToFrame(content);
      } else {
        DisplayDeselected(content);
      }
      NS_RELEASE(content);
    }
  } else {
    mDelayedIndexSetting = aIndex;
    mDelayedValueSetting = aSelected;
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
  if (NS_SUCCEEDED(GetNumberOfOptions(&max))) {
    for (i=0;i<max;i++) {
      SetContentSelected(i, PR_FALSE);
    }
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
  PRBool disabled = PR_FALSE;
  nsFormControlHelper::GetDisabled(mContent, &disabled);
  return !disabled && (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
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
  REFLOW_DEBUG_MSG("LBX::Reset\n");

  // if all the frames aren't here 
  // don't bother reseting
  if (!mIsAllFramesHere) {
    return;
  }

  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (!options) {
    return;
  }

  // Check here to see if this listbox has has its state restored
  // Either by being a list or by being a dropdown.
  // if we have been restored then we don't set any of the default selections
  PRBool hasBeenRestored = mPresState != nsnull;

  PRUint32 numOptions;
  options->GetLength(&numOptions);

  mSelectedIndex      = kNothingSelected;
  mStartExtendedIndex = kNothingSelected;
  mEndExtendedIndex   = kNothingSelected;
  PRBool multiple;
  GetMultiple(&multiple);

  Deselect();

  // Clear the cache and set the default selections
  // if we havn't been restored
  mSelectionCache->Clear();
  mSelectionCacheLength = 0;
  PRUint32 i;
  for (i = 0; i < numOptions; i++) {
    nsCOMPtr<nsIDOMHTMLOptionElement> option = getter_AddRefs(GetOption(*options, i));
    if (option) {
      PRBool selected = PR_FALSE;
      if (!hasBeenRestored) {
        option->GetDefaultSelected(&selected);
      }

      mSelectionCache->AppendElement((void*)selected);
      mSelectionCacheLength++;

      if (selected) {
        mSelectedIndex = i;
        SetContentSelected(i, PR_TRUE);
        if (multiple) {
          mStartExtendedIndex = i;
          if (mEndExtendedIndex == kNothingSelected) {
            mEndExtendedIndex = i;
          }
        }
      }
    }
  }

  // Ok, so we were restored, now set the last known selections from the restore state.
  if (hasBeenRestored) {
    nsCOMPtr<nsISupports> supp;
    mPresState->GetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("selecteditems"), getter_AddRefs(supp));

    nsresult res = NS_ERROR_NULL_POINTER;
    if (!supp)
      return;

    nsCOMPtr<nsISupportsArray> value = do_QueryInterface(supp);
    if (!value)
      return;

    PRUint32 count = 0;
    value->Count(&count);

    nsCOMPtr<nsISupportsPRInt32> thisVal;
    PRInt32 j=0;
    for (i=0; i<count; i++) {
      nsCOMPtr<nsISupports> suppval = getter_AddRefs(value->ElementAt(i));
      thisVal = do_QueryInterface(suppval);
      if (thisVal) {
        res = thisVal->GetData(&j);
        if (NS_SUCCEEDED(res)) {
          mSelectedIndex = j;
          SetContentSelected(j, PR_TRUE);// might want to use ToggleSelection
          mSelectionCache->ReplaceElementAt((void*)PR_TRUE, j);
          if (multiple) {
            mStartExtendedIndex = j;
            if (mEndExtendedIndex == kNothingSelected) {
              mEndExtendedIndex = j;
            }
          }
        }
      } else {
        res = NS_ERROR_UNEXPECTED;
      }
      if (!NS_SUCCEEDED(res)) break;
    }
  }
    
  if (mComboboxFrame != nsnull) {
    if (mSelectedIndex == kNothingSelected) {
      mComboboxFrame->MakeSureSomethingIsSelected(mPresContext);
    } else {
      mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, mSelectedIndex); // don't dispatch event
    }
  }
} 

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetName(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(NS_GET_IID(nsIHTMLContent),(void**)&formControl);
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

  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
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
      value.CompressWhitespace();
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
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);

  }
}


//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::SetComboboxFrame(nsIFrame* aComboboxFrame)
{
  nsresult rv = NS_OK;
  if (nsnull != aComboboxFrame) {
    rv = aComboboxFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame),(void**) &mComboboxFrame); 
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
  aStr.SetLength(0);
  nsresult rv = NS_ERROR_FAILURE; 
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));

  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);

    if (numOptions == 0) {
      rv = NS_OK;
    } else {
      nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = getter_AddRefs(GetOption(*options, mSelectedIndex));
      if (optionElement) {
#if 0 // This is for turning off labels Bug 4050
        nsAutoString text;
        rv = optionElement->GetLabel(text);
        // the return value is always NS_OK from DOMElements
        // it is meaningless to check for it
        if (text.Length() > 0) { 
          nsAutoString compressText = text;
          compressText.CompressWhitespace(PR_TRUE, PR_TRUE);
          if (compressText.Length() != 0) {
            text = compressText;
          }
        }

        if (0 == text.Length()) {
          // the return value is always NS_OK from DOMElements
          // it is meaningless to check for it
          optionElement->GetText(text);
        }          
        aStr = text;
#else
        optionElement->GetText(aStr);
#endif
        rv = NS_OK;
      }
    }
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
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);

    PRUint32 inx;
    for (inx = 0; inx < numOptions && (*aIndex == kNothingSelected); inx++) {
      nsCOMPtr<nsIContent> content = getter_AddRefs(GetOptionAsContent(options, inx));
      if (nsnull != content) {
        if (IsContentSelected(content)) {
          *aIndex = (PRInt32)inx;
        }
      }
    }
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
  if (mContent != nsnull) {
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
    if (nsnull == options) {
      *aNumOptions = 0;
    } else {
      PRUint32 length = 0;
      options->GetLength(&length);
      *aNumOptions = (PRInt32)length;
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
  //PRInt32 numContentItems = 0;
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  if (node) {
    // XXX Need to find a fail proff way to determine that
    // all the frames are there
    mIsAllFramesHere = PR_TRUE;//NS_OK == CountAllChild(node, numContentItems);
  }
  // now make sure we have a frame each piece of content

  return mIsAllFramesHere;
}

//-------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::DoneAddingContent(PRBool aIsDone)
{
  mIsAllContentHere = aIsDone;
  if (mIsAllContentHere) {
    // Here we check to see if all the frames have been created 
    // for all the content.
    // If so, then we can initialize;
    if (mIsAllFramesHere == PR_FALSE) {
      // if all the frames are now present we can initalize
      if (CheckIfAllFramesHere() && mPresContext) {
        mHasBeenInitialized = PR_TRUE;
        Reset(mPresContext);
      }
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::AddOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
#ifdef DO_REFLOW_DEBUG
  printf("---- Id: %d nsLCF %p Added Option %d\n", mReflowId, this, aIndex);
#endif

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

      mSelectionCache->InsertElementAt((void*)selected, aIndex);
      mSelectionCacheLength++;

      if (selected) {
        Reset(aPresContext); // this sets mSelectedIndex to the defaulted selection
        wasReset = PR_TRUE;
      }

#ifdef DEBUG_rods
      {
      nsAutoString text;
      text.AssignWithConversion("No Value");
      nsresult rv = option->GetLabel(text);
      if (NS_CONTENT_ATTR_NOT_THERE == rv || 0 == text.Length()) {
        option->GetText(text);
      }   
      printf("|||||this %p Index: %d  [%s] CB: %p\n", this, aIndex, text.ToNewCString(), mComboboxFrame); //leaks
      }
#endif
    }
  }

  if (!wasReset) {
    GetSelectedIndexFromDOM(&mSelectedIndex); // comes from the DOM
  }

  // if selection changed because of the new option being added 
  // notify the combox if necessary
  if (mComboboxFrame != nsnull) {
    if (mSelectedIndex == kNothingSelected) {
      mComboboxFrame->MakeSureSomethingIsSelected(mPresContext);
    } else if (oldSelection != mSelectedIndex) {
      mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, mSelectedIndex); // don't dispatch event
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------------
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

  mSelectionCache->RemoveElementAt(aIndex);
  mSelectionCacheLength--;

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
    //NS_ASSERTION(0,"nsListControlFrame: Cache sync'd with content!\n");
    changed = PR_TRUE; // Assume the worst, there was a change.
  }

  // Step through content looking for change in selection
  if (NS_SUCCEEDED(rv)) {
    if (!changed) {
      PRBool selected;
      // the content array of options is actually
      // out of sync with the array 
      // so until bug 38825 is fixed.
      if (mSelectionCacheLength != length) { // this shouldn't happend
        for (PRInt32 i = 0; i < length; i++) {
          selected = IsContentSelectedByIndex(i);
          if (selected != (PRBool)mSelectionCache->ElementAt(i)) {
            mSelectionCache->ReplaceElementAt((void*)selected, i);
            changed = PR_TRUE;
          }
        }
      } else {
        mSelectionCache->Clear();
        for (PRInt32 i = 0; i < length; i++) {
          selected = IsContentSelectedByIndex(i);
          mSelectionCache->InsertElementAt((void*)selected, i);
          changed = PR_TRUE;
        }
      }
    }

    PRBool isDroppedDown = PR_FALSE;
    if (mComboboxFrame != nsnull) {
      mComboboxFrame->IsDroppedDown(&isDroppedDown);
    }
    if (changed && aDoDispatchEvent && !isDroppedDown) {
      rv = SelectionChanged(aContent); // Dispatch event
    }
  }

  if ((changed || aForceUpdate) && mComboboxFrame) {
    rv = mComboboxFrame->SelectionChanged(); // Update view
  }
  return rv;
}

NS_IMETHODIMP
nsListControlFrame::GetOptionsContainer(nsIPresContext* aPresContext, nsIFrame** aFrame)
{
  return FirstChild(aPresContext, nsnull, aFrame);
}

NS_IMETHODIMP
nsListControlFrame::OptionDisabled(nsIContent * aContent)
{
  if (IsContentSelected(aContent)) {
    PRInt32 inx = GetSelectedIndexFromContent(aContent);
    SetOptionSelected(inx, PR_FALSE);
  }
  return NS_OK;
}

// Send out an onchange notification.
nsresult
nsListControlFrame::SelectionChanged(nsIContent* aContent)
{
  nsresult ret = NS_ERROR_FAILURE;

  // Dispatch the NS_FORM_CHANGE event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  event.widget = nsnull;
  event.message = NS_FORM_CHANGE;
  event.flags = NS_EVENT_FLAG_NONE;

  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell) {
    ret = presShell->HandleEventWithTarget(&event, this, nsnull, &status);
  }
  return ret;
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
nsListControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName,
                                const nsAReadableString& aValue)
{
  if (nsHTMLAtoms::selected == aName) {
    return NS_ERROR_INVALID_ARG; // Selected is readonly according to spec.

  } else if (nsHTMLAtoms::selectedindex == aName) {
    PRInt32 error = 0;
    nsAutoString str(aValue);
    PRInt32 selectedIndex = str.ToInteger(&error, 10); // Get index from aValue
    if (error) {
      return NS_ERROR_INVALID_ARG; // Couldn't convert to integer
    } else {
      // Start by getting the num of options
      // to make sure the new index is within the bounds
      // if selectedIndex is -1, deselect all (bug 28143)
      PRInt32 numOptions = 0;
      GetNumberOfOptions(&numOptions);
      if (selectedIndex < -1 || selectedIndex >= numOptions) {
        return NS_ERROR_FAILURE;
      }
      if (selectedIndex == -1) {
        Deselect();
      }
      else {
        // Get the DOM interface for the select
        // we will use this to see if it is a "multiple" select 
        nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = getter_AddRefs(GetSelect(mContent));
        if (selectElement) {
          // check to see if it is a mulitple select
          PRBool multiple = PR_FALSE;
          if (NS_FAILED(GetMultiple(&multiple, selectElement))) {
            multiple = PR_FALSE;
          }
          // if it is a multiple, select the new item
          if (multiple) {
            SetOptionSelected(selectedIndex, PR_TRUE);
          } else {
            // if it is a single select, 
            // check to see if it is the currect selection
            // if it is, then do nothing
            if (mSelectedIndex != selectedIndex) {
              ToggleSelected(selectedIndex); // sets mSelectedIndex
              if (nsnull != mComboboxFrame && mIsAllFramesHere) {
                mComboboxFrame->UpdateSelection(PR_FALSE, PR_TRUE, selectedIndex); // don't dispatch event
              }
            }
          }
        }
      }
    }
  }

  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  // Get the selected value of option from local cache (optimization vs. widget)
  if (nsHTMLAtoms::selected == aName) {
    nsAutoString val(aValue);
    PRInt32 error = 0;
    PRBool selected = PR_FALSE;
    PRInt32 indx = val.ToInteger(&error, 10); // Get index from aValue
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
    nsAutoString str; str.AppendInt(selectedIndex, 10);
    aValue.Append(str);
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
    aView->SetFloating(PR_TRUE);
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
    //viewManager->ResizeView(view, mRect.width, mRect.height);
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
    //view->SetVisibility(NS_STYLE_VISIBILITY_VISIBLE == disp->mVisible ?nsViewVisibility_kShow:nsViewVisibility_kHide); 
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
nsListControlFrame::DidReflow(nsIPresContext* aPresContext,
                              nsDidReflowStatus aStatus)
{
  if (PR_TRUE == IsInDropDownMode()) 
  {
    //SyncViewWithFrame();
    mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
    nsresult rv = nsScrollFrame::DidReflow(aPresContext, aStatus);
    mState |= NS_FRAME_SYNC_FRAME_AND_VIEW;
    SyncViewWithFrame(aPresContext);
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

  nsCOMPtr<nsIEventStateManager> stateManager;
  rv = mPresContext->GetEventStateManager(getter_AddRefs(stateManager));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIContent> content;
    rv = stateManager->GetEventTargetContent(nsnull, getter_AddRefs(content));
    if (NS_SUCCEEDED(rv) && content) {
      if (IsOptionElement(content)) {
        aIsDisabled = nsFormFrame::GetDisabled(this, content);
      } else {
        rv = NS_ERROR_FAILURE; // return error when it is not an option
      }
    }
  }
  return rv;
}

//----------------------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::IsOptionDisabled(PRInt32 anIndex, PRBool &aIsDisabled)
{
  PRBool isOptDisabled = PR_FALSE;
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  nsCOMPtr<nsIDOMHTMLOptionElement> optionElement;
  if (options) {
    optionElement = getter_AddRefs(GetOption(*options, anIndex));
    nsCOMPtr<nsIContent> content = do_QueryInterface(optionElement);
    if (content) {
      aIsDisabled = nsFormFrame::GetDisabled(nsnull, content);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
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
// helper
//----------------------------------------------------------------------
PRBool
nsListControlFrame::IsLeftButton(nsIDOMEvent* aMouseEvent)
{
  // only allow selection with the left button
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (mouseEvent) {
    PRUint16 whichButton;
    if (NS_SUCCEEDED(mouseEvent->GetButton(&whichButton))) {
      return whichButton != 1?PR_FALSE:PR_TRUE;
    }
  }
  return PR_FALSE;
}

//----------------------------------------------------------------------
// nsIDOMMouseListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseUp(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");

  REFLOW_DEBUG_MSG("--------------------------- MouseUp ----------------------------\n");

  if (nsFormFrame::GetDisabled(this)) { 
    return NS_OK;
  }

  // only allow selection with the left button
  // if a right button click is on the combobox itself
  // or on the select when in listbox mode, then let the click through
  if (!IsLeftButton(aMouseEvent)) {
    if (IsInDropDownMode()) {
      if (!IsClickingInCombobox(aMouseEvent)) {
        aMouseEvent->PreventDefault();
        aMouseEvent->PreventCapture();
        aMouseEvent->PreventBubble();
      } else {
        mButtonDown = PR_FALSE;
        CaptureMouseEvents(mPresContext, PR_FALSE);
        return NS_OK;
      }
      mButtonDown = PR_FALSE;
      CaptureMouseEvents(mPresContext, PR_FALSE);
      return NS_ERROR_FAILURE; // means consume event
    } else {
      mButtonDown = PR_FALSE;
      CaptureMouseEvents(mPresContext, PR_FALSE);
      return NS_OK;
    }
  }

  // Check to see if the disabled option was clicked on
  // NS_ERROR_FAILURE is returned is it isn't over an option
  PRBool optionIsDisabled;
  if (NS_OK == IsTargetOptionDisabled(optionIsDisabled)) {
    if (optionIsDisabled) {
      if (IsInDropDownMode() == PR_TRUE && mComboboxFrame) {
        ResetSelectedItem();
        mComboboxFrame->ListWasSelected(mPresContext, PR_FALSE); 
      } 
      REFLOW_DEBUG_MSG(">>>>>> Option is disabled");
      mButtonDown = PR_FALSE;
      CaptureMouseEvents(mPresContext, PR_FALSE);
      SetContentSelected(mSelectedIndex, PR_FALSE);
      mSelectedIndex = kNothingSelected;
      return NS_OK;
    }
  }

  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->IsVisible()) {
    REFLOW_DEBUG_MSG(">>>>>> Select is NOT visible");
    return NS_OK;
  }

  if (IsInDropDownMode() == PR_TRUE) {
    // XXX This is a bit of a hack, but.....
    // But the idea here is to make sure you get an "onclick" event when you mouse
    // down on the select and the drag over an option and let go
    // And then NOT get an "onclick" event when when you click down on the select
    // and then up outside of the select
    // the EventStateManager tracks the content of the mouse down and the mouse up
    // to make sure they are the same, and the onclick is sent in the PostHandleEvent
    // depeneding on whether the clickCount is non-zero.
    // So we cheat here by either setting or unsetting the clcikCount in the native event
    // so the right thing happens for the onclick event
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aMouseEvent));
    nsMouseEvent * mouseEvent;
    privateEvent->GetInternalNSEvent((nsEvent**)&mouseEvent);

    if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, mOldSelectedIndex, mSelectedIndex))) {
      REFLOW_DEBUG_MSG2(">>>>>> Found Index: %d", mSelectedIndex);
      if (kNothingSelected != mSelectedIndex) {
        SetContentSelected(mSelectedIndex, PR_TRUE);  
      }
      if (mComboboxFrame) {
        mComboboxFrame->ListWasSelected(mPresContext, PR_FALSE); 
      } 
      mouseEvent->clickCount = 1;
    } else {
      // the click was out side of the select or its dropdown
      mouseEvent->clickCount = IsClickingInCombobox(aMouseEvent)?1:0;
    }
  } else {
    REFLOW_DEBUG_MSG(">>>>>> Didn't find");
    mButtonDown = PR_FALSE;
    CaptureMouseEvents(mPresContext, PR_FALSE);
    if (mSelectedIndex != mOldSelectedIndex) {
      UpdateSelection(PR_TRUE, PR_FALSE, mContent);
    }
#if 0 // XXX - this is a partial fix for Bug 29990
    if (mSelectedIndex != mStartExtendedIndex) {
      mEndExtendedIndex = mSelectedIndex;
    }
#endif
  }

  return NS_OK;
}

//---------------------------------------------------------
// helper method
PRBool nsListControlFrame::IsClickingInCombobox(nsIDOMEvent* aMouseEvent)
{
  // Cheesey way to figure out if we clicking in the ListBox portion
  // or the Combobox portion
  // Return TRUE if we are clicking in the combobox frame
  if (mComboboxFrame) {
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));
    PRInt32 scrX;
    PRInt32 scrY;
    mouseEvent->GetScreenX(&scrX);
    mouseEvent->GetScreenY(&scrY);
    nsRect rect;
    mComboboxFrame->GetAbsoluteRect(&rect);
    if (rect.Contains(scrX, scrY)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
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
  if (IsClickingInCombobox(aMouseEvent)) {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG_rodsX
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));
  nsCOMPtr<nsIDOMNode> node;
  mouseEvent->GetTarget(getter_AddRefs(node));
  nsCOMPtr<nsIContent> content = do_QueryInterface(node);
  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));
  nsIFrame * frame;
  nsresult result = presShell->GetPrimaryFrameFor(content, &frame);
  printf("Target Frame: %p  this: %p\n", frame, this);
  printf("-->\n");
#endif

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIEventStateManager> stateManager;
  if (NS_SUCCEEDED(mPresContext->GetEventStateManager(getter_AddRefs(stateManager)))) {
    nsCOMPtr<nsIContent> content;
    stateManager->GetEventTargetContent(nsnull, getter_AddRefs(content));

    nsCOMPtr<nsIContent> optionContent = getter_AddRefs(GetOptionFromContent(content));
    if (optionContent) {
      aOldIndex = aCurIndex;
      aCurIndex = GetSelectedIndexFromContent(optionContent);
      //printf("--> Old: %d  New: %d\n", aOldIndex, aCurIndex);
      rv = NS_OK;
    }
  }
  //printf("--> bailing\n");
  return rv;
}

void
nsListControlFrame::GetScrollableView(nsIScrollableView*& aScrollableView)
{
  aScrollableView = nsnull;

  nsIView * scrollView;
  GetView(mPresContext, &scrollView);
  nsresult rv = scrollView->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&aScrollableView);
  NS_ASSERTION(NS_SUCCEEDED(rv) && aScrollableView, "We must be able to get a ScrollableView");
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");

  REFLOW_DEBUG_MSG("--------------------------- MouseDown ----------------------------\n");

  if (nsFormFrame::GetDisabled(this)) { 
    return NS_OK;
  }

  // only allow selection with the left button
  // if a right button click is on the combobox itself
  // or on the select when in listbox mode, then let the click through
  if (!IsLeftButton(aMouseEvent)) {
    if (IsInDropDownMode()) {
      if (!IsClickingInCombobox(aMouseEvent)) {
        aMouseEvent->PreventDefault();
        aMouseEvent->PreventCapture();
        aMouseEvent->PreventBubble();
      } else {
        return NS_OK;
      }
      return NS_ERROR_FAILURE; // means consume event
    } else {
      return NS_OK;
    }
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
      nsCOMPtr<nsIEventStateManager> stateManager;
      if (NS_SUCCEEDED(mPresContext->GetEventStateManager(getter_AddRefs(stateManager)))) {
        stateManager->ConsumeFocusEvents(PR_TRUE);
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
      nsCOMPtr<nsIEventStateManager> stateManager;
      if (NS_SUCCEEDED(mPresContext->GetEventStateManager(getter_AddRefs(stateManager)))) {
        nsIFrame * frame;
        stateManager->GetEventTarget(&frame);
        nsCOMPtr<nsIListControlFrame> listFrame(do_QueryInterface(frame));
        if (listFrame) {
          if (!IsClickingInCombobox(aMouseEvent)) {
            return NS_OK;
          }
        } else {
          nsIFrame * parentFrame;
          frame->GetParent(&parentFrame);
          stateManager->GetEventTarget(&frame);
          listFrame = do_QueryInterface(frame);
          if (listFrame) {
            if (!IsClickingInCombobox(aMouseEvent)) {
              return NS_OK;
            }
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
        return NS_OK;
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
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");
  //REFLOW_DEBUG_MSG("MouseMove\n");

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
  } else {// XXX - temporary until we get drag events
    if (mButtonDown) {
      return DragMove(aMouseEvent);
    }
  }
  return NS_OK;
}

nsresult
nsListControlFrame::DragMove(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");
  //REFLOW_DEBUG_MSG("DragMove\n");

  if (!mComboboxFrame) { // Synonym for IsInDropDownMode()
    // check to make sure we are a mulitple select list
    PRBool multipleSelections = PR_FALSE;
    GetMultiple(&multipleSelections);
    if (multipleSelections) {
      // get the currently moused over item
      PRInt32 oldIndex;
      PRInt32 curIndex = mSelectedIndex;
      if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, oldIndex, curIndex))) {
        if (curIndex != oldIndex) {
          // select down the list
          if (curIndex > oldIndex) {
            PRInt32 startInx = oldIndex > mStartExtendedIndex?oldIndex+1:oldIndex;
            PRInt32 endInx   = curIndex > mStartExtendedIndex?curIndex+1:curIndex;
            PRInt32 i;
            for (i=startInx;i<endInx;i++) {
              if (i != mStartExtendedIndex) { // skip the starting clicked on node
                PRBool optionIsDisabled;
                if (NS_OK == IsTargetOptionDisabled(optionIsDisabled)) {
                  if (!optionIsDisabled) {
                    mSelectedIndex = i;
                    SetContentSelected(mSelectedIndex, i > mStartExtendedIndex);
                  }
                }
              }
            }
            mSelectedIndex = curIndex;
          } else {
            // select up the list
            PRInt32 startInx = oldIndex >= mStartExtendedIndex?oldIndex:oldIndex-1;
            PRInt32 endInx   = curIndex >= mStartExtendedIndex?curIndex:curIndex-1;
            PRInt32 i;
            for (i=startInx;i>endInx;i--) {
              if (i != mStartExtendedIndex) { // skip the starting clicked on node
                PRBool optionIsDisabled;
                if (NS_OK == IsTargetOptionDisabled(optionIsDisabled)) {
                  if (!optionIsDisabled) {
                    mSelectedIndex = i;
                    SetContentSelected(mSelectedIndex, i < mStartExtendedIndex);
                  }
                }
              }
            }
            mSelectedIndex = curIndex;
          }
        } else {
          //mOldSelectedIndex = oldIndex;
          mSelectedIndex    = curIndex;
        }
      }
    } else { // Fix Bug 44454
      // get the currently moused over item
      PRInt32 oldIndex;
      PRInt32 curIndex = mSelectedIndex;
      if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, oldIndex, curIndex))) {
        if (curIndex != oldIndex) { // select down the list
          SetContentSelected(mSelectedIndex, PR_FALSE);
          mSelectedIndex = curIndex;
          SetContentSelected(mSelectedIndex, PR_TRUE);
          mStartExtendedIndex = mSelectedIndex;
          mEndExtendedIndex   = kNothingSelected;
        }
      }
    } // Fix Bug 44454
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMKeyListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::ScrollToFrame(nsIContent* aOptElement)
{
  nsIScrollableView * scrollableView;
  GetScrollableView(scrollableView);

  if (scrollableView) {
    // if null is passed in we scroll to 0,0
    if (nsnull == aOptElement) {
      scrollableView->ScrollTo(0, 0, PR_TRUE);
      return NS_OK;
    }
  
    // otherwise we find the content's frame and scroll to it
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    nsIFrame * childframe;
    nsresult result;
    if (aOptElement) {
      result = presShell->GetPrimaryFrameFor(aOptElement, &childframe);
    } else {
      return NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED(result) && childframe) {
      if (NS_SUCCEEDED(result) && scrollableView) {
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

        // This change for 33421 (remove this comment later)

        // options can be a child of an optgroup
        // this checks to see the parent is an optgroup
        // and then adds in the parent's y coord
        // XXX this assume only one level of nesting of optgroups
        //   which is all the spec specifies at the moment.
        nsCOMPtr<nsIContent> parentContent;
        aOptElement->GetParent(*getter_AddRefs(parentContent));
        nsCOMPtr<nsIDOMHTMLOptGroupElement> optGroup(do_QueryInterface(parentContent));
        nsRect optRect(0,0,0,0);
        if (optGroup) {
          nsIFrame * optFrame;
          result = presShell->GetPrimaryFrameFor(parentContent, &optFrame);
          if (NS_SUCCEEDED(result) && optFrame) {
            optFrame->GetRect(optRect);
          }
        }
        fRect.y += optRect.y;

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
  }
  return NS_OK;
}

//---------------------------------------------------------------------
// Ok, the entire idea of this routine is to move to the next item that 
// is suppose to be selected. If the item is disabled then we search in 
// the same direction looking for the next item to select. If we run off 
// the end of the list then we start at the end of the list and search 
// backwards until we get back to the original item or an enabled option
// 
// anNewIndex - will get set to the new index if it finds one
// anOldIndex - gets sets to the old index if a new index is found
// aDoSetNewIndex - indicates that a new item was found and it can be selected
// aWasDisabled - means it found a new item but it was disabled
// aNumOptions - the total number of options in the list
// aDoAdjustInc - the initial increment 1-n
// aDoAdjustIncNext - the increment used to search for the next enabled option
void
nsListControlFrame::AdjustIndexForDisabledOpt(PRInt32 &anNewIndex, PRInt32 &anOldIndex, 
                                              PRBool &aDoSetNewIndex, PRBool &aWasDisabled,
                                              PRInt32 aNumOptions, PRInt32 aDoAdjustInc, 
                                              PRInt32 aDoAdjustIncNext)
{
  // the aDoAdjustInc could be a "1" for a single item or 
  // any number greater representing a page of items
  //
  PRInt32 newIndex    = anNewIndex + aDoAdjustInc;
  PRBool doingReverse = PR_FALSE;   // means we reached the end of the list and now we are searching backwards
  PRInt32 bottom      = 0;          // lowest index in the search range
  PRInt32 top         = aNumOptions;// highest index in the search range

  // make sure we start off in the range
  if (newIndex < bottom) {
    newIndex = 0;
  } else if (newIndex >= top) {
    newIndex = aNumOptions;
  }


  aWasDisabled = PR_FALSE;
  while (1) {
    // Special Debug Code
    //printf("T:%d  B:%d  I:%d  R:%d  IM:%d  I:%d\n", top, bottom, newIndex, aDoAdjustInc, aDoAdjustIncNext, doingReverse);
    //if (newIndex < -30 || newIndex > 30) {
    //  printf("********************************* Stopped!\n");
    //  return;
    //}
    // if the newIndex isn't disabled, we are golden, bail out
    if (NS_OK == IsOptionDisabled(newIndex, aWasDisabled) && !aWasDisabled) {
      break;
    }

    // it WAS disabled, so sart looking ahead for the next enabled option
    newIndex += aDoAdjustIncNext;

    // well, if we reach end reverse the search
    if (newIndex < bottom) {
      if (doingReverse) {
        return; // if we are in reverse mode and reach the end bail out
      } else {
        // reset the newIndex to the end of the list we hit
        // reverse the incrementer
        // set the other end of the list to our original starting index
        newIndex         = bottom;
        aDoAdjustIncNext = -aDoAdjustIncNext;
        doingReverse     = PR_TRUE;
        top              = anNewIndex;
      }
    } else  if (newIndex >= top) {
      if (doingReverse) {
        return;        // if we are in reverse mode and reach the end bail out
      } else {
        // reset the newIndex to the end of the list we hit
        // reverse the incrementer
        // set the other end of the list to our original starting index
        newIndex = top - 1;
        aDoAdjustIncNext = -aDoAdjustIncNext;
        doingReverse     = PR_TRUE;
        bottom           = anNewIndex;
      }
    }
  }

  // Looks like we found one
  anOldIndex     = anNewIndex;
  anNewIndex     = newIndex;
  aDoSetNewIndex = PR_TRUE;
}

nsresult
nsListControlFrame::KeyPress(nsIDOMEvent* aKeyEvent)
{
  NS_ASSERTION(aKeyEvent != nsnull, "keyEvent is null.");

  if (nsFormFrame::GetDisabled(this))
    return NS_OK;

  nsresult rv         = NS_ERROR_FAILURE; 
  PRUint32 code       = 0;
  PRUint32 numOptions = 0;
  PRBool isShift      = PR_FALSE;
  nsCOMPtr<nsIDOMHTMLCollection> options;

  // Start by making sure we can query for a key event
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (keyEvent) {
    //uiEvent->GetCharCode(&code);
    //REFLOW_DEBUG_MSG3("%c %d   ", code, code);
    keyEvent->GetKeyCode(&code);
    if (code == 0) {
      keyEvent->GetCharCode(&code);
    }
#ifdef DO_REFLOW_DEBUG
    if (code >= 32) {
      REFLOW_DEBUG_MSG3("KeyCode: %c %d\n", code, code);
    }
#endif

    keyEvent->GetShiftKey(&isShift);

    // now make sure there are options or we are wasting our time
    options = getter_AddRefs(GetOptions(mContent));

    if (options) {
      options->GetLength(&numOptions);

      if (numOptions == 0) {
        return NS_OK;
      }
    } else{
      return rv;
    }
  } else {
    return rv;
  }
      
  // We are handling this so don't let it bubble up
  aKeyEvent->PreventBubble();

  // this tells us whether we need to process the new index that was set
  // DOM_VK_RETURN & DOM_VK_ESCAPE will leave this false
  PRBool doSetNewIndex = PR_FALSE;

  // set up the old and new selected index and process it
  // DOM_VK_RETURN selects the item
  // DOM_VK_ESCAPE cancels the selection
  // default processing checks to see if the pressed the first 
  //   letter of an item in the list and advances to it

  switch (code) {

    case nsIDOMKeyEvent::DOM_VK_UP:
    case nsIDOMKeyEvent::DOM_VK_LEFT: {
      REFLOW_DEBUG_MSG2("DOM_VK_UP   mSelectedIndex: %d ", mSelectedIndex);
      if (mSelectedIndex > 0) {
        PRBool wasDisabled;
        AdjustIndexForDisabledOpt(mSelectedIndex, mOldSelectedIndex, 
                                  doSetNewIndex, wasDisabled, 
                                  (PRInt32)numOptions, -1, -1);
      }
      REFLOW_DEBUG_MSG2("  After: %d\n", mSelectedIndex);
      } break;
    
    case nsIDOMKeyEvent::DOM_VK_DOWN:
    case nsIDOMKeyEvent::DOM_VK_RIGHT: {
      REFLOW_DEBUG_MSG2("DOM_VK_DOWN mSelectedIndex: %d ", mSelectedIndex);

      if (mSelectedIndex < (PRInt32)numOptions) {
        PRBool wasDisabled;
        AdjustIndexForDisabledOpt(mSelectedIndex, mOldSelectedIndex, 
                                  doSetNewIndex, wasDisabled, 
                                  (PRInt32)numOptions, 1, 1);
      }
      REFLOW_DEBUG_MSG2("  After: %d\n", mSelectedIndex);
      } break;

    case nsIDOMKeyEvent::DOM_VK_RETURN: {
      if (mComboboxFrame != nsnull) {
        PRBool isDroppedDown;
        mComboboxFrame->IsDroppedDown(&isDroppedDown);
        if (IsInDropDownMode() == PR_TRUE && mComboboxFrame) {
          mComboboxFrame->ListWasSelected(mPresContext, isDroppedDown);
        } else {
	        UpdateSelection(PR_TRUE, PR_FALSE, mContent);
	      }
      }
      } break;

    case nsIDOMKeyEvent::DOM_VK_ESCAPE: {
      if (IsInDropDownMode() == PR_TRUE && mComboboxFrame) {
        ResetSelectedItem();
        mComboboxFrame->ListWasSelected(mPresContext, PR_FALSE); 
      } 
      } break;

    case nsIDOMKeyEvent::DOM_VK_PAGE_UP: {
      if (mSelectedIndex > 0) {
        PRBool wasDisabled;
        AdjustIndexForDisabledOpt(mSelectedIndex, mOldSelectedIndex, 
                                  doSetNewIndex, wasDisabled, 
                                  (PRInt32)numOptions, -(mNumDisplayRows-1), -1);
      }
      } break;

    case nsIDOMKeyEvent::DOM_VK_PAGE_DOWN: {
      if (mSelectedIndex < (PRInt32)numOptions) {
        PRBool wasDisabled;
        AdjustIndexForDisabledOpt(mSelectedIndex, mOldSelectedIndex, 
                                  doSetNewIndex, wasDisabled, 
                                  (PRInt32)numOptions, (mNumDisplayRows-1), 1);
      }
      } break;

    case nsIDOMKeyEvent::DOM_VK_HOME: {
      if (mSelectedIndex > 0) {
        mOldSelectedIndex = mSelectedIndex;
        mSelectedIndex    = 0;
        doSetNewIndex     = PR_TRUE;
      }
      } break;

    case nsIDOMKeyEvent::DOM_VK_END: {
      if ((mSelectedIndex+1) < (PRInt32)numOptions) {
        mOldSelectedIndex = mSelectedIndex;
        mSelectedIndex    = (PRInt32)numOptions-1;
        if (mSelectedIndex > (PRInt32)numOptions) {
          mSelectedIndex = (PRInt32)numOptions-1;
        }
        doSetNewIndex = PR_TRUE;
      }
      } break;


    default: { // Select option with this as the first character
               // XXX Not I18N compliant
      PRInt32 selectedIndex = (mSelectedIndex == kNothingSelected ? 0 : mSelectedIndex+1) % numOptions;
      PRInt32 startedAtIndex    = selectedIndex;
      PRBool  loopedAround  = PR_FALSE;
      while ((selectedIndex < startedAtIndex && loopedAround) || !loopedAround) {
        nsCOMPtr<nsIDOMHTMLOptionElement>optionElement = getter_AddRefs(GetOption(*options, selectedIndex));
        if (optionElement) {
          nsAutoString text;
          if (NS_OK == optionElement->GetText(text)) {
            text.ToLowerCase();
            PRUnichar firstChar = text.CharAt(0);
            if (firstChar == (PRUnichar)code) {
              mOldSelectedIndex = mSelectedIndex;
              mSelectedIndex    = selectedIndex;
              SingleSelection();
              if (nsnull != mComboboxFrame && mIsAllFramesHere) {
                mComboboxFrame->UpdateSelection(PR_TRUE, PR_TRUE, mSelectedIndex); // dispatch event
              } else {
                UpdateSelection(PR_TRUE, PR_FALSE, GetOptionContent(mSelectedIndex)); // dispatch event
              }
              break;
            }
          }
        }
        selectedIndex++;
        if (selectedIndex == (PRInt32)numOptions) {
          selectedIndex = 0;
          loopedAround = PR_TRUE;
        }

      } // while
    } break;//case
  } // switch

  // actually process the new index and let the selection code
  // the scrolling for us
  if (doSetNewIndex) {
    PRBool multipleSelections = PR_FALSE;
    GetMultiple(&multipleSelections);
    if (multipleSelections && isShift) {
      REFLOW_DEBUG_MSG2("mStartExtendedIndex: %d\n", mStartExtendedIndex);

      if (mSelectedIndex < mStartExtendedIndex) {
        SetContentSelected(mSelectedIndex, PR_TRUE);
      } else {
        SetContentSelected(mSelectedIndex, PR_TRUE);
        SetContentSelected(mOldSelectedIndex, PR_FALSE);
      }
    } else {
      SingleSelection();
      if (nsnull != mComboboxFrame && mIsAllFramesHere) {
        mComboboxFrame->UpdateSelection(PR_TRUE, PR_TRUE, mSelectedIndex); // dispatch event
      } else {
        UpdateSelection(PR_TRUE, PR_FALSE, GetOptionContent(mSelectedIndex)); // dispatch event
      }
      mStartExtendedIndex = mSelectedIndex;
      mEndExtendedIndex   = kNothingSelected;
    }
    // XXX - Are we cover up a problem here???
    // Why aren't they getting flushed each time?
    // because this isn't needed for Gfx
    if (IsInDropDownMode() == PR_TRUE && mComboboxFrame) {
      nsCOMPtr<nsIPresShell> presShell;
      mPresContext->GetShell(getter_AddRefs(presShell));
      presShell->FlushPendingNotifications();
    }
    REFLOW_DEBUG_MSG2("  After: %d\n", mSelectedIndex);
  } else {
    REFLOW_DEBUG_MSG("  After: SKIPPED it\n");
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

//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SaveStateInternal(nsIPresContext* aPresContext, nsIPresState** aState)
{
  nsCOMPtr<nsISupportsArray> value;
  nsresult res = NS_NewISupportsArray(getter_AddRefs(value));
  if (NS_SUCCEEDED(res) && value) {
    PRInt32 j=0;
    PRInt32 length = 0;
    GetNumberOfOptions(&length);
    PRInt32 i;
    for (i=0; i<length; i++) {
      PRBool selected = PR_FALSE;
      res = GetOptionSelected(i, &selected);
      if (NS_SUCCEEDED(res) && selected) {
        nsCOMPtr<nsISupportsPRInt32> thisVal;
        res = nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_PROGID,
	                       nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(thisVal));
        if (NS_SUCCEEDED(res) && thisVal) {
          res = thisVal->SetData(i);
	        if (NS_SUCCEEDED(res)) {
            PRBool okay = value->InsertElementAt((nsISupports *)thisVal, j++);
	          if (!okay) res = NS_ERROR_OUT_OF_MEMORY; // Most likely cause;
          }
	      }
      }
      if (!NS_SUCCEEDED(res)) break;
    }
  }
  
  NS_NewPresState(aState);
  (*aState)->SetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("selecteditems"), value);
  return res;
}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SaveState(nsIPresContext* aPresContext,
                              nsIPresState** aState)
{
  if (mComboboxFrame == nsnull) {
    return SaveStateInternal(aPresContext, aState);\
  }

  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::RestoreStateInternal(nsIPresContext* aPresContext,
                                         nsIPresState* aState)
{
  mPresState = aState;
  return NS_OK;
}

//-----------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::RestoreState(nsIPresContext* aPresContext,
                                 nsIPresState* aState)
{
  // ignore requests for saving state that are made directly 
  // to the list frame by the system
  // The combobox frame will call RestoreStateInternal 
  // to have its state saved.
  //
  // mComboboxFrame is null when it is a stand-alone listbox
  if (mComboboxFrame == nsnull) {
    return RestoreStateInternal(aPresContext, aState);
  }

  return NS_OK;
}

/*******************************************************************************
 * nsListEventListener
 ******************************************************************************/

nsresult 
NS_NewListEventListener(nsIListEventListener ** aInstancePtr)
{
  nsListEventListener* it = new nsListEventListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIListEventListener), (void **) aInstancePtr);   
}

NS_IMPL_ADDREF(nsListEventListener)

NS_IMPL_RELEASE(nsListEventListener)


nsListEventListener::nsListEventListener()
{
  NS_INIT_REFCNT();
}

nsListEventListener::~nsListEventListener()
{
  // all refcounted objects are held as nsCOMPtrs, clear themselves
}

NS_IMETHODIMP
nsListEventListener::SetFrame(nsListControlFrame *aFrame)
{
  mFrame.SetReference(aFrame->WeakReferent());
  if (aFrame)
  {
    aFrame->GetContent(getter_AddRefs(mContent));
  }
  return NS_OK;
}

nsresult
nsListEventListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    nsIDOMKeyListener *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIDOMEventListener))) {
    nsIDOMKeyListener *kl = (nsIDOMKeyListener*)this;
    nsIDOMEventListener *temp = kl;
    *aInstancePtr = (void*)temp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  if (aIID.Equals(NS_GET_IID(nsIDOMMouseMotionListener))) {                                         
    *aInstancePtr = (void*)(nsIDOMMouseMotionListener*) this;                                        
    NS_ADDREF_THIS();
    return NS_OK;                                                        
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMKeyListener))) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMMouseListener))) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIListEventListener))) {
    *aInstancePtr = (void*)(nsIListEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsresult
nsListEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

/*================== nsIKeyListener =========================*/

nsresult
nsListEventListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->KeyDown(aKeyEvent);
  }
  return NS_OK;
}

nsresult
nsListEventListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->KeyUp(aKeyEvent);
  }
  return NS_OK;
}

nsresult
nsListEventListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->KeyPress(aKeyEvent);
  }
  return NS_OK;
}

/*=============== nsIMouseListener ======================*/

nsresult
nsListEventListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->MouseDown(aMouseEvent);
  }
  return NS_OK;
}

nsresult
nsListEventListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->MouseUp(aMouseEvent);
  }
  return NS_OK;
}

nsresult
nsListEventListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->MouseClick(aMouseEvent);
  }
  return NS_OK;
}

nsresult
nsListEventListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->MouseDblClick(aMouseEvent);
  }
  return NS_OK;
}

nsresult
nsListEventListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->MouseOver(aMouseEvent);
  }
  return NS_OK;
}

nsresult
nsListEventListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->MouseOut(aMouseEvent);
  }
  return NS_OK;
}

/*=============== nsIDOMMouseMotionListener ======================*/

nsresult
nsListEventListener::MouseMove(nsIDOMEvent* aMouseEvent)
{
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->MouseMove(aMouseEvent);
  }
  return NS_OK;
}

nsresult
nsListEventListener::DragMove(nsIDOMEvent* aMouseEvent)
{
  /*
  nsListControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    return gfxFrame->DragMove(aMouseEvent);
  }
  */
  return NS_OK;
}


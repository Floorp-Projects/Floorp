/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsListControlFrame.h"
#include "nsFormControlFrame.h" // for COMPARE macro
#include "nsFormControlHelper.h"
#include "nsHTMLAtoms.h"
#include "nsIFormControl.h"
#include "nsIDeviceContext.h" 
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h" 
#include "nsIDOMHTMLOptionsCollection.h" 
#include "nsIDOMNSHTMLOptionCollectn.h"
#include "nsIDOMHTMLSelectElement.h" 
#include "nsIDOMNSHTMLSelectElement.h" 
#include "nsIDOMHTMLOptionElement.h" 
#include "nsIComboboxControlFrame.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsWidgetsCID.h"
#include "nsHTMLReflowCommand.h"
#include "nsIPresShell.h"
#include "nsHTMLParts.h"
#include "nsIDOMEventReceiver.h"
#include "nsIEventStateManager.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsILookAndFeel.h"
#include "nsLayoutAtoms.h"
#include "nsIFontMetrics.h"
#include "nsVoidArray.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSEvent.h"
#include "nsGUIEvent.h"
#include "nsIServiceManager.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsISelectElement.h"
#include "nsIPrivateDOMEvent.h"
#include "nsCSSRendering.h"
#include "nsReflowPath.h"
#include "nsITheme.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMKeyListener.h"

// Timer Includes
#include "nsITimer.h"

// Constants
const nscoord kMaxDropDownRows          = 20; // This matches the setting for 4.x browsers
const PRInt32 kDefaultMultiselectHeight = 4; // This is compatible with 4.x browsers
const PRInt32 kNothingSelected          = -1;
const PRInt32 kMaxZ                     = 0x7fffffff; //XXX: Shouldn't there be a define somewhere for MaxInt for PRInt32
const PRInt32 kNoSizeSpecified          = -1;


nsListControlFrame * nsListControlFrame::mFocused = nsnull;

// Using for incremental typing navigation
#define INCREMENTAL_SEARCH_KEYPRESS_TIME 1000
// XXX, kyle.yuan@sun.com, there are 4 definitions for the same purpose:
//  nsMenuPopupFrame.h, nsListControlFrame.cpp, listbox.xml, tree.xml
//  need to find a good place to put them together.
//  if someone changes one, please also change the other.

DOMTimeStamp nsListControlFrame::gLastKeyTime = 0;

/******************************************************************************
 * nsListEventListener
 * This class is responsible for propagating events to the nsListControlFrame.
 * Frames are not refcounted so they can't be used as event listeners.
 *****************************************************************************/

class nsListEventListener : public nsIDOMKeyListener,
                            public nsIDOMMouseListener,
                            public nsIDOMMouseMotionListener
{
public:
  nsListEventListener(nsListControlFrame *aFrame)
    : mFrame(aFrame) { }

  void SetFrame(nsListControlFrame *aFrame) { mFrame = aFrame; }

  NS_DECL_ISUPPORTS

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMKeyListener
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);

  // nsIDOMMouseListener
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);

  // nsIDOMMouseMotionListener
  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent);

private:
  nsListControlFrame  *mFrame;
};

//---------------------------------------------------------
nsresult
NS_NewListControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCOMPtr<nsIDocument> doc;
  aPresShell->GetDocument(getter_AddRefs(doc));
  nsListControlFrame* it = new (aPresShell) nsListControlFrame(aPresShell, doc);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
#if 0
  // set the state flags (if any are provided)
  it->AddStateBits(NS_BLOCK_SPACE_MGR);
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

//---------------------------------------------------
//-- Update Timer Stuff
//---------------------------------------------------
class nsSelectUpdateTimer : public nsITimerCallback
{
public:

  NS_DECL_ISUPPORTS

  nsSelectUpdateTimer()
      : mPresContext(nsnull), mDelay(0), mHasBeenNotified(PR_FALSE), 
        mItemsAdded(PR_FALSE), mItemsRemoved(PR_FALSE), mItemsInxSet(PR_FALSE)
  {
  }

  virtual ~nsSelectUpdateTimer();
  NS_DECL_NSITIMERCALLBACK

  // Additional Methods
  nsresult Start(nsIPresContext *aPresContext) 
  {
    mPresContext = aPresContext;

    nsresult result = NS_OK;
    if (!mTimer) {
      mTimer = do_CreateInstance("@mozilla.org/timer;1", &result);

      if (NS_FAILED(result))
        return result;
    }
    result = mTimer->InitWithCallback(this, mDelay, nsITimer::TYPE_ONE_SHOT);

    if (mHasBeenNotified) {
      mItemsAdded      = PR_FALSE;
      mItemsRemoved    = PR_FALSE;
      mItemsInxSet     = PR_FALSE;
      mHasBeenNotified = PR_FALSE;
      mInxArray.Clear();
    }

    return result;
  }

  void Init(nsListControlFrame *aList, PRUint32 aDelay)  { mListControl = aList; mDelay = aDelay; }
  void Stop() { if (mTimer) mTimer->Cancel(); }

  void AdjustIndexes(PRBool aInserted, PRInt32 aInx)
  {
    // remove the index from the list
    if (!aInserted) {
      PRInt32 inx = (PRInt32)mInxArray.IndexOf((void*)aInx);
      if (inx > -1) {
        mInxArray.RemoveElementAt(inx);
      }
    }

    PRInt32 count = mInxArray.Count();
    for (PRInt32 i=0;i<count;i++) {
      PRInt32 inx = NS_PTR_TO_INT32(mInxArray[i]);
      if (inx > aInx) {
        mInxArray.ReplaceElementAt((void*)(inx+(aInserted?1:-1)), i);
      }
    }
  }

  void ItemAdded(PRInt32 aInx, PRInt32 aNumItems)
  { 
    mItemsAdded = PR_TRUE;
    if (mInxArray.Count() > 0 && aInx <= aNumItems-1) {
      AdjustIndexes(PR_TRUE, aInx);
    }
  }

  void ItemRemoved(PRInt32 aInx, PRInt32 aNumItems) 
  { 
    mItemsRemoved = PR_TRUE;
    if (mInxArray.Count() > 0 && aInx <= aNumItems) {
      AdjustIndexes(PR_FALSE, aInx);
    }
  }

  void ItemIndexSet(PRInt32 aInx)  
  { 
    mItemsInxSet = PR_TRUE;
    mInxArray.AppendElement((void*)aInx);
  }

  PRPackedBool HasBeenNotified()      { return mHasBeenNotified; }

private:
  nsListControlFrame * mListControl;
  nsCOMPtr<nsITimer>   mTimer;
  nsIPresContext*      mPresContext;
  PRUint32             mDelay;
  PRPackedBool         mHasBeenNotified;

  PRPackedBool         mItemsAdded;
  PRPackedBool         mItemsRemoved;
  PRPackedBool         mItemsInxSet;
  nsVoidArray          mInxArray;

};

NS_IMPL_ADDREF(nsSelectUpdateTimer)
NS_IMPL_RELEASE(nsSelectUpdateTimer)
NS_IMPL_QUERY_INTERFACE1(nsSelectUpdateTimer, nsITimerCallback)

nsresult NS_NewUpdateTimer(nsSelectUpdateTimer **aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = (nsSelectUpdateTimer*) new nsSelectUpdateTimer;

  if (!aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);

  return NS_OK;
}

// nsITimerCallback
NS_IMETHODIMP nsSelectUpdateTimer::Notify(nsITimer *timer)
{
  if (mPresContext && mListControl && !mHasBeenNotified) {
    mHasBeenNotified = PR_TRUE;
    if (mItemsAdded || mItemsInxSet) {
      mListControl->ResetList(mPresContext, &mInxArray);
    } else {
      mListControl->ItemsHaveBeenRemoved(mPresContext);
    }
  }
  return NS_OK;
}

nsSelectUpdateTimer::~nsSelectUpdateTimer()
{
  if (mTimer) {
    mTimer->Cancel();
  }
}

//---------------------------------------------------
//-- DONE: Update Timer Stuff
//---------------------------------------------------


//---------------------------------------------------------
nsListControlFrame::nsListControlFrame(nsIPresShell* aShell,
  nsIDocument* aDocument)
  : nsGfxScrollFrame(aShell, PR_FALSE)
{
  mComboboxFrame      = nsnull;
  mButtonDown         = PR_FALSE;
  mMaxWidth           = 0;
  mMaxHeight          = 0;
  mPresContext        = nsnull;

  mIsAllContentHere   = PR_FALSE;
  mIsAllFramesHere    = PR_FALSE;
  mHasBeenInitialized = PR_FALSE;
  mDoneWithInitialReflow = PR_FALSE;

  mCacheSize.width             = -1;
  mCacheSize.height            = -1;
  mCachedMaxElementWidth       = -1;
  mCachedAvailableSize.width   = -1;
  mCachedAvailableSize.height  = -1;
  mCachedUnconstrainedSize.width  = -1;
  mCachedUnconstrainedSize.height = -1;
  
  mOverrideReflowOpt           = PR_FALSE;
  mPassId                      = 0;

  mUpdateTimer                 = nsnull;
  mDummyFrame                  = nsnull;

  REFLOW_COUNTER_INIT()
}

//---------------------------------------------------------
nsListControlFrame::~nsListControlFrame()
{
  REFLOW_COUNTER_DUMP("nsLCF");
  if (mUpdateTimer != nsnull) {
    StopUpdateTimer();
    NS_RELEASE(mUpdateTimer);
  }

  mComboboxFrame = nsnull;
  NS_IF_RELEASE(mPresContext);
}

// for Bug 47302 (remove this comment later)
NS_IMETHODIMP
nsListControlFrame::Destroy(nsIPresContext *aPresContext)
{
  // get the receiver interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mContent));

  // Clear the frame pointer on our event listener, just in case the
  // event listener can outlive the frame.

  mEventListener->SetFrame(nsnull);

  receiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*,
                                                    mEventListener),
                                     NS_GET_IID(nsIDOMMouseListener));

  receiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseMotionListener*,
                                                    mEventListener),
                                     NS_GET_IID(nsIDOMMouseMotionListener));

  receiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMKeyListener*,
                                                    mEventListener),
                                     NS_GET_IID(nsIDOMKeyListener));

  if (IsInDropDownMode() == PR_FALSE) {
    nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  }
  return nsGfxScrollFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsListControlFrame::Paint(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  if (!GetStyleVisibility()->IsVisible()) {
    return PR_FALSE;
  }

  // Don't allow painting of list controls when painting is suppressed.
  PRBool paintingSuppressed = PR_FALSE;
  aPresContext->PresShell()->IsPaintingSuppressed(&paintingSuppressed);
  if (paintingSuppressed)
    return NS_OK;

  // Start by assuming we are visible and need to be painted
  PRBool isVisible = PR_TRUE;

  if (aPresContext->IsPaginated()) {
    if (aPresContext->IsRenderingOnlySelection()) {
      // Check the quick way first
      PRBool isSelected = (mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
      // if we aren't selected in the mState we could be a container
      // so check to see if we are in the selection range
      if (!isSelected) {
        nsCOMPtr<nsISelectionController> selcon;
        selcon = do_QueryInterface(aPresContext->PresShell());
        if (selcon) {
          nsCOMPtr<nsISelection> selection;
          selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
          nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
          selection->ContainsNode(node, PR_TRUE, &isVisible);
        } else {
          isVisible = PR_FALSE;
        }
      }
    }
  } 

  if (isVisible) {
    if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND) {
      const nsStyleDisplay* displayData = GetStyleDisplay();
      if (displayData->mAppearance) {
        nsCOMPtr<nsITheme> theme;
        aPresContext->GetTheme(getter_AddRefs(theme));
        nsRect  rect(0, 0, mRect.width, mRect.height);
        if (theme && theme->ThemeSupportsWidget(aPresContext, this, displayData->mAppearance))
          theme->DrawWidgetBackground(&aRenderingContext, this, 
                                      displayData->mAppearance, rect, aDirtyRect); 
      }
    }

    return nsGfxScrollFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  }

  DO_GLOBAL_REFLOW_COUNT_DSP("nsListControlFrame", &aRenderingContext);
  return NS_OK;

}

/* Note: this is called by the SelectsAreaFrame, which is the same
   as the frame returned by GetOptionsContainer. It's the frame which is
   scrolled by us. */
void nsListControlFrame::PaintFocus(nsIRenderingContext& aRC, nsFramePaintLayer aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) return;

  if (mFocused != this) return;

  // The mEndSelectionIndex is what is currently being selected
  // use the selected index if this is kNothingSelected
  PRInt32 focusedIndex;
  if (mEndSelectionIndex == kNothingSelected) {
    GetSelectedIndex(&focusedIndex);
  } else {
    focusedIndex = mEndSelectionIndex;
  }

  nsIScrollableView * scrollableView;
  GetScrollableView(mPresContext, &scrollableView);
  if (!scrollableView) return;

  nsIPresShell *presShell = mPresContext->GetPresShell();
  if (!presShell) return;

  nsIFrame* containerFrame;
  GetOptionsContainer(mPresContext, &containerFrame);
  if (!containerFrame) return;

  nsIFrame * childframe = nsnull;
  nsresult result = NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> focusedContent;

  nsCOMPtr<nsIDOMNSHTMLSelectElement> selectNSElement(do_QueryInterface(mContent));
  NS_ASSERTION(selectNSElement, "Can't be null");

  nsCOMPtr<nsISelectElement> selectElement(do_QueryInterface(mContent));
  NS_ASSERTION(selectElement, "Can't be null");

  // If we have a selected index then get that child frame
  if (focusedIndex != kNothingSelected) {
    focusedContent = getter_AddRefs(GetOptionContent(focusedIndex));
    // otherwise we find the focusedContent's frame and scroll to it
    if (focusedContent) {
      result = presShell->GetPrimaryFrameFor(focusedContent, &childframe);
    }
  } else {
    nsCOMPtr<nsIDOMHTMLSelectElement> selectHTMLElement(do_QueryInterface(mContent));
    NS_ASSERTION(selectElement, "Can't be null");

    // Since there isn't a selected item we need to show a focus ring around the first
    // non-disabled item and skip all the option group elements (nodes)
    nsCOMPtr<nsIDOMNode> node;

    PRUint32 length;
    selectHTMLElement->GetLength(&length);
    if (length) {
      // find the first non-disabled item
      PRBool isDisabled = PR_TRUE;
      for (PRInt32 i=0;i<PRInt32(length) && isDisabled;i++) {
        if (NS_FAILED(selectNSElement->Item(i, getter_AddRefs(node))) || !node) {
          break;
        }
        if (NS_FAILED(selectElement->IsOptionDisabled(i, &isDisabled))) {
          break;
        }
        if (isDisabled) {
          node = nsnull;
        } else {
          break;
        }
      }
      if (!node) {
        return;
      }
    }

    // if we found a node use, if not get the first child (this is for empty selects)
    if (node) {
      focusedContent = do_QueryInterface(node);
      result = presShell->GetPrimaryFrameFor(focusedContent, &childframe);
    }
    if (!childframe) {
      // The only way we can get right here is that there are no options
      // and we need to get the dummy frame so it has the focus ring
      childframe = containerFrame->GetFirstChild(nsnull);
      result = NS_OK;
    }
  }

  if (NS_FAILED(result) || !childframe) return;

  // get the child rect
  nsRect fRect = childframe->GetRect();
  
  // get it into the coordinates of containerFrame
  for (nsIFrame* ancestor = childframe->GetParent();
       ancestor && ancestor != containerFrame;
       ancestor = ancestor->GetParent()) {
    fRect += ancestor->GetPosition();
  }

  PRBool lastItemIsSelected = PR_FALSE;
  if (focusedIndex != kNothingSelected) {
    nsCOMPtr<nsIDOMNode> node;
    if (NS_SUCCEEDED(selectNSElement->Item(focusedIndex, getter_AddRefs(node)))) {
      nsCOMPtr<nsIDOMHTMLOptionElement> domOpt(do_QueryInterface(node));
      NS_ASSERTION(domOpt, "Something has gone seriously awry.  This should be an option element!");
      domOpt->GetSelected(&lastItemIsSelected);
    }
  }

  // set up back stop colors and then ask L&F service for the real colors
  nscolor color;
  mPresContext->LookAndFeel()->
    GetColor(lastItemIsSelected ?
             nsILookAndFeel::eColor_WidgetSelectForeground :
             nsILookAndFeel::eColor_WidgetSelectBackground, color);

  float p2t;
  mPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixelInTwips = NSToCoordRound(p2t);

  nsRect dirty;
  nscolor colors[] = {color, color, color, color};
  PRUint8 borderStyle[] = {NS_STYLE_BORDER_STYLE_DOTTED, NS_STYLE_BORDER_STYLE_DOTTED, NS_STYLE_BORDER_STYLE_DOTTED, NS_STYLE_BORDER_STYLE_DOTTED};
  nsRect innerRect = fRect;
  innerRect.Deflate(nsSize(onePixelInTwips, onePixelInTwips));
  nsCSSRendering::DrawDashedSides(0, aRC, dirty, borderStyle, colors, fRect, innerRect, 0, nsnull);

}

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
  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr =
      NS_STATIC_CAST(void*,NS_STATIC_CAST(nsIStatefulFrame*,this));
    return NS_OK;
  }
  return nsGfxScrollFrame::QueryInterface(aIID, aInstancePtr);
}

//----------------------------------------------------------------------
// nsIStatefulFrame
// These methods were originally in the nsScrollFrame superclass,
// but were moved here when nsListControlFrame switched to use
// nsGfxScrollFrame.
//----------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SaveState(nsIPresContext* aPresContext,
                              nsIPresState** aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  nsCOMPtr<nsIPresState> state;
  nsresult res = NS_OK;

  nsIScrollableView* scrollingView;
  GetScrollableView(aPresContext, &scrollingView);

  nscoord x = 0, y = 0;
  if (scrollingView) {
    scrollingView->GetScrollPosition(x, y);
  }

  // Don't save scroll position if we are at (0,0)
  if (x || y) {
    nsIView* child = nsnull;
    scrollingView->GetScrolledView(child);
    NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);

    nsRect childRect = child->GetBounds();

    res = NS_NewPresState(getter_AddRefs(state));
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsISupportsPRInt32> xoffset =
      do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID, &res);
    if (xoffset) {
      res = xoffset->SetData(x);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_LITERAL_STRING("x-offset"), xoffset);
    }

    nsCOMPtr<nsISupportsPRInt32> yoffset =
      do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID, &res);
    if (yoffset) {
      res = yoffset->SetData(y);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_LITERAL_STRING("y-offset"), yoffset);
    }

    nsCOMPtr<nsISupportsPRInt32> width =
      do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID, &res);
    if (width) {
      res = width->SetData(childRect.width);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_LITERAL_STRING("width"), width);
    }

    nsCOMPtr<nsISupportsPRInt32> height =
      do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID, &res);
    if (height) {
      res = height->SetData(childRect.height);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_LITERAL_STRING("height"), height);
    }
    *aState = state;
    NS_ADDREF(*aState);
  }
  return res;
}

//-----------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::RestoreState(nsIPresContext* aPresContext,
                                 nsIPresState* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsISupportsPRInt32> xoffset;
  nsCOMPtr<nsISupportsPRInt32> yoffset;
  nsCOMPtr<nsISupportsPRInt32> width;
  nsCOMPtr<nsISupportsPRInt32> height;
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("x-offset"), getter_AddRefs(xoffset));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("y-offset"), getter_AddRefs(yoffset));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("width"), getter_AddRefs(width));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("height"), getter_AddRefs(height));

  nsresult res = NS_ERROR_NULL_POINTER;
  if (xoffset && yoffset) {
    PRInt32 x,y,w,h;
    res = xoffset->GetData(&x);
    if (NS_SUCCEEDED(res))
      res = yoffset->GetData(&y);
    if (NS_SUCCEEDED(res))
      res = width->GetData(&w);
    if (NS_SUCCEEDED(res))
      res = height->GetData(&h);

    if (NS_SUCCEEDED(res)) {
      nsIScrollableView* scrollingView;
      GetScrollableView(aPresContext, &scrollingView);
      if (scrollingView) {
        nsIView* child = nsnull;
        nsRect childRect(0,0,0,0);
        if (NS_SUCCEEDED(scrollingView->GetScrolledView(child)) && child) {
          childRect = child->GetBounds();
        }
        x = (int)(((float)childRect.width / w) * x);
        y = (int)(((float)childRect.height / h) * y);

        scrollingView->ScrollTo(x,y,0);
      }
    }
  }

  return res;
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsListControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mContent);
    return accService->CreateHTMLListboxAccessible(node, mPresContext, aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

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

static nscoord
GetMaxOptionHeight(nsIPresContext *aPresContext, nsIFrame *aContainer)
{
  nscoord result = 0;
  for (nsIFrame* option = aContainer->GetFirstChild(nsnull);
       option; option = option->GetNextSibling()) {
    nscoord optionHeight;
    if (nsCOMPtr<nsIDOMHTMLOptGroupElement>
        (do_QueryInterface(option->GetContent()))) {
      // an optgroup
      optionHeight = GetMaxOptionHeight(aPresContext, option);
    } else {
      // an option
      optionHeight = option->GetSize().height;
    }
    if (result < optionHeight)
      result = optionHeight;
  }
  return result;
}

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
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
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
      const nsStyleDisplay* display = GetStyleDisplay();
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
                                       mCachedAscent,
                                       mCachedMaxElementWidth, 
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
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options =
    getter_AddRefs(GetOptions(mContent));
  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    printf("--- Num of Items %d ---\n", numOptions);
    for (PRUint32 i=0;i<numOptions;i++) {
      nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = getter_AddRefs(GetOption(options, i));
      if (optionElement) {
        nsAutoString text;
        rv = optionElement->GetLabel(text);
        if (NS_CONTENT_ATTR_HAS_VALUE != rv || text.IsEmpty()) {
          if (NS_OK != optionElement->GetText(text)) {
            text = "No Value";
          }
        } else {
            text = "No Value";
        }          
        printf("[%d] - %s\n", i, NS_LossyConvertUCS2toASCII(text).get());
      }
    }
  }
  }
#endif // DEBUG_rodsXXX


  // If all the content and frames are here 
  // then initialize it before reflow
    if (mIsAllContentHere && !mHasBeenInitialized) {
      if (PR_FALSE == mIsAllFramesHere) {
        CheckIfAllFramesHere();
      }
      if (mIsAllFramesHere && !mHasBeenInitialized) {
        mHasBeenInitialized = PR_TRUE;
      }
    }


    if (eReflowReason_Incremental == aReflowState.reason) {
      nsHTMLReflowCommand *command = aReflowState.path->mReflowCommand;
      if (command) {
        // XXX So this may do it too often
        // the side effect of this is if the user has scrolled to some other place in the list and
        // an incremental reflow comes through the list gets scrolled to the first selected item
        // I haven't been able to make it do it, but it will do it
        // basically the real solution is to know when all the reframes are there.
        PRInt32 selectedIndex = mEndSelectionIndex;
        if (selectedIndex == kNothingSelected) {
          GetSelectedIndex(&selectedIndex);
        }
        ScrollToIndex(selectedIndex);
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
    if (!IsInDropDownMode()) {
      nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
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
 
  nsHTMLReflowMetrics  scrolledAreaDesiredSize(PR_TRUE);


  if (eReflowReason_Incremental == aReflowState.reason) {
    nsHTMLReflowCommand *command = firstPassState.path->mReflowCommand;
    if (command) {
      nsReflowType type;
      command->GetType(type);
      firstPassState.reason = eReflowReason_StyleChange;
      firstPassState.path = nsnull;
    } else {
      nsresult res = nsGfxScrollFrame::Reflow(aPresContext, 
                                              scrolledAreaDesiredSize,
                                              aReflowState, 
                                              aStatus);
      if (NS_FAILED(res)) {
        NS_ASSERTION(aDesiredSize.width < 100000, "Width is still NS_UNCONSTRAINEDSIZE");
        NS_ASSERTION(aDesiredSize.height < 100000, "Height is still NS_UNCONSTRAINEDSIZE");
        return res;
      }

      firstPassState.reason = eReflowReason_StyleChange;
      firstPassState.path = nsnull;
    }
  }

  if (mPassId == 0 || mPassId == 1) {
    nsresult res = nsGfxScrollFrame::Reflow(aPresContext, 
                                            scrolledAreaDesiredSize,
                                            firstPassState, 
                                            aStatus);
    if (NS_FAILED(res)) {
      NS_ASSERTION(aDesiredSize.width < 100000, "Width is still NS_UNCONSTRAINEDSIZE");
      NS_ASSERTION(aDesiredSize.height < 100000, "Height is still NS_UNCONSTRAINEDSIZE");
      return res;
    }
    mCachedUnconstrainedSize.width  = scrolledAreaDesiredSize.width;
    mCachedUnconstrainedSize.height = scrolledAreaDesiredSize.height;
    mCachedAscent                   = scrolledAreaDesiredSize.ascent;
    mCachedDesiredMEW               = scrolledAreaDesiredSize.mMaxElementWidth;
  } else {
    scrolledAreaDesiredSize.width  = mCachedUnconstrainedSize.width;
    scrolledAreaDesiredSize.height = mCachedUnconstrainedSize.height;
    scrolledAreaDesiredSize.mMaxElementWidth = mCachedDesiredMEW;
  }

  // Compute the bounding box of the contents of the list using the area 
  // calculated by the first reflow as a starting point.
  //
  // The nsGfxScrollFrame::Reflow adds border and padding to the
  // maxElementWidth, so these need to be subtracted
  nscoord scrolledAreaWidth  = scrolledAreaDesiredSize.width -
    (aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right);
  nscoord scrolledAreaHeight = scrolledAreaDesiredSize.height -
    (aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom);

  // Set up max values
  mMaxWidth  = scrolledAreaWidth;

  // Now the scrolledAreaWidth and scrolledAreaHeight are exactly 
  // wide and high enough to enclose their contents
  PRBool isInDropDownMode = IsInDropDownMode();

  nscoord visibleWidth = 0;
  if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
    visibleWidth = scrolledAreaWidth;
  } else {
    visibleWidth = aReflowState.mComputedWidth;
  }

   // Determine if a scrollbar will be needed, If so we need to add
   // enough the width to allow for the scrollbar.
   // The scrollbar will be needed under two conditions:
   // (size * heightOfaRow) < scrolledAreaHeight or
   // the height set through Style < scrolledAreaHeight.

   // Calculate the height of a single row in the listbox or dropdown
   // list by using the tallest of the grandchildren, since there may be
   // option groups in addition to option elements, either of which may
   // be visible or invisible.
  nsIFrame *optionsContainer;
  GetOptionsContainer(aPresContext, &optionsContainer);
  PRInt32 heightOfARow = GetMaxOptionHeight(aPresContext, optionsContainer);

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
      nsresult result = mPresContext->PresShell()->
        GetPrimaryFrameFor(option, &optFrame);
      if (NS_SUCCEEDED(result) && optFrame != nsnull) {
        nsStyleContext* optStyle = optFrame->GetStyleContext();
        if (optStyle) {
          const nsStyleFont* styleFont = optStyle->GetStyleFont();
          nsIFontMetrics * fontMet;
          result = aPresContext->DeviceContext()->
            GetMetricsFor(styleFont->mFont, fontMet);
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
  mMaxHeight = heightOfARow;

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
          p2t = aPresContext->PixelsToTwips();
          nscoord screenHeight = NSIntPixelsToTwips(screenHeightInPixels, p2t);

          nscoord availDropHgt = (screenHeight / 2) - (heightOfARow*2); // approx half screen minus combo size
          availDropHgt -= aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;

          nscoord hgt = visibleHeight + aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
          if (heightOfARow > 0) {
            if (hgt > availDropHgt) {
              visibleHeight = (availDropHgt / heightOfARow) * heightOfARow;
            }
            mNumDisplayRows = visibleHeight / heightOfARow;
          } else {
            // Hmmm, not sure what to do here. Punt, and make both of them one
            visibleHeight   = 1;
            mNumDisplayRows = 1;
          }
        }
      }
    }
   
  } else {
      // Calculate the visible height of the listbox
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
      visibleHeight = aReflowState.mComputedHeight;
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
    if (aReflowState.mComputedWidth != 0) {
      scrolledAreaHeight = visibleHeight+1;
    }
  }

  // The visible height is zero, this could be a select with no options
  // or a select with a single option that has no content or no label
  //
  // So this may not be the best solution, but we get the height of the font
  // for the list frame and use that as the max/minimum size for the contents
  if (visibleHeight == 0) {
    if (aReflowState.mComputedHeight != 0) {
      nsCOMPtr<nsIFontMetrics> fontMet;
      nsresult rvv = nsFormControlHelper::GetFrameFontFM(aPresContext, this, getter_AddRefs(fontMet));
      if (NS_SUCCEEDED(rvv) && fontMet) {
        aReflowState.rendContext->SetFont(fontMet);
        fontMet->GetHeight(visibleHeight);
        mMaxHeight = visibleHeight;
      }
    }
  }

  // When in dropdown mode make sure we obey min/max-width and min/max-height
  if (!isInDropDownMode) {
    nscoord fullWidth = visibleWidth + aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
    if (fullWidth > aReflowState.mComputedMaxWidth) {
      visibleWidth = aReflowState.mComputedMaxWidth - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right;
    }
    if (fullWidth < aReflowState.mComputedMinWidth) {
      visibleWidth = aReflowState.mComputedMinWidth - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right;
    }

    // calculate full height for comparison
    // must add in Border & Padding twice because the scrolled area also inherits Border & Padding
    nscoord fullHeight = 0;
    if (aReflowState.mComputedHeight != 0) {
      fullHeight = visibleHeight + aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
                                         // + aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
    }
    if (fullHeight > aReflowState.mComputedMaxHeight) {
      visibleHeight = aReflowState.mComputedMaxHeight - aReflowState.mComputedBorderPadding.top - aReflowState.mComputedBorderPadding.bottom;
    }
    if (fullHeight < aReflowState.mComputedMinHeight) {
      visibleHeight = aReflowState.mComputedMinHeight - aReflowState.mComputedBorderPadding.top - aReflowState.mComputedBorderPadding.bottom;
    }
  }

   // Do a second reflow with the adjusted width and height settings
   // This sets up all of the frames with the correct width and height.
  secondPassState.mComputedWidth  = visibleWidth;
  secondPassState.mComputedHeight = visibleHeight;
  secondPassState.reason = eReflowReason_Resize;

  if (mPassId == 0 || mPassId == 2 || visibleHeight != scrolledAreaHeight ||
      visibleWidth != scrolledAreaWidth) {
    nsGfxScrollFrame::Reflow(aPresContext, aDesiredSize, secondPassState, aStatus);
    if (aReflowState.mComputedHeight == 0) {
      aDesiredSize.ascent  = 0;
      aDesiredSize.descent = 0;
      aDesiredSize.height  = 0;
    }

    // Set the max element size to be the same as the desired element size.
  } else {
    // aDesiredSize is the desired frame size, so includes border and padding
    aDesiredSize.width  = visibleWidth +
      (aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right);
    aDesiredSize.height = visibleHeight +
      (aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom);
    aDesiredSize.ascent =
      scrolledAreaDesiredSize.ascent + aReflowState.mComputedBorderPadding.top;
    aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;
  }

  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = aDesiredSize.width;
  }

  aStatus = NS_FRAME_COMPLETE;

#ifdef DEBUG_rodsX
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
    mCachedAvailableSize.width  = aDesiredSize.width - (aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right);
    REFLOW_DEBUG_MSG2("** nsLCF Caching AW: %d\n", PX(mCachedAvailableSize.width));
  }
  if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
    mCachedAvailableSize.height = aDesiredSize.height - (aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom);
    REFLOW_DEBUG_MSG2("** nsLCF Caching AH: %d\n", PX(mCachedAvailableSize.height));
  }

  //REFLOW_DEBUG_MSG3("** nsLCF Caching AW: %d  AH: %d\n", PX(mCachedAvailableSize.width), PX(mCachedAvailableSize.height));

  nsFormControlFrame::SetupCachedSizes(mCacheSize, mCachedAscent,
                                       mCachedMaxElementWidth, aDesiredSize);

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
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetFormContent(nsIContent*& aContent) const
{
  aContent = GetContent();
  NS_IF_ADDREF(aContent);
  return NS_OK;
}

nsGfxScrollFrameInner::ScrollbarStyles
nsListControlFrame::GetScrollbarStyles() const
{
  // We can't express this in the style system yet; when we can, this can go away
  // and GetScrollbarStyles can be devirtualized
  PRInt32 verticalStyle = IsInDropDownMode() ? NS_STYLE_OVERFLOW_AUTO
    : NS_STYLE_OVERFLOW_SCROLL;
  return nsGfxScrollFrameInner::ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN,
                                                verticalStyle);
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
// Starts at the passed in content object and walks up the 
// parent heierarchy looking for the nsIDOMHTMLOptionElement
//---------------------------------------------------------
nsIContent *
nsListControlFrame::GetOptionFromContent(nsIContent *aContent) 
{
  for (nsIContent* content = aContent; content; content = content->GetParent()) {
    if (IsOptionElement(content)) {
      return content;
    }
  }

  return nsnull;
}

//---------------------------------------------------------
// Finds the index of the hit frame's content in the list
// of option elements
//---------------------------------------------------------
PRInt32 
nsListControlFrame::GetIndexFromContent(nsIContent *aContent)
{
  nsCOMPtr<nsIDOMHTMLOptionElement> option;
  option = do_QueryInterface(aContent);
  if (option) {
    PRInt32 retval;
    option->GetIndex(&retval);
    if (retval >= 0) {
      return retval;
    }
  }
  return kNothingSelected;
}

//---------------------------------------------------------
PRBool
nsListControlFrame::ExtendedSelection(PRInt32 aStartIndex,
                                      PRInt32 aEndIndex,
                                      PRBool aClearAll)
{
  return SetOptionsSelectedFromFrame(aStartIndex, aEndIndex,
                                     PR_TRUE, aClearAll);
}

//---------------------------------------------------------
PRBool
nsListControlFrame::SingleSelection(PRInt32 aClickedIndex, PRBool aDoToggle)
{
  PRBool wasChanged = PR_FALSE;
  // Get Current selection
  if (aDoToggle) {
    wasChanged = ToggleOptionSelectedFromFrame(aClickedIndex);
  } else {
    wasChanged = SetOptionsSelectedFromFrame(aClickedIndex, aClickedIndex,
                                PR_TRUE, PR_TRUE);
  }
  ScrollToIndex(aClickedIndex);
  mStartSelectionIndex = aClickedIndex;
  mEndSelectionIndex = aClickedIndex;
  return wasChanged;
}

void
nsListControlFrame::InitSelectionRange(PRInt32 aClickedIndex)
{
  //
  // If nothing is selected, set the start selection depending on where
  // the user clicked and what the initial selection is:
  // - if the user clicked *before* selectedIndex, set the start index to
  //   the end of the first contiguous selection.
  // - if the user clicked *after* the end of the first contiguous
  //   selection, set the start index to selectedIndex.
  // - if the user clicked *within* the first contiguous selection, set the
  //   start index to selectedIndex.
  // The last two rules, of course, boil down to the same thing: if the user
  // clicked >= selectedIndex, return selectedIndex.
  //
  // This makes it so that shift click works properly when you first click
  // in a multiple select.
  //
  PRInt32 selectedIndex;
  GetSelectedIndex(&selectedIndex);
  if (selectedIndex >= 0) {
    // Get the end of the contiguous selection
    nsCOMPtr<nsIDOMHTMLOptionsCollection> options =
      getter_AddRefs(GetOptions(mContent));
    NS_ASSERTION(options, "Collection of options is null!");
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    PRUint32 i;
    // Push i to one past the last selected index in the group
    for (i=selectedIndex+1; i < numOptions; i++) {
      PRBool selected;
      GetOption(options, i)->GetSelected(&selected);
      if (!selected) {
        break;
      }
    }

    if (aClickedIndex < selectedIndex) {
      // User clicked before selection, so start selection at end of
      // contiguous selection
      mStartSelectionIndex = i-1;
      mEndSelectionIndex = selectedIndex;
    } else {
      // User clicked after selection, so start selection at start of
      // contiguous selection
      mStartSelectionIndex = selectedIndex;
      mEndSelectionIndex = i-1;
    }
  }
}

//---------------------------------------------------------
PRBool
nsListControlFrame::PerformSelection(PRInt32 aClickedIndex,
                                     PRBool aIsShift,
                                     PRBool aIsControl)
{
  PRBool wasChanged = PR_FALSE;

  PRBool isMultiple;
  GetMultiple(&isMultiple);

  if (aClickedIndex == kNothingSelected) {
  } else if (isMultiple) {
    if (aIsShift) {
      // Make sure shift+click actually does something expected when
      // the user has never clicked on the select
      if (mStartSelectionIndex == kNothingSelected) {
        InitSelectionRange(aClickedIndex);
      }

      // Get the range from beginning (low) to end (high)
      // Shift *always* works, even if the current option is disabled
      PRInt32 startIndex;
      PRInt32 endIndex;
      if (mStartSelectionIndex == kNothingSelected) {
        startIndex = aClickedIndex;
        endIndex   = aClickedIndex;
      } else if (mStartSelectionIndex <= aClickedIndex) {
        startIndex = mStartSelectionIndex;
        endIndex   = aClickedIndex;
      } else {
        startIndex = aClickedIndex;
        endIndex   = mStartSelectionIndex;
      }

      // Clear only if control was not pressed
      wasChanged = ExtendedSelection(startIndex, endIndex, !aIsControl);
      ScrollToIndex(aClickedIndex);

      if (mStartSelectionIndex == kNothingSelected) {
        mStartSelectionIndex = aClickedIndex;
        mEndSelectionIndex = aClickedIndex;
      } else {
        mEndSelectionIndex = aClickedIndex;
      }
    } else if (aIsControl) {
      wasChanged = SingleSelection(aClickedIndex, PR_TRUE);
    } else {
      wasChanged = SingleSelection(aClickedIndex, PR_FALSE);
    }
  } else {
    wasChanged = SingleSelection(aClickedIndex, PR_FALSE);
  }

#ifdef ACCESSIBILITY
  FireMenuItemActiveEvent(); // Inform assistive tech what got focus
#endif

  return wasChanged;
}

//---------------------------------------------------------
PRBool
nsListControlFrame::HandleListSelection(nsIDOMEvent* aEvent,
                                        PRInt32 aClickedIndex)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aEvent);
  PRBool isShift;
  PRBool isControl;
#if defined(XP_MAC) || defined(XP_MACOSX)
  mouseEvent->GetMetaKey(&isControl);
#else
  mouseEvent->GetCtrlKey(&isControl);
#endif
  mouseEvent->GetShiftKey(&isShift);
  return PerformSelection(aClickedIndex, isShift, isControl);
}

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::CaptureMouseEvents(nsIPresContext* aPresContext, PRBool aGrabMouseEvents)
{
  nsIView* view = nsnull;
  if (IsInDropDownMode()) {
    view = GetView();
  } else {
    nsIFrame* scrolledFrame = nsnull;
    GetScrolledFrame(aPresContext, scrolledFrame);
    NS_ASSERTION(scrolledFrame, "No scrolled frame found");
    NS_ENSURE_TRUE(scrolledFrame, NS_ERROR_FAILURE);
    
    nsIFrame* scrollport = scrolledFrame->GetParent();
    NS_ASSERTION(scrollport, "No scrollport found");
    NS_ENSURE_TRUE(scrollport, NS_ERROR_FAILURE);

    view = scrollport->GetView();
  }

  NS_ASSERTION(view, "no view???");
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

  nsIViewManager* viewMan = view->GetViewManager();
  if (viewMan) {
    PRBool result;
    // It's not clear why we don't have the widget capture mouse events here.
    if (aGrabMouseEvents) {
      viewMan->GrabMouseEvents(view, result);
    } else {
      nsIView* curGrabber;
      viewMan->GetMouseEventGrabber(curGrabber);
      PRBool dropDownIsHidden = PR_FALSE;
      if (IsInDropDownMode()) {
        PRBool isDroppedDown;
        mComboboxFrame->IsDroppedDown(&isDroppedDown);
        dropDownIsHidden = !isDroppedDown;
      }
      if (curGrabber == view || dropDownIsHidden) {
        // only unset the grabber if *we* are the ones doing the grabbing
        // (or if the dropdown is hidden, in which case NO-ONE should be
        // grabbing anything
        // it could be a scrollbar inside this listbox which is actually grabbing
        // This shouldn't be necessary. We should simply ensure that events targeting
        // scrollbars are never visible to DOM consumers.
        viewMan->GrabMouseEvents(nsnull, result);
      }
    }
  }

  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  // temp fix until Bug 124990 gets fixed
  if (aPresContext->IsPaginated() && NS_IS_MOUSE_EVENT(aEvent)) {
    return NS_OK;
  }

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
  const nsStyleUserInterface* uiStyle = GetStyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  if (nsFormControlHelper::GetDisabled(mContent))
    return NS_OK;

  return nsGfxScrollFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  // First check to see if all the content has been added
  mIsAllContentHere = mContent->IsDoneAddingChildren();
  if (!mIsAllContentHere) {
    mIsAllFramesHere    = PR_FALSE;
    mHasBeenInitialized = PR_FALSE;
  }
  nsresult rv = nsGfxScrollFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // If all the content is here now check
  // to see if all the frames have been created
  /*if (mIsAllContentHere) {
    // If all content and frames are here
    // the reset/initialize
    if (CheckIfAllFramesHere()) {
      ResetList(aPresContext);
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
                         nsStyleContext*  aContext,
                         nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);
  nsresult result = nsGfxScrollFrame::Init(aPresContext, aContent, aParent, aContext,
                                           aPrevInFlow);

  // get the receiver interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mContent));

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  // we need to hook up our listeners before the editor is initialized
  mEventListener = new nsListEventListener(this);
  if (!mEventListener) 
    return NS_ERROR_OUT_OF_MEMORY;

  receiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*,
                                                 mEventListener),
                                  NS_GET_IID(nsIDOMMouseListener));

  receiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseMotionListener*,
                                                 mEventListener),
                                  NS_GET_IID(nsIDOMMouseMotionListener));

  receiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMKeyListener*,
                                                 mEventListener),
                                  NS_GET_IID(nsIDOMKeyListener));

  mStartSelectionIndex = kNothingSelected;
  mEndSelectionIndex = kNothingSelected;

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
nsListControlFrame::GetOptionAsContent(nsIDOMHTMLOptionsCollection* aCollection, PRInt32 aIndex) 
{
  nsIContent * content = nsnull;
  nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = getter_AddRefs(GetOption(aCollection, aIndex));

  NS_ASSERTION(optionElement != nsnull, "could not get option element by index!");

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
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options =
    getter_AddRefs(GetOptions(mContent));
  NS_ASSERTION(options.get() != nsnull, "Collection of options is null!");

  if (options) {
    content = GetOptionAsContent(options, aIndex);
  } 
  return(content);
}

//---------------------------------------------------------
// This returns the collection for nsIDOMHTMLSelectElement or
// the nsIContent object is the select is null  (AddRefs)
//---------------------------------------------------------
nsIDOMHTMLOptionsCollection* 
nsListControlFrame::GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect)
{
  nsIDOMHTMLOptionsCollection* options = nsnull;
  if (!aSelect) {
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = getter_AddRefs(GetSelect(aContent));
    if (selectElement) {
      selectElement->GetOptions(&options);  // AddRefs (1)
    }
  } else {
    aSelect->GetOptions(&options); // AddRefs (1)
  }

  return options;
}

//---------------------------------------------------------
// Returns the nsIDOMHTMLOptionElement for a given index 
// in the select's collection
//---------------------------------------------------------
nsIDOMHTMLOptionElement*
nsListControlFrame::GetOption(nsIDOMHTMLOptionsCollection* aCollection,
                              PRInt32 aIndex)
{
  nsCOMPtr<nsIDOMNode> node;
  if (NS_SUCCEEDED(aCollection->Item(aIndex, getter_AddRefs(node)))) {
    NS_ASSERTION(node,
                 "Item was successful, but node from collection was null!");
    if (node) {
      nsIDOMHTMLOptionElement* option = nsnull;
      CallQueryInterface(node, &option);

      return option;
    }
  } else {
    NS_ERROR("Couldn't get option by index from collection!");
  }
  return nsnull;
}


//---------------------------------------------------------
// For a given piece of content, it determines whether the 
// content (an option) is selected or not
// return PR_TRUE if it is, PR_FALSE if it is NOT
//---------------------------------------------------------
PRBool 
nsListControlFrame::IsContentSelected(nsIContent* aContent)
{
  PRBool isSelected = PR_FALSE;

  nsCOMPtr<nsIDOMHTMLOptionElement> optEl = do_QueryInterface(aContent);
  if (optEl)
    optEl->GetSelected(&isSelected);

  return isSelected;
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
NS_IMETHODIMP
nsListControlFrame::OnOptionSelected(nsIPresContext* aPresContext,
                                     PRInt32 aIndex,
                                     PRBool aSelected)
{
  if (aSelected) {
    ScrollToIndex(aIndex);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::OnOptionTextChanged(nsIDOMHTMLOptionElement* option)
{
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
NS_IMETHODIMP_(PRInt32)
nsListControlFrame::GetFormControlType() const
{
  return NS_FORM_SELECT;
}


//---------------------------------------------------------
void
nsListControlFrame::MouseClicked(nsIPresContext* aPresContext)
{
}


NS_IMETHODIMP
nsListControlFrame::OnContentReset()
{
  ResetList(mPresContext);
  return NS_OK;
}

//---------------------------------------------------------
// Resets the select back to it's original default values;
// those values as determined by the original HTML
//---------------------------------------------------------
void 
nsListControlFrame::ResetList(nsIPresContext* aPresContext, nsVoidArray * aInxList)
{
  REFLOW_DEBUG_MSG("LBX::ResetList\n");

  // if all the frames aren't here 
  // don't bother reseting
  if (!mIsAllFramesHere) {
    return;
  }

  // Scroll to the selected index
  PRInt32 indexToSelect = kNothingSelected;

  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(mContent));
  NS_ASSERTION(selectElement, "No select element!");
  if (selectElement) {
    selectElement->GetSelectedIndex(&indexToSelect);
    ScrollToIndex(indexToSelect);
  }

  mStartSelectionIndex = kNothingSelected;
  mEndSelectionIndex = kNothingSelected;

  // Combobox will redisplay itself with the OnOptionSelected event
} 

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::GetName(nsAString* aResult)
{
  return nsFormControlHelper::GetName(mContent, aResult);
}
 

//---------------------------------------------------------
void 
nsListControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  if (aOn) {
    ComboboxFocusSet();
    PRInt32 selectedIndex;
    GetSelectedIndex(&selectedIndex);
    mFocused = this;
  } else {
    mFocused = nsnull;
  }

  // Make sure the SelectArea frame gets painted
  Invalidate(nsRect(0,0,mRect.width,mRect.height), PR_TRUE);
}

void nsListControlFrame::ComboboxFocusSet()
{
  gLastKeyTime = 0;
}

//---------------------------------------------------------
void 
nsListControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsIPresShell *presShell = aPresContext->GetPresShell();
    if (presShell) {
      presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
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
nsListControlFrame::GetOptionText(PRInt32 aIndex, nsAString & aStr)
{
  aStr.SetLength(0);
  nsresult rv = NS_ERROR_FAILURE; 
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options =
    getter_AddRefs(GetOptions(mContent));

  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);

    if (numOptions == 0) {
      rv = NS_OK;
    } else {
      nsCOMPtr<nsIDOMHTMLOptionElement> optionElement(
          getter_AddRefs(GetOption(options, aIndex)));
      if (optionElement) {
#if 0 // This is for turning off labels Bug 4050
        nsAutoString text;
        rv = optionElement->GetLabel(text);
        // the return value is always NS_OK from DOMElements
        // it is meaningless to check for it
        if (!text.IsEmpty()) { 
          nsAutoString compressText = text;
          compressText.CompressWhitespace(PR_TRUE, PR_TRUE);
          if (!compressText.IsEmpty()) {
            text = compressText;
          }
        }

        if (text.IsEmpty()) {
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
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(mContent));
  return selectElement->GetSelectedIndex(aIndex);
}

//---------------------------------------------------------
PRBool 
nsListControlFrame::IsInDropDownMode() const
{
  return((nsnull == mComboboxFrame) ? PR_FALSE : PR_TRUE);
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetNumberOfOptions(PRInt32* aNumOptions) 
{
  if (mContent != nsnull) {
    nsCOMPtr<nsIDOMHTMLOptionsCollection> options =
      getter_AddRefs(GetOptions(mContent));

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
nsListControlFrame::DoneAddingChildren(PRBool aIsDone)
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
        ResetList(mPresContext);
      }
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::AddOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
  StopUpdateTimer();

#ifdef DO_REFLOW_DEBUG
  printf("---- Id: %d nsLCF %p Added Option %d\n", mReflowId, this, aIndex);
#endif

  PRInt32 numOptions;
  GetNumberOfOptions(&numOptions);

  if (!mIsAllContentHere) {
    mIsAllContentHere = mContent->IsDoneAddingChildren();
    if (!mIsAllContentHere) {
      mIsAllFramesHere    = PR_FALSE;
      mHasBeenInitialized = PR_FALSE;
    } else {
      mIsAllFramesHere = aIndex == numOptions-1;
    }
  }
  
  if (!mHasBeenInitialized) {
    return NS_OK;
  }

  nsresult rv = StartUpdateTimer(aPresContext);
  if (NS_SUCCEEDED(rv) && mUpdateTimer) {
    mUpdateTimer->ItemAdded(aIndex, numOptions);
  }
  return NS_OK;
}

//-------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::RemoveOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
  StopUpdateTimer();
  nsresult rv = StartUpdateTimer(aPresContext);
  if (NS_SUCCEEDED(rv) && mUpdateTimer) {
    PRInt32 numOptions;
    GetNumberOfOptions(&numOptions);
    mUpdateTimer->ItemRemoved(aIndex, numOptions);
  }

  return NS_OK;
}

//---------------------------------------------------------
// Set the option selected in the DOM.  This method is named
// as it is because it indicates that the frame is the source
// of this event rather than the receiver.
PRBool
nsListControlFrame::SetOptionsSelectedFromFrame(PRInt32 aStartIndex,
                                                PRInt32 aEndIndex,
                                                PRBool aValue,
                                                PRBool aClearAll)
{
  nsCOMPtr<nsISelectElement> selectElement(do_QueryInterface(mContent));
  PRBool wasChanged = PR_FALSE;
  nsresult rv = selectElement->SetOptionsSelectedByIndex(aStartIndex,
                                                         aEndIndex,
                                                         aValue,
                                                         aClearAll,
                                                         PR_FALSE,
                                                         PR_TRUE,
                                                         &wasChanged);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetSelected failed");
  return wasChanged;
}

PRBool
nsListControlFrame::ToggleOptionSelectedFromFrame(PRInt32 aIndex)
{
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options =
    getter_AddRefs(GetOptions(mContent));
  NS_ASSERTION(options, "No options");
  if (!options) {
    return PR_FALSE;
  }
  nsCOMPtr<nsIDOMHTMLOptionElement> option(
      getter_AddRefs(GetOption(options, aIndex)));
  NS_ASSERTION(option, "No option");
  if (!option) {
    return PR_FALSE;
  }

  PRBool value = PR_FALSE;
  nsresult rv = option->GetSelected(&value);

  NS_ASSERTION(NS_SUCCEEDED(rv), "GetSelected failed");
  nsCOMPtr<nsISelectElement> selectElement(do_QueryInterface(mContent));
  PRBool wasChanged = PR_FALSE;
  rv = selectElement->SetOptionsSelectedByIndex(aIndex,
                                                aIndex,
                                                !value,
                                                PR_FALSE,
                                                PR_FALSE,
                                                PR_TRUE,
                                                &wasChanged);

  NS_ASSERTION(NS_SUCCEEDED(rv), "SetSelected failed");

  return wasChanged;
}


// Dispatch event and such
NS_IMETHODIMP
nsListControlFrame::UpdateSelection()
{
  nsresult rv = NS_OK;

  if (mIsAllFramesHere) {
    // if it's a combobox, display the new text
    if (mComboboxFrame) {
      rv = mComboboxFrame->RedisplaySelectedText();
    }
    // if it's a listbox, fire on change
    else if (mIsAllContentHere) {
      rv = FireOnChange();
    }
  }

  return rv;
}

NS_IMETHODIMP
nsListControlFrame::ComboboxFinish(PRInt32 aIndex)
{
  gLastKeyTime = 0;

  if (mComboboxFrame) {
    PerformSelection(aIndex, PR_FALSE, PR_FALSE);

    PRInt32 displayIndex;
    mComboboxFrame->GetIndexOfDisplayArea(&displayIndex);

    if (displayIndex != aIndex) {
      mComboboxFrame->RedisplaySelectedText();
    }

    mComboboxFrame->RollupFromList(mPresContext);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::GetOptionsContainer(nsIPresContext* aPresContext,
                                        nsIFrame** aFrame)
{
  return GetScrolledFrame(aPresContext, *aFrame);
}

// Send out an onchange notification.
NS_IMETHODIMP
nsListControlFrame::FireOnChange()
{
  nsresult rv = NS_OK;
  
  if (mComboboxFrame) {
    if (!mComboboxFrame->NeededToFireOnChange())
      return NS_OK;
  }

  // Dispatch the NS_FORM_CHANGE event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event(NS_FORM_CHANGE);

  nsIPresShell *presShell = mPresContext->GetPresShell();
  if (presShell) {
    rv = presShell->HandleEventWithTarget(&event, this, nsnull,
                                           NS_EVENT_FLAG_INIT, &status);
  }

  return rv;
}

//---------------------------------------------------------
// Determine if the specified item in the listbox is selected.
NS_IMETHODIMP
nsListControlFrame::GetOptionSelected(PRInt32 aIndex, PRBool* aValue)
{
  *aValue = IsContentSelectedByIndex(aIndex);
  return NS_OK;
}

//---------------------------------------------------------
// Used by layout to determine if we have a fake option
NS_IMETHODIMP
nsListControlFrame::GetDummyFrame(nsIFrame** aFrame)
{
  (*aFrame) = mDummyFrame;
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::SetDummyFrame(nsIFrame* aFrame)
{
  mDummyFrame = aFrame;
  return NS_OK;
}

//----------------------------------------------------------------------
// End nsISelectControlFrame
//----------------------------------------------------------------------

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName,
                                const nsAString& aValue)
{
  if (nsHTMLAtoms::selected == aName) {
    return NS_ERROR_INVALID_ARG; // Selected is readonly according to spec.
  } else if (nsHTMLAtoms::selectedindex == aName) {
    // You shouldn't be calling me for this!!!
    return NS_ERROR_INVALID_ARG;
  }

  // We should be told about selectedIndex by the DOM element through
  // OnOptionSelected

  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::GetProperty(nsIAtom* aName, nsAString& aValue)
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
  } else if (nsHTMLAtoms::selectedindex == aName) {
    // You shouldn't be calling me for this!!!
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

//---------------------------------------------------------
void
nsListControlFrame::GetViewOffset(nsIViewManager* aManager, nsIView* aView, 
  nsPoint& aPoint)
{
  aPoint.x = 0;
  aPoint.y = 0;
 
  for (nsIView* parent = aView;
       parent && parent->GetViewManager() == aManager;
       parent = parent->GetParent()) {
    aPoint += parent->GetPosition();
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

  nsContainerFrame::PositionFrameView(aPresContext, this);

  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::AboutToDropDown()
{
  if (mIsAllContentHere && mIsAllFramesHere && mHasBeenInitialized) {
    PRInt32 selectedIndex;
    GetSelectedIndex(&selectedIndex);
    ScrollToIndex(selectedIndex);
#ifdef ACCESSIBILITY
    FireMenuItemActiveEvent(); // Inform assistive tech what got focus
#endif
  }

  return NS_OK;
}

//---------------------------------------------------------
// We are about to be rolledup from the outside (ComboboxFrame)
NS_IMETHODIMP 
nsListControlFrame::AboutToRollup()
{
  // We've been updating the combobox with the keyboard up until now, but not
  // with the mouse.  The problem is, even with mouse selection, we are
  // updating the <select>.  So if the mouse goes over an option just before
  // he leaves the box and clicks, that's what the <select> will show.
  //
  // To deal with this we say "whatever is in the combobox is canonical."
  // - IF the combobox is different from the current selected index, we
  //   reset the index.

  if (IsInDropDownMode() == PR_TRUE) {
    PRInt32 index;
    mComboboxFrame->GetIndexOfDisplayArea(&index);
    ComboboxFinish(index);
  }
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::DidReflow(nsIPresContext*           aPresContext,
                              const nsHTMLReflowState*  aReflowState,
                              nsDidReflowStatus         aStatus)
{
  if (PR_TRUE == IsInDropDownMode()) 
  {
    //SyncViewWithFrame();
    mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
    nsresult rv = nsGfxScrollFrame::DidReflow(aPresContext, aReflowState, aStatus);
    mState |= NS_FRAME_SYNC_FRAME_AND_VIEW;
    SyncViewWithFrame(aPresContext);
    return rv;
  } else {
    nsresult rv = nsGfxScrollFrame::DidReflow(aPresContext, aReflowState, aStatus);
    PRInt32 selectedIndex = mEndSelectionIndex;
    if (selectedIndex == kNothingSelected) {
      GetSelectedIndex(&selectedIndex);
    }
    if (!mDoneWithInitialReflow && selectedIndex != kNothingSelected) {
      ScrollToIndex(selectedIndex);
      mDoneWithInitialReflow = PR_TRUE;
    }
    return rv;
  }
}

nsIAtom*
nsListControlFrame::GetType() const
{
  return nsLayoutAtoms::listControlFrame; 
}

#ifdef DEBUG
NS_IMETHODIMP
nsListControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ListControl"), aResult);
}
#endif

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
nsresult
nsListControlFrame::IsOptionDisabled(PRInt32 anIndex, PRBool &aIsDisabled)
{
  nsCOMPtr<nsISelectElement> sel(do_QueryInterface(mContent));
  if (sel) {
    sel->IsOptionDisabled(anIndex, &aIsDisabled);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
      return whichButton != 0?PR_FALSE:PR_TRUE;
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

  mButtonDown = PR_FALSE;

  if (nsFormControlHelper::GetDisabled(mContent)) {
    return NS_OK;
  }

  // only allow selection with the left button
  // if a right button click is on the combobox itself
  // or on the select when in listbox mode, then let the click through
  if (!IsLeftButton(aMouseEvent)) {
    if (IsInDropDownMode() == PR_TRUE) {
      if (!IsClickingInCombobox(aMouseEvent)) {
        aMouseEvent->PreventDefault();

        nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));

        if (nsevent) {
          nsevent->PreventCapture();
          nsevent->PreventBubble();
        }
      } else {
        CaptureMouseEvents(mPresContext, PR_FALSE);
        return NS_OK;
      }
      CaptureMouseEvents(mPresContext, PR_FALSE);
      return NS_ERROR_FAILURE; // means consume event
    } else {
      CaptureMouseEvents(mPresContext, PR_FALSE);
      return NS_OK;
    }
  }

  const nsStyleVisibility* vis = GetStyleVisibility();
      
  if (!vis->IsVisible()) {
    REFLOW_DEBUG_MSG(">>>>>> Select is NOT visible");
    return NS_OK;
  }

  if (IsInDropDownMode()) {
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

    PRInt32 selectedIndex;
    if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
      REFLOW_DEBUG_MSG2(">>>>>> Found Index: %d", selectedIndex);

      // If it's disabled, disallow the click and leave.
      PRBool isDisabled = PR_FALSE;
      IsOptionDisabled(selectedIndex, isDisabled);
      if (isDisabled) {
        aMouseEvent->PreventDefault();

        nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));

        if (nsevent) {
          nsevent->PreventCapture();
          nsevent->PreventBubble();
        }

        CaptureMouseEvents(mPresContext, PR_FALSE);
        return NS_ERROR_FAILURE;
      }

      if (kNothingSelected != selectedIndex) {
        ComboboxFinish(selectedIndex);
        FireOnChange();
      }

      mouseEvent->clickCount = 1;
    } else {
      // the click was out side of the select or its dropdown
      mouseEvent->clickCount = IsClickingInCombobox(aMouseEvent)?1:0;
    }
  } else {
    REFLOW_DEBUG_MSG(">>>>>> Didn't find");
    CaptureMouseEvents(mPresContext, PR_FALSE);
    // Notify
    if (mChangesSinceDragStart) {
      // reset this so that future MouseUps without a prior MouseDown
      // won't fire onchange
      mChangesSinceDragStart = PR_FALSE;
      FireOnChange();
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
// Fire a custom DOM event for the change, so that accessibility can
// fire a native focus event for accessibility 
// (Some 3rd party products need to track our focus)
#ifdef ACCESSIBILITY
void
nsListControlFrame::FireMenuItemActiveEvent()
{
  nsCOMPtr<nsIDOMEvent> event;
  nsCOMPtr<nsIEventListenerManager> manager;
  mContent->GetListenerManager(getter_AddRefs(manager));
  if (manager &&
      NS_SUCCEEDED(manager->CreateEvent(mPresContext, nsnull, NS_LITERAL_STRING("Events"), getter_AddRefs(event)))) {
    event->InitEvent(NS_LITERAL_STRING("DOMMenuItemActive"), PR_TRUE, PR_TRUE);
    PRBool noDefault;
    mPresContext->EventStateManager()->DispatchNewEvent(mContent, event,
                                                        &noDefault);
  }
}
#endif

//----------------------------------------------------------------------
// Sets the mSelectedIndex and mOldSelectedIndex from figuring out what 
// item was selected using content
// Returns NS_OK if it successfully found the selection
//----------------------------------------------------------------------
nsresult
nsListControlFrame::GetIndexFromDOMEvent(nsIDOMEvent* aMouseEvent, 
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
  nsIFrame * frame;
  mPresContext->PresShell()->GetPrimaryFrameFor(content, &frame);
  printf("Target Frame: %p  this: %p\n", frame, this);
  printf("-->\n");
#endif

  nsresult rv;
  
  nsCOMPtr<nsIContent> content;
  mPresContext->EventStateManager()->
    GetEventTargetContent(nsnull, getter_AddRefs(content));

  nsCOMPtr<nsIContent> optionContent = GetOptionFromContent(content);
  if (optionContent) {
    aCurIndex = GetIndexFromContent(optionContent);
    rv = NS_OK;
  } else {
    rv = NS_ERROR_FAILURE;
  }


  return rv;
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");

  REFLOW_DEBUG_MSG("--------------------------- MouseDown ----------------------------\n");

  mButtonDown = PR_TRUE;

  if (nsFormControlHelper::GetDisabled(mContent)) {
    return NS_OK;
  }

  // only allow selection with the left button
  // if a right button click is on the combobox itself
  // or on the select when in listbox mode, then let the click through
  if (!IsLeftButton(aMouseEvent)) {
    if (IsInDropDownMode()) {
      if (!IsClickingInCombobox(aMouseEvent)) {
        aMouseEvent->PreventDefault();

        nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));

        if (nsevent) {
          nsevent->PreventCapture();
          nsevent->PreventBubble();
        }
      } else {
        return NS_OK;
      }
      return NS_ERROR_FAILURE; // means consume event
    } else {
      return NS_OK;
    }
  }

  PRInt32 selectedIndex;
  if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
    if (!IsInDropDownMode()) {
      // Handle Like List
      CaptureMouseEvents(mPresContext, PR_TRUE);
      mChangesSinceDragStart = HandleListSelection(aMouseEvent, selectedIndex);
    }
  } else {
    // NOTE: the combo box is responsible for dropping it down
    if (mComboboxFrame) {
      if (!IsClickingInCombobox(aMouseEvent)) {
        return NS_OK;
      }

      PRBool isDroppedDown;
      mComboboxFrame->IsDroppedDown(&isDroppedDown);
      mComboboxFrame->ShowDropDown(!isDroppedDown);

      if (isDroppedDown) {
        CaptureMouseEvents(mPresContext, PR_FALSE);
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
  NS_ASSERTION(aMouseEvent, "aMouseEvent is null.");
  //REFLOW_DEBUG_MSG("MouseMove\n");

  if (IsInDropDownMode() == PR_TRUE) { 
    PRBool isDroppedDown = PR_FALSE;
    mComboboxFrame->IsDroppedDown(&isDroppedDown);
    if (isDroppedDown) {
      PRInt32 selectedIndex;
      if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
        PerformSelection(selectedIndex, PR_FALSE, PR_FALSE);
      }

      // Make sure the SelectArea frame gets painted
      // XXX this shouldn't be needed, but other places in this code do it
      // and if we don't do this, invalidation doesn't happen when we move out
      // of the top-level window. We should track this down and fix it --- roc
      Invalidate(nsRect(0,0,mRect.width,mRect.height), PR_TRUE);
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
  NS_ASSERTION(aMouseEvent, "aMouseEvent is null.");
  //REFLOW_DEBUG_MSG("DragMove\n");

  if (IsInDropDownMode() == PR_FALSE) { 
    PRInt32 selectedIndex;
    if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
      // Don't waste cycles if we already dragged over this item
      if (selectedIndex == mEndSelectionIndex) {
        return NS_OK;
      }
      nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
      NS_ASSERTION(mouseEvent, "aMouseEvent is not an nsIDOMMouseEvent!");
      PRBool isControl;
#if defined(XP_MAC) || defined(XP_MACOSX)
      mouseEvent->GetMetaKey(&isControl);
#else
      mouseEvent->GetCtrlKey(&isControl);
#endif
      // Turn SHIFT on when you are dragging, unless control is on.
      PRBool wasChanged = PerformSelection(selectedIndex,
                                           !isControl, isControl);
      mChangesSinceDragStart = mChangesSinceDragStart || wasChanged;
    }
  }
  return NS_OK;
}

nsresult
nsListControlFrame::ScrollToIndex(PRInt32 aIndex)
{
  if (aIndex < 0) {
    return ScrollToFrame(nsnull);
  } else {
    nsCOMPtr<nsIContent> content = getter_AddRefs(GetOptionContent(aIndex));
    if (content) {
      return ScrollToFrame(content);
    }
  }

  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::ScrollToFrame(nsIContent* aOptElement)
{
  nsIScrollableView * scrollableView;
  GetScrollableView(mPresContext, &scrollableView);

  if (scrollableView) {
    // if null is passed in we scroll to 0,0
    if (nsnull == aOptElement) {
      scrollableView->ScrollTo(0, 0, PR_TRUE);
      return NS_OK;
    }
  
    // otherwise we find the content's frame and scroll to it
    nsIPresShell *presShell = mPresContext->PresShell();
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
        nsRect rect = clippedView->GetBounds();
        // now move it by the offset of the scroll position
        rect.x = x;
        rect.y = y;

        // get the child
        nsRect fRect = childframe->GetRect();
        nsPoint pnt;
        nsIView * view;
        childframe->GetOffsetFromView(mPresContext, pnt, &view);

        // This change for 33421 (remove this comment later)

        // options can be a child of an optgroup
        // this checks to see the parent is an optgroup
        // and then adds in the parent's y coord
        // XXX this assume only one level of nesting of optgroups
        //   which is all the spec specifies at the moment.
        nsCOMPtr<nsIContent> parentContent = aOptElement->GetParent();
        nsCOMPtr<nsIDOMHTMLOptGroupElement> optGroup(do_QueryInterface(parentContent));
        nsRect optRect(0,0,0,0);
        if (optGroup) {
          nsIFrame * optFrame;
          result = presShell->GetPrimaryFrameFor(parentContent, &optFrame);
          if (NS_SUCCEEDED(result) && optFrame) {
            optRect = optFrame->GetRect();
          }
        }
        fRect.y += optRect.y;

        // See if the selected frame (fRect) is inside the scrolled
        // area (rect). Check only the vertical dimension. Don't
        // scroll just because there's horizontal overflow.
        if (!(rect.y <= fRect.y && fRect.YMost() <= rect.YMost())) {
          // figure out which direction we are going
          if (fRect.YMost() > rect.YMost()) {
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
// aStartIndex - the index to start searching from
// aNewIndex - will get set to the new index if it finds one
// aNumOptions - the total number of options in the list
// aDoAdjustInc - the initial increment 1-n
// aDoAdjustIncNext - the increment used to search for the next enabled option
//
// the aDoAdjustInc could be a "1" for a single item or
// any number greater representing a page of items
//
void
nsListControlFrame::AdjustIndexForDisabledOpt(PRInt32 aStartIndex,
                                              PRInt32 &aNewIndex,
                                              PRInt32 aNumOptions,
                                              PRInt32 aDoAdjustInc,
                                              PRInt32 aDoAdjustIncNext)
{
  // Cannot select anything if there is nothing to select
  if (aNumOptions == 0) {
    aNewIndex = kNothingSelected;
    return;
  }

  // means we reached the end of the list and now we are searching backwards
  PRBool doingReverse = PR_FALSE;
  // lowest index in the search range
  PRInt32 bottom      = 0;
  // highest index in the search range
  PRInt32 top         = aNumOptions;

  // Start off keyboard options at selectedIndex if nothing else is defaulted to
  //
  // XXX Perhaps this should happen for mouse too, to start off shift click
  // automatically in multiple ... to do this, we'd need to override
  // OnOptionSelected and set mStartSelectedIndex if nothing is selected.  Not
  // sure of the effects, though, so I'm not doing it just yet.
  PRInt32 startIndex = aStartIndex;
  if (startIndex < bottom) {
    GetSelectedIndex(&startIndex);
  }
  PRInt32 newIndex    = startIndex + aDoAdjustInc;

  // make sure we start off in the range
  if (newIndex < bottom) {
    newIndex = 0;
  } else if (newIndex >= top) {
    newIndex = aNumOptions-1;
  }

  while (1) {
    // if the newIndex isn't disabled, we are golden, bail out
    PRBool isDisabled = PR_TRUE;
    if (NS_SUCCEEDED(IsOptionDisabled(newIndex, isDisabled)) && !isDisabled) {
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
        aDoAdjustIncNext = 1;
        doingReverse     = PR_TRUE;
        top              = startIndex;
      }
    } else  if (newIndex >= top) {
      if (doingReverse) {
        return;        // if we are in reverse mode and reach the end bail out
      } else {
        // reset the newIndex to the end of the list we hit
        // reverse the incrementer
        // set the other end of the list to our original starting index
        newIndex = top - 1;
        aDoAdjustIncNext = -1;
        doingReverse     = PR_TRUE;
        bottom           = startIndex;
      }
    }
  }

  // Looks like we found one
  aNewIndex     = newIndex;
}

nsAString& 
nsListControlFrame::GetIncrementalString()
{ 
  static nsString incrementalString;
  return incrementalString; 
}

void
nsListControlFrame::DropDownToggleKey(nsIDOMEvent* aKeyEvent)
{
  if (IsInDropDownMode()) {
    PRBool isDroppedDown;
    mComboboxFrame->IsDroppedDown(&isDroppedDown);
    mComboboxFrame->ShowDropDown(!isDroppedDown);
    aKeyEvent->PreventDefault();
  }
}

nsresult
nsListControlFrame::KeyPress(nsIDOMEvent* aKeyEvent)
{
  NS_ASSERTION(aKeyEvent, "keyEvent is null.");

  if (nsFormControlHelper::GetDisabled(mContent))
    return NS_OK;

  // Start by making sure we can query for a key event
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_FAILURE);

  PRUint32 keycode = 0;
  PRUint32 charcode = 0;
  keyEvent->GetKeyCode(&keycode);
  keyEvent->GetCharCode(&charcode);
#ifdef DO_REFLOW_DEBUG
  if (code >= 32) {
    REFLOW_DEBUG_MSG3("KeyCode: %c %d\n", code, code);
  }
#endif

  PRBool isAlt = PR_FALSE;

  keyEvent->GetAltKey(&isAlt);
  if (isAlt) {
    if (keycode == nsIDOMKeyEvent::DOM_VK_UP || keycode == nsIDOMKeyEvent::DOM_VK_DOWN) {
      DropDownToggleKey(aKeyEvent);
    }
    return NS_OK;
  }

  // Get control / shift modifiers
  PRBool isControl = PR_FALSE;
  PRBool isShift   = PR_FALSE;
  keyEvent->GetCtrlKey(&isControl);
  if (!isControl) {
    keyEvent->GetMetaKey(&isControl);
  }
  keyEvent->GetShiftKey(&isShift);

  // now make sure there are options or we are wasting our time
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options =
    getter_AddRefs(GetOptions(mContent));
  NS_ENSURE_TRUE(options, NS_ERROR_FAILURE);

  PRUint32 numOptions = 0;
  options->GetLength(&numOptions);

  // Whether we did an incremental search or another action
  PRBool didIncrementalSearch = PR_FALSE;
  
  // this is the new index to set
  // DOM_VK_RETURN & DOM_VK_ESCAPE will not set this
  PRInt32 newIndex = kNothingSelected;

  // set up the old and new selected index and process it
  // DOM_VK_RETURN selects the item
  // DOM_VK_ESCAPE cancels the selection
  // default processing checks to see if the pressed the first 
  //   letter of an item in the list and advances to it

  switch (keycode) {

    case nsIDOMKeyEvent::DOM_VK_UP:
    case nsIDOMKeyEvent::DOM_VK_LEFT: {
      REFLOW_DEBUG_MSG2("DOM_VK_UP   mEndSelectionIndex: %d ",
                        mEndSelectionIndex);
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                (PRInt32)numOptions,
                                -1, -1);
      REFLOW_DEBUG_MSG2("  After: %d\n", newIndex);
      } break;
    
    case nsIDOMKeyEvent::DOM_VK_DOWN:
    case nsIDOMKeyEvent::DOM_VK_RIGHT: {
      REFLOW_DEBUG_MSG2("DOM_VK_DOWN mEndSelectionIndex: %d ",
                        mEndSelectionIndex);

      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                (PRInt32)numOptions,
                                1, 1);
      REFLOW_DEBUG_MSG2("  After: %d\n", newIndex);
      } break;

    case nsIDOMKeyEvent::DOM_VK_RETURN: {
      if (mComboboxFrame != nsnull) {
        PRBool droppedDown = PR_FALSE;
        mComboboxFrame->IsDroppedDown(&droppedDown);
        if (droppedDown) {
          ComboboxFinish(mEndSelectionIndex);
        }
        FireOnChange();
        return NS_OK;
      } else {
        newIndex = mEndSelectionIndex;
      }
      } break;

    case nsIDOMKeyEvent::DOM_VK_ESCAPE: {
      AboutToRollup();
      } break;

    case nsIDOMKeyEvent::DOM_VK_PAGE_UP: {
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                (PRInt32)numOptions,
                                -(mNumDisplayRows-1), -1);
      } break;

    case nsIDOMKeyEvent::DOM_VK_PAGE_DOWN: {
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                (PRInt32)numOptions,
                                (mNumDisplayRows-1), 1);
      } break;

    case nsIDOMKeyEvent::DOM_VK_HOME: {
      AdjustIndexForDisabledOpt(0, newIndex,
                                (PRInt32)numOptions,
                                0, 1);
      } break;

    case nsIDOMKeyEvent::DOM_VK_END: {
      AdjustIndexForDisabledOpt(numOptions-1, newIndex,
                                (PRInt32)numOptions,
                                0, -1);
      } break;

#if defined(XP_WIN) || defined(XP_OS2)
    case nsIDOMKeyEvent::DOM_VK_F4: {
      DropDownToggleKey(aKeyEvent);
      return NS_OK;
    } break;
#endif

    case nsIDOMKeyEvent::DOM_VK_TAB: {
      return NS_OK;
    }

    default: { // Select option with this as the first character
               // XXX Not I18N compliant
      
      if (isControl && charcode != ' ') {
        return NS_OK;
      }

      didIncrementalSearch = PR_TRUE;
      if (charcode == 0) {
        // Backspace key will delete the last char in the string
        if (keycode == NS_VK_BACK && !GetIncrementalString().IsEmpty()) {
          GetIncrementalString().Truncate(GetIncrementalString().Length() - 1);
          aKeyEvent->PreventDefault();
        }
        return NS_OK;
      }
      
      DOMTimeStamp keyTime;
      aKeyEvent->GetTimeStamp(&keyTime);

      // Incremental Search: if time elapsed is below
      // INCREMENTAL_SEARCH_KEYPRESS_TIME, append this keystroke to the search
      // string we will use to find options and start searching at the current
      // keystroke.  Otherwise, Truncate the string if it's been a long time
      // since our last keypress.
      if (keyTime - gLastKeyTime > INCREMENTAL_SEARCH_KEYPRESS_TIME) {
        // If this is ' ' and we are at the beginning of the string, treat it as
        // "select this option" (bug 191543)
        if (charcode == ' ') {
          newIndex = mEndSelectionIndex;
          break;
        }
        GetIncrementalString().Truncate();
      }
      gLastKeyTime = keyTime;

      // Append this keystroke to the search string. 
      PRUnichar uniChar = ToLowerCase(NS_STATIC_CAST(PRUnichar, charcode));
      GetIncrementalString().Append(uniChar);

      // See bug 188199, if all letters in incremental string are same, just try to match the first one
      nsAutoString incrementalString(GetIncrementalString());
      PRUint32 charIndex = 1, stringLength = incrementalString.Length();
      while (charIndex < stringLength && incrementalString[charIndex] == incrementalString[charIndex - 1]) {
        charIndex++;
      }
      if (charIndex == stringLength) {
        incrementalString.Truncate(1);
        stringLength = 1;
      }

      // Determine where we're going to start reading the string
      // If we have multiple characters to look for, we start looking *at* the
      // current option.  If we have only one character to look for, we start
      // looking *after* the current option.	
      // Exception: if there is no option selected to start at, we always start
      // *at* 0.
      PRInt32 startIndex;
      GetSelectedIndex(&startIndex);
      if (startIndex == kNothingSelected) {
        startIndex = 0;
      } else if (stringLength == 1) {
        startIndex++;
      }

      PRUint32 i;
      for (i = 0; i < numOptions; i++) {
        PRUint32 index = (i + startIndex) % numOptions;
        nsCOMPtr<nsIDOMHTMLOptionElement> optionElement(getter_AddRefs(GetOption(options, index)));
        if (optionElement) {
          nsAutoString text;
          if (NS_OK == optionElement->GetText(text)) {
            if (StringBeginsWith(text, incrementalString,
                                 nsCaseInsensitiveStringComparator())) {
              PRBool wasChanged = PerformSelection(index, isShift, isControl);
              if (wasChanged) {
                UpdateSelection(); // dispatch event, update combobox, etc.
              }
              break;
            }
          }
        }
      } // for

    } break;//case
  } // switch

  // We ate the key if we got this far.
  aKeyEvent->PreventDefault();

  // If we didn't do an incremental search, clear the string
  if (!didIncrementalSearch) {
    GetIncrementalString().Truncate();
  }

  // Actually process the new index and let the selection code
  // do the scrolling for us
  if (newIndex != kNothingSelected) {
    // If you hold control, no key will actually do anything except space.
    if (isControl && charcode != ' ') {
      mStartSelectionIndex = newIndex;
      mEndSelectionIndex = newIndex;
      ScrollToIndex(newIndex);
    } else {
      PRBool wasChanged = PerformSelection(newIndex, isShift, isControl);
      if (wasChanged) {
        UpdateSelection(); // dispatch event, update combobox, etc.
      }
    }

    // XXX - Are we cover up a problem here???
    // Why aren't they getting flushed each time?
    // because this isn't needed for Gfx
    if (IsInDropDownMode() == PR_TRUE) {
      // Don't flush anything but reflows lest it destroy us
      mPresContext->PresShell()->
        GetDocument()->FlushPendingNotifications(Flush_OnlyReflow);
    }
    REFLOW_DEBUG_MSG2("  After: %d\n", newIndex);

    // Make sure the SelectArea frame gets painted
    Invalidate(nsRect(0,0,mRect.width,mRect.height), PR_TRUE);

  } else {
    REFLOW_DEBUG_MSG("  After: SKIPPED it\n");
  }

  return NS_OK;
}


/******************************************************************************
 * nsListEventListener
 *****************************************************************************/

NS_IMPL_ADDREF(nsListEventListener)
NS_IMPL_RELEASE(nsListEventListener)
NS_INTERFACE_MAP_BEGIN(nsListEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMouseListener)
NS_INTERFACE_MAP_END

#define FORWARD_EVENT(_event) \
NS_IMETHODIMP \
nsListEventListener::_event(nsIDOMEvent* aEvent) \
{ \
  if (mFrame) \
    return mFrame->nsListControlFrame::_event(aEvent); \
  return NS_OK; \
}

#define IGNORE_EVENT(_event) \
NS_IMETHODIMP \
nsListEventListener::_event(nsIDOMEvent* aEvent) \
{ return NS_OK; }

IGNORE_EVENT(HandleEvent)

/*================== nsIDOMKeyListener =========================*/

IGNORE_EVENT(KeyDown)
IGNORE_EVENT(KeyUp)
FORWARD_EVENT(KeyPress)

/*=============== nsIDOMMouseListener ======================*/

FORWARD_EVENT(MouseDown)
FORWARD_EVENT(MouseUp)
IGNORE_EVENT(MouseClick)
IGNORE_EVENT(MouseDblClick)
IGNORE_EVENT(MouseOver)
IGNORE_EVENT(MouseOut)

/*=============== nsIDOMMouseMotionListener ======================*/

FORWARD_EVENT(MouseMove)
// XXXbryner does anyone call this, ever?
IGNORE_EVENT(DragMove)

#undef FORWARD_EVENT
#undef IGNORE_EVENT

/*=============== Timer Related Code ======================*/
nsresult
nsListControlFrame::StartUpdateTimer(nsIPresContext * aPresContext)
{

  if (mUpdateTimer == nsnull) {
    nsresult result = NS_NewUpdateTimer(&mUpdateTimer);
    if (NS_FAILED(result))
      return result;

    mUpdateTimer->Init(this, 0); // delay "0"
  }

  if (mUpdateTimer != nsnull) {
    return mUpdateTimer->Start(aPresContext);
  }

  return NS_ERROR_FAILURE;
}

void
nsListControlFrame::StopUpdateTimer()
{
  if (mUpdateTimer != nsnull) {
    mUpdateTimer->Stop();
  }
}

void
nsListControlFrame::ItemsHaveBeenRemoved(nsIPresContext * aPresContext)
{
  // Only adjust things if it is a combobox
  // removing items on a listbox should effect anything
  if (IsInDropDownMode()) {
    ResetList(aPresContext);
  }
}

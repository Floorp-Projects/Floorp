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
 * The Original Code is mozilla.org code.
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
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsComboboxControlFrame.h"
#include "nsIDOMEventReceiver.h"
#include "nsFrameManager.h"
#include "nsFormControlFrame.h"
#include "nsIHTMLContent.h"
#include "nsHTMLAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMElement.h"
#include "nsIListControlFrame.h"
#include "nsIDOMHTMLCollection.h" 
#include "nsIDOMHTMLSelectElement.h" 
#include "nsIDOMHTMLOptionElement.h" 
#include "nsIDOMNSHTMLOptionCollectn.h" 
#include "nsIPresShell.h"
#include "nsIPresState.h"
#include "nsIDeviceContext.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsIEventStateManager.h"
#include "nsIDOMNode.h"
#include "nsIPrivateDOMEvent.h"
#include "nsISupportsArray.h"
#include "nsISelectControlFrame.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollableView.h"
#include "nsListControlFrame.h"
#include "nsContentCID.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsGUIEvent.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"

static NS_DEFINE_CID(kTextNodeCID,   NS_TEXTNODE_CID);

#ifdef MOZ_XUL
#include "nsIXULDocument.h" // Temporary fix for Bug 36558
#endif

#ifdef DO_NEW_REFLOW
#include "nsIFontMetrics.h"
#endif


#define FIX_FOR_BUG_53259

// Drop down list event management.
// The combo box uses the following strategy for managing the drop-down list.
// If the combo box or it's arrow button is clicked on the drop-down list is displayed
// If mouse exit's the combo box with the drop-down list displayed the drop-down list
// is asked to capture events
// The drop-down list will capture all events including mouse down and up and will always
// return with ListWasSelected method call regardless of whether an item in the list was
// actually selected.
// The ListWasSelected code will turn off mouse-capture for the drop-down list.
// The drop-down list does not explicitly set capture when it is in the drop-down mode.


//XXX: This is temporary. It simulates psuedo states by using a attribute selector on 

const PRInt32 kSizeNotSet = -1;

/**
 * Helper class that listens to the combo boxes button. If the button is pressed the 
 * combo box is toggled to open or close. this is used by Accessibility which presses
 * that button Programmatically.
 */
class nsComboButtonListener: public nsIDOMMouseListener
{
  public:

  NS_DECL_ISUPPORTS
  NS_IMETHOD HandleEvent(nsIDOMEvent* anEvent) { return PR_FALSE; }
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent) { return PR_FALSE; }
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent) { return PR_FALSE; }
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { return PR_FALSE; }
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent) { return PR_FALSE; }
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent) { return PR_FALSE; }

  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent) 
  {
    PRBool isDroppedDown;
    mComboBox->IsDroppedDown(&isDroppedDown);
    mComboBox->ShowDropDown(!isDroppedDown);
    return PR_FALSE; 
  }

  nsComboButtonListener(nsComboboxControlFrame* aCombobox) 
  { 
    mComboBox = aCombobox; 
  }

  virtual ~nsComboButtonListener() {}

  nsComboboxControlFrame* mComboBox;
};

NS_IMPL_ISUPPORTS1(nsComboButtonListener, nsIDOMMouseListener)

// static class data member for Bug 32920
nsComboboxControlFrame * nsComboboxControlFrame::mFocused = nsnull;

nsresult
NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aStateFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsComboboxControlFrame* it = new (aPresShell) nsComboboxControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // set the state flags (if any are provided)
  it->AddStateBits(aStateFlags);
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

nsComboboxControlFrame::nsComboboxControlFrame()
  : nsAreaFrame() 
{
  mPresContext                 = nsnull;
  mListControlFrame            = nsnull;
  mDroppedDown                 = PR_FALSE;
  mDisplayFrame                = nsnull;
  mButtonFrame                 = nsnull;
  mDropdownFrame               = nsnull;

  mCacheSize.width             = kSizeNotSet;
  mCacheSize.height            = kSizeNotSet;
  mCachedAscent                = kSizeNotSet;
  mCachedMaxElementWidth       = kSizeNotSet;
  mCachedAvailableSize.width   = kSizeNotSet;
  mCachedAvailableSize.height  = kSizeNotSet;
  mCachedUncDropdownSize.width  = kSizeNotSet;
  mCachedUncDropdownSize.height = kSizeNotSet;
  mCachedUncComboSize.width    = kSizeNotSet;
  mCachedUncComboSize.height   = kSizeNotSet;
  mItemDisplayWidth             = 0;

  mGoodToGo = PR_FALSE;

  mRecentSelectedIndex = -1;

  //Shrink the area around it's contents
  //SetFlags(NS_BLOCK_SHRINK_WRAP);

  REFLOW_COUNTER_INIT()
}

//--------------------------------------------------------------
nsComboboxControlFrame::~nsComboboxControlFrame()
{
  REFLOW_COUNTER_DUMP("nsCCF");

  NS_IF_RELEASE(mPresContext);
}

//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsComboboxControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsIComboboxControlFrame))) {
    *aInstancePtr = (void*)(nsIComboboxControlFrame*)this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
    *aInstancePtr = (void*)(nsIFormControlFrame*)this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIAnonymousContentCreator))) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*)this;
    return NS_OK;   
  } else if (aIID.Equals(NS_GET_IID(nsISelectControlFrame))) {
    *aInstancePtr = (void *)(nsISelectControlFrame*)this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*)this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIRollupListener))) {
    *aInstancePtr = (void*)(nsIRollupListener*)this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIScrollableViewProvider))) {
    *aInstancePtr = (void*)(nsIScrollableViewProvider*)this;
    return NS_OK;
  } 
  
  return nsAreaFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsComboboxControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mContent);
    return accService->CreateHTMLComboboxAccessible(node, mPresContext, aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif



NS_IMETHODIMP
nsComboboxControlFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow)
{
   // Need to hold on the pres context because it is used later in methods
   // which don't have it passed in.
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);

  //-------------------------------
  // Start - Temporary fix for Bug 36558
  //-------------------------------
  mGoodToGo = PR_FALSE;
  nsIDocument* document = aContent->GetDocument();
  if (document) {
#ifdef MOZ_XUL
    nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(document));
    mGoodToGo = xulDoc?PR_FALSE:PR_TRUE;
#else
    mGoodToGo = PR_TRUE;
#endif
  }
  //-------------------------------
  // Done - Temporary fix for Bug 36558
  //-------------------------------
  
  return nsAreaFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

// Initialize the text string in the combobox using either the current
// selection in the list box or the first item item in the list box.
void 
nsComboboxControlFrame::InitTextStr()
{
  nsAutoString textToDisplay;
  PRInt32 selectedIndex;
  mListControlFrame->GetSelectedIndex(&selectedIndex);
  if (selectedIndex != -1) {
    mListControlFrame->GetOptionText(selectedIndex, textToDisplay);
  }

  mDisplayedIndex = selectedIndex;
  ActuallyDisplayText(textToDisplay, PR_FALSE);
}


//--------------------------------------------------------------
void 
nsComboboxControlFrame::InitializeControl(nsIPresContext* aPresContext)
{
  nsFormControlHelper::Reset(this, aPresContext);
}

//--------------------------------------------------------------
NS_IMETHODIMP_(PRInt32)
nsComboboxControlFrame::GetFormControlType() const
{
  return NS_FORM_SELECT;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFormContent(nsIContent*& aContent) const
{
  aContent = GetContent();
  NS_IF_ADDREF(aContent);
  return NS_OK;
}

//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return 0;
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return 0;
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                                 float aPixToTwip, 
                                                 nscoord aInnerHeight) const
{
   return 0;
}

//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}

void 
nsComboboxControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  if (aOn) {
    nsListControlFrame::ComboboxFocusSet();
    mFocused = this;
  } else {
    mFocused = nsnull;
    if (mDroppedDown) {
      mListControlFrame->ComboboxFinish(mDisplayedIndex);
    }
    mListControlFrame->FireOnChange();
  }

  // This is needed on a temporary basis. It causes the focus
  // rect to be drawn. This is much faster than ReResolvingStyle
  // Bug 32920
  Invalidate(nsRect(0,0,mRect.width,mRect.height), PR_TRUE);

  // Make sure the content area gets updated for where the dropdown was
  // This is only needed for embedding, the focus may go to 
  // the chrome that is not part of the Gecko system (Bug 83493)
  // XXX this is rather inefficient
  nsIViewManager* vm = GetPresContext()->GetViewManager();
  if (vm) {
    vm->UpdateAllViews(NS_VMREFRESH_NO_SYNC);
  }
}

void
nsComboboxControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsIPresShell *presShell = aPresContext->GetPresShell();
    if (presShell) {
      presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
  }
}


void
nsComboboxControlFrame::ShowPopup(PRBool aShowPopup)
{
  nsIView* view = mDropdownFrame->GetView();
  nsIViewManager* viewManager = view->GetViewManager();

  if (aShowPopup) {
    nsRect rect = mDropdownFrame->GetRect();
    rect.x = rect.y = 0;
    viewManager->ResizeView(view, rect);
    nsIScrollableView* scrollingView;
    if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {
      scrollingView->ComputeScrollOffsets(PR_TRUE);
    }
    viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
  } else {
    viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
    nsRect emptyRect(0, 0, 0, 0);
    viewManager->ResizeView(view, emptyRect);
  }

  // fire a popup dom event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(aShowPopup ? NS_XUL_POPUP_SHOWING : NS_XUL_POPUP_HIDING);

  nsIPresShell *shell = mPresContext->GetPresShell();
  if (shell) 
    shell->HandleDOMEventWithTarget(mContent, &event, &status);
}

// Show the dropdown list

void 
nsComboboxControlFrame::ShowList(nsIPresContext* aPresContext, PRBool aShowList)
{
  nsIWidget* widget = nsnull;

  // Get parent view
  nsIFrame * listFrame;
  if (NS_OK == mListControlFrame->QueryInterface(NS_GET_IID(nsIFrame), (void **)&listFrame)) {
    nsIView* view = listFrame->GetView();
    NS_ASSERTION(view, "nsComboboxControlFrame view is null");
    if (view) {
    	widget = view->GetWidget();
    }
  }

  if (PR_TRUE == aShowList) {
    ShowPopup(PR_TRUE);
    mDroppedDown = PR_TRUE;

     // The listcontrol frame will call back to the nsComboboxControlFrame's ListWasSelected
     // which will stop the capture.
    mListControlFrame->AboutToDropDown();
    mListControlFrame->CaptureMouseEvents(aPresContext, PR_TRUE);

  } else {
    ShowPopup(PR_FALSE);
    mDroppedDown = PR_FALSE;
  }

  // Don't flush anything but reflows lest it destroy us
  aPresContext->PresShell()->
    GetDocument()->FlushPendingNotifications(Flush_OnlyReflow);

  if (widget)
    widget->CaptureRollupEvents((nsIRollupListener *)this, mDroppedDown, aShowList);

}


//-------------------------------------------------------------
// this is in response to the MouseClick from the containing browse button
// XXX: TODO still need to get filters from accept attribute
void 
nsComboboxControlFrame::MouseClicked(nsIPresContext* aPresContext)
{
   //ToggleList(aPresContext);
}

nsresult
nsComboboxControlFrame::ReflowComboChildFrame(nsIFrame* aFrame, 
                                             nsIPresContext*  aPresContext, 
                                             nsHTMLReflowMetrics&     aDesiredSize,
                                             const nsHTMLReflowState& aReflowState, 
                                             nsReflowStatus&          aStatus,
                                             nscoord                  aAvailableWidth,
                                             nscoord                  aAvailableHeight)
{
   // Constrain the child's width and height to aAvailableWidth and aAvailableHeight
  nsSize availSize(aAvailableWidth, aAvailableHeight);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, aFrame,
                                   availSize);
  kidReflowState.mComputedWidth = aAvailableWidth;
  kidReflowState.mComputedHeight = aAvailableHeight;

  // ensure we start off hidden
  if (aReflowState.reason == eReflowReason_Initial) {
    nsIView* view = mDropdownFrame->GetView();
    nsIViewManager* viewManager = view->GetViewManager();
    viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
    nsRect emptyRect(0, 0, 0, 0);
    viewManager->ResizeView(view, emptyRect);
  }
  
   // Reflow child
  nsRect rect = aFrame->GetRect();
  nsresult rv = ReflowChild(aFrame, aPresContext, aDesiredSize, kidReflowState,
                            rect.x, rect.y, NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_VISIBILITY, aStatus);
 
   // Set the child's width and height to it's desired size
  FinishReflowChild(aFrame, aPresContext, &kidReflowState, aDesiredSize, 
                    rect.x, rect.y, NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_VISIBILITY);
  return rv;
}

// Suggest a size for the child frame. 
// Only frames which implement the nsIFormControlFrame interface and
// honor the SetSuggestedSize method will be placed and sized correctly.

void 
nsComboboxControlFrame::SetChildFrameSize(nsIFrame* aFrame, nscoord aWidth, nscoord aHeight) 
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = aFrame->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame);
  if (NS_SUCCEEDED(result) && (nsnull != fcFrame)) {
    fcFrame->SetSuggestedSize(aWidth, aHeight); 
  }
}

nsresult 
nsComboboxControlFrame::GetPrimaryComboFrame(nsIPresContext* aPresContext, nsIContent* aContent, nsIFrame** aFrame)
{
   // Get the primary frame from the presentation shell.
  nsIPresShell *presShell = aPresContext->GetPresShell();
  if (presShell) {
    presShell->GetPrimaryFrameFor(aContent, aFrame);
  }
  return NS_OK;
}

nsresult 
nsComboboxControlFrame::PositionDropdown(nsIPresContext* aPresContext, 
                                         nscoord aHeight, 
                                         nsRect aAbsoluteTwipsRect, 
                                         nsRect aAbsolutePixelRect)
{
   // Position the dropdown list. It is positioned below the display frame if there is enough
   // room on the screen to display the entire list. Otherwise it is placed above the display
   // frame.

   // Note: As first glance, it appears that you could simply get the absolute bounding box for the
   // dropdown list by first getting it's view, then getting the view's nsIWidget, then asking the nsIWidget
   // for it's AbsoluteBounds. The problem with this approach, is that the dropdown lists y location can
   // change based on whether the dropdown is placed below or above the display frame.
   // The approach, taken here is to get use the absolute position of the display frame and use it's location
   // to determine if the dropdown will go offscreen.

   // Use the height calculated for the area frame so it includes both
   // the display and button heights.
  nsresult rv = NS_OK;
  nscoord dropdownYOffset = aHeight;
// XXX: Enable this code to debug popping up above the display frame, rather than below it
  nsRect dropdownRect = mDropdownFrame->GetRect();

  nscoord screenHeightInPixels = 0;
  if (NS_SUCCEEDED(nsFormControlFrame::GetScreenHeight(aPresContext, screenHeightInPixels))) {
     // Get the height of the dropdown list in pixels.
     float t2p;
     t2p = aPresContext->TwipsToPixels();
     nscoord absoluteDropDownHeight = NSTwipsToIntPixels(dropdownRect.height, t2p);
    
      // Check to see if the drop-down list will go offscreen
    if (NS_SUCCEEDED(rv) && ((aAbsolutePixelRect.y + aAbsolutePixelRect.height + absoluteDropDownHeight) > screenHeightInPixels)) {
      // move the dropdown list up
      dropdownYOffset = - (dropdownRect.height);
    }
  }
 
  dropdownRect.x = 0;
  dropdownRect.y = dropdownYOffset; 

  mDropdownFrame->SetRect(dropdownRect);
  return rv;
}


////////////////////////////////////////////////////////////////
// Experimental Reflow
////////////////////////////////////////////////////////////////
#if defined(DO_NEW_REFLOW) || defined(DO_REFLOW_COUNTER)
//---------------------------------------------------------
// Returns the nsIDOMHTMLOptionElement for a given index 
// in the select's collection
//---------------------------------------------------------
static nsIDOMHTMLOptionElement* 
GetOption(nsIDOMHTMLCollection& aCollection, PRInt32 aIndex)
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
// for a given piece of content it returns nsIDOMHTMLSelectElement object
// or null 
//---------------------------------------------------------
static nsIDOMHTMLSelectElement* 
GetSelect(nsIContent * aContent)
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
//---------------------------------------------------------
// This returns the collection for nsIDOMHTMLSelectElement or
// the nsIContent object is the select is null  (AddRefs)
//---------------------------------------------------------
static nsIDOMHTMLCollection* 
GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect = nsnull)
{
  nsIDOMHTMLCollection* options = nsnull;
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

#ifdef DO_NEW_REFLOW
NS_IMETHODIMP 
nsComboboxControlFrame::ReflowItems(nsIPresContext* aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics& aDesiredSize) 
{
  //printf("*****************\n");
  nscoord visibleHeight = 0;
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult res = nsFormControlHelper::GetFrameFontFM(mDisplayFrame, getter_AddRefs(fontMet));
  if (fontMet) {
    fontMet->GetHeight(visibleHeight);
  }
 
  nsAutoString maxStr;
  nscoord maxWidth = 0;
  //nsIRenderingContext * rc = aReflowState.rendContext;
  nsresult rv = NS_ERROR_FAILURE; 
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    //printf("--- Num of Items %d ---\n", numOptions);
    for (PRUint32 i=0;i<numOptions;i++) {
      nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = getter_AddRefs(GetOption(*options, i));
      if (optionElement) {
        nsAutoString text;
        rv = optionElement->GetLabel(text);
        if (NS_CONTENT_ATTR_HAS_VALUE != rv || text.IsEmpty()) {
          if (NS_OK == optionElement->GetText(text)) {
            nscoord width;
            aReflowState.rendContext->GetWidth(text, width);
            if (width > maxWidth) {
              maxStr = text;
              maxWidth = width;
            }
            //maxWidth = PR_MAX(width, maxWidth);
            //printf("[%d] - %d %s \n", i, width, NS_LossyConvertUCS2toASCII(text).get());
          }
        }          
      }
    }
  }
  if (maxWidth == 0) {
    maxWidth = 11 * 15;
  }
  char * str = ToNewCString(maxStr);
  printf("id: %d maxWidth %d [%s]\n", mReflowId, maxWidth, str);
  delete [] str;

  // get the borderPadding for the display area
  nsMargin dspBorderPadding(0, 0, 0, 0);
  mDisplayFrame->CalcBorderPadding(dspBorderPadding);

  nscoord frmWidth  = maxWidth+dspBorderPadding.left+dspBorderPadding.right+
                      aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
  nscoord frmHeight = visibleHeight+dspBorderPadding.top+dspBorderPadding.bottom+
                      aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;

#if 0
  aDesiredSize.width  = frmWidth;
  aDesiredSize.height = frmHeight;
#else
  printf("Size frm:%d,%d   DS:%d,%d   DIF:%d,%d(tp)  %d,%d(px)\n", 
         frmWidth, frmHeight, 
         aDesiredSize.width, aDesiredSize.height,
         frmWidth-aDesiredSize.width, frmHeight-aDesiredSize.height,
         (frmWidth-aDesiredSize.width)/15, (frmHeight-aDesiredSize.height)/15);
#endif
  return NS_OK;
}
#endif

#endif

//------------------------------------------------------------------
// This Method reflow just the contents of the ComboBox
// The contents are a Block frame containing a Text Frame - This is the display area
// and then the GfxButton - The dropdown button
//--------------------------------------------------------------------------
void 
nsComboboxControlFrame::ReflowCombobox(nsIPresContext *         aPresContext,
                                           const nsHTMLReflowState& aReflowState,
                                           nsHTMLReflowMetrics&     aDesiredSize,
                                           nsReflowStatus&          aStatus,
                                           nsIFrame *               aDisplayFrame,
                                           nsIFrame *               aDropDownBtn,
                                           nscoord&                 aDisplayWidth,
                                           nscoord                  aBtnWidth,
                                           const nsMargin&          aBorderPadding,
                                           nscoord                  aFallBackHgt,
                                           PRBool                   aCheckHeight)
{
  // start out by using the cached height
  // XXX later this will change when we better handle constrained height 
  nscoord dispHeight = mCacheSize.height - aBorderPadding.top - aBorderPadding.bottom;
  nscoord dispWidth  = aDisplayWidth;

  REFLOW_NOISY_MSG3("+++1 AdjustCombo DW:%d DH:%d  ", PX(dispWidth), PX(dispHeight));
  REFLOW_NOISY_MSG3("BW:%d  BH:%d  ", PX(aBtnWidth), PX(dispHeight));
  REFLOW_NOISY_MSG3("mCacheSize.height:%d - %d\n", PX(mCacheSize.height), PX((aBorderPadding.top + aBorderPadding.bottom)));

  // get the border and padding for the DisplayArea (block frame & textframe)
  nsMargin dspBorderPadding(0, 0, 0, 0);
  mDisplayFrame->CalcBorderPadding(dspBorderPadding);

  // adjust the height
  if (mCacheSize.height == kSizeNotSet) {
    if (aFallBackHgt == kSizeNotSet) {
      NS_ASSERTION(aFallBackHgt != kSizeNotSet, "Fallback can't be kSizeNotSet when mCacheSize.height == kSizeNotSet");
    } else {
      dispHeight = aFallBackHgt;
      REFLOW_NOISY_MSG2("+++3 Adding (dspBorderPadding.top + dspBorderPadding.bottom): %d\n", (dspBorderPadding.top + dspBorderPadding.bottom));
      dispHeight += (dspBorderPadding.top + dspBorderPadding.bottom);
    }
  }

  // Fix for Bug 58220 (part of it)
  // make sure we size correctly if the CSS width is set to something really small like 0, 1, or 2 pixels
  nscoord computedWidth = aReflowState.mComputedWidth + aBorderPadding.left + aBorderPadding.right;
  if ((aReflowState.mComputedWidth != NS_UNCONSTRAINEDSIZE && computedWidth <= 0) || aReflowState.mComputedWidth == 0) {
    nsRect buttonRect(0,0,0,0);
    nsRect displayRect(0,0,0,0);
    aBtnWidth = 0;
    aDisplayFrame->SetRect(displayRect);
    aDropDownBtn->SetRect(buttonRect);
    SetChildFrameSize(aDropDownBtn, aBtnWidth, aDesiredSize.height);
    aDesiredSize.width = 0;
    aDesiredSize.height = dispHeight + aBorderPadding.top + aBorderPadding.bottom;
    // XXX What about ascent and descent?
    return;
  }

  REFLOW_NOISY_MSG3("+++2 AdjustCombo DW:%d DH:%d  ", PX(dispWidth), PX(dispHeight));
  REFLOW_NOISY_MSG3(" BW:%d  BH:%d\n", PX(aBtnWidth), PX(dispHeight));

  // This sets the button to be a specific size
  // so no matter what it reflows at these values
  SetChildFrameSize(aDropDownBtn, aBtnWidth, dispHeight);

#ifdef FIX_FOR_BUG_53259
  // Make sure we obey min/max-width and min/max-height
  if (dispWidth > aReflowState.mComputedMaxWidth) {
    dispWidth = aReflowState.mComputedMaxWidth - aBorderPadding.left - aBorderPadding.right;
  }
  if (dispWidth < aReflowState.mComputedMinWidth) {
    dispWidth = aReflowState.mComputedMinWidth - aBorderPadding.left - aBorderPadding.right;
  }

  if (dispHeight > aReflowState.mComputedMaxHeight) {
    dispHeight = aReflowState.mComputedMaxHeight - aBorderPadding.top - aBorderPadding.bottom;
  }
  if (dispHeight < aReflowState.mComputedMinHeight) {
    dispHeight = aReflowState.mComputedMinHeight - aBorderPadding.top - aBorderPadding.bottom;
  }
#endif

  // Make sure we get the reflow reason right. If an incremental
  // reflow arrives that's targeted directly at the top-level combobox
  // frame, then we can't pass it down to the children ``as is'':
  // we're the last frame in the reflow command's chain. So, convert
  // it to a resize reflow.
  nsReflowReason reason = aReflowState.reason;
  if (reason == eReflowReason_Incremental) {
    if (aReflowState.path->mReflowCommand)
      reason = eReflowReason_Resize;
  }

  // now that we know what the overall display width & height will be
  // set up a new reflow state and reflow the area frame at that size
  nsSize availSize(dispWidth + aBorderPadding.left + aBorderPadding.right, 
                   dispHeight + aBorderPadding.top + aBorderPadding.bottom);
  nsHTMLReflowState kidReflowState(aReflowState);
  kidReflowState.availableWidth  = availSize.width;
  kidReflowState.availableHeight = availSize.height;
  kidReflowState.mComputedWidth  = dispWidth;
  kidReflowState.mComputedHeight = dispHeight;
  kidReflowState.reason          = reason;

#ifdef IBMBIDI
  const nsStyleVisibility* vis = GetStyleVisibility();

  // M14 didn't calculate the RightEdge in the reflow
  // Unless we set the width to some thing other than unrestricted
  // the code changed this may not be the best place to put it
  // in this->Reflow like this :
  //
  // Reflow display + button
  // nsAreaFrame::Reflow(aPresContext, aDesiredSize, firstPassState, aStatus);

  if (vis->mDirection == NS_STYLE_DIRECTION_RTL)
  {
    kidReflowState.mComputedWidth = 0;
  }
#endif // IBMBIDI

  // do reflow
  nsAreaFrame::Reflow(aPresContext, aDesiredSize, kidReflowState, aStatus);

  /////////////////////////////////////////////////////////
  // The DisplayFrame is a Block frame containing a TextFrame
  // and it is completely anonymous, so we must manually reflow it
  nsHTMLReflowMetrics txtKidSize(PR_TRUE);
  nsSize txtAvailSize(dispWidth - aBtnWidth, dispHeight);
  nsHTMLReflowState   txtKidReflowState(aPresContext, aReflowState, aDisplayFrame, txtAvailSize, reason);

  aDisplayFrame->WillReflow(aPresContext);
  //aDisplayFrame->SetPosition(nsPoint(dspBorderPadding.left + aBorderPadding.left, dspBorderPadding.top + aBorderPadding.top));
  aDisplayFrame->SetPosition(nsPoint(aBorderPadding.left, aBorderPadding.top));
  nsAreaFrame::PositionFrameView(aPresContext, aDisplayFrame);
  nsReflowStatus status;
  nsresult rv = aDisplayFrame->Reflow(aPresContext, txtKidSize, txtKidReflowState, status);
  if (NS_FAILED(rv)) return;

  /////////////////////////////////////////////////////////
  // If we are Constrained then the AreaFrame Reflow is the correct size
  // if we are unconstrained then 
  //if (aReflowState.mComputedWidth == NS_UNCONSTRAINEDSIZE) {
  //  aDesiredSize.width += txtKidSize.width;
  //}

  // Apparently, XUL lays out differently than HTML 
  // (the code above works for HTML and not XUL), 
  // so instead of using the above calculation
  // I just set it to what it should be.
  aDesiredSize.width = availSize.width;
  //aDesiredSize.height = availSize.height;

  // now we need to adjust layout, because the AreaFrame
  // doesn't position things exactly where we want them
  nscoord insideHeight = aDesiredSize.height - aBorderPadding.top - aBorderPadding.bottom;

  // If the css width has been set to something very small
  //i.e. smaller than the dropdown button, set the button's width to zero
  if (aBtnWidth > dispWidth) {
    aBtnWidth = 0;
  }
  // set the display rect to be left justifed and 
  // fills the entire area except the button
  nscoord x = aBorderPadding.left;
  nsRect displayRect(x, aBorderPadding.top, PR_MAX(dispWidth - aBtnWidth, 0), insideHeight);
  aDisplayFrame->SetRect(displayRect);
  x += displayRect.width;

  // right justify the button
  nsRect buttonRect(x, aBorderPadding.top, aBtnWidth, insideHeight);
#ifdef IBMBIDI
  if (vis->mDirection == NS_STYLE_DIRECTION_RTL)
  {
    if (buttonRect.x > displayRect.x)
    {
      buttonRect.x = displayRect.x;
      displayRect.x += buttonRect.width;
      aDisplayFrame->SetRect(displayRect);
    }
  }
#endif // IBMBIDI
  aDropDownBtn->SetRect(buttonRect);

  // since we have changed the height of the button 
  // make sure it has these new values
  SetChildFrameSize(aDropDownBtn, aBtnWidth, aDesiredSize.height);
  
  // This is a last minute adjustment, if the CSS width was set and 
  // we calculated it to be a little big, then make sure we are no bigger the computed size
  // this only comes into play when the css width has been set to something smaller than
  // the dropdown arrow
  if (aReflowState.mComputedWidth != NS_UNCONSTRAINEDSIZE && aDesiredSize.width > computedWidth) {
    aDesiredSize.width = computedWidth;
  }

  REFLOW_NOISY_MSG3("**AdjustCombobox - Reflow: WW: %d  HH: %d\n", aDesiredSize.width, aDesiredSize.height);

  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = aDesiredSize.width;
  }

  aDesiredSize.ascent =
    txtKidSize.ascent + aReflowState.mComputedBorderPadding.top;
  aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;
  
  // Now cache the available height as our height without border and padding
  // This sets up the optimization for if a new available width comes in and we are equal or
  // less than it we can bail
  if (aDesiredSize.width != mCacheSize.width || aDesiredSize.height != mCacheSize.height) {
    if (aReflowState.availableWidth != NS_UNCONSTRAINEDSIZE) {
      mCachedAvailableSize.width  = aDesiredSize.width - (aBorderPadding.left + aBorderPadding.right);
    }
    if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
      mCachedAvailableSize.height = aDesiredSize.height - (aBorderPadding.top + aBorderPadding.bottom);
    }
    nsFormControlFrame::SetupCachedSizes(mCacheSize, mCachedAscent,
                                         mCachedMaxElementWidth, aDesiredSize);
  }

  ///////////////////////////////////////////////////////////////////
  // This is an experimental reflow that is turned off in the build
#ifdef DO_NEW_REFLOW
  ReflowItems(aPresContext, aReflowState, aDesiredSize);
#endif
  ///////////////////////////////////////////////////////////////////
}

//----------------------------------------------------------
// 
//----------------------------------------------------------
#ifdef DO_REFLOW_DEBUG
static int myCounter = 0;

static void printSize(char * aDesc, nscoord aSize) 
{
  printf(" %s: ", aDesc);
  if (aSize == NS_UNCONSTRAINEDSIZE) {
    printf("UC");
  } else {
    printf("%d", PX(aSize));
  }
}
#endif

//-------------------------------------------------------------------
//-- Main Reflow for the Combobox
//-------------------------------------------------------------------
NS_IMETHODIMP 
nsComboboxControlFrame::Reflow(nsIPresContext*          aPresContext, 
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState, 
                               nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsComboboxControlFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  aStatus = NS_FRAME_COMPLETE;

  REFLOW_COUNTER_REQUEST();

#ifdef DO_REFLOW_DEBUG
  printf("-------------Starting Combobox Reflow ----------------------------\n");
  printf("%p ** Id: %d nsCCF::Reflow %d R: ", this, mReflowId, myCounter++);
  switch (aReflowState.reason) {
    case eReflowReason_Initial:
      printf("Ini");break;
    case eReflowReason_Incremental:
      printf("Inc");break;
    case eReflowReason_Resize:
      printf("Rsz");break;
    case eReflowReason_StyleChange:
      printf("Sty");break;
    case eReflowReason_Dirty:
      printf("Drt ");
      break;
    default:printf("<unknown>%d", aReflowState.reason);break;
  }
  
  printSize("AW", aReflowState.availableWidth);
  printSize("AH", aReflowState.availableHeight);
  printSize("CW", aReflowState.mComputedWidth);
  printSize("CH", aReflowState.mComputedHeight);

  nsCOMPtr<nsIDOMHTMLCollection> optionsTemp = getter_AddRefs(GetOptions(mContent));
  PRUint32 numOptions;
  optionsTemp->GetLength(&numOptions);
  printSize("NO", (nscoord)numOptions);

  printf(" *\n");

#endif


  PRBool bailOnWidth;
  PRBool bailOnHeight;

  // Do initial check to see if we can bail out
  // If it is an Initial or Incremental Reflow we never bail out here
  // XXX right now we only bail if the width meets the criteria
  //
  // We bail:
  //   if mComputedWidth == NS_UNCONSTRAINEDSIZE and
  //      availableWidth == NS_UNCONSTRAINEDSIZE and 
  //      we have cached an available size
  //
  // We bail:
  //   if mComputedWidth == NS_UNCONSTRAINEDSIZE and
  //      availableWidth != NS_UNCONSTRAINEDSIZE and 
  //      availableWidth minus its border equals our cached available size
  //
  // We bail:
  //   if mComputedWidth != NS_UNCONSTRAINEDSIZE and
  //      cached availableSize.width == aReflowState.mComputedWidth and 
  //      cached AvailableSize.width == aCacheSize.width
  //
  // NOTE: this returns whether we are doing an Incremental reflow
  nsFormControlFrame::SkipResizeReflow(mCacheSize,
                                       mCachedAscent,
                                       mCachedMaxElementWidth,
                                       mCachedAvailableSize, 
                                       aDesiredSize, aReflowState, 
                                       aStatus, 
                                       bailOnWidth, bailOnHeight);
  if (bailOnWidth) {
#ifdef DO_REFLOW_DEBUG // check or size
    nsMargin borderPadding(0, 0, 0, 0);
    CalcBorderPadding(borderPadding);
    UNCONSTRAINED_CHECK();
#endif
    REFLOW_DEBUG_MSG3("^** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
    NS_ASSERTION(aDesiredSize.width != kSizeNotSet,  "aDesiredSize.width != kSizeNotSet");
    NS_ASSERTION(aDesiredSize.height != kSizeNotSet, "aDesiredSize.height != kSizeNotSet");
    aDesiredSize.mOverflowArea.x      = 0;
    aDesiredSize.mOverflowArea.y      = 0;
    aDesiredSize.mOverflowArea.width  = aDesiredSize.width;
    aDesiredSize.mOverflowArea.height = aDesiredSize.height;
    return NS_OK;
  }

  if (eReflowReason_Initial == aReflowState.reason) {
    if (NS_FAILED(CreateDisplayFrame(aPresContext))) {
      return NS_ERROR_FAILURE;
    }
  }

  // Go get all of the important frame
  nsresult rv = NS_OK;
  // Don't try to do any special sizing and positioning unless all of the frames
  // have been created.
  if ((nsnull == mDisplayFrame) ||
     (nsnull == mButtonFrame) ||
     (nsnull == mDropdownFrame)) 
  {
     // Since combobox frames are missing just do a normal area frame reflow
    return nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }

  // We should cache this instead getting it everytime
  // the default size of the of scrollbar
  // that will be the default width of the dropdown button
  // the height will be the height of the text
  float w, h;
  // Get the width in Device pixels times p2t
  aPresContext->DeviceContext()->GetScrollBarDimensions(w, h);
  nscoord scrollbarWidth = NSToCoordRound(w);
  
  // set up a new reflow state for use throughout
  nsHTMLReflowState firstPassState(aReflowState);
  nsHTMLReflowMetrics dropdownDesiredSize(nsnull);

  // Check to see if this a fully unconstrained reflow
  PRBool fullyUnconstrained = firstPassState.mComputedWidth == NS_UNCONSTRAINEDSIZE && 
                              firstPassState.mComputedHeight == NS_UNCONSTRAINEDSIZE;

  PRBool forceReflow = PR_FALSE;

  // Only reflow the display and button 
  // if they are the target of the incremental reflow, unless they change size. 
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsHTMLReflowCommand *command = firstPassState.path->mReflowCommand;

    // Check to see if we are the target of the Incremental Reflow
    if (command) {
      // We need to check here to see if we can get away with just reflowing
      // the combobox and not the dropdown
      REFLOW_DEBUG_MSG("-----------------Target is Combobox------------\n");

      // If the mComputedWidth matches our cached display width 
      // then we get away with bailing out
      PRBool doFullReflow = firstPassState.mComputedWidth != NS_UNCONSTRAINEDSIZE &&
                            firstPassState.mComputedWidth != mItemDisplayWidth;
      if (!doFullReflow) {
        // OK, so we got lucky and the size didn't change
        // so do a simple reflow and bail out
        REFLOW_DEBUG_MSG("------------Reflowing AreaFrame and bailing----\n\n");
        ReflowCombobox(aPresContext, firstPassState, aDesiredSize, aStatus, 
                           mDisplayFrame, mButtonFrame, mItemDisplayWidth, 
                           scrollbarWidth, aReflowState.mComputedBorderPadding);
        REFLOW_COUNTER();
        UNCONSTRAINED_CHECK();
        REFLOW_DEBUG_MSG3("&** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
        NS_ASSERTION(aDesiredSize.width != kSizeNotSet,  "aDesiredSize.width != kSizeNotSet");
        NS_ASSERTION(aDesiredSize.height != kSizeNotSet, "aDesiredSize.height != kSizeNotSet");
        aDesiredSize.mOverflowArea.x      = 0;
        aDesiredSize.mOverflowArea.y      = 0;
        aDesiredSize.mOverflowArea.width  = aDesiredSize.width;
        aDesiredSize.mOverflowArea.height = aDesiredSize.height;
      }
      else {
        // Nope, something changed that affected our size 
        // so we need to do a full reflow and resize ourself
        REFLOW_DEBUG_MSG("------------Do Full Reflow----\n\n");
        firstPassState.reason = eReflowReason_StyleChange;
        firstPassState.path = nsnull;
        forceReflow = PR_TRUE;
      }
    }

    // See if any of the children are targets, as well.
    nsReflowPath::iterator iter = aReflowState.path->FirstChild();
    nsReflowPath::iterator end = aReflowState.path->EndChildren();
    for ( ; iter != end; ++iter) {
      // Now, see if our target is the dropdown
      // If so, maybe an items was added or some style changed etc.
      //               OR
      // We get an Incremental reflow on the dropdown when it is being 
      // shown or hidden.
      if (*iter == mDropdownFrame) {
        REFLOW_DEBUG_MSG("---------Target is Dropdown (Clearing Unc DD Size)---\n");
        // Nope, we were unlucky so now we do a full reflow
        mCachedUncDropdownSize.width  = kSizeNotSet;
        mCachedUncDropdownSize.height = kSizeNotSet;       
        REFLOW_DEBUG_MSG("---- Doing Full Reflow\n");
        // This is an incremental reflow targeted at the dropdown list
        // and it didn't have anything to do with being show or hidden.
        // 
        // The incremental reflow will not get to the dropdown list 
        // because it is in the "popup" list 
        // when this flow of control drops out of this if it will do a reflow
        // on the AreaFrame which SHOULD make it get tothe drop down 
        // except that it is in the popup list, so we have it reflowed as
        // a StyleChange, this is not as effecient as doing an Incremental
        //
        // At this point we want to by pass the reflow optimization in the dropdown
        // because we aren't why it is getting an incremental reflow, but we do
        // know that it needs to be resized or restyled
        //mListControlFrame->SetOverrideReflowOptimization(PR_TRUE);

      } else if (*iter == mDisplayFrame || *iter == mButtonFrame) {
        REFLOW_DEBUG_MSG2("-----------------Target is %s------------\n", (*iter == mDisplayFrame?"DisplayItem Frame":"DropDown Btn Frame"));
        // The incremental reflow is targeted at either the block or the button
        REFLOW_DEBUG_MSG("---- Doing AreaFrame Reflow and then bailing out\n");
        // Do simple reflow and bail out
        ReflowCombobox(aPresContext, firstPassState, aDesiredSize, aStatus, 
                       mDisplayFrame, mButtonFrame, 
                       mItemDisplayWidth, scrollbarWidth,
                       aReflowState.mComputedBorderPadding,
                       kSizeNotSet, PR_TRUE);
        REFLOW_DEBUG_MSG3("+** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
        REFLOW_COUNTER();
        UNCONSTRAINED_CHECK();
        NS_ASSERTION(aDesiredSize.width != kSizeNotSet,  "aDesiredSize.width != kSizeNotSet");
        NS_ASSERTION(aDesiredSize.height != kSizeNotSet, "aDesiredSize.height != kSizeNotSet");
        aDesiredSize.mOverflowArea.x      = 0;
        aDesiredSize.mOverflowArea.y      = 0;
        aDesiredSize.mOverflowArea.width  = aDesiredSize.width;
        aDesiredSize.mOverflowArea.height = aDesiredSize.height;
        continue;
      } else {
        nsIFrame * plainLstFrame;
        if (NS_SUCCEEDED(mListControlFrame->QueryInterface(NS_GET_IID(nsIFrame), (void**)&plainLstFrame))) {
          nsIFrame * frame = plainLstFrame->GetFirstChild(nsnull);
          nsIScrollableFrame * scrollFrame;
          if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsIScrollableFrame), (void**)&scrollFrame))) {
            plainLstFrame->Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

            aDesiredSize.width  = mCacheSize.width;
            aDesiredSize.height = mCacheSize.height;
            aDesiredSize.ascent = mCachedAscent;
            aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;

            if (aDesiredSize.mComputeMEW) {
              aDesiredSize.mMaxElementWidth = mCachedMaxElementWidth;
            }
            NS_ASSERTION(aDesiredSize.width != kSizeNotSet,  "aDesiredSize.width != kSizeNotSet");
            NS_ASSERTION(aDesiredSize.height != kSizeNotSet, "aDesiredSize.height != kSizeNotSet");
            aDesiredSize.mOverflowArea.x      = 0;
            aDesiredSize.mOverflowArea.y      = 0;
            aDesiredSize.mOverflowArea.width  = aDesiredSize.width;
            aDesiredSize.mOverflowArea.height = aDesiredSize.height;
            continue;
          }
        }

        // Here the target of the reflow was a child of the dropdown list
        // so we must do a full reflow
        REFLOW_DEBUG_MSG("-----------------Target is Dropdown's Child (Option Item)------------\n");
        REFLOW_DEBUG_MSG("---- Doing Reflow as StyleChange\n");
      }
      firstPassState.reason = eReflowReason_StyleChange;
      firstPassState.path = nsnull;
      mListControlFrame->SetOverrideReflowOptimization(PR_TRUE);
      forceReflow = PR_TRUE;
    }
  }

#ifdef IBMBIDI
  else if (eReflowReason_StyleChange == aReflowState.reason) {
    forceReflow = PR_TRUE;
  }
#endif // IBMBIDI

  // Here is another special optimization
  // Only reflow the dropdown if it has never been reflowed unconstrained
  //
  // Or someone up above here may want to force it to be reflowed 
  // by setting one or both of these to kSizeNotSet
  if ((mCachedUncDropdownSize.width == kSizeNotSet && 
       mCachedUncDropdownSize.height == kSizeNotSet) || forceReflow) {
    REFLOW_DEBUG_MSG3("---Re %d,%d\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 

    // Tell it we are doing the first pass, which means it will
    // do the unconstained reflow and skip the second reflow this time around
    nsListControlFrame * lcf = NS_STATIC_CAST(nsListControlFrame*, mDropdownFrame);
    lcf->SetPassId(1);
    // A width has not been specified for the select so size the display area
    // to match the width of the longest item in the drop-down list. The dropdown
    // list has already been reflowed and sized to shrink around its contents above.
    ReflowComboChildFrame(mDropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, 
                          aStatus, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); 
    lcf->SetPassId(0); // reset it back

    if (forceReflow) {
      mCachedUncDropdownSize.width  = dropdownDesiredSize.width;
      mCachedUncDropdownSize.height = dropdownDesiredSize.height;
    }
  } else {
    // Here we pretended we did an unconstrained reflow
    // so we set the cached values and continue on
    REFLOW_DEBUG_MSG3("--- Using Cached ListBox Size %d,%d\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 
    dropdownDesiredSize.width  = mCachedUncDropdownSize.width;
    dropdownDesiredSize.height = mCachedUncDropdownSize.height;
  }

  /////////////////////////////////////////////////////////////////////////
  // XXX - I need to clean this nect part up a little it is very redundant

  // Check here to if this is a mComputed unconstrained reflow
  PRBool computedUnconstrained = firstPassState.mComputedWidth == NS_UNCONSTRAINEDSIZE && 
                                 firstPassState.mComputedHeight == NS_UNCONSTRAINEDSIZE;
  if (computedUnconstrained && !forceReflow) {
    // Because Incremental reflows aren't actually getting to the dropdown
    // we cache the size from when it did a fully unconstrained reflow
    // we then check to see if the size changed at all, 
    // if not then bail out we don't need to worry
    if (mCachedUncDropdownSize.width == kSizeNotSet && mCachedUncDropdownSize.height == kSizeNotSet) {
      mCachedUncDropdownSize.width  = dropdownDesiredSize.width;
      mCachedUncDropdownSize.height = dropdownDesiredSize.height;
      REFLOW_DEBUG_MSG3("---1 Caching mCachedUncDropdownSize %d,%d\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 

    } else if (mCachedUncDropdownSize.width == dropdownDesiredSize.width &&
               mCachedUncDropdownSize.height == dropdownDesiredSize.height) {

      if (mCachedUncComboSize.width != kSizeNotSet && mCachedUncComboSize.height != kSizeNotSet) {
        REFLOW_DEBUG_MSG3("--- Bailing because of mCachedUncDropdownSize %d,%d\n\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 
        aDesiredSize.width  = mCachedUncComboSize.width;
        aDesiredSize.height = mCachedUncComboSize.height;
        aDesiredSize.ascent = mCachedAscent;
        aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;

        if (aDesiredSize.mComputeMEW) {
          aDesiredSize.mMaxElementWidth = mCachedMaxElementWidth;
        }
        UNCONSTRAINED_CHECK();
        REFLOW_DEBUG_MSG3("#** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
        NS_ASSERTION(aDesiredSize.width != kSizeNotSet,  "aDesiredSize.width != kSizeNotSet");
        NS_ASSERTION(aDesiredSize.height != kSizeNotSet, "aDesiredSize.height != kSizeNotSet");
        aDesiredSize.mOverflowArea.x      = 0;
        aDesiredSize.mOverflowArea.y      = 0;
        aDesiredSize.mOverflowArea.width  = aDesiredSize.width;
        aDesiredSize.mOverflowArea.height = aDesiredSize.height;
        return NS_OK;
      }
    } else {
      mCachedUncDropdownSize.width  = dropdownDesiredSize.width;
      mCachedUncDropdownSize.height = dropdownDesiredSize.height;
    }
  }
  // clean up stops here
  /////////////////////////////////////////////////////////////////////////

  // So this point we know we flowed the dropdown unconstrained
  // now we get to figure out how big we need to be and 
  // 
  // We don't reflow the combobox here at the new size
  // we cache its new size and reflow it on the dropdown
  nsSize size;
  PRInt32 length = 0;
  mListControlFrame->GetNumberOfOptions(&length);

  // dropdownRect will hold the content size (minus border padding) 
  // for the display area
  nsRect dropdownRect = mDropdownFrame->GetRect();
  if (eReflowReason_Resize == aReflowState.reason) {
    dropdownRect.Deflate(aReflowState.mComputedBorderPadding);
  }

  // Get maximum size of the largest item in the dropdown
  // The height of the display frame will be that height
  // the width will be the same as 
  // the dropdown width (minus its borderPadding) OR
  // a caculation off the mComputedWidth from reflow
  mListControlFrame->GetMaximumSize(size);

  // the variable "size" will now be 
  // the default size of the dropdown btn
  if (scrollbarWidth > 0) {
    size.width = scrollbarWidth;
  }

  // Get the border and padding for the dropdown
  nsMargin dropBorderPadding(0, 0, 0, 0);
  mDropdownFrame->CalcBorderPadding(dropBorderPadding);

  // get the borderPadding for the display area
  nsMargin dspBorderPadding(0, 0, 0, 0);
  mDisplayFrame->CalcBorderPadding(dspBorderPadding);

  // Substract dropdown's borderPadding from the width of the dropdown rect
  // to get the size of the content area
  //
  // the height will come from the mDisplayFrame's height
  // declare a size for the item display frame

  //Set the desired size for the button and display frame
  if (NS_UNCONSTRAINEDSIZE == firstPassState.mComputedWidth) {
    REFLOW_DEBUG_MSG("Unconstrained.....\n");
    REFLOW_DEBUG_MSG4("*B mItemDisplayWidth %d  dropdownRect.width:%d dropdownRect.w+h %d\n", PX(mItemDisplayWidth), PX(dropdownRect.width), PX((dropBorderPadding.left + dropBorderPadding.right)));

    // Start with the dropdown rect's width (at this stage, it's the
    // natural width of the content in the list, i.e., the width of
    // the widest content, i.e. the preferred width for the display
    // frame) and add room for the button, which is assumed to match
    // the width of the scrollbar (note that the scrollbarWidth is
    // passed as aBtnWidth to ReflowCombobox).  (When the dropdown was
    // an nsScrollFrame the scrollbar width seems to have already been
    // added to its unconstrained width.)
    mItemDisplayWidth = dropdownRect.width + scrollbarWidth;

    REFLOW_DEBUG_MSG2("*  mItemDisplayWidth %d\n", PX(mItemDisplayWidth));

    // mItemDisplayWidth must be the size of the "display" frame including it's 
    // border and padding, but NOT including the comboboxes border and padding
    mItemDisplayWidth += dspBorderPadding.left + dspBorderPadding.right;
    mItemDisplayWidth -= aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;

    REFLOW_DEBUG_MSG2("*A mItemDisplayWidth %d\n", PX(mItemDisplayWidth));

  } else {
    REFLOW_DEBUG_MSG("Constrained.....\n");
    if (firstPassState.mComputedWidth > 0) {
      // Compute the display item's width from reflow's mComputedWidth
      // mComputedWidth has already excluded border and padding
      // so subtract off the button's size
      REFLOW_DEBUG_MSG3("B mItemDisplayWidth %d    %d\n", PX(mItemDisplayWidth), PX(dspBorderPadding.right));
      // Display Frame's width comes from the mComputedWidth and therefore implies that it
      // includes the "display" frame's border and padding.
      mItemDisplayWidth = firstPassState.mComputedWidth;
      REFLOW_DEBUG_MSG2("A mItemDisplayWidth %d\n", PX(mItemDisplayWidth));
      REFLOW_DEBUG_MSG4("firstPassState.mComputedWidth %d -  size.width %d dspBorderPadding.right %d\n", PX(firstPassState.mComputedWidth), PX(size.width), PX(dspBorderPadding.right));
    }
  }

  // Fix for Bug 44788 (remove this comment later)
  if (firstPassState.mComputedHeight > 0 && NS_UNCONSTRAINEDSIZE != firstPassState.mComputedHeight) {
    size.height = firstPassState.mComputedHeight;
  }


  // this reflows and makes and last minute adjustments
  ReflowCombobox(aPresContext, firstPassState, aDesiredSize, aStatus, 
                     mDisplayFrame, mButtonFrame, mItemDisplayWidth, scrollbarWidth, 
                     aReflowState.mComputedBorderPadding, size.height);

  // The dropdown was reflowed UNCONSTRAINED before, now we need to check to see
  // if it needs to be resized. 
  //
  // Optimization - The style (font, etc.) maybe different for the display item 
  // than for any particular item in the dropdown. So, if the new size of combobox
  // is smaller than the dropdown, that is OK, The dropdown MUST always be either the same
  // size as the combo or larger if necessary
  if (aDesiredSize.width > dropdownDesiredSize.width) {
    if (eReflowReason_Initial == firstPassState.reason) {
      firstPassState.reason = eReflowReason_Resize;
    }
    REFLOW_DEBUG_MSG3("*** Reflowing ListBox to width: %d it was %d\n", PX(aDesiredSize.width), PX(dropdownDesiredSize.width));

    // Tell it we are doing the second pass, which means we will skip
    // doing the unconstained reflow, we already know that size
    nsListControlFrame * lcf = NS_STATIC_CAST(nsListControlFrame*, mDropdownFrame);
    lcf->SetPassId(2);
    // Reflow the dropdown list to match the width of the display + button
    ReflowComboChildFrame(mDropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, aStatus, 
                          aDesiredSize.width, NS_UNCONSTRAINEDSIZE);
    lcf->SetPassId(0);
  }

  // Set the max element size to be the same as the desired element size.
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = aDesiredSize.width;
  }

#if 0
  COMPARE_QUIRK_SIZE("nsComboboxControlFrame", 127, 22) 
#endif

  // cache the availabe size to be our desired size minus the borders
  // this is so if our cached available size is ever equal to or less 
  // than the real available size we can bail out
  if (aReflowState.availableWidth != NS_UNCONSTRAINEDSIZE) {
    mCachedAvailableSize.width  = aDesiredSize.width -
      (aReflowState.mComputedBorderPadding.left +
       aReflowState.mComputedBorderPadding.right);
  }
  if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
    mCachedAvailableSize.height = aDesiredSize.height -
      (aReflowState.mComputedBorderPadding.top +
       aReflowState.mComputedBorderPadding.bottom);
  }

  nsFormControlFrame::SetupCachedSizes(mCacheSize, mCachedAscent, mCachedMaxElementWidth, aDesiredSize);

  REFLOW_DEBUG_MSG3("** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
  REFLOW_COUNTER();
  UNCONSTRAINED_CHECK();

  // If this was a fully unconstrained reflow we cache 
  // the combobox's unconstrained size
  if (fullyUnconstrained) {
    mCachedUncComboSize.width = aDesiredSize.width;
    mCachedUncComboSize.height = aDesiredSize.height;
  }

  NS_ASSERTION(aDesiredSize.width != kSizeNotSet,  "aDesiredSize.width != kSizeNotSet");
  NS_ASSERTION(aDesiredSize.height != kSizeNotSet, "aDesiredSize.height != kSizeNotSet");
  aDesiredSize.mOverflowArea.x      = 0;
  aDesiredSize.mOverflowArea.y      = 0;
  aDesiredSize.mOverflowArea.width  = aDesiredSize.width;
  aDesiredSize.mOverflowArea.height = aDesiredSize.height;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;

}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetName(nsAString* aResult)
{
  return nsFormControlHelper::GetName(mContent, aResult);
}

NS_IMETHODIMP
nsComboboxControlFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                         const nsPoint& aPoint,
                                         nsFramePaintLayer aWhichLayer,
                                         nsIFrame** aFrame)
{
  // The button is getting the hover events so...
  // None of the children frames of the combobox get
  // the events. (like the button frame), that way
  // all event based style rules affect the combobox 
  // and not the any of the child frames.  (The inability
  // of the parent to be in the :hover state at the same
  // time as its children is really a bug (#5693 / #33736)
  // in the implementation of :hover.)
  
  // It would be theoretically more elegant to check the
  // children when not disabled, and then use event
  // capturing.  It would correctly handle situations (obscure!!)
  // where the children were visible but the parent was not.
  // Now the functionality of the OPTIONs depends on the SELECT
  // being visible.  Oh well...

  if ( mRect.Contains(aPoint) &&
       (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) ) {
    if (GetStyleVisibility()->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}


//--------------------------------------------------------------

#ifdef NS_DEBUG
NS_IMETHODIMP
nsComboboxControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ComboboxControl"), aResult);
}
#endif


//----------------------------------------------------------------------
// nsIComboboxControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::ShowDropDown(PRBool aDoDropDown) 
{ 
  if (nsFormControlHelper::GetDisabled(mContent)) {
    return NS_OK;
  }

  if (!mDroppedDown && aDoDropDown) {
    if (mListControlFrame) {
      mListControlFrame->SyncViewWithFrame(mPresContext);
    }
    ToggleList(mPresContext);
    return NS_OK;
  } else if (mDroppedDown && !aDoDropDown) {
    ToggleList(mPresContext);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsComboboxControlFrame::SetDropDown(nsIFrame* aDropDownFrame)
{
  mDropdownFrame = aDropDownFrame;
 
  if (NS_OK != CallQueryInterface(mDropdownFrame, &mListControlFrame)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetDropDown(nsIFrame** aDropDownFrame) 
{
  if (nsnull == aDropDownFrame) {
    return NS_ERROR_FAILURE;
  }

  *aDropDownFrame = mDropdownFrame;
 
  return NS_OK;
}

// Toggle dropdown list.

NS_IMETHODIMP 
nsComboboxControlFrame::ToggleList(nsIPresContext* aPresContext)
{
  ShowList(aPresContext, (PR_FALSE == mDroppedDown));

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::AbsolutelyPositionDropDown()
{
  nsRect absoluteTwips;
  nsRect absolutePixels;

  if (NS_SUCCEEDED(nsFormControlFrame::GetAbsoluteFramePosition(mPresContext, this,  absoluteTwips, absolutePixels))) {
    PositionDropdown(mPresContext, GetRect().height, absoluteTwips, absolutePixels);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetAbsoluteRect(nsRect* aRect)
{
  nsRect absoluteTwips;
  return nsFormControlFrame::GetAbsoluteFramePosition(mPresContext, this,  absoluteTwips, *aRect);
}

///////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsComboboxControlFrame::RedisplaySelectedText()
{
  PRInt32 selectedIndex;
  mListControlFrame->GetSelectedIndex(&selectedIndex);

  return RedisplayText(selectedIndex);
}

nsresult
nsComboboxControlFrame::RedisplayText(PRInt32 aIndex)
{
  // Get the text to display
  nsAutoString textToDisplay;
  if (aIndex != -1) {
    mListControlFrame->GetOptionText(aIndex, textToDisplay);
  }
  mDisplayedIndex = aIndex;

#ifdef DO_REFLOW_DEBUG
  char * str =  ToNewCString(textToDisplay);
  REFLOW_DEBUG_MSG2("RedisplayText %s\n", str);
  delete [] str;
#endif

  // Send reflow command because the new text maybe larger
  nsresult rv = NS_OK;
  if (mDisplayContent) {
    nsAutoString value;
    mDisplayContent->Text()->AppendTo(value);

    PRBool shouldSetValue = PR_FALSE;
    if (value.IsEmpty()) {
      shouldSetValue = PR_TRUE;
    } else {
       shouldSetValue = value != textToDisplay;
       REFLOW_DEBUG_MSG3("**** CBX::RedisplayText  Old[%s]  New[%s]\n",
                         NS_LossyConvertUCS2toASCII(value).get(),
                         NS_LossyConvertUCS2toASCII(textToDisplay).get());
    }
    if (shouldSetValue) {
      rv = ActuallyDisplayText(textToDisplay, PR_TRUE);
      //mTextFrame->AddStateBits(NS_FRAME_IS_DIRTY);
      mDisplayFrame->AddStateBits(NS_FRAME_IS_DIRTY);
      ReflowDirtyChild(mPresContext->PresShell(), mDisplayFrame);
    }
  }
  return rv;
}

nsresult
nsComboboxControlFrame::ActuallyDisplayText(nsAString& aText, PRBool aNotify)
{
  if (aText.IsEmpty()) {
    // Have to use a non-breaking space for line-height calculations
    // to be right
    static const PRUnichar space = 0xA0;
    mDisplayContent->SetText(&space, 1, aNotify);
  } else {
    mDisplayContent->SetText(aText, aNotify);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetIndexOfDisplayArea(PRInt32* aDisplayedIndex)
{
  NS_ENSURE_ARG_POINTER(aDisplayedIndex);
  *aDisplayedIndex = mDisplayedIndex;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISelectControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::DoneAddingChildren(PRBool aIsDone)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsresult rv = NS_ERROR_FAILURE;
  if (mDropdownFrame != nsnull) {
    rv = CallQueryInterface(mDropdownFrame, &listFrame);
    if (listFrame) {
      rv = listFrame->DoneAddingChildren(aIsDone);
      NS_RELEASE(listFrame);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsComboboxControlFrame::AddOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
#ifdef DO_REFLOW_DEBUGXX
  printf("*********AddOption: %d\n", aIndex);
#endif
  nsListControlFrame * lcf = NS_STATIC_CAST(nsListControlFrame*, mDropdownFrame);
  return lcf->AddOption(aPresContext, aIndex);
}
  

NS_IMETHODIMP
nsComboboxControlFrame::RemoveOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
  // If we removed the last option, we need to blank things out
  PRInt32 len;
  mListControlFrame->GetNumberOfOptions(&len);
  if (len == 0) {
    RedisplayText(-1);
  }

  nsListControlFrame* lcf = NS_STATIC_CAST(nsListControlFrame*, mDropdownFrame);
  return lcf->RemoveOption(aPresContext, aIndex);
}

NS_IMETHODIMP
nsComboboxControlFrame::GetOptionSelected(PRInt32 aIndex, PRBool* aValue)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsresult rv = CallQueryInterface(mDropdownFrame, &listFrame);
  if (listFrame) {
    rv = listFrame->GetOptionSelected(aIndex, aValue);
    NS_RELEASE(listFrame);
  }
  return rv;
}

//---------------------------------------------------------
// Used by layout to determine if we have a fake option
NS_IMETHODIMP
nsComboboxControlFrame::GetDummyFrame(nsIFrame** aFrame)
{
  nsISelectControlFrame* listFrame = nsnull;
  NS_ASSERTION(mDropdownFrame, "No dropdown frame!");

  CallQueryInterface(mDropdownFrame, &listFrame);
  NS_ASSERTION(listFrame, "No list frame!");

  if (listFrame) {
    listFrame->GetDummyFrame(aFrame);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::SetDummyFrame(nsIFrame* aFrame)
{
  nsISelectControlFrame* listFrame = nsnull;
  NS_ASSERTION(mDropdownFrame, "No dropdown frame!");

  CallQueryInterface(mDropdownFrame, &listFrame);
  NS_ASSERTION(listFrame, "No list frame!");

  if (listFrame) {
    listFrame->SetDummyFrame(aFrame);
  }

  return NS_OK;
}

// End nsISelectControlFrame
//----------------------------------------------------------------------

NS_IMETHODIMP 
nsComboboxControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  // temp fix until Bug 124990 gets fixed
  if (aPresContext->IsPaginated() && NS_IS_MOUSE_EVENT(aEvent)) {
    return NS_OK;
  }

  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }
  if (nsFormControlHelper::GetDisabled(mContent)) {
    return NS_OK;
  }

  // If we have style that affects how we are selected, feed event down to
  // nsFrame::HandleEvent so that selection takes place when appropriate.
  const nsStyleUserInterface* uiStyle = GetStyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsAreaFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    
  return NS_OK;
}


NS_IMETHODIMP 
nsComboboxControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAString& aValue)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = CallQueryInterface(mDropdownFrame, &fcFrame);
  if ((NS_SUCCEEDED(result)) && (nsnull != fcFrame)) {
    return fcFrame->SetProperty(aPresContext, aName, aValue);
  }
  return result;
}

NS_IMETHODIMP 
nsComboboxControlFrame::GetProperty(nsIAtom* aName, nsAString& aValue)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = CallQueryInterface(mDropdownFrame, &fcFrame);
  if ((NS_SUCCEEDED(result)) && (nsnull != fcFrame)) {
    return fcFrame->GetProperty(aName, aValue);
  }
  return result;
}

nsIFrame*
nsComboboxControlFrame::GetContentInsertionFrame() {
  return mDropdownFrame->GetContentInsertionFrame();
}

NS_IMETHODIMP 
nsComboboxControlFrame::CreateDisplayFrame(nsIPresContext* aPresContext)
{
  if (mGoodToGo) {
    return NS_OK;
  }

  nsIPresShell *shell = aPresContext->PresShell();
  nsStyleSet *styleSet = shell->StyleSet();

  nsresult rv = NS_NewBlockFrame(shell, (nsIFrame**)&mDisplayFrame, NS_BLOCK_SPACE_MGR);
  if (NS_FAILED(rv)) { return rv; }
  if (!mDisplayFrame) { return NS_ERROR_NULL_POINTER; }

  // create the style context for the anonymous frame
  nsRefPtr<nsStyleContext> styleContext;
  styleContext = styleSet->ResolvePseudoStyleFor(mContent, 
                                                 nsCSSAnonBoxes::mozDisplayComboboxControlFrame,
                                                 mStyleContext);
  if (!styleContext) { return NS_ERROR_NULL_POINTER; }

  // create a text frame and put it inside the block frame
  rv = NS_NewTextFrame(shell, &mTextFrame);
  if (NS_FAILED(rv)) { return rv; }
  if (!mTextFrame) { return NS_ERROR_NULL_POINTER; }
  nsRefPtr<nsStyleContext> textStyleContext;
  textStyleContext = styleSet->ResolveStyleForNonElement(styleContext);
  if (!textStyleContext) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDisplayContent));
  mTextFrame->Init(aPresContext, content, mDisplayFrame, textStyleContext, nsnull);
  mTextFrame->SetInitialChildList(aPresContext, nsnull, nsnull);

  aPresContext->FrameManager()->SetPrimaryFrameFor(content, mTextFrame);

  rv = mDisplayFrame->Init(aPresContext, mContent, this, styleContext, nsnull);
  if (NS_FAILED(rv)) { return rv; }

  mDisplayFrame->SetInitialChildList(aPresContext, nsnull, mTextFrame);

  return NS_OK;
}


NS_IMETHODIMP
nsComboboxControlFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                               nsISupportsArray& aChildList)
{
  // The frames used to display the combo box and the button used to popup the dropdown list
  // are created through anonymous content. The dropdown list is not created through anonymous
  // content because it's frame is initialized specifically for the drop-down case and it is placed
  // a special list referenced through NS_COMBO_FRAME_POPUP_LIST_INDEX to keep separate from the
  // layout of the display and button. 
  //
  // Note: The value attribute of the display content is set when an item is selected in the dropdown list.
  // If the content specified below does not honor the value attribute than nothing will be displayed.
  // In addition, if the frame created by content below for does not implement the nsIFormControlFrame 
  // interface and honor the SetSuggestedSize method the placement and size of the display area will not
  // match what is normally desired for a combobox.


  // For now the content that is created corresponds to two input buttons. It would be better to create the
  // tag as something other than input, but then there isn't any way to create a button frame since it
  // isn't possible to set the display type in CSS2 to create a button frame.

    // create content used for display
  //nsIAtom* tag = NS_NewAtom("mozcombodisplay");

  // Add a child text content node for the label
  nsresult rv;
  nsCOMPtr<nsIContent> labelContent(do_CreateInstance(kTextNodeCID, &rv));
  if (NS_SUCCEEDED(rv) && labelContent) {
    // set the value of the text node
    mDisplayContent = do_QueryInterface(labelContent);
    mDisplayContent->SetText(NS_LITERAL_STRING("X"), PR_TRUE);

    nsCOMPtr<nsIDocument> doc = mContent->GetDocument();
    // mContent->AppendChildTo(labelContent, PR_FALSE, PR_FALSE);

    nsCOMPtr<nsINodeInfo> nodeInfo;
    doc->NodeInfoManager()->GetNodeInfo(nsHTMLAtoms::input, nsnull,
                                        kNameSpaceID_None,
                                        getter_AddRefs(nodeInfo));

    aChildList.AppendElement(labelContent);

    // create button which drops the list down
    nsCOMPtr<nsIContent> btnContent;
    rv = NS_NewHTMLElement(getter_AddRefs(btnContent), nodeInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    // make someone to listen to the button. If its pressed by someone like Accessibility
    // then open or close the combo box.
    nsCOMPtr<nsIDOMEventReceiver> eventReceiver(do_QueryInterface(btnContent));
    if (eventReceiver) {
       mButtonListener = new nsComboButtonListener(this);
       eventReceiver->AddEventListenerByIID(mButtonListener, NS_GET_IID(nsIDOMMouseListener));
    }

    btnContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::type, NS_LITERAL_STRING("button"), PR_FALSE);

    aChildList.AppendElement(btnContent);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsComboboxControlFrame::CreateFrameFor(nsIPresContext*   aPresContext,
                                       nsIContent *      aContent,
                                       nsIFrame**        aFrame) 
{ 
  NS_PRECONDITION(nsnull != aFrame, "null ptr");
  NS_PRECONDITION(nsnull != aContent, "null ptr");
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");

  *aFrame = nsnull;
  NS_ASSERTION(mDisplayContent != nsnull, "mDisplayContent can't be null!");

  if (!mGoodToGo) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDisplayContent));
  if (aContent == content.get()) {
    // Get PresShell
    nsIPresShell *shell = aPresContext->PresShell();
    nsStyleSet *styleSet = shell->StyleSet();

    // Start by by creating a containing frame
    nsresult rv = NS_NewBlockFrame(shell, (nsIFrame**)&mDisplayFrame, NS_BLOCK_SPACE_MGR);
    if (NS_FAILED(rv))  { return rv; }
    if (!mDisplayFrame) { return NS_ERROR_NULL_POINTER; }

    // create the style context for the anonymous block frame
    nsRefPtr<nsStyleContext> styleContext;
    styleContext = styleSet->ResolvePseudoStyleFor(mContent, 
                                                   nsCSSAnonBoxes::mozDisplayComboboxControlFrame,
                                                   mStyleContext);
    if (!styleContext) { return NS_ERROR_NULL_POINTER; }

    // Create a text frame and put it inside the block frame
    rv = NS_NewTextFrame(shell, &mTextFrame);
    if (NS_FAILED(rv)) { return rv; }
    if (!mTextFrame)   { return NS_ERROR_NULL_POINTER; }
    nsRefPtr<nsStyleContext> textStyleContext;
    textStyleContext = styleSet->ResolveStyleForNonElement(styleContext);
    if (!textStyleContext) { return NS_ERROR_NULL_POINTER; }

    // initialize the text frame
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDisplayContent));
    mTextFrame->Init(aPresContext, content, mDisplayFrame, textStyleContext, nsnull);
    mTextFrame->SetInitialChildList(aPresContext, nsnull, nsnull);

    /*nsCOMPtr<nsIFrameManager> frameManager;
    rv = shell->GetFrameManager(getter_AddRefs(frameManager));
    if (NS_FAILED(rv)) { return rv; }
    if (!frameManager) { return NS_ERROR_NULL_POINTER; }
    frameManager->SetPrimaryFrameFor(content, mTextFrame);
    */

    rv = mDisplayFrame->Init(aPresContext, mContent, this, styleContext, nsnull);
    if (NS_FAILED(rv)) { return rv; }

    mDisplayFrame->SetInitialChildList(aPresContext, nsnull, mTextFrame);
    *aFrame = mDisplayFrame;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}




NS_IMETHODIMP 
nsComboboxControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}



NS_IMETHODIMP
nsComboboxControlFrame::Destroy(nsIPresContext* aPresContext)
{
  nsFormControlFrame::RegUnRegAccessKey(mPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);

  if (mDroppedDown) {
    // Get parent view
    nsIFrame * listFrame;
    if (NS_OK == mListControlFrame->QueryInterface(NS_GET_IID(nsIFrame), (void **)&listFrame)) {
      nsIView* view = listFrame->GetView();
      NS_ASSERTION(view, "nsComboboxControlFrame view is null");
      if (view) {
    	  nsIWidget* widget = view->GetWidget();
        if (widget)
          widget->CaptureRollupEvents((nsIRollupListener *)this, PR_FALSE, PR_TRUE);
      }
    }
  }

   // Cleanup frames in popup child list
  mPopupFrames.DestroyFrames(aPresContext);

  if (!mGoodToGo) {
    if (mDisplayFrame) {
      mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, mDisplayFrame, nsnull);
      mDisplayFrame->Destroy(aPresContext);
      mDisplayFrame=nsnull;
    }
  }

  return nsAreaFrame::Destroy(aPresContext);
}


nsIFrame*
nsComboboxControlFrame::GetFirstChild(nsIAtom* aListName) const
{
  if (nsLayoutAtoms::popupList == aListName) {
    return mPopupFrames.FirstChild();
  }
  return nsAreaFrame::GetFirstChild(aListName);
}

NS_IMETHODIMP
nsComboboxControlFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;
  if (nsLayoutAtoms::popupList == aListName) {
    mPopupFrames.SetFrames(aChildList);
  } else {
    rv = nsAreaFrame::SetInitialChildList(aPresContext, aListName, aChildList);
    InitTextStr();

    for (nsIFrame * child = aChildList; child;
         child = child->GetNextSibling()) {
      nsIFormControlFrame* fcFrame = nsnull;
      CallQueryInterface(child, &fcFrame);
      if (fcFrame) {
        if (fcFrame->GetFormControlType() == NS_FORM_INPUT_BUTTON) {
          mButtonFrame = child;
        }
      } else {
        mDisplayFrame = child;
      }
    }
  }
  return rv;
}

nsIAtom*
nsComboboxControlFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
   // Maintain a separate child list for the dropdown list (i.e. popup listbox)
   // This is necessary because we don't want the listbox to be included in the layout
   // of the combox's children because it would take up space, when it is suppose to
   // be floating above the display.
  if (aIndex <= NS_BLOCK_FRAME_ABSOLUTE_LIST_INDEX) {
    return nsAreaFrame::GetAdditionalChildListName(aIndex);
  }
  
  if (NS_COMBO_FRAME_POPUP_LIST_INDEX == aIndex) {
    return nsLayoutAtoms::popupList;
  }
  return nsnull;
}

PRIntn
nsComboboxControlFrame::GetSkipSides() const
{    
    // Don't skip any sides during border rendering
  return 0;
}


//----------------------------------------------------------------------
  //nsIRollupListener
//----------------------------------------------------------------------
NS_IMETHODIMP 
nsComboboxControlFrame::Rollup()
{
  if (mDroppedDown) {
    mListControlFrame->AboutToRollup();
    ShowDropDown(PR_FALSE);
    mListControlFrame->CaptureMouseEvents(mPresContext, PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::RollupFromList(nsIPresContext* aPresContext)
{
  ShowList(aPresContext, PR_FALSE);
  mListControlFrame->CaptureMouseEvents(aPresContext, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP_(PRInt32)
nsComboboxControlFrame::UpdateRecentIndex(PRInt32 aIndex)
{
  PRInt32 index = mRecentSelectedIndex;
  if (mRecentSelectedIndex == -1 || aIndex == -1)
    mRecentSelectedIndex = aIndex;
  return index;
}

NS_METHOD 
nsComboboxControlFrame::Paint(nsIPresContext*     aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible) {
    return NS_OK;
  }
#ifdef NOISY
  printf("%p paint layer %d at (%d, %d, %d, %d)\n", this, aWhichLayer, 
    aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
#endif
  // We paint everything in the foreground so that the form control's
  // parents cannot paint over it in other passes (bug 95826).
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    nsAreaFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       NS_FRAME_PAINT_LAYER_BACKGROUND);
    nsAreaFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       NS_FRAME_PAINT_LAYER_FLOATS);
    nsAreaFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       NS_FRAME_PAINT_LAYER_FOREGROUND);

    // nsITheme should take care of drawing the focus border, but currently does so only on Mac.
    // If all of the nsITheme implementations are fixed to draw the focus border correctly,
    // this #ifdef should be replaced with a -moz-appearance / ThemeSupportsWidget() check.

#ifndef MOZ_WIDGET_COCOA
    if (mDisplayFrame) {
      aRenderingContext.PushState();
      nsRect clipRect = mDisplayFrame->GetRect();
      aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, 
                 mDisplayFrame, NS_FRAME_PAINT_LAYER_BACKGROUND);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, 
                 mDisplayFrame, NS_FRAME_PAINT_LAYER_FOREGROUND);

      /////////////////////
      // draw focus
      // XXX This is only temporary
      // Only paint the focus if we're visible
      if (GetStyleVisibility()->IsVisible()) {
        if (!nsFormControlHelper::GetDisabled(mContent) && mFocused == this) {
          aRenderingContext.SetLineStyle(nsLineStyle_kDotted);
          aRenderingContext.SetColor(0);
        } else {
          aRenderingContext.SetColor(GetStyleBackground()->mBackgroundColor);
          aRenderingContext.SetLineStyle(nsLineStyle_kSolid);
        }
        //aRenderingContext.DrawRect(clipRect);
        float p2t = aPresContext->PixelsToTwips();
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);
        clipRect.width -= onePixel;
        clipRect.height -= onePixel;
        aRenderingContext.DrawLine(clipRect.x, clipRect.y, 
                                   clipRect.x+clipRect.width, clipRect.y);
        aRenderingContext.DrawLine(clipRect.x+clipRect.width, clipRect.y, 
                                   clipRect.x+clipRect.width, clipRect.y+clipRect.height);
        aRenderingContext.DrawLine(clipRect.x+clipRect.width, clipRect.y+clipRect.height, 
                                   clipRect.x, clipRect.y+clipRect.height);
        aRenderingContext.DrawLine(clipRect.x, clipRect.y+clipRect.height, 
                                   clipRect.x, clipRect.y);
        aRenderingContext.DrawLine(clipRect.x, clipRect.y+clipRect.height, 
                                   clipRect.x, clipRect.y);
      }
      /////////////////////
      aRenderingContext.PopState();
    }
#endif
  }
  
  // Call to the base class to draw selection borders when appropriate
  return nsFrame::Paint(aPresContext,aRenderingContext,aDirtyRect,aWhichLayer);
}

//----------------------------------------------------------------------
  //nsIScrollableViewProvider
//----------------------------------------------------------------------
NS_METHOD
nsComboboxControlFrame::GetScrollableView(nsIPresContext* aPresContext,
                                          nsIScrollableView** aView)
{
  *aView = nsnull;

  if (!mDropdownFrame)
    return NS_ERROR_FAILURE;

  nsIScrollableFrame* scrollable = nsnull;
  nsresult rv = CallQueryInterface(mDropdownFrame, &scrollable);
  if (NS_FAILED(rv))
    return rv;

  return scrollable->GetScrollableView(aPresContext, aView);
}

//---------------------------------------------------------
// gets the content (an option) by index and then set it as
// being selected or not selected
//---------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::OnOptionSelected(nsIPresContext* aPresContext,
                                         PRInt32 aIndex,
                                         PRBool aSelected)
{
  if (mDroppedDown) {
    nsCOMPtr<nsISelectControlFrame> selectFrame
                                     = do_QueryInterface(mListControlFrame);
    if (selectFrame) {
      selectFrame->OnOptionSelected(aPresContext, aIndex, aSelected);
    }
  } else {
    if (aSelected) {
      RedisplayText(aIndex);
    } else {
      RedisplaySelectedText();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::OnOptionTextChanged(nsIDOMHTMLOptionElement* option)
{
  RedisplaySelectedText();
  if (mDroppedDown) {
    nsCOMPtr<nsISelectControlFrame> selectFrame
                                     = do_QueryInterface(mListControlFrame);
    if (selectFrame) {
      selectFrame->OnOptionTextChanged(option);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsComboboxControlFrame::OnContentReset()
{
  if (mListControlFrame) {
    nsCOMPtr<nsIFormControlFrame> formControl =
      do_QueryInterface(mListControlFrame);
    formControl->OnContentReset();
  }
  return NS_OK;
}


//--------------------------------------------------------
// nsIStatefulFrame
//--------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::SaveState(nsIPresContext* aPresContext,
                                  nsIPresState** aState)
{
  nsCOMPtr<nsIStatefulFrame> stateful(do_QueryInterface(mListControlFrame));
  NS_ASSERTION(stateful, "Couldn't cast list frame to stateful frame!!!");
  if (stateful) {
    return stateful->SaveState(aPresContext, aState);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::RestoreState(nsIPresContext* aPresContext,
                                     nsIPresState* aState)
{
  if (!mListControlFrame)
    return NS_ERROR_FAILURE;

  nsIStatefulFrame* stateful;
  nsresult rv = CallQueryInterface(mListControlFrame, &stateful);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Must implement nsIStatefulFrame");
  rv = stateful->RestoreState(aPresContext, aState);
  return rv;
}

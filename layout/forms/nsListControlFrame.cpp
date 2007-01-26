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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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
#include "nsLayoutAtoms.h"
#include "nsIFormControl.h"
#include "nsIDeviceContext.h" 
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h" 
#include "nsIDOMHTMLOptionsCollection.h" 
#include "nsIDOMNSHTMLOptionCollectn.h"
#include "nsIDOMHTMLSelectElement.h" 
#include "nsIDOMNSHTMLSelectElement.h" 
#include "nsIDOMHTMLOptionElement.h" 
#include "nsComboboxControlFrame.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsWidgetsCID.h"
#include "nsIPresShell.h"
#include "nsHTMLParts.h"
#include "nsIDOMEventReceiver.h"
#include "nsEventDispatcher.h"
#include "nsIEventStateManager.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsILookAndFeel.h"
#include "nsIFontMetrics.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSEvent.h"
#include "nsGUIEvent.h"
#include "nsIServiceManager.h"
#include "nsINodeInfo.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsISelectElement.h"
#include "nsIPrivateDOMEvent.h"
#include "nsCSSRendering.h"
#include "nsITheme.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMKeyListener.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"

// Constants
const nscoord kMaxDropDownRows          = 20; // This matches the setting for 4.x browsers
const PRInt32 kNothingSelected          = -1;


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
nsIFrame*
NS_NewListControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  nsListControlFrame* it =
    new (aPresShell) nsListControlFrame(aPresShell, aPresShell->GetDocument(), aContext);

  if (it) {
    it->AddStateBits(NS_FRAME_INDEPENDENT_SELECTION);
  }

  return it;
}

//---------------------------------------------------------
nsListControlFrame::nsListControlFrame(
  nsIPresShell* aShell, nsIDocument* aDocument, nsStyleContext* aContext)
  : nsHTMLScrollFrame(aShell, aContext, PR_FALSE),
    mMightNeedSecondPass(PR_FALSE),
    mLastDropdownComputedHeight(NS_UNCONSTRAINEDSIZE)
{
  mComboboxFrame      = nsnull;
  mChangesSinceDragStart = PR_FALSE;
  mButtonDown         = PR_FALSE;

  mIsAllContentHere   = PR_FALSE;
  mIsAllFramesHere    = PR_FALSE;
  mHasBeenInitialized = PR_FALSE;
  mNeedToReset        = PR_TRUE;
  mPostChildrenLoadedReset = PR_FALSE;

  mControlSelectMode           = PR_FALSE;
}

//---------------------------------------------------------
nsListControlFrame::~nsListControlFrame()
{
  mComboboxFrame = nsnull;
}

// for Bug 47302 (remove this comment later)
void
nsListControlFrame::Destroy()
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

  nsFormControlFrame::RegUnRegAccessKey(NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  nsHTMLScrollFrame::Destroy();
}

NS_IMETHODIMP
nsListControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  // We allow visibility:hidden <select>s to contain visible options.
  
  // Don't allow painting of list controls when painting is suppressed.
  // XXX why do we need this here? we should never reach this. Maybe
  // because these can have widgets? Hmm
  if (aBuilder->IsBackgroundOnly())
    return NS_OK;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsListControlFrame");

  // REVIEW: The selection visibility code that used to be here is what
  // we already do by default.
  // REVIEW: There was code here to paint the theme background. But as far
  // as I can tell, we'd just paint the theme background twice because
  // it was redundant with nsCSSRendering::PaintBackground
  return nsHTMLScrollFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
}

/**
 * This is called by the SelectsAreaFrame, which is the same
 * as the frame returned by GetOptionsContainer. It's the frame which is
 * scrolled by us.
 * @param aPt the offset of this frame, relative to the rendering reference
 * frame
 */
void nsListControlFrame::PaintFocus(nsIRenderingContext& aRC, nsPoint aPt)
{
  if (mFocused != this) return;

  // The mEndSelectionIndex is what is currently being selected
  // use the selected index if this is kNothingSelected
  PRInt32 focusedIndex;
  if (mEndSelectionIndex == kNothingSelected) {
    focusedIndex = GetSelectedIndex();
  } else {
    focusedIndex = mEndSelectionIndex;
  }

  nsPresContext* presContext = GetPresContext();
  if (!GetScrollableView()) return;

  nsIPresShell *presShell = presContext->GetPresShell();
  if (!presShell) return;

  nsIFrame* containerFrame = GetOptionsContainer();
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
    focusedContent = GetOptionContent(focusedIndex);
    // otherwise we find the focusedContent's frame and scroll to it
    if (focusedContent) {
      childframe = presShell->GetPrimaryFrameFor(focusedContent);
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
      childframe = presShell->GetPrimaryFrameFor(focusedContent);
    }
    if (!childframe) {
      // Failing all else, try the first thing we have, but only if
      // it's an element.  Text frames need not apply.
      childframe = containerFrame->GetFirstChild(nsnull);
      if (childframe &&
          !childframe->GetContent()->IsNodeOfType(nsINode::eELEMENT)) {
        childframe = nsnull;
      }
      result = NS_OK;
    }
  }

  nsRect fRect;
  if (childframe) {
    // get the child rect
    fRect = childframe->GetRect();
    // get it into our coordinates
    fRect.MoveBy(childframe->GetParent()->GetOffsetTo(this));
  } else {
    fRect.x = fRect.y = 0;
    fRect.width = GetScrollPortSize().width;
    fRect.height = CalcFallbackRowHeight(0);
    fRect.MoveBy(containerFrame->GetOffsetTo(this));
  }
  fRect += aPt;
  
  PRBool lastItemIsSelected = PR_FALSE;
  if (focusedContent) {
    nsCOMPtr<nsIDOMHTMLOptionElement> domOpt =
      do_QueryInterface(focusedContent);
    if (domOpt) {
      domOpt->GetSelected(&lastItemIsSelected);
    }
  }

  // set up back stop colors and then ask L&F service for the real colors
  nscolor color;
  presContext->LookAndFeel()->
    GetColor(lastItemIsSelected ?
             nsILookAndFeel::eColor_WidgetSelectForeground :
             nsILookAndFeel::eColor_WidgetSelectBackground, color);

  nscoord onePixelInTwips = presContext->IntScaledPixelsToTwips(1);

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
  return nsHTMLScrollFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsListControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mContent);
    nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(GetPresContext()->PresShell()));
    return accService->CreateHTMLListboxAccessible(node, weakShell, aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

static nscoord
GetMaxOptionHeight(nsIFrame* aContainer)
{
  nscoord result = 0;
  for (nsIFrame* option = aContainer->GetFirstChild(nsnull);
       option; option = option->GetNextSibling()) {
    nscoord optionHeight;
    if (nsCOMPtr<nsIDOMHTMLOptGroupElement>
        (do_QueryInterface(option->GetContent()))) {
      // an optgroup
      optionHeight = GetMaxOptionHeight(option);
    } else {
      // an option
      optionHeight = option->GetSize().height;
    }
    if (result < optionHeight)
      result = optionHeight;
  }
  return result;
}

static inline PRBool
IsOptGroup(nsIContent *aContent)
{
  return (aContent->NodeInfo()->Equals(nsGkAtoms::optgroup) &&
          aContent->IsNodeOfType(nsINode::eHTML));
}

static inline PRBool
IsOption(nsIContent *aContent)
{
  return (aContent->NodeInfo()->Equals(nsGkAtoms::option) &&
          aContent->IsNodeOfType(nsINode::eHTML));
}

static PRUint32
GetNumberOfOptionsRecursive(nsIContent* aContent)
{
  PRUint32 optionCount = 0;
  const PRUint32 childCount = aContent ? aContent->GetChildCount() : 0;
  for (PRUint32 index = 0; index < childCount; ++index) {
    nsIContent* child = aContent->GetChildAt(index);
    if (::IsOption(child)) {
      ++optionCount;
    }
    else if (::IsOptGroup(child)) {
      optionCount += ::GetNumberOfOptionsRecursive(child);
    }
  }
  return optionCount;
}

static nscoord
GetOptGroupLabelsHeight(nsPresContext* aPresContext,
                        nsIContent*    aContent,
                        nscoord        aRowHeight)
{
  nscoord height = 0;
  const PRUint32 childCount = aContent ? aContent->GetChildCount() : 0;
  for (PRUint32 index = 0; index < childCount; ++index) {
    nsIContent* child = aContent->GetChildAt(index);
    if (::IsOptGroup(child)) {
      PRUint32 numOptions = ::GetNumberOfOptionsRecursive(child);
      nscoord optionsHeight = aRowHeight * numOptions;
      nsIFrame* frame = aPresContext->GetPresShell()->GetPrimaryFrameFor(child);
      nscoord totalHeight = frame ? frame->GetSize().height : 0;
      height += PR_MAX(0, totalHeight - optionsHeight);
    }
  }
  return height;
}

//-----------------------------------------------------------------
// Main Reflow for ListBox/Dropdown
//-----------------------------------------------------------------

nscoord
nsListControlFrame::CalcHeightOfARow()
{
  // Calculate the height of a single row in the listbox or dropdown list by
  // using the tallest thing in the subtree, since there may be option groups
  // in addition to option elements, either of which may be visible or
  // invisible, may use different fonts, etc.
  PRInt32 heightOfARow = GetMaxOptionHeight(GetOptionsContainer());

  // Check to see if we have zero items 
  if (heightOfARow == 0) {
    heightOfARow = CalcFallbackRowHeight(GetNumberOfOptions());
  }

  return heightOfARow;
}

nscoord
nsListControlFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  // We don't want to have options wrapping unless they absolutely
  // have to, so our min width is our pref width.
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  result = GetPrefWidth(aRenderingContext);

  return result;
}

NS_IMETHODIMP 
nsListControlFrame::Reflow(nsPresContext*           aPresContext, 
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState, 
                           nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowState.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
                  "Must have a computed width");

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

  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, PR_TRUE);
  }

  if (IsInDropDownMode()) {
    return ReflowAsDropdown(aPresContext, aDesiredSize, aReflowState, aStatus);
  }

  /*
   * Due to the fact that our intrinsic height depends on the heights of our
   * kids, we end up having to do two-pass reflow, in general -- the first pass
   * to find the intrinsic height and a second pass to reflow the scrollframe
   * at that height (which will size the scrollbars correctly, etc).
   *
   * Naturaly, we want to avoid doing the second reflow as much as possible.
   * We can skip it in the following cases (in all of which the first reflow is
   * already happening at the right height):
   *
   * - We're reflowing with a constrained computed height -- just use that
   *   height.
   * - We're not dirty and have no dirty kids.  In this case, our cached max
   *   height of a child is not going to change.
   * - We do our first reflow using our cached max height of a child, then
   *   compute the new max height and it's the same as the old one.
   */

  PRBool autoHeight = (aReflowState.mComputedHeight == NS_UNCONSTRAINEDSIZE);

  mMightNeedSecondPass = autoHeight &&
    (GetStateBits() & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN));
  
  nsHTMLReflowState state(aReflowState);
  PRInt32 length = GetNumberOfOptions();  

  nscoord oldHeightOfARow = HeightOfARow();
  
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW) && autoHeight) {
    // When not doing an initial reflow, and when the height is auto, start off
    // with our computed height set to what we'd expect our height to be.
    state.mComputedHeight = CalcIntrinsicHeight(oldHeightOfARow, length);
    state.ApplyMinMaxConstraints(nsnull, &state.mComputedHeight);
  }

  nsresult rv = nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize,
                                          state, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mMightNeedSecondPass) {
    NS_ASSERTION(!autoHeight || HeightOfARow() == oldHeightOfARow,
                 "How did our height of a row change if nothing was dirty?");
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if we don't need a second pass!");
    return rv;
  }

  mMightNeedSecondPass = PR_FALSE;

  // Now see whether we need a second pass.  If we do, our nsSelectsAreaFrame
  // will have suppressed the scrollbar update.
  if (!IsScrollbarUpdateSuppressed()) {
    // All done.  No need to do more reflow.
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if the height of a row has not "
                 "changed!");
    return rv;
  }

  SetSuppressScrollbarUpdate(PR_FALSE);

  // Gotta reflow again.
  // XXXbz We're just changing the height here; do we need to dirty ourselves
  // or anything like that?  We might need to, per the letter of the reflow
  // protocol, but things seem to work fine without it...  Is that just an
  // implementation detail of nsHTMLScrollFrame that we're depending on?
  nsHTMLScrollFrame::DidReflow(aPresContext, &state, aStatus);

  // Now compute the height we want to have
  state.mComputedHeight = CalcIntrinsicHeight(HeightOfARow(), length);
  state.ApplyMinMaxConstraints(nsnull, &state.mComputedHeight);

  nsHTMLScrollFrame::WillReflow(aPresContext);

  // XXXbz to make the ascent really correct, we should add our
  // mComputedPadding.top to it (and subtract it from descent).  Need that
  // because nsGfxScrollFrame just adds in the border....
  return nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);
}

nsresult
nsListControlFrame::ReflowAsDropdown(nsPresContext*           aPresContext, 
                                     nsHTMLReflowMetrics&     aDesiredSize,
                                     const nsHTMLReflowState& aReflowState, 
                                     nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowState.mComputedHeight == NS_UNCONSTRAINEDSIZE,
                  "We should not have a computed height here!");
  
  mMightNeedSecondPass =
    (GetStateBits() & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0;

  nscoord oldHeightOfARow = HeightOfARow();

  nsHTMLReflowState state(aReflowState);

  nscoord oldVisibleHeight;
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // When not doing an initial reflow, and when the height is auto, start off
    // with our computed height set to what we'd expect our height to be.
    // Note: At this point, mLastDropdownComputedHeight can be
    // NS_UNCONSTRAINEDSIZE in cases when last time we didn't have to constrain
    // the height.  That's fine; just do the same thing as last time.
    state.mComputedHeight = mLastDropdownComputedHeight;
    oldVisibleHeight = GetScrolledFrame()->GetSize().height;
  } else {
    // Set oldVisibleHeight to something that will never test true against a
    // real height.
    oldVisibleHeight = NS_UNCONSTRAINEDSIZE;
  }

  nsresult rv = nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize,
                                          state, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mMightNeedSecondPass) {
    NS_ASSERTION(oldVisibleHeight == GetScrolledFrame()->GetSize().height,
                 "How did our kid's height change if nothing was dirty?");
    NS_ASSERTION(HeightOfARow() == oldHeightOfARow,
                 "How did our height of a row change if nothing was dirty?");
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if we don't need a second pass!");
    NS_ASSERTION(!(GetStateBits() & NS_FRAME_FIRST_REFLOW),
                 "How can we avoid a second pass during first reflow?");
    return rv;
  }

  mMightNeedSecondPass = PR_FALSE;

  // Now see whether we need a second pass.  If we do, our nsSelectsAreaFrame
  // will have suppressed the scrollbar update.
  if (!IsScrollbarUpdateSuppressed()) {
    // All done.  No need to do more reflow.
    NS_ASSERTION(!(GetStateBits() & NS_FRAME_FIRST_REFLOW),
                 "How can we avoid a second pass during first reflow?");
    return rv;
  }

  SetSuppressScrollbarUpdate(PR_FALSE);

  nscoord visibleHeight = GetScrolledFrame()->GetSize().height;
  nscoord heightOfARow = HeightOfARow();

  // Gotta reflow again.
  // XXXbz We're just changing the height here; do we need to dirty ourselves
  // or anything like that?  We might need to, per the letter of the reflow
  // protocol, but things seem to work fine without it...  Is that just an
  // implementation detail of nsHTMLScrollFrame that we're depending on?
  nsHTMLScrollFrame::DidReflow(aPresContext, &state, aStatus);

  // Now compute the height we want to have
  mNumDisplayRows = kMaxDropDownRows;
  if (visibleHeight > mNumDisplayRows * heightOfARow) {
    visibleHeight = mNumDisplayRows * heightOfARow;
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

    state.mComputedHeight = mNumDisplayRows * heightOfARow;
    // Note: no need to apply min/max constraints, since we have no such
    // rules applied to the combobox dropdown.
    // XXXbz this is ending up too big!!  Figure out why.
  } else if (visibleHeight == 0) {
    // Looks like we have no options.  Just size us to a single row height.
    state.mComputedHeight = heightOfARow;
  } else {
    // Not too big, not too small.  Just use it!
    state.mComputedHeight = NS_UNCONSTRAINEDSIZE;
  }

  // Note: At this point, state.mComputedHeight can be NS_UNCONSTRAINEDSIZE in
  // cases when there were some options, but not too many (so no scrollbar was
  // needed).  That's fine; just store that.
  mLastDropdownComputedHeight = state.mComputedHeight;

  nsHTMLScrollFrame::WillReflow(aPresContext);
  return nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);
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

nsIFrame*
nsListControlFrame::GetContentInsertionFrame() {
  return GetOptionsContainer()->GetContentInsertionFrame();
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
  if (mComboboxFrame) {
    mComboboxFrame->UpdateRecentIndex(GetSelectedIndex());
  }

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
  PRInt32 selectedIndex = GetSelectedIndex();
  if (selectedIndex >= 0) {
    // Get the end of the contiguous selection
    nsCOMPtr<nsIDOMHTMLOptionsCollection> options = GetOptions(mContent);
    NS_ASSERTION(options, "Collection of options is null!");
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    PRUint32 i;
    // Push i to one past the last selected index in the group
    for (i=selectedIndex+1; i < numOptions; i++) {
      PRBool selected;
      nsCOMPtr<nsIDOMHTMLOptionElement> option = GetOption(options, i);
      option->GetSelected(&selected);
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

  if (aClickedIndex == kNothingSelected) {
  }
  else if (GetMultiple()) {
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
void
nsListControlFrame::CaptureMouseEvents(PRBool aGrabMouseEvents)
{
  // Currently cocoa widgets use a native popup widget which tracks clicks synchronously,
  // so we never want to do mouse capturing. Note that we only bail if the list
  // is in drop-down mode, and the caller is requesting capture (we let release capture
  // requests go through to ensure that we can release capture requested via other
  // code paths, if any exist).
  if (aGrabMouseEvents && IsInDropDownMode() && nsComboboxControlFrame::ToolkitHasNativePopup())
    return;
  
  nsIView* view = GetScrolledFrame()->GetView();

  NS_ASSERTION(view, "no view???");
  if (NS_UNLIKELY(!view))
    return;

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
        dropDownIsHidden = !mComboboxFrame->IsDroppedDown();
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
}

//---------------------------------------------------------
NS_IMETHODIMP 
nsListControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                nsGUIEvent*    aEvent,
                                nsEventStatus* aEventStatus)
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
  const nsStyleUserInterface* uiStyle = GetStyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled))
    return NS_OK;

  return nsHTMLScrollFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SetInitialChildList(nsIAtom*       aListName,
                                        nsIFrame*      aChildList)
{
  // First check to see if all the content has been added
  mIsAllContentHere = mContent->IsDoneAddingChildren();
  if (!mIsAllContentHere) {
    mIsAllFramesHere    = PR_FALSE;
    mHasBeenInitialized = PR_FALSE;
  }
  nsresult rv = nsHTMLScrollFrame::SetInitialChildList(aListName, aChildList);

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
nsListControlFrame::Init(nsIContent*     aContent,
                         nsIFrame*       aParent,
                         nsIFrame*       aPrevInFlow)
{
  nsresult result = nsHTMLScrollFrame::Init(aContent, aParent, aPrevInFlow);

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
// Returns whether the nsIDOMHTMLSelectElement supports 
// mulitple selection
//---------------------------------------------------------
PRBool
nsListControlFrame::GetMultiple(nsIDOMHTMLSelectElement* aSelect) const
{
  PRBool multiple = PR_FALSE;
  nsresult rv = NS_OK;
  if (aSelect) {
    rv = aSelect->GetMultiple(&multiple);
  } else {
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = 
       do_QueryInterface(mContent);
  
    if (selectElement) {
      rv = selectElement->GetMultiple(&multiple);
    }
  }
  if (NS_SUCCEEDED(rv)) {
    return multiple;
  }
  return PR_FALSE;
}


//---------------------------------------------------------
// Returns the nsIContent object in the collection 
// for a given index (AddRefs)
//---------------------------------------------------------
already_AddRefed<nsIContent> 
nsListControlFrame::GetOptionAsContent(nsIDOMHTMLOptionsCollection* aCollection, PRInt32 aIndex) 
{
  nsIContent * content = nsnull;
  nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = GetOption(aCollection,
                                                              aIndex);

  NS_ASSERTION(optionElement != nsnull, "could not get option element by index!");

  if (optionElement) {
    CallQueryInterface(optionElement, &content);
  }
 
  return content;
}

//---------------------------------------------------------
// for a given index it returns the nsIContent object 
// from the select
//---------------------------------------------------------
already_AddRefed<nsIContent> 
nsListControlFrame::GetOptionContent(PRInt32 aIndex) const
  
{
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options = GetOptions(mContent);
  NS_ASSERTION(options.get() != nsnull, "Collection of options is null!");

  if (options) {
    return GetOptionAsContent(options, aIndex);
  } 
  return nsnull;
}

//---------------------------------------------------------
// This returns the options collection for aContent, if any
//---------------------------------------------------------
already_AddRefed<nsIDOMHTMLOptionsCollection>
nsListControlFrame::GetOptions(nsIContent * aContent)
{
  nsIDOMHTMLOptionsCollection* options = nsnull;
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = do_QueryInterface(aContent);
  if (selectElement) {
    selectElement->GetOptions(&options);  // AddRefs (1)
  }

  return options;
}

//---------------------------------------------------------
// Returns the nsIDOMHTMLOptionElement for a given index 
// in the select's collection
//---------------------------------------------------------
already_AddRefed<nsIDOMHTMLOptionElement>
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
nsListControlFrame::IsContentSelected(nsIContent* aContent) const
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
nsListControlFrame::IsContentSelectedByIndex(PRInt32 aIndex) const 
{
  nsCOMPtr<nsIContent> content = GetOptionContent(aIndex);
  NS_ASSERTION(content, "Failed to retrieve option content");

  return IsContentSelected(content);
}

//---------------------------------------------------------
// gets the content (an option) by index and then set it as
// being selected or not selected
//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::OnOptionSelected(nsPresContext* aPresContext,
                                     PRInt32 aIndex,
                                     PRBool aSelected)
{
  if (aSelected) {
    ScrollToIndex(aIndex);
  }
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
void
nsListControlFrame::OnContentReset()
{
  ResetList(PR_TRUE);
}

//---------------------------------------------------------
// Resets the select back to it's original default values;
// those values as determined by the original HTML
//---------------------------------------------------------
void 
nsListControlFrame::ResetList(PRBool aAllowScrolling)
{
  // if all the frames aren't here 
  // don't bother reseting
  if (!mIsAllFramesHere) {
    return;
  }

  if (aAllowScrolling) {
    mPostChildrenLoadedReset = PR_TRUE;

    // Scroll to the selected index
    PRInt32 indexToSelect = kNothingSelected;

    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(mContent));
    NS_ASSERTION(selectElement, "No select element!");
    if (selectElement) {
      selectElement->GetSelectedIndex(&indexToSelect);
      ScrollToIndex(indexToSelect);
    }
  }

  mStartSelectionIndex = kNothingSelected;
  mEndSelectionIndex = kNothingSelected;

  // Combobox will redisplay itself with the OnOptionSelected event
} 
 
//---------------------------------------------------------
void 
nsListControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  if (aOn) {
    ComboboxFocusSet();
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
nsListControlFrame::SetComboboxFrame(nsIFrame* aComboboxFrame)
{
  if (nsnull != aComboboxFrame) {
    aComboboxFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame),(void**) &mComboboxFrame); 
  }
}


//---------------------------------------------------------
// Gets the text of the currently selected item
// if the there are zero items then an empty string is returned
// if there is nothing selected, then the 0th item's text is returned
//---------------------------------------------------------
void
nsListControlFrame::GetOptionText(PRInt32 aIndex, nsAString & aStr)
{
  aStr.SetLength(0);
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options = GetOptions(mContent);

  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);

    if (numOptions != 0) {
      nsCOMPtr<nsIDOMHTMLOptionElement> optionElement =
        GetOption(options, aIndex);
      if (optionElement) {
#if 0 // This is for turning off labels Bug 4050
        nsAutoString text;
        optionElement->GetLabel(text);
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
      }
    }
  }
}

//---------------------------------------------------------
PRInt32
nsListControlFrame::GetSelectedIndex()
{
  PRInt32 aIndex;
  
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(mContent));
  selectElement->GetSelectedIndex(&aIndex);
  
  return aIndex;
}

//---------------------------------------------------------
PRBool 
nsListControlFrame::IsInDropDownMode() const
{
  return (mComboboxFrame != nsnull);
}

//---------------------------------------------------------
PRInt32
nsListControlFrame::GetNumberOfOptions()
{
  if (mContent != nsnull) {
    nsCOMPtr<nsIDOMHTMLOptionsCollection> options = GetOptions(mContent);

    if (!options) {
      return 0;
    } else {
      PRUint32 length = 0;
      options->GetLength(&length);
      return (PRInt32)length;
    }
  }
  return 0;
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
      // if all the frames are now present we can initialize
      if (CheckIfAllFramesHere()) {
        mHasBeenInitialized = PR_TRUE;
        ResetList(PR_TRUE);
      }
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::AddOption(nsPresContext* aPresContext, PRInt32 aIndex)
{
#ifdef DO_REFLOW_DEBUG
  printf("---- Id: %d nsLCF %p Added Option %d\n", mReflowId, this, aIndex);
#endif

  if (!mIsAllContentHere) {
    mIsAllContentHere = mContent->IsDoneAddingChildren();
    if (!mIsAllContentHere) {
      mIsAllFramesHere    = PR_FALSE;
      mHasBeenInitialized = PR_FALSE;
    } else {
      mIsAllFramesHere = (aIndex == GetNumberOfOptions()-1);
    }
  }
  
  if (!mHasBeenInitialized) {
    return NS_OK;
  }

  // Make sure we scroll to the selected option as needed
  mNeedToReset = PR_TRUE;
  mPostChildrenLoadedReset = mIsAllContentHere;
  return NS_OK;
}

//-------------------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::RemoveOption(nsPresContext* aPresContext, PRInt32 aIndex)
{
  // Need to reset if we're a dropdown
  if (IsInDropDownMode()) {
    mNeedToReset = PR_TRUE;
    mPostChildrenLoadedReset = mIsAllContentHere;
  }

  if (mStartSelectionIndex >= aIndex) {
    --mStartSelectionIndex;
    if (mStartSelectionIndex < 0) {
      mStartSelectionIndex = kNothingSelected;
    }    
  }

  if (mEndSelectionIndex >= aIndex) {
    --mEndSelectionIndex;
    if (mEndSelectionIndex < 0) {
      mEndSelectionIndex = kNothingSelected;
    }    
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
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options = GetOptions(mContent);
  NS_ASSERTION(options, "No options");
  if (!options) {
    return PR_FALSE;
  }
  nsCOMPtr<nsIDOMHTMLOptionElement> option = GetOption(options, aIndex);
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
PRBool
nsListControlFrame::UpdateSelection()
{
  if (mIsAllFramesHere) {
    // if it's a combobox, display the new text
    if (mComboboxFrame) {
      mComboboxFrame->RedisplaySelectedText();
    }
    // if it's a listbox, fire on change
    else if (mIsAllContentHere) {
      nsWeakFrame weakFrame(this);
      FireOnChange();
      return weakFrame.IsAlive();
    }
  }
  return PR_TRUE;
}

void
nsListControlFrame::ComboboxFinish(PRInt32 aIndex)
{
  gLastKeyTime = 0;

  if (mComboboxFrame) {
    PerformSelection(aIndex, PR_FALSE, PR_FALSE);

    PRInt32 displayIndex = mComboboxFrame->GetIndexOfDisplayArea();

    if (displayIndex != aIndex) {
      mComboboxFrame->RedisplaySelectedText();
    }

    mComboboxFrame->RollupFromList();
  }
}

// Send out an onchange notification.
void
nsListControlFrame::FireOnChange()
{
  if (mComboboxFrame) {
    // Return hit without changing anything
    PRInt32 index = mComboboxFrame->UpdateRecentIndex(NS_SKIP_NOTIFY_INDEX);
    if (index == NS_SKIP_NOTIFY_INDEX)
      return;

    // See if the selection actually changed
    if (index == GetSelectedIndex())
      return;
  }

  // Dispatch the NS_FORM_CHANGE event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event(PR_TRUE, NS_FORM_CHANGE);

  nsIPresShell *presShell = GetPresContext()->GetPresShell();
  if (presShell) {
    presShell->HandleEventWithTarget(&event, this, nsnull, &status);
  }
}

//---------------------------------------------------------
// Determine if the specified item in the listbox is selected.
NS_IMETHODIMP
nsListControlFrame::GetOptionSelected(PRInt32 aIndex, PRBool* aValue)
{
  *aValue = IsContentSelectedByIndex(aIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::OnSetSelectedIndex(PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (mComboboxFrame) {
    // UpdateRecentIndex with NS_SKIP_NOTIFY_INDEX, so that we won't fire an onchange
    // event for this setting of selectedIndex.
    mComboboxFrame->UpdateRecentIndex(NS_SKIP_NOTIFY_INDEX);
  }

  ScrollToIndex(aNewIndex);
  mStartSelectionIndex = aNewIndex;
  mEndSelectionIndex = aNewIndex;

#ifdef ACCESSIBILITY
  FireMenuItemActiveEvent();
#endif

  return NS_OK;
}

//----------------------------------------------------------------------
// End nsISelectControlFrame
//----------------------------------------------------------------------

//---------------------------------------------------------
nsresult
nsListControlFrame::SetFormProperty(nsIAtom* aName,
                                const nsAString& aValue)
{
  if (nsGkAtoms::selected == aName) {
    return NS_ERROR_INVALID_ARG; // Selected is readonly according to spec.
  } else if (nsGkAtoms::selectedindex == aName) {
    // You shouldn't be calling me for this!!!
    return NS_ERROR_INVALID_ARG;
  }

  // We should be told about selectedIndex by the DOM element through
  // OnOptionSelected

  return NS_OK;
}

//---------------------------------------------------------
nsresult 
nsListControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  // Get the selected value of option from local cache (optimization vs. widget)
  if (nsGkAtoms::selected == aName) {
    nsAutoString val(aValue);
    PRInt32 error = 0;
    PRBool selected = PR_FALSE;
    PRInt32 indx = val.ToInteger(&error, 10); // Get index from aValue
    if (error == 0)
       selected = IsContentSelectedByIndex(indx); 
  
    aValue.Assign(selected ? NS_LITERAL_STRING("1") : NS_LITERAL_STRING("0"));
    
  // For selectedIndex, get the value from the widget
  } else if (nsGkAtoms::selectedindex == aName) {
    // You shouldn't be calling me for this!!!
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

//---------------------------------------------------------
void
nsListControlFrame::SyncViewWithFrame()
{
    // Resync the view's position with the frame.
    // The problem is the dropdown's view is attached directly under
    // the root view. This means its view needs to have its coordinates calculated
    // as if it were in it's normal position in the view hierarchy.
  mComboboxFrame->AbsolutelyPositionDropDown();

  nsContainerFrame::PositionFrameView(this);
}

//---------------------------------------------------------
void
nsListControlFrame::AboutToDropDown()
{
  if (mIsAllContentHere && mIsAllFramesHere && mHasBeenInitialized) {
    ScrollToIndex(GetSelectedIndex());
#ifdef ACCESSIBILITY
    FireMenuItemActiveEvent(); // Inform assistive tech what got focus
#endif
  }
  mItemSelectionStarted = PR_FALSE;
}

//---------------------------------------------------------
// We are about to be rolledup from the outside (ComboboxFrame)
void
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

  if (IsInDropDownMode()) {
    ComboboxFinish(mComboboxFrame->GetIndexOfDisplayArea());
  }
}

//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::DidReflow(nsPresContext*           aPresContext,
                              const nsHTMLReflowState* aReflowState,
                              nsDidReflowStatus        aStatus)
{
  nsresult rv;
  
  if (IsInDropDownMode()) 
  {
    //SyncViewWithFrame();
    rv = nsHTMLScrollFrame::DidReflow(aPresContext, aReflowState, aStatus);
    SyncViewWithFrame();
  } else {
    rv = nsHTMLScrollFrame::DidReflow(aPresContext, aReflowState, aStatus);
  }

  if (mNeedToReset) {
    mNeedToReset = PR_FALSE;
    // Suppress scrolling to the selected element if we restored
    // scroll history state AND the list contents have not changed
    // since we loaded all the children AND nothing else forced us
    // to scroll by calling ResetList(PR_TRUE). The latter two conditions
    // are folded into mPostChildrenLoadedReset.
    //
    // The idea is that we want scroll history restoration to trump ResetList
    // scrolling to the selected element, when the ResetList was probably only
    // caused by content loading normally.
    ResetList(!DidHistoryRestore() || mPostChildrenLoadedReset);
  }

  return rv;
}

nsIAtom*
nsListControlFrame::GetType() const
{
  return nsGkAtoms::listControlFrame; 
}

PRBool
nsListControlFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eReplaced | eReplacedContainsBlock));
}

PRBool
nsListControlFrame::IsContainingBlock() const
{
  // We are in fact the containing block for our options.  They should
  // certainly not use our parent block (or worse yet our parent combobox) for
  // their sizing.
  return PR_TRUE;
}

void
nsListControlFrame::InvalidateInternal(const nsRect& aDamageRect,
                                       nscoord aX, nscoord aY, nsIFrame* aForChild,
                                       PRBool aImmediate)
{
  if (!IsInDropDownMode())
    nsHTMLScrollFrame::InvalidateInternal(aDamageRect, aX, aY, aForChild, aImmediate);
  InvalidateRoot(aDamageRect, aX, aY, aImmediate);
}

#ifdef DEBUG
NS_IMETHODIMP
nsListControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ListControl"), aResult);
}
#endif

//---------------------------------------------------------
nscoord
nsListControlFrame::GetHeightOfARow()
{
  return HeightOfARow();
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

nscoord
nsListControlFrame::CalcFallbackRowHeight(PRInt32 aNumOptions)
{
  const nsStyleFont* styleFont = nsnull;
    
  if (aNumOptions > 0) {
    // Try the first option
    nsCOMPtr<nsIContent> option = GetOptionContent(0);
    if (option) {
      nsIFrame * optFrame = GetPresContext()->PresShell()->
        GetPrimaryFrameFor(option);
      if (optFrame) {
        styleFont = optFrame->GetStyleFont();
      }
    }
  }

  if (!styleFont) {
    // Fall back to our own font
    styleFont = GetStyleFont();
  }

  NS_ASSERTION(styleFont, "Must have font style by now!");

  nscoord rowHeight = 0;
  
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult result = GetPresContext()->DeviceContext()->
    GetMetricsFor(styleFont->mFont, *getter_AddRefs(fontMet));
  if (NS_SUCCEEDED(result) && fontMet) {
    if (fontMet) {
      fontMet->GetHeight(rowHeight);
    }
  }

  return rowHeight;
}

nscoord
nsListControlFrame::CalcIntrinsicHeight(nscoord aHeightOfARow,
                                        PRInt32 aNumberOfOptions)
{
  NS_PRECONDITION(!IsInDropDownMode(),
                  "Shouldn't be in dropdown mode when we call this");
  
  mNumDisplayRows = 1;
  GetSizeAttribute(&mNumDisplayRows);

  // Extra height to tack on to aHeightOfARow * mNumDisplayRows
  nscoord extraHeight = 0;
  
  if (mNumDisplayRows < 1) {
    // When SIZE=0 or unspecified we constrain the height to
    // [2..kMaxDropDownRows] rows.  We add in the height of optgroup labels
    // (within the constraint above), bug 300474.
    nscoord labelHeight =
      ::GetOptGroupLabelsHeight(GetPresContext(), mContent, aHeightOfARow);

    if (GetMultiple()) {
      if (aNumberOfOptions < 2) {
        // Add in 1 aHeightOfARow also when aNumberOfOptions == 0
        mNumDisplayRows = 1;
        extraHeight = PR_MAX(aHeightOfARow, labelHeight);
      }
      else if (aNumberOfOptions * aHeightOfARow + labelHeight >
               kMaxDropDownRows * aHeightOfARow) {
        mNumDisplayRows = kMaxDropDownRows;
      } else {
        mNumDisplayRows = aNumberOfOptions;
        extraHeight = labelHeight;
      }
    }
    else {
      NS_NOTREACHED("Shouldn't hit this case -- we should a be a combobox if "
                    "we have no size set and no multiple set!");
    }
  }

  return mNumDisplayRows * aHeightOfARow + extraHeight;
}

//----------------------------------------------------------------------
// nsIDOMMouseListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseUp(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");

  UpdateInListState(aMouseEvent);

  mButtonDown = PR_FALSE;

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return NS_OK;
  }

  // only allow selection with the left button
  // if a right button click is on the combobox itself
  // or on the select when in listbox mode, then let the click through
  if (!IsLeftButton(aMouseEvent)) {
    if (IsInDropDownMode()) {
      if (!IgnoreMouseEventForSelection(aMouseEvent)) {
        aMouseEvent->PreventDefault();
        aMouseEvent->StopPropagation();
      } else {
        CaptureMouseEvents(PR_FALSE);
        return NS_OK;
      }
      CaptureMouseEvents(PR_FALSE);
      return NS_ERROR_FAILURE; // means consume event
    } else {
      CaptureMouseEvents(PR_FALSE);
      return NS_OK;
    }
  }

  const nsStyleVisibility* vis = GetStyleVisibility();
      
  if (!vis->IsVisible()) {
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
      // If it's disabled, disallow the click and leave.
      PRBool isDisabled = PR_FALSE;
      IsOptionDisabled(selectedIndex, isDisabled);
      if (isDisabled) {
        aMouseEvent->PreventDefault();
        aMouseEvent->StopPropagation();
        CaptureMouseEvents(PR_FALSE);
        return NS_ERROR_FAILURE;
      }

      if (kNothingSelected != selectedIndex) {
        ComboboxFinish(selectedIndex);
        FireOnChange();
      }

      mouseEvent->clickCount = 1;
    } else {
      // the click was out side of the select or its dropdown
      mouseEvent->clickCount = IgnoreMouseEventForSelection(aMouseEvent) ? 1 : 0;
    }
  } else {
    CaptureMouseEvents(PR_FALSE);
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

/**
 * If the dropdown is showing and the mouse has moved below our
 * border-inner-edge, then set mItemSelectionStarted.
 */
void
nsListControlFrame::UpdateInListState(nsIDOMEvent* aEvent)
{
  if (!mComboboxFrame || !mComboboxFrame->IsDroppedDown())
    return;

  nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aEvent, this);
  nsRect borderInnerEdge = GetScrollableView()->View()->GetBounds();
  if (pt.y >= borderInnerEdge.y && pt.y < borderInnerEdge.YMost()) {
    mItemSelectionStarted = PR_TRUE;
  }
}

/**
 * When the user clicks on the comboboxframe to show the dropdown
 * listbox, they then have to move the mouse into the list. We don't
 * want to process those mouse events as selection events (i.e., to
 * scroll list items into view). So we ignore the events until
 * the mouse moves below our border-inner-edge, when
 * mItemSelectionStarted is set.
 *
 * @param aPoint relative to this frame
 */
PRBool nsListControlFrame::IgnoreMouseEventForSelection(nsIDOMEvent* aEvent)
{
  if (!mComboboxFrame)
    return PR_FALSE;

  // Our DOM listener does get called when the dropdown is not
  // showing, because it listens to events on the SELECT element
  if (!mComboboxFrame->IsDroppedDown())
    return PR_TRUE;

  return !mItemSelectionStarted;
}

//----------------------------------------------------------------------
// Fire a custom DOM event for the change, so that accessibility can
// fire a native focus event for accessibility 
// (Some 3rd party products need to track our focus)
#ifdef ACCESSIBILITY
void
nsListControlFrame::FireMenuItemActiveEvent()
{
  if (mFocused != this && !IsInDropDownMode()) {
    return;
  }

  // The mEndSelectionIndex is what is currently being selected
  // use the selected index if this is kNothingSelected
  PRInt32 focusedIndex;
  if (mEndSelectionIndex == kNothingSelected) {
    focusedIndex = GetSelectedIndex();
  } else {
    focusedIndex = mEndSelectionIndex;
  }
  if (focusedIndex == kNothingSelected) {
    return;
  }

  nsCOMPtr<nsIContent> optionContent = GetOptionContent(focusedIndex);
  if (!optionContent) {
    return;
  }

  FireDOMEvent(NS_LITERAL_STRING("DOMMenuItemActive"), optionContent);
}
#endif

//----------------------------------------------------------------------
// Sets the mSelectedIndex and mOldSelectedIndex from figuring out what 
// item was selected using content
// @param aPoint the event point, in listcontrolframe coordinates
// @return NS_OK if it successfully found the selection
//----------------------------------------------------------------------
nsresult
nsListControlFrame::GetIndexFromDOMEvent(nsIDOMEvent* aMouseEvent, 
                                         PRInt32&     aCurIndex)
{
  if (IgnoreMouseEventForSelection(aMouseEvent))
    return NS_ERROR_FAILURE;

  nsIView* view = GetScrolledFrame()->GetView();
  nsIViewManager* viewMan = view->GetViewManager();
  nsIView* curGrabber;
  viewMan->GetMouseEventGrabber(curGrabber);
  if (curGrabber != view) {
    // If we're not capturing, then ignore movement in the border
    nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aMouseEvent, this);
    nsRect borderInnerEdge = GetScrollableView()->View()->GetBounds();
    if (!borderInnerEdge.Contains(pt)) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIContent> content;
  GetPresContext()->EventStateManager()->
    GetEventTargetContent(nsnull, getter_AddRefs(content));

  nsCOMPtr<nsIContent> optionContent = GetOptionFromContent(content);
  if (optionContent) {
    aCurIndex = GetIndexFromContent(optionContent);
    return NS_OK;
  }

  nsIPresShell *presShell = GetPresContext()->PresShell();
  PRInt32 numOptions = GetNumberOfOptions();
  if (numOptions < 1)
    return NS_ERROR_FAILURE;

  nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aMouseEvent, this);

  // If the event coordinate is above the first option frame, then target the
  // first option frame
  nsCOMPtr<nsIContent> firstOption = GetOptionContent(0);
  NS_ASSERTION(firstOption, "Can't find first option that's supposed to be there");
  nsIFrame* optionFrame = presShell->GetPrimaryFrameFor(firstOption);
  if (optionFrame) {
    nsPoint ptInOptionFrame = pt - optionFrame->GetOffsetTo(this);
    if (ptInOptionFrame.y < 0 && ptInOptionFrame.x >= 0 &&
        ptInOptionFrame.x < optionFrame->GetSize().width) {
      aCurIndex = 0;
      return NS_OK;
    }
  }

  nsCOMPtr<nsIContent> lastOption = GetOptionContent(numOptions - 1);
  // If the event coordinate is below the last option frame, then target the
  // last option frame
  NS_ASSERTION(lastOption, "Can't find last option that's supposed to be there");
  optionFrame = presShell->GetPrimaryFrameFor(lastOption);
  if (optionFrame) {
    nsPoint ptInOptionFrame = pt - optionFrame->GetOffsetTo(this);
    if (ptInOptionFrame.y >= optionFrame->GetSize().height && ptInOptionFrame.x >= 0 &&
        ptInOptionFrame.x < optionFrame->GetSize().width) {
      aCurIndex = numOptions - 1;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");

  UpdateInListState(aMouseEvent);

  mButtonDown = PR_TRUE;

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return NS_OK;
  }

  // only allow selection with the left button
  // if a right button click is on the combobox itself
  // or on the select when in listbox mode, then let the click through
  if (!IsLeftButton(aMouseEvent)) {
    if (IsInDropDownMode()) {
      if (!IgnoreMouseEventForSelection(aMouseEvent)) {
        aMouseEvent->PreventDefault();
        aMouseEvent->StopPropagation();
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
    // Handle Like List
    CaptureMouseEvents(PR_TRUE);
    mChangesSinceDragStart = HandleListSelection(aMouseEvent, selectedIndex);
#ifdef ACCESSIBILITY
    if (mChangesSinceDragStart) {
      FireMenuItemActiveEvent();
    }
#endif
  } else {
    // NOTE: the combo box is responsible for dropping it down
    if (mComboboxFrame) {
      if (!IgnoreMouseEventForSelection(aMouseEvent)) {
        return NS_OK;
      }

      if (!nsComboboxControlFrame::ToolkitHasNativePopup())
      {
        PRBool isDroppedDown = mComboboxFrame->IsDroppedDown();
        mComboboxFrame->ShowDropDown(!isDroppedDown);
        if (isDroppedDown) {
          CaptureMouseEvents(PR_FALSE);
        }
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

  UpdateInListState(aMouseEvent);

  if (IsInDropDownMode()) { 
    if (mComboboxFrame->IsDroppedDown()) {
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

  UpdateInListState(aMouseEvent);

  if (!IsInDropDownMode()) { 
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
    // XXX shouldn't we just do nothing if we're asked to scroll to
    // kNothingSelected?
    return ScrollToFrame(nsnull);
  } else {
    nsCOMPtr<nsIContent> content = GetOptionContent(aIndex);
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
  nsIScrollableView* scrollableView = GetScrollableView();

  if (scrollableView) {
    // if null is passed in we scroll to 0,0
    if (nsnull == aOptElement) {
      scrollableView->ScrollTo(0, 0, PR_TRUE);
      return NS_OK;
    }
  
    // otherwise we find the content's frame and scroll to it
    nsIPresShell *presShell = GetPresContext()->PresShell();
    nsIFrame * childframe;
    if (aOptElement) {
      childframe = presShell->GetPrimaryFrameFor(aOptElement);
    } else {
      return NS_ERROR_FAILURE;
    }

    if (childframe) {
      if (scrollableView) {
        nscoord x;
        nscoord y;
        scrollableView->GetScrollPosition(x,y);
        // get the clipped rect
        nsRect rect = scrollableView->View()->GetBounds();
        // now move it by the offset of the scroll position
        rect.x = x;
        rect.y = y;

        // get the child
        nsRect fRect = childframe->GetRect();
        nsPoint pnt;
        nsIView * view;
        childframe->GetOffsetFromView(pnt, &view);

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
          nsIFrame * optFrame = presShell->GetPrimaryFrameFor(parentContent);
          if (optFrame) {
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
    startIndex = GetSelectedIndex();
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
  // Cocoa widgets do native popups, so don't try to show
  // dropdowns there.
  if (IsInDropDownMode() && !nsComboboxControlFrame::ToolkitHasNativePopup()) {
    mComboboxFrame->ShowDropDown(!mComboboxFrame->IsDroppedDown());
    mComboboxFrame->RedisplaySelectedText();
    aKeyEvent->PreventDefault();
  }
}

nsresult
nsListControlFrame::KeyPress(nsIDOMEvent* aKeyEvent)
{
  NS_ASSERTION(aKeyEvent, "keyEvent is null.");

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled))
    return NS_OK;

  // Start by making sure we can query for a key event
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_FAILURE);

  PRUint32 keycode = 0;
  PRUint32 charcode = 0;
  keyEvent->GetKeyCode(&keycode);
  keyEvent->GetCharCode(&charcode);

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
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options = GetOptions(mContent);
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
  
  if (isControl && (keycode == nsIDOMKeyEvent::DOM_VK_UP ||
                    keycode == nsIDOMKeyEvent::DOM_VK_LEFT ||
                    keycode == nsIDOMKeyEvent::DOM_VK_DOWN ||
                    keycode == nsIDOMKeyEvent::DOM_VK_RIGHT)) {
    // Don't go into multiple select mode unless this list can handle it
    isControl = mControlSelectMode = GetMultiple();
  } else if (charcode != ' ') {
    mControlSelectMode = PR_FALSE;
  }
  switch (keycode) {

    case nsIDOMKeyEvent::DOM_VK_UP:
    case nsIDOMKeyEvent::DOM_VK_LEFT: {
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                (PRInt32)numOptions,
                                -1, -1);
      } break;
    
    case nsIDOMKeyEvent::DOM_VK_DOWN:
    case nsIDOMKeyEvent::DOM_VK_RIGHT: {
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                (PRInt32)numOptions,
                                1, 1);
      } break;

    case nsIDOMKeyEvent::DOM_VK_RETURN: {
      if (mComboboxFrame != nsnull) {
        if (mComboboxFrame->IsDroppedDown()) {
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
      PRInt32 startIndex = GetSelectedIndex();
      if (startIndex == kNothingSelected) {
        startIndex = 0;
      } else if (stringLength == 1) {
        startIndex++;
      }

      PRUint32 i;
      for (i = 0; i < numOptions; i++) {
        PRUint32 index = (i + startIndex) % numOptions;
        nsCOMPtr<nsIDOMHTMLOptionElement> optionElement =
          GetOption(options, index);
        if (optionElement) {
          nsAutoString text;
          if (NS_OK == optionElement->GetText(text)) {
            if (StringBeginsWith(text, incrementalString,
                                 nsCaseInsensitiveStringComparator())) {
              PRBool wasChanged = PerformSelection(index, isShift, isControl);
              if (wasChanged) {
                // dispatch event, update combobox, etc.
                if (!UpdateSelection()) {
                  return NS_OK;
                }
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
    PRBool wasChanged = PR_FALSE;
    if (isControl && charcode != ' ') {
      mStartSelectionIndex = newIndex;
      mEndSelectionIndex = newIndex;
      ScrollToIndex(newIndex);
    } else if (mControlSelectMode && charcode == ' ') {
      wasChanged = SingleSelection(newIndex, PR_TRUE);
    } else {
      wasChanged = PerformSelection(newIndex, isShift, isControl);
    }
    if (wasChanged) {
       // dispatch event, update combobox, etc.
      if (!UpdateSelection()) {
        return NS_OK;
      }
    }
#ifdef ACCESSIBILITY
    if (charcode != ' ') {
      FireMenuItemActiveEvent();
    }
#endif

    // XXX - Are we cover up a problem here???
    // Why aren't they getting flushed each time?
    // because this isn't needed for Gfx
    if (IsInDropDownMode()) {
      // Don't flush anything but reflows lest it destroy us
      GetPresContext()->PresShell()->
        GetDocument()->FlushPendingNotifications(Flush_OnlyReflow);
    }

    // Make sure the SelectArea frame gets painted
    Invalidate(nsRect(0,0,mRect.width,mRect.height), PR_TRUE);

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


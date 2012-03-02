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
 *   Mats Palmgren <matspal@gmail.com>
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
#include "nsGkAtoms.h"
#include "nsIFormControl.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsComboboxControlFrame.h"
#include "nsIViewManager.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsWidgetsCID.h"
#include "nsIPresShell.h"
#include "nsHTMLParts.h"
#include "nsIDOMEventTarget.h"
#include "nsEventDispatcher.h"
#include "nsEventStateManager.h"
#include "nsEventListenerManager.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsFontMetrics.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMNSEvent.h"
#include "nsGUIEvent.h"
#include "nsIServiceManager.h"
#include "nsINodeInfo.h"
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif
#include "nsHTMLSelectElement.h"
#include "nsCSSRendering.h"
#include "nsITheme.h"
#include "nsIDOMEventListener.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"
#include "mozilla/LookAndFeel.h"

using namespace mozilla;

// Constants
const nscoord kMaxDropDownRows          = 20; // This matches the setting for 4.x browsers
const PRInt32 kNothingSelected          = -1;

// Static members
nsListControlFrame * nsListControlFrame::mFocused = nsnull;
nsString * nsListControlFrame::sIncrementalString = nsnull;

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

class nsListEventListener : public nsIDOMEventListener
{
public:
  nsListEventListener(nsListControlFrame *aFrame)
    : mFrame(aFrame) { }

  void SetFrame(nsListControlFrame *aFrame) { mFrame = aFrame; }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

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

NS_IMPL_FRAMEARENA_HELPERS(nsListControlFrame)

//---------------------------------------------------------
nsListControlFrame::nsListControlFrame(
  nsIPresShell* aShell, nsIDocument* aDocument, nsStyleContext* aContext)
  : nsHTMLScrollFrame(aShell, aContext, false),
    mMightNeedSecondPass(false),
    mHasPendingInterruptAtStartOfReflow(false),
    mLastDropdownComputedHeight(NS_UNCONSTRAINEDSIZE)
{
  mComboboxFrame      = nsnull;
  mChangesSinceDragStart = false;
  mButtonDown         = false;

  mIsAllContentHere   = false;
  mIsAllFramesHere    = false;
  mHasBeenInitialized = false;
  mNeedToReset        = true;
  mPostChildrenLoadedReset = false;

  mControlSelectMode           = false;
}

//---------------------------------------------------------
nsListControlFrame::~nsListControlFrame()
{
  mComboboxFrame = nsnull;
}

// for Bug 47302 (remove this comment later)
void
nsListControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // get the receiver interface from the browser button's content node
  ENSURE_TRUE(mContent);

  // Clear the frame pointer on our event listener, just in case the
  // event listener can outlive the frame.

  mEventListener->SetFrame(nsnull);

  mContent->RemoveEventListener(NS_LITERAL_STRING("keypress"), mEventListener,
                                false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("mousedown"), mEventListener,
                                false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("mouseup"), mEventListener,
                                false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("mousemove"), mEventListener,
                                false);

  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsHTMLScrollFrame::DestroyFrom(aDestructRoot);
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

  if (IsInDropDownMode()) {
    NS_ASSERTION(NS_GET_A(mLastDropdownBackstopColor) == 255,
                 "need an opaque backstop color");
    // XXX Because we have an opaque widget and we get called to paint with
    // this frame as the root of a stacking context we need make sure to draw
    // some opaque color over the whole widget. (Bug 511323)
    aLists.BorderBackground()->AppendNewToBottom(
      new (aBuilder) nsDisplaySolidColor(aBuilder,
        this, nsRect(aBuilder->ToReferenceFrame(this), GetSize()),
        mLastDropdownBackstopColor));
  }

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
void nsListControlFrame::PaintFocus(nsRenderingContext& aRC, nsPoint aPt)
{
  if (mFocused != this) return;

  nsPresContext* presContext = PresContext();

  nsIFrame* containerFrame = GetOptionsContainer();
  if (!containerFrame) return;

  nsIFrame* childframe = nsnull;
  nsCOMPtr<nsIContent> focusedContent = GetCurrentOption();
  if (focusedContent) {
    childframe = focusedContent->GetPrimaryFrame();
  }

  nsRect fRect;
  if (childframe) {
    // get the child rect
    fRect = childframe->GetRect();
    // get it into our coordinates
    fRect.MoveBy(childframe->GetParent()->GetOffsetTo(this));
  } else {
    float inflation = nsLayoutUtils::FontSizeInflationFor(this,
                        nsLayoutUtils::eNotInReflow);
    fRect.x = fRect.y = 0;
    fRect.width = GetScrollPortRect().width;
    fRect.height = CalcFallbackRowHeight(inflation);
    fRect.MoveBy(containerFrame->GetOffsetTo(this));
  }
  fRect += aPt;
  
  bool lastItemIsSelected = false;
  if (focusedContent) {
    nsCOMPtr<nsIDOMHTMLOptionElement> domOpt =
      do_QueryInterface(focusedContent);
    if (domOpt) {
      domOpt->GetSelected(&lastItemIsSelected);
    }
  }

  // set up back stop colors and then ask L&F service for the real colors
  nscolor color =
    LookAndFeel::GetColor(lastItemIsSelected ?
                            LookAndFeel::eColorID_WidgetSelectForeground :
                            LookAndFeel::eColorID_WidgetSelectBackground);

  nsCSSRendering::PaintFocus(presContext, aRC, fRect, color);
}

void
nsListControlFrame::InvalidateFocus(const nsHTMLReflowState *aReflowState)
{
  if (mFocused != this)
    return;

  nsIFrame* containerFrame = GetOptionsContainer();
  if (containerFrame) {
    // Invalidating from the containerFrame because that's where our focus
    // is drawn.
    // The origin of the scrollport is the origin of containerFrame.
    float inflation = nsLayoutUtils::FontSizeInflationFor(this,
                        aReflowState ? nsLayoutUtils::eInReflow
                                     : nsLayoutUtils::eNotInReflow);
    nsRect invalidateArea = containerFrame->GetVisualOverflowRect();
    nsRect emptyFallbackArea(0, 0, GetScrollPortRect().width,
                             CalcFallbackRowHeight(inflation));
    invalidateArea.UnionRect(invalidateArea, emptyFallbackArea);
    containerFrame->Invalidate(invalidateArea);
  }
}

NS_QUERYFRAME_HEAD(nsListControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsIListControlFrame)
  NS_QUERYFRAME_ENTRY(nsISelectControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsHTMLScrollFrame)

#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
nsListControlFrame::CreateAccessible()
{
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    return accService->CreateHTMLListboxAccessible(mContent,
                                                   PresContext()->PresShell());
  }

  return nsnull;
}
#endif

static nscoord
GetMaxOptionHeight(nsIFrame* aContainer)
{
  nscoord result = 0;
  for (nsIFrame* option = aContainer->GetFirstPrincipalChild();
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

static PRUint32
GetNumberOfOptionsRecursive(nsIContent* aContent)
{
  if (!aContent) {
    return 0;
  }

  PRUint32 optionCount = 0;
  for (nsIContent* cur = aContent->GetFirstChild();
       cur;
       cur = cur->GetNextSibling()) {
    if (cur->IsHTML(nsGkAtoms::option)) {
      ++optionCount;
    } else if (cur->IsHTML(nsGkAtoms::optgroup)) {
      optionCount += GetNumberOfOptionsRecursive(cur);
    }
  }
  return optionCount;
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

  // Check to see if we have zero items (and optimize by checking
  // heightOfARow first)
  if (heightOfARow == 0 && GetNumberOfOptions() == 0) {
    float inflation =
      nsLayoutUtils::FontSizeInflationInner(this, nsLayoutUtils::eInReflow);
    heightOfARow = CalcFallbackRowHeight(inflation);
  }

  return heightOfARow;
}

nscoord
nsListControlFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  // Always add scrollbar widths to the pref-width of the scrolled
  // content. Combobox frames depend on this happening in the dropdown,
  // and standalone listboxes are overflow:scroll so they need it too.
  result = GetScrolledFrame()->GetPrefWidth(aRenderingContext);
  result = NSCoordSaturatingAdd(result,
          GetDesiredScrollbarSizes(PresContext(), aRenderingContext).LeftRight());

  return result;
}

nscoord
nsListControlFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  // Always add scrollbar widths to the min-width of the scrolled
  // content. Combobox frames depend on this happening in the dropdown,
  // and standalone listboxes are overflow:scroll so they need it too.
  result = GetScrolledFrame()->GetMinWidth(aRenderingContext);
  result += GetDesiredScrollbarSizes(PresContext(), aRenderingContext).LeftRight();

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

  mHasPendingInterruptAtStartOfReflow = aPresContext->HasPendingInterrupt();

  // If all the content and frames are here 
  // then initialize it before reflow
  if (mIsAllContentHere && !mHasBeenInitialized) {
    if (false == mIsAllFramesHere) {
      CheckIfAllFramesHere();
    }
    if (mIsAllFramesHere && !mHasBeenInitialized) {
      mHasBeenInitialized = true;
    }
  }

  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
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
   * - We're not dirty and have no dirty kids and shouldn't be reflowing all
   *   kids.  In this case, our cached max height of a child is not going to
   *   change.
   * - We do our first reflow using our cached max height of a child, then
   *   compute the new max height and it's the same as the old one.
   */

  bool autoHeight = (aReflowState.ComputedHeight() == NS_UNCONSTRAINEDSIZE);

  mMightNeedSecondPass = autoHeight &&
    (NS_SUBTREE_DIRTY(this) || aReflowState.ShouldReflowAllKids());
  
  nsHTMLReflowState state(aReflowState);
  PRInt32 length = GetNumberOfOptions();  

  nscoord oldHeightOfARow = HeightOfARow();

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW) && autoHeight) {
    // When not doing an initial reflow, and when the height is auto, start off
    // with our computed height set to what we'd expect our height to be.
    nscoord computedHeight = CalcIntrinsicHeight(oldHeightOfARow, length);
    state.ApplyMinMaxConstraints(nsnull, &computedHeight);
    state.SetComputedHeight(computedHeight);
  }

  nsresult rv = nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize,
                                          state, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mMightNeedSecondPass) {
    NS_ASSERTION(!autoHeight || HeightOfARow() == oldHeightOfARow,
                 "How did our height of a row change if nothing was dirty?");
    NS_ASSERTION(!autoHeight ||
                 !(GetStateBits() & NS_FRAME_FIRST_REFLOW),
                 "How do we not need a second pass during initial reflow at "
                 "auto height?");
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if we don't need a second pass!");
    if (!autoHeight) {
      // Update our mNumDisplayRows based on our new row height now that we
      // know it.  Note that if autoHeight and we landed in this code then we
      // already set mNumDisplayRows in CalcIntrinsicHeight.  Also note that we
      // can't use HeightOfARow() here because that just uses a cached value
      // that we didn't compute.
      nscoord rowHeight = CalcHeightOfARow();
      if (rowHeight == 0) {
        // Just pick something
        mNumDisplayRows = 1;
      } else {
        mNumDisplayRows = NS_MAX(1, state.ComputedHeight() / rowHeight);
      }
    }

    return rv;
  }

  mMightNeedSecondPass = false;

  // Now see whether we need a second pass.  If we do, our nsSelectsAreaFrame
  // will have suppressed the scrollbar update.
  if (!IsScrollbarUpdateSuppressed()) {
    // All done.  No need to do more reflow.
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if the height of a row has not "
                 "changed!");
    return rv;
  }

  SetSuppressScrollbarUpdate(false);

  // Gotta reflow again.
  // XXXbz We're just changing the height here; do we need to dirty ourselves
  // or anything like that?  We might need to, per the letter of the reflow
  // protocol, but things seem to work fine without it...  Is that just an
  // implementation detail of nsHTMLScrollFrame that we're depending on?
  nsHTMLScrollFrame::DidReflow(aPresContext, &state, aStatus);

  // Now compute the height we want to have
  nscoord computedHeight = CalcIntrinsicHeight(HeightOfARow(), length); 
  state.ApplyMinMaxConstraints(nsnull, &computedHeight);
  state.SetComputedHeight(computedHeight);

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
  NS_PRECONDITION(aReflowState.ComputedHeight() == NS_UNCONSTRAINEDSIZE,
                  "We should not have a computed height here!");
  
  mMightNeedSecondPass = NS_SUBTREE_DIRTY(this) ||
    aReflowState.ShouldReflowAllKids();

#ifdef DEBUG
  nscoord oldHeightOfARow = HeightOfARow();
#endif

  nsHTMLReflowState state(aReflowState);

  nscoord oldVisibleHeight;
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // When not doing an initial reflow, and when the height is auto, start off
    // with our computed height set to what we'd expect our height to be.
    // Note: At this point, mLastDropdownComputedHeight can be
    // NS_UNCONSTRAINEDSIZE in cases when last time we didn't have to constrain
    // the height.  That's fine; just do the same thing as last time.
    state.SetComputedHeight(mLastDropdownComputedHeight);
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

  mMightNeedSecondPass = false;

  // Now see whether we need a second pass.  If we do, our nsSelectsAreaFrame
  // will have suppressed the scrollbar update.
  if (!IsScrollbarUpdateSuppressed()) {
    // All done.  No need to do more reflow.
    NS_ASSERTION(!(GetStateBits() & NS_FRAME_FIRST_REFLOW),
                 "How can we avoid a second pass during first reflow?");
    return rv;
  }

  SetSuppressScrollbarUpdate(false);

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
    nsRect screen = nsFormControlFrame::GetUsableScreenRect(aPresContext);
    nscoord screenHeight = screen.height;

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

    state.SetComputedHeight(mNumDisplayRows * heightOfARow);
    // Note: no need to apply min/max constraints, since we have no such
    // rules applied to the combobox dropdown.
    // XXXbz this is ending up too big!!  Figure out why.
  } else if (visibleHeight == 0) {
    // Looks like we have no options.  Just size us to a single row height.
    state.SetComputedHeight(heightOfARow);
  } else {
    // Not too big, not too small.  Just use it!
    state.SetComputedHeight(NS_UNCONSTRAINEDSIZE);
  }

  // Note: At this point, state.mComputedHeight can be NS_UNCONSTRAINEDSIZE in
  // cases when there were some options, but not too many (so no scrollbar was
  // needed).  That's fine; just store that.
  mLastDropdownComputedHeight = state.ComputedHeight();

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

bool
nsListControlFrame::ShouldPropagateComputedHeightToScrolledContent() const
{
  return !IsInDropDownMode();
}

//---------------------------------------------------------
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
    if (content->IsHTML(nsGkAtoms::option)) {
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
bool
nsListControlFrame::ExtendedSelection(PRInt32 aStartIndex,
                                      PRInt32 aEndIndex,
                                      bool aClearAll)
{
  return SetOptionsSelectedFromFrame(aStartIndex, aEndIndex,
                                     true, aClearAll);
}

//---------------------------------------------------------
bool
nsListControlFrame::SingleSelection(PRInt32 aClickedIndex, bool aDoToggle)
{
  if (mComboboxFrame) {
    mComboboxFrame->UpdateRecentIndex(GetSelectedIndex());
  }

  bool wasChanged = false;
  // Get Current selection
  if (aDoToggle) {
    wasChanged = ToggleOptionSelectedFromFrame(aClickedIndex);
  } else {
    wasChanged = SetOptionsSelectedFromFrame(aClickedIndex, aClickedIndex,
                                true, true);
  }
  ScrollToIndex(aClickedIndex);

#ifdef ACCESSIBILITY
  bool isCurrentOptionChanged = mEndSelectionIndex != aClickedIndex;
#endif
  mStartSelectionIndex = aClickedIndex;
  mEndSelectionIndex = aClickedIndex;
  InvalidateFocus();

#ifdef ACCESSIBILITY
  if (isCurrentOptionChanged) {
    FireMenuItemActiveEvent();
  }
#endif

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
      bool selected;
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
bool
nsListControlFrame::PerformSelection(PRInt32 aClickedIndex,
                                     bool aIsShift,
                                     bool aIsControl)
{
  bool wasChanged = false;

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
      }
#ifdef ACCESSIBILITY
      bool isCurrentOptionChanged = mEndSelectionIndex != aClickedIndex;
#endif
      mEndSelectionIndex = aClickedIndex;
      InvalidateFocus();

#ifdef ACCESSIBILITY
      if (isCurrentOptionChanged) {
        FireMenuItemActiveEvent();
      }
#endif
    } else if (aIsControl) {
      wasChanged = SingleSelection(aClickedIndex, true);
    } else {
      wasChanged = SingleSelection(aClickedIndex, false);
    }
  } else {
    wasChanged = SingleSelection(aClickedIndex, false);
  }

  return wasChanged;
}

//---------------------------------------------------------
bool
nsListControlFrame::HandleListSelection(nsIDOMEvent* aEvent,
                                        PRInt32 aClickedIndex)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aEvent);
  bool isShift;
  bool isControl;
#ifdef XP_MACOSX
  mouseEvent->GetMetaKey(&isControl);
#else
  mouseEvent->GetCtrlKey(&isControl);
#endif
  mouseEvent->GetShiftKey(&isShift);
  return PerformSelection(aClickedIndex, isShift, isControl);
}

//---------------------------------------------------------
void
nsListControlFrame::CaptureMouseEvents(bool aGrabMouseEvents)
{
  // Currently cocoa widgets use a native popup widget which tracks clicks synchronously,
  // so we never want to do mouse capturing. Note that we only bail if the list
  // is in drop-down mode, and the caller is requesting capture (we let release capture
  // requests go through to ensure that we can release capture requested via other
  // code paths, if any exist).
  if (aGrabMouseEvents && IsInDropDownMode() && nsComboboxControlFrame::ToolkitHasNativePopup())
    return;

  if (aGrabMouseEvents) {
    nsIPresShell::SetCapturingContent(mContent, CAPTURE_IGNOREALLOWED);
  } else {
    nsIContent* capturingContent = nsIPresShell::GetCapturingContent();

    bool dropDownIsHidden = false;
    if (IsInDropDownMode()) {
      dropDownIsHidden = !mComboboxFrame->IsDroppedDown();
    }
    if (capturingContent == mContent || dropDownIsHidden) {
      // only clear the capturing content if *we* are the ones doing the
      // capturing (or if the dropdown is hidden, in which case NO-ONE should
      // be capturing anything - it could be a scrollbar inside this listbox
      // which is actually grabbing
      // This shouldn't be necessary. We should simply ensure that events targeting
      // scrollbars are never visible to DOM consumers.
      nsIPresShell::SetCapturingContent(nsnull, 0);
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

  nsEventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED))
    return NS_OK;

  return nsHTMLScrollFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


//---------------------------------------------------------
NS_IMETHODIMP
nsListControlFrame::SetInitialChildList(ChildListID    aListID,
                                        nsFrameList&   aChildList)
{
  // First check to see if all the content has been added
  mIsAllContentHere = mContent->IsDoneAddingChildren();
  if (!mIsAllContentHere) {
    mIsAllFramesHere    = false;
    mHasBeenInitialized = false;
  }
  nsresult rv = nsHTMLScrollFrame::SetInitialChildList(aListID, aChildList);

  // If all the content is here now check
  // to see if all the frames have been created
  /*if (mIsAllContentHere) {
    // If all content and frames are here
    // the reset/initialize
    if (CheckIfAllFramesHere()) {
      ResetList(aPresContext);
      mHasBeenInitialized = true;
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
  NS_ENSURE_STATE(mContent);

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  // we need to hook up our listeners before the editor is initialized
  mEventListener = new nsListEventListener(this);
  if (!mEventListener) 
    return NS_ERROR_OUT_OF_MEMORY;

  mContent->AddEventListener(NS_LITERAL_STRING("keypress"), mEventListener,
                             false, false);
  mContent->AddEventListener(NS_LITERAL_STRING("mousedown"), mEventListener,
                             false, false);
  mContent->AddEventListener(NS_LITERAL_STRING("mouseup"), mEventListener,
                             false, false);
  mContent->AddEventListener(NS_LITERAL_STRING("mousemove"), mEventListener,
                             false, false);

  mStartSelectionIndex = kNothingSelected;
  mEndSelectionIndex = kNothingSelected;

  mLastDropdownBackstopColor = PresContext()->DefaultBackgroundColor();

  return result;
}

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

bool 
nsListControlFrame::IsContentSelected(nsIContent* aContent) const
{
  bool isSelected = false;

  nsCOMPtr<nsIDOMHTMLOptionElement> optEl = do_QueryInterface(aContent);
  if (optEl)
    optEl->GetSelected(&isSelected);

  return isSelected;
}

bool 
nsListControlFrame::IsContentSelectedByIndex(PRInt32 aIndex) const 
{
  nsCOMPtr<nsIContent> content = GetOptionContent(aIndex);
  NS_ASSERTION(content, "Failed to retrieve option content");

  return IsContentSelected(content);
}

NS_IMETHODIMP
nsListControlFrame::OnOptionSelected(PRInt32 aIndex, bool aSelected)
{
  if (aSelected) {
    ScrollToIndex(aIndex);
  }
  return NS_OK;
}

PRIntn
nsListControlFrame::GetSkipSides() const
{    
    // Don't skip any sides during border rendering
  return 0;
}

void
nsListControlFrame::OnContentReset()
{
  ResetList(true);
}

void 
nsListControlFrame::ResetList(bool aAllowScrolling,
                              const nsHTMLReflowState *aReflowState)
{
  // if all the frames aren't here 
  // don't bother reseting
  if (!mIsAllFramesHere) {
    return;
  }

  if (aAllowScrolling) {
    mPostChildrenLoadedReset = true;

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
  InvalidateFocus(aReflowState);
  // Combobox will redisplay itself with the OnOptionSelected event
} 
 
void 
nsListControlFrame::SetFocus(bool aOn, bool aRepaint)
{
  InvalidateFocus();

  if (aOn) {
    ComboboxFocusSet();
    mFocused = this;
  } else {
    mFocused = nsnull;
  }

  InvalidateFocus();
}

void nsListControlFrame::ComboboxFocusSet()
{
  gLastKeyTime = 0;
}

void
nsListControlFrame::SetComboboxFrame(nsIFrame* aComboboxFrame)
{
  if (nsnull != aComboboxFrame) {
    mComboboxFrame = do_QueryFrame(aComboboxFrame);
  }
}

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
          compressText.CompressWhitespace(true, true);
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

PRInt32
nsListControlFrame::GetSelectedIndex()
{
  PRInt32 aIndex;
  
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(mContent));
  selectElement->GetSelectedIndex(&aIndex);
  
  return aIndex;
}

already_AddRefed<nsIContent>
nsListControlFrame::GetCurrentOption()
{
  // The mEndSelectionIndex is what is currently being selected. Use
  // the selected index if this is kNothingSelected.
  PRInt32 focusedIndex = (mEndSelectionIndex == kNothingSelected) ?
    GetSelectedIndex() : mEndSelectionIndex;

  if (focusedIndex != kNothingSelected) {
    return GetOptionContent(focusedIndex);
  }

  nsRefPtr<nsHTMLSelectElement> selectElement =
    nsHTMLSelectElement::FromContent(mContent);
  NS_ASSERTION(selectElement, "Can't be null");

  // There is no a selected item return the first non-disabled item and skip all
  // the option group elements.
  nsCOMPtr<nsIDOMNode> node;

  PRUint32 length;
  selectElement->GetLength(&length);
  if (length) {
    bool isDisabled = true;
    for (PRUint32 i = 0; i < length && isDisabled; i++) {
      if (NS_FAILED(selectElement->Item(i, getter_AddRefs(node))) || !node) {
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
      return nsnull;
    }
  }

  if (node) {
    nsCOMPtr<nsIContent> focusedOption = do_QueryInterface(node);
    return focusedOption.forget();
  }
  return nsnull;
}

bool 
nsListControlFrame::IsInDropDownMode() const
{
  return (mComboboxFrame != nsnull);
}

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
bool nsListControlFrame::CheckIfAllFramesHere()
{
  // Get the number of optgroups and options
  //PRInt32 numContentItems = 0;
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  if (node) {
    // XXX Need to find a fail proff way to determine that
    // all the frames are there
    mIsAllFramesHere = true;//NS_OK == CountAllChild(node, numContentItems);
  }
  // now make sure we have a frame each piece of content

  return mIsAllFramesHere;
}

NS_IMETHODIMP
nsListControlFrame::DoneAddingChildren(bool aIsDone)
{
  mIsAllContentHere = aIsDone;
  if (mIsAllContentHere) {
    // Here we check to see if all the frames have been created 
    // for all the content.
    // If so, then we can initialize;
    if (!mIsAllFramesHere) {
      // if all the frames are now present we can initialize
      if (CheckIfAllFramesHere()) {
        mHasBeenInitialized = true;
        ResetList(true);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsListControlFrame::AddOption(PRInt32 aIndex)
{
#ifdef DO_REFLOW_DEBUG
  printf("---- Id: %d nsLCF %p Added Option %d\n", mReflowId, this, aIndex);
#endif

  if (!mIsAllContentHere) {
    mIsAllContentHere = mContent->IsDoneAddingChildren();
    if (!mIsAllContentHere) {
      mIsAllFramesHere    = false;
      mHasBeenInitialized = false;
    } else {
      mIsAllFramesHere = (aIndex == GetNumberOfOptions()-1);
    }
  }
  
  // Make sure we scroll to the selected option as needed
  mNeedToReset = true;

  if (!mHasBeenInitialized) {
    return NS_OK;
  }

  mPostChildrenLoadedReset = mIsAllContentHere;
  return NS_OK;
}

static PRInt32
DecrementAndClamp(PRInt32 aSelectionIndex, PRInt32 aLength)
{
  return aLength == 0 ? kNothingSelected : NS_MAX(0, aSelectionIndex - 1);
}

NS_IMETHODIMP
nsListControlFrame::RemoveOption(PRInt32 aIndex)
{
  NS_PRECONDITION(aIndex >= 0, "negative <option> index");

  // Need to reset if we're a dropdown
  if (IsInDropDownMode()) {
    mNeedToReset = true;
    mPostChildrenLoadedReset = mIsAllContentHere;
  }

  if (mStartSelectionIndex != kNothingSelected) {
    NS_ASSERTION(mEndSelectionIndex != kNothingSelected, "");
    PRInt32 numOptions = GetNumberOfOptions();
    // NOTE: numOptions is the new number of options whereas aIndex is the
    // unadjusted index of the removed option (hence the <= below).
    NS_ASSERTION(aIndex <= numOptions, "out-of-bounds <option> index");

    PRInt32 forward = mEndSelectionIndex - mStartSelectionIndex;
    PRInt32* low  = forward >= 0 ? &mStartSelectionIndex : &mEndSelectionIndex;
    PRInt32* high = forward >= 0 ? &mEndSelectionIndex : &mStartSelectionIndex;
    if (aIndex < *low)
      *low = ::DecrementAndClamp(*low, numOptions);
    if (aIndex <= *high)
      *high = ::DecrementAndClamp(*high, numOptions);
    if (forward == 0)
      *low = *high;
  }
  else
    NS_ASSERTION(mEndSelectionIndex == kNothingSelected, "");

  InvalidateFocus();
  return NS_OK;
}

//---------------------------------------------------------
// Set the option selected in the DOM.  This method is named
// as it is because it indicates that the frame is the source
// of this event rather than the receiver.
bool
nsListControlFrame::SetOptionsSelectedFromFrame(PRInt32 aStartIndex,
                                                PRInt32 aEndIndex,
                                                bool aValue,
                                                bool aClearAll)
{
  nsRefPtr<nsHTMLSelectElement> selectElement =
    nsHTMLSelectElement::FromContent(mContent);
  bool wasChanged = false;
#ifdef DEBUG
  nsresult rv = 
#endif
    selectElement->SetOptionsSelectedByIndex(aStartIndex,
                                             aEndIndex,
                                             aValue,
                                             aClearAll,
                                             false,
                                             true,
                                             &wasChanged);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetSelected failed");
  return wasChanged;
}

bool
nsListControlFrame::ToggleOptionSelectedFromFrame(PRInt32 aIndex)
{
  nsCOMPtr<nsIDOMHTMLOptionsCollection> options = GetOptions(mContent);
  NS_ASSERTION(options, "No options");
  if (!options) {
    return false;
  }
  nsCOMPtr<nsIDOMHTMLOptionElement> option = GetOption(options, aIndex);
  NS_ASSERTION(option, "No option");
  if (!option) {
    return false;
  }

  bool value = false;
#ifdef DEBUG
  nsresult rv =
#endif
    option->GetSelected(&value);

  NS_ASSERTION(NS_SUCCEEDED(rv), "GetSelected failed");
  nsRefPtr<nsHTMLSelectElement> selectElement =
    nsHTMLSelectElement::FromContent(mContent);
  bool wasChanged = false;
#ifdef DEBUG
  rv =
#endif
    selectElement->SetOptionsSelectedByIndex(aIndex,
                                             aIndex,
                                             !value,
                                             false,
                                             false,
                                             true,
                                             &wasChanged);

  NS_ASSERTION(NS_SUCCEEDED(rv), "SetSelected failed");

  return wasChanged;
}


// Dispatch event and such
bool
nsListControlFrame::UpdateSelection()
{
  if (mIsAllFramesHere) {
    // if it's a combobox, display the new text
    nsWeakFrame weakFrame(this);
    if (mComboboxFrame) {
      mComboboxFrame->RedisplaySelectedText();
    }
    // if it's a listbox, fire on change
    else if (mIsAllContentHere) {
      FireOnChange();
    }
    return weakFrame.IsAlive();
  }
  return true;
}

void
nsListControlFrame::ComboboxFinish(PRInt32 aIndex)
{
  gLastKeyTime = 0;

  if (mComboboxFrame) {
    PerformSelection(aIndex, false, false);

    PRInt32 displayIndex = mComboboxFrame->GetIndexOfDisplayArea();

    nsWeakFrame weakFrame(this);

    if (displayIndex != aIndex) {
      mComboboxFrame->RedisplaySelectedText(); // might destroy us
    }

    if (weakFrame.IsAlive() && mComboboxFrame) {
      mComboboxFrame->RollupFromList(); // might destroy us
    }
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

  // Dispatch the change event.
  nsContentUtils::DispatchTrustedEvent(mContent->OwnerDoc(), mContent,
                                       NS_LITERAL_STRING("change"), true,
                                       false);
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
  InvalidateFocus();

#ifdef ACCESSIBILITY
  FireMenuItemActiveEvent();
#endif

  return NS_OK;
}

//----------------------------------------------------------------------
// End nsISelectControlFrame
//----------------------------------------------------------------------

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

nsresult 
nsListControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  // Get the selected value of option from local cache (optimization vs. widget)
  if (nsGkAtoms::selected == aName) {
    nsAutoString val(aValue);
    PRInt32 error = 0;
    bool selected = false;
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

void
nsListControlFrame::AboutToDropDown()
{
  NS_ASSERTION(IsInDropDownMode(),
    "AboutToDropDown called without being in dropdown mode");

  // Our widget doesn't get invalidated on changes to the rest of the document,
  // so compute and store this color at the start of a dropdown so we don't
  // get weird painting behaviour.
  // We start looking for backgrounds above the combobox frame to avoid
  // duplicating the combobox frame's background and compose each background
  // color we find underneath until we have an opaque color, or run out of
  // backgrounds. We compose with the PresContext default background color,
  // which is always opaque, in case we don't end up with an opaque color.
  // This gives us a very poor approximation of translucency.
  nsIFrame* comboboxFrame = do_QueryFrame(mComboboxFrame);
  nsStyleContext* context = comboboxFrame->GetStyleContext()->GetParent();
  mLastDropdownBackstopColor = NS_RGBA(0,0,0,0);
  while (NS_GET_A(mLastDropdownBackstopColor) < 255 && context) {
    mLastDropdownBackstopColor =
      NS_ComposeColors(context->GetStyleBackground()->mBackgroundColor,
                       mLastDropdownBackstopColor);
    context = context->GetParent();
  }
  mLastDropdownBackstopColor =
    NS_ComposeColors(PresContext()->DefaultBackgroundColor(),
                     mLastDropdownBackstopColor);

  if (mIsAllContentHere && mIsAllFramesHere && mHasBeenInitialized) {
    ScrollToIndex(GetSelectedIndex());
#ifdef ACCESSIBILITY
    FireMenuItemActiveEvent(); // Inform assistive tech what got focus
#endif
  }
  mItemSelectionStarted = false;
}

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
    ComboboxFinish(mComboboxFrame->GetIndexOfDisplayArea()); // might destroy us
  }
}

NS_IMETHODIMP
nsListControlFrame::DidReflow(nsPresContext*           aPresContext,
                              const nsHTMLReflowState* aReflowState,
                              nsDidReflowStatus        aStatus)
{
  nsresult rv;
  bool wasInterrupted = !mHasPendingInterruptAtStartOfReflow &&
                          aPresContext->HasPendingInterrupt();

  if (IsInDropDownMode()) 
  {
    //SyncViewWithFrame();
    rv = nsHTMLScrollFrame::DidReflow(aPresContext, aReflowState, aStatus);
    SyncViewWithFrame();
  } else {
    rv = nsHTMLScrollFrame::DidReflow(aPresContext, aReflowState, aStatus);
  }

  if (mNeedToReset && !wasInterrupted) {
    mNeedToReset = false;
    // Suppress scrolling to the selected element if we restored
    // scroll history state AND the list contents have not changed
    // since we loaded all the children AND nothing else forced us
    // to scroll by calling ResetList(true). The latter two conditions
    // are folded into mPostChildrenLoadedReset.
    //
    // The idea is that we want scroll history restoration to trump ResetList
    // scrolling to the selected element, when the ResetList was probably only
    // caused by content loading normally.
    ResetList(!DidHistoryRestore() || mPostChildrenLoadedReset, aReflowState);
  }

  mHasPendingInterruptAtStartOfReflow = false;
  return rv;
}

nsIAtom*
nsListControlFrame::GetType() const
{
  return nsGkAtoms::listControlFrame; 
}

void
nsListControlFrame::InvalidateInternal(const nsRect& aDamageRect,
                                       nscoord aX, nscoord aY, nsIFrame* aForChild,
                                       PRUint32 aFlags)
{
  if (!IsInDropDownMode()) {
    nsHTMLScrollFrame::InvalidateInternal(aDamageRect, aX, aY, this, aFlags);
    return;
  }
  InvalidateRoot(aDamageRect + nsPoint(aX, aY), aFlags);
}

#ifdef DEBUG
NS_IMETHODIMP
nsListControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ListControl"), aResult);
}
#endif

nscoord
nsListControlFrame::GetHeightOfARow()
{
  return HeightOfARow();
}

nsresult
nsListControlFrame::IsOptionDisabled(PRInt32 anIndex, bool &aIsDisabled)
{
  nsRefPtr<nsHTMLSelectElement> sel =
    nsHTMLSelectElement::FromContent(mContent);
  if (sel) {
    sel->IsOptionDisabled(anIndex, &aIsDisabled);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// helper
//----------------------------------------------------------------------
bool
nsListControlFrame::IsLeftButton(nsIDOMEvent* aMouseEvent)
{
  // only allow selection with the left button
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (mouseEvent) {
    PRUint16 whichButton;
    if (NS_SUCCEEDED(mouseEvent->GetButton(&whichButton))) {
      return whichButton != 0?false:true;
    }
  }
  return false;
}

nscoord
nsListControlFrame::CalcFallbackRowHeight(float aFontSizeInflation)
{
  nscoord rowHeight = 0;

  nsRefPtr<nsFontMetrics> fontMet;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet),
                                        aFontSizeInflation);
  if (fontMet) {
    rowHeight = fontMet->MaxHeight();
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

  if (mNumDisplayRows < 1) {
    mNumDisplayRows = 4;
  }

  return mNumDisplayRows * aHeightOfARow;
}

//----------------------------------------------------------------------
// nsIDOMMouseListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseUp(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_FAILURE);

  UpdateInListState(aMouseEvent);

  mButtonDown = false;

  nsEventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
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
        CaptureMouseEvents(false);
        return NS_OK;
      }
      CaptureMouseEvents(false);
      return NS_ERROR_FAILURE; // means consume event
    } else {
      CaptureMouseEvents(false);
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
    mouseEvent = (nsMouseEvent *) privateEvent->GetInternalNSEvent();

    PRInt32 selectedIndex;
    if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
      // If it's disabled, disallow the click and leave.
      bool isDisabled = false;
      IsOptionDisabled(selectedIndex, isDisabled);
      if (isDisabled) {
        aMouseEvent->PreventDefault();
        aMouseEvent->StopPropagation();
        CaptureMouseEvents(false);
        return NS_ERROR_FAILURE;
      }

      if (kNothingSelected != selectedIndex) {
        nsWeakFrame weakFrame(this);
        ComboboxFinish(selectedIndex);
        if (!weakFrame.IsAlive())
          return NS_OK;
        FireOnChange();
      }

      mouseEvent->clickCount = 1;
    } else {
      // the click was out side of the select or its dropdown
      mouseEvent->clickCount = IgnoreMouseEventForSelection(aMouseEvent) ? 1 : 0;
    }
  } else {
    CaptureMouseEvents(false);
    // Notify
    if (mChangesSinceDragStart) {
      // reset this so that future MouseUps without a prior MouseDown
      // won't fire onchange
      mChangesSinceDragStart = false;
      FireOnChange();
    }
  }

  return NS_OK;
}

void
nsListControlFrame::UpdateInListState(nsIDOMEvent* aEvent)
{
  if (!mComboboxFrame || !mComboboxFrame->IsDroppedDown())
    return;

  nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aEvent, this);
  nsRect borderInnerEdge = GetScrollPortRect();
  if (pt.y >= borderInnerEdge.y && pt.y < borderInnerEdge.YMost()) {
    mItemSelectionStarted = true;
  }
}

bool nsListControlFrame::IgnoreMouseEventForSelection(nsIDOMEvent* aEvent)
{
  if (!mComboboxFrame)
    return false;

  // Our DOM listener does get called when the dropdown is not
  // showing, because it listens to events on the SELECT element
  if (!mComboboxFrame->IsDroppedDown())
    return true;

  return !mItemSelectionStarted;
}

#ifdef ACCESSIBILITY
void
nsListControlFrame::FireMenuItemActiveEvent()
{
  if (mFocused != this && !IsInDropDownMode()) {
    return;
  }

  nsCOMPtr<nsIContent> optionContent = GetCurrentOption();
  if (!optionContent) {
    return;
  }

  FireDOMEvent(NS_LITERAL_STRING("DOMMenuItemActive"), optionContent);
}
#endif

nsresult
nsListControlFrame::GetIndexFromDOMEvent(nsIDOMEvent* aMouseEvent, 
                                         PRInt32&     aCurIndex)
{
  if (IgnoreMouseEventForSelection(aMouseEvent))
    return NS_ERROR_FAILURE;

  if (nsIPresShell::GetCapturingContent() != mContent) {
    // If we're not capturing, then ignore movement in the border
    nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aMouseEvent, this);
    nsRect borderInnerEdge = GetScrollPortRect();
    if (!borderInnerEdge.Contains(pt)) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIContent> content = PresContext()->EventStateManager()->
    GetEventTargetContent(nsnull);

  nsCOMPtr<nsIContent> optionContent = GetOptionFromContent(content);
  if (optionContent) {
    aCurIndex = GetIndexFromContent(optionContent);
    return NS_OK;
  }

  PRInt32 numOptions = GetNumberOfOptions();
  if (numOptions < 1)
    return NS_ERROR_FAILURE;

  nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aMouseEvent, this);

  // If the event coordinate is above the first option frame, then target the
  // first option frame
  nsCOMPtr<nsIContent> firstOption = GetOptionContent(0);
  NS_ASSERTION(firstOption, "Can't find first option that's supposed to be there");
  nsIFrame* optionFrame = firstOption->GetPrimaryFrame();
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
  optionFrame = lastOption->GetPrimaryFrame();
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

nsresult
nsListControlFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nsnull, "aMouseEvent is null.");

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_FAILURE);

  UpdateInListState(aMouseEvent);

  nsEventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
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
    mButtonDown = true;
    CaptureMouseEvents(true);
    mChangesSinceDragStart = HandleListSelection(aMouseEvent, selectedIndex);
  } else {
    // NOTE: the combo box is responsible for dropping it down
    if (mComboboxFrame) {
      if (!IgnoreMouseEventForSelection(aMouseEvent)) {
        return NS_OK;
      }

      if (!nsComboboxControlFrame::ToolkitHasNativePopup())
      {
        bool isDroppedDown = mComboboxFrame->IsDroppedDown();
        nsIFrame* comboFrame = do_QueryFrame(mComboboxFrame);
        nsWeakFrame weakFrame(comboFrame);
        mComboboxFrame->ShowDropDown(!isDroppedDown);
        if (!weakFrame.IsAlive())
          return NS_OK;
        if (isDroppedDown) {
          CaptureMouseEvents(false);
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
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_FAILURE);

  UpdateInListState(aMouseEvent);

  if (IsInDropDownMode()) { 
    if (mComboboxFrame->IsDroppedDown()) {
      PRInt32 selectedIndex;
      if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
        PerformSelection(selectedIndex, false, false);
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
      bool isControl;
#ifdef XP_MACOSX
      mouseEvent->GetMetaKey(&isControl);
#else
      mouseEvent->GetCtrlKey(&isControl);
#endif
      // Turn SHIFT on when you are dragging, unless control is on.
      bool wasChanged = PerformSelection(selectedIndex,
                                           !isControl, isControl);
      mChangesSinceDragStart = mChangesSinceDragStart || wasChanged;
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// Scroll helpers.
//----------------------------------------------------------------------
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

nsresult
nsListControlFrame::ScrollToFrame(nsIContent* aOptElement)
{
  // if null is passed in we scroll to 0,0
  if (nsnull == aOptElement) {
    ScrollTo(nsPoint(0, 0), nsIScrollableFrame::INSTANT);
    return NS_OK;
  }

  // otherwise we find the content's frame and scroll to it
  nsIFrame *childFrame = aOptElement->GetPrimaryFrame();
  if (childFrame) {
    nsPoint pt = GetScrollPosition();
    // get the scroll port rect relative to the scrolled frame
    nsRect rect = GetScrollPortRect() + pt;
    // get the option's rect relative to the scrolled frame
    nsRect fRect(childFrame->GetOffsetTo(GetScrolledFrame()),
                 childFrame->GetSize());

    // See if the selected frame (fRect) is inside the scrollport
    // area (rect). Check only the vertical dimension. Don't
    // scroll just because there's horizontal overflow.
    if (!(rect.y <= fRect.y && fRect.YMost() <= rect.YMost())) {
      // figure out which direction we are going
      if (fRect.YMost() > rect.YMost()) {
        pt.y = fRect.y - (rect.height - fRect.height);
      } else {
        pt.y = fRect.y;
      }
      ScrollTo(nsPoint(fRect.x, pt.y), nsIScrollableFrame::INSTANT);
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
  bool doingReverse = false;
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
    bool isDisabled = true;
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
        doingReverse     = true;
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
        doingReverse     = true;
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
  if (sIncrementalString == nsnull)
    sIncrementalString = new nsString();

  return *sIncrementalString;
}

void
nsListControlFrame::Shutdown()
{
  delete sIncrementalString;
  sIncrementalString = nsnull;
}

void
nsListControlFrame::DropDownToggleKey(nsIDOMEvent* aKeyEvent)
{
  // Cocoa widgets do native popups, so don't try to show
  // dropdowns there.
  if (IsInDropDownMode() && !nsComboboxControlFrame::ToolkitHasNativePopup()) {
    aKeyEvent->PreventDefault();
    if (!mComboboxFrame->IsDroppedDown()) {
      mComboboxFrame->ShowDropDown(true);
    } else {
      nsWeakFrame weakFrame(this);
      // mEndSelectionIndex is the last item that got selected.
      ComboboxFinish(mEndSelectionIndex);
      if (weakFrame.IsAlive()) {
        FireOnChange();
      }
    }
  }
}

nsresult
nsListControlFrame::KeyPress(nsIDOMEvent* aKeyEvent)
{
  NS_ASSERTION(aKeyEvent, "keyEvent is null.");

  nsEventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED))
    return NS_OK;

  // Start by making sure we can query for a key event
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_FAILURE);

  PRUint32 keycode = 0;
  PRUint32 charcode = 0;
  keyEvent->GetKeyCode(&keycode);
  keyEvent->GetCharCode(&charcode);

  bool isAlt = false;

  keyEvent->GetAltKey(&isAlt);
  if (isAlt) {
    if (keycode == nsIDOMKeyEvent::DOM_VK_UP || keycode == nsIDOMKeyEvent::DOM_VK_DOWN) {
      DropDownToggleKey(aKeyEvent);
    }
    return NS_OK;
  }

  // Get control / shift modifiers
  bool isControl = false;
  bool isShift   = false;
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
  bool didIncrementalSearch = false;
  
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
    mControlSelectMode = false;
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
          nsWeakFrame weakFrame(this);
          ComboboxFinish(mEndSelectionIndex);
          if (!weakFrame.IsAlive())
            return NS_OK;
        }
        FireOnChange();
        return NS_OK;
      } else {
        newIndex = mEndSelectionIndex;
      }
      } break;

    case nsIDOMKeyEvent::DOM_VK_ESCAPE: {
      nsWeakFrame weakFrame(this);
      AboutToRollup();
      if (!weakFrame.IsAlive()) {
        aKeyEvent->PreventDefault(); // since we won't reach the one below
        return NS_OK;
      }
    } break;

    case nsIDOMKeyEvent::DOM_VK_PAGE_UP: {
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                (PRInt32)numOptions,
                                -NS_MAX(1, mNumDisplayRows-1), -1);
      } break;

    case nsIDOMKeyEvent::DOM_VK_PAGE_DOWN: {
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                (PRInt32)numOptions,
                                NS_MAX(1, mNumDisplayRows-1), 1);
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

      didIncrementalSearch = true;
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
      PRUnichar uniChar = ToLowerCase(static_cast<PRUnichar>(charcode));
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
              bool wasChanged = PerformSelection(index, isShift, isControl);
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
    // If you hold control, but not shift, no key will actually do anything
    // except space.
    bool wasChanged = false;
    if (isControl && !isShift && charcode != ' ') {
      mStartSelectionIndex = newIndex;
      mEndSelectionIndex = newIndex;
      InvalidateFocus();
      ScrollToIndex(newIndex);

#ifdef ACCESSIBILITY
      FireMenuItemActiveEvent();
#endif
    } else if (mControlSelectMode && charcode == ' ') {
      wasChanged = SingleSelection(newIndex, true);
    } else {
      wasChanged = PerformSelection(newIndex, isShift, isControl);
    }
    if (wasChanged) {
       // dispatch event, update combobox, etc.
      if (!UpdateSelection()) {
        return NS_OK;
      }
    }
  }

  return NS_OK;
}


/******************************************************************************
 * nsListEventListener
 *****************************************************************************/

NS_IMPL_ISUPPORTS1(nsListEventListener, nsIDOMEventListener)

NS_IMETHODIMP
nsListEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!mFrame)
    return NS_OK;

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("keypress"))
    return mFrame->nsListControlFrame::KeyPress(aEvent);
  if (eventType.EqualsLiteral("mousedown"))
    return mFrame->nsListControlFrame::MouseDown(aEvent);
  if (eventType.EqualsLiteral("mouseup"))
    return mFrame->nsListControlFrame::MouseUp(aEvent);
  if (eventType.EqualsLiteral("mousemove"))
    return mFrame->nsListControlFrame::MouseMove(aEvent);

  NS_ABORT();
  return NS_OK;
}

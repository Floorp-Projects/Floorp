/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsUnicharUtils.h"
#include "nsListControlFrame.h"
#include "nsFormControlFrame.h" // for COMPARE macro
#include "nsGkAtoms.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsComboboxControlFrame.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIPresShell.h"
#include "nsIDOMMouseEvent.h"
#include "nsIXULRuntime.h"
#include "nsFontMetrics.h"
#include "nsIScrollableFrame.h"
#include "nsCSSRendering.h"
#include "nsIDOMEventListener.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/HTMLOptionsCollection.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include <algorithm>

using namespace mozilla;

// Constants
const uint32_t kMaxDropDownRows         = 20; // This matches the setting for 4.x browsers
const int32_t kNothingSelected          = -1;

// Static members
nsListControlFrame * nsListControlFrame::mFocused = nullptr;
nsString * nsListControlFrame::sIncrementalString = nullptr;

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

class nsListEventListener MOZ_FINAL : public nsIDOMEventListener
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
nsContainerFrame*
NS_NewListControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  nsListControlFrame* it =
    new (aPresShell) nsListControlFrame(aPresShell, aPresShell->GetDocument(), aContext);

  it->AddStateBits(NS_FRAME_INDEPENDENT_SELECTION);

  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsListControlFrame)

//---------------------------------------------------------
nsListControlFrame::nsListControlFrame(
  nsIPresShell* aShell, nsIDocument* aDocument, nsStyleContext* aContext)
  : nsHTMLScrollFrame(aShell, aContext, false),
    mMightNeedSecondPass(false),
    mHasPendingInterruptAtStartOfReflow(false),
    mDropdownCanGrow(false),
    mLastDropdownComputedHeight(NS_UNCONSTRAINEDSIZE)
{
  mComboboxFrame      = nullptr;
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
  mComboboxFrame = nullptr;
}

// for Bug 47302 (remove this comment later)
void
nsListControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // get the receiver interface from the browser button's content node
  ENSURE_TRUE(mContent);

  // Clear the frame pointer on our event listener, just in case the
  // event listener can outlive the frame.

  mEventListener->SetFrame(nullptr);

  mContent->RemoveSystemEventListener(NS_LITERAL_STRING("keydown"),
                                      mEventListener, false);
  mContent->RemoveSystemEventListener(NS_LITERAL_STRING("keypress"),
                                      mEventListener, false);
  mContent->RemoveSystemEventListener(NS_LITERAL_STRING("mousedown"),
                                      mEventListener, false);
  mContent->RemoveSystemEventListener(NS_LITERAL_STRING("mouseup"),
                                      mEventListener, false);
  mContent->RemoveSystemEventListener(NS_LITERAL_STRING("mousemove"),
                                      mEventListener, false);

  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsHTMLScrollFrame::DestroyFrom(aDestructRoot);
}

void
nsListControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  // We allow visibility:hidden <select>s to contain visible options.
  
  // Don't allow painting of list controls when painting is suppressed.
  // XXX why do we need this here? we should never reach this. Maybe
  // because these can have widgets? Hmm
  if (aBuilder->IsBackgroundOnly())
    return;

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

  nsHTMLScrollFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
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

  nsIFrame* childframe = nullptr;
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
    float inflation = nsLayoutUtils::FontSizeInflationFor(this);
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
nsListControlFrame::InvalidateFocus()
{
  if (mFocused != this)
    return;

  nsIFrame* containerFrame = GetOptionsContainer();
  if (containerFrame) {
    containerFrame->InvalidateFrame();
  }
}

NS_QUERYFRAME_HEAD(nsListControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsIListControlFrame)
  NS_QUERYFRAME_ENTRY(nsISelectControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsHTMLScrollFrame)

#ifdef ACCESSIBILITY
a11y::AccType
nsListControlFrame::AccessibleType()
{
  return a11y::eHTMLSelectListType;
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
  int32_t heightOfARow = GetMaxOptionHeight(GetOptionsContainer());

  // Check to see if we have zero items (and optimize by checking
  // heightOfARow first)
  if (heightOfARow == 0 && GetNumberOfOptions() == 0) {
    float inflation = nsLayoutUtils::FontSizeInflationFor(this);
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

void
nsListControlFrame::Reflow(nsPresContext*           aPresContext, 
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState, 
                           nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowState.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
                  "Must have a computed width");

  SchedulePaint();

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
    ReflowAsDropdown(aPresContext, aDesiredSize, aReflowState, aStatus);
    return;
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
  int32_t length = GetNumberOfRows();

  nscoord oldHeightOfARow = HeightOfARow();

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW) && autoHeight) {
    // When not doing an initial reflow, and when the height is auto, start off
    // with our computed height set to what we'd expect our height to be.
    nscoord computedHeight = CalcIntrinsicHeight(oldHeightOfARow, length);
    computedHeight = state.ApplyMinMaxHeight(computedHeight);
    state.SetComputedHeight(computedHeight);
  }

  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);

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
        mNumDisplayRows = std::max(1, state.ComputedHeight() / rowHeight);
      }
    }

    return;
  }

  mMightNeedSecondPass = false;

  // Now see whether we need a second pass.  If we do, our nsSelectsAreaFrame
  // will have suppressed the scrollbar update.
  if (!IsScrollbarUpdateSuppressed()) {
    // All done.  No need to do more reflow.
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if the height of a row has not "
                 "changed!");
    return;
  }

  SetSuppressScrollbarUpdate(false);

  // Gotta reflow again.
  // XXXbz We're just changing the height here; do we need to dirty ourselves
  // or anything like that?  We might need to, per the letter of the reflow
  // protocol, but things seem to work fine without it...  Is that just an
  // implementation detail of nsHTMLScrollFrame that we're depending on?
  nsHTMLScrollFrame::DidReflow(aPresContext, &state,
                               nsDidReflowStatus::FINISHED);

  // Now compute the height we want to have
  nscoord computedHeight = CalcIntrinsicHeight(HeightOfARow(), length); 
  computedHeight = state.ApplyMinMaxHeight(computedHeight);
  state.SetComputedHeight(computedHeight);

  nsHTMLScrollFrame::WillReflow(aPresContext);

  // XXXbz to make the ascent really correct, we should add our
  // mComputedPadding.top to it (and subtract it from descent).  Need that
  // because nsGfxScrollFrame just adds in the border....
  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);
}

void
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
  nscoord oldVisibleHeight = (GetStateBits() & NS_FRAME_FIRST_REFLOW) ?
    NS_UNCONSTRAINEDSIZE : GetScrolledFrame()->GetSize().height;
#endif

  nsHTMLReflowState state(aReflowState);

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // When not doing an initial reflow, and when the height is auto, start off
    // with our computed height set to what we'd expect our height to be.
    // Note: At this point, mLastDropdownComputedHeight can be
    // NS_UNCONSTRAINEDSIZE in cases when last time we didn't have to constrain
    // the height.  That's fine; just do the same thing as last time.
    state.SetComputedHeight(mLastDropdownComputedHeight);
  }

  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);

  if (!mMightNeedSecondPass) {
    NS_ASSERTION(oldVisibleHeight == GetScrolledFrame()->GetSize().height,
                 "How did our kid's height change if nothing was dirty?");
    NS_ASSERTION(HeightOfARow() == oldHeightOfARow,
                 "How did our height of a row change if nothing was dirty?");
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if we don't need a second pass!");
    NS_ASSERTION(!(GetStateBits() & NS_FRAME_FIRST_REFLOW),
                 "How can we avoid a second pass during first reflow?");
    return;
  }

  mMightNeedSecondPass = false;

  // Now see whether we need a second pass.  If we do, our nsSelectsAreaFrame
  // will have suppressed the scrollbar update.
  if (!IsScrollbarUpdateSuppressed()) {
    // All done.  No need to do more reflow.
    NS_ASSERTION(!(GetStateBits() & NS_FRAME_FIRST_REFLOW),
                 "How can we avoid a second pass during first reflow?");
    return;
  }

  SetSuppressScrollbarUpdate(false);

  nscoord visibleHeight = GetScrolledFrame()->GetSize().height;
  nscoord heightOfARow = HeightOfARow();

  // Gotta reflow again.
  // XXXbz We're just changing the height here; do we need to dirty ourselves
  // or anything like that?  We might need to, per the letter of the reflow
  // protocol, but things seem to work fine without it...  Is that just an
  // implementation detail of nsHTMLScrollFrame that we're depending on?
  nsHTMLScrollFrame::DidReflow(aPresContext, &state,
                               nsDidReflowStatus::FINISHED);

  // Now compute the height we want to have.
  // Note: no need to apply min/max constraints, since we have no such
  // rules applied to the combobox dropdown.

  mDropdownCanGrow = false;
  if (visibleHeight <= 0 || heightOfARow <= 0) {
    // Looks like we have no options.  Just size us to a single row height.
    state.SetComputedHeight(heightOfARow);
    mNumDisplayRows = 1;
  } else {
    nsComboboxControlFrame* combobox = static_cast<nsComboboxControlFrame*>(mComboboxFrame);
    nsPoint translation;
    nscoord above, below;
    combobox->GetAvailableDropdownSpace(&above, &below, &translation);
    if (above <= 0 && below <= 0) {
      state.SetComputedHeight(heightOfARow);
      mNumDisplayRows = 1;
      mDropdownCanGrow = GetNumberOfRows() > 1;
    } else {
      nscoord bp = aReflowState.ComputedPhysicalBorderPadding().TopBottom();
      nscoord availableHeight = std::max(above, below) - bp;
      nscoord newHeight;
      uint32_t rows;
      if (visibleHeight <= availableHeight) {
        // The dropdown fits in the available height.
        rows = GetNumberOfRows();
        mNumDisplayRows = clamped<uint32_t>(rows, 1, kMaxDropDownRows);
        if (mNumDisplayRows == rows) {
          newHeight = visibleHeight;  // use the exact height
        } else {
          newHeight = mNumDisplayRows * heightOfARow; // approximate
        }
      } else {
        rows = availableHeight / heightOfARow;
        mNumDisplayRows = clamped<uint32_t>(rows, 1, kMaxDropDownRows);
        newHeight = mNumDisplayRows * heightOfARow; // approximate
      }
      state.SetComputedHeight(newHeight);
      mDropdownCanGrow = visibleHeight - newHeight >= heightOfARow &&
                         mNumDisplayRows != kMaxDropDownRows;
    }
  }

  mLastDropdownComputedHeight = state.ComputedHeight();

  nsHTMLScrollFrame::WillReflow(aPresContext);
  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);
}

ScrollbarStyles
nsListControlFrame::GetScrollbarStyles() const
{
  // We can't express this in the style system yet; when we can, this can go away
  // and GetScrollbarStyles can be devirtualized
  int32_t verticalStyle = IsInDropDownMode() ? NS_STYLE_OVERFLOW_AUTO
    : NS_STYLE_OVERFLOW_SCROLL;
  return ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN, verticalStyle);
}

bool
nsListControlFrame::ShouldPropagateComputedHeightToScrolledContent() const
{
  return !IsInDropDownMode();
}

//---------------------------------------------------------
nsContainerFrame*
nsListControlFrame::GetContentInsertionFrame() {
  return GetOptionsContainer()->GetContentInsertionFrame();
}

//---------------------------------------------------------
bool
nsListControlFrame::ExtendedSelection(int32_t aStartIndex,
                                      int32_t aEndIndex,
                                      bool aClearAll)
{
  return SetOptionsSelectedFromFrame(aStartIndex, aEndIndex,
                                     true, aClearAll);
}

//---------------------------------------------------------
bool
nsListControlFrame::SingleSelection(int32_t aClickedIndex, bool aDoToggle)
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
  nsWeakFrame weakFrame(this);
  ScrollToIndex(aClickedIndex);
  if (!weakFrame.IsAlive()) {
    return wasChanged;
  }

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
nsListControlFrame::InitSelectionRange(int32_t aClickedIndex)
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
  int32_t selectedIndex = GetSelectedIndex();
  if (selectedIndex >= 0) {
    // Get the end of the contiguous selection
    nsRefPtr<dom::HTMLOptionsCollection> options = GetOptions();
    NS_ASSERTION(options, "Collection of options is null!");
    uint32_t numOptions = options->Length();
    // Push i to one past the last selected index in the group.
    uint32_t i;
    for (i = selectedIndex + 1; i < numOptions; i++) {
      if (!options->ItemAsOption(i)->Selected()) {
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

static uint32_t
CountOptionsAndOptgroups(nsIFrame* aFrame)
{
  uint32_t count = 0;
  nsFrameList::Enumerator e(aFrame->PrincipalChildList());
  for (; !e.AtEnd(); e.Next()) {
    nsIFrame* child = e.get();
    nsIContent* content = child->GetContent();
    if (content) {
      if (content->IsHTML(nsGkAtoms::option)) {
        ++count;
      } else {
        nsCOMPtr<nsIDOMHTMLOptGroupElement> optgroup = do_QueryInterface(content);
        if (optgroup) {
          nsAutoString label;
          optgroup->GetLabel(label);
          if (label.Length() > 0) {
            ++count;
          }
          count += CountOptionsAndOptgroups(child);
        }
      }
    }
  }
  return count;
}

uint32_t
nsListControlFrame::GetNumberOfRows()
{
  return ::CountOptionsAndOptgroups(GetContentInsertionFrame());
}

//---------------------------------------------------------
bool
nsListControlFrame::PerformSelection(int32_t aClickedIndex,
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
      int32_t startIndex;
      int32_t endIndex;
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
      nsWeakFrame weakFrame(this);
      ScrollToIndex(aClickedIndex);
      if (!weakFrame.IsAlive()) {
        return wasChanged;
      }

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
      wasChanged = SingleSelection(aClickedIndex, true); // might destroy us
    } else {
      wasChanged = SingleSelection(aClickedIndex, false); // might destroy us
    }
  } else {
    wasChanged = SingleSelection(aClickedIndex, false); // might destroy us
  }

  return wasChanged;
}

//---------------------------------------------------------
bool
nsListControlFrame::HandleListSelection(nsIDOMEvent* aEvent,
                                        int32_t aClickedIndex)
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
  return PerformSelection(aClickedIndex, isShift, isControl); // might destroy us
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
      nsIPresShell::SetCapturingContent(nullptr, 0);
    }
  }
}

//---------------------------------------------------------
nsresult 
nsListControlFrame::HandleEvent(nsPresContext* aPresContext,
                                WidgetGUIEvent* aEvent,
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
  const nsStyleUserInterface* uiStyle = StyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED))
    return NS_OK;

  return nsHTMLScrollFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


//---------------------------------------------------------
void
nsListControlFrame::SetInitialChildList(ChildListID    aListID,
                                        nsFrameList&   aChildList)
{
  // First check to see if all the content has been added
  mIsAllContentHere = mContent->IsDoneAddingChildren();
  if (!mIsAllContentHere) {
    mIsAllFramesHere    = false;
    mHasBeenInitialized = false;
  }
  nsHTMLScrollFrame::SetInitialChildList(aListID, aChildList);

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
}

//---------------------------------------------------------
void
nsListControlFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  nsHTMLScrollFrame::Init(aContent, aParent, aPrevInFlow);

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  // we need to hook up our listeners before the editor is initialized
  mEventListener = new nsListEventListener(this);

  mContent->AddSystemEventListener(NS_LITERAL_STRING("keydown"),
                                   mEventListener, false, false);
  mContent->AddSystemEventListener(NS_LITERAL_STRING("keypress"),
                                   mEventListener, false, false);
  mContent->AddSystemEventListener(NS_LITERAL_STRING("mousedown"),
                                   mEventListener, false, false);
  mContent->AddSystemEventListener(NS_LITERAL_STRING("mouseup"),
                                   mEventListener, false, false);
  mContent->AddSystemEventListener(NS_LITERAL_STRING("mousemove"),
                                   mEventListener, false, false);

  mStartSelectionIndex = kNothingSelected;
  mEndSelectionIndex = kNothingSelected;

  mLastDropdownBackstopColor = PresContext()->DefaultBackgroundColor();

  if (IsInDropDownMode()) {
    AddStateBits(NS_FRAME_IN_POPUP);
  }
}

dom::HTMLOptionsCollection*
nsListControlFrame::GetOptions() const
{
  dom::HTMLSelectElement* select =
    dom::HTMLSelectElement::FromContentOrNull(mContent);
  NS_ENSURE_TRUE(select, nullptr);

  return select->Options();
}

dom::HTMLOptionElement*
nsListControlFrame::GetOption(uint32_t aIndex) const
{
  dom::HTMLSelectElement* select =
    dom::HTMLSelectElement::FromContentOrNull(mContent);
  NS_ENSURE_TRUE(select, nullptr);

  return select->Item(aIndex);
}

NS_IMETHODIMP
nsListControlFrame::OnOptionSelected(int32_t aIndex, bool aSelected)
{
  if (aSelected) {
    ScrollToIndex(aIndex);
  }
  return NS_OK;
}

void
nsListControlFrame::OnContentReset()
{
  ResetList(true);
}

void 
nsListControlFrame::ResetList(bool aAllowScrolling)
{
  // if all the frames aren't here 
  // don't bother reseting
  if (!mIsAllFramesHere) {
    return;
  }

  if (aAllowScrolling) {
    mPostChildrenLoadedReset = true;

    // Scroll to the selected index
    int32_t indexToSelect = kNothingSelected;

    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(mContent));
    NS_ASSERTION(selectElement, "No select element!");
    if (selectElement) {
      selectElement->GetSelectedIndex(&indexToSelect);
      nsWeakFrame weakFrame(this);
      ScrollToIndex(indexToSelect);
      if (!weakFrame.IsAlive()) {
        return;
      }
    }
  }

  mStartSelectionIndex = kNothingSelected;
  mEndSelectionIndex = kNothingSelected;
  InvalidateFocus();
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
    mFocused = nullptr;
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
  if (nullptr != aComboboxFrame) {
    mComboboxFrame = do_QueryFrame(aComboboxFrame);
  }
}

void
nsListControlFrame::GetOptionText(uint32_t aIndex, nsAString& aStr)
{
  aStr.Truncate();
  if (dom::HTMLOptionElement* optionElement = GetOption(aIndex)) {
    optionElement->GetText(aStr);
  }
}

int32_t
nsListControlFrame::GetSelectedIndex()
{
  dom::HTMLSelectElement* select =
    dom::HTMLSelectElement::FromContentOrNull(mContent);
  return select->SelectedIndex();
}

dom::HTMLOptionElement*
nsListControlFrame::GetCurrentOption()
{
  // The mEndSelectionIndex is what is currently being selected. Use
  // the selected index if this is kNothingSelected.
  int32_t focusedIndex = (mEndSelectionIndex == kNothingSelected) ?
    GetSelectedIndex() : mEndSelectionIndex;

  if (focusedIndex != kNothingSelected) {
    return GetOption(SafeCast<uint32_t>(focusedIndex));
  }

  // There is no selected item. Return the first non-disabled item.
  nsRefPtr<dom::HTMLSelectElement> selectElement =
    dom::HTMLSelectElement::FromContent(mContent);

  for (uint32_t i = 0, length = selectElement->Length(); i < length; ++i) {
    dom::HTMLOptionElement* node = selectElement->Item(i);
    if (!node) {
      return nullptr;
    }

    if (!selectElement->IsOptionDisabled(node)) {
      return node;
    }
  }

  return nullptr;
}

bool 
nsListControlFrame::IsInDropDownMode() const
{
  return (mComboboxFrame != nullptr);
}

uint32_t
nsListControlFrame::GetNumberOfOptions()
{
  dom::HTMLOptionsCollection* options = GetOptions();
  if (!options) {
    return 0;
  }

  return options->Length();
}

//----------------------------------------------------------------------
// nsISelectControlFrame
//----------------------------------------------------------------------
bool nsListControlFrame::CheckIfAllFramesHere()
{
  // Get the number of optgroups and options
  //int32_t numContentItems = 0;
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
nsListControlFrame::AddOption(int32_t aIndex)
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
      mIsAllFramesHere = (aIndex == static_cast<int32_t>(GetNumberOfOptions()-1));
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

static int32_t
DecrementAndClamp(int32_t aSelectionIndex, int32_t aLength)
{
  return aLength == 0 ? kNothingSelected : std::max(0, aSelectionIndex - 1);
}

NS_IMETHODIMP
nsListControlFrame::RemoveOption(int32_t aIndex)
{
  NS_PRECONDITION(aIndex >= 0, "negative <option> index");

  // Need to reset if we're a dropdown
  if (IsInDropDownMode()) {
    mNeedToReset = true;
    mPostChildrenLoadedReset = mIsAllContentHere;
  }

  if (mStartSelectionIndex != kNothingSelected) {
    NS_ASSERTION(mEndSelectionIndex != kNothingSelected, "");
    int32_t numOptions = GetNumberOfOptions();
    // NOTE: numOptions is the new number of options whereas aIndex is the
    // unadjusted index of the removed option (hence the <= below).
    NS_ASSERTION(aIndex <= numOptions, "out-of-bounds <option> index");

    int32_t forward = mEndSelectionIndex - mStartSelectionIndex;
    int32_t* low  = forward >= 0 ? &mStartSelectionIndex : &mEndSelectionIndex;
    int32_t* high = forward >= 0 ? &mEndSelectionIndex : &mStartSelectionIndex;
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
nsListControlFrame::SetOptionsSelectedFromFrame(int32_t aStartIndex,
                                                int32_t aEndIndex,
                                                bool aValue,
                                                bool aClearAll)
{
  nsRefPtr<dom::HTMLSelectElement> selectElement =
    dom::HTMLSelectElement::FromContent(mContent);

  uint32_t mask = dom::HTMLSelectElement::NOTIFY;
  if (aValue) {
    mask |= dom::HTMLSelectElement::IS_SELECTED;
  }

  if (aClearAll) {
    mask |= dom::HTMLSelectElement::CLEAR_ALL;
  }

  return selectElement->SetOptionsSelectedByIndex(aStartIndex, aEndIndex, mask);
}

bool
nsListControlFrame::ToggleOptionSelectedFromFrame(int32_t aIndex)
{
  nsRefPtr<dom::HTMLOptionElement> option =
    GetOption(static_cast<uint32_t>(aIndex));
  NS_ENSURE_TRUE(option, false);

  nsRefPtr<dom::HTMLSelectElement> selectElement =
    dom::HTMLSelectElement::FromContent(mContent);

  uint32_t mask = dom::HTMLSelectElement::NOTIFY;
  if (!option->Selected()) {
    mask |= dom::HTMLSelectElement::IS_SELECTED;
  }

  return selectElement->SetOptionsSelectedByIndex(aIndex, aIndex, mask);
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
nsListControlFrame::ComboboxFinish(int32_t aIndex)
{
  gLastKeyTime = 0;

  if (mComboboxFrame) {
    nsWeakFrame weakFrame(this);
    PerformSelection(aIndex, false, false);  // might destroy us
    if (!weakFrame.IsAlive() || !mComboboxFrame) {
      return;
    }

    int32_t displayIndex = mComboboxFrame->GetIndexOfDisplayArea();
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
    int32_t index = mComboboxFrame->UpdateRecentIndex(NS_SKIP_NOTIFY_INDEX);
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
nsListControlFrame::OnSetSelectedIndex(int32_t aOldIndex, int32_t aNewIndex)
{
  if (mComboboxFrame) {
    // UpdateRecentIndex with NS_SKIP_NOTIFY_INDEX, so that we won't fire an onchange
    // event for this setting of selectedIndex.
    mComboboxFrame->UpdateRecentIndex(NS_SKIP_NOTIFY_INDEX);
  }

  nsWeakFrame weakFrame(this);
  ScrollToIndex(aNewIndex);
  if (!weakFrame.IsAlive()) {
    return NS_OK;
  }
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
  nsStyleContext* context = comboboxFrame->StyleContext()->GetParent();
  mLastDropdownBackstopColor = NS_RGBA(0,0,0,0);
  while (NS_GET_A(mLastDropdownBackstopColor) < 255 && context) {
    mLastDropdownBackstopColor =
      NS_ComposeColors(context->StyleBackground()->mBackgroundColor,
                       mLastDropdownBackstopColor);
    context = context->GetParent();
  }
  mLastDropdownBackstopColor =
    NS_ComposeColors(PresContext()->DefaultBackgroundColor(),
                     mLastDropdownBackstopColor);

  if (mIsAllContentHere && mIsAllFramesHere && mHasBeenInitialized) {
    nsWeakFrame weakFrame(this);
    ScrollToIndex(GetSelectedIndex());
    if (!weakFrame.IsAlive()) {
      return;
    }
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

void
nsListControlFrame::DidReflow(nsPresContext*           aPresContext,
                              const nsHTMLReflowState* aReflowState,
                              nsDidReflowStatus        aStatus)
{
  bool wasInterrupted = !mHasPendingInterruptAtStartOfReflow &&
                          aPresContext->HasPendingInterrupt();

  nsHTMLScrollFrame::DidReflow(aPresContext, aReflowState, aStatus);

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
    ResetList(!DidHistoryRestore() || mPostChildrenLoadedReset);
  }

  mHasPendingInterruptAtStartOfReflow = false;
}

nsIAtom*
nsListControlFrame::GetType() const
{
  return nsGkAtoms::listControlFrame; 
}

#ifdef DEBUG_FRAME_DUMP
nsresult
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
nsListControlFrame::IsOptionDisabled(int32_t anIndex, bool &aIsDisabled)
{
  nsRefPtr<dom::HTMLSelectElement> sel =
    dom::HTMLSelectElement::FromContent(mContent);
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
    int16_t whichButton;
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
                                        int32_t aNumberOfOptions)
{
  NS_PRECONDITION(!IsInDropDownMode(),
                  "Shouldn't be in dropdown mode when we call this");

  dom::HTMLSelectElement* select =
    dom::HTMLSelectElement::FromContentOrNull(mContent);
  if (select) {
    mNumDisplayRows = select->Size();
  } else {
    mNumDisplayRows = 1;
  }

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
  NS_ASSERTION(aMouseEvent != nullptr, "aMouseEvent is null.");

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_FAILURE);

  UpdateInListState(aMouseEvent);

  mButtonDown = false;

  EventStates eventStates = mContent->AsElement()->State();
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

  const nsStyleVisibility* vis = StyleVisibility();
      
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
    WidgetMouseEvent* mouseEvent =
      aMouseEvent->GetInternalNSEvent()->AsMouseEvent();

    int32_t selectedIndex;
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
                                         int32_t&     aCurIndex)
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

  nsRefPtr<dom::HTMLOptionElement> option;
  for (nsCOMPtr<nsIContent> content =
         PresContext()->EventStateManager()->GetEventTargetContent(nullptr);
       content && !option;
       content = content->GetParent()) {
    option = dom::HTMLOptionElement::FromContent(content);
  }

  if (option) {
    aCurIndex = option->Index();
    MOZ_ASSERT(aCurIndex >= 0);
    return NS_OK;
  }

  int32_t numOptions = GetNumberOfOptions();
  if (numOptions < 1)
    return NS_ERROR_FAILURE;

  nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aMouseEvent, this);

  // If the event coordinate is above the first option frame, then target the
  // first option frame
  nsRefPtr<dom::HTMLOptionElement> firstOption = GetOption(0);
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

  nsRefPtr<dom::HTMLOptionElement> lastOption = GetOption(numOptions - 1);
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
  NS_ASSERTION(aMouseEvent != nullptr, "aMouseEvent is null.");

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_FAILURE);

  UpdateInListState(aMouseEvent);

  EventStates eventStates = mContent->AsElement()->State();
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

  int32_t selectedIndex;
  if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
    // Handle Like List
    mButtonDown = true;
    CaptureMouseEvents(true);
    nsWeakFrame weakFrame(this);
    bool change =
      HandleListSelection(aMouseEvent, selectedIndex); // might destroy us
    if (!weakFrame.IsAlive()) {
      return NS_OK;
    }
    mChangesSinceDragStart = change;
  } else {
    // NOTE: the combo box is responsible for dropping it down
    if (mComboboxFrame) {
      if (XRE_GetProcessType() == GeckoProcessType_Content && BrowserTabsRemote()) {
        nsContentUtils::DispatchChromeEvent(mContent->OwnerDoc(), mContent,
                                            NS_LITERAL_STRING("mozshowdropdown"), true,
                                            false);
        return NS_OK;
      }

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
      int32_t selectedIndex;
      if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
        PerformSelection(selectedIndex, false, false); // might destroy us
      }
    }
  } else {// XXX - temporary until we get drag events
    if (mButtonDown) {
      return DragMove(aMouseEvent); // might destroy us
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
    int32_t selectedIndex;
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
      nsWeakFrame weakFrame(this);
      // Turn SHIFT on when you are dragging, unless control is on.
      bool wasChanged = PerformSelection(selectedIndex,
                                           !isControl, isControl);
      if (!weakFrame.IsAlive()) {
        return NS_OK;
      }
      mChangesSinceDragStart = mChangesSinceDragStart || wasChanged;
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// Scroll helpers.
//----------------------------------------------------------------------
void
nsListControlFrame::ScrollToIndex(int32_t aIndex)
{
  if (aIndex < 0) {
    // XXX shouldn't we just do nothing if we're asked to scroll to
    // kNothingSelected?
    ScrollTo(nsPoint(0, 0), nsIScrollableFrame::INSTANT);
  } else {
    nsRefPtr<dom::HTMLOptionElement> option =
      GetOption(SafeCast<uint32_t>(aIndex));
    if (option) {
      ScrollToFrame(*option);
    }
  }
}

void
nsListControlFrame::ScrollToFrame(dom::HTMLOptionElement& aOptElement)
{
  // otherwise we find the content's frame and scroll to it
  nsIFrame* childFrame = aOptElement.GetPrimaryFrame();
  if (childFrame) {
    PresContext()->PresShell()->
      ScrollFrameRectIntoView(childFrame,
                              nsRect(nsPoint(0, 0), childFrame->GetSize()),
                              nsIPresShell::ScrollAxis(), nsIPresShell::ScrollAxis(),
                              nsIPresShell::SCROLL_OVERFLOW_HIDDEN |
                              nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY);
  }
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
nsListControlFrame::AdjustIndexForDisabledOpt(int32_t aStartIndex,
                                              int32_t &aNewIndex,
                                              int32_t aNumOptions,
                                              int32_t aDoAdjustInc,
                                              int32_t aDoAdjustIncNext)
{
  // Cannot select anything if there is nothing to select
  if (aNumOptions == 0) {
    aNewIndex = kNothingSelected;
    return;
  }

  // means we reached the end of the list and now we are searching backwards
  bool doingReverse = false;
  // lowest index in the search range
  int32_t bottom      = 0;
  // highest index in the search range
  int32_t top         = aNumOptions;

  // Start off keyboard options at selectedIndex if nothing else is defaulted to
  //
  // XXX Perhaps this should happen for mouse too, to start off shift click
  // automatically in multiple ... to do this, we'd need to override
  // OnOptionSelected and set mStartSelectedIndex if nothing is selected.  Not
  // sure of the effects, though, so I'm not doing it just yet.
  int32_t startIndex = aStartIndex;
  if (startIndex < bottom) {
    startIndex = GetSelectedIndex();
  }
  int32_t newIndex    = startIndex + aDoAdjustInc;

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
  if (sIncrementalString == nullptr)
    sIncrementalString = new nsString();

  return *sIncrementalString;
}

void
nsListControlFrame::Shutdown()
{
  delete sIncrementalString;
  sIncrementalString = nullptr;
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
nsListControlFrame::KeyDown(nsIDOMEvent* aKeyEvent)
{
  MOZ_ASSERT(aKeyEvent, "aKeyEvent is null.");

  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    return NS_OK;
  }

  AutoIncrementalSearchResetter incrementalSearchResetter;

  // Don't check defaultPrevented value because other browsers don't prevent
  // the key navigation of list control even if preventDefault() is called.

  const WidgetKeyboardEvent* keyEvent =
    aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
  MOZ_ASSERT(keyEvent,
    "DOM event must have WidgetKeyboardEvent for its internal event");

  if (keyEvent->IsAlt()) {
    if (keyEvent->keyCode == NS_VK_UP || keyEvent->keyCode == NS_VK_DOWN) {
      DropDownToggleKey(aKeyEvent);
    }
    return NS_OK;
  }

  // now make sure there are options or we are wasting our time
  nsRefPtr<dom::HTMLOptionsCollection> options = GetOptions();
  NS_ENSURE_TRUE(options, NS_ERROR_FAILURE);

  uint32_t numOptions = options->Length();

  // this is the new index to set
  int32_t newIndex = kNothingSelected;

  bool isControlOrMeta = (keyEvent->IsControl() || keyEvent->IsMeta());
  // Don't try to handle multiple-select pgUp/pgDown in single-select lists.
  if (isControlOrMeta && !GetMultiple() &&
      (keyEvent->keyCode == NS_VK_PAGE_UP ||
       keyEvent->keyCode == NS_VK_PAGE_DOWN)) {
    return NS_OK;
  }
  if (isControlOrMeta && (keyEvent->keyCode == NS_VK_UP ||
                          keyEvent->keyCode == NS_VK_LEFT ||
                          keyEvent->keyCode == NS_VK_DOWN ||
                          keyEvent->keyCode == NS_VK_RIGHT ||
                          keyEvent->keyCode == NS_VK_HOME ||
                          keyEvent->keyCode == NS_VK_END)) {
    // Don't go into multiple-select mode unless this list can handle it.
    isControlOrMeta = mControlSelectMode = GetMultiple();
  } else if (keyEvent->keyCode != NS_VK_SPACE) {
    mControlSelectMode = false;
  }

  switch (keyEvent->keyCode) {
    case NS_VK_UP:
    case NS_VK_LEFT:
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                static_cast<int32_t>(numOptions),
                                -1, -1);
      break;
    case NS_VK_DOWN:
    case NS_VK_RIGHT:
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                static_cast<int32_t>(numOptions),
                                1, 1);
      break;
    case NS_VK_RETURN:
      if (IsInDropDownMode()) {
        if (mComboboxFrame->IsDroppedDown()) {
          // If the select element is a dropdown style, Enter key should be
          // consumed while the dropdown is open for security.
          aKeyEvent->PreventDefault();

          nsWeakFrame weakFrame(this);
          ComboboxFinish(mEndSelectionIndex);
          if (!weakFrame.IsAlive()) {
            return NS_OK;
          }
        }
        // XXX This is strange. On other browsers, "change" event is fired
        //     immediately after the selected item is changed rather than
        //     Enter key is pressed.
        FireOnChange();
        return NS_OK;
      }

      // If this is single select listbox, Enter key doesn't cause anything.
      if (!GetMultiple()) {
        return NS_OK;
      }

      newIndex = mEndSelectionIndex;
      break;
    case NS_VK_ESCAPE: {
      // If the select element is a listbox style, Escape key causes nothing.
      if (!IsInDropDownMode()) {
        return NS_OK;
      }

      AboutToRollup();
      // If the select element is a dropdown style, Enter key should be
      // consumed everytime since Escape key may be pressed accidentally after
      // the dropdown is closed by Escepe key.
      aKeyEvent->PreventDefault();
      return NS_OK;
    }
    case NS_VK_PAGE_UP: {
      int32_t itemsPerPage =
        std::max(1, static_cast<int32_t>(mNumDisplayRows - 1));
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                static_cast<int32_t>(numOptions),
                                -itemsPerPage, -1);
      break;
    }
    case NS_VK_PAGE_DOWN: {
      int32_t itemsPerPage =
        std::max(1, static_cast<int32_t>(mNumDisplayRows - 1));
      AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                static_cast<int32_t>(numOptions),
                                itemsPerPage, 1);
      break;
    }
    case NS_VK_HOME:
      AdjustIndexForDisabledOpt(0, newIndex,
                                static_cast<int32_t>(numOptions),
                                0, 1);
      break;
    case NS_VK_END:
      AdjustIndexForDisabledOpt(static_cast<int32_t>(numOptions) - 1, newIndex,
                                static_cast<int32_t>(numOptions),
                                0, -1);
      break;

#if defined(XP_WIN)
    case NS_VK_F4:
      if (!isControlOrMeta) {
        DropDownToggleKey(aKeyEvent);
      }
      return NS_OK;
#endif

    default: // printable key will be handled by keypress event.
      incrementalSearchResetter.Cancel();
      return NS_OK;
  }

  aKeyEvent->PreventDefault();

  // Actually process the new index and let the selection code
  // do the scrolling for us
  PostHandleKeyEvent(newIndex, 0, keyEvent->IsShift(), isControlOrMeta);
  return NS_OK;
}

nsresult
nsListControlFrame::KeyPress(nsIDOMEvent* aKeyEvent)
{
  MOZ_ASSERT(aKeyEvent, "aKeyEvent is null.");

  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    return NS_OK;
  }

  AutoIncrementalSearchResetter incrementalSearchResetter;

  const WidgetKeyboardEvent* keyEvent =
    aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
  MOZ_ASSERT(keyEvent,
    "DOM event must have WidgetKeyboardEvent for its internal event");

  // Select option with this as the first character
  // XXX Not I18N compliant

  // Don't do incremental search if the key event has already consumed.
  if (keyEvent->mFlags.mDefaultPrevented) {
    return NS_OK;
  }

  if (keyEvent->IsAlt()) {
    return NS_OK;
  }

  // With some keyboard layout, space key causes non-ASCII space.
  // So, the check in keydown event handler isn't enough, we need to check it
  // again with keypress event.
  if (keyEvent->charCode != ' ') {
    mControlSelectMode = false;
  }

  bool isControlOrMeta = (keyEvent->IsControl() || keyEvent->IsMeta());
  if (isControlOrMeta && keyEvent->charCode != ' ') {
    return NS_OK;
  }

  // NOTE: If keyCode of keypress event is not 0, charCode is always 0.
  //       Therefore, all non-printable keys are not handled after this block.
  if (!keyEvent->charCode) {
    // Backspace key will delete the last char in the string.  Otherwise,
    // non-printable keypress should reset incremental search.
    if (keyEvent->keyCode == NS_VK_BACK) {
      incrementalSearchResetter.Cancel();
      if (!GetIncrementalString().IsEmpty()) {
        GetIncrementalString().Truncate(GetIncrementalString().Length() - 1);
      }
      aKeyEvent->PreventDefault();
    } else {
      // XXX When a select element has focus, even if the key causes nothing,
      //     it might be better to call preventDefault() here because nobody
      //     should expect one of other elements including chrome handles the
      //     key event.
    }
    return NS_OK;
  }

  incrementalSearchResetter.Cancel();

  // We ate the key if we got this far.
  aKeyEvent->PreventDefault();

  // XXX Why don't we check/modify timestamp first?

  // Incremental Search: if time elapsed is below
  // INCREMENTAL_SEARCH_KEYPRESS_TIME, append this keystroke to the search
  // string we will use to find options and start searching at the current
  // keystroke.  Otherwise, Truncate the string if it's been a long time
  // since our last keypress.
  if (keyEvent->time - gLastKeyTime > INCREMENTAL_SEARCH_KEYPRESS_TIME) {
    // If this is ' ' and we are at the beginning of the string, treat it as
    // "select this option" (bug 191543)
    if (keyEvent->charCode == ' ') {
      // Actually process the new index and let the selection code
      // do the scrolling for us
      PostHandleKeyEvent(mEndSelectionIndex, keyEvent->charCode,
                         keyEvent->IsShift(), isControlOrMeta);

      return NS_OK;
    }

    GetIncrementalString().Truncate();
  }

  gLastKeyTime = keyEvent->time;

  // Append this keystroke to the search string. 
  char16_t uniChar = ToLowerCase(static_cast<char16_t>(keyEvent->charCode));
  GetIncrementalString().Append(uniChar);

  // See bug 188199, if all letters in incremental string are same, just try to
  // match the first one
  nsAutoString incrementalString(GetIncrementalString());
  uint32_t charIndex = 1, stringLength = incrementalString.Length();
  while (charIndex < stringLength &&
         incrementalString[charIndex] == incrementalString[charIndex - 1]) {
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
  int32_t startIndex = GetSelectedIndex();
  if (startIndex == kNothingSelected) {
    startIndex = 0;
  } else if (stringLength == 1) {
    startIndex++;
  }

  // now make sure there are options or we are wasting our time
  nsRefPtr<dom::HTMLOptionsCollection> options = GetOptions();
  NS_ENSURE_TRUE(options, NS_ERROR_FAILURE);

  uint32_t numOptions = options->Length();

  nsWeakFrame weakFrame(this);
  for (uint32_t i = 0; i < numOptions; ++i) {
    uint32_t index = (i + startIndex) % numOptions;
    nsRefPtr<dom::HTMLOptionElement> optionElement =
      options->ItemAsOption(index);
    if (!optionElement || !optionElement->GetPrimaryFrame()) {
      continue;
    }

    nsAutoString text;
    if (NS_FAILED(optionElement->GetText(text)) ||
        !StringBeginsWith(
           nsContentUtils::TrimWhitespace<
             nsContentUtils::IsHTMLWhitespaceOrNBSP>(text, false),
           incrementalString, nsCaseInsensitiveStringComparator())) {
      continue;
    }

    bool wasChanged = PerformSelection(index, keyEvent->IsShift(), isControlOrMeta);
    if (!weakFrame.IsAlive()) {
      return NS_OK;
    }
    if (!wasChanged) {
      break;
    }

    // If UpdateSelection() returns false, that means the frame is no longer
    // alive. We should stop doing anything.
    if (!UpdateSelection()) {
      return NS_OK;
    }
    break;
  }

  return NS_OK;
}

void
nsListControlFrame::PostHandleKeyEvent(int32_t aNewIndex,
                                       uint32_t aCharCode,
                                       bool aIsShift,
                                       bool aIsControlOrMeta)
{
  if (aNewIndex == kNothingSelected) {
    return;
  }

  // If you hold control, but not shift, no key will actually do anything
  // except space.
  nsWeakFrame weakFrame(this);
  bool wasChanged = false;
  if (aIsControlOrMeta && !aIsShift && aCharCode != ' ') {
    mStartSelectionIndex = aNewIndex;
    mEndSelectionIndex = aNewIndex;
    InvalidateFocus();
    ScrollToIndex(aNewIndex);
    if (!weakFrame.IsAlive()) {
      return;
    }

#ifdef ACCESSIBILITY
    FireMenuItemActiveEvent();
#endif
  } else if (mControlSelectMode && aCharCode == ' ') {
    wasChanged = SingleSelection(aNewIndex, true);
  } else {
    wasChanged = PerformSelection(aNewIndex, aIsShift, aIsControlOrMeta);
  }
  if (wasChanged && weakFrame.IsAlive()) {
    // dispatch event, update combobox, etc.
    UpdateSelection();
  }
}


/******************************************************************************
 * nsListEventListener
 *****************************************************************************/

NS_IMPL_ISUPPORTS(nsListEventListener, nsIDOMEventListener)

NS_IMETHODIMP
nsListEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!mFrame)
    return NS_OK;

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("keydown"))
    return mFrame->nsListControlFrame::KeyDown(aEvent);
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

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsUnicharUtils.h"
#include "nsListControlFrame.h"
#include "nsCheckboxRadioFrame.h" // for COMPARE macro
#include "nsGkAtoms.h"
#include "nsComboboxControlFrame.h"
#include "nsIPresShell.h"
#include "nsIXULRuntime.h"
#include "nsFontMetrics.h"
#include "nsIScrollableFrame.h"
#include "nsCSSRendering.h"
#include "nsIDOMEventListener.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLOptGroupElement.h"
#include "mozilla/dom/HTMLOptionsCollection.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/MouseEventBinding.h"
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

class nsListEventListener final : public nsIDOMEventListener
{
public:
  explicit nsListEventListener(nsListControlFrame *aFrame)
    : mFrame(aFrame) { }

  void SetFrame(nsListControlFrame *aFrame) { mFrame = aFrame; }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

private:
  ~nsListEventListener() {}

  nsListControlFrame  *mFrame;
};

//---------------------------------------------------------
nsContainerFrame*
NS_NewListControlFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  nsListControlFrame* it =
    new (aPresShell) nsListControlFrame(aStyle);

  it->AddStateBits(NS_FRAME_INDEPENDENT_SELECTION);

  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsListControlFrame)

//---------------------------------------------------------
nsListControlFrame::nsListControlFrame(ComputedStyle* aStyle)
  : nsHTMLScrollFrame(aStyle, kClassID, false)
  , mView(nullptr)
  , mMightNeedSecondPass(false)
  , mHasPendingInterruptAtStartOfReflow(false)
  , mDropdownCanGrow(false)
  , mForceSelection(false)
  , mLastDropdownComputedBSize(NS_UNCONSTRAINEDSIZE)
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

static bool ShouldFireDropDownEvent() {
  // We don't need to fire the event to SelectContentHelper when content-select
  // is enabled.
  if (nsLayoutUtils::IsContentSelectEnabled()) {
    return false;
  }

  return (XRE_IsContentProcess() &&
          Preferences::GetBool("browser.tabs.remote.desktopbehavior", false)) ||
         Preferences::GetBool("dom.select_popup_in_parent.enabled", false);
}

// for Bug 47302 (remove this comment later)
void
nsListControlFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
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

  if (ShouldFireDropDownEvent()) {
    nsContentUtils::AddScriptRunner(
      new AsyncEventDispatcher(mContent,
                               NS_LITERAL_STRING("mozhidedropdown"), true,
                               true));
  }

  nsCheckboxRadioFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsHTMLScrollFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void
nsListControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
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
    aLists.BorderBackground()->AppendToBottom(
      MakeDisplayItem<nsDisplaySolidColor>(aBuilder,
        this, nsRect(aBuilder->ToReferenceFrame(this), GetSize()),
        mLastDropdownBackstopColor));
  }

  nsHTMLScrollFrame::BuildDisplayList(aBuilder, aLists);
}

/**
 * This is called by the SelectsAreaFrame, which is the same
 * as the frame returned by GetOptionsContainer. It's the frame which is
 * scrolled by us.
 * @param aPt the offset of this frame, relative to the rendering reference
 * frame
 */
void nsListControlFrame::PaintFocus(DrawTarget* aDrawTarget, nsPoint aPt)
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
    if (GetWritingMode().IsVertical()) {
      fRect.width = GetScrollPortRect().width;
      fRect.height = CalcFallbackRowBSize(inflation);
    } else {
      fRect.width = CalcFallbackRowBSize(inflation);
      fRect.height = GetScrollPortRect().height;
    }
    fRect.MoveBy(containerFrame->GetOffsetTo(this));
  }
  fRect += aPt;

  bool lastItemIsSelected = false;
  HTMLOptionElement* domOpt = HTMLOptionElement::FromNodeOrNull(focusedContent);
  if (domOpt) {
    lastItemIsSelected = domOpt->Selected();
  }

  // set up back stop colors and then ask L&F service for the real colors
  nscolor color =
    LookAndFeel::GetColor(lastItemIsSelected ?
                            LookAndFeel::eColorID_WidgetSelectForeground :
                            LookAndFeel::eColorID_WidgetSelectBackground);

  nsCSSRendering::PaintFocus(presContext, aDrawTarget, fRect, color);
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
GetMaxOptionBSize(nsIFrame* aContainer, WritingMode aWM)
{
  nscoord result = 0;
  for (nsIFrame* option : aContainer->PrincipalChildList()) {
    nscoord optionBSize;
    if (HTMLOptGroupElement::FromNode(option->GetContent())) {
      // An optgroup; drill through any scroll frame and recurse.  |frame| might
      // be null here though if |option| is an anonymous leaf frame of some sort.
      auto frame = option->GetContentInsertionFrame();
      optionBSize = frame ? GetMaxOptionBSize(frame, aWM) : 0;
    } else {
      // an option
      optionBSize = option->BSize(aWM);
    }
    if (result < optionBSize)
      result = optionBSize;
  }
  return result;
}

//-----------------------------------------------------------------
// Main Reflow for ListBox/Dropdown
//-----------------------------------------------------------------

nscoord
nsListControlFrame::CalcBSizeOfARow()
{
  // Calculate the block size in our writing mode of a single row in the
  // listbox or dropdown list by using the tallest thing in the subtree,
  // since there may be option groups in addition to option elements,
  // either of which may be visible or invisible, may use different
  // fonts, etc.
  int32_t blockSizeOfARow = GetMaxOptionBSize(GetOptionsContainer(),
                                              GetWritingMode());

  // Check to see if we have zero items (and optimize by checking
  // blockSizeOfARow first)
  if (blockSizeOfARow == 0 && GetNumberOfOptions() == 0) {
    float inflation = nsLayoutUtils::FontSizeInflationFor(this);
    blockSizeOfARow = CalcFallbackRowBSize(inflation);
  }

  return blockSizeOfARow;
}

nscoord
nsListControlFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  // Always add scrollbar inline sizes to the pref-inline-size of the
  // scrolled content. Combobox frames depend on this happening in the
  // dropdown, and standalone listboxes are overflow:scroll so they need
  // it too.
  WritingMode wm = GetWritingMode();
  result = GetScrolledFrame()->GetPrefISize(aRenderingContext);
  LogicalMargin scrollbarSize(wm, GetDesiredScrollbarSizes(PresContext(),
                                                           aRenderingContext));
  result = NSCoordSaturatingAdd(result, scrollbarSize.IStartEnd(wm));
  return result;
}

nscoord
nsListControlFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  // Always add scrollbar inline sizes to the min-inline-size of the
  // scrolled content. Combobox frames depend on this happening in the
  // dropdown, and standalone listboxes are overflow:scroll so they need
  // it too.
  WritingMode wm = GetWritingMode();
  result = GetScrolledFrame()->GetMinISize(aRenderingContext);
  LogicalMargin scrollbarSize(wm, GetDesiredScrollbarSizes(PresContext(),
                                                           aRenderingContext));
  result += scrollbarSize.IStartEnd(wm);

  return result;
}

void
nsListControlFrame::Reflow(nsPresContext*           aPresContext,
                           ReflowOutput&     aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aStatus)
{
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_PRECONDITION(aReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE,
                  "Must have a computed inline size");

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
    nsCheckboxRadioFrame::RegUnRegAccessKey(this, true);
  }

  if (IsInDropDownMode()) {
    ReflowAsDropdown(aPresContext, aDesiredSize, aReflowInput, aStatus);
    return;
  }

  MarkInReflow();
  /*
   * Due to the fact that our intrinsic block size depends on the block
   * sizes of our kids, we end up having to do two-pass reflow, in
   * general -- the first pass to find the intrinsic block size and a
   * second pass to reflow the scrollframe at that block size (which
   * will size the scrollbars correctly, etc).
   *
   * Naturally, we want to avoid doing the second reflow as much as
   * possible.
   * We can skip it in the following cases (in all of which the first
   * reflow is already happening at the right block size):
   *
   * - We're reflowing with a constrained computed block size -- just
   *   use that block size.
   * - We're not dirty and have no dirty kids and shouldn't be reflowing
   *   all kids.  In this case, our cached max block size of a child is
   *   not going to change.
   * - We do our first reflow using our cached max block size of a
   *   child, then compute the new max block size and it's the same as
   *   the old one.
   */

  bool autoBSize = (aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE);

  mMightNeedSecondPass = autoBSize &&
    (NS_SUBTREE_DIRTY(this) || aReflowInput.ShouldReflowAllKids());

  ReflowInput state(aReflowInput);
  int32_t length = GetNumberOfRows();

  nscoord oldBSizeOfARow = BSizeOfARow();

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW) && autoBSize) {
    // When not doing an initial reflow, and when the block size is
    // auto, start off with our computed block size set to what we'd
    // expect our block size to be.
    nscoord computedBSize = CalcIntrinsicBSize(oldBSizeOfARow, length);
    computedBSize = state.ApplyMinMaxBSize(computedBSize);
    state.SetComputedBSize(computedBSize);
  }

  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);

  if (!mMightNeedSecondPass) {
    NS_ASSERTION(!autoBSize || BSizeOfARow() == oldBSizeOfARow,
                 "How did our BSize of a row change if nothing was dirty?");
    NS_ASSERTION(!autoBSize ||
                 !(GetStateBits() & NS_FRAME_FIRST_REFLOW),
                 "How do we not need a second pass during initial reflow at "
                 "auto BSize?");
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if we don't need a second pass!");
    if (!autoBSize) {
      // Update our mNumDisplayRows based on our new row block size now
      // that we know it.  Note that if autoBSize and we landed in this
      // code then we already set mNumDisplayRows in CalcIntrinsicBSize.
      //  Also note that we can't use BSizeOfARow() here because that
      // just uses a cached value that we didn't compute.
      nscoord rowBSize = CalcBSizeOfARow();
      if (rowBSize == 0) {
        // Just pick something
        mNumDisplayRows = 1;
      } else {
        mNumDisplayRows = std::max(1, state.ComputedBSize() / rowBSize);
      }
    }

    return;
  }

  mMightNeedSecondPass = false;

  // Now see whether we need a second pass.  If we do, our
  // nsSelectsAreaFrame will have suppressed the scrollbar update.
  if (!IsScrollbarUpdateSuppressed()) {
    // All done.  No need to do more reflow.
    return;
  }

  SetSuppressScrollbarUpdate(false);

  // Gotta reflow again.
  // XXXbz We're just changing the block size here; do we need to dirty
  // ourselves or anything like that?  We might need to, per the letter
  // of the reflow protocol, but things seem to work fine without it...
  // Is that just an implementation detail of nsHTMLScrollFrame that
  // we're depending on?
  nsHTMLScrollFrame::DidReflow(aPresContext, &state);

  // Now compute the block size we want to have
  nscoord computedBSize = CalcIntrinsicBSize(BSizeOfARow(), length);
  computedBSize = state.ApplyMinMaxBSize(computedBSize);
  state.SetComputedBSize(computedBSize);

  // XXXbz to make the ascent really correct, we should add our
  // mComputedPadding.top to it (and subtract it from descent).  Need that
  // because nsGfxScrollFrame just adds in the border....
  aStatus.Reset();
  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);
}

void
nsListControlFrame::ReflowAsDropdown(nsPresContext*           aPresContext,
                                     ReflowOutput&     aDesiredSize,
                                     const ReflowInput& aReflowInput,
                                     nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE,
                  "We should not have a computed block size here!");

  mMightNeedSecondPass = NS_SUBTREE_DIRTY(this) ||
    aReflowInput.ShouldReflowAllKids();

  WritingMode wm = aReflowInput.GetWritingMode();
#ifdef DEBUG
  nscoord oldBSizeOfARow = BSizeOfARow();
  nscoord oldVisibleBSize = (GetStateBits() & NS_FRAME_FIRST_REFLOW) ?
    NS_UNCONSTRAINEDSIZE : GetScrolledFrame()->BSize(wm);
#endif

  ReflowInput state(aReflowInput);

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // When not doing an initial reflow, and when the block size is
    // auto, start off with our computed block size set to what we'd
    // expect our block size to be.
    // Note: At this point, mLastDropdownComputedBSize can be
    // NS_UNCONSTRAINEDSIZE in cases when last time we didn't have to
    // constrain the block size.  That's fine; just do the same thing as
    // last time.
    state.SetComputedBSize(mLastDropdownComputedBSize);
  }

  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);

  if (!mMightNeedSecondPass) {
    NS_ASSERTION(oldVisibleBSize == GetScrolledFrame()->BSize(wm),
                 "How did our kid's BSize change if nothing was dirty?");
    NS_ASSERTION(BSizeOfARow() == oldBSizeOfARow,
                 "How did our BSize of a row change if nothing was dirty?");
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

  nscoord visibleBSize = GetScrolledFrame()->BSize(wm);
  nscoord blockSizeOfARow = BSizeOfARow();

  // Gotta reflow again.
  // XXXbz We're just changing the block size here; do we need to dirty
  // ourselves or anything like that?  We might need to, per the letter
  // of the reflow protocol, but things seem to work fine without it...
  // Is that just an implementation detail of nsHTMLScrollFrame that
  // we're depending on?
  nsHTMLScrollFrame::DidReflow(aPresContext, &state);

  // Now compute the block size we want to have.
  // Note: no need to apply min/max constraints, since we have no such
  // rules applied to the combobox dropdown.

  mDropdownCanGrow = false;
  if (visibleBSize <= 0 || blockSizeOfARow <= 0 || XRE_IsContentProcess()) {
    // Looks like we have no options.  Just size us to a single row
    // block size.
    state.SetComputedBSize(blockSizeOfARow);
    mNumDisplayRows = 1;
  } else {
    nsComboboxControlFrame* combobox =
      static_cast<nsComboboxControlFrame*>(mComboboxFrame);
    LogicalPoint translation(wm);
    nscoord before, after;
    combobox->GetAvailableDropdownSpace(wm, &before, &after, &translation);
    if (before <= 0 && after <= 0) {
      state.SetComputedBSize(blockSizeOfARow);
      mNumDisplayRows = 1;
      mDropdownCanGrow = GetNumberOfRows() > 1;
    } else {
      nscoord bp = aReflowInput.ComputedLogicalBorderPadding().BStartEnd(wm);
      nscoord availableBSize = std::max(before, after) - bp;
      nscoord newBSize;
      uint32_t rows;
      if (visibleBSize <= availableBSize) {
        // The dropdown fits in the available block size.
        rows = GetNumberOfRows();
        mNumDisplayRows = clamped<uint32_t>(rows, 1, kMaxDropDownRows);
        if (mNumDisplayRows == rows) {
          newBSize = visibleBSize;  // use the exact block size
        } else {
          newBSize = mNumDisplayRows * blockSizeOfARow; // approximate
          // The approximation here might actually be too big (bug 1208978);
          // don't let it exceed the actual block-size of the list.
          newBSize = std::min(newBSize, visibleBSize);
        }
      } else {
        rows = availableBSize / blockSizeOfARow;
        mNumDisplayRows = clamped<uint32_t>(rows, 1, kMaxDropDownRows);
        newBSize = mNumDisplayRows * blockSizeOfARow; // approximate
      }
      state.SetComputedBSize(newBSize);
      mDropdownCanGrow = visibleBSize - newBSize >= blockSizeOfARow &&
                         mNumDisplayRows != kMaxDropDownRows;
    }
  }

  mLastDropdownComputedBSize = state.ComputedBSize();

  aStatus.Reset();
  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);
}

ScrollbarStyles
nsListControlFrame::GetScrollbarStyles() const
{
  // We can't express this in the style system yet; when we can, this can go away
  // and GetScrollbarStyles can be devirtualized
  int32_t style = IsInDropDownMode() ? NS_STYLE_OVERFLOW_AUTO
                                     : NS_STYLE_OVERFLOW_SCROLL;
  if (GetWritingMode().IsVertical()) {
    return ScrollbarStyles(style, NS_STYLE_OVERFLOW_HIDDEN);
  } else {
    return ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN, style);
  }
}

bool
nsListControlFrame::ShouldPropagateComputedBSizeToScrolledContent() const
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
  AutoWeakFrame weakFrame(this);
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
    RefPtr<dom::HTMLOptionsCollection> options = GetOptions();
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
      if (content->IsHTMLElement(nsGkAtoms::option)) {
        ++count;
      } else {
        RefPtr<HTMLOptGroupElement> optgroup =
          HTMLOptGroupElement::FromNode(content);
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

  if (aClickedIndex == kNothingSelected && !mForceSelection) {
    // Ignore kNothingSelected unless the selection is forced
  } else if (GetMultiple()) {
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
      AutoWeakFrame weakFrame(this);
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
nsListControlFrame::HandleListSelection(dom::Event* aEvent,
                                        int32_t aClickedIndex)
{
  MouseEvent* mouseEvent = aEvent->AsMouseEvent();
  bool isControl;
#ifdef XP_MACOSX
  isControl = mouseEvent->MetaKey();
#else
  isControl = mouseEvent->CtrlKey();
#endif
  bool isShift = mouseEvent->ShiftKey();
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

  /*const char * desc[] = {"eMouseMove",
                          "NS_MOUSE_LEFT_BUTTON_UP",
                          "NS_MOUSE_LEFT_BUTTON_DOWN",
                          "<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>",
                          "NS_MOUSE_MIDDLE_BUTTON_UP",
                          "NS_MOUSE_MIDDLE_BUTTON_DOWN",
                          "<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>","<NA>",
                          "NS_MOUSE_RIGHT_BUTTON_UP",
                          "NS_MOUSE_RIGHT_BUTTON_DOWN",
                          "eMouseOver",
                          "eMouseOut",
                          "NS_MOUSE_LEFT_DOUBLECLICK",
                          "NS_MOUSE_MIDDLE_DOUBLECLICK",
                          "NS_MOUSE_RIGHT_DOUBLECLICK",
                          "NS_MOUSE_LEFT_CLICK",
                          "NS_MOUSE_MIDDLE_CLICK",
                          "NS_MOUSE_RIGHT_CLICK"};
  int inx = aEvent->mMessage - eMouseEventFirst;
  if (inx >= 0 && inx <= (NS_MOUSE_RIGHT_CLICK - eMouseEventFirst)) {
    printf("Mouse in ListFrame %s [%d]\n", desc[inx], aEvent->mMessage);
  } else {
    printf("Mouse in ListFrame <UNKNOWN> [%d]\n", aEvent->mMessage);
  }*/

  if (nsEventStatus_eConsumeNoDefault == *aEventStatus)
    return NS_OK;

  // disabled state affects how we're selected, but we don't want to go through
  // nsHTMLScrollFrame if we're disabled.
  if (IsContentDisabled()) {
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  return nsHTMLScrollFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


//---------------------------------------------------------
void
nsListControlFrame::SetInitialChildList(ChildListID    aListID,
                                        nsFrameList&   aChildList)
{
  if (aListID == kPrincipalList) {
    // First check to see if all the content has been added
    mIsAllContentHere = mContent->IsDoneAddingChildren();
    if (!mIsAllContentHere) {
      mIsAllFramesHere    = false;
      mHasBeenInitialized = false;
    }
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

  if (!nsLayoutUtils::IsContentSelectEnabled() &&
      IsInDropDownMode()) {
    // TODO(kuoe0) Remove the following code when content-select is enabled.
    AddStateBits(NS_FRAME_IN_POPUP);
    CreateView();
  }

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
}

dom::HTMLOptionsCollection*
nsListControlFrame::GetOptions() const
{
  dom::HTMLSelectElement* select =
    dom::HTMLSelectElement::FromNodeOrNull(mContent);
  NS_ENSURE_TRUE(select, nullptr);

  return select->Options();
}

dom::HTMLOptionElement*
nsListControlFrame::GetOption(uint32_t aIndex) const
{
  dom::HTMLSelectElement* select =
    dom::HTMLSelectElement::FromNodeOrNull(mContent);
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

    HTMLSelectElement* selectElement = HTMLSelectElement::FromNode(mContent);
    if (selectElement) {
      indexToSelect = selectElement->SelectedIndex();
      AutoWeakFrame weakFrame(this);
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
    dom::HTMLSelectElement::FromNodeOrNull(mContent);
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
    return GetOption(AssertedCast<uint32_t>(focusedIndex));
  }

  // There is no selected option. Return the first non-disabled option, if any.
  return GetNonDisabledOptionFrom(0);
}

HTMLOptionElement*
nsListControlFrame::GetNonDisabledOptionFrom(int32_t aFromIndex,
                                             int32_t* aFoundIndex)
{
  RefPtr<dom::HTMLSelectElement> selectElement =
    dom::HTMLSelectElement::FromNode(mContent);

  const uint32_t length = selectElement->Length();
  for (uint32_t i = std::max(aFromIndex, 0); i < length; ++i) {
    HTMLOptionElement* node = selectElement->Item(i);
    if (!node) {
      break;
    }
    if (!selectElement->IsOptionDisabled(node)) {
      if (aFoundIndex) {
        *aFoundIndex = i;
      }
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
  RefPtr<dom::HTMLSelectElement> selectElement =
    dom::HTMLSelectElement::FromNode(mContent);

  uint32_t mask = dom::HTMLSelectElement::NOTIFY;
  if (mForceSelection) {
    mask |= dom::HTMLSelectElement::SET_DISABLED;
  }
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
  RefPtr<dom::HTMLOptionElement> option =
    GetOption(static_cast<uint32_t>(aIndex));
  NS_ENSURE_TRUE(option, false);

  RefPtr<dom::HTMLSelectElement> selectElement =
    dom::HTMLSelectElement::FromNode(mContent);

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
    AutoWeakFrame weakFrame(this);
    if (mComboboxFrame) {
      mComboboxFrame->RedisplaySelectedText();

      // When dropdown list is open, onchange event will be fired when Enter key
      // is hit or when dropdown list is dismissed.
      if (mComboboxFrame->IsDroppedDown()) {
        return weakFrame.IsAlive();
      }
    }
    if (mIsAllContentHere) {
      FireOnInputAndOnChange();
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
    int32_t displayIndex = mComboboxFrame->GetIndexOfDisplayArea();
    // Make sure we can always reset to the displayed index
    mForceSelection = displayIndex == aIndex;

    AutoWeakFrame weakFrame(this);
    PerformSelection(aIndex, false, false);  // might destroy us
    if (!weakFrame.IsAlive() || !mComboboxFrame) {
      return;
    }

    if (displayIndex != aIndex) {
      mComboboxFrame->RedisplaySelectedText(); // might destroy us
    }

    if (weakFrame.IsAlive() && mComboboxFrame) {
      mComboboxFrame->RollupFromList(); // might destroy us
    }
  }
}

// Send out an onInput and onChange notification.
void
nsListControlFrame::FireOnInputAndOnChange()
{
  if (mComboboxFrame) {
    // Return hit without changing anything
    int32_t index = mComboboxFrame->UpdateRecentIndex(NS_SKIP_NOTIFY_INDEX);
    if (index == NS_SKIP_NOTIFY_INDEX) {
      return;
    }

    // See if the selection actually changed
    if (index == GetSelectedIndex()) {
      return;
    }
  }

  nsCOMPtr<nsIContent> content = mContent;
  // Dispatch the input event.
  nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(), content,
                                       NS_LITERAL_STRING("input"), true,
                                       false);
  // Dispatch the change event.
  nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(), content,
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

  AutoWeakFrame weakFrame(this);
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
nsListControlFrame::SetFormProperty(nsAtom* aName,
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
  nsIFrame* ancestor = comboboxFrame->GetParent();
  mLastDropdownBackstopColor = NS_RGBA(0,0,0,0);
  while (NS_GET_A(mLastDropdownBackstopColor) < 255 && ancestor) {
    ComputedStyle* context = ancestor->Style();
    mLastDropdownBackstopColor =
      NS_ComposeColors(context->StyleBackground()->BackgroundColor(context),
                       mLastDropdownBackstopColor);
    ancestor = ancestor->GetParent();
  }
  mLastDropdownBackstopColor =
    NS_ComposeColors(PresContext()->DefaultBackgroundColor(),
                     mLastDropdownBackstopColor);

  if (mIsAllContentHere && mIsAllFramesHere && mHasBeenInitialized) {
    AutoWeakFrame weakFrame(this);
    ScrollToIndex(GetSelectedIndex());
    if (!weakFrame.IsAlive()) {
      return;
    }
#ifdef ACCESSIBILITY
    FireMenuItemActiveEvent(); // Inform assistive tech what got focus
#endif
  }
  mItemSelectionStarted = false;
  mForceSelection = false;
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
                              const ReflowInput* aReflowInput)
{
  bool wasInterrupted = !mHasPendingInterruptAtStartOfReflow &&
                          aPresContext->HasPendingInterrupt();

  nsHTMLScrollFrame::DidReflow(aPresContext, aReflowInput);

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

#ifdef DEBUG_FRAME_DUMP
nsresult
nsListControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ListControl"), aResult);
}
#endif

nscoord
nsListControlFrame::GetBSizeOfARow()
{
  return BSizeOfARow();
}

nsresult
nsListControlFrame::IsOptionDisabled(int32_t anIndex, bool &aIsDisabled)
{
  RefPtr<dom::HTMLSelectElement> sel =
    dom::HTMLSelectElement::FromNode(mContent);
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
nsListControlFrame::IsLeftButton(dom::Event* aMouseEvent)
{
  // only allow selection with the left button
  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
  return mouseEvent && mouseEvent->Button() == 0;
}

nscoord
nsListControlFrame::CalcFallbackRowBSize(float aFontSizeInflation)
{
  RefPtr<nsFontMetrics> fontMet =
    nsLayoutUtils::GetFontMetricsForFrame(this, aFontSizeInflation);
  return fontMet->MaxHeight();
}

nscoord
nsListControlFrame::CalcIntrinsicBSize(nscoord aBSizeOfARow,
                                       int32_t aNumberOfOptions)
{
  NS_PRECONDITION(!IsInDropDownMode(),
                  "Shouldn't be in dropdown mode when we call this");

  dom::HTMLSelectElement* select =
    dom::HTMLSelectElement::FromNodeOrNull(mContent);
  if (select) {
    mNumDisplayRows = select->Size();
  } else {
    mNumDisplayRows = 1;
  }

  if (mNumDisplayRows < 1) {
    mNumDisplayRows = 4;
  }

  return mNumDisplayRows * aBSizeOfARow;
}

//----------------------------------------------------------------------
// nsIDOMMouseListener
//----------------------------------------------------------------------
nsresult
nsListControlFrame::MouseUp(dom::Event* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nullptr, "aMouseEvent is null.");

  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
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
      aMouseEvent->WidgetEventPtr()->AsMouseEvent();

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
        AutoWeakFrame weakFrame(this);
        ComboboxFinish(selectedIndex);
        if (!weakFrame.IsAlive()) {
          return NS_OK;
        }

        FireOnInputAndOnChange();
      }

      mouseEvent->mClickCount = 1;
    } else {
      // the click was out side of the select or its dropdown
      mouseEvent->mClickCount =
        IgnoreMouseEventForSelection(aMouseEvent) ? 1 : 0;
    }
  } else {
    CaptureMouseEvents(false);
    // Notify
    if (mChangesSinceDragStart) {
      // reset this so that future MouseUps without a prior MouseDown
      // won't fire onchange
      mChangesSinceDragStart = false;
      FireOnInputAndOnChange();
    }
  }

  return NS_OK;
}

void
nsListControlFrame::UpdateInListState(dom::Event* aEvent)
{
  if (!mComboboxFrame || !mComboboxFrame->IsDroppedDown())
    return;

  nsPoint pt = nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aEvent, this);
  nsRect borderInnerEdge = GetScrollPortRect();
  if (pt.y >= borderInnerEdge.y && pt.y < borderInnerEdge.YMost()) {
    mItemSelectionStarted = true;
  }
}

bool nsListControlFrame::IgnoreMouseEventForSelection(dom::Event* aEvent)
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
nsListControlFrame::GetIndexFromDOMEvent(dom::Event* aMouseEvent,
                                         int32_t&    aCurIndex)
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

  RefPtr<dom::HTMLOptionElement> option;
  for (nsCOMPtr<nsIContent> content =
         PresContext()->EventStateManager()->GetEventTargetContent(nullptr);
       content && !option;
       content = content->GetParent()) {
    option = dom::HTMLOptionElement::FromNode(content);
  }

  if (option) {
    aCurIndex = option->Index();
    MOZ_ASSERT(aCurIndex >= 0);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

static bool
FireShowDropDownEvent(nsIContent* aContent, bool aShow, bool aIsSourceTouchEvent)
{
  if (ShouldFireDropDownEvent()) {
    nsString eventName;
    if (aShow) {
      eventName = aIsSourceTouchEvent ? NS_LITERAL_STRING("mozshowdropdown-sourcetouch") :
                                        NS_LITERAL_STRING("mozshowdropdown");
    } else {
      eventName = NS_LITERAL_STRING("mozhidedropdown");
    }
    nsContentUtils::DispatchChromeEvent(aContent->OwnerDoc(), aContent,
                                        eventName, true, false);
    return true;
  }

  return false;
}

nsresult
nsListControlFrame::MouseDown(dom::Event* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent != nullptr, "aMouseEvent is null.");

  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
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
    AutoWeakFrame weakFrame(this);
    bool change =
      HandleListSelection(aMouseEvent, selectedIndex); // might destroy us
    if (!weakFrame.IsAlive()) {
      return NS_OK;
    }
    mChangesSinceDragStart = change;
  } else {
    // NOTE: the combo box is responsible for dropping it down
    if (mComboboxFrame) {
      // Ignore the click that occurs on the option element when one is
      // selected from the parent process popup.
      if (mComboboxFrame->IsOpenInParentProcess()) {
        nsCOMPtr<nsIContent> econtent =
          do_QueryInterface(aMouseEvent->GetTarget());
        HTMLOptionElement* option = HTMLOptionElement::FromNodeOrNull(econtent);
        if (option) {
          return NS_OK;
        }
      }

      uint16_t inputSource = mouseEvent->MozInputSource();
      bool isSourceTouchEvent = inputSource == MouseEventBinding::MOZ_SOURCE_TOUCH;
      if (FireShowDropDownEvent(mContent, !mComboboxFrame->IsDroppedDownOrHasParentPopup(),
                                isSourceTouchEvent)) {
        return NS_OK;
      }

      if (!IgnoreMouseEventForSelection(aMouseEvent)) {
        return NS_OK;
      }

      if (!nsComboboxControlFrame::ToolkitHasNativePopup())
      {
        bool isDroppedDown = mComboboxFrame->IsDroppedDown();
        nsIFrame* comboFrame = do_QueryFrame(mComboboxFrame);
        AutoWeakFrame weakFrame(comboFrame);
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

nsresult
nsListControlFrame::MouseMove(dom::Event* aMouseEvent)
{
  NS_ASSERTION(aMouseEvent, "aMouseEvent is null.");
  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
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
nsListControlFrame::DragMove(dom::Event* aMouseEvent)
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
      MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
      NS_ASSERTION(mouseEvent, "aMouseEvent is not a MouseEvent!");
      bool isControl;
#ifdef XP_MACOSX
      isControl = mouseEvent->MetaKey();
#else
      isControl = mouseEvent->CtrlKey();
#endif
      AutoWeakFrame weakFrame(this);
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
    RefPtr<dom::HTMLOptionElement> option =
      GetOption(AssertedCast<uint32_t>(aIndex));
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
    PresShell()->
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
nsListControlFrame::DropDownToggleKey(dom::Event* aKeyEvent)
{
  // Cocoa widgets do native popups, so don't try to show
  // dropdowns there.
  if (IsInDropDownMode() && !nsComboboxControlFrame::ToolkitHasNativePopup()) {
    aKeyEvent->PreventDefault();
    if (!mComboboxFrame->IsDroppedDown()) {
      if (!FireShowDropDownEvent(mContent, true, false)) {
        mComboboxFrame->ShowDropDown(true);
      }
    } else {
      AutoWeakFrame weakFrame(this);
      // mEndSelectionIndex is the last item that got selected.
      ComboboxFinish(mEndSelectionIndex);
      if (weakFrame.IsAlive()) {
        FireOnInputAndOnChange();
      }
    }
  }
}

nsresult
nsListControlFrame::KeyDown(dom::Event* aKeyEvent)
{
  MOZ_ASSERT(aKeyEvent, "aKeyEvent is null.");

  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    return NS_OK;
  }

  AutoIncrementalSearchResetter incrementalSearchResetter;

  // Don't check defaultPrevented value because other browsers don't prevent
  // the key navigation of list control even if preventDefault() is called.
  // XXXmats 2015-04-16: the above is not true anymore, Chrome prevents all
  // XXXmats keyboard events, even tabbing, when preventDefault() is called
  // XXXmats in onkeydown. That seems sub-optimal though.

  const WidgetKeyboardEvent* keyEvent =
    aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  MOZ_ASSERT(keyEvent,
    "DOM event must have WidgetKeyboardEvent for its internal event");

  bool dropDownMenuOnUpDown;
  bool dropDownMenuOnSpace;
#ifdef XP_MACOSX
  dropDownMenuOnUpDown = IsInDropDownMode() && !mComboboxFrame->IsDroppedDown();
  dropDownMenuOnSpace = !keyEvent->IsAlt() && !keyEvent->IsControl() &&
    !keyEvent->IsMeta();
#else
  dropDownMenuOnUpDown = keyEvent->IsAlt();
  dropDownMenuOnSpace = IsInDropDownMode() && !mComboboxFrame->IsDroppedDown();
#endif
  bool withinIncrementalSearchTime =
    keyEvent->mTime - gLastKeyTime <= INCREMENTAL_SEARCH_KEYPRESS_TIME;
  if ((dropDownMenuOnUpDown &&
       (keyEvent->mKeyCode == NS_VK_UP || keyEvent->mKeyCode == NS_VK_DOWN)) ||
      (dropDownMenuOnSpace && keyEvent->mKeyCode == NS_VK_SPACE &&
       !withinIncrementalSearchTime)) {
    DropDownToggleKey(aKeyEvent);
    if (keyEvent->DefaultPrevented()) {
      return NS_OK;
    }
  }
  if (keyEvent->IsAlt()) {
    return NS_OK;
  }

  // now make sure there are options or we are wasting our time
  RefPtr<dom::HTMLOptionsCollection> options = GetOptions();
  NS_ENSURE_TRUE(options, NS_ERROR_FAILURE);

  uint32_t numOptions = options->Length();

  // this is the new index to set
  int32_t newIndex = kNothingSelected;

  bool isControlOrMeta = (keyEvent->IsControl() || keyEvent->IsMeta());
  // Don't try to handle multiple-select pgUp/pgDown in single-select lists.
  if (isControlOrMeta && !GetMultiple() &&
      (keyEvent->mKeyCode == NS_VK_PAGE_UP ||
       keyEvent->mKeyCode == NS_VK_PAGE_DOWN)) {
    return NS_OK;
  }
  if (isControlOrMeta && (keyEvent->mKeyCode == NS_VK_UP ||
                          keyEvent->mKeyCode == NS_VK_LEFT ||
                          keyEvent->mKeyCode == NS_VK_DOWN ||
                          keyEvent->mKeyCode == NS_VK_RIGHT ||
                          keyEvent->mKeyCode == NS_VK_HOME ||
                          keyEvent->mKeyCode == NS_VK_END)) {
    // Don't go into multiple-select mode unless this list can handle it.
    isControlOrMeta = mControlSelectMode = GetMultiple();
  } else if (keyEvent->mKeyCode != NS_VK_SPACE) {
    mControlSelectMode = false;
  }

  // We should not change the selection if the popup is "opened
  // in the parent process" (even when we're in single-process mode).
  bool shouldSelectByKey = !mComboboxFrame ||
                           !mComboboxFrame->IsOpenInParentProcess();

  switch (keyEvent->mKeyCode) {
    case NS_VK_UP:
    case NS_VK_LEFT:
      if (shouldSelectByKey) {
        AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                  static_cast<int32_t>(numOptions),
                                  -1, -1);
      }
      break;
    case NS_VK_DOWN:
    case NS_VK_RIGHT:
      if (shouldSelectByKey) {
        AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                  static_cast<int32_t>(numOptions),
                                  1, 1);
      }
      break;
    case NS_VK_RETURN:
      if (IsInDropDownMode()) {
        if (mComboboxFrame->IsDroppedDown()) {
          // If the select element is a dropdown style, Enter key should be
          // consumed while the dropdown is open for security.
          aKeyEvent->PreventDefault();

          AutoWeakFrame weakFrame(this);
          ComboboxFinish(mEndSelectionIndex);
          if (!weakFrame.IsAlive()) {
            return NS_OK;
          }
        }
        FireOnInputAndOnChange();
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
      if (shouldSelectByKey) {
        int32_t itemsPerPage =
          std::max(1, static_cast<int32_t>(mNumDisplayRows - 1));
        AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                  static_cast<int32_t>(numOptions),
                                  -itemsPerPage, -1);
      }
      break;
    }
    case NS_VK_PAGE_DOWN: {
      if (shouldSelectByKey) {
        int32_t itemsPerPage =
          std::max(1, static_cast<int32_t>(mNumDisplayRows - 1));
        AdjustIndexForDisabledOpt(mEndSelectionIndex, newIndex,
                                  static_cast<int32_t>(numOptions),
                                  itemsPerPage, 1);
      }
      break;
    }
    case NS_VK_HOME:
      if (shouldSelectByKey) {
        AdjustIndexForDisabledOpt(0, newIndex,
                                  static_cast<int32_t>(numOptions),
                                  0, 1);
      }
      break;
    case NS_VK_END:
      if (shouldSelectByKey) {
        AdjustIndexForDisabledOpt(static_cast<int32_t>(numOptions) - 1, newIndex,
                                  static_cast<int32_t>(numOptions),
                                  0, -1);
      }
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
nsListControlFrame::KeyPress(dom::Event* aKeyEvent)
{
  MOZ_ASSERT(aKeyEvent, "aKeyEvent is null.");

  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    return NS_OK;
  }

  AutoIncrementalSearchResetter incrementalSearchResetter;

  const WidgetKeyboardEvent* keyEvent =
    aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  MOZ_ASSERT(keyEvent,
    "DOM event must have WidgetKeyboardEvent for its internal event");

  // Select option with this as the first character
  // XXX Not I18N compliant

  // Don't do incremental search if the key event has already consumed.
  if (keyEvent->DefaultPrevented()) {
    return NS_OK;
  }

  if (keyEvent->IsAlt()) {
    return NS_OK;
  }

  // With some keyboard layout, space key causes non-ASCII space.
  // So, the check in keydown event handler isn't enough, we need to check it
  // again with keypress event.
  if (keyEvent->mCharCode != ' ') {
    mControlSelectMode = false;
  }

  bool isControlOrMeta = (keyEvent->IsControl() || keyEvent->IsMeta());
  if (isControlOrMeta && keyEvent->mCharCode != ' ') {
    return NS_OK;
  }

  // NOTE: If mKeyCode of keypress event is not 0, mCharCode is always 0.
  //       Therefore, all non-printable keys are not handled after this block.
  if (!keyEvent->mCharCode) {
    // Backspace key will delete the last char in the string.  Otherwise,
    // non-printable keypress should reset incremental search.
    if (keyEvent->mKeyCode == NS_VK_BACK) {
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
  if (keyEvent->mTime - gLastKeyTime > INCREMENTAL_SEARCH_KEYPRESS_TIME) {
    // If this is ' ' and we are at the beginning of the string, treat it as
    // "select this option" (bug 191543)
    if (keyEvent->mCharCode == ' ') {
      // Actually process the new index and let the selection code
      // do the scrolling for us
      PostHandleKeyEvent(mEndSelectionIndex, keyEvent->mCharCode,
                         keyEvent->IsShift(), isControlOrMeta);

      return NS_OK;
    }

    GetIncrementalString().Truncate();
  }

  gLastKeyTime = keyEvent->mTime;

  // Append this keystroke to the search string.
  char16_t uniChar = ToLowerCase(static_cast<char16_t>(keyEvent->mCharCode));
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
  RefPtr<dom::HTMLOptionsCollection> options = GetOptions();
  NS_ENSURE_TRUE(options, NS_ERROR_FAILURE);

  uint32_t numOptions = options->Length();

  AutoWeakFrame weakFrame(this);
  for (uint32_t i = 0; i < numOptions; ++i) {
    uint32_t index = (i + startIndex) % numOptions;
    RefPtr<dom::HTMLOptionElement> optionElement =
      options->ItemAsOption(index);
    if (!optionElement || !optionElement->GetPrimaryFrame()) {
      continue;
    }

    nsAutoString text;
    optionElement->GetText(text);
    if (!StringBeginsWith(
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
    int32_t focusedIndex = mEndSelectionIndex == kNothingSelected ?
      GetSelectedIndex() : mEndSelectionIndex;
    if (focusedIndex != kNothingSelected) {
      return;
    }
    // No options are selected.  In this case the focus ring is on the first
    // non-disabled option (if any), so we should behave as if that's the option
    // the user acted on.
    if (!GetNonDisabledOptionFrom(0, &aNewIndex)) {
      return;
    }
  }

  // If you hold control, but not shift, no key will actually do anything
  // except space.
  AutoWeakFrame weakFrame(this);
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
nsListEventListener::HandleEvent(dom::Event* aEvent)
{
  if (!mFrame)
    return NS_OK;

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("keydown")) {
    return mFrame->nsListControlFrame::KeyDown(aEvent);
  }
  if (eventType.EqualsLiteral("keypress")) {
    return mFrame->nsListControlFrame::KeyPress(aEvent);
  }
  if (eventType.EqualsLiteral("mousedown")) {
    if (aEvent->DefaultPrevented()) {
      return NS_OK;
    }
    return mFrame->nsListControlFrame::MouseDown(aEvent);
  }
  if (eventType.EqualsLiteral("mouseup")) {
    // Don't try to honor defaultPrevented here - it's not web compatible.
    // (bug 1194733)
    return mFrame->nsListControlFrame::MouseUp(aEvent);
  }
  if (eventType.EqualsLiteral("mousemove")) {
    // I don't think we want to honor defaultPrevented on mousemove
    // in general, and it would only prevent highlighting here.
    return mFrame->nsListControlFrame::MouseMove(aEvent);
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected eventType");
  return NS_OK;
}

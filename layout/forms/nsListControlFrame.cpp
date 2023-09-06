/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsUnicharUtils.h"
#include "nsListControlFrame.h"
#include "HTMLSelectEventListener.h"
#include "nsGkAtoms.h"
#include "nsComboboxControlFrame.h"
#include "nsFontMetrics.h"
#include "nsIScrollableFrame.h"
#include "nsCSSRendering.h"
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
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/TextEvents.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

// Static members
nsListControlFrame* nsListControlFrame::mFocused = nullptr;

//---------------------------------------------------------
nsListControlFrame* NS_NewListControlFrame(PresShell* aPresShell,
                                           ComputedStyle* aStyle) {
  nsListControlFrame* it =
      new (aPresShell) nsListControlFrame(aStyle, aPresShell->GetPresContext());

  it->AddStateBits(NS_FRAME_INDEPENDENT_SELECTION);

  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsListControlFrame)

nsListControlFrame::nsListControlFrame(ComputedStyle* aStyle,
                                       nsPresContext* aPresContext)
    : nsHTMLScrollFrame(aStyle, aPresContext, kClassID, false),
      mMightNeedSecondPass(false),
      mHasPendingInterruptAtStartOfReflow(false),
      mForceSelection(false) {
  mChangesSinceDragStart = false;

  mIsAllContentHere = false;
  mIsAllFramesHere = false;
  mHasBeenInitialized = false;
  mNeedToReset = true;
  mPostChildrenLoadedReset = false;
}

nsListControlFrame::~nsListControlFrame() = default;

Maybe<nscoord> nsListControlFrame::GetNaturalBaselineBOffset(
    WritingMode aWM, BaselineSharingGroup aBaselineGroup,
    BaselineExportContext) const {
  // Unlike scroll frames which we inherit from, we don't export a baseline.
  return Nothing{};
}
// for Bug 47302 (remove this comment later)
void nsListControlFrame::Destroy(DestroyContext& aContext) {
  // get the receiver interface from the browser button's content node
  NS_ENSURE_TRUE_VOID(mContent);

  // Clear the frame pointer on our event listener, just in case the
  // event listener can outlive the frame.

  mEventListener->Detach();
  nsHTMLScrollFrame::Destroy(aContext);
}

void nsListControlFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  // We allow visibility:hidden <select>s to contain visible options.

  // Don't allow painting of list controls when painting is suppressed.
  // XXX why do we need this here? we should never reach this. Maybe
  // because these can have widgets? Hmm
  if (aBuilder->IsBackgroundOnly()) return;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsListControlFrame");

  nsHTMLScrollFrame::BuildDisplayList(aBuilder, aLists);
}

HTMLOptionElement* nsListControlFrame::GetCurrentOption() const {
  return mEventListener->GetCurrentOption();
}

/**
 * This is called by the SelectsAreaFrame, which is the same
 * as the frame returned by GetOptionsContainer. It's the frame which is
 * scrolled by us.
 * @param aPt the offset of this frame, relative to the rendering reference
 * frame
 */
void nsListControlFrame::PaintFocus(DrawTarget* aDrawTarget, nsPoint aPt) {
  if (mFocused != this) return;

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

  const auto* domOpt = HTMLOptionElement::FromNodeOrNull(focusedContent);
  const bool isSelected = domOpt && domOpt->Selected();

  // Set up back stop colors and then ask L&F service for the real colors
  nscolor color =
      LookAndFeel::Color(isSelected ? LookAndFeel::ColorID::Selecteditemtext
                                    : LookAndFeel::ColorID::Selecteditem,
                         this);

  nsCSSRendering::PaintFocus(PresContext(), aDrawTarget, fRect, color);
}

void nsListControlFrame::InvalidateFocus() {
  if (mFocused != this) return;

  nsIFrame* containerFrame = GetOptionsContainer();
  if (containerFrame) {
    containerFrame->InvalidateFrame();
  }
}

NS_QUERYFRAME_HEAD(nsListControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsISelectControlFrame)
  NS_QUERYFRAME_ENTRY(nsListControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsHTMLScrollFrame)

#ifdef ACCESSIBILITY
a11y::AccType nsListControlFrame::AccessibleType() {
  return a11y::eHTMLSelectListType;
}
#endif

// Return true if we found at least one <option> or non-empty <optgroup> label
// that has a frame.  aResult will be the maximum BSize of those.
static bool GetMaxRowBSize(nsIFrame* aContainer, WritingMode aWM,
                           nscoord* aResult) {
  bool found = false;
  for (nsIFrame* child : aContainer->PrincipalChildList()) {
    if (child->GetContent()->IsHTMLElement(nsGkAtoms::optgroup)) {
      // An optgroup; drill through any scroll frame and recurse.  |inner| might
      // be null here though if |inner| is an anonymous leaf frame of some sort.
      auto inner = child->GetContentInsertionFrame();
      if (inner && GetMaxRowBSize(inner, aWM, aResult)) {
        found = true;
      }
    } else {
      // an option or optgroup label
      bool isOptGroupLabel =
          child->Style()->IsPseudoElement() &&
          aContainer->GetContent()->IsHTMLElement(nsGkAtoms::optgroup);
      nscoord childBSize = child->BSize(aWM);
      // XXX bug 1499176: skip empty <optgroup> labels (zero bsize) for now
      if (!isOptGroupLabel || childBSize > nscoord(0)) {
        found = true;
        *aResult = std::max(childBSize, *aResult);
      }
    }
  }
  return found;
}

//-----------------------------------------------------------------
// Main Reflow for ListBox/Dropdown
//-----------------------------------------------------------------

nscoord nsListControlFrame::CalcBSizeOfARow() {
  // Calculate the block size in our writing mode of a single row in the
  // listbox or dropdown list by using the tallest thing in the subtree,
  // since there may be option groups in addition to option elements,
  // either of which may be visible or invisible, may use different
  // fonts, etc.
  nscoord rowBSize(0);
  if (GetContainSizeAxes().mBContained ||
      !GetMaxRowBSize(GetOptionsContainer(), GetWritingMode(), &rowBSize)) {
    // We don't have any <option>s or <optgroup> labels with a frame.
    // (Or we're size-contained in block axis, which has the same outcome for
    // our sizing.)
    float inflation = nsLayoutUtils::FontSizeInflationFor(this);
    rowBSize = CalcFallbackRowBSize(inflation);
  }
  return rowBSize;
}

nscoord nsListControlFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  // Always add scrollbar inline sizes to the pref-inline-size of the
  // scrolled content. Combobox frames depend on this happening in the
  // dropdown, and standalone listboxes are overflow:scroll so they need
  // it too.
  WritingMode wm = GetWritingMode();
  Maybe<nscoord> containISize = ContainIntrinsicISize();
  result = containISize ? *containISize
                        : GetScrolledFrame()->GetPrefISize(aRenderingContext);
  LogicalMargin scrollbarSize(wm, GetDesiredScrollbarSizes());
  result = NSCoordSaturatingAdd(result, scrollbarSize.IStartEnd(wm));
  return result;
}

nscoord nsListControlFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  // Always add scrollbar inline sizes to the min-inline-size of the
  // scrolled content. Combobox frames depend on this happening in the
  // dropdown, and standalone listboxes are overflow:scroll so they need
  // it too.
  WritingMode wm = GetWritingMode();
  Maybe<nscoord> containISize = ContainIntrinsicISize();
  result = containISize ? *containISize
                        : GetScrolledFrame()->GetMinISize(aRenderingContext);
  LogicalMargin scrollbarSize(wm, GetDesiredScrollbarSizes());
  result += scrollbarSize.IStartEnd(wm);

  return result;
}

void nsListControlFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_WARNING_ASSERTION(aReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE,
                       "Must have a computed inline size");

  SchedulePaint();

  mHasPendingInterruptAtStartOfReflow = aPresContext->HasPendingInterrupt();

  // If all the content and frames are here
  // then initialize it before reflow
  if (mIsAllContentHere && !mHasBeenInitialized) {
    if (!mIsAllFramesHere) {
      CheckIfAllFramesHere();
    }
    if (mIsAllFramesHere && !mHasBeenInitialized) {
      mHasBeenInitialized = true;
    }
  }

  MarkInReflow();
  // Due to the fact that our intrinsic block size depends on the block
  // sizes of our kids, we end up having to do two-pass reflow, in
  // general -- the first pass to find the intrinsic block size and a
  // second pass to reflow the scrollframe at that block size (which
  // will size the scrollbars correctly, etc).
  //
  // Naturally, we want to avoid doing the second reflow as much as
  // possible. We can skip it in the following cases (in all of which the first
  // reflow is already happening at the right block size):
  bool autoBSize = (aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE);
  Maybe<nscoord> containBSize = ContainIntrinsicBSize(NS_UNCONSTRAINEDSIZE);
  bool usingContainBSize =
      autoBSize && containBSize && *containBSize != NS_UNCONSTRAINEDSIZE;

  mMightNeedSecondPass = [&] {
    if (!autoBSize) {
      // We're reflowing with a constrained computed block size -- just use that
      // block size.
      return false;
    }
    if (!IsSubtreeDirty() && !aReflowInput.ShouldReflowAllKids()) {
      // We're not dirty and have no dirty kids and shouldn't be reflowing all
      // kids. In this case, our cached max block size of a child is not going
      // to change.
      return false;
    }
    if (usingContainBSize) {
      // We're size-contained in the block axis. In this case the size of a row
      // doesn't depend on our children (it's the "fallback" size).
      return false;
    }
    // We might need to do a second pass. If we do our first reflow using our
    // cached max block size of a child, then compute the new max block size,
    // and it's the same as the old one, we might still skip it (see the
    // IsScrollbarUpdateSuppressed() check).
    return true;
  }();

  ReflowInput state(aReflowInput);
  int32_t length = GetNumberOfRows();

  nscoord oldBSizeOfARow = BSizeOfARow();

  if (!HasAnyStateBits(NS_FRAME_FIRST_REFLOW) && autoBSize) {
    // When not doing an initial reflow, and when the block size is
    // auto, start off with our computed block size set to what we'd
    // expect our block size to be.
    nscoord computedBSize = CalcIntrinsicBSize(oldBSizeOfARow, length);
    computedBSize = state.ApplyMinMaxBSize(computedBSize);
    state.SetComputedBSize(computedBSize);
  }

  if (usingContainBSize) {
    state.SetComputedBSize(*containBSize);
  }

  nsHTMLScrollFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);

  if (!mMightNeedSecondPass) {
    NS_ASSERTION(!autoBSize || BSizeOfARow() == oldBSizeOfARow,
                 "How did our BSize of a row change if nothing was dirty?");
    NS_ASSERTION(!autoBSize || !HasAnyStateBits(NS_FRAME_FIRST_REFLOW) ||
                     usingContainBSize,
                 "How do we not need a second pass during initial reflow at "
                 "auto BSize?");
    NS_ASSERTION(!IsScrollbarUpdateSuppressed(),
                 "Shouldn't be suppressing if we don't need a second pass!");
    if (!autoBSize || usingContainBSize) {
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

bool nsListControlFrame::ShouldPropagateComputedBSizeToScrolledContent() const {
  return true;
}

//---------------------------------------------------------
nsContainerFrame* nsListControlFrame::GetContentInsertionFrame() {
  return GetOptionsContainer()->GetContentInsertionFrame();
}

//---------------------------------------------------------
bool nsListControlFrame::ExtendedSelection(int32_t aStartIndex,
                                           int32_t aEndIndex, bool aClearAll) {
  return SetOptionsSelectedFromFrame(aStartIndex, aEndIndex, true, aClearAll);
}

//---------------------------------------------------------
bool nsListControlFrame::SingleSelection(int32_t aClickedIndex,
                                         bool aDoToggle) {
#ifdef ACCESSIBILITY
  nsCOMPtr<nsIContent> prevOption = mEventListener->GetCurrentOption();
#endif
  bool wasChanged = false;
  // Get Current selection
  if (aDoToggle) {
    wasChanged = ToggleOptionSelectedFromFrame(aClickedIndex);
  } else {
    wasChanged =
        SetOptionsSelectedFromFrame(aClickedIndex, aClickedIndex, true, true);
  }
  AutoWeakFrame weakFrame(this);
  ScrollToIndex(aClickedIndex);
  if (!weakFrame.IsAlive()) {
    return wasChanged;
  }

  mStartSelectionIndex = aClickedIndex;
  mEndSelectionIndex = aClickedIndex;
  InvalidateFocus();

#ifdef ACCESSIBILITY
  FireMenuItemActiveEvent(prevOption);
#endif

  return wasChanged;
}

void nsListControlFrame::InitSelectionRange(int32_t aClickedIndex) {
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
      mStartSelectionIndex = i - 1;
      mEndSelectionIndex = selectedIndex;
    } else {
      // User clicked after selection, so start selection at start of
      // contiguous selection
      mStartSelectionIndex = selectedIndex;
      mEndSelectionIndex = i - 1;
    }
  }
}

static uint32_t CountOptionsAndOptgroups(nsIFrame* aFrame) {
  uint32_t count = 0;
  for (nsIFrame* child : aFrame->PrincipalChildList()) {
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

uint32_t nsListControlFrame::GetNumberOfRows() {
  return ::CountOptionsAndOptgroups(GetContentInsertionFrame());
}

//---------------------------------------------------------
bool nsListControlFrame::PerformSelection(int32_t aClickedIndex, bool aIsShift,
                                          bool aIsControl) {
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
        endIndex = aClickedIndex;
      } else if (mStartSelectionIndex <= aClickedIndex) {
        startIndex = mStartSelectionIndex;
        endIndex = aClickedIndex;
      } else {
        startIndex = aClickedIndex;
        endIndex = mStartSelectionIndex;
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
      nsCOMPtr<nsIContent> prevOption = GetCurrentOption();
#endif
      mEndSelectionIndex = aClickedIndex;
      InvalidateFocus();

#ifdef ACCESSIBILITY
      FireMenuItemActiveEvent(prevOption);
#endif
    } else if (aIsControl) {
      wasChanged = SingleSelection(aClickedIndex, true);  // might destroy us
    } else {
      wasChanged = SingleSelection(aClickedIndex, false);  // might destroy us
    }
  } else {
    wasChanged = SingleSelection(aClickedIndex, false);  // might destroy us
  }

  return wasChanged;
}

//---------------------------------------------------------
bool nsListControlFrame::HandleListSelection(dom::Event* aEvent,
                                             int32_t aClickedIndex) {
  MouseEvent* mouseEvent = aEvent->AsMouseEvent();
  bool isControl;
#ifdef XP_MACOSX
  isControl = mouseEvent->MetaKey();
#else
  isControl = mouseEvent->CtrlKey();
#endif
  bool isShift = mouseEvent->ShiftKey();
  return PerformSelection(aClickedIndex, isShift,
                          isControl);  // might destroy us
}

//---------------------------------------------------------
void nsListControlFrame::CaptureMouseEvents(bool aGrabMouseEvents) {
  if (aGrabMouseEvents) {
    PresShell::SetCapturingContent(mContent, CaptureFlags::IgnoreAllowedState);
  } else {
    nsIContent* capturingContent = PresShell::GetCapturingContent();
    if (capturingContent == mContent) {
      // only clear the capturing content if *we* are the ones doing the
      // capturing (or if the dropdown is hidden, in which case NO-ONE should
      // be capturing anything - it could be a scrollbar inside this listbox
      // which is actually grabbing
      // This shouldn't be necessary. We should simply ensure that events
      // targeting scrollbars are never visible to DOM consumers.
      PresShell::ReleaseCapturingContent();
    }
  }
}

//---------------------------------------------------------
nsresult nsListControlFrame::HandleEvent(nsPresContext* aPresContext,
                                         WidgetGUIEvent* aEvent,
                                         nsEventStatus* aEventStatus) {
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

  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) return NS_OK;

  // disabled state affects how we're selected, but we don't want to go through
  // nsHTMLScrollFrame if we're disabled.
  if (IsContentDisabled()) {
    return nsIFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  return nsHTMLScrollFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

//---------------------------------------------------------
void nsListControlFrame::SetInitialChildList(ChildListID aListID,
                                             nsFrameList&& aChildList) {
  if (aListID == FrameChildListID::Principal) {
    // First check to see if all the content has been added
    mIsAllContentHere = Select().IsDoneAddingChildren();
    if (!mIsAllContentHere) {
      mIsAllFramesHere = false;
      mHasBeenInitialized = false;
    }
  }
  nsHTMLScrollFrame::SetInitialChildList(aListID, std::move(aChildList));
}

HTMLSelectElement& nsListControlFrame::Select() const {
  return *static_cast<HTMLSelectElement*>(GetContent());
}

//---------------------------------------------------------
void nsListControlFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                              nsIFrame* aPrevInFlow) {
  nsHTMLScrollFrame::Init(aContent, aParent, aPrevInFlow);

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  // we need to hook up our listeners before the editor is initialized
  mEventListener = new HTMLSelectEventListener(
      Select(), HTMLSelectEventListener::SelectType::Listbox);

  mStartSelectionIndex = kNothingSelected;
  mEndSelectionIndex = kNothingSelected;
}

dom::HTMLOptionsCollection* nsListControlFrame::GetOptions() const {
  return Select().Options();
}

dom::HTMLOptionElement* nsListControlFrame::GetOption(uint32_t aIndex) const {
  return Select().Item(aIndex);
}

NS_IMETHODIMP
nsListControlFrame::OnOptionSelected(int32_t aIndex, bool aSelected) {
  if (aSelected) {
    ScrollToIndex(aIndex);
  }
  return NS_OK;
}

void nsListControlFrame::OnContentReset() { ResetList(true); }

void nsListControlFrame::ResetList(bool aAllowScrolling) {
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

void nsListControlFrame::SetFocus(bool aOn, bool aRepaint) {
  InvalidateFocus();

  if (aOn) {
    mFocused = this;
  } else {
    mFocused = nullptr;
  }

  InvalidateFocus();
}

void nsListControlFrame::GetOptionText(uint32_t aIndex, nsAString& aStr) {
  aStr.Truncate();
  if (dom::HTMLOptionElement* optionElement = GetOption(aIndex)) {
    optionElement->GetRenderedLabel(aStr);
  }
}

int32_t nsListControlFrame::GetSelectedIndex() {
  dom::HTMLSelectElement* select =
      dom::HTMLSelectElement::FromNodeOrNull(mContent);
  return select->SelectedIndex();
}

uint32_t nsListControlFrame::GetNumberOfOptions() {
  dom::HTMLOptionsCollection* options = GetOptions();
  if (!options) {
    return 0;
  }

  return options->Length();
}

//----------------------------------------------------------------------
// nsISelectControlFrame
//----------------------------------------------------------------------
bool nsListControlFrame::CheckIfAllFramesHere() {
  // XXX Need to find a fail proof way to determine that
  // all the frames are there
  mIsAllFramesHere = true;

  // now make sure we have a frame each piece of content

  return mIsAllFramesHere;
}

NS_IMETHODIMP
nsListControlFrame::DoneAddingChildren(bool aIsDone) {
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
nsListControlFrame::AddOption(int32_t aIndex) {
#ifdef DO_REFLOW_DEBUG
  printf("---- Id: %d nsLCF %p Added Option %d\n", mReflowId, this, aIndex);
#endif

  if (!mIsAllContentHere) {
    mIsAllContentHere = Select().IsDoneAddingChildren();
    if (!mIsAllContentHere) {
      mIsAllFramesHere = false;
      mHasBeenInitialized = false;
    } else {
      mIsAllFramesHere =
          (aIndex == static_cast<int32_t>(GetNumberOfOptions() - 1));
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

static int32_t DecrementAndClamp(int32_t aSelectionIndex, int32_t aLength) {
  return aLength == 0 ? nsListControlFrame::kNothingSelected
                      : std::max(0, aSelectionIndex - 1);
}

NS_IMETHODIMP
nsListControlFrame::RemoveOption(int32_t aIndex) {
  MOZ_ASSERT(aIndex >= 0, "negative <option> index");

  // Need to reset if we're a dropdown
  if (mStartSelectionIndex != kNothingSelected) {
    NS_ASSERTION(mEndSelectionIndex != kNothingSelected, "");
    int32_t numOptions = GetNumberOfOptions();
    // NOTE: numOptions is the new number of options whereas aIndex is the
    // unadjusted index of the removed option (hence the <= below).
    NS_ASSERTION(aIndex <= numOptions, "out-of-bounds <option> index");

    int32_t forward = mEndSelectionIndex - mStartSelectionIndex;
    int32_t* low = forward >= 0 ? &mStartSelectionIndex : &mEndSelectionIndex;
    int32_t* high = forward >= 0 ? &mEndSelectionIndex : &mStartSelectionIndex;
    if (aIndex < *low) *low = ::DecrementAndClamp(*low, numOptions);
    if (aIndex <= *high) *high = ::DecrementAndClamp(*high, numOptions);
    if (forward == 0) *low = *high;
  } else
    NS_ASSERTION(mEndSelectionIndex == kNothingSelected, "");

  InvalidateFocus();
  return NS_OK;
}

//---------------------------------------------------------
// Set the option selected in the DOM.  This method is named
// as it is because it indicates that the frame is the source
// of this event rather than the receiver.
bool nsListControlFrame::SetOptionsSelectedFromFrame(int32_t aStartIndex,
                                                     int32_t aEndIndex,
                                                     bool aValue,
                                                     bool aClearAll) {
  using OptionFlag = HTMLSelectElement::OptionFlag;
  RefPtr<HTMLSelectElement> selectElement =
      HTMLSelectElement::FromNode(mContent);

  HTMLSelectElement::OptionFlags mask = OptionFlag::Notify;
  if (mForceSelection) {
    mask += OptionFlag::SetDisabled;
  }
  if (aValue) {
    mask += OptionFlag::IsSelected;
  }
  if (aClearAll) {
    mask += OptionFlag::ClearAll;
  }

  return selectElement->SetOptionsSelectedByIndex(aStartIndex, aEndIndex, mask);
}

bool nsListControlFrame::ToggleOptionSelectedFromFrame(int32_t aIndex) {
  RefPtr<HTMLOptionElement> option = GetOption(static_cast<uint32_t>(aIndex));
  NS_ENSURE_TRUE(option, false);

  RefPtr<HTMLSelectElement> selectElement =
      HTMLSelectElement::FromNode(mContent);

  HTMLSelectElement::OptionFlags mask = HTMLSelectElement::OptionFlag::Notify;
  if (!option->Selected()) {
    mask += HTMLSelectElement::OptionFlag::IsSelected;
  }

  return selectElement->SetOptionsSelectedByIndex(aIndex, aIndex, mask);
}

// Dispatch event and such
bool nsListControlFrame::UpdateSelection() {
  if (mIsAllFramesHere) {
    // if it's a combobox, display the new text. Note that after
    // FireOnInputAndOnChange we might be dead, as that can run script.
    AutoWeakFrame weakFrame(this);
    if (mIsAllContentHere) {
      RefPtr listener = mEventListener;
      listener->FireOnInputAndOnChange();
    }
    return weakFrame.IsAlive();
  }
  return true;
}

NS_IMETHODIMP_(void)
nsListControlFrame::OnSetSelectedIndex(int32_t aOldIndex, int32_t aNewIndex) {
#ifdef ACCESSIBILITY
  nsCOMPtr<nsIContent> prevOption = GetCurrentOption();
#endif

  AutoWeakFrame weakFrame(this);
  ScrollToIndex(aNewIndex);
  if (!weakFrame.IsAlive()) {
    return;
  }
  mStartSelectionIndex = aNewIndex;
  mEndSelectionIndex = aNewIndex;
  InvalidateFocus();

#ifdef ACCESSIBILITY
  if (aOldIndex != aNewIndex) {
    FireMenuItemActiveEvent(prevOption);
  }
#endif
}

//----------------------------------------------------------------------
// End nsISelectControlFrame
//----------------------------------------------------------------------

class AsyncReset final : public Runnable {
 public:
  AsyncReset(nsListControlFrame* aFrame, bool aScroll)
      : Runnable("AsyncReset"), mFrame(aFrame), mScroll(aScroll) {}

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    if (mFrame.IsAlive()) {
      static_cast<nsListControlFrame*>(mFrame.GetFrame())->ResetList(mScroll);
    }
    return NS_OK;
  }

 private:
  WeakFrame mFrame;
  bool mScroll;
};

nsresult nsListControlFrame::SetFormProperty(nsAtom* aName,
                                             const nsAString& aValue) {
  if (nsGkAtoms::selected == aName) {
    return NS_ERROR_INVALID_ARG;  // Selected is readonly according to spec.
  } else if (nsGkAtoms::selectedindex == aName) {
    // You shouldn't be calling me for this!!!
    return NS_ERROR_INVALID_ARG;
  }

  // We should be told about selectedIndex by the DOM element through
  // OnOptionSelected

  return NS_OK;
}

void nsListControlFrame::DidReflow(nsPresContext* aPresContext,
                                   const ReflowInput* aReflowInput) {
  bool wasInterrupted = !mHasPendingInterruptAtStartOfReflow &&
                        aPresContext->HasPendingInterrupt();

  nsHTMLScrollFrame::DidReflow(aPresContext, aReflowInput);

  if (mNeedToReset && !wasInterrupted) {
    mNeedToReset = false;
    // Suppress scrolling to the selected element if we restored scroll
    // history state AND the list contents have not changed since we loaded
    // all the children AND nothing else forced us to scroll by calling
    // ResetList(true). The latter two conditions are folded into
    // mPostChildrenLoadedReset.
    //
    // The idea is that we want scroll history restoration to trump ResetList
    // scrolling to the selected element, when the ResetList was probably only
    // caused by content loading normally.
    const bool scroll = !DidHistoryRestore() || mPostChildrenLoadedReset;
    nsContentUtils::AddScriptRunner(new AsyncReset(this, scroll));
  }

  mHasPendingInterruptAtStartOfReflow = false;
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsListControlFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"ListControl"_ns, aResult);
}
#endif

nscoord nsListControlFrame::GetBSizeOfARow() { return BSizeOfARow(); }

bool nsListControlFrame::IsOptionInteractivelySelectable(int32_t aIndex) const {
  auto& select = Select();
  if (HTMLOptionElement* item = select.Item(aIndex)) {
    return IsOptionInteractivelySelectable(&select, item);
  }
  return false;
}

bool nsListControlFrame::IsOptionInteractivelySelectable(
    HTMLSelectElement* aSelect, HTMLOptionElement* aOption) {
  return !aSelect->IsOptionDisabled(aOption) && aOption->GetPrimaryFrame();
}

nscoord nsListControlFrame::CalcFallbackRowBSize(float aFontSizeInflation) {
  RefPtr<nsFontMetrics> fontMet =
      nsLayoutUtils::GetFontMetricsForFrame(this, aFontSizeInflation);
  return fontMet->MaxHeight();
}

nscoord nsListControlFrame::CalcIntrinsicBSize(nscoord aBSizeOfARow,
                                               int32_t aNumberOfOptions) {
  mNumDisplayRows = Select().Size();
  if (mNumDisplayRows < 1) {
    mNumDisplayRows = 4;
  }
  return mNumDisplayRows * aBSizeOfARow;
}

#ifdef ACCESSIBILITY
void nsListControlFrame::FireMenuItemActiveEvent(nsIContent* aPreviousOption) {
  if (mFocused != this) {
    return;
  }

  nsIContent* optionContent = GetCurrentOption();
  if (aPreviousOption == optionContent) {
    // No change
    return;
  }

  if (aPreviousOption) {
    FireDOMEvent(u"DOMMenuItemInactive"_ns, aPreviousOption);
  }

  if (optionContent) {
    FireDOMEvent(u"DOMMenuItemActive"_ns, optionContent);
  }
}
#endif

nsresult nsListControlFrame::GetIndexFromDOMEvent(dom::Event* aMouseEvent,
                                                  int32_t& aCurIndex) {
  if (PresShell::GetCapturingContent() != mContent) {
    // If we're not capturing, then ignore movement in the border
    nsPoint pt =
        nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(aMouseEvent, this);
    nsRect borderInnerEdge = GetScrollPortRect();
    if (!borderInnerEdge.Contains(pt)) {
      return NS_ERROR_FAILURE;
    }
  }

  RefPtr<dom::HTMLOptionElement> option;
  for (nsCOMPtr<nsIContent> content =
           PresContext()->EventStateManager()->GetEventTargetContent(nullptr);
       content && !option; content = content->GetParent()) {
    option = dom::HTMLOptionElement::FromNode(content);
  }

  if (option) {
    aCurIndex = option->Index();
    MOZ_ASSERT(aCurIndex >= 0);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult nsListControlFrame::HandleLeftButtonMouseDown(
    dom::Event* aMouseEvent) {
  int32_t selectedIndex;
  if (NS_SUCCEEDED(GetIndexFromDOMEvent(aMouseEvent, selectedIndex))) {
    // Handle Like List
    CaptureMouseEvents(true);
    AutoWeakFrame weakFrame(this);
    bool change =
        HandleListSelection(aMouseEvent, selectedIndex);  // might destroy us
    if (!weakFrame.IsAlive()) {
      return NS_OK;
    }
    mChangesSinceDragStart = change;
  }
  return NS_OK;
}

nsresult nsListControlFrame::HandleLeftButtonMouseUp(dom::Event* aMouseEvent) {
  if (!StyleVisibility()->IsVisible()) {
    return NS_OK;
  }
  // Notify
  if (mChangesSinceDragStart) {
    // reset this so that future MouseUps without a prior MouseDown
    // won't fire onchange
    mChangesSinceDragStart = false;
    RefPtr listener = mEventListener;
    listener->FireOnInputAndOnChange();
    // Note that `this` may be dead now, as the above call runs script.
  }
  return NS_OK;
}

nsresult nsListControlFrame::DragMove(dom::Event* aMouseEvent) {
  NS_ASSERTION(aMouseEvent, "aMouseEvent is null.");

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
    bool wasChanged = PerformSelection(selectedIndex, !isControl, isControl);
    if (!weakFrame.IsAlive()) {
      return NS_OK;
    }
    mChangesSinceDragStart = mChangesSinceDragStart || wasChanged;
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// Scroll helpers.
//----------------------------------------------------------------------
void nsListControlFrame::ScrollToIndex(int32_t aIndex) {
  if (aIndex < 0) {
    // XXX shouldn't we just do nothing if we're asked to scroll to
    // kNothingSelected?
    ScrollTo(nsPoint(0, 0), ScrollMode::Instant);
  } else {
    RefPtr<dom::HTMLOptionElement> option =
        GetOption(AssertedCast<uint32_t>(aIndex));
    if (option) {
      ScrollToFrame(*option);
    }
  }
}

void nsListControlFrame::ScrollToFrame(dom::HTMLOptionElement& aOptElement) {
  // otherwise we find the content's frame and scroll to it
  if (nsIFrame* childFrame = aOptElement.GetPrimaryFrame()) {
    RefPtr<mozilla::PresShell> presShell = PresShell();
    presShell->ScrollFrameIntoView(childFrame, Nothing(), ScrollAxis(),
                                   ScrollAxis(),
                                   ScrollFlags::ScrollOverflowHidden |
                                       ScrollFlags::ScrollFirstAncestorOnly);
  }
}

void nsListControlFrame::UpdateSelectionAfterKeyEvent(
    int32_t aNewIndex, uint32_t aCharCode, bool aIsShift, bool aIsControlOrMeta,
    bool aIsControlSelectMode) {
  // If you hold control, but not shift, no key will actually do anything
  // except space.
  AutoWeakFrame weakFrame(this);
  bool wasChanged = false;
  if (aIsControlOrMeta && !aIsShift && aCharCode != ' ') {
#ifdef ACCESSIBILITY
    nsCOMPtr<nsIContent> prevOption = GetCurrentOption();
#endif
    mStartSelectionIndex = aNewIndex;
    mEndSelectionIndex = aNewIndex;
    InvalidateFocus();
    ScrollToIndex(aNewIndex);
    if (!weakFrame.IsAlive()) {
      return;
    }

#ifdef ACCESSIBILITY
    FireMenuItemActiveEvent(prevOption);
#endif
  } else if (aIsControlSelectMode && aCharCode == ' ') {
    wasChanged = SingleSelection(aNewIndex, true);
  } else {
    wasChanged = PerformSelection(aNewIndex, aIsShift, aIsControlOrMeta);
  }
  if (wasChanged && weakFrame.IsAlive()) {
    // dispatch event, update combobox, etc.
    UpdateSelection();
  }
}

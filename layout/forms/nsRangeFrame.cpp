/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRangeFrame.h"

#include "ListMutationObserver.h"
#include "mozilla/Assertions.h"
#include "mozilla/PresShell.h"
#include "mozilla/TouchEvents.h"

#include "gfxContext.h"
#include "nsContentCreatorFunctions.h"
#include "nsCSSRendering.h"
#include "nsDisplayList.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"
#include "mozilla/dom/Document.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/HTMLDataListElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsPresContext.h"
#include "nsNodeInfoManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/ServoStyleSet.h"
#include "nsTArray.h"

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif

// Our intrinsic size is 12em in the main-axis and 1.3em in the cross-axis.
#define MAIN_AXIS_EM_SIZE 12
#define CROSS_AXIS_EM_SIZE 1.3f

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::image;

nsIFrame* NS_NewRangeFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsRangeFrame(aStyle, aPresShell->GetPresContext());
}

nsRangeFrame::nsRangeFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID) {}

void nsRangeFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                        nsIFrame* aPrevInFlow) {
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
  if (InputElement().HasAttr(nsGkAtoms::list_)) {
    mListMutationObserver = new ListMutationObserver(*this);
  }
}

nsRangeFrame::~nsRangeFrame() = default;

NS_IMPL_FRAMEARENA_HELPERS(nsRangeFrame)

NS_QUERYFRAME_HEAD(nsRangeFrame)
  NS_QUERYFRAME_ENTRY(nsRangeFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void nsRangeFrame::Destroy(DestroyContext& aContext) {
  NS_ASSERTION(!GetPrevContinuation() && !GetNextContinuation(),
               "nsRangeFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  if (mListMutationObserver) {
    mListMutationObserver->Detach();
  }
  aContext.AddAnonymousContent(mTrackDiv.forget());
  aContext.AddAnonymousContent(mProgressDiv.forget());
  aContext.AddAnonymousContent(mThumbDiv.forget());
  nsContainerFrame::Destroy(aContext);
}

static already_AddRefed<Element> MakeAnonymousDiv(
    Document& aDoc, PseudoStyleType aOldPseudoType,
    PseudoStyleType aModernPseudoType,
    nsTArray<nsIAnonymousContentCreator::ContentInfo>& aElements) {
  RefPtr<Element> result = aDoc.CreateHTMLElement(nsGkAtoms::div);

  // Associate the pseudo-element with the anonymous child.
  if (StaticPrefs::layout_css_modern_range_pseudos_enabled()) {
    result->SetPseudoElementType(aModernPseudoType);
  } else {
    result->SetPseudoElementType(aOldPseudoType);
  }

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier, or change the return type to void.
  aElements.AppendElement(result);

  return result.forget();
}

nsresult nsRangeFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  Document* doc = mContent->OwnerDoc();
  // Create the ::-moz-range-track pseudo-element (a div):
  mTrackDiv = MakeAnonymousDiv(*doc, PseudoStyleType::mozRangeTrack,
                               PseudoStyleType::sliderTrack, aElements);
  // Create the ::-moz-range-progress pseudo-element (a div):
  mProgressDiv = MakeAnonymousDiv(*doc, PseudoStyleType::mozRangeProgress,
                                  PseudoStyleType::sliderFill, aElements);
  // Create the ::-moz-range-thumb pseudo-element (a div):
  mThumbDiv = MakeAnonymousDiv(*doc, PseudoStyleType::mozRangeThumb,
                               PseudoStyleType::sliderThumb, aElements);
  return NS_OK;
}

void nsRangeFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                            uint32_t aFilter) {
  if (mTrackDiv) {
    aElements.AppendElement(mTrackDiv);
  }

  if (mProgressDiv) {
    aElements.AppendElement(mProgressDiv);
  }

  if (mThumbDiv) {
    aElements.AppendElement(mThumbDiv);
  }
}

void nsRangeFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                    const nsDisplayListSet& aLists) {
  const nsStyleDisplay* disp = StyleDisplay();
  if (IsThemed(disp)) {
    DisplayBorderBackgroundOutline(aBuilder, aLists);
    // Only create items for the thumb. Specifically, we do not want the track
    // to paint, since *our* background is used to paint the track, and we don't
    // want the unthemed track painting over the top of the themed track.
    // This logic is copied from
    // nsContainerFrame::BuildDisplayListForNonBlockChildren as
    // called by BuildDisplayListForInline.
    if (nsIFrame* thumb = mThumbDiv->GetPrimaryFrame()) {
      nsDisplayListSet set(aLists, aLists.Content());
      BuildDisplayListForChild(aBuilder, thumb, set, DisplayChildFlag::Inline);
    }
  } else {
    BuildDisplayListForInline(aBuilder, aLists);
  }
}

void nsRangeFrame::Reflow(nsPresContext* aPresContext,
                          ReflowOutput& aDesiredSize,
                          const ReflowInput& aReflowInput,
                          nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsRangeFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  NS_ASSERTION(mTrackDiv, "::-moz-range-track div must exist!");
  NS_ASSERTION(mProgressDiv, "::-moz-range-progress div must exist!");
  NS_ASSERTION(mThumbDiv, "::-moz-range-thumb div must exist!");
  NS_ASSERTION(!GetPrevContinuation() && !GetNextContinuation(),
               "nsRangeFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  WritingMode wm = aReflowInput.GetWritingMode();
  nscoord computedBSize = aReflowInput.ComputedBSize();
  if (computedBSize == NS_UNCONSTRAINEDSIZE) {
    computedBSize = 0;
  }
  const auto borderPadding = aReflowInput.ComputedLogicalBorderPadding(wm);
  LogicalSize finalSize(
      wm, aReflowInput.ComputedISize() + borderPadding.IStartEnd(wm),
      computedBSize + borderPadding.BStartEnd(wm));
  aDesiredSize.SetSize(wm, finalSize);

  ReflowAnonymousContent(aPresContext, aDesiredSize, aReflowInput);

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  nsIFrame* trackFrame = mTrackDiv->GetPrimaryFrame();
  if (trackFrame) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, trackFrame);
  }

  nsIFrame* rangeProgressFrame = mProgressDiv->GetPrimaryFrame();
  if (rangeProgressFrame) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, rangeProgressFrame);
  }

  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();
  if (thumbFrame) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, thumbFrame);
  }

  FinishAndStoreOverflow(&aDesiredSize);

  MOZ_ASSERT(aStatus.IsEmpty(), "This type of frame can't be split.");
}

void nsRangeFrame::ReflowAnonymousContent(nsPresContext* aPresContext,
                                          ReflowOutput& aDesiredSize,
                                          const ReflowInput& aReflowInput) {
  // The width/height of our content box, which is the available width/height
  // for our anonymous content:
  nscoord rangeFrameContentBoxWidth = aReflowInput.ComputedWidth();
  nscoord rangeFrameContentBoxHeight = aReflowInput.ComputedHeight();
  if (rangeFrameContentBoxHeight == NS_UNCONSTRAINEDSIZE) {
    rangeFrameContentBoxHeight = 0;
  }

  nsIFrame* trackFrame = mTrackDiv->GetPrimaryFrame();

  if (trackFrame) {  // display:none?

    // Position the track:
    // The idea here is that we allow content authors to style the width,
    // height, border and padding of the track, but we ignore margin and
    // positioning properties and do the positioning ourself to keep the center
    // of the track's border box on the center of the nsRangeFrame's content
    // box.

    WritingMode wm = trackFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    ReflowInput trackReflowInput(aPresContext, aReflowInput, trackFrame,
                                 availSize);

    // Find the x/y position of the track frame such that it will be positioned
    // as described above. These coordinates are with respect to the
    // nsRangeFrame's border-box.
    nscoord trackX = rangeFrameContentBoxWidth / 2;
    nscoord trackY = rangeFrameContentBoxHeight / 2;

    // Account for the track's border and padding (we ignore its margin):
    trackX -= trackReflowInput.ComputedPhysicalBorderPadding().left +
              trackReflowInput.ComputedWidth() / 2;
    trackY -= trackReflowInput.ComputedPhysicalBorderPadding().top +
              trackReflowInput.ComputedHeight() / 2;

    // Make relative to our border box instead of our content box:
    trackX += aReflowInput.ComputedPhysicalBorderPadding().left;
    trackY += aReflowInput.ComputedPhysicalBorderPadding().top;

    nsReflowStatus frameStatus;
    ReflowOutput trackDesiredSize(aReflowInput);
    ReflowChild(trackFrame, aPresContext, trackDesiredSize, trackReflowInput,
                trackX, trackY, ReflowChildFlags::Default, frameStatus);
    MOZ_ASSERT(
        frameStatus.IsFullyComplete(),
        "We gave our child unconstrained height, so it should be complete");
    FinishReflowChild(trackFrame, aPresContext, trackDesiredSize,
                      &trackReflowInput, trackX, trackY,
                      ReflowChildFlags::Default);
  }

  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();

  if (thumbFrame) {  // display:none?
    WritingMode wm = thumbFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    ReflowInput thumbReflowInput(aPresContext, aReflowInput, thumbFrame,
                                 availSize);

    // Where we position the thumb depends on its size, so we first reflow
    // the thumb at {0,0} to obtain its size, then position it afterwards.

    nsReflowStatus frameStatus;
    ReflowOutput thumbDesiredSize(aReflowInput);
    ReflowChild(thumbFrame, aPresContext, thumbDesiredSize, thumbReflowInput, 0,
                0, ReflowChildFlags::Default, frameStatus);
    MOZ_ASSERT(
        frameStatus.IsFullyComplete(),
        "We gave our child unconstrained height, so it should be complete");
    FinishReflowChild(thumbFrame, aPresContext, thumbDesiredSize,
                      &thumbReflowInput, 0, 0, ReflowChildFlags::Default);
    DoUpdateThumbPosition(thumbFrame,
                          nsSize(aDesiredSize.Width(), aDesiredSize.Height()));
  }

  nsIFrame* rangeProgressFrame = mProgressDiv->GetPrimaryFrame();

  if (rangeProgressFrame) {  // display:none?
    WritingMode wm = rangeProgressFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    ReflowInput progressReflowInput(aPresContext, aReflowInput,
                                    rangeProgressFrame, availSize);

    // We first reflow the range-progress frame at {0,0} to obtain its
    // unadjusted dimensions, then we adjust it to so that the appropriate edge
    // ends at the thumb.

    nsReflowStatus frameStatus;
    ReflowOutput progressDesiredSize(aReflowInput);
    ReflowChild(rangeProgressFrame, aPresContext, progressDesiredSize,
                progressReflowInput, 0, 0, ReflowChildFlags::Default,
                frameStatus);
    MOZ_ASSERT(
        frameStatus.IsFullyComplete(),
        "We gave our child unconstrained height, so it should be complete");
    FinishReflowChild(rangeProgressFrame, aPresContext, progressDesiredSize,
                      &progressReflowInput, 0, 0, ReflowChildFlags::Default);
    DoUpdateRangeProgressFrame(
        rangeProgressFrame,
        nsSize(aDesiredSize.Width(), aDesiredSize.Height()));
  }
}

#ifdef ACCESSIBILITY
a11y::AccType nsRangeFrame::AccessibleType() { return a11y::eHTMLRangeType; }
#endif

double nsRangeFrame::GetValueAsFractionOfRange() {
  return GetDoubleAsFractionOfRange(InputElement().GetValueAsDecimal());
}

double nsRangeFrame::GetDoubleAsFractionOfRange(const Decimal& aValue) {
  auto& input = InputElement();

  Decimal minimum = input.GetMinimum();
  Decimal maximum = input.GetMaximum();

  MOZ_ASSERT(aValue.isFinite() && minimum.isFinite() && maximum.isFinite(),
             "type=range should have a default maximum/minimum");

  if (maximum <= minimum) {
    // Avoid rounding triggering the assert by checking against an epsilon.
    MOZ_ASSERT((aValue - minimum).abs().toDouble() <
                   std::numeric_limits<float>::epsilon(),
               "Unsanitized value");
    return 0.0;
  }

  MOZ_ASSERT(aValue >= minimum && aValue <= maximum, "Unsanitized value");

  return ((aValue - minimum) / (maximum - minimum)).toDouble();
}

Decimal nsRangeFrame::GetValueAtEventPoint(WidgetGUIEvent* aEvent) {
  MOZ_ASSERT(
      aEvent->mClass == eMouseEventClass || aEvent->mClass == eTouchEventClass,
      "Unexpected event type - aEvent->mRefPoint may be meaningless");

  MOZ_ASSERT(mContent->IsHTMLElement(nsGkAtoms::input), "bad cast");
  dom::HTMLInputElement* input =
      static_cast<dom::HTMLInputElement*>(GetContent());

  MOZ_ASSERT(input->ControlType() == FormControlType::InputRange);

  Decimal minimum = input->GetMinimum();
  Decimal maximum = input->GetMaximum();
  MOZ_ASSERT(minimum.isFinite() && maximum.isFinite(),
             "type=range should have a default maximum/minimum");
  if (maximum <= minimum) {
    return minimum;
  }
  Decimal range = maximum - minimum;

  LayoutDeviceIntPoint absPoint;
  if (aEvent->mClass == eTouchEventClass) {
    MOZ_ASSERT(aEvent->AsTouchEvent()->mTouches.Length() == 1,
               "Unexpected number of mTouches");
    absPoint = aEvent->AsTouchEvent()->mTouches[0]->mRefPoint;
  } else {
    absPoint = aEvent->mRefPoint;
  }
  nsPoint point = nsLayoutUtils::GetEventCoordinatesRelativeTo(
      aEvent, absPoint, RelativeTo{this});

  if (point == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)) {
    // We don't want to change the current value for this error state.
    return static_cast<dom::HTMLInputElement*>(GetContent())
        ->GetValueAsDecimal();
  }

  nsRect rangeContentRect = GetContentRectRelativeToSelf();
  nsSize thumbSize;

  if (IsThemed()) {
    // We need to get the size of the thumb from the theme.
    nsPresContext* pc = PresContext();
    LayoutDeviceIntSize size = pc->Theme()->GetMinimumWidgetSize(
        pc, this, StyleAppearance::RangeThumb);
    thumbSize =
        LayoutDeviceIntSize::ToAppUnits(size, pc->AppUnitsPerDevPixel());
    // For GTK, GetMinimumWidgetSize returns zero for the thumb dimension
    // perpendicular to the orientation of the slider.  That's okay since we
    // only care about the dimension in the direction of the slider when using
    // |thumbSize| below, but it means this assertion need to check
    // IsHorizontal().
    MOZ_ASSERT((IsHorizontal() && thumbSize.width > 0) ||
                   (!IsHorizontal() && thumbSize.height > 0),
               "The thumb is expected to take up some slider space");
  } else {
    nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();
    if (thumbFrame) {  // diplay:none?
      thumbSize = thumbFrame->GetSize();
    }
  }

  Decimal fraction;
  if (IsHorizontal()) {
    nscoord traversableDistance = rangeContentRect.width - thumbSize.width;
    if (traversableDistance <= 0) {
      return minimum;
    }
    nscoord posAtStart = rangeContentRect.x + thumbSize.width / 2;
    nscoord posAtEnd = posAtStart + traversableDistance;
    nscoord posOfPoint = mozilla::clamped(point.x, posAtStart, posAtEnd);
    fraction = Decimal(posOfPoint - posAtStart) / Decimal(traversableDistance);
    if (IsRightToLeft()) {
      fraction = Decimal(1) - fraction;
    }
  } else {
    nscoord traversableDistance = rangeContentRect.height - thumbSize.height;
    if (traversableDistance <= 0) {
      return minimum;
    }
    nscoord posAtStart = rangeContentRect.y + thumbSize.height / 2;
    nscoord posAtEnd = posAtStart + traversableDistance;
    nscoord posOfPoint = mozilla::clamped(point.y, posAtStart, posAtEnd);
    // For a vertical range, the top (posAtStart) is the highest value, so we
    // subtract the fraction from 1.0 to get that polarity correct.
    fraction = Decimal(posOfPoint - posAtStart) / Decimal(traversableDistance);
    if (IsUpwards()) {
      fraction = Decimal(1) - fraction;
    }
  }

  MOZ_ASSERT(fraction >= Decimal(0) && fraction <= Decimal(1));
  return minimum + fraction * range;
}

void nsRangeFrame::UpdateForValueChange() {
  if (IsSubtreeDirty()) {
    return;  // we're going to be updated when we reflow
  }
  nsIFrame* rangeProgressFrame = mProgressDiv->GetPrimaryFrame();
  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();
  if (!rangeProgressFrame && !thumbFrame) {
    return;  // diplay:none?
  }
  if (rangeProgressFrame) {
    DoUpdateRangeProgressFrame(rangeProgressFrame, GetSize());
  }
  if (thumbFrame) {
    DoUpdateThumbPosition(thumbFrame, GetSize());
  }
  if (IsThemed()) {
    // We don't know the exact dimensions or location of the thumb when native
    // theming is applied, so we just repaint the entire range.
    InvalidateFrame();
  }

#ifdef ACCESSIBILITY
  if (nsAccessibilityService* accService = GetAccService()) {
    accService->RangeValueChanged(PresShell(), mContent);
  }
#endif

  SchedulePaint();
}

nsTArray<Decimal> nsRangeFrame::TickMarks() {
  nsTArray<Decimal> tickMarks;
  auto& input = InputElement();
  auto* list = input.GetList();
  if (!list) {
    return tickMarks;
  }
  auto min = input.GetMinimum();
  auto max = input.GetMaximum();
  auto* options = list->Options();
  nsAutoString label;
  for (uint32_t i = 0; i < options->Length(); ++i) {
    auto* item = options->Item(i);
    auto* option = HTMLOptionElement::FromNode(item);
    MOZ_ASSERT(option);
    if (option->Disabled()) {
      continue;
    }
    nsAutoString str;
    option->GetValue(str);
    auto tickMark = HTMLInputElement::StringToDecimal(str);
    if (tickMark.isNaN() || tickMark < min || tickMark > max ||
        input.ValueIsStepMismatch(tickMark)) {
      continue;
    }
    tickMarks.AppendElement(tickMark);
  }
  tickMarks.Sort();
  return tickMarks;
}

Decimal nsRangeFrame::NearestTickMark(const Decimal& aValue) {
  auto tickMarks = TickMarks();
  if (tickMarks.IsEmpty() || aValue.isNaN()) {
    return Decimal::nan();
  }
  size_t index;
  if (BinarySearch(tickMarks, 0, tickMarks.Length(), aValue, &index)) {
    return tickMarks[index];
  }
  if (index == tickMarks.Length()) {
    return tickMarks.LastElement();
  }
  if (index == 0) {
    return tickMarks[0];
  }
  const auto& smallerTickMark = tickMarks[index - 1];
  const auto& largerTickMark = tickMarks[index];
  MOZ_ASSERT(smallerTickMark < aValue);
  MOZ_ASSERT(largerTickMark > aValue);
  return (aValue - smallerTickMark).abs() < (aValue - largerTickMark).abs()
             ? smallerTickMark
             : largerTickMark;
}

mozilla::dom::HTMLInputElement& nsRangeFrame::InputElement() const {
  MOZ_ASSERT(mContent->IsHTMLElement(nsGkAtoms::input), "bad cast");
  auto& input = *static_cast<dom::HTMLInputElement*>(GetContent());
  MOZ_ASSERT(input.ControlType() == FormControlType::InputRange);
  return input;
}

void nsRangeFrame::DoUpdateThumbPosition(nsIFrame* aThumbFrame,
                                         const nsSize& aRangeSize) {
  MOZ_ASSERT(aThumbFrame);

  // The idea here is that we want to position the thumb so that the center
  // of the thumb is on an imaginary line drawn from the middle of one edge
  // of the range frame's content box to the middle of the opposite edge of
  // its content box (the opposite edges being the left/right edge if the
  // range is horizontal, or else the top/bottom edges if the range is
  // vertical). How far along this line the center of the thumb is placed
  // depends on the value of the range.

  nsMargin borderAndPadding = GetUsedBorderAndPadding();
  nsPoint newPosition(borderAndPadding.left, borderAndPadding.top);

  nsSize rangeContentBoxSize(aRangeSize);
  rangeContentBoxSize.width -= borderAndPadding.LeftRight();
  rangeContentBoxSize.height -= borderAndPadding.TopBottom();

  nsSize thumbSize = aThumbFrame->GetSize();
  double fraction = GetValueAsFractionOfRange();
  MOZ_ASSERT(fraction >= 0.0 && fraction <= 1.0);

  if (IsHorizontal()) {
    if (thumbSize.width < rangeContentBoxSize.width) {
      nscoord traversableDistance = rangeContentBoxSize.width - thumbSize.width;
      if (IsRightToLeft()) {
        newPosition.x += NSToCoordRound((1.0 - fraction) * traversableDistance);
      } else {
        newPosition.x += NSToCoordRound(fraction * traversableDistance);
      }
      newPosition.y += (rangeContentBoxSize.height - thumbSize.height) / 2;
    }
  } else {
    if (thumbSize.height < rangeContentBoxSize.height) {
      nscoord traversableDistance =
          rangeContentBoxSize.height - thumbSize.height;
      newPosition.x += (rangeContentBoxSize.width - thumbSize.width) / 2;
      if (IsUpwards()) {
        newPosition.y += NSToCoordRound((1.0 - fraction) * traversableDistance);
      } else {
        newPosition.y += NSToCoordRound(fraction * traversableDistance);
      }
    }
  }
  aThumbFrame->SetPosition(newPosition);
}

void nsRangeFrame::DoUpdateRangeProgressFrame(nsIFrame* aRangeProgressFrame,
                                              const nsSize& aRangeSize) {
  MOZ_ASSERT(aRangeProgressFrame);

  // The idea here is that we want to position the ::-moz-range-progress
  // pseudo-element so that the center line running along its length is on the
  // corresponding center line of the nsRangeFrame's content box. In the other
  // dimension, we align the "start" edge of the ::-moz-range-progress
  // pseudo-element's border-box with the corresponding edge of the
  // nsRangeFrame's content box, and we size the progress element's border-box
  // to have a length of GetValueAsFractionOfRange() times the nsRangeFrame's
  // content-box size.

  nsMargin borderAndPadding = GetUsedBorderAndPadding();
  nsSize progSize = aRangeProgressFrame->GetSize();
  nsRect progRect(borderAndPadding.left, borderAndPadding.top, progSize.width,
                  progSize.height);

  nsSize rangeContentBoxSize(aRangeSize);
  rangeContentBoxSize.width -= borderAndPadding.LeftRight();
  rangeContentBoxSize.height -= borderAndPadding.TopBottom();

  double fraction = GetValueAsFractionOfRange();
  MOZ_ASSERT(fraction >= 0.0 && fraction <= 1.0);

  if (IsHorizontal()) {
    nscoord progLength = NSToCoordRound(fraction * rangeContentBoxSize.width);
    if (IsRightToLeft()) {
      progRect.x += rangeContentBoxSize.width - progLength;
    }
    progRect.y += (rangeContentBoxSize.height - progSize.height) / 2;
    progRect.width = progLength;
  } else {
    nscoord progLength = NSToCoordRound(fraction * rangeContentBoxSize.height);
    progRect.x += (rangeContentBoxSize.width - progSize.width) / 2;
    if (IsUpwards()) {
      progRect.y += rangeContentBoxSize.height - progLength;
    }
    progRect.height = progLength;
  }
  aRangeProgressFrame->SetRect(progRect);
}

nsresult nsRangeFrame::AttributeChanged(int32_t aNameSpaceID,
                                        nsAtom* aAttribute, int32_t aModType) {
  NS_ASSERTION(mTrackDiv, "The track div must exist!");
  NS_ASSERTION(mThumbDiv, "The thumb div must exist!");

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::min ||
        aAttribute == nsGkAtoms::max || aAttribute == nsGkAtoms::step) {
      // We want to update the position of the thumb, except in one special
      // case: If the value attribute is being set, it is possible that we are
      // in the middle of a type change away from type=range, under the
      // SetAttr(..., nsGkAtoms::value, ...) call in HTMLInputElement::
      // HandleTypeChange. In that case the HTMLInputElement's type will
      // already have changed, and if we call UpdateForValueChange()
      // we'll fail the asserts under that call that check the type of our
      // HTMLInputElement. Given that we're changing away from being a range
      // and this frame will shortly be destroyed, there's no point in calling
      // UpdateForValueChange() anyway.
      MOZ_ASSERT(mContent->IsHTMLElement(nsGkAtoms::input), "bad cast");
      bool typeIsRange =
          static_cast<dom::HTMLInputElement*>(GetContent())->ControlType() ==
          FormControlType::InputRange;
      // If script changed the <input>'s type before setting these attributes
      // then we don't need to do anything since we are going to be reframed.
      if (typeIsRange) {
        UpdateForValueChange();
      }
    } else if (aAttribute == nsGkAtoms::orient) {
      PresShell()->FrameNeedsReflow(this, IntrinsicDirty::None,
                                    NS_FRAME_IS_DIRTY);
    } else if (aAttribute == nsGkAtoms::list_) {
      const bool isRemoval = aModType == MutationEvent_Binding::REMOVAL;
      if (mListMutationObserver) {
        mListMutationObserver->Detach();
        if (isRemoval) {
          mListMutationObserver = nullptr;
        } else {
          mListMutationObserver->Attach();
        }
      } else if (!isRemoval) {
        mListMutationObserver = new ListMutationObserver(*this, true);
      }
    }
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nscoord nsRangeFrame::AutoCrossSize(Length aEm) {
  nscoord minCrossSize(0);
  if (IsThemed()) {
    nsPresContext* pc = PresContext();
    LayoutDeviceIntSize size = pc->Theme()->GetMinimumWidgetSize(
        pc, this, StyleAppearance::RangeThumb);
    minCrossSize =
        pc->DevPixelsToAppUnits(IsHorizontal() ? size.height : size.width);
  }
  return std::max(minCrossSize, aEm.ScaledBy(CROSS_AXIS_EM_SIZE).ToAppUnits());
}

static mozilla::Length OneEm(nsRangeFrame* aFrame) {
  return aFrame->StyleFont()->mFont.size.ScaledBy(
      nsLayoutUtils::FontSizeInflationFor(aFrame));
}

LogicalSize nsRangeFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  bool isInlineOriented = IsInlineOriented();
  auto em = OneEm(this);

  const WritingMode wm = GetWritingMode();
  LogicalSize autoSize(wm);
  if (isInlineOriented) {
    autoSize.ISize(wm) = em.ScaledBy(MAIN_AXIS_EM_SIZE).ToAppUnits();
    autoSize.BSize(wm) = AutoCrossSize(em);
  } else {
    autoSize.ISize(wm) = AutoCrossSize(em);
    autoSize.BSize(wm) = em.ScaledBy(MAIN_AXIS_EM_SIZE).ToAppUnits();
  }

  return autoSize.ConvertTo(aWM, wm);
}

nscoord nsRangeFrame::GetMinISize(gfxContext* aRenderingContext) {
  const auto* pos = StylePosition();
  auto wm = GetWritingMode();
  if (pos->ISize(wm).HasPercent()) {
    // https://drafts.csswg.org/css-sizing-3/#percentage-sizing
    // https://drafts.csswg.org/css-sizing-3/#min-content-zero
    return nsLayoutUtils::ResolveToLength<true>(
        pos->ISize(wm).AsLengthPercentage(), nscoord(0));
  }
  return GetPrefISize(aRenderingContext);
}

nscoord nsRangeFrame::GetPrefISize(gfxContext* aRenderingContext) {
  auto em = OneEm(this);
  if (IsInlineOriented()) {
    return em.ScaledBy(MAIN_AXIS_EM_SIZE).ToAppUnits();
  }
  return AutoCrossSize(em);
}

bool nsRangeFrame::IsHorizontal() const {
  dom::HTMLInputElement* element =
      static_cast<dom::HTMLInputElement*>(GetContent());
  return element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::orient,
                              nsGkAtoms::horizontal, eCaseMatters) ||
         (!element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::orient,
                                nsGkAtoms::vertical, eCaseMatters) &&
          GetWritingMode().IsVertical() ==
              element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::orient,
                                   nsGkAtoms::block, eCaseMatters));
}

double nsRangeFrame::GetMin() const {
  return static_cast<dom::HTMLInputElement*>(GetContent())
      ->GetMinimum()
      .toDouble();
}

double nsRangeFrame::GetMax() const {
  return static_cast<dom::HTMLInputElement*>(GetContent())
      ->GetMaximum()
      .toDouble();
}

double nsRangeFrame::GetValue() const {
  return static_cast<dom::HTMLInputElement*>(GetContent())
      ->GetValueAsDecimal()
      .toDouble();
}

bool nsRangeFrame::ShouldUseNativeStyle() const {
  nsIFrame* trackFrame = mTrackDiv->GetPrimaryFrame();
  nsIFrame* progressFrame = mProgressDiv->GetPrimaryFrame();
  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();

  return StyleDisplay()->EffectiveAppearance() == StyleAppearance::Range &&
         trackFrame &&
         !trackFrame->Style()->HasAuthorSpecifiedBorderOrBackground() &&
         progressFrame &&
         !progressFrame->Style()->HasAuthorSpecifiedBorderOrBackground() &&
         thumbFrame &&
         !thumbFrame->Style()->HasAuthorSpecifiedBorderOrBackground();
}

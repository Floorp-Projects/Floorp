/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRangeFrame.h"

#include "mozilla/EventStates.h"
#include "mozilla/PresShell.h"
#include "mozilla/TouchEvents.h"

#include "gfxContext.h"
#include "nsContentCreatorFunctions.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRendering.h"
#include "nsCheckboxRadioFrame.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "nsPresContext.h"
#include "nsPresContextInlines.h"
#include "nsNodeInfoManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/ServoStyleSet.h"
#include "nsStyleConsts.h"

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif

// Our intrinsic size is 12em in the main-axis and 1.3em in the cross-axis.
#define MAIN_AXIS_EM_SIZE 12
#define CROSS_AXIS_EM_SIZE 1.3f

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::image;

NS_IMPL_ISUPPORTS(nsRangeFrame::DummyTouchListener, nsIDOMEventListener)

nsIFrame* NS_NewRangeFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsRangeFrame(aStyle, aPresShell->GetPresContext());
}

nsRangeFrame::nsRangeFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID) {}

nsRangeFrame::~nsRangeFrame() = default;

NS_IMPL_FRAMEARENA_HELPERS(nsRangeFrame)

NS_QUERYFRAME_HEAD(nsRangeFrame)
  NS_QUERYFRAME_ENTRY(nsRangeFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void nsRangeFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                        nsIFrame* aPrevInFlow) {
  // With APZ enabled, touch events may be handled directly by the APZC code
  // if the APZ knows that there is no content interested in the touch event.
  // The range input element *is* interested in touch events, but doesn't use
  // the usual mechanism (i.e. registering an event listener) to handle touch
  // input. Instead, we do it here so that the APZ finds out about it, and
  // makes sure to wait for content to run handlers before handling the touch
  // input itself.
  if (!mDummyTouchListener) {
    mDummyTouchListener = new DummyTouchListener();
  }
  aContent->AddEventListener(NS_LITERAL_STRING("touchstart"),
                             mDummyTouchListener, false);

  ServoStyleSet* styleSet = PresContext()->StyleSet();

  mOuterFocusStyle = styleSet->ProbePseudoElementStyle(
      *aContent->AsElement(), PseudoStyleType::mozFocusOuter, Style());

  return nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

void nsRangeFrame::DestroyFrom(nsIFrame* aDestructRoot,
                               PostDestroyData& aPostDestroyData) {
  NS_ASSERTION(!GetPrevContinuation() && !GetNextContinuation(),
               "nsRangeFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  mContent->RemoveEventListener(NS_LITERAL_STRING("touchstart"),
                                mDummyTouchListener, false);

  nsCheckboxRadioFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  aPostDestroyData.AddAnonymousContent(mTrackDiv.forget());
  aPostDestroyData.AddAnonymousContent(mProgressDiv.forget());
  aPostDestroyData.AddAnonymousContent(mThumbDiv.forget());
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsresult nsRangeFrame::MakeAnonymousDiv(Element** aResult,
                                        PseudoStyleType aPseudoType,
                                        nsTArray<ContentInfo>& aElements) {
  nsCOMPtr<Document> doc = mContent->GetComposedDoc();
  RefPtr<Element> resultElement = doc->CreateHTMLElement(nsGkAtoms::div);

  // Associate the pseudo-element with the anonymous child.
  resultElement->SetPseudoElementType(aPseudoType);

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier, or change the return type to void.
  aElements.AppendElement(resultElement);

  resultElement.forget(aResult);
  return NS_OK;
}

nsresult nsRangeFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  nsresult rv;

  // Create the ::-moz-range-track pseudo-element (a div):
  rv = MakeAnonymousDiv(getter_AddRefs(mTrackDiv),
                        PseudoStyleType::mozRangeTrack, aElements);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the ::-moz-range-progress pseudo-element (a div):
  rv = MakeAnonymousDiv(getter_AddRefs(mProgressDiv),
                        PseudoStyleType::mozRangeProgress, aElements);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the ::-moz-range-thumb pseudo-element (a div):
  rv = MakeAnonymousDiv(getter_AddRefs(mThumbDiv),
                        PseudoStyleType::mozRangeThumb, aElements);
  return rv;
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

class nsDisplayRangeFocusRing final : public nsPaintedDisplayItem {
 public:
  nsDisplayRangeFocusRing(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayRangeFocusRing);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayRangeFocusRing)

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override;
  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("RangeFocusRing", TYPE_RANGE_FOCUS_RING)
};

nsDisplayItemGeometry* nsDisplayRangeFocusRing::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void nsDisplayRangeFocusRing::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto geometry =
      static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

nsRect nsDisplayRangeFocusRing::GetBounds(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap) const {
  *aSnap = false;
  nsRect rect(ToReferenceFrame(), Frame()->GetSize());

  // We want to paint as if specifying a border for ::-moz-focus-outer
  // specifies an outline for our frame, so inflate by the border widths:
  ComputedStyle* computedStyle =
      static_cast<nsRangeFrame*>(mFrame)->mOuterFocusStyle;
  MOZ_ASSERT(computedStyle, "We only exist if mOuterFocusStyle is non-null");
  rect.Inflate(computedStyle->StyleBorder()->GetComputedBorder());

  return rect;
}

void nsDisplayRangeFocusRing::Paint(nsDisplayListBuilder* aBuilder,
                                    gfxContext* aCtx) {
  bool unused;
  ComputedStyle* computedStyle =
      static_cast<nsRangeFrame*>(mFrame)->mOuterFocusStyle;
  MOZ_ASSERT(computedStyle, "We only exist if mOuterFocusStyle is non-null");

  PaintBorderFlags flags = aBuilder->ShouldSyncDecodeImages()
                               ? PaintBorderFlags::SyncDecodeImages
                               : PaintBorderFlags();

  ImgDrawResult result = nsCSSRendering::PaintBorder(
      mFrame->PresContext(), *aCtx, mFrame, GetPaintRect(),
      GetBounds(aBuilder, &unused), computedStyle, flags);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

void nsRangeFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                    const nsDisplayListSet& aLists) {
  const nsStyleDisplay* disp = StyleDisplay();
  if (IsThemed(disp)) {
    DisplayBorderBackgroundOutline(aBuilder, aLists);
    // Only create items for the thumb. Specifically, we do not want
    // the track to paint, since *our* background is used to paint
    // the track, and we don't want the unthemed track painting over
    // the top of the themed track.
    // This logic is copied from
    // nsContainerFrame::BuildDisplayListForNonBlockChildren as
    // called by BuildDisplayListForInline.
    nsIFrame* thumb = mThumbDiv->GetPrimaryFrame();
    if (thumb) {
      nsDisplayListSet set(aLists, aLists.Content());
      BuildDisplayListForChild(aBuilder, thumb, set, DISPLAY_CHILD_INLINE);
    }
  } else {
    BuildDisplayListForInline(aBuilder, aLists);
  }

  // Draw a focus outline if appropriate:

  if (!aBuilder->IsForPainting() || !IsVisibleForPainting()) {
    // we don't want the focus ring item for hit-testing or if the item isn't
    // in the area being [re]painted
    return;
  }

  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED) ||
      !eventStates.HasState(NS_EVENT_STATE_FOCUSRING)) {
    return;  // can't have focus or doesn't match :-moz-focusring
  }

  if (!mOuterFocusStyle || !mOuterFocusStyle->StyleBorder()->HasBorder()) {
    // no ::-moz-focus-outer specified border (how style specifies a focus ring
    // for range)
    return;
  }

  if (IsThemed(disp) &&
      PresContext()->Theme()->ThemeDrawsFocusForWidget(disp->mAppearance)) {
    return;  // the native theme displays its own visual indication of focus
  }

  aLists.Content()->AppendNewToTop<nsDisplayRangeFocusRing>(aBuilder, this);
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

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsCheckboxRadioFrame::RegUnRegAccessKey(this, true);
  }

  WritingMode wm = aReflowInput.GetWritingMode();
  nscoord computedBSize = aReflowInput.ComputedBSize();
  if (computedBSize == NS_UNCONSTRAINEDSIZE) {
    computedBSize = 0;
  }
  LogicalSize finalSize(
      wm,
      aReflowInput.ComputedISize() +
          aReflowInput.ComputedLogicalBorderPadding().IStartEnd(wm),
      computedBSize +
          aReflowInput.ComputedLogicalBorderPadding().BStartEnd(wm));
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

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
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
  MOZ_ASSERT(mContent->IsHTMLElement(nsGkAtoms::input), "bad cast");
  dom::HTMLInputElement* input =
      static_cast<dom::HTMLInputElement*>(GetContent());

  MOZ_ASSERT(input->ControlType() == NS_FORM_INPUT_RANGE);

  Decimal value = input->GetValueAsDecimal();
  Decimal minimum = input->GetMinimum();
  Decimal maximum = input->GetMaximum();

  MOZ_ASSERT(value.isFinite() && minimum.isFinite() && maximum.isFinite(),
             "type=range should have a default maximum/minimum");

  if (maximum <= minimum) {
    // Avoid rounding triggering the assert by checking against an epsilon.
    MOZ_ASSERT((value - minimum).abs().toDouble() <
                   std::numeric_limits<float>::epsilon(),
               "Unsanitized value");
    return 0.0;
  }

  MOZ_ASSERT(value >= minimum && value <= maximum, "Unsanitized value");

  return ((value - minimum) / (maximum - minimum)).toDouble();
}

Decimal nsRangeFrame::GetValueAtEventPoint(WidgetGUIEvent* aEvent) {
  MOZ_ASSERT(
      aEvent->mClass == eMouseEventClass || aEvent->mClass == eTouchEventClass,
      "Unexpected event type - aEvent->mRefPoint may be meaningless");

  MOZ_ASSERT(mContent->IsHTMLElement(nsGkAtoms::input), "bad cast");
  dom::HTMLInputElement* input =
      static_cast<dom::HTMLInputElement*>(GetContent());

  MOZ_ASSERT(input->ControlType() == NS_FORM_INPUT_RANGE);

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
  nsPoint point =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, absPoint, this);

  if (point == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)) {
    // We don't want to change the current value for this error state.
    return static_cast<dom::HTMLInputElement*>(GetContent())
        ->GetValueAsDecimal();
  }

  nsRect rangeContentRect = GetContentRectRelativeToSelf();
  nsSize thumbSize;

  if (IsThemed()) {
    // We need to get the size of the thumb from the theme.
    nsPresContext* presContext = PresContext();
    bool notUsedCanOverride;
    LayoutDeviceIntSize size;
    presContext->Theme()->GetMinimumWidgetSize(presContext, this,
                                               StyleAppearance::RangeThumb,
                                               &size, &notUsedCanOverride);
    thumbSize.width = presContext->DevPixelsToAppUnits(size.width);
    thumbSize.height = presContext->DevPixelsToAppUnits(size.height);
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
    fraction = Decimal(1) -
               Decimal(posOfPoint - posAtStart) / Decimal(traversableDistance);
  }

  MOZ_ASSERT(fraction >= Decimal(0) && fraction <= Decimal(1));
  return minimum + fraction * range;
}

void nsRangeFrame::UpdateForValueChange() {
  if (NS_SUBTREE_DIRTY(this)) {
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
  if (nsAccessibilityService* accService =
          PresShell::GetAccessibilityService()) {
    accService->RangeValueChanged(PresShell(), mContent);
  }
#endif

  SchedulePaint();
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
      newPosition.y += NSToCoordRound((1.0 - fraction) * traversableDistance);
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
    progRect.y += rangeContentBoxSize.height - progLength;
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
          NS_FORM_INPUT_RANGE;
      // If script changed the <input>'s type before setting these attributes
      // then we don't need to do anything since we are going to be reframed.
      if (typeIsRange) {
        UpdateForValueChange();
      }
    } else if (aAttribute == nsGkAtoms::orient) {
      PresShell()->FrameNeedsReflow(this, IntrinsicDirty::Resize,
                                    NS_FRAME_IS_DIRTY);
    }
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nscoord nsRangeFrame::AutoCrossSize(nscoord aEm) {
  nscoord minCrossSize(0);
  if (IsThemed()) {
    bool unused;
    LayoutDeviceIntSize size;
    nsPresContext* pc = PresContext();
    pc->Theme()->GetMinimumWidgetSize(pc, this, StyleAppearance::RangeThumb,
                                      &size, &unused);
    minCrossSize =
        pc->DevPixelsToAppUnits(IsHorizontal() ? size.height : size.width);
  }
  return std::max(minCrossSize, NSToCoordRound(CROSS_AXIS_EM_SIZE * aEm));
}

LogicalSize nsRangeFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorder, const LogicalSize& aPadding,
    ComputeSizeFlags aFlags) {
  bool isInlineOriented = IsInlineOriented();
  auto em = StyleFont()->mFont.size * nsLayoutUtils::FontSizeInflationFor(this);

  const WritingMode wm = GetWritingMode();
  LogicalSize autoSize(wm);
  if (isInlineOriented) {
    autoSize.ISize(wm) = NSToCoordRound(MAIN_AXIS_EM_SIZE * em);
    autoSize.BSize(wm) = AutoCrossSize(em);
  } else {
    autoSize.ISize(wm) = AutoCrossSize(em);
    autoSize.BSize(wm) = NSToCoordRound(MAIN_AXIS_EM_SIZE * em);
  }

  return autoSize.ConvertTo(aWM, wm);
}

nscoord nsRangeFrame::GetMinISize(gfxContext* aRenderingContext) {
  auto pos = StylePosition();
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
  bool isInline = IsInlineOriented();
  auto em = StyleFont()->mFont.size * nsLayoutUtils::FontSizeInflationFor(this);
  return isInline ? NSToCoordRound(em * MAIN_AXIS_EM_SIZE) : AutoCrossSize(em);
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

#define STYLES_DISABLING_NATIVE_THEMING \
  NS_AUTHOR_SPECIFIED_BORDER_OR_BACKGROUND | NS_AUTHOR_SPECIFIED_PADDING

bool nsRangeFrame::ShouldUseNativeStyle() const {
  nsIFrame* trackFrame = mTrackDiv->GetPrimaryFrame();
  nsIFrame* progressFrame = mProgressDiv->GetPrimaryFrame();
  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();

  return StyleDisplay()->mAppearance == StyleAppearance::Range && trackFrame &&
         !PresContext()->HasAuthorSpecifiedRules(
             trackFrame, STYLES_DISABLING_NATIVE_THEMING) &&
         progressFrame &&
         !PresContext()->HasAuthorSpecifiedRules(
             progressFrame, STYLES_DISABLING_NATIVE_THEMING) &&
         thumbFrame &&
         !PresContext()->HasAuthorSpecifiedRules(
             thumbFrame, STYLES_DISABLING_NATIVE_THEMING);
}

ComputedStyle* nsRangeFrame::GetAdditionalComputedStyle(int32_t aIndex) const {
  // We only implement this so that SetAdditionalComputedStyle will be
  // called if style changes that would change the -moz-focus-outer
  // pseudo-element have occurred.
  if (aIndex != 0) {
    return nullptr;
  }
  return mOuterFocusStyle;
}

void nsRangeFrame::SetAdditionalComputedStyle(int32_t aIndex,
                                              ComputedStyle* aComputedStyle) {
  MOZ_ASSERT(aIndex == 0,
             "GetAdditionalComputedStyle is handling other indexes?");

  // The -moz-focus-outer pseudo-element's style has changed.
  mOuterFocusStyle = aComputedStyle;
}

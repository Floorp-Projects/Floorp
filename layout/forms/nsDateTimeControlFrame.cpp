/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This frame type is used for input type=date, time, month, week, and
 * datetime-local.
 */

#include "nsDateTimeControlFrame.h"

#include "nsContentUtils.h"
#include "nsCheckboxRadioFrame.h"
#include "nsGkAtoms.h"
#include "nsContentCreatorFunctions.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsNodeInfoManager.h"
#include "nsIObserverService.h"
#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame* NS_NewDateTimeControlFrame(PresShell* aPresShell,
                                     ComputedStyle* aStyle) {
  return new (aPresShell)
      nsDateTimeControlFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsDateTimeControlFrame)

NS_QUERYFRAME_HEAD(nsDateTimeControlFrame)
  NS_QUERYFRAME_ENTRY(nsDateTimeControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsDateTimeControlFrame::nsDateTimeControlFrame(ComputedStyle* aStyle,
                                               nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID) {}

nscoord nsDateTimeControlFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  if (kid) {  // display:none?
    result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, kid,
                                                  nsLayoutUtils::MIN_ISIZE);
  } else {
    result = 0;
  }

  return result;
}

nscoord nsDateTimeControlFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  if (kid) {  // display:none?
    result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, kid,
                                                  nsLayoutUtils::PREF_ISIZE);
  } else {
    result = 0;
  }

  return result;
}

void nsDateTimeControlFrame::Reflow(nsPresContext* aPresContext,
                                    ReflowOutput& aDesiredSize,
                                    const ReflowInput& aReflowInput,
                                    nsReflowStatus& aStatus) {
  MarkInReflow();

  DO_GLOBAL_REFLOW_COUNT("nsDateTimeControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE(
      NS_FRAME_TRACE_CALLS,
      ("enter nsDateTimeControlFrame::Reflow: availSize=%d,%d",
       aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  NS_ASSERTION(mFrames.GetLength() <= 1,
               "There should be no more than 1 frames");

  const WritingMode myWM = aReflowInput.GetWritingMode();

  // The ISize of our content box, which is the available ISize
  // for our anonymous content:
  const nscoord contentBoxISize = aReflowInput.ComputedISize();
  nscoord contentBoxBSize = aReflowInput.ComputedBSize();

  // Figure out our border-box sizes as well (by adding borderPadding to
  // content-box sizes):
  const nscoord borderBoxISize =
      contentBoxISize +
      aReflowInput.ComputedLogicalBorderPadding().IStartEnd(myWM);

  nscoord borderBoxBSize;
  if (contentBoxBSize != NS_INTRINSICSIZE) {
    borderBoxBSize =
        contentBoxBSize +
        aReflowInput.ComputedLogicalBorderPadding().BStartEnd(myWM);
  }  // else, we'll figure out borderBoxBSize after we resolve contentBoxBSize.

  nsIFrame* inputAreaFrame = mFrames.FirstChild();
  if (!inputAreaFrame) {  // display:none?
    if (contentBoxBSize == NS_INTRINSICSIZE) {
      contentBoxBSize = 0;
      borderBoxBSize =
          aReflowInput.ComputedLogicalBorderPadding().BStartEnd(myWM);
    }
  } else {
    ReflowOutput childDesiredSize(aReflowInput);

    WritingMode wm = inputAreaFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

    ReflowInput childReflowOuput(aPresContext, aReflowInput, inputAreaFrame,
                                 availSize);

    // Convert input area margin into my own writing-mode (in case it differs):
    LogicalMargin childMargin =
        childReflowOuput.ComputedLogicalMargin().ConvertTo(myWM, wm);

    // offsets of input area frame within this frame:
    LogicalPoint childOffset(
        myWM,
        aReflowInput.ComputedLogicalBorderPadding().IStart(myWM) +
            childMargin.IStart(myWM),
        aReflowInput.ComputedLogicalBorderPadding().BStart(myWM) +
            childMargin.BStart(myWM));

    nsReflowStatus childStatus;
    // We initially reflow the child with a dummy containerSize; positioning
    // will be fixed later.
    const nsSize dummyContainerSize;
    ReflowChild(inputAreaFrame, aPresContext, childDesiredSize,
                childReflowOuput, myWM, childOffset, dummyContainerSize, 0,
                childStatus);
    MOZ_ASSERT(childStatus.IsFullyComplete(),
               "We gave our child unconstrained available block-size, "
               "so it should be complete");

    nscoord childMarginBoxBSize =
        childDesiredSize.BSize(myWM) + childMargin.BStartEnd(myWM);

    if (contentBoxBSize == NS_INTRINSICSIZE) {
      // We are intrinsically sized -- we should shrinkwrap the input area's
      // block-size:
      contentBoxBSize = childMarginBoxBSize;

      // Make sure we obey min/max-bsize in the case when we're doing intrinsic
      // sizing (we get it for free when we have a non-intrinsic
      // aReflowInput.ComputedBSize()).  Note that we do this before
      // adjusting for borderpadding, since ComputedMaxBSize and
      // ComputedMinBSize are content heights.
      contentBoxBSize =
          NS_CSS_MINMAX(contentBoxBSize, aReflowInput.ComputedMinBSize(),
                        aReflowInput.ComputedMaxBSize());

      borderBoxBSize =
          contentBoxBSize +
          aReflowInput.ComputedLogicalBorderPadding().BStartEnd(myWM);
    }

    // Center child in block axis
    nscoord extraSpace = contentBoxBSize - childMarginBoxBSize;
    childOffset.B(myWM) += std::max(0, extraSpace / 2);

    // Needed in FinishReflowChild, for logical-to-physical conversion:
    nsSize borderBoxSize =
        LogicalSize(myWM, borderBoxISize, borderBoxBSize).GetPhysicalSize(myWM);

    // Place the child
    FinishReflowChild(inputAreaFrame, aPresContext, childDesiredSize,
                      &childReflowOuput, myWM, childOffset, borderBoxSize, 0);

    nsSize contentBoxSize = LogicalSize(myWM, contentBoxISize, contentBoxBSize)
                                .GetPhysicalSize(myWM);
    aDesiredSize.SetBlockStartAscent(
        childDesiredSize.BlockStartAscent() +
        inputAreaFrame->BStart(aReflowInput.GetWritingMode(), contentBoxSize));
  }

  LogicalSize logicalDesiredSize(myWM, borderBoxISize, borderBoxBSize);
  aDesiredSize.SetSize(myWM, logicalDesiredSize);

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  if (inputAreaFrame) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, inputAreaFrame);
  }

  FinishAndStoreOverflow(&aDesiredSize);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsDateTimeControlFrame::Reflow: size=%d,%d",
                  aDesiredSize.Width(), aDesiredSize.Height()));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void nsDateTimeControlFrame::SyncDisabledState() {
  Element* dateTimeBoxElement =
      static_cast<dom::HTMLInputElement*>(GetContent())
          ->GetDateTimeBoxElement();
  if (!dateTimeBoxElement) {
    return;
  }

  AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
      dateTimeBoxElement, NS_LITERAL_STRING("MozDateTimeAttributeChanged"),
      CanBubble::eNo, ChromeOnlyDispatch::eNo);
  dispatcher->RunDOMEventWhenSafe();
}

nsresult nsDateTimeControlFrame::AttributeChanged(int32_t aNameSpaceID,
                                                  nsAtom* aAttribute,
                                                  int32_t aModType) {
  // nsGkAtoms::disabled is handled by SyncDisabledState
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::readonly ||
        aAttribute == nsGkAtoms::tabindex) {
      MOZ_ASSERT(mContent->IsHTMLElement(nsGkAtoms::input), "bad cast");
      auto contentAsInputElem =
          static_cast<dom::HTMLInputElement*>(GetContent());
      // If script changed the <input>'s type before setting these attributes
      // then we don't need to do anything since we are going to be reframed.
      if (contentAsInputElem->ControlType() == NS_FORM_INPUT_TIME ||
          contentAsInputElem->ControlType() == NS_FORM_INPUT_DATE) {
        Element* dateTimeBoxElement =
            static_cast<dom::HTMLInputElement*>(GetContent())
                ->GetDateTimeBoxElement();
        if (aAttribute == nsGkAtoms::value) {
          if (dateTimeBoxElement) {
            AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
                dateTimeBoxElement,
                NS_LITERAL_STRING("MozDateTimeValueChanged"), CanBubble::eNo,
                ChromeOnlyDispatch::eNo);
            dispatcher->RunDOMEventWhenSafe();
          }
        } else {
          if (dateTimeBoxElement) {
            AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
                dateTimeBoxElement,
                NS_LITERAL_STRING("MozDateTimeAttributeChanged"),
                CanBubble::eNo, ChromeOnlyDispatch::eNo);
            dispatcher->RunDOMEventWhenSafe();
          }
        }
      }
    }
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

void nsDateTimeControlFrame::ContentStatesChanged(EventStates aStates) {
  if (aStates.HasState(NS_EVENT_STATE_DISABLED)) {
    nsContentUtils::AddScriptRunner(new SyncDisabledStateEvent(this));
  }
}

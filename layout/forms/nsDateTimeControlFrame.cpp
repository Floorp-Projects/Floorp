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
#include "nsContentUtils.h"
#include "nsContentCreatorFunctions.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "nsNodeInfoManager.h"
#include "nsIDateTimeInputArea.h"
#include "nsIObserverService.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMMutationEvent.h"
#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame*
NS_NewDateTimeControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsDateTimeControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsDateTimeControlFrame)

NS_QUERYFRAME_HEAD(nsDateTimeControlFrame)
  NS_QUERYFRAME_ENTRY(nsDateTimeControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsDateTimeControlFrame::nsDateTimeControlFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext, kClassID)
{
}

void
nsDateTimeControlFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  aPostDestroyData.AddAnonymousContent(mInputAreaContent.forget());
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void
nsDateTimeControlFrame::OnValueChanged()
{
  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);
  if (inputAreaContent) {
    inputAreaContent->NotifyInputElementValueChanged();
  }
}

void
nsDateTimeControlFrame::OnMinMaxStepAttrChanged()
{
  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);
  if (inputAreaContent) {
    inputAreaContent->NotifyMinMaxStepAttrChanged();
  }
}

void
nsDateTimeControlFrame::SetValueFromPicker(const DateTimeValue& aValue)
{
  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);
  if (inputAreaContent) {
    AutoJSAPI api;
    if (!api.Init(mContent->OwnerDoc()->GetScopeObject())) {
      return;
    }

    JSObject* wrapper = mContent->GetWrapper();
    if (!wrapper) {
      return;
    }

    JSObject* scope = xpc::GetXBLScope(api.cx(), wrapper);
    AutoJSAPI jsapi;
    if (!scope || !jsapi.Init(scope)) {
      return;
    }

    JS::Rooted<JS::Value> jsValue(jsapi.cx());
    if (!ToJSValue(jsapi.cx(), aValue, &jsValue)) {
      return;
    }

    inputAreaContent->SetValueFromPicker(jsValue);
  }
}

void
nsDateTimeControlFrame::SetPickerState(bool aOpen)
{
  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);
  if (inputAreaContent) {
    inputAreaContent->SetPickerState(aOpen);
  }
}

void
nsDateTimeControlFrame::HandleFocusEvent()
{
  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);
  if (inputAreaContent) {
    inputAreaContent->FocusInnerTextBox();
  }
}

void
nsDateTimeControlFrame::HandleBlurEvent()
{
  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);
  if (inputAreaContent) {
    inputAreaContent->BlurInnerTextBox();
  }
}

bool
nsDateTimeControlFrame::HasBadInput()
{
  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);

  bool result = false;
  if (inputAreaContent) {
    inputAreaContent->HasBadInput(&result);
  }

  return result;
}

nscoord
nsDateTimeControlFrame::GetMinISize(gfxContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  if (kid) { // display:none?
    result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                  kid,
                                                  nsLayoutUtils::MIN_ISIZE);
  } else {
    result = 0;
  }

  return result;
}

nscoord
nsDateTimeControlFrame::GetPrefISize(gfxContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  if (kid) { // display:none?
    result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                  kid,
                                                  nsLayoutUtils::PREF_ISIZE);
  } else {
    result = 0;
  }

  return result;
}

void
nsDateTimeControlFrame::Reflow(nsPresContext* aPresContext,
                               ReflowOutput& aDesiredSize,
                               const ReflowInput& aReflowInput,
                               nsReflowStatus& aStatus)
{
  MarkInReflow();

  DO_GLOBAL_REFLOW_COUNT("nsDateTimeControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsDateTimeControlFrame::Reflow: availSize=%d,%d",
                  aReflowInput.AvailableWidth(),
                  aReflowInput.AvailableHeight()));

  NS_ASSERTION(mInputAreaContent, "The input area content must exist!");

  const WritingMode myWM = aReflowInput.GetWritingMode();

  // The ISize of our content box, which is the available ISize
  // for our anonymous content:
  const nscoord contentBoxISize = aReflowInput.ComputedISize();
  nscoord contentBoxBSize = aReflowInput.ComputedBSize();

  // Figure out our border-box sizes as well (by adding borderPadding to
  // content-box sizes):
  const nscoord borderBoxISize = contentBoxISize +
    aReflowInput.ComputedLogicalBorderPadding().IStartEnd(myWM);

  nscoord borderBoxBSize;
  if (contentBoxBSize != NS_INTRINSICSIZE) {
    borderBoxBSize = contentBoxBSize +
      aReflowInput.ComputedLogicalBorderPadding().BStartEnd(myWM);
  } // else, we'll figure out borderBoxBSize after we resolve contentBoxBSize.

  nsIFrame* inputAreaFrame = mFrames.FirstChild();
  if (!inputAreaFrame) { // display:none?
    if (contentBoxBSize == NS_INTRINSICSIZE) {
      contentBoxBSize = 0;
      borderBoxBSize =
        aReflowInput.ComputedLogicalBorderPadding().BStartEnd(myWM);
    }
  } else {
    NS_ASSERTION(inputAreaFrame->GetContent() == mInputAreaContent,
                 "What is this child doing here?");

    ReflowOutput childDesiredSize(aReflowInput);

    WritingMode wm = inputAreaFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

    ReflowInput childReflowOuput(aPresContext, aReflowInput,
                                 inputAreaFrame, availSize);

    // Convert input area margin into my own writing-mode (in case it differs):
    LogicalMargin childMargin =
      childReflowOuput.ComputedLogicalMargin().ConvertTo(myWM, wm);

    // offsets of input area frame within this frame:
    LogicalPoint
      childOffset(myWM,
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
        NS_CSS_MINMAX(contentBoxBSize,
                      aReflowInput.ComputedMinBSize(),
                      aReflowInput.ComputedMaxBSize());

      borderBoxBSize = contentBoxBSize +
        aReflowInput.ComputedLogicalBorderPadding().BStartEnd(myWM);
    }

    // Center child in block axis
    nscoord extraSpace = contentBoxBSize - childMarginBoxBSize;
    childOffset.B(myWM) += std::max(0, extraSpace / 2);

    // Needed in FinishReflowChild, for logical-to-physical conversion:
    nsSize borderBoxSize = LogicalSize(myWM, borderBoxISize, borderBoxBSize).
                           GetPhysicalSize(myWM);

    // Place the child
    FinishReflowChild(inputAreaFrame, aPresContext, childDesiredSize,
                      &childReflowOuput, myWM, childOffset, borderBoxSize, 0);

    nsSize contentBoxSize =
      LogicalSize(myWM, contentBoxISize, contentBoxBSize).
        GetPhysicalSize(myWM);
    aDesiredSize.SetBlockStartAscent(
      childDesiredSize.BlockStartAscent() +
      inputAreaFrame->BStart(aReflowInput.GetWritingMode(),
                             contentBoxSize));
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

nsresult
nsDateTimeControlFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Set up "datetimebox" XUL element which will be XBL-bound to the
  // actual controls.
  nsNodeInfoManager* nodeInfoManager =
    mContent->GetComposedDoc()->NodeInfoManager();
  RefPtr<NodeInfo> nodeInfo =
    nodeInfoManager->GetNodeInfo(nsGkAtoms::datetimebox, nullptr,
                                 kNameSpaceID_XUL, nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  NS_TrustedNewXULElement(getter_AddRefs(mInputAreaContent), nodeInfo.forget());
  aElements.AppendElement(mInputAreaContent);

  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);
  if (inputAreaContent) {
    // Propogate our tabindex.
    nsAutoString tabIndexStr;
    if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr)) {
      inputAreaContent->SetEditAttribute(NS_LITERAL_STRING("tabindex"),
                                         tabIndexStr);
    }

    // Propagate our readonly state.
    nsAutoString readonly;
    if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::readonly, readonly)) {
      inputAreaContent->SetEditAttribute(NS_LITERAL_STRING("readonly"),
                                         readonly);
    }

    SyncDisabledState();
  }

  return NS_OK;
}

void
nsDateTimeControlFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                                 uint32_t aFilter)
{
  if (mInputAreaContent) {
    aElements.AppendElement(mInputAreaContent);
  }
}

void
nsDateTimeControlFrame::SyncDisabledState()
{
  NS_ASSERTION(mInputAreaContent, "The input area content must exist!");
  nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
    do_QueryInterface(mInputAreaContent);
  if (!inputAreaContent) {
    return;
  }

  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    inputAreaContent->SetEditAttribute(NS_LITERAL_STRING("disabled"),
                                       EmptyString());
  } else {
    inputAreaContent->RemoveEditAttribute(NS_LITERAL_STRING("disabled"));
  }
}

nsresult
nsDateTimeControlFrame::AttributeChanged(int32_t aNameSpaceID,
                                         nsAtom* aAttribute,
                                         int32_t aModType)
{
  NS_ASSERTION(mInputAreaContent, "The input area content must exist!");

  // nsGkAtoms::disabled is handled by SyncDisabledState
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value ||
        aAttribute == nsGkAtoms::readonly ||
        aAttribute == nsGkAtoms::tabindex) {
      MOZ_ASSERT(mContent->IsHTMLElement(nsGkAtoms::input), "bad cast");
      auto contentAsInputElem = static_cast<dom::HTMLInputElement*>(GetContent());
      // If script changed the <input>'s type before setting these attributes
      // then we don't need to do anything since we are going to be reframed.
      if (contentAsInputElem->ControlType() == NS_FORM_INPUT_TIME ||
          contentAsInputElem->ControlType() == NS_FORM_INPUT_DATE) {
        nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
          do_QueryInterface(mInputAreaContent);
        if (aAttribute == nsGkAtoms::value) {
          if (inputAreaContent) {
            nsContentUtils::AddScriptRunner(NewRunnableMethod(
              "nsIDateTimeInputArea::NotifyInputElementValueChanged",
              inputAreaContent,
              &nsIDateTimeInputArea::NotifyInputElementValueChanged));
          }
        } else {
          if (aModType == nsIDOMMutationEvent::REMOVAL) {
            if (inputAreaContent) {
              nsAtomString name(aAttribute);
              inputAreaContent->RemoveEditAttribute(name);
            }
          } else {
            MOZ_ASSERT(aModType == nsIDOMMutationEvent::ADDITION ||
                       aModType == nsIDOMMutationEvent::MODIFICATION);
            if (inputAreaContent) {
              nsAtomString name(aAttribute);
              nsAutoString value;
              mContent->GetAttr(aNameSpaceID, aAttribute, value);
              inputAreaContent->SetEditAttribute(name, value);
            }
          }
        }
      }
    }
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                            aModType);
}

void
nsDateTimeControlFrame::ContentStatesChanged(EventStates aStates)
{
  if (aStates.HasState(NS_EVENT_STATE_DISABLED)) {
    nsContentUtils::AddScriptRunner(new SyncDisabledStateEvent(this));
  }
}

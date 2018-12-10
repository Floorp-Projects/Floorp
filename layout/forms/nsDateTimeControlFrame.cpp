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
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsDOMTokenList.h"
#include "nsNodeInfoManager.h"
#include "nsIDateTimeInputArea.h"
#include "nsIObserverService.h"
#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame* NS_NewDateTimeControlFrame(nsIPresShell* aPresShell,
                                     ComputedStyle* aStyle) {
  return new (aPresShell) nsDateTimeControlFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(nsDateTimeControlFrame)

NS_QUERYFRAME_HEAD(nsDateTimeControlFrame)
  NS_QUERYFRAME_ENTRY(nsDateTimeControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsDateTimeControlFrame::nsDateTimeControlFrame(ComputedStyle* aStyle)
    : nsContainerFrame(aStyle, kClassID) {}

void nsDateTimeControlFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                         PostDestroyData& aPostDestroyData) {
  aPostDestroyData.AddAnonymousContent(mInputAreaContent.forget());
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsDateTimeControlFrame::OnValueChanged() {
  if (mInputAreaContent) {
    nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
        do_QueryInterface(mInputAreaContent);
    if (inputAreaContent) {
      inputAreaContent->NotifyInputElementValueChanged();
    }
  } else {
    Element* inputAreaContent = GetInputAreaContentAsElement();
    if (!inputAreaContent) {
      return;
    }

    AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
        inputAreaContent, NS_LITERAL_STRING("MozDateTimeValueChanged"),
        CanBubble::eNo, ChromeOnlyDispatch::eNo);
    dispatcher->RunDOMEventWhenSafe();
  }
}

void nsDateTimeControlFrame::OnMinMaxStepAttrChanged() {
  if (mInputAreaContent) {
    nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
        do_QueryInterface(mInputAreaContent);
    if (inputAreaContent) {
      inputAreaContent->NotifyMinMaxStepAttrChanged();
    }
  } else {
    Element* inputAreaContent = GetInputAreaContentAsElement();
    if (!inputAreaContent) {
      return;
    }

    AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
        inputAreaContent, NS_LITERAL_STRING("MozNotifyMinMaxStepAttrChanged"),
        CanBubble::eNo, ChromeOnlyDispatch::eNo);
    dispatcher->RunDOMEventWhenSafe();
  }
}

void nsDateTimeControlFrame::HandleFocusEvent() {
  if (mInputAreaContent) {
    nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
        do_QueryInterface(mInputAreaContent);
    if (inputAreaContent) {
      inputAreaContent->FocusInnerTextBox();
    }
  } else {
    Element* inputAreaContent = GetInputAreaContentAsElement();
    if (!inputAreaContent) {
      return;
    }

    AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
        inputAreaContent, NS_LITERAL_STRING("MozFocusInnerTextBox"),
        CanBubble::eNo, ChromeOnlyDispatch::eNo);
    dispatcher->RunDOMEventWhenSafe();
  }
}

void nsDateTimeControlFrame::HandleBlurEvent() {
  if (mInputAreaContent) {
    nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
        do_QueryInterface(mInputAreaContent);
    if (inputAreaContent) {
      inputAreaContent->BlurInnerTextBox();
    }
  } else {
    Element* inputAreaContent = GetInputAreaContentAsElement();
    if (!inputAreaContent) {
      return;
    }

    AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
        inputAreaContent, NS_LITERAL_STRING("MozBlurInnerTextBox"),
        CanBubble::eNo, ChromeOnlyDispatch::eNo);
    dispatcher->RunDOMEventWhenSafe();
  }
}

bool nsDateTimeControlFrame::HasBadInput() {
  Element* editWrapperElement = nullptr;
  if (mInputAreaContent) {
    // edit-wrapper is inside an XBL binding
    editWrapperElement =
        mInputAreaContent->GetComposedDoc()->GetAnonymousElementByAttribute(
            mInputAreaContent, nsGkAtoms::anonid,
            NS_LITERAL_STRING("edit-wrapper"));
  } else if (mContent->GetShadowRoot()) {
    // edit-wrapper is inside an UA Widget Shadow DOM
    editWrapperElement = mContent->GetShadowRoot()->GetElementById(
        NS_LITERAL_STRING("edit-wrapper"));
  }

  if (!editWrapperElement) {
    return false;
  }

  // Incomplete field does not imply bad input.
  for (Element* child = editWrapperElement->GetFirstElementChild(); child;
       child = child->GetNextElementSibling()) {
    if (child->ClassList()->Contains(
            NS_LITERAL_STRING("datetime-edit-field"))) {
      nsAutoString value;
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::value, value);
      if (value.IsEmpty()) {
        return false;
      }
    }
  }

  // All fields are available but input element's value is empty implies
  // it has been sanitized.
  nsAutoString value;
  HTMLInputElement::FromNode(mContent)->GetValue(value, CallerType::System);
  return value.IsEmpty();
}

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

/**
 * nsDateTimeControlFrame should be a non-leaf frame when UA Widget is enabled,
 * so the datetimebox container element inserted under the Shadow Root can be
 * picked up. No frames will be generated from elements from the web content,
 * given that they have been replaced by the Shadow Root without an <slots>
 * element in the DOM tree.
 *
 * When the UA Widget is disabled, i.e. the datetimebox is bound as anonymous
 * content with XBL, nsDateTimeControlFrame has to be a leaf so no frames from
 * web content element will be generated.
 */
bool nsDateTimeControlFrame::IsLeafDynamic() const {
  return !nsContentUtils::IsUAWidgetEnabled();
}

nsresult nsDateTimeControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  if (nsContentUtils::IsUAWidgetEnabled()) {
    return NS_OK;
  }

  // Set up "datetimebox" XUL element which will be XBL-bound to the
  // actual controls.
  nsNodeInfoManager* nodeInfoManager =
      mContent->GetComposedDoc()->NodeInfoManager();
  RefPtr<NodeInfo> nodeInfo = nodeInfoManager->GetNodeInfo(
      nsGkAtoms::datetimebox, nullptr, kNameSpaceID_XUL, nsINode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  NS_TrustedNewXULElement(getter_AddRefs(mInputAreaContent), nodeInfo.forget());
  aElements.AppendElement(mInputAreaContent);

  return NS_OK;
}

void nsDateTimeControlFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  if (mInputAreaContent) {
    aElements.AppendElement(mInputAreaContent);
  }
}

void nsDateTimeControlFrame::SyncDisabledState() {
  if (mInputAreaContent) {
    nsCOMPtr<nsIDateTimeInputArea> inputAreaContent =
        do_QueryInterface(mInputAreaContent);
    if (!inputAreaContent) {
      return;
    }
    inputAreaContent->UpdateEditAttributes();
  } else {
    Element* inputAreaContent = GetInputAreaContentAsElement();
    if (!inputAreaContent) {
      return;
    }

    AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
        inputAreaContent, NS_LITERAL_STRING("MozDateTimeAttributeChanged"),
        CanBubble::eNo, ChromeOnlyDispatch::eNo);
    dispatcher->RunDOMEventWhenSafe();
  }
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
        if (mInputAreaContent) {
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
            if (inputAreaContent) {
              inputAreaContent->UpdateEditAttributes();
            }
          }
        } else {
          Element* inputAreaContent = GetInputAreaContentAsElement();
          if (aAttribute == nsGkAtoms::value) {
            if (inputAreaContent) {
              AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
                  inputAreaContent,
                  NS_LITERAL_STRING("NotifyInputElementValueChanged"),
                  CanBubble::eNo, ChromeOnlyDispatch::eNo);
              dispatcher->RunDOMEventWhenSafe();
            }
          } else {
            if (inputAreaContent) {
              AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(
                  inputAreaContent,
                  NS_LITERAL_STRING("MozDateTimeValueChanged"), CanBubble::eNo,
                  ChromeOnlyDispatch::eNo);
              dispatcher->RunDOMEventWhenSafe();
            }
          }
        }
      }
    }
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nsIContent* nsDateTimeControlFrame::GetInputAreaContent() {
  if (mInputAreaContent) {
    return mInputAreaContent;
  }
  if (mContent->GetShadowRoot()) {
    // The datetimebox <div> is the only child of the UA Widget Shadow Root
    // if it is present.
    MOZ_ASSERT(mContent->GetShadowRoot()->IsUAWidget());
    MOZ_ASSERT(1 >= mContent->GetShadowRoot()->GetChildCount());
    return mContent->GetShadowRoot()->GetFirstChild();
  }
  return nullptr;
}

Element* nsDateTimeControlFrame::GetInputAreaContentAsElement() {
  nsIContent* inputAreaContent = GetInputAreaContent();
  if (inputAreaContent) {
    return inputAreaContent->AsElement();
  }
  return nullptr;
}

void nsDateTimeControlFrame::ContentStatesChanged(EventStates aStates) {
  if (aStates.HasState(NS_EVENT_STATE_DISABLED)) {
    nsContentUtils::AddScriptRunner(new SyncDisabledStateEvent(this));
  }
}

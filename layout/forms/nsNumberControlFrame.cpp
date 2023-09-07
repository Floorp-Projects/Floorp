/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNumberControlFrame.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PresShell.h"
#include "HTMLInputElement.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsStyleConsts.h"
#include "nsContentUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsCSSPseudoElements.h"
#include "nsLayoutUtils.h"

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/AccTypes.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame* NS_NewNumberControlFrame(PresShell* aPresShell,
                                   ComputedStyle* aStyle) {
  return new (aPresShell)
      nsNumberControlFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsNumberControlFrame)

NS_QUERYFRAME_HEAD(nsNumberControlFrame)
  NS_QUERYFRAME_ENTRY(nsNumberControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsTextControlFrame)

nsNumberControlFrame::nsNumberControlFrame(ComputedStyle* aStyle,
                                           nsPresContext* aPresContext)
    : nsTextControlFrame(aStyle, aPresContext, kClassID) {}

void nsNumberControlFrame::Destroy(DestroyContext& aContext) {
  aContext.AddAnonymousContent(mSpinBox.forget());
  nsTextControlFrame::Destroy(aContext);
}

nsresult nsNumberControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  // We create an anonymous tree for our input element that is structured as
  // follows:
  //
  // input
  //   div    - placeholder
  //   div    - preview div
  //   div    - editor root
  //   div    - spin box wrapping up/down arrow buttons
  //     div  - spin up (up arrow button)
  //     div  - spin down (down arrow button)
  //
  // If you change this, be careful to change the order of stuff returned in
  // AppendAnonymousContentTo.

  nsTextControlFrame::CreateAnonymousContent(aElements);

  // The author has elected to hide the spinner by setting this
  // -moz-appearance. We will reframe if it changes.
  if (StyleDisplay()->EffectiveAppearance() != StyleAppearance::Textfield) {
    // Create the ::-moz-number-spin-box pseudo-element:
    mSpinBox = MakeAnonElement(PseudoStyleType::mozNumberSpinBox);

    // Create the ::-moz-number-spin-up pseudo-element:
    mSpinUp = MakeAnonElement(PseudoStyleType::mozNumberSpinUp, mSpinBox);

    // Create the ::-moz-number-spin-down pseudo-element:
    mSpinDown = MakeAnonElement(PseudoStyleType::mozNumberSpinDown, mSpinBox);

    aElements.AppendElement(mSpinBox);
  }

  return NS_OK;
}

/* static */
nsNumberControlFrame* nsNumberControlFrame::GetNumberControlFrameForSpinButton(
    nsIFrame* aFrame) {
  // If aFrame is a spin button for an <input type=number> then we expect the
  // frame of the NAC root parent to be that input's frame. We have to check for
  // this via the content tree because we don't know whether extra frames will
  // be wrapped around any of the elements between aFrame and the
  // nsNumberControlFrame that we're looking for (e.g. flex wrappers).
  nsIContent* content = aFrame->GetContent();
  auto* nacHost = content->GetClosestNativeAnonymousSubtreeRootParentOrHost();
  if (!nacHost) {
    return nullptr;
  }
  auto* input = HTMLInputElement::FromNode(nacHost);
  if (!input || input->ControlType() != FormControlType::InputNumber) {
    return nullptr;
  }
  return do_QueryFrame(input->GetPrimaryFrame());
}

int32_t nsNumberControlFrame::GetSpinButtonForPointerEvent(
    WidgetGUIEvent* aEvent) const {
  MOZ_ASSERT(aEvent->mClass == eMouseEventClass, "Unexpected event type");

  if (!mSpinBox) {
    // we don't have a spinner
    return eSpinButtonNone;
  }
  if (aEvent->mOriginalTarget == mSpinUp) {
    return eSpinButtonUp;
  }
  if (aEvent->mOriginalTarget == mSpinDown) {
    return eSpinButtonDown;
  }
  if (aEvent->mOriginalTarget == mSpinBox) {
    // In the case that the up/down buttons are hidden (display:none) we use
    // just the spin box element, spinning up if the pointer is over the top
    // half of the element, or down if it's over the bottom half. This is
    // important to handle since this is the state things are in for the
    // default UA style sheet. See the comment in forms.css for why.
    LayoutDeviceIntPoint absPoint = aEvent->mRefPoint;
    nsPoint point = nsLayoutUtils::GetEventCoordinatesRelativeTo(
        aEvent, absPoint, RelativeTo{mSpinBox->GetPrimaryFrame()});
    if (point != nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)) {
      if (point.y < mSpinBox->GetPrimaryFrame()->GetSize().height / 2) {
        return eSpinButtonUp;
      }
      return eSpinButtonDown;
    }
  }
  return eSpinButtonNone;
}

void nsNumberControlFrame::SpinnerStateChanged() const {
  if (mSpinUp) {
    nsIFrame* spinUpFrame = mSpinUp->GetPrimaryFrame();
    if (spinUpFrame && spinUpFrame->IsThemed()) {
      spinUpFrame->InvalidateFrame();
    }
  }
  if (mSpinDown) {
    nsIFrame* spinDownFrame = mSpinDown->GetPrimaryFrame();
    if (spinDownFrame && spinDownFrame->IsThemed()) {
      spinDownFrame->InvalidateFrame();
    }
  }
}

bool nsNumberControlFrame::SpinnerUpButtonIsDepressed() const {
  return HTMLInputElement::FromNode(mContent)
      ->NumberSpinnerUpButtonIsDepressed();
}

bool nsNumberControlFrame::SpinnerDownButtonIsDepressed() const {
  return HTMLInputElement::FromNode(mContent)
      ->NumberSpinnerDownButtonIsDepressed();
}

void nsNumberControlFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  nsTextControlFrame::AppendAnonymousContentTo(aElements, aFilter);
  if (mSpinBox) {
    aElements.AppendElement(mSpinBox);
  }
}

#ifdef ACCESSIBILITY
a11y::AccType nsNumberControlFrame::AccessibleType() {
  return a11y::eHTMLSpinnerType;
}
#endif

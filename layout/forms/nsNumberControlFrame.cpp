/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNumberControlFrame.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/EventStates.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PresShell.h"
#include "HTMLInputElement.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsStyleConsts.h"
#include "nsContentUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsCSSPseudoElements.h"

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

void nsNumberControlFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                       PostDestroyData& aPostDestroyData) {
  aPostDestroyData.AddAnonymousContent(mOuterWrapper.forget());
  nsTextControlFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

already_AddRefed<Element> nsNumberControlFrame::MakeAnonymousElement(
    Element* aParent, nsAtom* aTagName, PseudoStyleType aPseudoType) {
  // Get the NodeInfoManager and tag necessary to create the anonymous divs.
  Document* doc = mContent->GetComposedDoc();
  RefPtr<Element> resultElement = doc->CreateHTMLElement(aTagName);
  resultElement->SetPseudoElementType(aPseudoType);

  if (aPseudoType == PseudoStyleType::mozNumberSpinDown ||
      aPseudoType == PseudoStyleType::mozNumberSpinUp) {
    resultElement->SetAttr(kNameSpaceID_None, nsGkAtoms::aria_hidden,
                           NS_LITERAL_STRING("true"), false);
  }

  if (aParent) {
    aParent->AppendChildTo(resultElement, false);
  }

  return resultElement.forget();
}

nsresult nsNumberControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  // We create an anonymous tree for our input element that is structured as
  // follows:
  //
  // input
  //   div      - outer wrapper with "display:flex" by default
  //     div    - editor root
  //     div    - spin box wrapping up/down arrow buttons
  //       div  - spin up (up arrow button)
  //       div  - spin down (down arrow button)
  //   div      - placeholder
  //   div      - preview div
  //
  // If you change this, be careful to change the destruction order in
  // nsNumberControlFrame::DestroyFrom.

  // Create the anonymous outer wrapper:
  mOuterWrapper = MakeAnonymousElement(nullptr, nsGkAtoms::div,
                                       PseudoStyleType::mozNumberWrapper);

  // We want to do this now, rather than on the caller, so that the
  // AppendChildTo calls below know that they are anonymous already. This is
  // important for the NODE_IS_EDITABLE flag handling, for example.
  mOuterWrapper->SetIsNativeAnonymousRoot();

  aElements.AppendElement(mOuterWrapper);

  nsTArray<ContentInfo> nestedContent;
  nsTextControlFrame::CreateAnonymousContent(nestedContent);
  for (auto& content : nestedContent) {
    // The root goes inside the container.
    if (content.mContent == mRootNode) {
      mOuterWrapper->AppendChildTo(content.mContent, false);
    } else {
      // The rest (placeholder and preview), directly under us.
      aElements.AppendElement(std::move(content));
    }
  }

  if (StyleDisplay()->mAppearance == StyleAppearance::Textfield) {
    // The author has elected to hide the spinner by setting this
    // -moz-appearance. We will reframe if it changes.
    return NS_OK;
  }

  // Create the ::-moz-number-spin-box pseudo-element:
  mSpinBox = MakeAnonymousElement(mOuterWrapper, nsGkAtoms::div,
                                  PseudoStyleType::mozNumberSpinBox);

  // Create the ::-moz-number-spin-up pseudo-element:
  mSpinUp = MakeAnonymousElement(mSpinBox, nsGkAtoms::div,
                                 PseudoStyleType::mozNumberSpinUp);

  // Create the ::-moz-number-spin-down pseudo-element:
  mSpinDown = MakeAnonymousElement(mSpinBox, nsGkAtoms::div,
                                   PseudoStyleType::mozNumberSpinDown);

  return NS_OK;
}

/* static */
nsNumberControlFrame* nsNumberControlFrame::GetNumberControlFrameForTextField(
    nsIFrame* aFrame) {
  // If aFrame is the anon text field for an <input type=number> then we expect
  // the frame of its mContent's grandparent to be that input's frame. We
  // have to check for this via the content tree because we don't know whether
  // extra frames will be wrapped around any of the elements between aFrame and
  // the nsNumberControlFrame that we're looking for (e.g. flex wrappers).
  nsIContent* content = aFrame->GetContent();
  if (content->IsInNativeAnonymousSubtree() && content->GetParent() &&
      content->GetParent()->GetParent()) {
    nsIContent* grandparent = content->GetParent()->GetParent();
    if (grandparent->IsHTMLElement(nsGkAtoms::input) &&
        grandparent->AsElement()->AttrValueIs(
            kNameSpaceID_None, nsGkAtoms::type, nsGkAtoms::number,
            eCaseMatters)) {
      return do_QueryFrame(grandparent->GetPrimaryFrame());
    }
  }
  return nullptr;
}

/* static */
nsNumberControlFrame* nsNumberControlFrame::GetNumberControlFrameForSpinButton(
    nsIFrame* aFrame) {
  // If aFrame is a spin button for an <input type=number> then we expect the
  // frame of its mContent's great-grandparent to be that input's frame. We
  // have to check for this via the content tree because we don't know whether
  // extra frames will be wrapped around any of the elements between aFrame and
  // the nsNumberControlFrame that we're looking for (e.g. flex wrappers).
  nsIContent* content = aFrame->GetContent();
  if (content->IsInNativeAnonymousSubtree() && content->GetParent() &&
      content->GetParent()->GetParent() &&
      content->GetParent()->GetParent()->GetParent()) {
    nsIContent* greatgrandparent =
        content->GetParent()->GetParent()->GetParent();
    if (greatgrandparent->IsHTMLElement(nsGkAtoms::input) &&
        greatgrandparent->AsElement()->AttrValueIs(
            kNameSpaceID_None, nsGkAtoms::type, nsGkAtoms::number,
            eCaseMatters)) {
      return do_QueryFrame(greatgrandparent->GetPrimaryFrame());
    }
  }
  return nullptr;
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
        aEvent, absPoint, mSpinBox->GetPrimaryFrame());
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
  MOZ_ASSERT(mSpinUp && mSpinDown,
             "We should not be called when we have no spinner");

  nsIFrame* spinUpFrame = mSpinUp->GetPrimaryFrame();
  if (spinUpFrame && spinUpFrame->IsThemed()) {
    spinUpFrame->InvalidateFrame();
  }
  nsIFrame* spinDownFrame = mSpinDown->GetPrimaryFrame();
  if (spinDownFrame && spinDownFrame->IsThemed()) {
    spinDownFrame->InvalidateFrame();
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

#define STYLES_DISABLING_NATIVE_THEMING \
  NS_AUTHOR_SPECIFIED_BORDER_OR_BACKGROUND | NS_AUTHOR_SPECIFIED_PADDING

bool nsNumberControlFrame::ShouldUseNativeStyleForSpinner() const {
  MOZ_ASSERT(mSpinUp && mSpinDown,
             "We should not be called when we have no spinner");

  nsIFrame* spinUpFrame = mSpinUp->GetPrimaryFrame();
  nsIFrame* spinDownFrame = mSpinDown->GetPrimaryFrame();

  return spinUpFrame &&
         spinUpFrame->StyleDisplay()->mAppearance ==
             StyleAppearance::SpinnerUpbutton &&
         !PresContext()->HasAuthorSpecifiedRules(
             spinUpFrame, STYLES_DISABLING_NATIVE_THEMING) &&
         spinDownFrame &&
         spinDownFrame->StyleDisplay()->mAppearance ==
             StyleAppearance::SpinnerDownbutton &&
         !PresContext()->HasAuthorSpecifiedRules(
             spinDownFrame, STYLES_DISABLING_NATIVE_THEMING);
}

void nsNumberControlFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  if (mOuterWrapper) {
    aElements.AppendElement(mOuterWrapper);
  }
  if (mPlaceholderDiv) {
    aElements.AppendElement(mPlaceholderDiv);
  }
  if (mPreviewDiv) {
    aElements.AppendElement(mPreviewDiv);
  }
}

#ifdef ACCESSIBILITY
a11y::AccType nsNumberControlFrame::AccessibleType() {
  return a11y::eHTMLSpinnerType;
}
#endif

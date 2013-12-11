/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNumberControlFrame.h"

#include "HTMLInputElement.h"
#include "nsIFocusManager.h"
#include "nsIPresShell.h"
#include "nsFocusManager.h"
#include "nsFontMetrics.h"
#include "nsFormControlFrame.h"
#include "nsGkAtoms.h"
#include "nsINodeInfo.h"
#include "nsINameSpaceManager.h"
#include "nsThemeConstants.h"
#include "mozilla/BasicEvents.h"
#include "nsContentUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentList.h"
#include "nsStyleSet.h"
#include "nsIDOMMutationEvent.h"

#ifdef ACCESSIBILITY
#include "mozilla/a11y/AccTypes.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame*
NS_NewNumberControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsNumberControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsNumberControlFrame)

NS_QUERYFRAME_HEAD(nsNumberControlFrame)
  NS_QUERYFRAME_ENTRY(nsNumberControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsNumberControlFrame::nsNumberControlFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
  , mHandlingInputEvent(false)
{
}

void
nsNumberControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!GetPrevContinuation() && !GetNextContinuation(),
               "nsNumberControlFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first");
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContentUtils::DestroyAnonymousContent(&mOuterWrapper);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
nsNumberControlFrame::Reflow(nsPresContext* aPresContext,
                             nsHTMLReflowMetrics& aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsNumberControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(mOuterWrapper, "Outer wrapper div must exist!");

  NS_ASSERTION(!GetPrevContinuation() && !GetNextContinuation(),
               "nsNumberControlFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first");

  NS_ASSERTION(!mFrames.FirstChild() ||
               !mFrames.FirstChild()->GetNextSibling(),
               "We expect at most one direct child frame");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  nsHTMLReflowMetrics wrappersDesiredSize;
  nsIFrame* outerWrapperFrame = mOuterWrapper->GetPrimaryFrame();
  if (outerWrapperFrame) { // display:none?
    NS_ASSERTION(outerWrapperFrame == mFrames.FirstChild(), "huh?");
    nsresult rv =
      ReflowAnonymousContent(aPresContext, wrappersDesiredSize,
                             aReflowState, outerWrapperFrame);
    NS_ENSURE_SUCCESS(rv, rv);
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, outerWrapperFrame);
  }

  nscoord computedHeight = aReflowState.ComputedHeight();
  if (computedHeight == NS_AUTOHEIGHT) {
    computedHeight =
      outerWrapperFrame ? outerWrapperFrame->GetSize().height : 0;
  }
  aDesiredSize.width = aReflowState.ComputedWidth() +
                         aReflowState.mComputedBorderPadding.LeftRight();
  aDesiredSize.height = computedHeight +
                          aReflowState.mComputedBorderPadding.TopBottom();

  if (outerWrapperFrame) {
    aDesiredSize.ascent = wrappersDesiredSize.ascent +
                            outerWrapperFrame->GetPosition().y;
  }

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  FinishAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return NS_OK;
}

nsresult
nsNumberControlFrame::
  ReflowAnonymousContent(nsPresContext* aPresContext,
                         nsHTMLReflowMetrics& aWrappersDesiredSize,
                         const nsHTMLReflowState& aParentReflowState,
                         nsIFrame* aOuterWrapperFrame)
{
  MOZ_ASSERT(aOuterWrapperFrame);

  // The width of our content box, which is the available width
  // for our anonymous content:
  nscoord inputFrameContentBoxWidth = aParentReflowState.ComputedWidth();

  nsHTMLReflowState wrapperReflowState(aPresContext, aParentReflowState,
                                       aOuterWrapperFrame,
                                       nsSize(inputFrameContentBoxWidth,
                                              NS_UNCONSTRAINEDSIZE));

  nscoord xoffset = aParentReflowState.mComputedBorderPadding.left +
                      wrapperReflowState.mComputedMargin.left;
  nscoord yoffset = aParentReflowState.mComputedBorderPadding.top +
                      wrapperReflowState.mComputedMargin.top;

  nsReflowStatus childStatus;
  nsresult rv = ReflowChild(aOuterWrapperFrame, aPresContext,
                            aWrappersDesiredSize, wrapperReflowState,
                            xoffset, yoffset, 0, childStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(NS_FRAME_IS_FULLY_COMPLETE(childStatus),
             "We gave our child unconstrained height, so it should be complete");
  return FinishReflowChild(aOuterWrapperFrame, aPresContext,
                           &wrapperReflowState, aWrappersDesiredSize,
                           xoffset, yoffset, 0);
}

NS_IMETHODIMP
nsNumberControlFrame::AttributeChanged(int32_t  aNameSpaceID,
                                       nsIAtom* aAttribute,
                                       int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::placeholder ||
        aAttribute == nsGkAtoms::readonly ||
        aAttribute == nsGkAtoms::tabindex) {
      if (aModType == nsIDOMMutationEvent::REMOVAL) {
        mTextField->UnsetAttr(aNameSpaceID, aAttribute, true);
      } else {
        MOZ_ASSERT(aModType == nsIDOMMutationEvent::ADDITION ||
                   aModType == nsIDOMMutationEvent::MODIFICATION);
        nsAutoString value;
        mContent->GetAttr(aNameSpaceID, aAttribute, value);
        mTextField->SetAttr(aNameSpaceID, aAttribute, value, true);
      }
    }
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                            aModType);
}

nsresult
nsNumberControlFrame::MakeAnonymousElement(Element** aResult,
                                           nsTArray<ContentInfo>& aElements,
                                           nsIAtom* aTagName,
                                           nsCSSPseudoElements::Type aPseudoType,
                                           nsStyleContext* aParentContext)
{
  // Get the NodeInfoManager and tag necessary to create the anonymous divs.
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();
  nsRefPtr<Element> resultElement = doc->CreateHTMLElement(aTagName);

  // If we legitimately fail this assertion and need to allow
  // non-pseudo-element anonymous children, then we'll need to add a branch
  // that calls ResolveStyleFor((*aResult)->AsElement(), aParentContext)") to
  // set newStyleContext.
  NS_ASSERTION(aPseudoType != nsCSSPseudoElements::ePseudo_NotPseudoElement,
               "Expecting anonymous children to all be pseudo-elements");
  // Associate the pseudo-element with the anonymous child
  nsRefPtr<nsStyleContext> newStyleContext =
    PresContext()->StyleSet()->ResolvePseudoElementStyle(mContent->AsElement(),
                                                         aPseudoType,
                                                         aParentContext,
                                                         resultElement);

  if (!aElements.AppendElement(ContentInfo(resultElement, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (aPseudoType == nsCSSPseudoElements::ePseudo_mozNumberSpinDown ||
      aPseudoType == nsCSSPseudoElements::ePseudo_mozNumberSpinUp) {
    resultElement->SetAttr(kNameSpaceID_None, nsGkAtoms::role,
                           NS_LITERAL_STRING("button"), false);
  }

  resultElement.forget(aResult);
  return NS_OK;
}

nsresult
nsNumberControlFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  nsresult rv;

  // We create an anonymous tree for our input element that is structured as
  // follows:
  //
  // input
  //   div      - outer wrapper with "display:flex" by default
  //     input  - text input field
  //     div    - spin box wrapping up/down arrow buttons
  //       div  - spin up (up arrow button)
  //       div  - spin down (down arrow button)
  //
  // If you change this, be careful to change the destruction order in
  // nsNumberControlFrame::DestroyFrom.


  // Create the anonymous outer wrapper:
  rv = MakeAnonymousElement(getter_AddRefs(mOuterWrapper),
                            aElements,
                            nsGkAtoms::div,
                            nsCSSPseudoElements::ePseudo_mozNumberWrapper,
                            mStyleContext);
  NS_ENSURE_SUCCESS(rv, rv);

  ContentInfo& outerWrapperCI = aElements.LastElement();

  // Create the ::-moz-number-text pseudo-element:
  rv = MakeAnonymousElement(getter_AddRefs(mTextField),
                            outerWrapperCI.mChildren,
                            nsGkAtoms::input,
                            nsCSSPseudoElements::ePseudo_mozNumberText,
                            outerWrapperCI.mStyleContext);
  NS_ENSURE_SUCCESS(rv, rv);

  mTextField->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                      NS_LITERAL_STRING("text"), PR_FALSE);

  HTMLInputElement* content = HTMLInputElement::FromContent(mContent);
  HTMLInputElement* textField = HTMLInputElement::FromContent(mTextField);

  // Initialize the text field value:
  nsAutoString value;
  content->GetValue(value);
  mTextField->SetAttr(kNameSpaceID_None, nsGkAtoms::value, value, false);

  // If we're readonly, make sure our anonymous text control is too:
  nsAutoString readonly;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::readonly, readonly)) {
    mTextField->SetAttr(kNameSpaceID_None, nsGkAtoms::readonly, readonly, false);
  }

  // Propogate our tabindex:
  int32_t tabIndex;
  content->GetTabIndex(&tabIndex);
  textField->SetTabIndex(tabIndex);

  // Initialize the text field's placeholder, if ours is set:
  nsAutoString placeholder;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder, placeholder)) {
    mTextField->SetAttr(kNameSpaceID_None, nsGkAtoms::placeholder, placeholder, false);
  }

  if (mContent->AsElement()->State().HasState(NS_EVENT_STATE_FOCUS)) {
    // We don't want to focus the frame but the text field.
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mTextField);
    NS_ASSERTION(element, "Really, this should be a nsIDOMElement!");
    fm->SetFocus(element, 0);
  }

  if (StyleDisplay()->mAppearance == NS_THEME_TEXTFIELD) {
    // The author has elected to hide the spinner by setting this
    // -moz-appearance. We will reframe if it changes.
    return rv;
  }

  // Create the ::-moz-number-spin-box pseudo-element:
  rv = MakeAnonymousElement(getter_AddRefs(mSpinBox),
                            outerWrapperCI.mChildren,
                            nsGkAtoms::div,
                            nsCSSPseudoElements::ePseudo_mozNumberSpinBox,
                            outerWrapperCI.mStyleContext);
  NS_ENSURE_SUCCESS(rv, rv);

  ContentInfo& spinBoxCI = outerWrapperCI.mChildren.LastElement();

  // Create the ::-moz-number-spin-up pseudo-element:
  rv = MakeAnonymousElement(getter_AddRefs(mSpinUp),
                            spinBoxCI.mChildren,
                            nsGkAtoms::div,
                            nsCSSPseudoElements::ePseudo_mozNumberSpinUp,
                            spinBoxCI.mStyleContext);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the ::-moz-number-spin-down pseudo-element:
  rv = MakeAnonymousElement(getter_AddRefs(mSpinDown),
                            spinBoxCI.mChildren,
                            nsGkAtoms::div,
                            nsCSSPseudoElements::ePseudo_mozNumberSpinDown,
                            spinBoxCI.mStyleContext);
  return rv;
}

nsIAtom*
nsNumberControlFrame::GetType() const
{
  return nsGkAtoms::numberControlFrame;
}

HTMLInputElement*
nsNumberControlFrame::GetAnonTextControl()
{
  return mTextField ? HTMLInputElement::FromContent(mTextField) : nullptr;
}

/* static */ nsNumberControlFrame*
nsNumberControlFrame::GetNumberControlFrameForTextField(nsIFrame* aFrame)
{
  // If aFrame is the anon text field for an <input type=number> then we expect
  // the frame of its mContent's grandparent to be that input's frame. We
  // have to check for this via the content tree because we don't know whether
  // extra frames will be wrapped around any of the elements between aFrame and
  // the nsNumberControlFrame that we're looking for (e.g. flex wrappers).
  nsIContent* content = aFrame->GetContent();
  if (content->IsInNativeAnonymousSubtree() &&
      content->GetParent() && content->GetParent()->GetParent()) {
    nsIContent* grandparent = content->GetParent()->GetParent();
    if (grandparent->IsHTML(nsGkAtoms::input) &&
        grandparent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                 nsGkAtoms::number, eCaseMatters)) {
      return do_QueryFrame(grandparent->GetPrimaryFrame());
    }
  }
  return nullptr;
}

/* static */ nsNumberControlFrame*
nsNumberControlFrame::GetNumberControlFrameForSpinButton(nsIFrame* aFrame)
{
  // If aFrame is a spin button for an <input type=number> then we expect the
  // frame of its mContent's great-grandparent to be that input's frame. We
  // have to check for this via the content tree because we don't know whether
  // extra frames will be wrapped around any of the elements between aFrame and
  // the nsNumberControlFrame that we're looking for (e.g. flex wrappers).
  nsIContent* content = aFrame->GetContent();
  if (content->IsInNativeAnonymousSubtree() &&
      content->GetParent() && content->GetParent()->GetParent() &&
      content->GetParent()->GetParent()->GetParent()) {
    nsIContent* greatgrandparent = content->GetParent()->GetParent()->GetParent();
    if (greatgrandparent->IsHTML(nsGkAtoms::input) &&
        greatgrandparent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                      nsGkAtoms::number, eCaseMatters)) {
      return do_QueryFrame(greatgrandparent->GetPrimaryFrame());
    }
  }
  return nullptr;
}

int32_t
nsNumberControlFrame::GetSpinButtonForPointerEvent(WidgetGUIEvent* aEvent) const
{
  MOZ_ASSERT(aEvent->eventStructType == NS_MOUSE_EVENT,
             "Unexpected event type");

  if (!mSpinBox) {
    // we don't have a spinner
    return eSpinButtonNone;
  }
  if (aEvent->originalTarget == mSpinUp) {
    return eSpinButtonUp;
  }
  if (aEvent->originalTarget == mSpinDown) {
    return eSpinButtonDown;
  }
  if (aEvent->originalTarget == mSpinBox) {
    // In the case that the up/down buttons are hidden (display:none) we use
    // just the spin box element, spinning up if the pointer is over the top
    // half of the element, or down if it's over the bottom half. This is
    // important to handle since this is the state things are in for the
    // default UA style sheet. See the comment in forms.css for why.
    LayoutDeviceIntPoint absPoint = aEvent->refPoint;
    nsPoint point =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                       LayoutDeviceIntPoint::ToUntyped(absPoint),
                       mSpinBox->GetPrimaryFrame());
    if (point != nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)) {
      if (point.y < mSpinBox->GetPrimaryFrame()->GetSize().height / 2) {
        return eSpinButtonUp;
      }
      return eSpinButtonDown;
    }
  }
  return eSpinButtonNone;
}

void
nsNumberControlFrame::SpinnerStateChanged() const
{
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

bool
nsNumberControlFrame::SpinnerUpButtonIsDepressed() const
{
  return HTMLInputElement::FromContent(mContent)->
           NumberSpinnerUpButtonIsDepressed();
}

bool
nsNumberControlFrame::SpinnerDownButtonIsDepressed() const
{
  return HTMLInputElement::FromContent(mContent)->
           NumberSpinnerDownButtonIsDepressed();
}

bool
nsNumberControlFrame::IsFocused() const
{
  // Normally this depends on the state of our anonymous text control (which
  // takes focus for us), but in the case that it does not have a frame we will
  // have focus ourself.
  return mTextField->AsElement()->State().HasState(NS_EVENT_STATE_FOCUS) ||
         mContent->AsElement()->State().HasState(NS_EVENT_STATE_FOCUS);
}

void
nsNumberControlFrame::HandleFocusEvent(WidgetEvent* aEvent)
{
  if (aEvent->originalTarget != mTextField) {
    // Move focus to our text field
    HTMLInputElement::FromContent(mTextField)->Focus();
  }
}

#define STYLES_DISABLING_NATIVE_THEMING \
  NS_AUTHOR_SPECIFIED_BACKGROUND | \
  NS_AUTHOR_SPECIFIED_PADDING | \
  NS_AUTHOR_SPECIFIED_BORDER

bool
nsNumberControlFrame::ShouldUseNativeStyleForSpinner() const
{
  MOZ_ASSERT(mSpinUp && mSpinDown,
             "We should not be called when we have no spinner");

  nsIFrame* spinUpFrame = mSpinUp->GetPrimaryFrame();
  nsIFrame* spinDownFrame = mSpinDown->GetPrimaryFrame();

  return spinUpFrame &&
    spinUpFrame->StyleDisplay()->mAppearance == NS_THEME_SPINNER_UP_BUTTON &&
    !PresContext()->HasAuthorSpecifiedRules(spinUpFrame,
                                            STYLES_DISABLING_NATIVE_THEMING) &&
    spinDownFrame &&
    spinDownFrame->StyleDisplay()->mAppearance == NS_THEME_SPINNER_DOWN_BUTTON &&
    !PresContext()->HasAuthorSpecifiedRules(spinDownFrame,
                                            STYLES_DISABLING_NATIVE_THEMING);
}

void
nsNumberControlFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                               uint32_t aFilter)
{
  // Only one direct anonymous child:
  aElements.MaybeAppendElement(mOuterWrapper);
}

void
nsNumberControlFrame::UpdateForValueChange(const nsAString& aValue)
{
  if (mHandlingInputEvent) {
    // We have been called while our HTMLInputElement is processing a DOM
    // 'input' event targeted at our anonymous text control. Our
    // HTMLInputElement has taken the value of our anon text control and
    // called SetValueInternal on itself to keep its own value in sync. As a
    // result SetValueInternal has called us. In this one case we do not want
    // to update our anon text control, especially since aValue will be the
    // sanitized value, and only the internal value should be sanitized (not
    // the value shown to the user, and certainly we shouldn't change it as
    // they type).
    return;
  }
  // We need to update the value of our anonymous text control here. Note that
  // this must be its value, and not its 'value' attribute (the default value),
  // since the default value is ignored once a user types into the text
  // control.
  HTMLInputElement::FromContent(mTextField)->SetValue(aValue);
}

Element*
nsNumberControlFrame::GetPseudoElement(nsCSSPseudoElements::Type aType)
{
  if (aType == nsCSSPseudoElements::ePseudo_mozNumberWrapper) {
    return mOuterWrapper;
  }

  if (aType == nsCSSPseudoElements::ePseudo_mozNumberText) {
    return mTextField;
  }

  if (aType == nsCSSPseudoElements::ePseudo_mozNumberSpinBox) {
    MOZ_ASSERT(mSpinBox);
    return mSpinBox;
  }

  if (aType == nsCSSPseudoElements::ePseudo_mozNumberSpinUp) {
    MOZ_ASSERT(mSpinUp);
    return mSpinUp;
  }

  if (aType == nsCSSPseudoElements::ePseudo_mozNumberSpinDown) {
    MOZ_ASSERT(mSpinDown);
    return mSpinDown;
  }

  return nsContainerFrame::GetPseudoElement(aType);
}

#ifdef ACCESSIBILITY
a11y::AccType
nsNumberControlFrame::AccessibleType()
{
  return a11y::eHTMLSpinnerType;
}
#endif

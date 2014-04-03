/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNumberControlFrame.h"

#include "HTMLInputElement.h"
#include "ICUUtils.h"
#include "nsIFocusManager.h"
#include "nsIPresShell.h"
#include "nsFocusManager.h"
#include "nsFontMetrics.h"
#include "nsFormControlFrame.h"
#include "nsGkAtoms.h"
#include "nsINodeInfo.h"
#include "nsNameSpaceManager.h"
#include "nsThemeConstants.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventStates.h"
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

nscoord
nsNumberControlFrame::GetMinWidth(nsRenderingContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  if (kid) { // display:none?
    result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                  kid,
                                                  nsLayoutUtils::MIN_WIDTH);
  } else {
    result = 0;
  }

  return result;
}

nscoord
nsNumberControlFrame::GetPrefWidth(nsRenderingContext* aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  if (kid) { // display:none?
    result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                  kid,
                                                  nsLayoutUtils::PREF_WIDTH);
  } else {
    result = 0;
  }

  return result;
}

nsresult
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

  // The width of our content box, which is the available width
  // for our anonymous content:
  const nscoord contentBoxWidth = aReflowState.ComputedWidth();
  nscoord contentBoxHeight = aReflowState.ComputedHeight();

  nsIFrame* outerWrapperFrame = mOuterWrapper->GetPrimaryFrame();

  if (!outerWrapperFrame) { // display:none?
    if (contentBoxHeight == NS_INTRINSICSIZE) {
      contentBoxHeight = 0;
    }
  } else {
    NS_ASSERTION(outerWrapperFrame == mFrames.FirstChild(), "huh?");

    nsHTMLReflowMetrics wrappersDesiredSize(aReflowState);

    nsHTMLReflowState wrapperReflowState(aPresContext, aReflowState,
                                         outerWrapperFrame,
                                         nsSize(contentBoxWidth,
                                                NS_UNCONSTRAINEDSIZE));

    // offsets of wrapper frame
    nscoord xoffset = aReflowState.ComputedPhysicalBorderPadding().left +
                        wrapperReflowState.ComputedPhysicalMargin().left;
    nscoord yoffset = aReflowState.ComputedPhysicalBorderPadding().top +
                        wrapperReflowState.ComputedPhysicalMargin().top;

    nsReflowStatus childStatus;
    nsresult rv = ReflowChild(outerWrapperFrame, aPresContext,
                              wrappersDesiredSize, wrapperReflowState,
                              xoffset, yoffset, 0, childStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(NS_FRAME_IS_FULLY_COMPLETE(childStatus),
               "We gave our child unconstrained height, so it should be complete");

    nscoord wrappersMarginBoxHeight = wrappersDesiredSize.Height() +
      wrapperReflowState.ComputedPhysicalMargin().TopBottom();

    if (contentBoxHeight == NS_INTRINSICSIZE) {
      // We are intrinsically sized -- we should shrinkwrap the outer wrapper's
      // height:
      contentBoxHeight = wrappersMarginBoxHeight;

      // Make sure we obey min/max-height in the case when we're doing intrinsic
      // sizing (we get it for free when we have a non-intrinsic
      // aReflowState.ComputedHeight()).  Note that we do this before
      // adjusting for borderpadding, since mComputedMaxHeight and
      // mComputedMinHeight are content heights.
      contentBoxHeight =
        NS_CSS_MINMAX(contentBoxHeight,
                      aReflowState.ComputedMinHeight(),
                      aReflowState.ComputedMaxHeight());
    }

    // Center child vertically
    nscoord extraSpace = contentBoxHeight - wrappersMarginBoxHeight;
    yoffset += std::max(0, extraSpace / 2);

    // Place the child
    rv = FinishReflowChild(outerWrapperFrame, aPresContext,
                           wrappersDesiredSize, &wrapperReflowState,
                           xoffset, yoffset, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    aDesiredSize.SetTopAscent(wrappersDesiredSize.TopAscent() +
                              outerWrapperFrame->GetPosition().y);
  }

  aDesiredSize.Width() = contentBoxWidth +
                         aReflowState.ComputedPhysicalBorderPadding().LeftRight();
  aDesiredSize.Height() = contentBoxHeight +
                          aReflowState.ComputedPhysicalBorderPadding().TopBottom();

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  if (outerWrapperFrame) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, outerWrapperFrame);
  }

  FinishAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return NS_OK;
}

void
nsNumberControlFrame::SyncDisabledState()
{
  EventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    mTextField->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, EmptyString(),
                        true);
  } else {
    mTextField->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  }
}

nsresult
nsNumberControlFrame::AttributeChanged(int32_t  aNameSpaceID,
                                       nsIAtom* aAttribute,
                                       int32_t  aModType)
{
  // nsGkAtoms::disabled is handled by SyncDisabledState
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

void
nsNumberControlFrame::ContentStatesChanged(EventStates aStates)
{
  if (aStates.HasState(NS_EVENT_STATE_DISABLED)) {
    nsContentUtils::AddScriptRunner(new SyncDisabledStateEvent(this));
  }
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
  SetValueOfAnonTextControl(value);

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

  SyncDisabledState();

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

nsresult
nsNumberControlFrame::HandleSelectCall()
{
  return HTMLInputElement::FromContent(mTextField)->Select();
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
nsNumberControlFrame::SetValueOfAnonTextControl(const nsAString& aValue)
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

  // Init to aValue so that we set aValue as the value of our text control if
  // aValue isn't a valid number (in which case the HTMLInputElement's validity
  // state will be set to invalid) or if aValue can't be localized:
  nsAutoString localizedValue(aValue);

#ifdef ENABLE_INTL_API
  // Try and localize the value we will set:
  Decimal val = HTMLInputElement::StringToDecimal(aValue);
  if (val.isFinite()) {
    ICUUtils::LanguageTagIterForContent langTagIter(mContent);
    ICUUtils::LocalizeNumber(val.toDouble(), langTagIter, localizedValue);
  }
#endif

  // We need to update the value of our anonymous text control here. Note that
  // this must be its value, and not its 'value' attribute (the default value),
  // since the default value is ignored once a user types into the text
  // control.
  HTMLInputElement::FromContent(mTextField)->SetValue(localizedValue);
}

void
nsNumberControlFrame::GetValueOfAnonTextControl(nsAString& aValue)
{
  if (!mTextField) {
    aValue.Truncate();
    return;
  }

  HTMLInputElement::FromContent(mTextField)->GetValue(aValue);

#ifdef ENABLE_INTL_API
  // Here we need to de-localize any number typed in by the user. That is, we
  // need to convert it from the number format of the user's language, region,
  // etc. to the format that the HTML 5 spec defines to be a "valid
  // floating-point number":
  //
  //   http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#floating-point-numbers
  //
  // so that it can be parsed by functions like HTMLInputElement::
  // StringToDecimal (the HTML-5-conforming parsing function) which don't know
  // how to handle numbers that are formatted differently (for example, with
  // non-ASCII digits, with grouping separator characters or with a decimal
  // separator character other than '.').
  //
  // We need to be careful to avoid normalizing numbers that are already
  // formatted for a locale that matches the format of HTML 5's "valid
  // floating-point number" and have no grouping separator characters. (In
  // other words we want to return the number as specified by the user, not the
  // de-localized serialization, since the latter will normalize the value.)
  // For example, if the user's locale is English and the user types in "2e2"
  // then inputElement.value should be "2e2" and not "100". This is because
  // content (and tests) expect us to avoid "normalizing" the number that the
  // user types in if it's not necessary in order to make sure it conforms to
  // HTML 5's "valid floating-point number" format.
  //
  // Note that we also need to be careful when trying to avoid normalization.
  // For example, just because "1.234" _looks_ like a valid floating-point
  // number according to the spec does not mean that it should be returned
  // as-is. If the user's locale is German, then this represents the value
  // 1234, not 1.234, so it still needs to be de-localized. Alternatively, if
  // the user's locale is English and they type in "1,234" we _do_ need to
  // normalize the number to "1234" because HTML 5's valid floating-point
  // number format does not allow the ',' grouping separator. We can detect all
  // the cases where we need to convert by seeing if the locale-specific
  // parsing function understands the user input to mean the same thing as the
  // HTML-5-conforming parsing function. If so, then we should return the value
  // as-is to avoid normalization. Otherwise, we return the de-localized
  // serialization.
  ICUUtils::LanguageTagIterForContent langTagIter(mContent);
  double value = ICUUtils::ParseNumber(aValue, langTagIter);
  if (NS_finite(value) &&
      value != HTMLInputElement::StringToDecimal(aValue).toDouble()) {
    aValue.Truncate();
    aValue.AppendFloat(value);
  }
#endif
  // else, we return whatever FromContent put into aValue (the number as typed
  // in by the user)
}

bool
nsNumberControlFrame::AnonTextControlIsEmpty()
{
  if (!mTextField) {
    return true;
  }
  nsAutoString value;
  HTMLInputElement::FromContent(mTextField)->GetValue(value);
  return value.IsEmpty();
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

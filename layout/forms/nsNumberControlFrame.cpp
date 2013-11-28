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
#include "nsContentUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentList.h"
#include "nsStyleSet.h"
#include "nsIDOMMutationEvent.h"

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
        aAttribute == nsGkAtoms::readonly) {
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
nsNumberControlFrame::MakeAnonymousElement(nsIContent** aResult,
                                           nsTArray<ContentInfo>& aElements,
                                           nsIAtom* aTagName,
                                           nsCSSPseudoElements::Type aPseudoType,
                                           nsStyleContext* aParentContext)
{
  // Get the NodeInfoManager and tag necessary to create the anonymous divs.
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(aTagName, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = NS_NewHTMLElement(aResult, nodeInfo.forget(),
                                  dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we legitimately fail this assertion and need to allow
  // non-pseudo-element anonymous children, then we'll need to add a branch
  // that calls ResolveStyleFor((*aResult)->AsElement(), aParentContext)") to
  // set newStyleContext.
  NS_ASSERTION(aPseudoType != nsCSSPseudoElements::ePseudo_NotPseudoElement,
               "Expecting anonymous children to all be pseudo-elements");
  // Associate the pseudo-element with the anonymous child
  Element* resultElement = (*aResult)->AsElement();
  nsRefPtr<nsStyleContext> newStyleContext =
    PresContext()->StyleSet()->ResolvePseudoElementStyle(mContent->AsElement(),
                                                         aPseudoType,
                                                         aParentContext,
                                                         resultElement);

  if (!aElements.AppendElement(ContentInfo(resultElement, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
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

  // Initialize the text field value:
  nsAutoString value;
  HTMLInputElement::FromContent(mContent)->GetValue(value);
  mTextField->SetAttr(kNameSpaceID_None, nsGkAtoms::value, value, false);

  // If we're readonly, make sure our anonymous text control is too:
  nsAutoString readonly;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::readonly, readonly)) {
    mTextField->SetAttr(kNameSpaceID_None, nsGkAtoms::readonly, readonly, false);
  }

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

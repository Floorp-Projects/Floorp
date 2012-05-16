/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com>
 *   Vincent Lamotte <Vincent.Lamotte@ensimag.imag.fr>
 *   Laurent Dulary <Laurent.Dulary@ensimag.imag.fr>
 *   Yoan Teboul <Yoan.Teboul@ensimag.imag.fr>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMeterFrame.h"

#include "nsIDOMHTMLMeterElement.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsNodeInfoManager.h"
#include "nsINodeInfo.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsFormControlFrame.h"
#include "nsFontMetrics.h"
#include "mozilla/dom/Element.h"


nsIFrame*
NS_NewMeterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMeterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMeterFrame)

nsMeterFrame::nsMeterFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
  , mBarDiv(nsnull)
{
}

nsMeterFrame::~nsMeterFrame()
{
}

void
nsMeterFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!GetPrevContinuation(),
               "nsMeterFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContentUtils::DestroyAnonymousContent(&mBarDiv);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsMeterFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Get the NodeInfoManager and tag necessary to create the meter bar div.
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nsnull,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  // Create the div.
  nsresult rv = NS_NewHTMLElement(getter_AddRefs(mBarDiv), nodeInfo.forget(),
                                  mozilla::dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Associate ::-moz-meter-bar pseudo-element to the anonymous child.
  nsCSSPseudoElements::Type pseudoType = nsCSSPseudoElements::ePseudo_mozMeterBar;
  nsRefPtr<nsStyleContext> newStyleContext = PresContext()->StyleSet()->
    ResolvePseudoElementStyle(mContent->AsElement(), pseudoType,
                              GetStyleContext());

  if (!aElements.AppendElement(ContentInfo(mBarDiv, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsMeterFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                       PRUint32 aFilter)
{
  aElements.MaybeAppendElement(mBarDiv);
}

NS_QUERYFRAME_HEAD(nsMeterFrame)
  NS_QUERYFRAME_ENTRY(nsMeterFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)


NS_IMETHODIMP nsMeterFrame::Reflow(nsPresContext*           aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsMeterFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(mBarDiv, "Meter bar div must exist!");
  NS_ASSERTION(!GetPrevContinuation(),
               "nsMeterFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
  NS_ASSERTION(barFrame, "The meter frame should have a child with a frame!");

  ReflowBarFrame(barFrame, aPresContext, aReflowState, aStatus);

  aDesiredSize.width = aReflowState.ComputedWidth() +
                       aReflowState.mComputedBorderPadding.LeftRight();
  aDesiredSize.height = aReflowState.ComputedHeight() +
                        aReflowState.mComputedBorderPadding.TopBottom();
  aDesiredSize.height = NS_CSS_MINMAX(aDesiredSize.height,
                                      aReflowState.mComputedMinHeight,
                                      aReflowState.mComputedMaxHeight);

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, barFrame);
  FinishAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return NS_OK;
}

void
nsMeterFrame::ReflowBarFrame(nsIFrame*                aBarFrame,
                             nsPresContext*           aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  bool vertical = GetStyleDisplay()->mOrient == NS_STYLE_ORIENT_VERTICAL;
  nsHTMLReflowState reflowState(aPresContext, aReflowState, aBarFrame,
                                nsSize(aReflowState.ComputedWidth(),
                                       NS_UNCONSTRAINEDSIZE));
  nscoord size = vertical ? aReflowState.ComputedHeight()
                          : aReflowState.ComputedWidth();
  nscoord xoffset = aReflowState.mComputedBorderPadding.left;
  nscoord yoffset = aReflowState.mComputedBorderPadding.top;

  // NOTE: Introduce a new function getPosition in the content part ?
  double position, max, min, value;
  nsCOMPtr<nsIDOMHTMLMeterElement> meterElement =
    do_QueryInterface(mContent);

  meterElement->GetMax(&max);
  meterElement->GetMin(&min);
  meterElement->GetValue(&value);

  position = max - min;
  position = position != 0 ? (value - min) / position : 1;

  size = NSToCoordRound(size * position);

  if (!vertical && GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    xoffset += aReflowState.ComputedWidth() - size;
  }

  if (vertical) {
    // We want the bar to begin at the bottom.
    yoffset += aReflowState.ComputedHeight() - size;

    size -= reflowState.mComputedMargin.TopBottom() +
            reflowState.mComputedBorderPadding.TopBottom();
    size = NS_MAX(size, 0);
    reflowState.SetComputedHeight(size);
  } else {
    size -= reflowState.mComputedMargin.LeftRight() +
            reflowState.mComputedBorderPadding.LeftRight();
    size = NS_MAX(size, 0);
    reflowState.SetComputedWidth(size);
  }

  xoffset += reflowState.mComputedMargin.left;
  yoffset += reflowState.mComputedMargin.top;

  nsHTMLReflowMetrics barDesiredSize;
  ReflowChild(aBarFrame, aPresContext, barDesiredSize, reflowState, xoffset,
              yoffset, 0, aStatus);
  FinishReflowChild(aBarFrame, aPresContext, &reflowState, barDesiredSize,
                    xoffset, yoffset, 0);
}

NS_IMETHODIMP
nsMeterFrame::AttributeChanged(PRInt32  aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32  aModType)
{
  NS_ASSERTION(mBarDiv, "Meter bar div must exist!");

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::value ||
       aAttribute == nsGkAtoms::max   ||
       aAttribute == nsGkAtoms::min )) {
    nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
    NS_ASSERTION(barFrame, "The meter frame should have a child with a frame!");
    PresContext()->PresShell()->FrameNeedsReflow(barFrame,
                                                 nsIPresShell::eResize,
                                                 NS_FRAME_IS_DIRTY);
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                            aModType);
}

nsSize
nsMeterFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                              nsSize aCBSize, nscoord aAvailableWidth,
                              nsSize aMargin, nsSize aBorder,
                              nsSize aPadding, bool aShrinkWrap)
{
  nsRefPtr<nsFontMetrics> fontMet;
  NS_ENSURE_SUCCESS(nsLayoutUtils::GetFontMetricsForFrame(this,
                                                          getter_AddRefs(fontMet)),
                    nsSize(0, 0));

  nsSize autoSize;
  autoSize.height = autoSize.width = fontMet->Font().size; // 1em

  if (GetStyleDisplay()->mOrient == NS_STYLE_ORIENT_VERTICAL) {
    autoSize.height *= 5; // 5em
  } else {
    autoSize.width *= 5; // 5em
  }
  
  return autoSize;
}

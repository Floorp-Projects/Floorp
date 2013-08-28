/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLegendFrame.h"
#include "nsIContent.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsFormControlFrame.h"

nsIFrame*
NS_NewLegendFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
#ifdef DEBUG
  const nsStyleDisplay* disp = aContext->StyleDisplay();
  NS_ASSERTION(!disp->IsAbsolutelyPositionedStyle() && !disp->IsFloatingStyle(),
               "Legends should not be positioned and should not float");
#endif

  nsIFrame* f = new (aPresShell) nsLegendFrame(aContext);
  if (f) {
    f->AddStateBits(NS_BLOCK_FLOAT_MGR | NS_BLOCK_MARGIN_ROOT);
  }
  return f;
}

NS_IMPL_FRAMEARENA_HELPERS(nsLegendFrame)

nsIAtom*
nsLegendFrame::GetType() const
{
  return nsGkAtoms::legendFrame; 
}

void
nsLegendFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsBlockFrame::DestroyFrom(aDestructRoot);
}

NS_QUERYFRAME_HEAD(nsLegendFrame)
  NS_QUERYFRAME_ENTRY(nsLegendFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

NS_IMETHODIMP 
nsLegendFrame::Reflow(nsPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsLegendFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), true);
  }
  return nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

// REVIEW: We don't need to override BuildDisplayList, nsBlockFrame will honour
// our visibility setting
int32_t nsLegendFrame::GetAlign()
{
  int32_t intValue = NS_STYLE_TEXT_ALIGN_LEFT;
#ifdef IBMBIDI
  if (mParent && NS_STYLE_DIRECTION_RTL == mParent->StyleVisibility()->mDirection) {
    intValue = NS_STYLE_TEXT_ALIGN_RIGHT;
  }
#endif // IBMBIDI

  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);

  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::align);
    if (attr && attr->Type() == nsAttrValue::eEnum) {
      intValue = attr->GetEnumValue();
    }
  }
  return intValue;
}

#ifdef DEBUG
NS_IMETHODIMP
nsLegendFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Legend"), aResult);
}
#endif

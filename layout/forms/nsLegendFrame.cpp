/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLegendFrame.h"

#include "mozilla/PresShell.h"
#include "ComputedStyle.h"
#include "nsIContent.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsCheckboxRadioFrame.h"
#include "WritingModes.h"

using namespace mozilla;

nsIFrame* NS_NewLegendFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
#ifdef DEBUG
  const nsStyleDisplay* disp = aStyle->StyleDisplay();
  NS_ASSERTION(!disp->IsAbsolutelyPositionedStyle() && !disp->IsFloatingStyle(),
               "Legends should not be positioned and should not float");
#endif

  nsIFrame* f =
      new (aPresShell) nsLegendFrame(aStyle, aPresShell->GetPresContext());
  f->AddStateBits(NS_BLOCK_FORMATTING_CONTEXT_STATE_BITS);
  return f;
}

NS_IMPL_FRAMEARENA_HELPERS(nsLegendFrame)

void nsLegendFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                PostDestroyData& aPostDestroyData) {
  nsCheckboxRadioFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsBlockFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

NS_QUERYFRAME_HEAD(nsLegendFrame)
  NS_QUERYFRAME_ENTRY(nsLegendFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

void nsLegendFrame::Reflow(nsPresContext* aPresContext,
                           ReflowOutput& aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus& aStatus) {
  DO_GLOBAL_REFLOW_COUNT("nsLegendFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsCheckboxRadioFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), true);
  }
  return nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowInput,
                              aStatus);
}

int32_t nsLegendFrame::GetLogicalAlign(WritingMode aCBWM) {
  int32_t intValue = NS_STYLE_TEXT_ALIGN_START;
  nsGenericHTMLElement* content = nsGenericHTMLElement::FromNode(mContent);
  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::align);
    if (attr && attr->Type() == nsAttrValue::eEnum) {
      intValue = attr->GetEnumValue();
      switch (intValue) {
        case NS_STYLE_TEXT_ALIGN_LEFT:
          intValue = aCBWM.IsBidiLTR() ? NS_STYLE_TEXT_ALIGN_START
                                       : NS_STYLE_TEXT_ALIGN_END;
          break;
        case NS_STYLE_TEXT_ALIGN_RIGHT:
          intValue = aCBWM.IsBidiLTR() ? NS_STYLE_TEXT_ALIGN_END
                                       : NS_STYLE_TEXT_ALIGN_START;
          break;
      }
    }
  }
  return intValue;
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsLegendFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("Legend"), aResult);
}
#endif

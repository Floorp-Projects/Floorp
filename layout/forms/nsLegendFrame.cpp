/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLegendFrame.h"

#include "mozilla/dom/HTMLLegendElement.h"
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

dom::HTMLLegendElement::LegendAlignValue nsLegendFrame::GetLogicalAlign(
    WritingMode aCBWM) {
  using LegendAlignValue = dom::HTMLLegendElement::LegendAlignValue;

  auto* element = nsGenericHTMLElement::FromNode(mContent);
  if (!element) {
    return LegendAlignValue::InlineStart;
  }

  const nsAttrValue* attr = element->GetParsedAttr(nsGkAtoms::align);
  if (!attr || attr->Type() != nsAttrValue::eEnum) {
    return LegendAlignValue::InlineStart;
  }

  auto value = static_cast<LegendAlignValue>(attr->GetEnumValue());
  switch (value) {
    case LegendAlignValue::Left:
      return aCBWM.IsBidiLTR() ? LegendAlignValue::InlineStart
                               : LegendAlignValue::InlineEnd;
    case LegendAlignValue::Right:
      return aCBWM.IsBidiLTR() ? LegendAlignValue::InlineEnd
                               : LegendAlignValue::InlineStart;
    default:
      return value;
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsLegendFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("Legend"), aResult);
}
#endif

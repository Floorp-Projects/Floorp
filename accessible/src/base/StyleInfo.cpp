/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set expandtab shiftwidth=2 tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StyleInfo.h"

#include "mozilla/dom/Element.h"
#include "nsComputedDOMStyle.h"
#include "nsCSSProps.h"
#include "nsIFrame.h"

using namespace mozilla;
using namespace mozilla::a11y;

StyleInfo::StyleInfo(dom::Element* aElement, nsIPresShell* aPresShell) :
  mElement(aElement)
{
  mStyleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nullptr,
                                                         aPresShell);
}

void
StyleInfo::Display(nsAString& aValue)
{
  aValue.Truncate();
  AppendASCIItoUTF16(
    nsCSSProps::ValueToKeyword(mStyleContext->StyleDisplay()->mDisplay,
                               nsCSSProps::kDisplayKTable), aValue);
}

void
StyleInfo::TextAlign(nsAString& aValue)
{
  aValue.Truncate();
  AppendASCIItoUTF16(
    nsCSSProps::ValueToKeyword(mStyleContext->StyleText()->mTextAlign,
                               nsCSSProps::kTextAlignKTable), aValue);
}

void
StyleInfo::TextIndent(nsAString& aValue)
{
  aValue.Truncate();

  const nsStyleCoord& styleCoord =
    mStyleContext->StyleText()->mTextIndent;

  nscoord coordVal = 0;
  switch (styleCoord.GetUnit()) {
    case eStyleUnit_Coord:
      coordVal = styleCoord.GetCoordValue();
      break;

    case eStyleUnit_Percent:
    {
      nsIFrame* frame = mElement->GetPrimaryFrame();
      nsIFrame* containerFrame = frame->GetContainingBlock();
      nscoord percentageBase = containerFrame->GetContentRect().width;
      coordVal = NSCoordSaturatingMultiply(percentageBase,
                                           styleCoord.GetPercentValue());
      break;
    }

    case eStyleUnit_Null:
    case eStyleUnit_Normal:
    case eStyleUnit_Auto:
    case eStyleUnit_None:
    case eStyleUnit_Factor:
    case eStyleUnit_Degree:
    case eStyleUnit_Grad:
    case eStyleUnit_Radian:
    case eStyleUnit_Turn:
    case eStyleUnit_Integer:
    case eStyleUnit_Enumerated:
    case eStyleUnit_Calc:
      break;
  }

  aValue.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(coordVal));
  aValue.AppendLiteral("px");
}

void
StyleInfo::Margin(css::Side aSide, nsAString& aValue)
{
  aValue.Truncate();

  nscoord coordVal = mElement->GetPrimaryFrame()->GetUsedMargin().Side(aSide);
  aValue.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(coordVal));
  aValue.AppendLiteral("px");
}

void
StyleInfo::FormatColor(const nscolor& aValue, nsString& aFormattedValue)
{
  // Combine the string like rgb(R, G, B) from nscolor.
  aFormattedValue.AppendLiteral("rgb(");
  aFormattedValue.AppendInt(NS_GET_R(aValue));
  aFormattedValue.AppendLiteral(", ");
  aFormattedValue.AppendInt(NS_GET_G(aValue));
  aFormattedValue.AppendLiteral(", ");
  aFormattedValue.AppendInt(NS_GET_B(aValue));
  aFormattedValue.Append(')');
}

void
StyleInfo::FormatFontStyle(const nscoord& aValue, nsAString& aFormattedValue)
{
  nsCSSKeyword keyword =
    nsCSSProps::ValueToKeywordEnum(aValue, nsCSSProps::kFontStyleKTable);
  AppendUTF8toUTF16(nsCSSKeywords::GetStringValue(keyword), aFormattedValue);
}

void
StyleInfo::FormatTextDecorationStyle(uint8_t aValue, nsAString& aFormattedValue)
{
  nsCSSKeyword keyword =
    nsCSSProps::ValueToKeywordEnum(aValue,
                                   nsCSSProps::kTextDecorationStyleKTable);
  AppendUTF8toUTF16(nsCSSKeywords::GetStringValue(keyword), aFormattedValue);
}

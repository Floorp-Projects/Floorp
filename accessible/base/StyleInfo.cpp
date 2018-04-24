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

StyleInfo::StyleInfo(dom::Element* aElement) :
  mElement(aElement)
{
  mComputedStyle =
    nsComputedDOMStyle::GetComputedStyleNoFlush(aElement, nullptr);
}

void
StyleInfo::Display(nsAString& aValue)
{
  aValue.Truncate();
  AppendASCIItoUTF16(
    nsCSSProps::ValueToKeyword(mComputedStyle->StyleDisplay()->mDisplay,
                               nsCSSProps::kDisplayKTable), aValue);
}

void
StyleInfo::TextAlign(nsAString& aValue)
{
  aValue.Truncate();
  AppendASCIItoUTF16(
    nsCSSProps::ValueToKeyword(mComputedStyle->StyleText()->mTextAlign,
                               nsCSSProps::kTextAlignKTable), aValue);
}

void
StyleInfo::TextIndent(nsAString& aValue)
{
  aValue.Truncate();

  const nsStyleCoord& styleCoord =
    mComputedStyle->StyleText()->mTextIndent;

  nscoord coordVal = 0;
  switch (styleCoord.GetUnit()) {
    case eStyleUnit_Coord:
      coordVal = styleCoord.GetCoordValue();
      aValue.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(coordVal));
      aValue.AppendLiteral("px");
      break;

    case eStyleUnit_Percent:
      aValue.AppendFloat(styleCoord.GetPercentValue() * 100);
      aValue.AppendLiteral("%");
      break;

    case eStyleUnit_Null:
    case eStyleUnit_Normal:
    case eStyleUnit_Auto:
    case eStyleUnit_None:
    case eStyleUnit_Factor:
    case eStyleUnit_Degree:
    case eStyleUnit_Grad:
    case eStyleUnit_Radian:
    case eStyleUnit_Turn:
    case eStyleUnit_FlexFraction:
    case eStyleUnit_Integer:
    case eStyleUnit_Enumerated:
    case eStyleUnit_Calc:
      aValue.AppendLiteral("0px");
      break;
  }
}

void
StyleInfo::Margin(Side aSide, nsAString& aValue)
{
  MOZ_ASSERT(mElement->GetPrimaryFrame(), " mElement->GetPrimaryFrame() needs to be valid pointer");
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
StyleInfo::FormatTextDecorationStyle(uint8_t aValue, nsAString& aFormattedValue)
{
  nsCSSKeyword keyword =
    nsCSSProps::ValueToKeywordEnum(aValue,
                                   nsCSSProps::kTextDecorationStyleKTable);
  AppendUTF8toUTF16(nsCSSKeywords::GetStringValue(keyword), aFormattedValue);
}

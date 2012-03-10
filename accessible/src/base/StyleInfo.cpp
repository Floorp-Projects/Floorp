/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2: */
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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "StyleInfo.h"

#include "mozilla/dom/Element.h"
#include "nsComputedDOMStyle.h"

using namespace mozilla;
using namespace mozilla::a11y;

StyleInfo::StyleInfo(dom::Element* aElement, nsIPresShell* aPresShell) :
  mElement(aElement)
{
  mStyleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nsnull,
                                                         aPresShell);
}

void
StyleInfo::Display(nsAString& aValue)
{
  aValue.Truncate();
  AppendASCIItoUTF16(
    nsCSSProps::ValueToKeyword(mStyleContext->GetStyleDisplay()->mDisplay,
                               nsCSSProps::kDisplayKTable), aValue);
}

void
StyleInfo::TextAlign(nsAString& aValue)
{
  aValue.Truncate();
  AppendASCIItoUTF16(
    nsCSSProps::ValueToKeyword(mStyleContext->GetStyleText()->mTextAlign,
                               nsCSSProps::kTextAlignKTable), aValue);
}

void
StyleInfo::TextIndent(nsAString& aValue)
{
  aValue.Truncate();

  const nsStyleCoord& styleCoord =
    mStyleContext->GetStyleText()->mTextIndent;

  nscoord coordVal;
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
StyleInfo::FormatTextDecorationStyle(PRUint8 aValue, nsAString& aFormattedValue)
{
  nsCSSKeyword keyword =
    nsCSSProps::ValueToKeywordEnum(aValue,
                                   nsCSSProps::kTextDecorationStyleKTable);
  AppendUTF8toUTF16(nsCSSKeywords::GetStringValue(keyword), aFormattedValue);
}

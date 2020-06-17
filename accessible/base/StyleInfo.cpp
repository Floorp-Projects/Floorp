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

StyleInfo::StyleInfo(dom::Element* aElement) : mElement(aElement) {
  mComputedStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(aElement, nullptr);
}

void StyleInfo::Display(nsAString& aValue) {
  aValue.Truncate();
  mComputedStyle->GetComputedPropertyValue(eCSSProperty_display, aValue);
}

void StyleInfo::TextAlign(nsAString& aValue) {
  aValue.Truncate();
  mComputedStyle->GetComputedPropertyValue(eCSSProperty_text_align, aValue);
}

void StyleInfo::TextIndent(nsAString& aValue) {
  aValue.Truncate();

  const auto& textIndent = mComputedStyle->StyleText()->mTextIndent;
  if (textIndent.ConvertsToLength()) {
    aValue.AppendFloat(textIndent.ToLengthInCSSPixels());
    aValue.AppendLiteral("px");
    return;
  }
  if (textIndent.ConvertsToPercentage()) {
    aValue.AppendFloat(textIndent.ToPercentage() * 100);
    aValue.AppendLiteral("%");
    return;
  }
  // FIXME: This doesn't handle calc in any meaningful way? Probably should just
  // use the Servo serialization code...
  aValue.AppendLiteral("0px");
}

void StyleInfo::Margin(Side aSide, nsAString& aValue) {
  MOZ_ASSERT(mElement->GetPrimaryFrame(),
             " mElement->GetPrimaryFrame() needs to be valid pointer");
  aValue.Truncate();

  nsIFrame* frame = mElement->GetPrimaryFrame();

  // This is here only to guarantee that we do the same as getComputedStyle
  // does, so that we don't hit precision errors in tests.
  auto& margin = frame->StyleMargin()->mMargin.Get(aSide);
  if (margin.ConvertsToLength()) {
    aValue.AppendFloat(margin.AsLengthPercentage().ToLengthInCSSPixels());
  } else {
    nscoord coordVal = frame->GetUsedMargin().Side(aSide);
    aValue.AppendFloat(CSSPixel::FromAppUnits(coordVal));
  }

  aValue.AppendLiteral("px");
}

void StyleInfo::FormatColor(const nscolor& aValue, nsString& aFormattedValue) {
  // Combine the string like rgb(R, G, B) from nscolor.
  aFormattedValue.AppendLiteral("rgb(");
  aFormattedValue.AppendInt(NS_GET_R(aValue));
  aFormattedValue.AppendLiteral(", ");
  aFormattedValue.AppendInt(NS_GET_G(aValue));
  aFormattedValue.AppendLiteral(", ");
  aFormattedValue.AppendInt(NS_GET_B(aValue));
  aFormattedValue.Append(')');
}

void StyleInfo::FormatTextDecorationStyle(uint8_t aValue,
                                          nsAString& aFormattedValue) {
  // TODO: When these are enum classes that rust also understands we should just
  // make an FFI call here.
  switch (aValue) {
    case NS_STYLE_TEXT_DECORATION_STYLE_NONE:
      return aFormattedValue.AssignASCII("-moz-none");
    case NS_STYLE_TEXT_DECORATION_STYLE_SOLID:
      return aFormattedValue.AssignASCII("solid");
    case NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE:
      return aFormattedValue.AssignASCII("double");
    case NS_STYLE_TEXT_DECORATION_STYLE_DOTTED:
      return aFormattedValue.AssignASCII("dotted");
    case NS_STYLE_TEXT_DECORATION_STYLE_DASHED:
      return aFormattedValue.AssignASCII("dashed");
    case NS_STYLE_TEXT_DECORATION_STYLE_WAVY:
      return aFormattedValue.AssignASCII("wavy");
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown decoration style");
      break;
  }
}

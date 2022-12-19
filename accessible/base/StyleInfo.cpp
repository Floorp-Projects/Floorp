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
  mComputedStyle = nsComputedDOMStyle::GetComputedStyleNoFlush(aElement);
}

already_AddRefed<nsAtom> StyleInfo::Display() {
  nsAutoCString value;
  mComputedStyle->GetComputedPropertyValue(eCSSProperty_display, value);
  RefPtr<nsAtom> atomVal = NS_Atomize(value);
  return atomVal.forget();
}

already_AddRefed<nsAtom> StyleInfo::TextAlign() {
  nsAutoCString value;
  mComputedStyle->GetComputedPropertyValue(eCSSProperty_text_align, value);
  RefPtr<nsAtom> atomVal = NS_Atomize(value);
  return atomVal.forget();
}

mozilla::LengthPercentage StyleInfo::TextIndent() {
  return mComputedStyle->StyleText()->mTextIndent;
}

CSSCoord StyleInfo::Margin(Side aSide) {
  MOZ_ASSERT(mElement->GetPrimaryFrame(),
             " mElement->GetPrimaryFrame() needs to be valid pointer");

  nsIFrame* frame = mElement->GetPrimaryFrame();

  // This is here only to guarantee that we do the same as getComputedStyle
  // does, so that we don't hit precision errors in tests.
  auto& margin = frame->StyleMargin()->mMargin.Get(aSide);
  if (margin.ConvertsToLength()) {
    return margin.AsLengthPercentage().ToLengthInCSSPixels();
  }

  nscoord coordVal = frame->GetUsedMargin().Side(aSide);
  return CSSPixel::FromAppUnits(coordVal);
}

void StyleInfo::FormatColor(const nscolor& aValue, nsAString& aFormattedValue) {
  // Combine the string like rgb(R, G, B) from nscolor.
  aFormattedValue.AppendLiteral("rgb(");
  aFormattedValue.AppendInt(NS_GET_R(aValue));
  aFormattedValue.AppendLiteral(", ");
  aFormattedValue.AppendInt(NS_GET_G(aValue));
  aFormattedValue.AppendLiteral(", ");
  aFormattedValue.AppendInt(NS_GET_B(aValue));
  aFormattedValue.Append(')');
}

already_AddRefed<nsAtom> StyleInfo::TextDecorationStyleToAtom(
    StyleTextDecorationStyle aValue) {
  // TODO: When these are enum classes that rust also understands we should just
  // make an FFI call here.
  switch (aValue) {
    case StyleTextDecorationStyle::None:
      return NS_Atomize("-moz-none");
    case StyleTextDecorationStyle::Solid:
      return NS_Atomize("solid");
    case StyleTextDecorationStyle::Double:
      return NS_Atomize("double");
    case StyleTextDecorationStyle::Dotted:
      return NS_Atomize("dotted");
    case StyleTextDecorationStyle::Dashed:
      return NS_Atomize("dashed");
    case StyleTextDecorationStyle::Wavy:
      return NS_Atomize("wavy");
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown decoration style");
      break;
  }

  return nullptr;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS parsing utility functions */

#include "ServoCSSParser.h"

#include "mozilla/AnimatedPropertyID.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/dom/Document.h"

using namespace mozilla;
using namespace mozilla::dom;

/* static */
bool ServoCSSParser::IsValidCSSColor(const nsACString& aValue) {
  return Servo_IsValidCSSColor(&aValue);
}

/* static */
bool ServoCSSParser::ComputeColor(ServoStyleSet* aStyleSet,
                                  nscolor aCurrentColor,
                                  const nsACString& aValue,
                                  nscolor* aResultColor, bool* aWasCurrentColor,
                                  css::Loader* aLoader) {
  return Servo_ComputeColor(aStyleSet ? aStyleSet->RawData() : nullptr,
                            aCurrentColor, &aValue, aResultColor,
                            aWasCurrentColor, aLoader);
}

/* static */
already_AddRefed<StyleLockedDeclarationBlock> ServoCSSParser::ParseProperty(
    nsCSSPropertyID aProperty, const nsACString& aValue,
    const ParsingEnvironment& aParsingEnvironment,
    const StyleParsingMode& aParsingMode) {
  AnimatedPropertyID property(aProperty);
  return ParseProperty(property, aValue, aParsingEnvironment, aParsingMode);
}

/* static */
already_AddRefed<StyleLockedDeclarationBlock> ServoCSSParser::ParseProperty(
    const AnimatedPropertyID& aProperty, const nsACString& aValue,
    const ParsingEnvironment& aParsingEnvironment,
    const StyleParsingMode& aParsingMode) {
  return Servo_ParseProperty(
             &aProperty, &aValue, aParsingEnvironment.mUrlExtraData,
             aParsingMode, aParsingEnvironment.mCompatMode,
             aParsingEnvironment.mLoader, aParsingEnvironment.mRuleType)
      .Consume();
}

/* static */
bool ServoCSSParser::ParseEasing(const nsACString& aValue,
                                 StyleComputedTimingFunction& aResult) {
  return Servo_ParseEasing(&aValue, &aResult);
}

/* static */
bool ServoCSSParser::ParseTransformIntoMatrix(const nsACString& aValue,
                                              bool& aContains3DTransform,
                                              gfx::Matrix4x4& aResult) {
  return Servo_ParseTransformIntoMatrix(&aValue, &aContains3DTransform,
                                        &aResult.components);
}

/* static */
bool ServoCSSParser::ParseFontShorthandForMatching(
    const nsACString& aValue, URLExtraData* aUrl, StyleFontFamilyList& aList,
    StyleFontStyle& aStyle, StyleFontStretch& aStretch,
    StyleFontWeight& aWeight, float* aSize, bool* aSmallCaps) {
  return Servo_ParseFontShorthandForMatching(
      &aValue, aUrl, &aList, &aStyle, &aStretch, &aWeight, aSize, aSmallCaps);
}

/* static */
already_AddRefed<URLExtraData> ServoCSSParser::GetURLExtraData(
    Document* aDocument) {
  MOZ_ASSERT(aDocument);
  return do_AddRef(aDocument->DefaultStyleAttrURLData());
}

/* static */ ServoCSSParser::ParsingEnvironment
ServoCSSParser::GetParsingEnvironment(Document* aDocument) {
  return {GetURLExtraData(aDocument), aDocument->GetCompatibilityMode(),
          aDocument->CSSLoader()};
}

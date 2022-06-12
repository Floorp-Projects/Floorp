/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS parsing utility functions */

#include "ServoCSSParser.h"

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
  return Servo_ComputeColor(aStyleSet ? aStyleSet->RawSet() : nullptr,
                            aCurrentColor, &aValue, aResultColor,
                            aWasCurrentColor, aLoader);
}

/* static */
already_AddRefed<RawServoDeclarationBlock> ServoCSSParser::ParseProperty(
    nsCSSPropertyID aProperty, const nsACString& aValue,
    const ParsingEnvironment& aParsingEnvironment, ParsingMode aParsingMode) {
  return Servo_ParseProperty(
             aProperty, &aValue, aParsingEnvironment.mUrlExtraData,
             aParsingMode, aParsingEnvironment.mCompatMode,
             aParsingEnvironment.mLoader, aParsingEnvironment.mRuleType)
      .Consume();
}

/* static */
bool ServoCSSParser::ParseEasing(const nsACString& aValue,
                                 nsTimingFunction& aResult) {
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
    StyleComputedFontStyleDescriptor& aStyle, float& aStretch, float& aWeight,
    float* aSize) {
  return Servo_ParseFontShorthandForMatching(&aValue, aUrl, &aList, &aStyle,
                                             &aStretch, &aWeight, aSize);
}

/* static */
already_AddRefed<URLExtraData> ServoCSSParser::GetURLExtraData(
    Document* aDocument) {
  MOZ_ASSERT(aDocument);

  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      ReferrerInfo::CreateForInternalCSSResources(aDocument);

  RefPtr<URLExtraData> url = new URLExtraData(
      aDocument->GetBaseURI(), referrerInfo, aDocument->NodePrincipal());
  return url.forget();
}

/* static */ ServoCSSParser::ParsingEnvironment
ServoCSSParser::GetParsingEnvironment(Document* aDocument) {
  return {GetURLExtraData(aDocument), aDocument->GetCompatibilityMode(),
          aDocument->CSSLoader()};
}

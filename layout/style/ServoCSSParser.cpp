/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS parsing utility functions */

#include "ServoCSSParser.h"

#include "mozilla/ServoBindings.h"
#include "nsIDocument.h"

using namespace mozilla;

/* static */ bool
ServoCSSParser::IsValidCSSColor(const nsAString& aValue)
{
  return Servo_IsValidCSSColor(&aValue);
}

/* static */ bool
ServoCSSParser::ComputeColor(ServoStyleSet* aStyleSet,
                             nscolor aCurrentColor,
                             const nsAString& aValue,
                             nscolor* aResultColor,
                             bool* aWasCurrentColor,
                             css::Loader* aLoader)
{
  return Servo_ComputeColor(aStyleSet ? aStyleSet->RawSet() : nullptr,
                            aCurrentColor, &aValue, aResultColor,
                            aWasCurrentColor, aLoader);
}

/* static */ bool
ServoCSSParser::ParseIntersectionObserverRootMargin(const nsAString& aValue,
                                                    nsCSSRect* aResult)
{
  return Servo_ParseIntersectionObserverRootMargin(&aValue, aResult);
}

/* static */ already_AddRefed<nsAtom>
ServoCSSParser::ParseCounterStyleName(const nsAString& aValue)
{
  NS_ConvertUTF16toUTF8 value(aValue);
  nsAtom* atom = Servo_ParseCounterStyleName(&value);
  return already_AddRefed<nsAtom>(atom);
}

/* static */ bool
ServoCSSParser::ParseCounterStyleDescriptor(nsCSSCounterDesc aDescriptor,
                                            const nsAString& aValue,
                                            URLExtraData* aURLExtraData,
                                            nsCSSValue& aResult)
{
  NS_ConvertUTF16toUTF8 value(aValue);
  return Servo_ParseCounterStyleDescriptor(aDescriptor, &value, aURLExtraData,
                                           &aResult);
}

/* static */ already_AddRefed<RawServoDeclarationBlock>
ServoCSSParser::ParseProperty(nsCSSPropertyID aProperty,
                              const nsAString& aValue,
                              const ParsingEnvironment& aParsingEnvironment,
                              ParsingMode aParsingMode)
{
  NS_ConvertUTF16toUTF8 value(aValue);
  return Servo_ParseProperty(aProperty,
                             &value,
                             aParsingEnvironment.mUrlExtraData,
                             aParsingMode,
                             aParsingEnvironment.mCompatMode,
                             aParsingEnvironment.mLoader).Consume();
}

/* static */ bool
ServoCSSParser::ParseEasing(const nsAString& aValue,
                            URLExtraData* aUrl,
                            nsTimingFunction& aResult)
{
  return Servo_ParseEasing(&aValue, aUrl, &aResult);
}

/* static */ bool
ServoCSSParser::ParseTransformIntoMatrix(const nsAString& aValue,
                                         bool& aContains3DTransform,
                                         RawGeckoGfxMatrix4x4& aResult)
{
  return Servo_ParseTransformIntoMatrix(&aValue,
                                        &aContains3DTransform,
                                        &aResult);
}

/* static */ bool
ServoCSSParser::ParseFontDescriptor(nsCSSFontDesc aDescID,
                                    const nsAString& aValue,
                                    URLExtraData* aUrl,
                                    nsCSSValue& aResult)
{
  return Servo_ParseFontDescriptor(aDescID, &aValue, aUrl, &aResult);
}

/* static */ bool
ServoCSSParser::ParseFontShorthandForMatching(const nsAString& aValue,
                                              URLExtraData* aUrl,
                                              RefPtr<SharedFontList>& aList,
                                              nsCSSValue& aStyle,
                                              nsCSSValue& aStretch,
                                              nsCSSValue& aWeight)
{
  return Servo_ParseFontShorthandForMatching(&aValue, aUrl, &aList,
                                             &aStyle, &aStretch, &aWeight);
}

/* static */ already_AddRefed<URLExtraData>
ServoCSSParser::GetURLExtraData(nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);

  // FIXME this is using the wrong base uri (bug 1343919)
  RefPtr<URLExtraData> url = new URLExtraData(aDocument->GetDocumentURI(),
                                              aDocument->GetDocumentURI(),
                                              aDocument->NodePrincipal());
  return url.forget();
}

/* static */ ServoCSSParser::ParsingEnvironment
ServoCSSParser::GetParsingEnvironment(nsIDocument* aDocument)
{
  return ParsingEnvironment(GetURLExtraData(aDocument),
                            aDocument->GetCompatibilityMode(),
                            aDocument->CSSLoader());
}

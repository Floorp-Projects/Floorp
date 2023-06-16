/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSFontPaletteValuesRule.h"
#include "mozilla/dom/CSSFontPaletteValuesRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

size_t CSSFontPaletteValuesRule::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  // TODO Implement this!
  return aMallocSizeOf(this);
}

StyleCssRuleType CSSFontPaletteValuesRule::Type() const {
  return StyleCssRuleType::FontPaletteValues;
}

#ifdef DEBUG
void CSSFontPaletteValuesRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_FontPaletteValuesRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

void CSSFontPaletteValuesRule::SetRawAfterClone(
    RefPtr<StyleFontPaletteValuesRule> aRaw) {
  mRawRule = std::move(aRaw);
}

/* CSSRule implementation */

void CSSFontPaletteValuesRule::GetCssText(nsACString& aCssText) const {
  Servo_FontPaletteValuesRule_GetCssText(mRawRule, &aCssText);
}

/* CSSFontPaletteValuesRule implementation */

void CSSFontPaletteValuesRule::GetName(nsACString& aNameStr) const {
  Servo_FontPaletteValuesRule_GetName(mRawRule, &aNameStr);
}

void CSSFontPaletteValuesRule::GetFontFamily(nsACString& aFamilyListStr) const {
  Servo_FontPaletteValuesRule_GetFontFamily(mRawRule, &aFamilyListStr);
}

void CSSFontPaletteValuesRule::GetBasePalette(nsACString& aPaletteStr) const {
  Servo_FontPaletteValuesRule_GetBasePalette(mRawRule, &aPaletteStr);
}

void CSSFontPaletteValuesRule::GetOverrideColors(nsACString& aColorsStr) const {
  Servo_FontPaletteValuesRule_GetOverrideColors(mRawRule, &aColorsStr);
}

// If this ever gets its own cycle-collection bits, reevaluate our IsCCLeaf
// implementation.

bool CSSFontPaletteValuesRule::IsCCLeaf() const { return Rule::IsCCLeaf(); }

/* virtual */
JSObject* CSSFontPaletteValuesRule::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CSSFontPaletteValuesRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

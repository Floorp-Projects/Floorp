/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSFontFeatureValuesRule.h"
#include "mozilla/dom/CSSFontFeatureValuesRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

size_t CSSFontFeatureValuesRule::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  // TODO Implement this!
  return aMallocSizeOf(this);
}

StyleCssRuleType CSSFontFeatureValuesRule::Type() const {
  return StyleCssRuleType::FontFeatureValues;
}

#ifdef DEBUG
void CSSFontFeatureValuesRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_FontFeatureValuesRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

void CSSFontFeatureValuesRule::SetRawAfterClone(
    RefPtr<RawServoFontFeatureValuesRule> aRaw) {
  mRawRule = std::move(aRaw);
}

/* CSSRule implementation */

void CSSFontFeatureValuesRule::GetCssText(nsACString& aCssText) const {
  Servo_FontFeatureValuesRule_GetCssText(mRawRule, &aCssText);
}

/* CSSFontFeatureValuesRule implementation */

void CSSFontFeatureValuesRule::GetFontFamily(nsACString& aFamilyListStr) {
  Servo_FontFeatureValuesRule_GetFontFamily(mRawRule, &aFamilyListStr);
}

void CSSFontFeatureValuesRule::GetValueText(nsACString& aValueText) {
  Servo_FontFeatureValuesRule_GetValueText(mRawRule, &aValueText);
}

void CSSFontFeatureValuesRule::SetFontFamily(const nsACString& aFontFamily,
                                             ErrorResult& aRv) {
  if (IsReadOnly()) {
    return;
  }

  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void CSSFontFeatureValuesRule::SetValueText(const nsACString& aValueText,
                                            ErrorResult& aRv) {
  if (IsReadOnly()) {
    return;
  }

  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

// If this ever gets its own cycle-collection bits, reevaluate our IsCCLeaf
// implementation.

bool CSSFontFeatureValuesRule::IsCCLeaf() const { return Rule::IsCCLeaf(); }

/* virtual */
JSObject* CSSFontFeatureValuesRule::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CSSFontFeatureValuesRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

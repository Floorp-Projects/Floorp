/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSFontFeatureValuesRule_h
#define mozilla_dom_CSSFontFeatureValuesRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsICSSDeclaration.h"

namespace mozilla {
namespace dom {

class CSSFontFeatureValuesRule final : public css::Rule
{
public:
  CSSFontFeatureValuesRule(RefPtr<RawServoFontFeatureValuesRule> aRawRule,
                           StyleSheet* aSheet,
                           css::Rule* aParentRule,
                           uint32_t aLine,
                           uint32_t aColumn)
    : css::Rule(aSheet, aParentRule, aLine, aColumn)
    , mRawRule(std::move(aRawRule))
  {
  }

  virtual bool IsCCLeaf() const override;

  RawServoFontFeatureValuesRule* Raw() const { return mRawRule; }

  // WebIDL interfaces
  uint16_t Type() const final { return CSSRule_Binding::FONT_FEATURE_VALUES_RULE; }
  void GetCssText(nsAString& aCssText) const override;
  void GetFontFamily(nsAString& aFamily);
  void SetFontFamily(const nsAString& aFamily, mozilla::ErrorResult& aRv);
  void GetValueText(nsAString& aValueText);
  void SetValueText(const nsAString& aValueText, mozilla::ErrorResult& aRv);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  ~CSSFontFeatureValuesRule() = default;

  RefPtr<RawServoFontFeatureValuesRule> mRawRule;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSFontFeatureValuesRule_h

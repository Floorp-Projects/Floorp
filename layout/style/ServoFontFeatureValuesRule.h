/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSFontFeatureValuesRule for stylo */

#ifndef mozilla_ServoFontFeatureValuesRule_h
#define mozilla_ServoFontFeatureValuesRule_h

#include "mozilla/dom/CSSFontFeatureValuesRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla {

class ServoFontFeatureValuesRule final : public dom::CSSFontFeatureValuesRule
{
public:
  ServoFontFeatureValuesRule(RefPtr<RawServoFontFeatureValuesRule> aRawRule,
                             uint32_t aLine, uint32_t aColumn);

  RawServoFontFeatureValuesRule* Raw() const { return mRawRule; }

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const override;
  void GetFontFamily(nsAString& aFamily) final;
  void SetFontFamily(const nsAString& aFamily, mozilla::ErrorResult& aRv) final;
  void GetValueText(nsAString& aValueText) final;
  void SetValueText(const nsAString& aValueText, mozilla::ErrorResult& aRv) final;

  // Methods of mozilla::css::Rule
  already_AddRefed<css::Rule> Clone() const final;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const final;

  #ifdef DEBUG
    void List(FILE* out = stdout, int32_t aIndent = 0) const final;
  #endif

private:
  ~ServoFontFeatureValuesRule();

  RefPtr<RawServoFontFeatureValuesRule> mRawRule;
};

} // namespace mozilla

#endif // mozilla_ServoFontFeatureValuesRule_h

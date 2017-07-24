/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSFontFeatureValuesRule_h
#define mozilla_dom_CSSFontFeatureValuesRule_h

#include "mozilla/css/Rule.h"

#include "nsICSSDeclaration.h"
#include "nsIDOMCSSFontFeatureValuesRule.h"
#include "nsIDOMCSSStyleDeclaration.h"

namespace mozilla {
namespace dom {

class CSSFontFeatureValuesRule : public css::Rule
                               , public nsIDOMCSSFontFeatureValuesRule
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  virtual bool IsCCLeaf() const override;

  int32_t GetType() const final { return Rule::FONT_FEATURE_VALUES_RULE; }
  using Rule::GetType;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override = 0;

  using nsIDOMCSSFontFeatureValuesRule::SetFontFamily;
  using nsIDOMCSSFontFeatureValuesRule::SetValueText;
  // WebIDL interfaces
  uint16_t Type() const final { return nsIDOMCSSRule::FONT_FEATURE_VALUES_RULE; }
  virtual void GetCssTextImpl(nsAString& aCssText) const override = 0;
  // The XPCOM GetFontFamily is fine
  void SetFontFamily(const nsAString& aFamily, mozilla::ErrorResult& aRv);
  // The XPCOM GetValueText is fine
  void SetValueText(const nsAString& aFamily, mozilla::ErrorResult& aRv);

  virtual size_t
  SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override = 0;

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  using Rule::Rule;

  virtual ~CSSFontFeatureValuesRule() {};
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSFontFeatureValuesRule_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSFontFeatureValuesRule.h"
#include "mozilla/dom/CSSFontFeatureValuesRuleBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(CSSFontFeatureValuesRule, css::Rule)
NS_IMPL_RELEASE_INHERITED(CSSFontFeatureValuesRule, css::Rule)

// QueryInterface implementation for CSSFontFeatureValuesRule
// If this ever gets its own cycle-collection bits, reevaluate our IsCCLeaf
// implementation.
NS_INTERFACE_MAP_BEGIN(CSSFontFeatureValuesRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSFontFeatureValuesRule)
NS_INTERFACE_MAP_END_INHERITING(mozilla::css::Rule)

void
CSSFontFeatureValuesRule::SetFontFamily(const nsAString& aFamily,
                                              ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void
CSSFontFeatureValuesRule::SetValueText(const nsAString& aFamily,
                                             ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

bool
CSSFontFeatureValuesRule::IsCCLeaf() const
{
  return Rule::IsCCLeaf();
}

/* virtual */ JSObject*
CSSFontFeatureValuesRule::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto)
{
  return CSSFontFeatureValuesRuleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSSupportsRule.h"

#include "mozilla/dom/CSSSupportsRuleBinding.h"

using namespace mozilla::css;

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(CSSSupportsRule, css::ConditionRule)
NS_IMPL_RELEASE_INHERITED(CSSSupportsRule, css::ConditionRule)

// QueryInterface implementation for CSSSupportsRule
NS_INTERFACE_MAP_BEGIN(CSSSupportsRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSGroupingRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSConditionRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSSupportsRule)
NS_INTERFACE_MAP_END_INHERITING(ConditionRule)

// nsIDOMCSSGroupingRule methods
NS_IMETHODIMP
CSSSupportsRule::GetCssRules(nsIDOMCSSRuleList** aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
CSSSupportsRule::InsertRule(const nsAString & aRule, uint32_t aIndex, uint32_t* _retval)
{
  return GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
CSSSupportsRule::DeleteRule(uint32_t aIndex)
{
  return GroupRule::DeleteRule(aIndex);
}

void
CSSSupportsRule::SetConditionText(const nsAString& aConditionText,
                                  ErrorResult& aRv)
{
  aRv = SetConditionText(aConditionText);
}

/* virtual */ JSObject*
CSSSupportsRule::WrapObject(JSContext* aCx,
                            JS::Handle<JSObject*> aGivenProto)
{
  return CSSSupportsRuleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

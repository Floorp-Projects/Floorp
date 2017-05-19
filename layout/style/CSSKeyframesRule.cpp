/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSKeyframesRule.h"

#include "mozilla/dom/CSSKeyframesRuleBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(CSSKeyframesRule, css::GroupRule)
NS_IMPL_RELEASE_INHERITED(CSSKeyframesRule, css::GroupRule)

// QueryInterface implementation for CSSKeyframesRule
NS_INTERFACE_MAP_BEGIN(CSSKeyframesRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSKeyframesRule)
NS_INTERFACE_MAP_END_INHERITING(GroupRule)

NS_IMETHODIMP
CSSKeyframesRule::GetCssRules(nsIDOMCSSRuleList** aRuleList)
{
  NS_ADDREF(*aRuleList = CssRules());
  return NS_OK;
}

NS_IMETHODIMP
CSSKeyframesRule::FindRule(const nsAString& aKey,
                           nsIDOMCSSKeyframeRule** aResult)
{
  NS_IF_ADDREF(*aResult = FindRule(aKey));
  return NS_OK;
}

/* virtual */ bool
CSSKeyframesRule::UseForPresentation(nsPresContext* aPresContext,
                                     nsMediaQueryResultCacheKey& aKey)
{
  MOZ_ASSERT_UNREACHABLE("should not be called");
  return false;
}

/* virtual */ JSObject*
CSSKeyframesRule::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CSSKeyframesRuleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

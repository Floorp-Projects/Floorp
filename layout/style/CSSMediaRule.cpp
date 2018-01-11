/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSMediaRule.h"

#include "mozilla/dom/CSSMediaRuleBinding.h"
#include "mozilla/dom/MediaList.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(CSSMediaRule, css::ConditionRule)
NS_IMPL_RELEASE_INHERITED(CSSMediaRule, css::ConditionRule)

// QueryInterface implementation for CSSMediaRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSMediaRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSGroupingRule)
NS_INTERFACE_MAP_END_INHERITING(css::ConditionRule)

// nsIDOMCSSGroupingRule methods
NS_IMETHODIMP
CSSMediaRule::GetCssRules(nsIDOMCSSRuleList** aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
CSSMediaRule::InsertRule(const nsAString& aRule,
                         uint32_t aIndex, uint32_t* _retval)
{
  return GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
CSSMediaRule::DeleteRule(uint32_t aIndex)
{
  return GroupRule::DeleteRule(aIndex);
}

/* virtual */ JSObject*
CSSMediaRule::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CSSMediaRuleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

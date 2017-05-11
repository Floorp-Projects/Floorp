/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSMozDocumentRule.h"
#include "mozilla/dom/CSSMozDocumentRuleBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(CSSMozDocumentRule, css::ConditionRule)
NS_IMPL_RELEASE_INHERITED(CSSMozDocumentRule, css::ConditionRule)

// QueryInterface implementation for CSSMozDocumentRule
NS_INTERFACE_MAP_BEGIN(CSSMozDocumentRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSGroupingRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSConditionRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSMozDocumentRule)
NS_INTERFACE_MAP_END_INHERITING(ConditionRule)

// nsIDOMCSSGroupingRule methods
NS_IMETHODIMP
CSSMozDocumentRule::GetCssRules(nsIDOMCSSRuleList** aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
CSSMozDocumentRule::InsertRule(const nsAString & aRule,
                               uint32_t aIndex, uint32_t* _retval)
{
  return GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
CSSMozDocumentRule::DeleteRule(uint32_t aIndex)
{
  return GroupRule::DeleteRule(aIndex);
}

void
CSSMozDocumentRule::SetConditionText(const nsAString& aConditionText,
                                     ErrorResult& aRv)
{
  aRv = SetConditionText(aConditionText);
}

/* virtual */ JSObject*
CSSMozDocumentRule::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto)
{
  return CSSMozDocumentRuleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

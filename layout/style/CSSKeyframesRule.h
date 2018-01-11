/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSKeyframesRule_h
#define mozilla_dom_CSSKeyframesRule_h

#include "mozilla/css/GroupRule.h"
#include "mozilla/dom/CSSKeyframeRule.h"

namespace mozilla {
namespace dom {

class CSSKeyframesRule : public css::GroupRule
{
protected:
  using css::GroupRule::GroupRule;
  virtual ~CSSKeyframesRule() {}

public:
  int32_t GetType() const final { return Rule::KEYFRAMES_RULE; }

  // WebIDL interface
  uint16_t Type() const final { return CSSRuleBinding::KEYFRAMES_RULE; }
  virtual void GetName(nsAString& aName) const = 0;
  virtual void SetName(const nsAString& aName) = 0;
  virtual CSSRuleList* CssRules() = 0;
  virtual void AppendRule(const nsAString& aRule) = 0;
  virtual void DeleteRule(const nsAString& aKey) = 0;
  virtual CSSKeyframeRule* FindRule(const nsAString& aKey) = 0;

  bool UseForPresentation(nsPresContext* aPresContext,
                          nsMediaQueryResultCacheKey& aKey) final;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override = 0;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSKeyframesRule_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSSupportsRule_h
#define mozilla_dom_CSSSupportsRule_h

#include "mozilla/css/GroupRule.h"
#include "nsIDOMCSSSupportsRule.h"

namespace mozilla {
namespace dom {

class CSSSupportsRule : public css::ConditionRule
                      , public nsIDOMCSSSupportsRule
{
protected:
  using ConditionRule::ConditionRule;
  virtual ~CSSSupportsRule() {}

public:
  NS_DECL_ISUPPORTS_INHERITED

  int32_t GetType() const override { return css::Rule::SUPPORTS_RULE; }
  using Rule::GetType;

  // nsIDOMCSSGroupingRule interface
  NS_DECL_NSIDOMCSSGROUPINGRULE

  // nsIDOMCSSConditionRule interface
  NS_IMETHOD SetConditionText(const nsAString& aConditionText) override = 0;

  // nsIDOMCSSSupportsRule interface
  NS_DECL_NSIDOMCSSSUPPORTSRULE

  // WebIDL interface
  uint16_t Type() const override { return nsIDOMCSSRule::SUPPORTS_RULE; }
  // Our XPCOM GetConditionText is OK
  void SetConditionText(const nsAString& aConditionText,
                        ErrorResult& aRv) final;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSSupportsRule_h

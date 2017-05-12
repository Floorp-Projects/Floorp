/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSMozDocumentRule_h
#define mozilla_dom_CSSMozDocumentRule_h

#include "mozilla/css/GroupRule.h"
#include "nsIDOMCSSMozDocumentRule.h"

namespace mozilla {
namespace dom {

class CSSMozDocumentRule : public css::ConditionRule
                         , public nsIDOMCSSMozDocumentRule
{
protected:
  using ConditionRule::ConditionRule;
  virtual ~CSSMozDocumentRule() {}

public:
  NS_DECL_ISUPPORTS_INHERITED

  int32_t GetType() const final override { return css::Rule::DOCUMENT_RULE; }
  using Rule::GetType;

  // nsIDOMCSSGroupingRule interface
  NS_DECL_NSIDOMCSSGROUPINGRULE

  // nsIDOMCSSConditionRule interface
  NS_IMETHOD SetConditionText(const nsAString& aConditionText) override = 0;

  // nsIDOMCSSMozDocumentRule interface
  NS_DECL_NSIDOMCSSMOZDOCUMENTRULE

  // WebIDL interface
  uint16_t Type() const final override {
    return nsIDOMCSSRule::DOCUMENT_RULE;
  }
  // Our XPCOM GetConditionText is OK
  void SetConditionText(const nsAString& aConditionText,
                        ErrorResult& aRv) final;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSMozDocumentRule_h

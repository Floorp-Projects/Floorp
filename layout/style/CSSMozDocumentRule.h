/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSMozDocumentRule_h
#define mozilla_dom_CSSMozDocumentRule_h

#include "mozilla/css/GroupRule.h"
#include "mozilla/css/URLMatchingFunction.h"

namespace mozilla {
namespace dom {

class CSSMozDocumentRule : public css::ConditionRule
{
protected:
  using ConditionRule::ConditionRule;
  virtual ~CSSMozDocumentRule() {}

public:
  int32_t GetType() const final override { return css::Rule::DOCUMENT_RULE; }

  static bool Match(nsIDocument* aDoc,
                    nsIURI* aDocURI,
                    const nsACString& aDocURISpec,
                    const nsACString& aPattern,
                    css::URLMatchingFunction aUrlMatchingFunction);

  // WebIDL interface
  uint16_t Type() const final override { return CSSRuleBinding::DOCUMENT_RULE; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSMozDocumentRule_h

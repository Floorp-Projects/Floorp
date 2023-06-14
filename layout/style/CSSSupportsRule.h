/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSSupportsRule_h
#define mozilla_dom_CSSSupportsRule_h

#include "mozilla/css/GroupRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla::dom {

class CSSSupportsRule final : public css::ConditionRule {
 public:
  CSSSupportsRule(RefPtr<StyleSupportsRule> aRawRule, StyleSheet* aSheet,
                  css::Rule* aParentRule, uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  StyleSupportsRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<StyleSupportsRule>);

  // WebIDL interface
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;
  void GetConditionText(nsACString& aConditionText) final;
  already_AddRefed<StyleLockedCssRules> GetOrCreateRawRules() final;

  size_t SizeOfIncludingThis(MallocSizeOf) const override;
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~CSSSupportsRule() = default;

  RefPtr<StyleSupportsRule> mRawRule;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CSSSupportsRule_h

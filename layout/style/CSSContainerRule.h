/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSContainerRule_h
#define mozilla_dom_CSSContainerRule_h

#include "mozilla/css/GroupRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla::dom {

class CSSContainerRule final : public css::ConditionRule {
 public:
  CSSContainerRule(RefPtr<RawServoContainerRule> aRawRule, StyleSheet* aSheet,
                   css::Rule* aParentRule, uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  RawServoContainerRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<RawServoContainerRule>);

  // WebIDL interface
  StyleCssRuleType Type() const override;
  // WebIDL interface
  void GetCssText(nsACString& aCssText) const final;
  void GetConditionText(nsACString& aConditionText) final;

  void GetContainerName(nsACString&) const;
  void GetContainerQuery(nsACString&) const;

  size_t SizeOfIncludingThis(MallocSizeOf) const override;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  Element* QueryContainerFor(const Element&) const;

 private:
  virtual ~CSSContainerRule();

  RefPtr<RawServoContainerRule> mRawRule;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CSSContainerRule_h

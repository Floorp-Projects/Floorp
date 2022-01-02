/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSLayerRule_h
#define mozilla_dom_CSSLayerRule_h

#include "mozilla/css/GroupRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla::dom {

class CSSLayerRule : public css::GroupRule {
 public:
  CSSLayerRule(RefPtr<RawServoLayerRule> aRawRule, StyleSheet* aSheet,
               css::Rule* aParentRule, uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  RawServoLayerRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<RawServoLayerRule>);

  // WebIDL interface
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;

  size_t SizeOfIncludingThis(MallocSizeOf) const override;
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;

 private:
  ~CSSLayerRule() = default;

  RefPtr<RawServoLayerRule> mRawRule;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CSSLayerRule_h

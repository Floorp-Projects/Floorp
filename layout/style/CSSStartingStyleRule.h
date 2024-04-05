/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSSStaringStyleRule_h___
#define CSSStaringStyleRule_h___

#include "mozilla/css/GroupRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla::dom {

class CSSStartingStyleRule final : public css::GroupRule {
 public:
  CSSStartingStyleRule(RefPtr<StyleStartingStyleRule> aRawRule,
                       StyleSheet* aSheet, css::Rule* aParentRule,
                       uint32_t aLine, uint32_t aColumn)
      : css::GroupRule(aSheet, aParentRule, aLine, aColumn),
        mRawRule(std::move(aRawRule)) {}

  NS_DECL_ISUPPORTS_INHERITED

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  StyleStartingStyleRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<StyleStartingStyleRule> aRaw) {
    mRawRule = std::move(aRaw);
    css::GroupRule::DidSetRawAfterClone();
  }

  already_AddRefed<StyleLockedCssRules> GetOrCreateRawRules() final;

  // WebIDL interface
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;

 private:
  ~CSSStartingStyleRule() = default;

  RefPtr<StyleStartingStyleRule> mRawRule;
};

}  // namespace mozilla::dom

#endif

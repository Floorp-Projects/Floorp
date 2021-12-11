/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSScrollTimelineRule_h
#define mozilla_dom_CSSScrollTimelineRule_h

#include "mozilla/css/Rule.h"

namespace mozilla::dom {

class CSSScrollTimelineRule final : public css::Rule {
 public:
  CSSScrollTimelineRule(RefPtr<RawServoScrollTimelineRule> aRawRule,
                        StyleSheet* aSheet, css::Rule* aParentRule,
                        uint32_t aLine, uint32_t aColumn)
      : css::Rule(aSheet, aParentRule, aLine, aColumn),
        mRawRule(std::move(aRawRule)) {}

  const RawServoScrollTimelineRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<RawServoScrollTimelineRule> aRaw);

  StyleCssRuleType Type() const final;
  // WebIDL interface: CSSRule
  void GetCssText(nsACString& aCssText) const final;

  // WebIDL interface: CSSScrollTimelineRule
  void GetName(nsAString& aName) const;
  void GetSource(nsString& aSource) const;
  void GetOrientation(nsString& aOrientation) const;
  void GetScrollOffsets(nsString& aScrollOffsets) const;

  // Methods of mozilla::css::Rule
  bool IsCCLeaf() const final;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

 private:
  virtual ~CSSScrollTimelineRule() = default;
  CSSScrollTimelineRule(const CSSScrollTimelineRule&) = delete;

  RefPtr<RawServoScrollTimelineRule> mRawRule;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CSSScrollTimelineRule_h

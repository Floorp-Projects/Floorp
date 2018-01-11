/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoKeyframesRule_h
#define mozilla_ServoKeyframesRule_h

#include "mozilla/dom/CSSKeyframesRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla {

class ServoKeyframeList;

class ServoKeyframesRule final : public dom::CSSKeyframesRule
{
public:
  ServoKeyframesRule(RefPtr<RawServoKeyframesRule> aRawRule,
                     uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServoKeyframesRule,
                                           dom::CSSKeyframesRule)
  bool IsCCLeaf() const final;

  already_AddRefed<css::Rule> Clone() const final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif
  void SetStyleSheet(StyleSheet* aSheet) final;

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final;
  void GetName(nsAString& aName) const final;
  void SetName(const nsAString& aName) final;
  dom::CSSRuleList* CssRules() final;
  void AppendRule(const nsAString& aRule) final;
  void DeleteRule(const nsAString& aKey) final;
  dom::CSSKeyframeRule* FindRule(const nsAString& aKey) final;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

private:
  uint32_t FindRuleIndexForKey(const nsAString& aKey);

  template<typename Func>
  void UpdateRule(Func aCallback);

  virtual ~ServoKeyframesRule();

  RefPtr<RawServoKeyframesRule> mRawRule;
  RefPtr<ServoKeyframeList> mKeyframeList; // lazily constructed
};

} // namespace mozilla

#endif // mozilla_ServoKeyframesRule_h

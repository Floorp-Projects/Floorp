/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSKeyframesRule_h
#define mozilla_dom_CSSKeyframesRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/dom/CSSKeyframeRule.h"

namespace mozilla {
namespace dom {

class CSSKeyframeList;

class CSSKeyframesRule final : public css::Rule
{
public:
  CSSKeyframesRule(RefPtr<RawServoKeyframesRule> aRawRule,
                   StyleSheet* aSheet,
                   css::Rule* aParentRule,
                   uint32_t aLine,
                   uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CSSKeyframesRule,
                                           css::Rule)

  bool IsCCLeaf() const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  void DropSheetReference() final;

  // WebIDL interface
  uint16_t Type() const final { return CSSRule_Binding::KEYFRAMES_RULE; }
  void GetCssText(nsAString& aCssText) const final;
  void GetName(nsAString& aName) const;
  void SetName(const nsAString& aName);
  CSSRuleList* CssRules();
  void AppendRule(const nsAString& aRule);
  void DeleteRule(const nsAString& aKey);
  CSSKeyframeRule* FindRule(const nsAString& aKey);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

private:
  uint32_t FindRuleIndexForKey(const nsAString& aKey);

  template<typename Func>
  void UpdateRule(Func aCallback);

  virtual ~CSSKeyframesRule();

  RefPtr<RawServoKeyframesRule> mRawRule;
  RefPtr<CSSKeyframeList> mKeyframeList; // lazily constructed
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSKeyframesRule_h

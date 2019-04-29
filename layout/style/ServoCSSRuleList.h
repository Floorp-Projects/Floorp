/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSRuleList for stylo */

#ifndef mozilla_ServoCSSRuleList_h
#define mozilla_ServoCSSRuleList_h

#include "mozilla/ServoBindingTypes.h"
#include "mozilla/dom/CSSRuleList.h"
#include "nsDataHashtable.h"

namespace mozilla {

namespace dom {
class CSSStyleRule;
}  // namespace dom
class StyleSheet;
namespace css {
class GroupRule;
class Rule;
}  // namespace css

class ServoCSSRuleList final : public dom::CSSRuleList {
 public:
  ServoCSSRuleList(already_AddRefed<ServoCssRules> aRawRules,
                   StyleSheet* aSheet, css::GroupRule* aParentRule);
  css::GroupRule* GetParentRule() const { return mParentRule; }
  void DropSheetReference();
  void DropParentRuleReference();

  void DropReferences() {
    DropSheetReference();
    DropParentRuleReference();
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServoCSSRuleList, dom::CSSRuleList)

  StyleSheet* GetParentObject() final { return mStyleSheet; }

  css::Rule* IndexedGetter(uint32_t aIndex, bool& aFound) final;
  uint32_t Length() final { return mRules.Length(); }

  css::Rule* GetRule(uint32_t aIndex);
  nsresult InsertRule(const nsAString& aRule, uint32_t aIndex);
  nsresult DeleteRule(uint32_t aIndex);

  uint16_t GetDOMCSSRuleType(uint32_t aIndex) const;

 private:
  virtual ~ServoCSSRuleList();

  // XXX Is it possible to have an address lower than or equal to 255?
  //     Is it possible to have more than 255 CSS rule types?
  static const uintptr_t kMaxRuleType = UINT8_MAX;

  static uintptr_t CastToUint(css::Rule* aPtr) {
    return reinterpret_cast<uintptr_t>(aPtr);
  }
  static css::Rule* CastToPtr(uintptr_t aInt) {
    MOZ_ASSERT(aInt > kMaxRuleType);
    return reinterpret_cast<css::Rule*>(aInt);
  }

  template <typename Func>
  void EnumerateInstantiatedRules(Func aCallback);

  void DropAllRules();

  bool IsReadOnly() const;

  // mStyleSheet may be nullptr when it drops the reference to us.
  StyleSheet* mStyleSheet = nullptr;
  // mParentRule is nullptr if it isn't a nested rule list.
  css::GroupRule* mParentRule = nullptr;
  RefPtr<ServoCssRules> mRawRules;
  // Array stores either a number indicating rule type, or a pointer to
  // css::Rule. If the value is less than kMaxRuleType, the given rule
  // instance has not been constructed, and the value means the type
  // of the rule. Otherwise, it is a pointer.
  nsTArray<uintptr_t> mRules;
};

}  // namespace mozilla

#endif  // mozilla_ServoCSSRuleList_h

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

namespace mozilla {

class ServoStyleSheet;
namespace css {
class Rule;
} // namespace css

class ServoCSSRuleList final : public CSSRuleList
{
public:
  ServoCSSRuleList(ServoStyleSheet* aStyleSheet,
                   already_AddRefed<ServoCssRules> aRawRules);

  ServoStyleSheet* GetParentObject() final { return mStyleSheet; }

  nsIDOMCSSRule* IndexedGetter(uint32_t aIndex, bool& aFound) final;
  uint32_t Length() final { return mRules.Length(); }

  void DropReference();

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

  template<typename Func>
  void EnumerateInstantiatedRules(Func aCallback);

  // mStyleSheet may be nullptr when it drops the reference to us.
  ServoStyleSheet* mStyleSheet;
  RefPtr<ServoCssRules> mRawRules;
  // Array stores either a number indicating rule type, or a pointer to
  // css::Rule. If the value is less than kMaxRuleType, the given rule
  // instance has not been constructed, and the value means the type
  // of the rule. Otherwise, it is a pointer.
  nsTArray<uintptr_t> mRules;
};

} // namespace mozilla

#endif // mozilla_ServoCSSRuleList_h

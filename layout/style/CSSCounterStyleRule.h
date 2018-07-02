/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSCounterStyleRule_h
#define mozilla_CSSCounterStyleRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

struct RawServoCounterStyleRule;

namespace mozilla {
namespace dom {

class CSSCounterStyleRule final : public css::Rule
{
public:
  CSSCounterStyleRule(already_AddRefed<RawServoCounterStyleRule> aRawRule,
                      StyleSheet* aSheet,
                      css::Rule* aParentRule,
                      uint32_t aLine,
                      uint32_t aColumn)
    : css::Rule(aSheet, aParentRule, aLine, aColumn)
    , mRawRule(std::move(aRawRule))
  {
  }

private:
  CSSCounterStyleRule(const CSSCounterStyleRule& aCopy) = delete;
  ~CSSCounterStyleRule() = default;

public:
  bool IsCCLeaf() const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  // WebIDL interface
  uint16_t Type() const override;
  void GetCssText(nsAString& aCssText) const override;
  void GetName(nsAString& aName);
  void SetName(const nsAString& aName);
#define CSS_COUNTER_DESC(name_, method_) \
  void Get##method_(nsAString& aValue); \
  void Set##method_(const nsAString& aValue);
#include "nsCSSCounterDescList.h"
#undef CSS_COUNTER_DESC

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

private:
  RefPtr<RawServoCounterStyleRule> mRawRule;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_CSSCounterStyleRule_h

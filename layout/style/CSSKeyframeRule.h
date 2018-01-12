/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSKeyframeRule_h
#define mozilla_dom_CSSKeyframeRule_h

#include "mozilla/css/Rule.h"

namespace mozilla {
namespace dom {

class CSSKeyframeRule : public css::Rule
{
protected:
  using css::Rule::Rule;
  virtual ~CSSKeyframeRule() {}

public:
  int32_t GetType() const final { return Rule::KEYFRAME_RULE; }
  bool IsCCLeaf() const override { return Rule::IsCCLeaf(); }

  // WebIDL interface
  uint16_t Type() const final { return CSSRuleBinding::KEYFRAME_RULE; }
  virtual void GetKeyText(nsAString& aKey) = 0;
  virtual void SetKeyText(const nsAString& aKey) = 0;
  virtual nsICSSDeclaration* Style() = 0;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override = 0;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSKeyframeRule_h

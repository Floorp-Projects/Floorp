/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSKeyframeRule_h
#define mozilla_dom_CSSKeyframeRule_h

#include "mozilla/css/Rule.h"
#include "nsIDOMCSSKeyframeRule.h"

namespace mozilla {
namespace dom {

class CSSKeyframeRule : public css::Rule
                      , public nsIDOMCSSKeyframeRule
{
protected:
  using css::Rule::Rule;
  virtual ~CSSKeyframeRule() {}

public:
  NS_DECL_ISUPPORTS_INHERITED

  int32_t GetType() const final { return Rule::KEYFRAME_RULE; }
  using Rule::GetType;
  bool IsCCLeaf() const override { return Rule::IsCCLeaf(); }

  // nsIDOMCSSKeyframeRule
  NS_IMETHOD GetStyle(nsIDOMCSSStyleDeclaration** aStyle) final;

  // WebIDL interface
  uint16_t Type() const final { return nsIDOMCSSRule::KEYFRAME_RULE; }
  // The XPCOM GetKeyText is fine.
  // The XPCOM SetKeyText is fine.
  virtual nsICSSDeclaration* Style() = 0;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override = 0;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSKeyframeRule_h

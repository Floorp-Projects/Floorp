/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSStyleRule for stylo */

#ifndef mozilla_ServoStyleRule_h
#define mozilla_ServoStyleRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsIDOMCSSStyleRule.h"

namespace mozilla {

class ServoStyleRule final : public css::Rule
                           , public nsIDOMCSSStyleRule
{
public:
  explicit ServoStyleRule(already_AddRefed<RawServoStyleRule> aRawRule)
    : css::Rule(0, 0)
    , mRawRule(aRawRule)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMCSSRULE
  NS_DECL_NSIDOMCSSSTYLERULE

  // Methods of mozilla::css::Rule
  int32_t GetType() const final { return css::Rule::STYLE_RULE; }
  already_AddRefed<Rule> Clone() const final;
  nsIDOMCSSRule* GetDOMRule() final { return this; }
  nsIDOMCSSRule* GetExistingDOMRule() final { return this; }
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

private:
  ~ServoStyleRule() {}

  RefPtr<RawServoStyleRule> mRawRule;
};

} // namespace mozilla

#endif // mozilla_ServoStyleRule_h

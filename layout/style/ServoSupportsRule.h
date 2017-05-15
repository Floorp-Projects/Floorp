/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSSupportsRule for stylo */

#ifndef mozilla_ServoSupportsRule_h
#define mozilla_ServoSupportsRule_h

#include "mozilla/dom/CSSSupportsRule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla {

class ServoSupportsRule final : public dom::CSSSupportsRule
{
public:
  ServoSupportsRule(RefPtr<RawServoSupportsRule> aRawRule,
                    uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED

  already_AddRefed<css::Rule> Clone() const override;
  bool UseForPresentation(nsPresContext* aPresContext,
                          nsMediaQueryResultCacheKey& aKey) final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  RawServoSupportsRule* Raw() const { return mRawRule; }

  // nsIDOMCSSConditionRule interface
  NS_DECL_NSIDOMCSSCONDITIONRULE

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const override;
  using CSSSupportsRule::SetConditionText;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override;

private:
  virtual ~ServoSupportsRule();

  RefPtr<RawServoSupportsRule> mRawRule;
};

} // namespace mozilla

#endif // mozilla_ServoSupportsRule_h

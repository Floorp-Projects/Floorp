/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSSupportsRule for stylo */

#include "mozilla/ServoSupportsRule.h"

#include "mozilla/ServoBindings.h"

using namespace mozilla::dom;

namespace mozilla {

ServoSupportsRule::ServoSupportsRule(RefPtr<RawServoSupportsRule> aRawRule)
  : CSSSupportsRule(Servo_SupportsRule_GetRules(aRawRule).Consume())
  , mRawRule(Move(aRawRule))
{
}

ServoSupportsRule::~ServoSupportsRule()
{
}

NS_IMPL_ADDREF_INHERITED(ServoSupportsRule, CSSSupportsRule)
NS_IMPL_RELEASE_INHERITED(ServoSupportsRule, CSSSupportsRule)

// QueryInterface implementation for SupportsRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServoSupportsRule)
NS_INTERFACE_MAP_END_INHERITING(CSSSupportsRule)

/* virtual */ already_AddRefed<css::Rule>
ServoSupportsRule::Clone() const
{
  // Rule::Clone is only used when CSSStyleSheetInner is cloned in
  // preparation of being mutated. However, ServoStyleSheet never clones
  // anything, so this method should never be called.
  MOZ_ASSERT_UNREACHABLE("Shouldn't be cloning ServoSupportsRule");
  return nullptr;
}

/* virtual */ bool
ServoSupportsRule::UseForPresentation(nsPresContext* aPresContext,
                                      nsMediaQueryResultCacheKey& aKey)
{
  // GroupRule::UseForPresentation is only used in nsCSSRuleProcessor,
  // so this should never be called.
  MOZ_ASSERT_UNREACHABLE("Shouldn't be calling UseForPresentation");
  return false;
}

#ifdef DEBUG
/* virtual */ void
ServoSupportsRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_SupportsRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

// nsIDOMCSSConditionRule methods

NS_IMETHODIMP
ServoSupportsRule::GetConditionText(nsAString& aConditionText)
{
  Servo_SupportsRule_GetConditionText(mRawRule, &aConditionText);
  return NS_OK;
}

NS_IMETHODIMP
ServoSupportsRule::SetConditionText(const nsAString& aConditionText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* virtual */ void
ServoSupportsRule::GetCssTextImpl(nsAString& aCssText) const
{
  Servo_SupportsRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ size_t
ServoSupportsRule::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
  const
{
  // TODO Implement this!
  return aMallocSizeOf(this);
}

} // namespace mozilla

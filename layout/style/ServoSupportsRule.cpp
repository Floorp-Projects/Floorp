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

ServoSupportsRule::ServoSupportsRule(RefPtr<RawServoSupportsRule> aRawRule,
                                     uint32_t aLine, uint32_t aColumn)
  : CSSSupportsRule(Servo_SupportsRule_GetRules(aRawRule).Consume(),
                    aLine, aColumn)
  , mRawRule(std::move(aRawRule))
{
}

ServoSupportsRule::~ServoSupportsRule()
{
}

NS_IMPL_ADDREF_INHERITED(ServoSupportsRule, CSSSupportsRule)
NS_IMPL_RELEASE_INHERITED(ServoSupportsRule, CSSSupportsRule)

// QueryInterface implementation for SupportsRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoSupportsRule)
NS_INTERFACE_MAP_END_INHERITING(CSSSupportsRule)

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

void
ServoSupportsRule::GetConditionText(nsAString& aConditionText)
{
  Servo_SupportsRule_GetConditionText(mRawRule, &aConditionText);
}

void
ServoSupportsRule::SetConditionText(const nsAString& aConditionText,
                                    ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

/* virtual */ void
ServoSupportsRule::GetCssText(nsAString& aCssText) const
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

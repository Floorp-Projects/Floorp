/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSSupportsRule.h"

#include "mozilla/css/GroupRule.h"
#include "mozilla/dom/CSSSupportsRuleBinding.h"
#include "mozilla/ServoBindings.h"

using namespace mozilla::css;

namespace mozilla {
namespace dom {

CSSSupportsRule::CSSSupportsRule(RefPtr<RawServoSupportsRule> aRawRule,
                                 StyleSheet* aSheet,
                                 css::Rule* aParentRule,
                                 uint32_t aLine,
                                 uint32_t aColumn)
  : css::ConditionRule(Servo_SupportsRule_GetRules(aRawRule).Consume(),
                       aSheet, aParentRule, aLine, aColumn)
  , mRawRule(std::move(aRawRule))
{
}

NS_IMPL_ADDREF_INHERITED(CSSSupportsRule, ConditionRule)
NS_IMPL_RELEASE_INHERITED(CSSSupportsRule, ConditionRule)

// QueryInterface implementation for SupportsRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSSupportsRule)
NS_INTERFACE_MAP_END_INHERITING(ConditionRule)

#ifdef DEBUG
/* virtual */ void
CSSSupportsRule::List(FILE* out, int32_t aIndent) const
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
CSSSupportsRule::GetConditionText(nsAString& aConditionText)
{
  Servo_SupportsRule_GetConditionText(mRawRule, &aConditionText);
}

void
CSSSupportsRule::SetConditionText(const nsAString& aConditionText,
                                  ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

/* virtual */ void
CSSSupportsRule::GetCssText(nsAString& aCssText) const
{
  Servo_SupportsRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ size_t
CSSSupportsRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  // TODO Implement this!
  return aMallocSizeOf(this);
}

/* virtual */ JSObject*
CSSSupportsRule::WrapObject(JSContext* aCx,
                            JS::Handle<JSObject*> aGivenProto)
{
  return CSSSupportsRule_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

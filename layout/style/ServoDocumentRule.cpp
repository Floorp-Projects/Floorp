/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Representation of CSSMozDocumentRule for stylo */

#include "mozilla/ServoBindings.h"
#include "mozilla/ServoDocumentRule.h"

using namespace mozilla::dom;

namespace mozilla {

ServoDocumentRule::ServoDocumentRule(RefPtr<RawServoDocumentRule> aRawRule,
                                     uint32_t aLine, uint32_t aColumn)
  : CSSMozDocumentRule(Servo_DocumentRule_GetRules(aRawRule).Consume(),
                       aLine, aColumn)
  , mRawRule(Move(aRawRule))
{
}

ServoDocumentRule::~ServoDocumentRule()
{
}

NS_IMPL_ADDREF_INHERITED(ServoDocumentRule, CSSMozDocumentRule)
NS_IMPL_RELEASE_INHERITED(ServoDocumentRule, CSSMozDocumentRule)

// QueryInterface implementation for MozDocumentRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoDocumentRule)
NS_INTERFACE_MAP_END_INHERITING(CSSMozDocumentRule)

/* virtual */ already_AddRefed<css::Rule>
ServoDocumentRule::Clone() const
{
  // Rule::Clone is only used when CSSStyleSheetInner is cloned in
  // preparation of being mutated. However, ServoStyleSheet never clones
  // anything, so this method should never be called.
  MOZ_ASSERT_UNREACHABLE("Shouldn't be cloning ServoDocumentRule");
  return nullptr;
}

/* virtual */ bool
ServoDocumentRule::UseForPresentation(nsPresContext* aPresContext,
                                      nsMediaQueryResultCacheKey& aKey)
{
  // GroupRule::UseForPresentation is only used in nsCSSRuleProcessor,
  // so this should never be called.
  MOZ_ASSERT_UNREACHABLE("Shouldn't be calling UseForPresentation");
  return false;
}

#ifdef DEBUG
/* virtual */ void
ServoDocumentRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_DocumentRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

void
ServoDocumentRule::GetConditionText(nsAString& aConditionText)
{
  Servo_DocumentRule_GetConditionText(mRawRule, &aConditionText);
}

void
ServoDocumentRule::SetConditionText(const nsAString& aConditionText,
                                    ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

/* virtual */ void
ServoDocumentRule::GetCssTextImpl(nsAString& aCssText) const
{
  Servo_DocumentRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ size_t
ServoDocumentRule::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
  const
{
  // TODO Implement this!
  return aMallocSizeOf(this);
}

} // namespace mozilla

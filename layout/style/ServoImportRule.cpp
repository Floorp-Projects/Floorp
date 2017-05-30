/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSImportRule for stylo */

#include "mozilla/ServoImportRule.h"

#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSheet.h"

namespace mozilla {

ServoImportRule::ServoImportRule(RefPtr<RawServoImportRule> aRawRule,
                                 ServoStyleSheet* aSheet,
                                 uint32_t aLine, uint32_t aColumn)
  : CSSImportRule(aLine, aColumn)
  , mRawRule(Move(aRawRule))
  , mChildSheet(aSheet)
{
}

ServoImportRule::~ServoImportRule()
{
  if (mChildSheet) {
    mChildSheet->SetOwnerRule(nullptr);
  }
}

NS_IMPL_ADDREF_INHERITED(ServoImportRule, dom::CSSImportRule)
NS_IMPL_RELEASE_INHERITED(ServoImportRule, dom::CSSImportRule)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServoImportRule,
                                   dom::CSSImportRule, mChildSheet)

// QueryInterface implementation for ServoImportRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServoImportRule)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSImportRule)

/* virtual */ already_AddRefed<css::Rule>
ServoImportRule::Clone() const
{
  // Rule::Clone is only used when CSSStyleSheetInner is cloned in
  // preparation of being mutated. However, ServoStyleSheet never clones
  // anything, so this method should never be called.
  MOZ_ASSERT_UNREACHABLE("Shouldn't be cloning ServoSupportsRule");
  return nullptr;
}

#ifdef DEBUG
/* virtual */ void
ServoImportRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_ImportRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

dom::MediaList*
ServoImportRule::Media() const
{
  return mChildSheet->Media();
}

StyleSheet*
ServoImportRule::GetStyleSheet() const
{
  return mChildSheet;
}

NS_IMETHODIMP
ServoImportRule::GetHref(nsAString& aHref)
{
  Servo_ImportRule_GetHref(mRawRule, &aHref);
  return NS_OK;
}

/* virtual */ void
ServoImportRule::GetCssTextImpl(nsAString& aCssText) const
{
  Servo_ImportRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ size_t
ServoImportRule::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // TODO Implement this!
  return aMallocSizeOf(this);
}

} // namespace mozilla

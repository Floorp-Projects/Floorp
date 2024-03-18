/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSScopeRule.h"
#include "mozilla/dom/CSSScopeRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

CSSScopeRule::CSSScopeRule(RefPtr<StyleScopeRule> aRawRule, StyleSheet* aSheet,
                           css::Rule* aParentRule, uint32_t aLine,
                           uint32_t aColumn)
    : css::GroupRule(aSheet, aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)) {}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(CSSScopeRule, css::GroupRule)

// QueryInterface implementation for SupportsRule

#ifdef DEBUG
void CSSScopeRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_ScopeRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

StyleCssRuleType CSSScopeRule::Type() const { return StyleCssRuleType::Scope; }

already_AddRefed<StyleLockedCssRules> CSSScopeRule::GetOrCreateRawRules() {
  return Servo_ScopeRule_GetRules(mRawRule).Consume();
}

void CSSScopeRule::SetRawAfterClone(RefPtr<StyleScopeRule> aRaw) {
  mRawRule = std::move(aRaw);
  css::GroupRule::DidSetRawAfterClone();
}

void CSSScopeRule::GetCssText(nsACString& aCssText) const {
  Servo_ScopeRule_GetCssText(mRawRule.get(), &aCssText);
}

void CSSScopeRule::GetStart(nsACString& aStart) const {
  Servo_ScopeRule_GetStart(mRawRule.get(), &aStart);
}

void CSSScopeRule::GetEnd(nsACString& aEnd) const {
  Servo_ScopeRule_GetEnd(mRawRule.get(), &aEnd);
}

size_t CSSScopeRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

JSObject* CSSScopeRule::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return CSSScopeRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

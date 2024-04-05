/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSStartingStyleRule.h"
#include "mozilla/dom/CSSStartingStyleRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(CSSStartingStyleRule,
                                               css::GroupRule)

// QueryInterface implementation for SupportsRule

#ifdef DEBUG
void CSSStartingStyleRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_StartingStyleRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

StyleCssRuleType CSSStartingStyleRule::Type() const {
  return StyleCssRuleType::StartingStyle;
}

already_AddRefed<StyleLockedCssRules>
CSSStartingStyleRule::GetOrCreateRawRules() {
  return Servo_StartingStyleRule_GetRules(mRawRule).Consume();
}

void CSSStartingStyleRule::GetCssText(nsACString& aCssText) const {
  Servo_StartingStyleRule_GetCssText(mRawRule.get(), &aCssText);
}

JSObject* CSSStartingStyleRule::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return CSSStartingStyleRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

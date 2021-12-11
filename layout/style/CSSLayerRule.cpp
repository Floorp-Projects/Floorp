/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSLayerRule.h"
#include "mozilla/dom/CSSLayerRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

CSSLayerRule::CSSLayerRule(RefPtr<RawServoLayerRule> aRawRule,
                           StyleSheet* aSheet, css::Rule* aParentRule,
                           uint32_t aLine, uint32_t aColumn)
    : css::GroupRule(Servo_LayerRule_GetRules(aRawRule).Consume(), aSheet,
                     aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)) {}

NS_IMPL_ADDREF_INHERITED(CSSLayerRule, GroupRule)
NS_IMPL_RELEASE_INHERITED(CSSLayerRule, GroupRule)

// QueryInterface implementation for SupportsRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSLayerRule)
NS_INTERFACE_MAP_END_INHERITING(GroupRule)

#ifdef DEBUG
void CSSLayerRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_LayerRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

StyleCssRuleType CSSLayerRule::Type() const { return StyleCssRuleType::Layer; }

void CSSLayerRule::SetRawAfterClone(RefPtr<RawServoLayerRule> aRaw) {
  mRawRule = std::move(aRaw);
  css::GroupRule::SetRawAfterClone(
      Servo_LayerRule_GetRules(mRawRule).Consume());
}

void CSSLayerRule::GetCssText(nsACString& aCssText) const {
  Servo_LayerRule_GetCssText(mRawRule.get(), &aCssText);
}

size_t CSSLayerRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

JSObject* CSSLayerRule::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return CSSLayerRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

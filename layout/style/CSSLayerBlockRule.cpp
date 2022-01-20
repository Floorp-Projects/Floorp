/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSLayerBlockRule.h"
#include "mozilla/dom/CSSLayerBlockRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

CSSLayerBlockRule::CSSLayerBlockRule(RefPtr<RawServoLayerBlockRule> aRawRule,
                                     StyleSheet* aSheet, css::Rule* aParentRule,
                                     uint32_t aLine, uint32_t aColumn)
    : css::GroupRule(Servo_LayerBlockRule_GetRules(aRawRule).Consume(), aSheet,
                     aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)) {}

NS_IMPL_ADDREF_INHERITED(CSSLayerBlockRule, GroupRule)
NS_IMPL_RELEASE_INHERITED(CSSLayerBlockRule, GroupRule)

// QueryInterface implementation for SupportsRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSLayerBlockRule)
NS_INTERFACE_MAP_END_INHERITING(GroupRule)

#ifdef DEBUG
void CSSLayerBlockRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_LayerBlockRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

StyleCssRuleType CSSLayerBlockRule::Type() const {
  return StyleCssRuleType::LayerBlock;
}

void CSSLayerBlockRule::SetRawAfterClone(RefPtr<RawServoLayerBlockRule> aRaw) {
  mRawRule = std::move(aRaw);
  css::GroupRule::SetRawAfterClone(
      Servo_LayerBlockRule_GetRules(mRawRule).Consume());
}

void CSSLayerBlockRule::GetCssText(nsACString& aCssText) const {
  Servo_LayerBlockRule_GetCssText(mRawRule.get(), &aCssText);
}

void CSSLayerBlockRule::GetName(nsACString& aName) const {
  Servo_LayerBlockRule_GetName(mRawRule.get(), &aName);
}

size_t CSSLayerBlockRule::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

JSObject* CSSLayerBlockRule::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return CSSLayerBlockRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

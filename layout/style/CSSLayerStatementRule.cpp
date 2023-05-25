/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSLayerStatementRule.h"
#include "mozilla/dom/CSSLayerStatementRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

CSSLayerStatementRule::CSSLayerStatementRule(
    RefPtr<StyleLayerStatementRule> aRawRule, StyleSheet* aSheet,
    css::Rule* aParentRule, uint32_t aLine, uint32_t aColumn)
    : Rule(aSheet, aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)) {}

NS_IMPL_ADDREF_INHERITED(CSSLayerStatementRule, Rule)
NS_IMPL_RELEASE_INHERITED(CSSLayerStatementRule, Rule)

// QueryInterface implementation for SupportsRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSLayerStatementRule)
NS_INTERFACE_MAP_END_INHERITING(Rule)

#ifdef DEBUG
void CSSLayerStatementRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_LayerStatementRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

StyleCssRuleType CSSLayerStatementRule::Type() const {
  return StyleCssRuleType::LayerStatement;
}

void CSSLayerStatementRule::SetRawAfterClone(
    RefPtr<StyleLayerStatementRule> aRaw) {
  mRawRule = std::move(aRaw);
}

void CSSLayerStatementRule::GetCssText(nsACString& aCssText) const {
  Servo_LayerStatementRule_GetCssText(mRawRule.get(), &aCssText);
}

void CSSLayerStatementRule::GetNameList(nsTArray<nsCString>& aNames) const {
  size_t size = Servo_LayerStatementRule_GetNameCount(mRawRule.get());
  for (size_t i = 0; i < size; ++i) {
    Servo_LayerStatementRule_GetNameAt(mRawRule.get(), i,
                                       aNames.AppendElement());
  }
}

size_t CSSLayerStatementRule::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

JSObject* CSSLayerStatementRule::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return CSSLayerStatementRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

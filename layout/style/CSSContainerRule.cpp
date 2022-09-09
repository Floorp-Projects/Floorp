/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSContainerRule.h"

#include "mozilla/css/GroupRule.h"
#include "mozilla/dom/CSSContainerRuleBinding.h"
#include "mozilla/ServoBindings.h"

using namespace mozilla::css;

namespace mozilla::dom {

CSSContainerRule::CSSContainerRule(RefPtr<RawServoContainerRule> aRawRule,
                                   StyleSheet* aSheet, css::Rule* aParentRule,
                                   uint32_t aLine, uint32_t aColumn)
    : css::ConditionRule(Servo_ContainerRule_GetRules(aRawRule).Consume(),
                         aSheet, aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)) {}

CSSContainerRule::~CSSContainerRule() = default;

NS_IMPL_ADDREF_INHERITED(CSSContainerRule, ConditionRule)
NS_IMPL_RELEASE_INHERITED(CSSContainerRule, ConditionRule)

// QueryInterface implementation for ContainerRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSContainerRule)
NS_INTERFACE_MAP_END_INHERITING(ConditionRule)

#ifdef DEBUG
/* virtual */
void CSSContainerRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_ContainerRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

StyleCssRuleType CSSContainerRule::Type() const {
  return StyleCssRuleType::Container;
}

void CSSContainerRule::GetConditionText(nsACString& aConditionText) {
  Servo_ContainerRule_GetConditionText(mRawRule, &aConditionText);
}

/* virtual */
void CSSContainerRule::GetCssText(nsACString& aCssText) const {
  Servo_ContainerRule_GetCssText(mRawRule, &aCssText);
}

void CSSContainerRule::GetContainerName(nsACString& aName) const {
  Servo_ContainerRule_GetContainerName(mRawRule, &aName);
}

void CSSContainerRule::GetContainerQuery(nsACString& aQuery) const {
  Servo_ContainerRule_GetContainerQuery(mRawRule, &aQuery);
}

Element* CSSContainerRule::QueryContainerFor(const Element& aElement) const {
  return const_cast<Element*>(
      Servo_ContainerRule_QueryContainerFor(mRawRule, &aElement));
}

void CSSContainerRule::SetRawAfterClone(RefPtr<RawServoContainerRule> aRaw) {
  mRawRule = std::move(aRaw);

  css::ConditionRule::SetRawAfterClone(
      Servo_ContainerRule_GetRules(mRawRule).Consume());
}

/* virtual */
size_t CSSContainerRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  // TODO Implement this!
  return aMallocSizeOf(this);
}

/* virtual */
JSObject* CSSContainerRule::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return CSSContainerRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

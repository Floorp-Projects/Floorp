/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSScrollTimelineRule.h"
#include "mozilla/dom/CSSScrollTimelineRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

void CSSScrollTimelineRule::SetRawAfterClone(
    RefPtr<RawServoScrollTimelineRule> aRaw) {
  mRawRule = std::move(aRaw);
}

StyleCssRuleType CSSScrollTimelineRule::Type() const {
  return StyleCssRuleType::ScrollTimeline;
}

/* virtual */
void CSSScrollTimelineRule::GetCssText(nsACString& aCssText) const {
  Servo_ScrollTimelineRule_GetCssText(mRawRule, &aCssText);
}

void CSSScrollTimelineRule::GetName(nsAString& aName) const {
  nsAtom* name = Servo_ScrollTimelineRule_GetName(mRawRule);
  name->ToString(aName);
}

void CSSScrollTimelineRule::GetSource(nsString& aSource) const {
  Servo_ScrollTimelineRule_GetSource(mRawRule, &aSource);
}

void CSSScrollTimelineRule::GetOrientation(nsString& aOrientation) const {
  Servo_ScrollTimelineRule_GetOrientationAsString(mRawRule, &aOrientation);
}

void CSSScrollTimelineRule::GetScrollOffsets(nsString& aScrollOffsets) const {
  Servo_ScrollTimelineRule_GetScrollOffsets(mRawRule, &aScrollOffsets);
}

bool CSSScrollTimelineRule::IsCCLeaf() const { return Rule::IsCCLeaf(); }

/* virtual */
size_t CSSScrollTimelineRule::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

/* virtual */
JSObject* CSSScrollTimelineRule::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return CSSScrollTimelineRule_Binding::Wrap(aCx, this, aGivenProto);
}

#ifdef DEBUG
/* virtual */
void CSSScrollTimelineRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_ScrollTimelineRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

}  // namespace mozilla::dom

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSPropertyRule.h"
#include "mozilla/dom/CSSPropertyRuleBinding.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

bool CSSPropertyRule::IsCCLeaf() const { return Rule::IsCCLeaf(); }

void CSSPropertyRule::SetRawAfterClone(RefPtr<StylePropertyRule> aRaw) {
  mRawRule = std::move(aRaw);
}

/* virtual */
JSObject* CSSPropertyRule::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return CSSPropertyRule_Binding::Wrap(aCx, this, aGivenProto);
}

#ifdef DEBUG
void CSSPropertyRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_PropertyRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

size_t CSSPropertyRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  // TODO Implement this!
  return aMallocSizeOf(this);
}

StyleCssRuleType CSSPropertyRule::Type() const {
  return StyleCssRuleType::Property;
}

/* CSSRule implementation */

void CSSPropertyRule::GetCssText(nsACString& aCssText) const {
  Servo_PropertyRule_GetCssText(mRawRule, &aCssText);
}

/* CSSPropertyRule implementation */

void CSSPropertyRule::GetName(nsACString& aNameStr) const {
  Servo_PropertyRule_GetName(mRawRule, &aNameStr);
}

void CSSPropertyRule::GetSyntax(nsACString& aSyntaxStr) const {
  Servo_PropertyRule_GetSyntax(mRawRule, &aSyntaxStr);
}

bool CSSPropertyRule::Inherits() const {
  return Servo_PropertyRule_GetInherits(mRawRule);
}

void CSSPropertyRule::GetInitialValue(nsACString& aInitialValueStr) const {
  bool found = Servo_PropertyRule_GetInitialValue(mRawRule, &aInitialValueStr);
  if (!found) {
    aInitialValueStr.SetIsVoid(true);
  }
}

}  // namespace mozilla::dom

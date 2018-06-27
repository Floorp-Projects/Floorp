/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSCounterStyleRule.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/dom/CSSCounterStyleRuleBinding.h"
#include "mozilla/ServoBindings.h"
#include "nsStyleUtil.h"

namespace mozilla {
namespace dom {

bool
CSSCounterStyleRule::IsCCLeaf() const
{
  return Rule::IsCCLeaf();
}

#ifdef DEBUG
void
CSSCounterStyleRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_CounterStyleRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

uint16_t
CSSCounterStyleRule::Type() const
{
  return CSSRule_Binding::COUNTER_STYLE_RULE;
}

void
CSSCounterStyleRule::GetCssText(nsAString& aCssText) const
{
  Servo_CounterStyleRule_GetCssText(mRawRule, &aCssText);
}

void
CSSCounterStyleRule::GetName(nsAString& aName)
{
  aName.Truncate();
  nsAtom* name = Servo_CounterStyleRule_GetName(mRawRule);
  nsDependentAtomString nameStr(name);
  nsStyleUtil::AppendEscapedCSSIdent(nameStr, aName);
}

void
CSSCounterStyleRule::SetName(const nsAString& aName)
{
  NS_ConvertUTF16toUTF8 name(aName);
  if (Servo_CounterStyleRule_SetName(mRawRule, &name)) {
    if (StyleSheet* sheet = GetStyleSheet()) {
      sheet->RuleChanged(this);
    }
  }
}

#define CSS_COUNTER_DESC(name_, method_)                        \
  void                                                          \
  CSSCounterStyleRule::Get##method_(nsAString& aValue)          \
  {                                                             \
    aValue.Truncate();                                          \
    Servo_CounterStyleRule_GetDescriptorCssText(                \
      mRawRule, eCSSCounterDesc_##method_, &aValue);            \
  }                                                             \
  void                                                          \
  CSSCounterStyleRule::Set##method_(const nsAString& aValue)    \
  {                                                             \
    NS_ConvertUTF16toUTF8 value(aValue);                        \
    if (Servo_CounterStyleRule_SetDescriptor(                   \
          mRawRule, eCSSCounterDesc_##method_, &value)) {       \
      if (StyleSheet* sheet = GetStyleSheet()) {                \
        sheet->RuleChanged(this);                               \
      }                                                         \
    }                                                           \
  }
#include "nsCSSCounterDescList.h"
#undef CSS_COUNTER_DESC

/* virtual */ size_t
CSSCounterStyleRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

/* virtual */ JSObject*
CSSCounterStyleRule::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return CSSCounterStyleRule_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

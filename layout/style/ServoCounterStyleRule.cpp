/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoCounterStyleRule.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/dom/CSSCounterStyleRuleBinding.h"
#include "mozilla/ServoBindings.h"
#include "nsStyleUtil.h"

namespace mozilla {

bool
ServoCounterStyleRule::IsCCLeaf() const
{
  return Rule::IsCCLeaf();
}

#ifdef DEBUG
void
ServoCounterStyleRule::List(FILE* out, int32_t aIndent) const
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
ServoCounterStyleRule::Type() const
{
  return CSSRuleBinding::COUNTER_STYLE_RULE;
}

void
ServoCounterStyleRule::GetCssText(nsAString& aCssText) const
{
  Servo_CounterStyleRule_GetCssText(mRawRule, &aCssText);
}

void
ServoCounterStyleRule::GetName(nsAString& aName)
{
  aName.Truncate();
  nsAtom* name = Servo_CounterStyleRule_GetName(mRawRule);
  nsDependentAtomString nameStr(name);
  nsStyleUtil::AppendEscapedCSSIdent(nameStr, aName);
}

void
ServoCounterStyleRule::SetName(const nsAString& aName)
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
  ServoCounterStyleRule::Get##method_(nsAString& aValue)        \
  {                                                             \
    aValue.Truncate();                                          \
    Servo_CounterStyleRule_GetDescriptorCssText(                \
      mRawRule, eCSSCounterDesc_##method_, &aValue);            \
  }                                                             \
  void                                                          \
  ServoCounterStyleRule::Set##method_(const nsAString& aValue)  \
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
ServoCounterStyleRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

/* virtual */ JSObject*
ServoCounterStyleRule::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto)
{
  return CSSCounterStyleRuleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace mozilla

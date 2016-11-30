/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSRuleList for stylo */

#include "mozilla/ServoCSSRuleList.h"

#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleRule.h"

namespace mozilla {

ServoCSSRuleList::ServoCSSRuleList(ServoStyleSheet* aStyleSheet,
                                   already_AddRefed<ServoCssRules> aRawRules)
  : mStyleSheet(aStyleSheet)
  , mRawRules(aRawRules)
{
  Servo_CssRules_ListTypes(mRawRules, &mRules);
  // XXX We may want to eagerly create object for import rule, so that
  //     we don't lose the reference to child stylesheet when our own
  //     stylesheet goes away.
}

css::Rule*
ServoCSSRuleList::GetRule(uint32_t aIndex)
{
  uintptr_t rule = mRules[aIndex];
  if (rule <= kMaxRuleType) {
    RefPtr<css::Rule> ruleObj = nullptr;
    switch (rule) {
      case nsIDOMCSSRule::STYLE_RULE: {
        ruleObj = new ServoStyleRule(
          Servo_CssRules_GetStyleRuleAt(mRawRules, aIndex).Consume());
        break;
      }
      case nsIDOMCSSRule::MEDIA_RULE:
      case nsIDOMCSSRule::FONT_FACE_RULE:
      case nsIDOMCSSRule::KEYFRAMES_RULE:
      case nsIDOMCSSRule::NAMESPACE_RULE:
        // XXX create corresponding rules
      default:
        NS_ERROR("stylo: not implemented yet");
        return nullptr;
    }
    ruleObj->SetStyleSheet(mStyleSheet);
    rule = CastToUint(ruleObj.forget().take());
    mRules[aIndex] = rule;
  }
  return CastToPtr(rule);
}

nsIDOMCSSRule*
ServoCSSRuleList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  if (aIndex >= mRules.Length()) {
    aFound = false;
    return nullptr;
  }
  aFound = true;
  if (css::Rule* rule = GetRule(aIndex)) {
    return rule->GetDOMRule();
  }
  return nullptr;
}

template<typename Func>
void
ServoCSSRuleList::EnumerateInstantiatedRules(Func aCallback)
{
  for (uintptr_t rule : mRules) {
    if (rule > kMaxRuleType) {
      aCallback(CastToPtr(rule));
    }
  }
}

void
ServoCSSRuleList::DropReference()
{
  mStyleSheet = nullptr;
  EnumerateInstantiatedRules([](css::Rule* rule) {
    rule->SetStyleSheet(nullptr);
  });
}

nsresult
ServoCSSRuleList::InsertRule(const nsAString& aRule, uint32_t aIndex)
{
  NS_ConvertUTF16toUTF8 rule(aRule);
  // XXX This needs to actually reflect whether it is nested when we
  // support using CSSRuleList in CSSGroupingRules.
  bool nested = false;
  uint16_t type;
  nsresult rv = Servo_CssRules_InsertRule(mRawRules, mStyleSheet->RawSheet(),
                                          &rule, aIndex, nested, &type);
  if (!NS_FAILED(rv)) {
    mRules.InsertElementAt(aIndex, type);
  }
  return rv;
}

nsresult
ServoCSSRuleList::DeleteRule(uint32_t aIndex)
{
  nsresult rv = Servo_CssRules_DeleteRule(mRawRules, aIndex);
  if (!NS_FAILED(rv)) {
    mRules.RemoveElementAt(aIndex);
  }
  return rv;
}

ServoCSSRuleList::~ServoCSSRuleList()
{
  EnumerateInstantiatedRules([](css::Rule* rule) { rule->Release(); });
}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSRuleList for stylo */

#include "mozilla/ServoCSSRuleList.h"

#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleRule.h"
#include "mozilla/ServoMediaRule.h"
#include "mozilla/ServoNamespaceRule.h"

namespace mozilla {

ServoCSSRuleList::ServoCSSRuleList(already_AddRefed<ServoCssRules> aRawRules)
  : mRawRules(aRawRules)
{
  Servo_CssRules_ListTypes(mRawRules, &mRules);
  // XXX We may want to eagerly create object for import rule, so that
  //     we don't lose the reference to child stylesheet when our own
  //     stylesheet goes away.
}

// QueryInterface implementation for ServoCSSRuleList
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServoCSSRuleList)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSRuleList)

NS_IMPL_ADDREF_INHERITED(ServoCSSRuleList, dom::CSSRuleList)
NS_IMPL_RELEASE_INHERITED(ServoCSSRuleList, dom::CSSRuleList)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoCSSRuleList)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ServoCSSRuleList)
  tmp->DropAllRules();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(dom::CSSRuleList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoCSSRuleList,
                                                  dom::CSSRuleList)
  tmp->EnumerateInstantiatedRules([&](css::Rule* aRule) {
    if (!aRule->IsCCLeaf()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRules[i]");
      cb.NoteXPCOMChild(aRule);
    }
  });
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void
ServoCSSRuleList::SetParentRule(css::GroupRule* aParentRule)
{
  mParentRule = aParentRule;
  EnumerateInstantiatedRules([aParentRule](css::Rule* rule) {
    rule->SetParentRule(aParentRule);
  });
}

void
ServoCSSRuleList::SetStyleSheet(StyleSheet* aStyleSheet)
{
  mStyleSheet = aStyleSheet ? aStyleSheet->AsServo() : nullptr;
  EnumerateInstantiatedRules([aStyleSheet](css::Rule* rule) {
    rule->SetStyleSheet(aStyleSheet);
  });
}

css::Rule*
ServoCSSRuleList::GetRule(uint32_t aIndex)
{
  uintptr_t rule = mRules[aIndex];
  if (rule <= kMaxRuleType) {
    RefPtr<css::Rule> ruleObj = nullptr;
    switch (rule) {
#define CASE_RULE(const_, name_)                                            \
      case nsIDOMCSSRule::const_##_RULE: {                                  \
        ruleObj = new Servo##name_##Rule(                                   \
          Servo_CssRules_Get##name_##RuleAt(mRawRules, aIndex).Consume());  \
        break;                                                              \
      }
      CASE_RULE(STYLE, Style)
      CASE_RULE(MEDIA, Media)
      CASE_RULE(NAMESPACE, Namespace)
#undef CASE_RULE
      case nsIDOMCSSRule::FONT_FACE_RULE:
      case nsIDOMCSSRule::KEYFRAMES_RULE:
        // XXX create corresponding rules
      default:
        NS_WARNING("stylo: not implemented yet");
        return nullptr;
    }
    ruleObj->SetStyleSheet(mStyleSheet);
    ruleObj->SetParentRule(mParentRule);
    rule = CastToUint(ruleObj.forget().take());
    mRules[aIndex] = rule;
  }
  return CastToPtr(rule);
}

css::Rule*
ServoCSSRuleList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  if (aIndex >= mRules.Length()) {
    aFound = false;
    return nullptr;
  }
  aFound = true;
  return GetRule(aIndex);
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

static void
DropRule(already_AddRefed<css::Rule> aRule)
{
  RefPtr<css::Rule> rule = aRule;
  rule->SetStyleSheet(nullptr);
  rule->SetParentRule(nullptr);
}

void
ServoCSSRuleList::DropAllRules()
{
  EnumerateInstantiatedRules([](css::Rule* rule) {
    DropRule(already_AddRefed<css::Rule>(rule));
  });
  mRules.Clear();
}

void
ServoCSSRuleList::DropReference()
{
  mStyleSheet = nullptr;
  mParentRule = nullptr;
  DropAllRules();
}

nsresult
ServoCSSRuleList::InsertRule(const nsAString& aRule, uint32_t aIndex)
{
  NS_ConvertUTF16toUTF8 rule(aRule);
  bool nested = !!mParentRule;
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
    uintptr_t rule = mRules[aIndex];
    if (rule > kMaxRuleType) {
      DropRule(already_AddRefed<css::Rule>(CastToPtr(rule)));
    }
    mRules.RemoveElementAt(aIndex);
  }
  return rv;
}

ServoCSSRuleList::~ServoCSSRuleList()
{
  DropAllRules();
}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSRuleList for stylo */

#include "mozilla/ServoCSSRuleList.h"

#include "mozilla/ServoBindings.h"

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

nsIDOMCSSRule*
ServoCSSRuleList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  if (aIndex >= mRules.Length()) {
    aFound = false;
    return nullptr;
  }
  aFound = true;
  uintptr_t rule = mRules[aIndex];
  if (rule <= kMaxRuleType) {
    RefPtr<css::Rule> ruleObj = nullptr;
    switch (rule) {
      case nsIDOMCSSRule::STYLE_RULE:
      case nsIDOMCSSRule::MEDIA_RULE:
      case nsIDOMCSSRule::FONT_FACE_RULE:
      case nsIDOMCSSRule::KEYFRAMES_RULE:
      case nsIDOMCSSRule::NAMESPACE_RULE:
        // XXX create corresponding rules
      default:
        MOZ_CRASH("stylo: not implemented yet");
    }
    ruleObj->SetStyleSheet(mStyleSheet);
    rule = CastToUint(ruleObj.forget().take());
    mRules[aIndex] = rule;
  }
  return CastToPtr(rule)->GetDOMRule();
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

ServoCSSRuleList::~ServoCSSRuleList()
{
  EnumerateInstantiatedRules([](css::Rule* rule) { rule->Release(); });
}

} // namespace mozilla

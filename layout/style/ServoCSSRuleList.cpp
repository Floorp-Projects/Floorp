/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSRuleList for stylo */

#include "mozilla/ServoCSSRuleList.h"

#include "mozilla/ServoBindings.h"
#include "mozilla/ServoDocumentRule.h"
#include "mozilla/ServoImportRule.h"
#include "mozilla/ServoKeyframesRule.h"
#include "mozilla/ServoMediaRule.h"
#include "mozilla/ServoNamespaceRule.h"
#include "mozilla/ServoPageRule.h"
#include "mozilla/ServoStyleRule.h"
#include "mozilla/ServoSupportsRule.h"
#include "nsCSSCounterStyleRule.h"
#include "nsCSSFontFaceRule.h"

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
      // Note about @font-face and @counter-style rule again, since
      // there is an indirect owning edge through Servo's struct that
      // FontFaceRule / CounterStyleRule in Servo owns a Gecko
      // nsCSSFontFaceRule / nsCSSCounterStyleRule object.
      auto type = aRule->Type();
      if (type == nsIDOMCSSRule::FONT_FACE_RULE ||
          type == nsIDOMCSSRule::COUNTER_STYLE_RULE) {
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRawRules[i]");
        cb.NoteXPCOMChild(aRule);
      }
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
  EnumerateInstantiatedRules([this](css::Rule* rule) {
    rule->SetStyleSheet(mStyleSheet);
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
        uint32_t line = 0, column = 0;                                      \
        RefPtr<RawServo##name_##Rule> rule =                                \
          Servo_CssRules_Get##name_##RuleAt(                                \
              mRawRules, aIndex, &line, &column                             \
          ).Consume();                                                      \
        ruleObj = new Servo##name_##Rule(rule.forget(), line, column);      \
        break;                                                              \
      }
      CASE_RULE(STYLE, Style)
      CASE_RULE(IMPORT, Import)
      CASE_RULE(KEYFRAMES, Keyframes)
      CASE_RULE(MEDIA, Media)
      CASE_RULE(NAMESPACE, Namespace)
      CASE_RULE(PAGE, Page)
      CASE_RULE(SUPPORTS, Supports)
      CASE_RULE(DOCUMENT, Document)
#undef CASE_RULE
      // For @font-face and @counter-style rules, the function returns
      // a borrowed Gecko rule object directly, so we don't need to
      // create anything here. But we still need to have the style sheet
      // and parent rule set properly.
      case nsIDOMCSSRule::FONT_FACE_RULE: {
        ruleObj = Servo_CssRules_GetFontFaceRuleAt(mRawRules, aIndex);
        break;
      }
      case nsIDOMCSSRule::COUNTER_STYLE_RULE: {
        ruleObj = Servo_CssRules_GetCounterStyleRuleAt(mRawRules, aIndex);
        break;
      }
      case nsIDOMCSSRule::KEYFRAME_RULE:
        MOZ_ASSERT_UNREACHABLE("keyframe rule cannot be here");
        return nullptr;
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
  mRawRules = nullptr;
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
  MOZ_ASSERT(mStyleSheet, "Caller must ensure that "
             "the list is not unlinked from stylesheet");
  NS_ConvertUTF16toUTF8 rule(aRule);
  bool nested = !!mParentRule;
  css::Loader* loader = nullptr;
  if (nsIDocument* doc = mStyleSheet->GetAssociatedDocument()) {
    loader = doc->CSSLoader();
  }
  uint16_t type;
  nsresult rv = Servo_CssRules_InsertRule(mRawRules, mStyleSheet->RawSheet(),
                                          &rule, aIndex, nested,
                                          loader, mStyleSheet, &type);
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

uint16_t
ServoCSSRuleList::GetRuleType(uint32_t aIndex) const
{
  uintptr_t rule = mRules[aIndex];
  if (rule <= kMaxRuleType) {
    return rule;
  }
  return CastToPtr(rule)->Type();
}

void
ServoCSSRuleList::FillStyleRuleHashtable(StyleRuleHashtable& aTable)
{
  for (uint32_t i = 0; i < mRules.Length(); i++) {
    uint16_t type = GetRuleType(i);
    if (type == nsIDOMCSSRule::STYLE_RULE) {
      ServoStyleRule* castedRule = static_cast<ServoStyleRule*>(GetRule(i));
      RawServoStyleRule* rawRule = castedRule->Raw();
      aTable.Put(rawRule, castedRule);
    } else if (type == nsIDOMCSSRule::MEDIA_RULE ||
               type == nsIDOMCSSRule::SUPPORTS_RULE ||
               type == nsIDOMCSSRule::DOCUMENT_RULE) {
      auto castedRule = static_cast<css::GroupRule*>(GetRule(i));

      // Call this method recursively on the ServoCSSRuleList in the rule.
      ServoCSSRuleList* castedRuleList = static_cast<ServoCSSRuleList*>(
        castedRule->CssRules());
      castedRuleList->FillStyleRuleHashtable(aTable);
    }
  }
}

ServoCSSRuleList::~ServoCSSRuleList()
{
  DropAllRules();
}

} // namespace mozilla

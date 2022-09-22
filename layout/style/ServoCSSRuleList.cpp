/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSRuleList for stylo */

#include "mozilla/ServoCSSRuleList.h"

#include "mozilla/dom/CSSCounterStyleRule.h"
#include "mozilla/dom/CSSFontFaceRule.h"
#include "mozilla/dom/CSSFontFeatureValuesRule.h"
#include "mozilla/dom/CSSImportRule.h"
#include "mozilla/dom/CSSLayerBlockRule.h"
#include "mozilla/dom/CSSLayerStatementRule.h"
#include "mozilla/dom/CSSKeyframesRule.h"
#include "mozilla/dom/CSSContainerRule.h"
#include "mozilla/dom/CSSMediaRule.h"
#include "mozilla/dom/CSSMozDocumentRule.h"
#include "mozilla/dom/CSSNamespaceRule.h"
#include "mozilla/dom/CSSPageRule.h"
#include "mozilla/dom/CSSStyleRule.h"
#include "mozilla/dom/CSSSupportsRule.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/dom/Document.h"

using namespace mozilla::dom;

namespace mozilla {

ServoCSSRuleList::ServoCSSRuleList(already_AddRefed<ServoCssRules> aRawRules,
                                   StyleSheet* aSheet,
                                   css::GroupRule* aParentRule)
    : mStyleSheet(aSheet), mParentRule(aParentRule), mRawRules(aRawRules) {
  ResetRules();
}

// QueryInterface implementation for ServoCSSRuleList
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoCSSRuleList)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSRuleList)

NS_IMPL_ADDREF_INHERITED(ServoCSSRuleList, dom::CSSRuleList)
NS_IMPL_RELEASE_INHERITED(ServoCSSRuleList, dom::CSSRuleList)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoCSSRuleList)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ServoCSSRuleList)
  tmp->DropAllRules();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(dom::CSSRuleList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoCSSRuleList,
                                                  dom::CSSRuleList)
  tmp->EnumerateInstantiatedRules([&](css::Rule* aRule, uint32_t) {
    if (!aRule->IsCCLeaf()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRules[i]");
      cb.NoteXPCOMChild(aRule);
    }
  });
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

css::Rule* ServoCSSRuleList::GetRule(uint32_t aIndex) {
  uintptr_t rule = mRules[aIndex];
  if (rule <= kMaxRuleType) {
    RefPtr<css::Rule> ruleObj = nullptr;
    switch (StyleCssRuleType(rule)) {
#define CASE_RULE(const_, name_)                                             \
  case StyleCssRuleType::const_: {                                           \
    uint32_t line = 0, column = 0;                                           \
    RefPtr<RawServo##name_##Rule> raw =                                      \
        Servo_CssRules_Get##name_##RuleAt(mRawRules, aIndex, &line, &column) \
            .Consume();                                                      \
    MOZ_ASSERT(raw);                                                         \
    ruleObj = new CSS##name_##Rule(raw.forget(), mStyleSheet, mParentRule,   \
                                   line, column);                            \
    MOZ_ASSERT(ruleObj->Type() == StyleCssRuleType(rule));                   \
    break;                                                                   \
  }
      CASE_RULE(Style, Style)
      CASE_RULE(Keyframes, Keyframes)
      CASE_RULE(Media, Media)
      CASE_RULE(Namespace, Namespace)
      CASE_RULE(Page, Page)
      CASE_RULE(Supports, Supports)
      CASE_RULE(Document, MozDocument)
      CASE_RULE(Import, Import)
      CASE_RULE(FontFeatureValues, FontFeatureValues)
      CASE_RULE(FontFace, FontFace)
      CASE_RULE(CounterStyle, CounterStyle)
      CASE_RULE(LayerBlock, LayerBlock)
      CASE_RULE(LayerStatement, LayerStatement)
      CASE_RULE(Container, Container)
#undef CASE_RULE
      case StyleCssRuleType::Viewport:
        MOZ_ASSERT_UNREACHABLE("viewport is not implemented in Gecko");
        return nullptr;
      case StyleCssRuleType::Keyframe:
        MOZ_ASSERT_UNREACHABLE("keyframe rule cannot be here");
        return nullptr;
    }
    rule = CastToUint(ruleObj.forget().take());
    mRules[aIndex] = rule;
  }
  return CastToPtr(rule);
}

css::Rule* ServoCSSRuleList::IndexedGetter(uint32_t aIndex, bool& aFound) {
  if (aIndex >= mRules.Length()) {
    aFound = false;
    return nullptr;
  }
  aFound = true;
  return GetRule(aIndex);
}

template <typename Func>
void ServoCSSRuleList::EnumerateInstantiatedRules(Func aCallback) {
  uint32_t index = 0;
  for (uintptr_t rule : mRules) {
    if (rule > kMaxRuleType) {
      aCallback(CastToPtr(rule), index);
    }
    index++;
  }
}

static void DropRule(already_AddRefed<css::Rule> aRule) {
  RefPtr<css::Rule> rule = aRule;
  rule->DropReferences();
}

void ServoCSSRuleList::ResetRules() {
  // DropRule could reenter here via the cycle collector.
  auto rules = std::move(mRules);
  for (uintptr_t rule : rules) {
    if (rule > kMaxRuleType) {
      DropRule(already_AddRefed<css::Rule>(CastToPtr(rule)));
    }
  }
  MOZ_ASSERT(mRules.IsEmpty());
  if (mRawRules) {
    Servo_CssRules_ListTypes(mRawRules, &mRules);
  }
}

void ServoCSSRuleList::DropAllRules() {
  mStyleSheet = nullptr;
  mParentRule = nullptr;
  mRawRules = nullptr;

  ResetRules();
}

void ServoCSSRuleList::DropSheetReference() {
  // If mStyleSheet is not set on this rule list, then it is not set on any of
  // its instantiated rules either.  To avoid O(N^2) beavhior in the depth of
  // group rule nesting, which can happen if we are unlinked starting from the
  // deepest nested group rule, skip recursing into the rule list if we know we
  // don't need to.
  if (!mStyleSheet) {
    return;
  }
  mStyleSheet = nullptr;
  EnumerateInstantiatedRules(
      [](css::Rule* rule, uint32_t) { rule->DropSheetReference(); });
}

void ServoCSSRuleList::DropParentRuleReference() {
  mParentRule = nullptr;
  EnumerateInstantiatedRules(
      [](css::Rule* rule, uint32_t) { rule->DropParentRuleReference(); });
}

nsresult ServoCSSRuleList::InsertRule(const nsACString& aRule,
                                      uint32_t aIndex) {
  MOZ_ASSERT(mStyleSheet,
             "Caller must ensure that "
             "the list is not unlinked from stylesheet");

  if (!mRawRules || IsReadOnly()) {
    return NS_OK;
  }

  mStyleSheet->WillDirty();

  bool nested = !!mParentRule;
  css::Loader* loader = nullptr;
  auto allowImportRules = mStyleSheet->SelfOrAncestorIsConstructed()
                              ? StyleAllowImportRules::No
                              : StyleAllowImportRules::Yes;

  // TODO(emilio, bug 1535456): Should probably always be able to get a handle
  // to some loader if we're parsing an @import rule, but which one?
  //
  // StyleSheet::ReparseSheet just mints a new loader, but that'd be wrong in
  // this case I think, since such a load will bypass CSP checks.
  if (Document* doc = mStyleSheet->GetAssociatedDocument()) {
    loader = doc->CSSLoader();
  }
  StyleCssRuleType type;
  nsresult rv = Servo_CssRules_InsertRule(mRawRules, mStyleSheet->RawContents(),
                                          &aRule, aIndex, nested, loader,
                                          allowImportRules, mStyleSheet, &type);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mRules.InsertElementAt(aIndex, uintptr_t(type));
  return rv;
}

nsresult ServoCSSRuleList::DeleteRule(uint32_t aIndex) {
  if (!mRawRules || IsReadOnly()) {
    return NS_OK;
  }

  nsresult rv = Servo_CssRules_DeleteRule(mRawRules, aIndex);
  if (!NS_FAILED(rv)) {
    uintptr_t rule = mRules[aIndex];
    mRules.RemoveElementAt(aIndex);
    if (rule > kMaxRuleType) {
      DropRule(already_AddRefed<css::Rule>(CastToPtr(rule)));
    }
  }
  return rv;
}

void ServoCSSRuleList::SetRawContents(RefPtr<ServoCssRules> aNewRules,
                                      bool aFromClone) {
  mRawRules = std::move(aNewRules);
  if (!aFromClone) {
    ResetRules();
    return;
  }

  EnumerateInstantiatedRules([&](css::Rule* aRule, uint32_t aIndex) {
#define CASE_FOR(constant_, type_)                                           \
  case StyleCssRuleType::constant_: {                                        \
    uint32_t line = 0, column = 0;                                           \
    RefPtr<RawServo##type_##Rule> raw =                                      \
        Servo_CssRules_Get##type_##RuleAt(mRawRules, aIndex, &line, &column) \
            .Consume();                                                      \
    static_cast<dom::CSS##type_##Rule*>(aRule)->SetRawAfterClone(            \
        std::move(raw));                                                     \
    break;                                                                   \
  }
    switch (aRule->Type()) {
      CASE_FOR(Style, Style)
      CASE_FOR(Keyframes, Keyframes)
      CASE_FOR(Media, Media)
      CASE_FOR(Namespace, Namespace)
      CASE_FOR(Page, Page)
      CASE_FOR(Supports, Supports)
      CASE_FOR(Document, MozDocument)
      CASE_FOR(Import, Import)
      CASE_FOR(FontFeatureValues, FontFeatureValues)
      CASE_FOR(FontFace, FontFace)
      CASE_FOR(CounterStyle, CounterStyle)
      CASE_FOR(LayerBlock, LayerBlock)
      CASE_FOR(LayerStatement, LayerStatement)
      CASE_FOR(Container, Container)
      case StyleCssRuleType::Keyframe:
        MOZ_ASSERT_UNREACHABLE("keyframe rule cannot be here");
        break;
      case StyleCssRuleType::Viewport:
        MOZ_ASSERT_UNREACHABLE("Gecko doesn't implemente @viewport?");
        break;
    }
#undef CASE_FOR
  });
}

ServoCSSRuleList::~ServoCSSRuleList() {
  MOZ_ASSERT(!mStyleSheet, "Backpointer should have been cleared");
  MOZ_ASSERT(!mParentRule, "Backpointer should have been cleared");
  DropAllRules();
}

bool ServoCSSRuleList::IsReadOnly() const {
  MOZ_ASSERT(!mStyleSheet || !mParentRule ||
                 mStyleSheet->IsReadOnly() == mParentRule->IsReadOnly(),
             "a parent rule should be read only iff the owning sheet is "
             "read only");
  return mStyleSheet && mStyleSheet->IsReadOnly();
}

}  // namespace mozilla

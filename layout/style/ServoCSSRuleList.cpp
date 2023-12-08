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
#include "mozilla/dom/CSSFontPaletteValuesRule.h"
#include "mozilla/dom/CSSImportRule.h"
#include "mozilla/dom/CSSLayerBlockRule.h"
#include "mozilla/dom/CSSLayerStatementRule.h"
#include "mozilla/dom/CSSKeyframesRule.h"
#include "mozilla/dom/CSSContainerRule.h"
#include "mozilla/dom/CSSMediaRule.h"
#include "mozilla/dom/CSSMozDocumentRule.h"
#include "mozilla/dom/CSSNamespaceRule.h"
#include "mozilla/dom/CSSPageRule.h"
#include "mozilla/dom/CSSPropertyRule.h"
#include "mozilla/dom/CSSStyleRule.h"
#include "mozilla/dom/CSSSupportsRule.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/dom/Document.h"

using namespace mozilla::dom;

namespace mozilla {

ServoCSSRuleList::ServoCSSRuleList(
    already_AddRefed<StyleLockedCssRules> aRawRules, StyleSheet* aSheet,
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
#define CASE_RULE_WITH_PREFIX(const_, prefix_, name_)                         \
  case StyleCssRuleType::const_: {                                            \
    uint32_t line = 0, column = 0;                                            \
    RefPtr<Style##prefix_##const_##Rule> raw =                                \
        Servo_CssRules_Get##const_##RuleAt(mRawRules, aIndex, &line, &column) \
            .Consume();                                                       \
    MOZ_ASSERT(raw);                                                          \
    ruleObj = new CSS##name_##Rule(raw.forget(), mStyleSheet, mParentRule,    \
                                   line, column);                             \
    MOZ_ASSERT(ruleObj->Type() == StyleCssRuleType(rule));                    \
    break;                                                                    \
  }
#define CASE_RULE_LOCKED(const_, name_) \
  CASE_RULE_WITH_PREFIX(const_, Locked, name_)
#define CASE_RULE_UNLOCKED(const_, name_) CASE_RULE_WITH_PREFIX(const_, , name_)
      CASE_RULE_LOCKED(Style, Style)
      CASE_RULE_LOCKED(Keyframes, Keyframes)
      CASE_RULE_UNLOCKED(Media, Media)
      CASE_RULE_UNLOCKED(Namespace, Namespace)
      CASE_RULE_LOCKED(Page, Page)
      CASE_RULE_UNLOCKED(Property, Property)
      CASE_RULE_UNLOCKED(Supports, Supports)
      CASE_RULE_UNLOCKED(Document, MozDocument)
      CASE_RULE_LOCKED(Import, Import)
      CASE_RULE_UNLOCKED(FontFeatureValues, FontFeatureValues)
      CASE_RULE_UNLOCKED(FontPaletteValues, FontPaletteValues)
      CASE_RULE_LOCKED(FontFace, FontFace)
      CASE_RULE_LOCKED(CounterStyle, CounterStyle)
      CASE_RULE_UNLOCKED(LayerBlock, LayerBlock)
      CASE_RULE_UNLOCKED(LayerStatement, LayerStatement)
      CASE_RULE_UNLOCKED(Container, Container)
#undef CASE_RULE_LOCKED
#undef CASE_RULE_UNLOCKED
#undef CASE_RULE_WITH_PREFIX
      case StyleCssRuleType::Keyframe:
        MOZ_ASSERT_UNREACHABLE("keyframe rule cannot be here");
        return nullptr;
      case StyleCssRuleType::Margin:
        // Margin rules not implemented yet, see bug 1864737
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
  uint32_t containingTypes = 0;
  for (css::Rule* rule = mParentRule; rule; rule = rule->GetParentRule()) {
    containingTypes |= (1 << uint32_t(rule->Type()));
  }
  nsresult rv = Servo_CssRules_InsertRule(
      mRawRules, mStyleSheet->RawContents(), &aRule, aIndex, containingTypes,
      loader, allowImportRules, mStyleSheet, &type);
  NS_ENSURE_SUCCESS(rv, rv);
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

void ServoCSSRuleList::SetRawContents(RefPtr<StyleLockedCssRules> aNewRules,
                                      bool aFromClone) {
  mRawRules = std::move(aNewRules);
  if (!aFromClone) {
    ResetRules();
    return;
  }

  EnumerateInstantiatedRules([&](css::Rule* aRule, uint32_t aIndex) {
#define RULE_CASE_WITH_PREFIX(constant_, prefix_, type_)                \
  case StyleCssRuleType::constant_: {                                   \
    uint32_t line = 0, column = 0;                                      \
    RefPtr<Style##prefix_##constant_##Rule> raw =                       \
        Servo_CssRules_Get##constant_##RuleAt(mRawRules, aIndex, &line, \
                                              &column)                  \
            .Consume();                                                 \
    static_cast<dom::CSS##type_##Rule*>(aRule)->SetRawAfterClone(       \
        std::move(raw));                                                \
    break;                                                              \
  }
#define RULE_CASE_LOCKED(constant_, type_) \
  RULE_CASE_WITH_PREFIX(constant_, Locked, type_)
#define RULE_CASE_UNLOCKED(constant_, type_) \
  RULE_CASE_WITH_PREFIX(constant_, , type_)
    switch (aRule->Type()) {
      RULE_CASE_LOCKED(Style, Style)
      RULE_CASE_LOCKED(Keyframes, Keyframes)
      RULE_CASE_UNLOCKED(Media, Media)
      RULE_CASE_UNLOCKED(Namespace, Namespace)
      RULE_CASE_LOCKED(Page, Page)
      RULE_CASE_UNLOCKED(Property, Property)
      RULE_CASE_UNLOCKED(Supports, Supports)
      RULE_CASE_UNLOCKED(Document, MozDocument)
      RULE_CASE_LOCKED(Import, Import)
      RULE_CASE_UNLOCKED(FontFeatureValues, FontFeatureValues)
      RULE_CASE_UNLOCKED(FontPaletteValues, FontPaletteValues)
      RULE_CASE_LOCKED(FontFace, FontFace)
      RULE_CASE_LOCKED(CounterStyle, CounterStyle)
      RULE_CASE_UNLOCKED(LayerBlock, LayerBlock)
      RULE_CASE_UNLOCKED(LayerStatement, LayerStatement)
      RULE_CASE_UNLOCKED(Container, Container)
      case StyleCssRuleType::Keyframe:
        MOZ_ASSERT_UNREACHABLE("keyframe rule cannot be here");
        break;
      case StyleCssRuleType::Margin:
        // Margin rules not implemented yet, see bug 1864737
        break;
    }
#undef RULE_CASE_WITH_PREFIX
#undef RULE_CASE_LOCKED
#undef RULE_CASE_UNLOCKED
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

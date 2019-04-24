/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleRuleMap.h"

#include "mozilla/css/GroupRule.h"
#include "mozilla/dom/CSSImportRule.h"
#include "mozilla/dom/CSSRuleBinding.h"
#include "mozilla/dom/CSSStyleRule.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StyleSheetInlines.h"
#include "nsStyleSheetService.h"

using namespace mozilla::dom;

namespace mozilla {

void ServoStyleRuleMap::EnsureTable(ServoStyleSet& aStyleSet) {
  if (!IsEmpty()) {
    return;
  }
  aStyleSet.EnumerateStyleSheets(
      [&](StyleSheet& aSheet) { FillTableFromStyleSheet(aSheet); });
}

void ServoStyleRuleMap::EnsureTable(ShadowRoot& aShadowRoot) {
  if (!IsEmpty()) {
    return;
  }
  for (auto index : IntegerRange(aShadowRoot.SheetCount())) {
    FillTableFromStyleSheet(*aShadowRoot.SheetAt(index));
  }
}

void ServoStyleRuleMap::SheetAdded(StyleSheet& aStyleSheet) {
  if (!IsEmpty()) {
    FillTableFromStyleSheet(aStyleSheet);
  }
}

void ServoStyleRuleMap::SheetRemoved(StyleSheet& aStyleSheet) {
  // Invalidate all data inside. This isn't strictly necessary since
  // we should always get update from document before new queries come.
  // But it is probably still safer if we try to avoid having invalid
  // pointers inside. Also if the document keep adding and removing
  // stylesheets, this would also prevent us from infinitely growing
  // memory usage.
  mTable.Clear();
}

void ServoStyleRuleMap::RuleAdded(StyleSheet& aStyleSheet,
                                  css::Rule& aStyleRule) {
  if (!IsEmpty()) {
    FillTableFromRule(aStyleRule);
  }
}

void ServoStyleRuleMap::RuleRemoved(StyleSheet& aStyleSheet,
                                    css::Rule& aStyleRule) {
  if (IsEmpty()) {
    return;
  }

  switch (aStyleRule.Type()) {
    case CSSRule_Binding::STYLE_RULE: {
      auto& rule = static_cast<CSSStyleRule&>(aStyleRule);
      mTable.Remove(rule.Raw());
      break;
    }
    case CSSRule_Binding::IMPORT_RULE:
    case CSSRule_Binding::MEDIA_RULE:
    case CSSRule_Binding::SUPPORTS_RULE:
    case CSSRule_Binding::DOCUMENT_RULE: {
      // See the comment in StyleSheetRemoved.
      mTable.Clear();
      break;
    }
    case CSSRule_Binding::FONT_FACE_RULE:
    case CSSRule_Binding::PAGE_RULE:
    case CSSRule_Binding::KEYFRAMES_RULE:
    case CSSRule_Binding::KEYFRAME_RULE:
    case CSSRule_Binding::NAMESPACE_RULE:
    case CSSRule_Binding::COUNTER_STYLE_RULE:
    case CSSRule_Binding::FONT_FEATURE_VALUES_RULE:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled rule");
  }
}

size_t ServoStyleRuleMap::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

void ServoStyleRuleMap::FillTableFromRule(css::Rule& aRule) {
  switch (aRule.Type()) {
    case CSSRule_Binding::STYLE_RULE: {
      auto& rule = static_cast<CSSStyleRule&>(aRule);
      mTable.Put(rule.Raw(), &rule);
      break;
    }
    case CSSRule_Binding::MEDIA_RULE:
    case CSSRule_Binding::SUPPORTS_RULE:
    case CSSRule_Binding::DOCUMENT_RULE: {
      auto& rule = static_cast<css::GroupRule&>(aRule);
      auto ruleList = static_cast<ServoCSSRuleList*>(rule.CssRules());
      FillTableFromRuleList(*ruleList);
      break;
    }
    case CSSRule_Binding::IMPORT_RULE: {
      auto& rule = static_cast<CSSImportRule&>(aRule);
      MOZ_ASSERT(aRule.GetStyleSheet());
      FillTableFromStyleSheet(*rule.GetStyleSheet());
      break;
    }
  }
}

void ServoStyleRuleMap::FillTableFromRuleList(ServoCSSRuleList& aRuleList) {
  for (uint32_t i : IntegerRange(aRuleList.Length())) {
    FillTableFromRule(*aRuleList.GetRule(i));
  }
}

void ServoStyleRuleMap::FillTableFromStyleSheet(StyleSheet& aSheet) {
  if (aSheet.IsComplete()) {
    FillTableFromRuleList(*aSheet.GetCssRulesInternal());
  }
}

}  // namespace mozilla

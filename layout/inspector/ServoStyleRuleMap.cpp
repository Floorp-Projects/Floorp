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
#include "mozilla/dom/Element.h"
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
  for (const auto& sheet : aShadowRoot.AdoptedStyleSheets()) {
    FillTableFromStyleSheet(*sheet);
  }
}

void ServoStyleRuleMap::SheetAdded(StyleSheet& aStyleSheet) {
  if (!IsEmpty()) {
    FillTableFromStyleSheet(aStyleSheet);
  }
}

void ServoStyleRuleMap::SheetCloned(StyleSheet& aStyleSheet) {
  // Invalidate all data inside. We could probably track down all the individual
  // rules that changed etc, but it doesn't seem worth it.
  //
  // TODO: We can't do this until GetCssRulesInternal stops cloning.
  // mTable.Clear();
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
    case StyleCssRuleType::Style:
    case StyleCssRuleType::Import:
    case StyleCssRuleType::Media:
    case StyleCssRuleType::Supports:
    case StyleCssRuleType::LayerBlock:
    case StyleCssRuleType::Container:
    case StyleCssRuleType::Document: {
      // See the comment in SheetRemoved.
      mTable.Clear();
      break;
    }
    case StyleCssRuleType::LayerStatement:
    case StyleCssRuleType::FontFace:
    case StyleCssRuleType::Page:
    case StyleCssRuleType::Property:
    case StyleCssRuleType::Keyframes:
    case StyleCssRuleType::Keyframe:
    case StyleCssRuleType::Margin:
    case StyleCssRuleType::Namespace:
    case StyleCssRuleType::CounterStyle:
    case StyleCssRuleType::FontFeatureValues:
    case StyleCssRuleType::FontPaletteValues:
      break;
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
    case StyleCssRuleType::Style: {
      auto& rule = static_cast<CSSStyleRule&>(aRule);
      mTable.InsertOrUpdate(rule.Raw(), &rule);
      [[fallthrough]];
    }
    case StyleCssRuleType::LayerBlock:
    case StyleCssRuleType::Media:
    case StyleCssRuleType::Supports:
    case StyleCssRuleType::Container:
    case StyleCssRuleType::Document: {
      auto& rule = static_cast<css::GroupRule&>(aRule);
      FillTableFromRuleList(*rule.CssRules());
      break;
    }
    case StyleCssRuleType::Import: {
      auto& rule = static_cast<CSSImportRule&>(aRule);
      if (auto* sheet = rule.GetStyleSheet()) {
        FillTableFromStyleSheet(*sheet);
      }
      break;
    }
    case StyleCssRuleType::LayerStatement:
    case StyleCssRuleType::FontFace:
    case StyleCssRuleType::Page:
    case StyleCssRuleType::Property:
    case StyleCssRuleType::Keyframes:
    case StyleCssRuleType::Keyframe:
    case StyleCssRuleType::Margin:
    case StyleCssRuleType::Namespace:
    case StyleCssRuleType::CounterStyle:
    case StyleCssRuleType::FontFeatureValues:
    case StyleCssRuleType::FontPaletteValues:
      break;
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

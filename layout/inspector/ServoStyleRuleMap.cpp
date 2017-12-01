/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleRuleMap.h"

#include "mozilla/css/GroupRule.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ServoStyleRule.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoImportRule.h"
#include "mozilla/StyleSheetInlines.h"

#include "nsDocument.h"
#include "nsStyleSheetService.h"

namespace mozilla {

ServoStyleRuleMap::ServoStyleRuleMap(ServoStyleSet* aStyleSet)
  : mStyleSet(aStyleSet)
{
}

ServoStyleRuleMap::~ServoStyleRuleMap()
{
}

void
ServoStyleRuleMap::EnsureTable()
{
  if (!IsEmpty()) {
    return;
  }
  mStyleSet->EnumerateStyleSheetArrays(
    [this](const nsTArray<RefPtr<ServoStyleSheet>>& aArray) {
      for (auto& sheet : aArray) {
        FillTableFromStyleSheet(*sheet);
      }
    });
}

void
ServoStyleRuleMap::SheetAdded(ServoStyleSheet& aStyleSheet)
{
  if (!IsEmpty()) {
    FillTableFromStyleSheet(aStyleSheet);
  }
}

void
ServoStyleRuleMap::SheetRemoved(ServoStyleSheet& aStyleSheet)
{
  // Invalidate all data inside. This isn't strictly necessary since
  // we should always get update from document before new queries come.
  // But it is probably still safer if we try to avoid having invalid
  // pointers inside. Also if the document keep adding and removing
  // stylesheets, this would also prevent us from infinitely growing
  // memory usage.
  mTable.Clear();
}

void
ServoStyleRuleMap::RuleAdded(ServoStyleSheet& aStyleSheet, css::Rule& aStyleRule)
{
  if (!IsEmpty()) {
    FillTableFromRule(aStyleRule);
  }
}

void
ServoStyleRuleMap::RuleRemoved(ServoStyleSheet& aStyleSheet,
                               css::Rule& aStyleRule)
{
  if (IsEmpty()) {
    return;
  }

  switch (aStyleRule.Type()) {
    case nsIDOMCSSRule::STYLE_RULE: {
      auto& rule = static_cast<ServoStyleRule&>(aStyleRule);
      mTable.Remove(rule.Raw());
      break;
    }
    case nsIDOMCSSRule::IMPORT_RULE:
    case nsIDOMCSSRule::MEDIA_RULE:
    case nsIDOMCSSRule::SUPPORTS_RULE:
    case nsIDOMCSSRule::DOCUMENT_RULE: {
      // See the comment in StyleSheetRemoved.
      mTable.Clear();
      break;
    }
    case nsIDOMCSSRule::FONT_FACE_RULE:
    case nsIDOMCSSRule::PAGE_RULE:
    case nsIDOMCSSRule::KEYFRAMES_RULE:
    case nsIDOMCSSRule::KEYFRAME_RULE:
    case nsIDOMCSSRule::NAMESPACE_RULE:
    case nsIDOMCSSRule::COUNTER_STYLE_RULE:
    case nsIDOMCSSRule::FONT_FEATURE_VALUES_RULE:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled rule");
  }
}

size_t
ServoStyleRuleMap::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

void
ServoStyleRuleMap::FillTableFromRule(css::Rule& aRule)
{
  switch (aRule.Type()) {
    case nsIDOMCSSRule::STYLE_RULE: {
      auto& rule = static_cast<ServoStyleRule&>(aRule);
      mTable.Put(rule.Raw(), &rule);
      break;
    }
    case nsIDOMCSSRule::MEDIA_RULE:
    case nsIDOMCSSRule::SUPPORTS_RULE:
    case nsIDOMCSSRule::DOCUMENT_RULE: {
      auto& rule = static_cast<css::GroupRule&>(aRule);
      auto ruleList = static_cast<ServoCSSRuleList*>(rule.CssRules());
      FillTableFromRuleList(*ruleList);
      break;
    }
    case nsIDOMCSSRule::IMPORT_RULE: {
      auto& rule = static_cast<ServoImportRule&>(aRule);
      MOZ_ASSERT(aRule.GetStyleSheet());
      FillTableFromStyleSheet(*rule.GetStyleSheet()->AsServo());
      break;
    }
  }
}

void
ServoStyleRuleMap::FillTableFromRuleList(ServoCSSRuleList& aRuleList)
{
  for (uint32_t i : IntegerRange(aRuleList.Length())) {
    FillTableFromRule(*aRuleList.GetRule(i));
  }
}

void
ServoStyleRuleMap::FillTableFromStyleSheet(ServoStyleSheet& aSheet)
{
  if (aSheet.IsComplete()) {
    FillTableFromRuleList(*aSheet.GetCssRulesInternal());
  }
}

} // namespace mozilla

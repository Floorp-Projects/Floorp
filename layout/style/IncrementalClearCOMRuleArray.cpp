/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/IncrementalClearCOMRuleArray.h"

#include <algorithm> // For std::min
#include "nsCycleCollector.h"
#include "mozilla/DeferredFinalize.h"
#include "nsTArray.h"
#include "nsCCUncollectableMarker.h"

using namespace mozilla;

typedef nsCOMArray<css::Rule> RuleArray;
typedef nsTArray<RuleArray> RuleArrayArray;


// These methods are based on those in DeferredFinalizerImpl.

static void*
AppendRulesArrayPointer(void* aData, void* aObject)
{
  RuleArrayArray* rulesArray = static_cast<RuleArrayArray*>(aData);
  RuleArray* oldRules = static_cast<RuleArray*>(aObject);

  if (!rulesArray) {
    rulesArray = new RuleArrayArray();
  }

  RuleArray* newRules = rulesArray->AppendElement();
  newRules->SwapElements(*oldRules);

  return rulesArray;
}

// Remove up to |aSliceBudget| css::Rules from the arrays, starting at
// the end of the last array.
static bool
DeferredFinalizeRulesArray(uint32_t aSliceBudget, void* aData)
{
  MOZ_ASSERT(aSliceBudget > 0, "nonsensical/useless call with aSliceBudget == 0");
  RuleArrayArray* rulesArray = static_cast<RuleArrayArray*>(aData);

  size_t newOuterLen = rulesArray->Length();

  while (aSliceBudget > 0 && newOuterLen > 0) {
    RuleArray& lastArray = rulesArray->ElementAt(newOuterLen - 1);
    uint32_t innerLen = lastArray.Length();
    uint32_t currSlice = std::min(innerLen, aSliceBudget);
    uint32_t newInnerLen = innerLen - currSlice;
    lastArray.TruncateLength(newInnerLen);
    aSliceBudget -= currSlice;
    if (newInnerLen == 0) {
      newOuterLen -= 1;
    }
  }

  rulesArray->TruncateLength(newOuterLen);

  if (newOuterLen == 0) {
    delete rulesArray;
    return true;
  }
  return false;
}

void
IncrementalClearCOMRuleArray::Clear()
{
  // Destroy the array incrementally if it is long and we
  // haven't started shutting down.
  if (Length() > 10 && nsCCUncollectableMarker::sGeneration) {
    DeferredFinalize(AppendRulesArrayPointer, DeferredFinalizeRulesArray, this);
  } else {
    nsCOMArray<css::Rule>::Clear();
  }
  MOZ_ASSERT(Length() == 0);
}

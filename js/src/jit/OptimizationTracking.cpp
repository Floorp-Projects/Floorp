/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/OptimizationTracking.h"

#include "ds/Sort.h"
#include "jit/IonBuilder.h"
#include "jit/JitcodeMap.h"
#include "jit/JitSpewer.h"
#include "js/TrackedOptimizationInfo.h"
#include "util/Text.h"

#include "vm/ObjectGroup-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

using JS::TrackedOutcome;
using JS::TrackedStrategy;
using JS::TrackedTypeSite;

JS_PUBLIC_API const char* JS::TrackedStrategyString(TrackedStrategy strategy) {
  switch (strategy) {
#define STRATEGY_CASE(name)   \
  case TrackedStrategy::name: \
    return #name;
    TRACKED_STRATEGY_LIST(STRATEGY_CASE)
#undef STRATEGY_CASE

    default:
      MOZ_CRASH("bad strategy");
  }
}

JS_PUBLIC_API const char* JS::TrackedOutcomeString(TrackedOutcome outcome) {
  switch (outcome) {
#define OUTCOME_CASE(name)   \
  case TrackedOutcome::name: \
    return #name;
    TRACKED_OUTCOME_LIST(OUTCOME_CASE)
#undef OUTCOME_CASE

    default:
      MOZ_CRASH("bad outcome");
  }
}

JS_PUBLIC_API const char* JS::TrackedTypeSiteString(TrackedTypeSite site) {
  switch (site) {
#define TYPESITE_CASE(name)   \
  case TrackedTypeSite::name: \
    return #name;
    TRACKED_TYPESITE_LIST(TYPESITE_CASE)
#undef TYPESITE_CASE

    default:
      MOZ_CRASH("bad type site");
  }
}

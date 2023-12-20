/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Pretenuring.h"

#include "mozilla/Sprintf.h"

#include "gc/GCInternals.h"
#include "gc/PublicIterators.h"
#include "jit/Invalidation.h"

#include "gc/PrivateIterators-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::gc;

// The number of nursery allocations at which to pay attention to an allocation
// site. This must be large enough to ensure we have enough information to infer
// the lifetime and also large enough to avoid pretenuring low volume allocation
// sites.
static constexpr size_t NormalSiteAttentionThreshold = 500;
static constexpr size_t UnknownSiteAttentionThreshold = 30000;

// The maximum number of alloc sites to create between each minor
// collection. Stop tracking allocation after this limit is reached. This
// prevents unbounded time traversing the list during minor GC.
static constexpr size_t MaxAllocSitesPerMinorGC = 500;

// The maximum number of times to invalidate JIT code for a site. After this we
// leave the site's state as Unknown and don't pretenure allocations.
// Note we use 4 bits to store the invalidation count.
static constexpr size_t MaxInvalidationCount = 5;

// The minimum number of allocated cells needed to determine the survival rate
// of cells in newly created arenas.
static constexpr size_t MinCellsRequiredForSurvivalRate = 100;

// The young survival rate below which a major collection is determined to have
// a low young survival rate.
static constexpr double LowYoungSurvivalThreshold = 0.05;

// The number of consecutive major collections with a low young survival rate
// that must occur before recovery is attempted.
static constexpr size_t LowYoungSurvivalCountBeforeRecovery = 2;

// The proportion of the nursery that must be tenured above which a minor
// collection may be determined to have a high nursery survival rate.
static constexpr double HighNurserySurvivalPromotionThreshold = 0.6;

// The number of nursery allocations made by optimized JIT code that must be
// tenured above which a minor collection may be determined to have a high
// nursery survival rate.
static constexpr size_t HighNurserySurvivalOptimizedAllocThreshold = 10000;

// The number of consecutive minor collections with a high nursery survival rate
// that must occur before recovery is attempted.
static constexpr size_t HighNurserySurvivalCountBeforeRecovery = 2;

AllocSite* const AllocSite::EndSentinel = reinterpret_cast<AllocSite*>(1);
JSScript* const AllocSite::WasmScript =
    reinterpret_cast<JSScript*>(AllocSite::STATE_MASK + 1);

static bool SiteBasedPretenuringEnabled = true;

JS_PUBLIC_API void JS::SetSiteBasedPretenuringEnabled(bool enable) {
  SiteBasedPretenuringEnabled = enable;
}

bool PretenuringNursery::canCreateAllocSite() {
  MOZ_ASSERT(allocSitesCreated <= MaxAllocSitesPerMinorGC);
  return SiteBasedPretenuringEnabled &&
         allocSitesCreated < MaxAllocSitesPerMinorGC;
}

size_t PretenuringNursery::doPretenuring(GCRuntime* gc, JS::GCReason reason,
                                         bool validPromotionRate,
                                         double promotionRate, bool reportInfo,
                                         size_t reportThreshold) {
  size_t sitesActive = 0;
  size_t sitesPretenured = 0;
  size_t sitesInvalidated = 0;
  size_t zonesWithHighNurserySurvival = 0;

  // Zero allocation counts.
  totalAllocCount_ = 0;
  for (ZonesIter zone(gc, SkipAtoms); !zone.done(); zone.next()) {
    for (auto& count : zone->pretenuring.nurseryAllocCounts) {
      count = 0;
    }
  }

  // Check whether previously optimized code has changed its behaviour and
  // needs to be recompiled so that it can pretenure its allocations.
  if (validPromotionRate) {
    for (ZonesIter zone(gc, SkipAtoms); !zone.done(); zone.next()) {
      bool highNurserySurvivalRate =
          promotionRate > HighNurserySurvivalPromotionThreshold &&
          zone->optimizedAllocSite()->nurseryTenuredCount >=
              HighNurserySurvivalOptimizedAllocThreshold;
      zone->pretenuring.noteHighNurserySurvivalRate(highNurserySurvivalRate);
      if (highNurserySurvivalRate) {
        zonesWithHighNurserySurvival++;
      }
    }
  }

  if (reportInfo) {
    AllocSite::printInfoHeader(reason, promotionRate);
  }

  AllocSite* site = allocatedSites;
  allocatedSites = AllocSite::EndSentinel;
  while (site != AllocSite::EndSentinel) {
    AllocSite* next = site->nextNurseryAllocated;
    site->nextNurseryAllocated = nullptr;

    if (site->isNormal()) {
      sitesActive++;
      updateTotalAllocCounts(site);
      auto result = site->processSite(gc, NormalSiteAttentionThreshold,
                                      reportInfo, reportThreshold);
      if (result == AllocSite::WasPretenured ||
          result == AllocSite::WasPretenuredAndInvalidated) {
        sitesPretenured++;
        if (site->hasScript()) {
          site->script()->realm()->numAllocSitesPretenured++;
        }
      }
      if (result == AllocSite::WasPretenuredAndInvalidated) {
        sitesInvalidated++;
      }
    }

    site = next;
  }

  // Catch-all sites don't end up on the list if they are only used from
  // optimized JIT code, so process them here.
  for (ZonesIter zone(gc, SkipAtoms); !zone.done(); zone.next()) {
    for (auto& site : zone->pretenuring.unknownAllocSites) {
      updateTotalAllocCounts(&site);
      if (site.traceKind() == JS::TraceKind::Object) {
        site.processCatchAllSite(reportInfo, reportThreshold);
      } else {
        site.processSite(gc, UnknownSiteAttentionThreshold, reportInfo,
                         reportThreshold);
      }
      // Result checked in Nursery::doPretenuring.
    }
    updateTotalAllocCounts(zone->optimizedAllocSite());
    zone->optimizedAllocSite()->processCatchAllSite(reportInfo,
                                                    reportThreshold);
  }

  if (reportInfo) {
    AllocSite::printInfoFooter(allocSitesCreated, sitesActive, sitesPretenured,
                               sitesInvalidated);
    if (zonesWithHighNurserySurvival) {
      fprintf(stderr, "  %zu zones with high nursery survival rate\n",
              zonesWithHighNurserySurvival);
    }
  }

  allocSitesCreated = 0;

  return sitesPretenured;
}

AllocSite::SiteResult AllocSite::processSite(GCRuntime* gc,
                                             size_t attentionThreshold,
                                             bool reportInfo,
                                             size_t reportThreshold) {
  MOZ_ASSERT(kind() != Kind::Optimized);
  MOZ_ASSERT(nurseryAllocCount >= nurseryTenuredCount);

  SiteResult result = NoChange;

  bool hasPromotionRate = false;
  double promotionRate = 0.0;
  bool wasInvalidated = false;

  if (nurseryAllocCount > attentionThreshold) {
    promotionRate = double(nurseryTenuredCount) / double(nurseryAllocCount);
    hasPromotionRate = true;

    AllocSite::State prevState = state();
    updateStateOnMinorGC(promotionRate);
    AllocSite::State newState = state();

    if (prevState == AllocSite::State::Unknown &&
        newState == AllocSite::State::LongLived) {
      result = WasPretenured;

      // We can optimize JIT code before we realise that a site should be
      // pretenured. Make sure we invalidate any existing optimized code.
      if (hasScript()) {
        wasInvalidated = invalidateScript(gc);
        if (wasInvalidated) {
          result = WasPretenuredAndInvalidated;
        }
      }
    }
  }

  if (reportInfo && allocCount() >= reportThreshold) {
    printInfo(hasPromotionRate, promotionRate, wasInvalidated);
  }

  resetNurseryAllocations();

  return result;
}

void AllocSite::processCatchAllSite(bool reportInfo, size_t reportThreshold) {
  MOZ_ASSERT(!isNormal());

  if (!hasNurseryAllocations()) {
    return;
  }

  if (reportInfo && allocCount() >= reportThreshold) {
    printInfo(false, 0.0, false);
  }

  resetNurseryAllocations();
}

void PretenuringNursery::updateTotalAllocCounts(AllocSite* site) {
  JS::TraceKind kind = site->traceKind();
  totalAllocCount_ += site->nurseryAllocCount;
  PretenuringZone& zone = site->zone()->pretenuring;
  zone.nurseryAllocCount(kind) += site->nurseryAllocCount;
}

bool AllocSite::invalidateScript(GCRuntime* gc) {
  CancelOffThreadIonCompile(script());

  if (!script()->hasIonScript()) {
    return false;
  }

  if (invalidationLimitReached()) {
    MOZ_ASSERT(state() == State::Unknown);
    return false;
  }

  invalidationCount++;
  if (invalidationLimitReached()) {
    setState(State::Unknown);
  }

  JSContext* cx = gc->rt->mainContextFromOwnThread();
  jit::Invalidate(cx, script(),
                  /* resetUses = */ false,
                  /* cancelOffThread = */ true);
  return true;
}

bool AllocSite::invalidationLimitReached() const {
  MOZ_ASSERT(invalidationCount <= MaxInvalidationCount);
  return invalidationCount == MaxInvalidationCount;
}

void PretenuringNursery::maybeStopPretenuring(GCRuntime* gc) {
  for (GCZonesIter zone(gc); !zone.done(); zone.next()) {
    double rate;
    if (zone->pretenuring.calculateYoungTenuredSurvivalRate(&rate)) {
      bool lowYoungSurvivalRate = rate < LowYoungSurvivalThreshold;
      zone->pretenuring.noteLowYoungTenuredSurvivalRate(lowYoungSurvivalRate);
    }
  }
}

AllocSite::Kind AllocSite::kind() const {
  if (isNormal()) {
    return Kind::Normal;
  }

  if (this == zone()->optimizedAllocSite()) {
    return Kind::Optimized;
  }

  MOZ_ASSERT(this == zone()->unknownAllocSite(traceKind()));
  return Kind::Unknown;
}

void AllocSite::updateStateOnMinorGC(double promotionRate) {
  // The state changes based on whether the promotion rate is deemed high
  // (greater that 90%):
  //
  //                      high                          high
  //               ------------------>           ------------------>
  //   ShortLived                       Unknown                        LongLived
  //               <------------------           <------------------
  //                      !high                         !high
  //
  // The nursery is used to allocate if the site's state is Unknown or
  // ShortLived. There are no direct transition between ShortLived and LongLived
  // to avoid pretenuring sites that we've recently observed being short-lived.

  if (invalidationLimitReached()) {
    MOZ_ASSERT(state() == State::Unknown);
    return;
  }

  bool highPromotionRate = promotionRate >= 0.9;

  switch (state()) {
    case State::Unknown:
      if (highPromotionRate) {
        setState(State::LongLived);
      } else {
        setState(State::ShortLived);
      }
      break;

    case State::ShortLived: {
      if (highPromotionRate) {
        setState(State::Unknown);
      }
      break;
    }

    case State::LongLived: {
      if (!highPromotionRate) {
        setState(State::Unknown);
      }
      break;
    }
  }
}

bool AllocSite::maybeResetState() {
  if (invalidationLimitReached()) {
    MOZ_ASSERT(state() == State::Unknown);
    return false;
  }

  invalidationCount++;
  setState(State::Unknown);
  return true;
}

void AllocSite::trace(JSTracer* trc) {
  if (hasScript()) {
    JSScript* s = script();
    TraceManuallyBarrieredEdge(trc, &s, "AllocSite script");
    if (s != script()) {
      setScript(s);
    }
  }
}

bool PretenuringZone::calculateYoungTenuredSurvivalRate(double* rateOut) {
  MOZ_ASSERT(allocCountInNewlyCreatedArenas >=
             survivorCountInNewlyCreatedArenas);
  if (allocCountInNewlyCreatedArenas < MinCellsRequiredForSurvivalRate) {
    return false;
  }

  *rateOut = double(survivorCountInNewlyCreatedArenas) /
             double(allocCountInNewlyCreatedArenas);
  return true;
}

void PretenuringZone::noteLowYoungTenuredSurvivalRate(
    bool lowYoungSurvivalRate) {
  if (lowYoungSurvivalRate) {
    lowYoungTenuredSurvivalCount++;
  } else {
    lowYoungTenuredSurvivalCount = 0;
  }
}

void PretenuringZone::noteHighNurserySurvivalRate(
    bool highNurserySurvivalRate) {
  if (highNurserySurvivalRate) {
    highNurserySurvivalCount++;
  } else {
    highNurserySurvivalCount = 0;
  }
}

bool PretenuringZone::shouldResetNurseryAllocSites() {
  bool shouldReset =
      highNurserySurvivalCount >= HighNurserySurvivalCountBeforeRecovery;
  if (shouldReset) {
    highNurserySurvivalCount = 0;
  }
  return shouldReset;
}

bool PretenuringZone::shouldResetPretenuredAllocSites() {
  bool shouldReset =
      lowYoungTenuredSurvivalCount >= LowYoungSurvivalCountBeforeRecovery;
  if (shouldReset) {
    lowYoungTenuredSurvivalCount = 0;
  }
  return shouldReset;
}

/* static */
void AllocSite::printInfoHeader(JS::GCReason reason, double promotionRate) {
  fprintf(stderr, "  %-16s %-16s %-20s %-8s %-8s %-6s %-10s\n", "site", "zone",
          "script/kind", "nallocs", "tenures", "prate", "state");
}

/* static */
void AllocSite::printInfoFooter(size_t sitesCreated, size_t sitesActive,
                                size_t sitesPretenured,
                                size_t sitesInvalidated) {
  fprintf(stderr,
          "  %zu alloc sites created, %zu active, %zu pretenured, %zu "
          "invalidated\n",
          sitesCreated, sitesActive, sitesPretenured, sitesInvalidated);
}

static const char* AllocSiteKindName(AllocSite::Kind kind) {
  switch (kind) {
    case AllocSite::Kind::Normal:
      return "normal";
    case AllocSite::Kind::Unknown:
      return "unknown";
    case AllocSite::Kind::Optimized:
      return "optimized";
    default:
      MOZ_CRASH("Bad AllocSite kind");
  }
}

void AllocSite::printInfo(bool hasPromotionRate, double promotionRate,
                          bool wasInvalidated) const {
  // Zone.
  fprintf(stderr, "  %16p %16p", this, zone());

  // Script, or which kind of catch-all site this is.
  if (!hasScript()) {
    const char* siteKindName = AllocSiteKindName(kind());
    if (kind() == Kind::Unknown) {
      char buffer[32];
      const char* traceKindName = JS::GCTraceKindToAscii(traceKind());
      SprintfLiteral(buffer, "%s %s", siteKindName, traceKindName);
      fprintf(stderr, " %-20s", buffer);
    } else {
      fprintf(stderr, " %-20s", siteKindName);
    }
  } else {
    fprintf(stderr, " %20p", script());
  }

  // Nursery allocation count, missing for optimized sites.
  char buffer[16] = {'\0'};
  if (kind() != Kind::Optimized) {
    SprintfLiteral(buffer, "%8" PRIu32, nurseryAllocCount);
  }
  fprintf(stderr, " %8s", buffer);

  // Nursery tenure count.
  fprintf(stderr, " %8" PRIu32, nurseryTenuredCount);

  // Promotion rate, if there were enough allocations.
  buffer[0] = '\0';
  if (hasPromotionRate) {
    SprintfLiteral(buffer, "%5.1f%%", std::min(1.0, promotionRate) * 100);
  }
  fprintf(stderr, " %6s", buffer);

  // Current state where applicable.
  const char* state = kind() != Kind::Optimized ? stateName() : "";
  fprintf(stderr, " %-10s", state);

  // Whether the associated script was invalidated.
  if (wasInvalidated) {
    fprintf(stderr, " invalidated");
  }

  fprintf(stderr, "\n");
}

const char* AllocSite::stateName() const {
  switch (state()) {
    case State::ShortLived:
      return "ShortLived";
    case State::Unknown:
      return "Unknown";
    case State::LongLived:
      return "LongLived";
  }

  MOZ_CRASH("Unknown state");
}

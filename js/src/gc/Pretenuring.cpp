/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Pretenuring.h"

#include "mozilla/Sprintf.h"

#include "gc/PublicIterators.h"

using namespace js;
using namespace js::gc;

AllocSite* const AllocSite::EndSentinel = reinterpret_cast<AllocSite*>(1);

void PretenuringNursery::doPretenuring(GCRuntime* gc, bool reportInfo) {
  size_t sitesActive = 0;

  if (reportInfo) {
    AllocSite::printInfoHeader();
  }

  AllocSite* site = allocatedSites;
  allocatedSites = AllocSite::EndSentinel;
  while (site != AllocSite::EndSentinel) {
    AllocSite* next = site->nextNurseryAllocated;
    site->nextNurseryAllocated = nullptr;

    MOZ_ASSERT_IF(site->hasScript(),
                  site->nurseryAllocCount >= site->nurseryTenuredCount);

    bool hasPromotionRate = false;
    double promotionRate = 0.0;
    if (site->hasScript()) {
      sitesActive++;

      if (site->nurseryAllocCount > 100) {
        promotionRate =
            double(site->nurseryTenuredCount) / double(site->nurseryAllocCount);
        hasPromotionRate = true;
        site->updateStateOnMinorGC(promotionRate);
      }
    }

    if (reportInfo) {
      site->printInfo(hasPromotionRate, promotionRate);
    }

    site->resetNurseryAllocations();

    site = next;
  }

  // Optimized sites don't end up on the list if it is only used from optimized
  // JIT code so process them here.
  for (ZonesIter zone(gc, SkipAtoms); !zone.done(); zone.next()) {
    AllocSite* site = zone->optimizedAllocSite();
    if (site->hasNurseryAllocations()) {
      if (reportInfo) {
        site->printInfo(false, 0.0);
      }
      site->resetNurseryAllocations();
    }
  }

  if (reportInfo) {
    AllocSite::printInfoFooter(sitesActive);
  }
}

AllocSite::Kind AllocSite::kind() const {
  if (hasScript()) {
    return Kind::Normal;
  }

  if (this == zone()->unknownAllocSite()) {
    return Kind::Unknown;
  }

  MOZ_ASSERT(this == zone()->optimizedAllocSite());
  return Kind::Optimized;
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

  bool highPromotionRate = promotionRate >= 0.9;

  switch (state_) {
    case State::Unknown:
      if (highPromotionRate) {
        state_ = State::LongLived;
      } else {
        state_ = State::ShortLived;
      }
      break;

    case State::ShortLived: {
      if (highPromotionRate) {
        state_ = State::Unknown;
      }
      break;
    }

    case State::LongLived: {
      if (!highPromotionRate) {
        state_ = State::Unknown;
      }
      break;
    }
  }
}

/* static */
void AllocSite::printInfoHeader() {
  fprintf(stderr, "Pretenuring info after minor GC:\n");
}

/* static */
void AllocSite::printInfoFooter(size_t sitesActive) {
  fprintf(stderr, "  %zu sites active\n", sitesActive);
}

void AllocSite::printInfo(bool hasPromotionRate, double promotionRate) const {
  // Zone.
  fprintf(stderr, "  %p %p", this, zone());

  // Script, or which kind of catch-all site this is.
  if (!hasScript()) {
    fprintf(stderr, " %16s",
            kind() == Kind::Optimized ? "optimized" : "unknown");
  } else {
    fprintf(stderr, " %16p", script());
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

  // Current state for sites associated with a script.
  if (hasScript()) {
    fprintf(stderr, " %10s", stateName());
  }

  fprintf(stderr, "\n");
}

const char* AllocSite::stateName() const {
  switch (state_) {
    case State::ShortLived:
      return "ShortLived";
    case State::Unknown:
      return "Unknown";
    case State::LongLived:
      return "LongLived";
  }

  MOZ_CRASH("Unknown state");
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Pretenuring.
 *
 * Some kinds of GC cells can be allocated in either the nursery or the tenured
 * heap. The pretenuring system decides where to allocate such cells based on
 * their expected lifetime with the aim of minimising total collection time.
 *
 * Lifetime is predicted based on data gathered about the cells' allocation
 * site. This data is gathered in the middle JIT tiers, after code has stopped
 * executing in the interpreter and before we generate fully optimized code.
 */

#ifndef gc_Pretenuring_h
#define gc_Pretenuring_h

#include "mozilla/Atomics.h"

#include "gc/AllocKind.h"
#include "js/TypeDecls.h"

namespace js {
namespace gc {

// Information about an allocation site.
//
// Nursery cells contain a pointer to one of these in their cell header (stored
// before the cell). The site can relate to either for a specific bytecode
// instruction or can be a catch-all instance for unknown sites or optimized
// code.
class AllocSite {
 public:
  enum class State : uint32_t { ShortLived, Unknown, LongLived };

 private:
  JS::Zone* const zone_;

  // Pointer to the JSScript that owns this allocation site or null for unknown
  // sites. This is used when we need to invalidate the script.
  JSScript* script_ = nullptr;

  // The state is atomic as it can be read off-thread by warp transpilation.
  mozilla::Atomic<State, mozilla::ReleaseAcquire> state_{State::Unknown};

 public:
  // Create a dummy site to use for unknown allocations.
  explicit AllocSite(JS::Zone* zone) : zone_(zone) {}

  // Create a site for an opcode in the given script.
  AllocSite(JS::Zone* zone, JSScript* script) : zone_(zone), script_(script) {}

  JS::Zone* zone() const { return zone_; }
  bool hasScript() const { return script_; }
  JSScript* script() const { return script_; }

  // Whether allocations at this site should be allocated in the nursery or the
  // tenured heap.
  InitialHeap initialHeap() const {
    return state_ == State::LongLived ? TenuredHeap : DefaultHeap;
  }
};

// Pretenuring information stored per zone.
struct PretenuringZone {
  explicit PretenuringZone(JS::Zone* zone)
      : unknownAllocSite(zone), optimizedAllocSite(zone) {}

  // Catch-all allocation site instance used when the actual site is unknown, or
  // when optimized JIT code allocates a GC thing that's not handled by the
  // pretenuring system.
  AllocSite unknownAllocSite;

  // Catch-all allocation instance used by optimized JIT code when allocating GC
  // things that are handled by the pretenuring system.  Allocation counts are
  // not recorded by optimized JIT code.
  AllocSite optimizedAllocSite;
};

}  // namespace gc
}  // namespace js

#endif /* gc_Pretenuring_h */

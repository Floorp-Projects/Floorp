/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_InvalidatingFuse_h
#define vm_InvalidatingFuse_h

#include "gc/Barrier.h"
#include "gc/SweepingAPI.h"
#include "vm/GuardFuse.h"
#include "vm/JSScript.h"

namespace js {

// [SMDOC] Invalidating Fuses
//
// An invalidating fuse will invalidate a set of dependent IonScripts when the
// fuse is popped. In this way Ion can choose to ignore fuse guarded
// possibilities when doing compilation.
class InvalidatingFuse : public GuardFuse {
 public:
  // Register a script's IonScript as having a dependency on this fuse.
  virtual bool addFuseDependency(JSContext* cx, Handle<JSScript*> script) = 0;
};

// [SMDOC] Invalidating Runtime Fuses
//
// A specialized sublass for handling runtime wide fuses. This provides a
// version of addFuseDependency which records scripts into sets associated with
// their home zone, and invalidates all sets across all zones linked to this
// specific fuse.
class InvalidatingRuntimeFuse : public InvalidatingFuse {
 public:
  virtual bool addFuseDependency(JSContext* cx,
                                 Handle<JSScript*> script) override;
  virtual void popFuse(JSContext* cx) override;
};

// A (weak) set of scripts which are dependent on an associated fuse.
//
// These are typically stored in a vector at the moment, due to the low number
// of invalidating fuses, and so the associated fuse is stored along with the
// set.
//
// Because it uses JS::WeakCache, GC tracing is taken care of without any need
// for tracing in this class.
class DependentScriptSet {
 public:
  DependentScriptSet(JSContext* cx, InvalidatingFuse* fuse);

  InvalidatingFuse* associatedFuse;
  bool addScriptForFuse(InvalidatingFuse* fuse, Handle<JSScript*> script);
  void invalidateForFuse(JSContext* cx, InvalidatingFuse* fuse);

 private:
  using WeakScriptSet = GCHashSet<WeakHeapPtr<JSScript*>,
                                  StableCellHasher<WeakHeapPtr<JSScript*>>,
                                  js::SystemAllocPolicy>;
  js::WeakCache<WeakScriptSet> weakScripts;
};

}  // namespace js

#endif  // vm_InvalidatingFuse_h

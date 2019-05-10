/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_FreeOp_h
#define gc_FreeOp_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "jit/ExecutableAllocator.h"  // jit::JitPoisonRangeVector
#include "js/AllocPolicy.h"           // SystemAllocPolicy
#include "js/MemoryFunctions.h"       // JSFreeOp
#include "js/Utility.h"               // AutoEnterOOMUnsafeRegion, js_free
#include "js/Vector.h"                // js::Vector

struct JSRuntime;

namespace js {

enum class MemoryUse : uint8_t;

/*
 * A FreeOp can do one thing: free memory. For convenience, it has delete_
 * convenience methods that also call destructors.
 *
 * FreeOp is passed to finalizers and other sweep-phase hooks so that we do not
 * need to pass a JSContext to those hooks.
 */
class FreeOp : public JSFreeOp {
  Vector<void*, 0, SystemAllocPolicy> freeLaterList;
  jit::JitPoisonRangeVector jitPoisonRanges;
  const bool isDefault;

 public:
  static FreeOp* get(JSFreeOp* fop) { return static_cast<FreeOp*>(fop); }

  explicit FreeOp(JSRuntime* maybeRuntime, bool isDefault = false);
  ~FreeOp();

  bool onMainThread() const { return runtime_ != nullptr; }

  bool maybeOnHelperThread() const {
    // Sometimes background finalization happens on the main thread so
    // runtime_ being null doesn't always mean we are off thread.
    return !runtime_;
  }

  bool isDefaultFreeOp() const { return isDefault; }

  void free_(void* p) { js_free(p); }

  // Free memory that was associated with a GC thing using js::AddCellMemory.
  void free_(gc::Cell* cell, void* p, size_t nbytes, MemoryUse use);

  // Queue an allocation to be freed when the FreeOp is destroyed.
  //
  // This should not be called on the default FreeOps returned by
  // JSRuntime/JSContext::defaultFreeOp() since these are not destroyed until
  // the runtime itself is destroyed.
  //
  // This is used to ensure that copy-on-write object elements are not freed
  // until all objects that refer to them have been finalized.
  void freeLater(void* p);

  // Free memory that was associated with a GC thing using js::AddCellMemory.
  void freeLater(gc::Cell* cell, void* p, size_t nbytes, MemoryUse use);

  bool appendJitPoisonRange(const jit::JitPoisonRange& range) {
    // FreeOps other than the defaultFreeOp() are constructed on the stack,
    // and won't hold onto the pointers to free indefinitely.
    MOZ_ASSERT(!isDefaultFreeOp());

    return jitPoisonRanges.append(range);
  }

  template <class T>
  void delete_(T* p) {
    if (p) {
      p->~T();
      free_(p);
    }
  }

  // Delete a C++ object that was associated with a GC thing using
  // js::AddCellMemory. The size is determined by the type T.
  template <class T>
  void delete_(gc::Cell* cell, T* p, MemoryUse use) {
    delete_(cell, p, sizeof(T), use);
  }

  // Delete a C++ object that was associated with a GC thing using
  // js::AddCellMemory. The size of the allocation is passed in to allow for
  // allocations with trailing data after the object.
  template <class T>
  void delete_(gc::Cell* cell, T* p, size_t nbytes, MemoryUse use) {
    if (p) {
      p->~T();
      free_(cell, p, nbytes, use);
    }
  }
};

}  // namespace js

#endif  // gc_FreeOp_h

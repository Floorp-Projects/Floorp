/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitZone_h
#define jit_JitZone_h

#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "jit/ExecutableAllocator.h"
#include "jit/ICStubSpace.h"
#include "jit/Invalidation.h"
#include "jit/JitScript.h"
#include "js/AllocPolicy.h"
#include "js/GCHashTable.h"
#include "js/HashTable.h"
#include "js/TracingAPI.h"
#include "js/TypeDecls.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "threading/ProtectedData.h"

namespace JS {
struct CodeSizes;
}

namespace js {
namespace jit {

enum class CacheKind : uint8_t;
class CacheIRStubInfo;
class JitCode;
class JitScript;

enum class ICStubEngine : uint8_t {
  // Baseline IC, see BaselineIC.h.
  Baseline = 0,

  // Ion IC, see IonIC.h.
  IonIC
};

struct CacheIRStubKey : public DefaultHasher<CacheIRStubKey> {
  struct Lookup {
    CacheKind kind;
    ICStubEngine engine;
    const uint8_t* code;
    uint32_t length;

    Lookup(CacheKind kind, ICStubEngine engine, const uint8_t* code,
           uint32_t length)
        : kind(kind), engine(engine), code(code), length(length) {}
  };

  static HashNumber hash(const Lookup& l);
  static bool match(const CacheIRStubKey& entry, const Lookup& l);

  UniquePtr<CacheIRStubInfo, JS::FreePolicy> stubInfo;

  explicit CacheIRStubKey(CacheIRStubInfo* info) : stubInfo(info) {}
  CacheIRStubKey(CacheIRStubKey&& other)
      : stubInfo(std::move(other.stubInfo)) {}

  void operator=(CacheIRStubKey&& other) {
    stubInfo = std::move(other.stubInfo);
  }
};

struct BaselineCacheIRStubCodeMapGCPolicy {
  static bool traceWeak(JSTracer* trc, CacheIRStubKey*,
                        WeakHeapPtr<JitCode*>* value) {
    return TraceWeakEdge(trc, value, "traceWeak");
  }
};

enum JitScriptFilter : bool { SkipDyingScripts, IncludeDyingScripts };

class JitZone {
  // Allocated space for optimized baseline stubs.
  OptimizedICStubSpace optimizedStubSpace_;

  // Set of CacheIRStubInfo instances used by Ion stubs in this Zone.
  using IonCacheIRStubInfoSet =
      HashSet<CacheIRStubKey, CacheIRStubKey, SystemAllocPolicy>;
  IonCacheIRStubInfoSet ionCacheIRStubInfoSet_;

  // Map CacheIRStubKey to shared JitCode objects.
  using BaselineCacheIRStubCodeMap =
      GCHashMap<CacheIRStubKey, WeakHeapPtr<JitCode*>, CacheIRStubKey,
                SystemAllocPolicy, BaselineCacheIRStubCodeMapGCPolicy>;
  BaselineCacheIRStubCodeMap baselineCacheIRStubCodes_;

  // Executable allocator for all code except wasm code.
  MainThreadData<ExecutableAllocator> execAlloc_;

  // HashMap that maps scripts to compilations inlining those scripts.
  using InlinedScriptMap =
      GCHashMap<WeakHeapPtr<BaseScript*>, RecompileInfoVector,
                StableCellHasher<WeakHeapPtr<BaseScript*>>, SystemAllocPolicy>;
  InlinedScriptMap inlinedCompilations_;

  mozilla::LinkedList<JitScript> jitScripts_;

  mozilla::Maybe<IonCompilationId> currentCompilationId_;
  bool keepJitScripts_ = false;

 public:
  ~JitZone() {
    MOZ_ASSERT(jitScripts_.isEmpty());
    MOZ_ASSERT(!keepJitScripts_);
  }

  void traceWeak(JSTracer* trc, Zone* zone);

  void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              JS::CodeSizes* code, size_t* jitZone,
                              size_t* baselineStubsOptimized) const;

  OptimizedICStubSpace* optimizedStubSpace() { return &optimizedStubSpace_; }

  JitCode* getBaselineCacheIRStubCode(const CacheIRStubKey::Lookup& key,
                                      CacheIRStubInfo** stubInfo) {
    auto p = baselineCacheIRStubCodes_.lookup(key);
    if (p) {
      *stubInfo = p->key().stubInfo.get();
      return p->value();
    }
    *stubInfo = nullptr;
    return nullptr;
  }
  [[nodiscard]] bool putBaselineCacheIRStubCode(
      const CacheIRStubKey::Lookup& lookup, CacheIRStubKey& key,
      JitCode* stubCode) {
    auto p = baselineCacheIRStubCodes_.lookupForAdd(lookup);
    MOZ_ASSERT(!p);
    return baselineCacheIRStubCodes_.add(p, std::move(key), stubCode);
  }

  CacheIRStubInfo* getIonCacheIRStubInfo(const CacheIRStubKey::Lookup& key) {
    IonCacheIRStubInfoSet::Ptr p = ionCacheIRStubInfoSet_.lookup(key);
    return p ? p->stubInfo.get() : nullptr;
  }
  [[nodiscard]] bool putIonCacheIRStubInfo(const CacheIRStubKey::Lookup& lookup,
                                           CacheIRStubKey& key) {
    IonCacheIRStubInfoSet::AddPtr p =
        ionCacheIRStubInfoSet_.lookupForAdd(lookup);
    MOZ_ASSERT(!p);
    return ionCacheIRStubInfoSet_.add(p, std::move(key));
  }
  void purgeIonCacheIRStubInfo() { ionCacheIRStubInfoSet_.clearAndCompact(); }

  ExecutableAllocator& execAlloc() { return execAlloc_.ref(); }
  const ExecutableAllocator& execAlloc() const { return execAlloc_.ref(); }

  [[nodiscard]] bool addInlinedCompilation(const RecompileInfo& info,
                                           JSScript* inlined);

  RecompileInfoVector* maybeInlinedCompilations(JSScript* inlined) {
    auto p = inlinedCompilations_.lookup(inlined);
    return p ? &p->value() : nullptr;
  }

  void removeInlinedCompilations(JSScript* inlined) {
    inlinedCompilations_.remove(inlined);
  }

  void registerJitScript(JitScript* script) { jitScripts_.insertBack(script); }

  // Iterate over all JitScripts in this zone calling |f| on each, allowing |f|
  // to remove the script. The template parameter |filter| controls whether to
  // include dying JitScripts during GC sweeping. Be careful when using this not
  // to let GC things reachable from the JitScript escape - they may be gray.
  template <JitScriptFilter filter = SkipDyingScripts, typename F>
  void forEachJitScript(F&& f) {
    JitScript* script = jitScripts_.getFirst();
    while (script) {
      JitScript* next = script->getNext();
      if (filter == IncludeDyingScripts ||
          !gc::IsAboutToBeFinalizedUnbarriered(script->owningScript())) {
        f(script);
      }
      script = next;
    }
  }

  // Like forEachJitScript above, but abort if |f| returns false.
  template <JitScriptFilter filter = SkipDyingScripts, typename F>
  bool forEachJitScriptFallible(F&& f) {
    JitScript* script = jitScripts_.getFirst();
    while (script) {
      JitScript* next = script->getNext();
      if (filter == IncludeDyingScripts ||
          !gc::IsAboutToBeFinalizedUnbarriered(script->owningScript())) {
        if (!f(script)) {
          return false;
        }
      }
      script = next;
    }
    return true;
  }

  bool keepJitScripts() const { return keepJitScripts_; }
  void setKeepJitScripts(bool keep) { keepJitScripts_ = keep; }

  mozilla::Maybe<IonCompilationId> currentCompilationId() const {
    return currentCompilationId_;
  }
  mozilla::Maybe<IonCompilationId>& currentCompilationIdRef() {
    return currentCompilationId_;
  }
};

}  // namespace jit
}  // namespace js

#endif /* jit_JitZone_h */

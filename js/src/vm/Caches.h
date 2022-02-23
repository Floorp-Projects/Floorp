/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Caches_h
#define vm_Caches_h

#include <iterator>
#include <new>

#include "frontend/SourceNotes.h"  // SrcNote
#include "gc/Tracer.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/UniquePtr.h"
#include "util/Memory.h"
#include "vm/ArrayObject.h"
#include "vm/JSAtom.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/NativeObject.h"
#include "vm/StencilCache.h"  // js::StencilCache

namespace js {

/*
 * GetSrcNote cache to avoid O(n^2) growth in finding a source note for a
 * given pc in a script. We use the script->code pointer to tag the cache,
 * instead of the script address itself, so that source notes are always found
 * by offset from the bytecode with which they were generated.
 */
struct GSNCache {
  typedef HashMap<jsbytecode*, const SrcNote*, PointerHasher<jsbytecode*>,
                  SystemAllocPolicy>
      Map;

  jsbytecode* code;
  Map map;

  GSNCache() : code(nullptr) {}

  void purge();
};

struct EvalCacheEntry {
  JSLinearString* str;
  JSScript* script;
  JSScript* callerScript;
  jsbytecode* pc;

  // We sweep this cache after a nursery collection to update entries with
  // string keys that have been tenured.
  //
  // The entire cache is purged on a major GC, so we don't need to sweep it
  // then.
  bool traceWeak(JSTracer* trc) {
    MOZ_ASSERT(trc->kind() == JS::TracerKind::MinorSweeping);
    return TraceManuallyBarrieredWeakEdge(trc, &str, "EvalCacheEntry::str");
  }
};

struct EvalCacheLookup {
  explicit EvalCacheLookup(JSContext* cx) : str(cx), callerScript(cx) {}
  RootedLinearString str;
  RootedScript callerScript;
  MOZ_INIT_OUTSIDE_CTOR jsbytecode* pc;
};

struct EvalCacheHashPolicy {
  using Lookup = EvalCacheLookup;

  static HashNumber hash(const Lookup& l);
  static bool match(const EvalCacheEntry& entry, const EvalCacheLookup& l);
};

using EvalCache =
    GCHashSet<EvalCacheEntry, EvalCacheHashPolicy, SystemAllocPolicy>;

// Cache for AtomizeString, mapping JSLinearString* to the corresponding
// JSAtom*. Also used by nursery GC to de-duplicate strings to atoms.
// Purged on minor and major GC.
class StringToAtomCache {
  using Map = HashMap<JSLinearString*, JSAtom*, PointerHasher<JSLinearString*>,
                      SystemAllocPolicy>;
  Map map_;

 public:
  // Don't use the cache for short strings. Hashing them is less expensive.
  static constexpr size_t MinStringLength = 30;

  JSAtom* lookup(JSLinearString* s) {
    MOZ_ASSERT(!s->isAtom());
    if (!s->inStringToAtomCache()) {
      MOZ_ASSERT(!map_.lookup(s));
      return nullptr;
    }

    MOZ_ASSERT(s->length() >= MinStringLength);

    auto p = map_.lookup(s);
    JSAtom* atom = p ? p->value() : nullptr;
    MOZ_ASSERT_IF(atom, EqualStrings(s, atom));
    return atom;
  }

  void maybePut(JSLinearString* s, JSAtom* atom) {
    MOZ_ASSERT(!s->isAtom());
    if (s->length() < MinStringLength) {
      return;
    }
    if (!map_.putNew(s, atom)) {
      return;
    }
    s->setInStringToAtomCache();
  }

  void purge() { map_.clearAndCompact(); }
};

class RuntimeCaches {
 public:
  GSNCache gsnCache;
  UncompressedSourceCache uncompressedSourceCache;
  EvalCache evalCache;
  StringToAtomCache stringToAtomCache;

  // This cache is used to store the result of delazification compilations which
  // might be happening off-thread. The main-thread will concurrently read the
  // content of this cache to avoid delazification, or fallback on running the
  // delazification on the main-thread.
  //
  // Main-thread results are not stored in the StencilCache as there is no other
  // consumer.
  StencilCache delazificationCache;

  void sweepAfterMinorGC(JSTracer* trc) { evalCache.traceWeak(trc); }
#ifdef JSGC_HASH_TABLE_CHECKS
  void checkEvalCacheAfterMinorGC();
#endif

  void purgeForCompaction() {
    evalCache.clear();
    stringToAtomCache.purge();
  }

  void purgeStencils() { delazificationCache.clearAndDisable(); }

  void purge() {
    purgeForCompaction();
    gsnCache.purge();
    uncompressedSourceCache.purge();
    purgeStencils();
  }
};

}  // namespace js

#endif /* vm_Caches_h */

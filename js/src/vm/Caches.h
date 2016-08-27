/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Caches_h
#define vm_Caches_h

#include "jsatom.h"
#include "jsbytecode.h"
#include "jsobj.h"
#include "jsscript.h"

#include "ds/FixedSizeHash.h"
#include "frontend/SourceNotes.h"
#include "gc/Tracer.h"
#include "js/RootingAPI.h"
#include "js/UniquePtr.h"
#include "vm/NativeObject.h"

namespace js {

/*
 * GetSrcNote cache to avoid O(n^2) growth in finding a source note for a
 * given pc in a script. We use the script->code pointer to tag the cache,
 * instead of the script address itself, so that source notes are always found
 * by offset from the bytecode with which they were generated.
 */
struct GSNCache {
    typedef HashMap<jsbytecode*,
                    jssrcnote*,
                    PointerHasher<jsbytecode*, 0>,
                    SystemAllocPolicy> Map;

    jsbytecode*     code;
    Map             map;

    GSNCache() : code(nullptr) { }

    void purge();
};

/*
 * EnvironmentCoordinateName cache to avoid O(n^2) growth in finding the name
 * associated with a given aliasedvar operation.
 */
struct EnvironmentCoordinateNameCache {
    typedef HashMap<uint32_t,
                    jsid,
                    DefaultHasher<uint32_t>,
                    SystemAllocPolicy> Map;

    Shape* shape;
    Map map;

    EnvironmentCoordinateNameCache() : shape(nullptr) {}
    void purge();
};

struct EvalCacheEntry
{
    JSLinearString* str;
    JSScript* script;
    JSScript* callerScript;
    jsbytecode* pc;
};

struct EvalCacheLookup
{
    explicit EvalCacheLookup(JSContext* cx) : str(cx), callerScript(cx) {}
    RootedLinearString str;
    RootedScript callerScript;
    JSVersion version;
    jsbytecode* pc;
};

struct EvalCacheHashPolicy
{
    typedef EvalCacheLookup Lookup;

    static HashNumber hash(const Lookup& l);
    static bool match(const EvalCacheEntry& entry, const EvalCacheLookup& l);
};

typedef HashSet<EvalCacheEntry, EvalCacheHashPolicy, SystemAllocPolicy> EvalCache;

struct LazyScriptHashPolicy
{
    struct Lookup {
        JSContext* cx;
        LazyScript* lazy;

        Lookup(JSContext* cx, LazyScript* lazy)
          : cx(cx), lazy(lazy)
        {}
    };

    static const size_t NumHashes = 3;

    static void hash(const Lookup& lookup, HashNumber hashes[NumHashes]);
    static bool match(JSScript* script, const Lookup& lookup);

    // Alternate methods for use when removing scripts from the hash without an
    // explicit LazyScript lookup.
    static void hash(JSScript* script, HashNumber hashes[NumHashes]);
    static bool match(JSScript* script, JSScript* lookup) { return script == lookup; }

    static void clear(JSScript** pscript) { *pscript = nullptr; }
    static bool isCleared(JSScript* script) { return !script; }
};

typedef FixedSizeHashSet<JSScript*, LazyScriptHashPolicy, 769> LazyScriptCache;

class PropertyIteratorObject;

class NativeIterCache
{
    static const size_t SIZE = size_t(1) << 8;

    /* Cached native iterators. */
    PropertyIteratorObject* data[SIZE];

    static size_t getIndex(uint32_t key) {
        return size_t(key) % SIZE;
    }

  public:
    /* Native iterator most recently started. */
    PropertyIteratorObject* last;

    NativeIterCache()
      : last(nullptr)
    {
        mozilla::PodArrayZero(data);
    }

    void purge() {
        last = nullptr;
        mozilla::PodArrayZero(data);
    }

    PropertyIteratorObject* get(uint32_t key) const {
        return data[getIndex(key)];
    }

    void set(uint32_t key, PropertyIteratorObject* iterobj) {
        data[getIndex(key)] = iterobj;
    }
};

class MathCache;

class ContextCaches
{
    UniquePtr<js::MathCache> mathCache_;

    js::MathCache* createMathCache(JSContext* cx);

  public:
    js::GSNCache gsnCache;
    js::EnvironmentCoordinateNameCache envCoordinateNameCache;
    js::NativeIterCache nativeIterCache;
    js::UncompressedSourceCache uncompressedSourceCache;
    js::EvalCache evalCache;
    js::LazyScriptCache lazyScriptCache;

    bool init();

    js::MathCache* getMathCache(JSContext* cx) {
        return mathCache_ ? mathCache_.get() : createMathCache(cx);
    }
    js::MathCache* maybeGetMathCache() {
        return mathCache_.get();
    }
};

} // namespace js

#endif /* vm_Caches_h */

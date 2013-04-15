/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef pcscriptcache_h__
#define pcscriptcache_h__

// Defines a fixed-size hash table solely for the purpose of caching ion::GetPcScript().
// One cache is attached to each JSRuntime; it functions as if cleared on GC.

struct JSRuntime;

namespace js {
namespace ion {

struct PcScriptCacheEntry
{
    uint8_t *returnAddress; // Key into the hash table.
    jsbytecode *pc;         // Cached PC.
    RawScript script;       // Cached script.
};

struct PcScriptCache
{
    static const uint32_t Length = 73;

    // GC number at the time the cache was filled or created.
    // Storing and checking against this number allows us to not bother
    // clearing this cache on every GC -- only when actually necessary.
    uint64_t gcNumber;

    // List of cache entries.
    PcScriptCacheEntry entries[Length];

    void clear(uint64_t gcNumber) {
        for (uint32_t i = 0; i < Length; i++)
            entries[i].returnAddress = NULL;
        this->gcNumber = gcNumber;
    }

    // Get a value from the cache. May perform lazy allocation.
    // Defined in PcScriptCache-inl.h.
    bool get(JSRuntime *rt, uint32_t hash, uint8_t *addr,
             JSScript **scriptRes, jsbytecode **pcRes);

    void add(uint32_t hash, uint8_t *addr, jsbytecode *pc, RawScript script) {
        entries[hash].returnAddress = addr;
        entries[hash].pc = pc;
        entries[hash].script = script;
    }

    static uint32_t Hash(uint8_t *addr) {
        uint32_t key = (uint32_t)((uintptr_t)addr);
        return ((key >> 3) * 2654435761u) % Length;
    }
};

} // namespace ion
} // namespace js

#endif // pcscriptcache_h__

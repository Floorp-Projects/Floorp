/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_PcScriptCache_inl_h
#define jit_PcScriptCache_inl_h

#include "PcScriptCache.h"

namespace js {
namespace jit {

// Get a value from the cache. May perform lazy allocation.
bool
PcScriptCache::get(JSRuntime *rt, uint32_t hash, uint8_t *addr,
                   JSScript **scriptRes, jsbytecode **pcRes)
{
    // If a GC occurred, lazily clear the cache now.
    if (gcNumber != rt->gcNumber) {
        clear(rt->gcNumber);
        return false;
    }

    if (entries[hash].returnAddress != addr)
        return false;

    *scriptRes = entries[hash].script;
    if (pcRes)
        *pcRes = entries[hash].pc;

    return true;
}

} // namespace jit
} // namespace js

#endif /* jit_PcScriptCache_inl_h */

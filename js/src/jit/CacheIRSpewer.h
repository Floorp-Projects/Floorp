/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CacheIRSpewer_h
#define jit_CacheIRSpewer_h

#ifdef JS_JITSPEW

#include "mozilla/Maybe.h"

#include "jit/CacheIR.h"
#include "jit/JSONPrinter.h"
#include "js/TypeDecls.h"
#include "threading/LockGuard.h"
#include "vm/MutexIDs.h"

namespace js {
namespace jit {

class CacheIRSpewer
{
    Mutex outputLock;
    Fprinter output;
    mozilla::Maybe<JSONPrinter> json;

  public:
    CacheIRSpewer();
    ~CacheIRSpewer();

    bool init();
    bool enabled() { return json.isSome(); }

    // These methods can only be called when enabled() is true.
    Mutex& lock() { MOZ_ASSERT(enabled()); return outputLock; }

    void beginCache(LockGuard<Mutex>&, const IRGenerator& generator);
    void valueProperty(LockGuard<Mutex>&, const char* name, HandleValue v);
    void attached(LockGuard<Mutex>&, const char* name);
    void endCache(LockGuard<Mutex>&);
};

CacheIRSpewer&
GetCacheIRSpewerSingleton();

} // namespace jit
} // namespace js

#endif /* JS_JITSPEW */

#endif /* jit_CacheIRSpewer_h */

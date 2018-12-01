/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineCacheIRCompiler_h
#define jit_BaselineCacheIRCompiler_h

#include "gc/Barrier.h"
#include "jit/CacheIR.h"
#include "jit/CacheIRCompiler.h"

namespace js {
namespace jit {

class ICFallbackStub;
class ICStub;

enum class BaselineCacheIRStubKind { Regular, Monitored, Updated };

ICStub* AttachBaselineCacheIRStub(JSContext* cx, const CacheIRWriter& writer,
                                  CacheKind kind,
                                  BaselineCacheIRStubKind stubKind,
                                  JSScript* outerScript, ICFallbackStub* stub,
                                  bool* attached);

}  // namespace jit
}  // namespace js

#endif /* jit_BaselineCacheIRCompiler_h */

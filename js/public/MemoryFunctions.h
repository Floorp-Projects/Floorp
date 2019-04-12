/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Low-level memory-allocation functions. */

#ifndef js_MemoryFunctions_h
#define js_MemoryFunctions_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include <stddef.h>  // size_t

#include "jstypes.h"  // JS_PUBLIC_API

struct JSContext;
struct JSRuntime;

struct JSFreeOp {
 protected:
  JSRuntime* runtime_;

  explicit JSFreeOp(JSRuntime* rt) : runtime_(rt) {}

 public:
  JSRuntime* runtime() const {
    MOZ_ASSERT(runtime_);
    return runtime_;
  }
};

extern JS_PUBLIC_API void* JS_malloc(JSContext* cx, size_t nbytes);

extern JS_PUBLIC_API void* JS_realloc(JSContext* cx, void* p, size_t oldBytes,
                                      size_t newBytes);

/**
 * A wrapper for |js_free(p)| that may delay |js_free(p)| invocation as a
 * performance optimization.  |cx| may be nullptr.
 */
extern JS_PUBLIC_API void JS_free(JSContext* cx, void* p);

/**
 * Same as above, but for buffers that will be used with the BYOB
 * (Bring Your Own Buffer) JSString creation functions, such as
 * JS_NewLatin1String and JS_NewUCString
 */
extern JS_PUBLIC_API void* JS_string_malloc(JSContext* cx, size_t nbytes);

extern JS_PUBLIC_API void* JS_string_realloc(JSContext* cx, void* p,
                                             size_t oldBytes, size_t newBytes);

extern JS_PUBLIC_API void JS_string_free(JSContext* cx, void* p);

/**
 * A wrapper for |js_free(p)| that may delay |js_free(p)| invocation as a
 * performance optimization as specified by the given JSFreeOp instance.
 */
extern JS_PUBLIC_API void JS_freeop(JSFreeOp* fop, void* p);

extern JS_PUBLIC_API void JS_updateMallocCounter(JSContext* cx, size_t nbytes);

#endif /* js_MemoryFunctions_h */

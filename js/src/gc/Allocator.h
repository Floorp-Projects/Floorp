/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Allocator_h
#define gc_Allocator_h

#include "gc/Heap.h"
#include "js/RootingAPI.h"

namespace JS {
class Symbol;
} // namespace JS
class JSExternalString;
class JSFatInlineString;
class JSObject;
class JSScript;
class JSString;

namespace js {
struct Class;
class BaseShape;
class LazyScript;
class ObjectGroup;
class Shape;
namespace jit {
class JitCode;
} // namespace jit

template <typename, AllowGC allowGC = CanGC>
JSObject *
Allocate(ExclusiveContext *cx, gc::AllocKind kind, size_t nDynamicSlots, gc::InitialHeap heap,
         const Class *clasp);

template <typename T, AllowGC allowGC = CanGC>
T *
Allocate(ExclusiveContext *cx);

namespace gc {

template <AllowGC allowGC>
NativeObject *
AllocateObjectForCacheHit(JSContext *cx, AllocKind kind, InitialHeap heap, const Class *clasp);

} // namespace gc
} // namespace js

#endif // gc_Allocator_h

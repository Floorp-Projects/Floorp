/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Runtime_inl_h
#define vm_Runtime_inl_h

#include "vm/Runtime.h"

#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsworkers.h"

#include "builtin/Object.h" // For js::obj_construct
#include "frontend/ParseMaps.h"
#include "jit/IonFrames.h" // For GetPcScript
#include "vm/Interpreter.h"
#include "vm/Probes.h"
#include "vm/RegExpObject.h"

#include "jsgcinlines.h"

#include "vm/ObjectImpl-inl.h"

namespace js {

inline bool
NewObjectCache::lookupProto(Class *clasp, JSObject *proto, gc::AllocKind kind, EntryIndex *pentry)
{
    JS_ASSERT(!proto->is<GlobalObject>());
    return lookup(clasp, proto, kind, pentry);
}

inline bool
NewObjectCache::lookupGlobal(Class *clasp, js::GlobalObject *global, gc::AllocKind kind, EntryIndex *pentry)
{
    return lookup(clasp, global, kind, pentry);
}

inline void
NewObjectCache::fillGlobal(EntryIndex entry, Class *clasp, js::GlobalObject *global, gc::AllocKind kind, JSObject *obj)
{
    //JS_ASSERT(global == obj->getGlobal());
    return fill(entry, clasp, global, kind, obj);
}

inline void
NewObjectCache::copyCachedToObject(JSObject *dst, JSObject *src, gc::AllocKind kind)
{
    js_memcpy(dst, src, gc::Arena::thingSize(kind));
#ifdef JSGC_GENERATIONAL
    Shape::writeBarrierPost(dst->shape_, &dst->shape_);
    types::TypeObject::writeBarrierPost(dst->type_, &dst->type_);
#endif
}

inline JSObject *
NewObjectCache::newObjectFromHit(JSContext *cx, EntryIndex entry_, js::gc::InitialHeap heap)
{
    // The new object cache does not account for metadata attached via callbacks.
    JS_ASSERT(!cx->compartment()->objectMetadataCallback);

    JS_ASSERT(unsigned(entry_) < mozilla::ArrayLength(entries));
    Entry *entry = &entries[entry_];

    JSObject *obj = js_NewGCObject<NoGC>(cx, entry->kind, heap);
    if (obj) {
        copyCachedToObject(obj, reinterpret_cast<JSObject *>(&entry->templateObject), entry->kind);
        Probes::createObject(cx, obj);
        return obj;
    }

    return NULL;
}

inline
ThreadDataIter::ThreadDataIter(JSRuntime *rt)
{
#ifdef JS_WORKER_THREADS
    // Only allow iteration over a runtime's threads when those threads are
    // paused, to avoid racing when reading data from the PerThreadData.
    JS_ASSERT_IF(rt->workerThreadState, rt->workerThreadState->shouldPause);
#endif
    iter = rt->threadList.getFirst();
}

}  /* namespace js */

#endif /* vm_Runtime_inl_h */

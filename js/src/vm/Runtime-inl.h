/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Runtime_inl_h
#define vm_Runtime_inl_h

#include "vm/Runtime.h"

#include "jscompartment.h"

#include "vm/Probes.h"

#include "jsgcinlines.h"

namespace js {

inline bool
NewObjectCache::lookupProto(const Class *clasp, JSObject *proto, gc::AllocKind kind, EntryIndex *pentry)
{
    JS_ASSERT(!proto->is<GlobalObject>());
    return lookup(clasp, proto, kind, pentry);
}

inline bool
NewObjectCache::lookupGlobal(const Class *clasp, js::GlobalObject *global, gc::AllocKind kind, EntryIndex *pentry)
{
    return lookup(clasp, global, kind, pentry);
}

inline void
NewObjectCache::fillGlobal(EntryIndex entry, const Class *clasp, js::GlobalObject *global, gc::AllocKind kind, JSObject *obj)
{
    //JS_ASSERT(global == obj->getGlobal());
    return fill(entry, clasp, global, kind, obj);
}

inline JSObject *
NewObjectCache::newObjectFromHit(JSContext *cx, EntryIndex entry_, js::gc::InitialHeap heap)
{
    // The new object cache does not account for metadata attached via callbacks.
    JS_ASSERT(!cx->compartment()->hasObjectMetadataCallback());

    JS_ASSERT(unsigned(entry_) < mozilla::ArrayLength(entries));
    Entry *entry = &entries[entry_];

    JSObject *templateObj = reinterpret_cast<JSObject *>(&entry->templateObject);

    // Do an end run around JSObject::type() to avoid doing AutoUnprotectCell
    // on the templateObj, which is not a GC thing and can't use runtimeFromAnyThread.
    types::TypeObject *type = templateObj->type_;

    if (type->shouldPreTenure())
        heap = gc::TenuredHeap;

    if (cx->runtime()->upcomingZealousGC())
        return nullptr;

    JSObject *obj = js_NewGCObject<NoGC>(cx, entry->kind, heap);
    if (obj) {
        copyCachedToObject(obj, templateObj, entry->kind);
        probes::CreateObject(cx, obj);
        return obj;
    }

    return nullptr;
}

}  /* namespace js */

#endif /* vm_Runtime_inl_h */

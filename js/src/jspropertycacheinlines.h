/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jspropertycacheinlines_h___
#define jspropertycacheinlines_h___

#include "jslock.h"
#include "jspropertycache.h"
#include "jsscope.h"

/*
 * This method is designed to inline the fast path in js_Interpret, so it makes
 * "just-so" restrictions on parameters, e.g. pobj and obj should not be the
 * same variable, since for JOF_PROP-mode opcodes, obj must not be changed
 * because of a cache miss.
 *
 * On return, if name is null then obj points to the scope chain element in
 * which the property was found, pobj is locked, and entry is valid. If name is
 * non-null then no object is locked but entry is still set correctly for use,
 * e.g., by PropertyCache::fill and name should be used as the id to find.
 *
 * We must lock pobj on a hit in order to close races with threads that might
 * be deleting a property from its scope, or otherwise invalidating property
 * caches (on all threads) by re-generating JSObject::shape().
 */
JS_ALWAYS_INLINE void
js::PropertyCache::test(JSContext *cx, jsbytecode *pc, JSObject *&obj,
                        JSObject *&pobj, PropertyCacheEntry *&entry, PropertyName *&name)
{
    AssertRootingUnnecessary assert(cx);

    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));

    Shape *kshape = obj->lastProperty();
    entry = &table[hash(pc, kshape)];
    PCMETER(pctestentry = entry);
    PCMETER(tests++);
    JS_ASSERT(&obj != &pobj);
    if (entry->kpc == pc && entry->kshape == kshape) {
        JSObject *tmp;
        pobj = obj;
        if (entry->isPrototypePropertyHit() &&
            (tmp = pobj->getProto()) != NULL) {
            pobj = tmp;
        }

        if (pobj->lastProperty() == entry->pshape) {
            PCMETER(pchits++);
            PCMETER(entry->isOwnPropertyHit() || protopchits++);
            name = NULL;
            return;
        }
    }
    name = fullTest(cx, pc, &obj, &pobj, entry);
    if (name)
        PCMETER(misses++);
}

JS_ALWAYS_INLINE bool
js::PropertyCache::testForSet(JSContext *cx, jsbytecode *pc, JSObject *obj,
                              PropertyCacheEntry **entryp, JSObject **obj2p, PropertyName **namep)
{
    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));

    Shape *kshape = obj->lastProperty();
    PropertyCacheEntry *entry = &table[hash(pc, kshape)];
    *entryp = entry;
    PCMETER(pctestentry = entry);
    PCMETER(tests++);
    PCMETER(settests++);
    if (entry->kpc == pc && entry->kshape == kshape)
        return true;

    PropertyName *name = fullTest(cx, pc, &obj, obj2p, entry);
    JS_ASSERT(name);

    PCMETER(misses++);
    PCMETER(setmisses++);

    *namep = name;
    return false;
}

#endif /* jspropertycacheinlines_h___ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov <igor@mir2.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jspropertycacheinlines_h___
#define jspropertycacheinlines_h___

#include "jslock.h"
#include "jspropertycache.h"
#include "jsscope.h"

using namespace js;

/*
 * This method is designed to inline the fast path in js_Interpret, so it makes
 * "just-so" restrictions on parameters, e.g. pobj and obj should not be the
 * same variable, since for JOF_PROP-mode opcodes, obj must not be changed
 * because of a cache miss.
 *
 * On return, if atom is null then obj points to the scope chain element in
 * which the property was found, pobj is locked, and entry is valid. If atom is
 * non-null then no object is locked but entry is still set correctly for use,
 * e.g., by PropertyCache::fill and atom should be used as the id to find.
 *
 * We must lock pobj on a hit in order to close races with threads that might
 * be deleting a property from its scope, or otherwise invalidating property
 * caches (on all threads) by re-generating JSObject::shape().
 */
JS_ALWAYS_INLINE void
PropertyCache::test(JSContext *cx, jsbytecode *pc, JSObject *&obj,
                    JSObject *&pobj, PropertyCacheEntry *&entry, JSAtom *&atom)
{
    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));

    const Shape *kshape = obj->lastProperty();
    entry = &table[hash(pc, kshape)];
    PCMETER(pctestentry = entry);
    PCMETER(tests++);
    JS_ASSERT(&obj != &pobj);
    if (entry->kpc == pc && entry->kshape == kshape) {
        JSObject *tmp;
        pobj = obj;
        if (entry->vindex == 1 &&
            (tmp = pobj->getProto()) != NULL) {
            pobj = tmp;
        }

        if (pobj->lastProperty() == entry->pshape) {
            PCMETER(pchits++);
            PCMETER(!entry->vindex || protopchits++);
            atom = NULL;
            return;
        }
    }
    atom = fullTest(cx, pc, &obj, &pobj, entry);
    if (atom)
        PCMETER(misses++);
}

JS_ALWAYS_INLINE bool
PropertyCache::testForSet(JSContext *cx, jsbytecode *pc, JSObject *obj,
                          PropertyCacheEntry **entryp, JSObject **obj2p, JSAtom **atomp)
{
    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));

    const Shape *kshape = obj->lastProperty();
    PropertyCacheEntry *entry = &table[hash(pc, kshape)];
    *entryp = entry;
    PCMETER(pctestentry = entry);
    PCMETER(tests++);
    PCMETER(settests++);
    if (entry->kpc == pc && entry->kshape == kshape)
        return true;

    JSAtom *atom = fullTest(cx, pc, &obj, obj2p, entry);
    JS_ASSERT(atom);

    PCMETER(misses++);
    PCMETER(setmisses++);

    *atomp = atom;
    return false;
}

#endif /* jspropertycacheinlines_h___ */

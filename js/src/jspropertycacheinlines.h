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

#include "jsinterp.h"
#include "jslock.h"
#include "jsscope.h"

/* static */ inline bool
JSPropCacheEntry::matchShape(JSContext *cx, JSObject *obj, uint32 shape)
{
    return CX_OWNS_OBJECT_TITLE(cx, obj) && OBJ_SHAPE(obj) == shape;
}

/*
 * Property cache lookup macros. PROPERTY_CACHE_TEST is designed to inline the
 * fast path in js_Interpret, so it makes "just-so" restrictions on parameters,
 * e.g. pobj and obj should not be the same variable, since for JOF_PROP-mode
 * opcodes, obj must not be changed because of a cache miss.
 *
 * On return from PROPERTY_CACHE_TEST, if atom is null then obj points to the
 * scope chain element in which the property was found, pobj is locked, and
 * entry is valid. If atom is non-null then no object is locked but entry is
 * still set correctly for use, e.g., by js_FillPropertyCache and atom should
 * be used as the id to find.
 *
 * We must lock pobj on a hit in order to close races with threads that might
 * be deleting a property from its scope, or otherwise invalidating property
 * caches (on all threads) by re-generating scope->shape.
 */
#define PROPERTY_CACHE_TEST(cx, pc, obj, pobj, entry, atom)                   \
    do {                                                                      \
        JSPropertyCache *cache_ = &JS_PROPERTY_CACHE(cx);                     \
        uint32 kshape_ = (JS_ASSERT(OBJ_IS_NATIVE(obj)), OBJ_SHAPE(obj));     \
        entry = &cache_->table[PROPERTY_CACHE_HASH(pc, kshape_)];             \
        PCMETER(cache_->pctestentry = entry);                                 \
        PCMETER(cache_->tests++);                                             \
        JS_ASSERT(&obj != &pobj);                                             \
        if (entry->kpc == pc && entry->kshape == kshape_) {                   \
            JSObject *tmp_;                                                   \
            pobj = obj;                                                       \
            if (PCVCAP_TAG(entry->vcap) == 1 &&                               \
                (tmp_ = pobj->getProto()) != NULL) {                          \
                pobj = tmp_;                                                  \
            }                                                                 \
                                                                              \
            if (JSPropCacheEntry::matchShape(cx, pobj,                        \
                                             PCVCAP_SHAPE(entry->vcap))) {    \
                PCMETER(cache_->pchits++);                                    \
                PCMETER(!PCVCAP_TAG(entry->vcap) || cache_->protopchits++);   \
                atom = NULL;                                                  \
                break;                                                        \
            }                                                                 \
        }                                                                     \
        atom = js_FullTestPropertyCache(cx, pc, &obj, &pobj, entry);          \
        if (atom)                                                             \
            PCMETER(cache_->misses++);                                        \
    } while (0)

#endif /* jspropertycacheinlines_h___ */

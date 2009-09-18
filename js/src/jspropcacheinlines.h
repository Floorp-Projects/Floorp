/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef jspropcacheinlines_h___
#define jspropcacheinlines_h___

inline JSBool
js_IsPropertyCacheDisabled(JSContext *cx)
{
    return cx->runtime->shapeGen >= SHAPE_OVERFLOW_BIT;
}

/*
 * Add kshape rather than xor it to avoid collisions between nearby bytecode
 * that are evolving an object by setting successive properties, incrementing
 * the object's scope->shape on each set.
 */
inline jsuword
PROPERTY_CACHE_HASH(jsuword pc, jsuword kshape)
{
    return (((pc >> PROPERTY_CACHE_LOG2) ^ pc) + (kshape)) & PROPERTY_CACHE_MASK;
}

inline jsuword
PROPERTY_CACHE_HASH_PC(jsbytecode *pc, jsuword kshape)
{
    return PROPERTY_CACHE_HASH((jsuword) pc, kshape);
}

inline jsuword
PROPERTY_CACHE_HASH_ATOM(JSAtom *atom, JSObject *obj)
{
    return PROPERTY_CACHE_HASH((jsuword) atom >> 2, OBJ_SHAPE(obj));
}

/*
 * Property cache lookup.
 *
 * On return from PROPERTY_CACHE_TEST, if atom is null then obj points to the
 * scope chain element in which the property was found, pobj is locked, and
 * entry is valid. If atom is non-null then no object is locked but entry is
 * still set correctly for use, e.g., by JSPropertyCache::fill and atom should
 * be used as the id to find.
 *
 * We must lock pobj on a hit in order to close races with threads that might
 * be deleting a property from its scope, or otherwise invalidating property
 * caches (on all threads) by re-generating scope->shape.
 */
#define PROPERTY_CACHE_TEST(cx, pc, obj, pobj, entry, atom) \
    JS_PROPERTY_CACHE(cx).test(cx, pc, &obj, &pobj, &entry, &atom)

JS_ALWAYS_INLINE void
JSPropertyCache::test(JSContext *cx, jsbytecode *pc, JSObject **objp,
                      JSObject **pobjp, JSPropCacheEntry **entryp, JSAtom **atomp)
{
    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));
    JS_ASSERT(OBJ_IS_NATIVE(*objp));

    uint32 kshape = OBJ_SHAPE(*objp);
    JSPropCacheEntry *entry = &table[PROPERTY_CACHE_HASH_PC(pc, kshape)];
    PCMETER(pctestentry = entry);
    PCMETER(tests++);
    JS_ASSERT(objp != pobjp);
    if (entry->kpc == pc && entry->kshape == kshape) {
        JSObject *tmp;
        JSObject *pobj = *objp;

        JS_ASSERT(PCVCAP_TAG(entry->vcap) <= 1);
        if (PCVCAP_TAG(entry->vcap) == 1 && (tmp = pobj->getProto()) != NULL)
            pobj = tmp;

        if (JS_LOCK_OBJ_IF_SHAPE(cx, pobj, PCVCAP_SHAPE(entry->vcap))) {
            PCMETER(pchits++);
            PCMETER(!PCVCAP_TAG(entry->vcap) || protopchits++);
            *pobjp = pobj;
            *entryp = entry;
            *atomp = NULL;
            return;
        }
    }
    *atomp = fullTest(cx, pc, objp, pobjp, entryp);
    if (*atomp)
        PCMETER(misses++);
}

JS_ALWAYS_INLINE bool
JSPropertyCache::testForSet(JSContext *cx, jsbytecode *pc, JSObject **objp,
                            JSObject **pobjp, JSPropCacheEntry **entryp, JSAtom **atomp)
{
    uint32 kshape = OBJ_SHAPE(*objp);
    JSPropCacheEntry *entry = &table[PROPERTY_CACHE_HASH_PC(pc, kshape)];
    PCMETER(pctestentry = entry);
    PCMETER(tests++);
    PCMETER(settests++);

    *entryp = entry;
    if (entry->kpc == pc && entry->kshape == kshape) {
        JS_ASSERT(PCVCAP_TAG(entry->vcap) <= 1);
        if (JS_LOCK_OBJ_IF_SHAPE(cx, *objp, kshape))
            return true;
    }

    *atomp = JS_PROPERTY_CACHE(cx).fullTest(cx, pc, objp, pobjp, entryp);
    if (*atomp) {
        PCMETER(misses++);
        PCMETER(setmisses++);
    }

    return false;
}

JS_ALWAYS_INLINE bool
JSPropertyCache::testForInit(JSContext *cx, jsbytecode *pc, JSObject *obj,
                             JSPropCacheEntry **entryp, JSScopeProperty **spropp)
{
    uint32 shape = OBJ_SCOPE(obj)->shape;
    JSPropCacheEntry *entry = &table[PROPERTY_CACHE_HASH_PC(pc, shape)];
    PCMETER(pctestentry = entry);
    PCMETER(tests++);
    PCMETER(initests++);

    if (entry->kpc == pc &&
        entry->kshape == shape &&
        PCVCAP_SHAPE(entry->vcap) == cx->runtime->protoHazardShape)
    {
        JS_ASSERT(PCVCAP_TAG(entry->vcap) == 0);
        PCMETER(pchits++);
        PCMETER(inipchits++);
        JS_ASSERT(PCVAL_IS_SPROP(entry->vword));
        JSScopeProperty *sprop = PCVAL_TO_SPROP(entry->vword);
        JS_ASSERT(!(sprop->attrs & JSPROP_READONLY));
        *spropp = sprop;
        *entryp = entry;
        return true;
    }
    return false;
}

inline JSPropCacheEntry *
JSPropertyCache::fillEntry(jsuword khash, jsbytecode *pc, jsuword kshape,
                           jsuword vcap, jsuword vword)
{
    JSPropCacheEntry *entry = &table[khash];
    PCMETER(PCVAL_IS_NULL(entry->vword) || recycles++);
    entry->kpc = pc;
    entry->kshape = kshape;
    entry->vcap = vcap;
    entry->vword = vword;

    empty = JS_FALSE;
    PCMETER(fills++);

    /*
     * The modfills counter is not exact. It increases if a getter or setter
     * recurse into the interpreter.
     */
    PCMETER(entry == pctestentry || modfills++);
    PCMETER(pctestentry = NULL);
    return entry;
}

inline JSPropCacheEntry *
JSPropertyCache::fillByPC(jsbytecode *pc, jsuword kshape,
                          jsuword vshape, jsuword scopeIndex, jsuword protoIndex,
                          jsuword vword)
{
    return fillEntry(PROPERTY_CACHE_HASH_PC(pc, kshape), pc, kshape,
                     PCVCAP_MAKE(vshape, scopeIndex, protoIndex), vword);
}

inline JSPropCacheEntry *
JSPropertyCache::fillByAtom(JSAtom *atom, JSObject *obj,
                            jsuword vshape, jsuword scopeIndex, jsuword protoIndex,
                            jsuword vword)
{
    JS_ASSERT(obj->isDelegate());
    jsuword khash = PROPERTY_CACHE_HASH_ATOM(atom, obj);
    PCMETER(if (PCVCAP_TAG(table[khash].vcap) <= 1)
                pcrecycles++);
    return fillEntry(khash,
                     reinterpret_cast<jsbytecode *>(atom),
                     reinterpret_cast<jsuword>(obj),
                     PCVCAP_MAKE(vshape, scopeIndex, protoIndex),
                     vword);
}

#endif /* jspropcacheinlines_h___ */

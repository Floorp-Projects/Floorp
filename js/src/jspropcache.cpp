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

#include <string.h>
#include "jsapi.h"
#include "jscntxt.h"
#include "jsinterp.h"
#include "jsscope.h"
#include "jspropcacheinlines.h"

JS_REQUIRES_STACK JSPropCacheEntry *
JSPropertyCache::fill(JSContext *cx, JSObject *obj, uintN scopeIndex, uintN protoIndex,
                      JSObject *pobj, JSScopeProperty *sprop, JSBool adding)
{
    jsbytecode *pc;
    JSScope *scope;
    jsuword kshape, vshape;
    JSOp op;
    const JSCodeSpec *cs;
    jsuword vword;
    ptrdiff_t pcoff;
    JSAtom *atom;

    JS_ASSERT(!cx->runtime->gcRunning);
    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));

    /* FIXME bug 489098: consider enabling the property cache for eval. */
    if (js_IsPropertyCacheDisabled(cx) || (cx->fp->flags & JSFRAME_EVAL)) {
        PCMETER(disfills++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Check for fill from js_SetPropertyHelper where the setter removed sprop
     * from pobj's scope (via unwatch or delete, e.g.).
     */
    scope = OBJ_SCOPE(pobj);
    if (!scope->has(sprop)) {
        PCMETER(oddfills++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Check for overdeep scope and prototype chain. Because resolve, getter,
     * and setter hooks can change the prototype chain using JS_SetPrototype
     * after js_LookupPropertyWithFlags has returned the nominal protoIndex,
     * we have to validate protoIndex if it is non-zero. If it is zero, then
     * we know thanks to the scope->has test above, combined with the fact that
     * obj == pobj, that protoIndex is invariant.
     *
     * The scopeIndex can't be wrong. We require JS_SetParent calls to happen
     * before any running script might consult a parent-linked scope chain. If
     * this requirement is not satisfied, the fill in progress will never hit,
     * but vcap vs. scope shape tests ensure nothing malfunctions.
     */
    JS_ASSERT_IF(scopeIndex == 0 && protoIndex == 0, obj == pobj);

    if (protoIndex != 0) {
        JSObject *tmp = obj;

        for (uintN i = 0; i != scopeIndex; i++)
            tmp = OBJ_GET_PARENT(cx, tmp);
        JS_ASSERT(tmp != pobj);

        protoIndex = 1;
        for (;;) {
            tmp = OBJ_GET_PROTO(cx, tmp);

            /*
             * We cannot cache properties coming from native objects behind
             * non-native ones on the prototype chain. The non-natives can
             * mutate in arbitrary way without changing any shapes.
             */
            if (!tmp || !OBJ_IS_NATIVE(tmp)) {
                PCMETER(noprotos++);
                return JS_NO_PROP_CACHE_FILL;
            }
            if (tmp == pobj)
                break;
            ++protoIndex;
        }
    }

    if (scopeIndex > PCVCAP_SCOPEMASK || protoIndex > PCVCAP_PROTOMASK) {
        PCMETER(longchains++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Optimize the cached vword based on our parameters and the current pc's
     * opcode format flags.
     */
    pc = cx->fp->regs->pc;
    op = js_GetOpcode(cx, cx->fp->script, pc);
    cs = &js_CodeSpec[op];
    kshape = 0;

    do {
        /*
         * Check for a prototype "plain old method" callee computation. What
         * is a plain old method? It's a function-valued property with stub
         * getter, so get of a function is idempotent.
         */
        if (cs->format & JOF_CALLOP) {
            jsval v;

            if (sprop->isMethod()) {
                /*
                 * A compiler-created function object, AKA a method, already
                 * memoized in the property tree.
                 */
                JS_ASSERT(scope->hasMethodBarrier());
                v = sprop->methodValue();
                JS_ASSERT(VALUE_IS_FUNCTION(cx, v));
                JS_ASSERT(v == LOCKED_OBJ_GET_SLOT(pobj, sprop->slot));
                vword = JSVAL_OBJECT_TO_PCVAL(v);
                break;
            }

            if (SPROP_HAS_STUB_GETTER(sprop) &&
                SPROP_HAS_VALID_SLOT(sprop, scope)) {
                v = LOCKED_OBJ_GET_SLOT(pobj, sprop->slot);
                if (VALUE_IS_FUNCTION(cx, v)) {
                    /*
                     * Great, we have a function-valued prototype property
                     * where the getter is JS_PropertyStub. The type id in
                     * pobj's scope does not evolve with changes to property
                     * values, however.
                     *
                     * So here, on first cache fill for this method, we brand
                     * the scope with a new shape and set the JSScope::BRANDED
                     * flag. Once this flag is set, any property assignment
                     * that changes the value from or to a different function
                     * object will result in shape being regenerated.
                     */
                    if (!scope->branded()) {
                        PCMETER(brandfills++);
#ifdef DEBUG_notme
                        fprintf(stderr,
                                "branding %p (%s) for funobj %p (%s), shape %lu\n",
                                pobj, pobj->getClass()->name,
                                JSVAL_TO_OBJECT(v),
                                JS_GetFunctionName(GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(v))),
                                OBJ_SHAPE(obj));
#endif
                        scope->brandingShapeChange(cx, sprop->slot, v);
                        if (js_IsPropertyCacheDisabled(cx))  /* check for rt->shapeGen overflow */
                            return JS_NO_PROP_CACHE_FILL;
                        scope->setBranded();
                    }
                    vword = JSVAL_OBJECT_TO_PCVAL(v);
                    break;
                }
            }
        }

        /* If getting a value via a stub getter, we can cache the slot. */
        if (!(cs->format & (JOF_SET | JOF_INCDEC | JOF_FOR)) &&
            SPROP_HAS_STUB_GETTER(sprop) &&
            SPROP_HAS_VALID_SLOT(sprop, scope)) {
            /* Great, let's cache sprop's slot and use it on cache hit. */
            vword = SLOT_TO_PCVAL(sprop->slot);
        } else {
            /* Best we can do is to cache sprop (still a nice speedup). */
            vword = SPROP_TO_PCVAL(sprop);
            if (adding &&
                sprop == scope->lastProp &&
                scope->shape == sprop->shape) {
                /*
                 * Our caller added a new property. We also know that a setter
                 * that js_NativeSet could have run has not mutated the scope,
                 * so the added property is still the last one added, and the
                 * scope is not branded.
                 *
                 * We want to cache under scope's shape before the property
                 * addition to bias for the case when the mutator opcode
                 * always adds the same property. This allows us to optimize
                 * periodic execution of object initializers or other explicit
                 * initialization sequences such as
                 *
                 *   obj = {}; obj.x = 1; obj.y = 2;
                 *
                 * We assume that on average the win from this optimization is
                 * greater than the cost of an extra mismatch per loop owing to
                 * the bias for the following case:
                 *
                 *   obj = {}; ... for (...) { ... obj.x = ... }
                 *
                 * On the first iteration of such a for loop, JSOP_SETPROP
                 * fills the cache with the shape of the newly created object
                 * obj, not the shape of obj after obj.x has been assigned.
                 * That mismatches obj's shape on the second iteration. Note
                 * that on the third and subsequent iterations the cache will
                 * be hit because the shape is no longer updated.
                 */
                JS_ASSERT(scope->owned());
                if (sprop->parent) {
                    kshape = sprop->parent->shape;
                } else {
                    /*
                     * If obj had its own empty scope before, with a unique
                     * shape, that is lost. Here we only attempt to find a
                     * matching empty scope. In unusual cases involving
                     * __proto__ assignment we may not find one.
                     */
                    JSObject *proto = STOBJ_GET_PROTO(obj);
                    if (!proto || !OBJ_IS_NATIVE(proto))
                        return JS_NO_PROP_CACHE_FILL;
                    JSScope *protoscope = OBJ_SCOPE(proto);
                    if (!protoscope->emptyScope ||
                        !js_ObjectIsSimilarToProto(cx, obj, obj->map->ops, OBJ_GET_CLASS(cx, obj),
                                                   proto)) {
                        return JS_NO_PROP_CACHE_FILL;
                    }
                    kshape = protoscope->emptyScope->shape;
                }

                /*
                 * When adding we predict no prototype object will later gain a
                 * readonly property or setter.
                 */
                vshape = cx->runtime->protoHazardShape;
            }
        }
    } while (0);

    if (kshape == 0) {
        kshape = OBJ_SHAPE(obj);
        vshape = scope->shape;
    }

    if (obj == pobj) {
        JS_ASSERT(scopeIndex == 0 && protoIndex == 0);
    } else {
        if (op == JSOP_LENGTH) {
            atom = cx->runtime->atomState.lengthAtom;
        } else {
            pcoff = (JOF_TYPE(cs->format) == JOF_SLOTATOM) ? SLOTNO_LEN : 0;
            GET_ATOM_FROM_BYTECODE(cx->fp->script, pc, pcoff, atom);
        }

#ifdef DEBUG
        if (scopeIndex == 0) {
            JS_ASSERT(protoIndex != 0);
            JS_ASSERT((protoIndex == 1) == (OBJ_GET_PROTO(cx, obj) == pobj));
        }
#endif

        if (scopeIndex != 0 || protoIndex != 1) {
            /*
             * Make sure that a later shadowing assignment will enter
             * PurgeProtoChain and invalidate this entry, bug 479198.
             *
             * This is thread-safe even though obj is not locked. Only the
             * DELEGATE bit of obj->classword can change at runtime, given that
             * obj is native; and the bit is only set, never cleared. And on
             * platforms where another CPU can fail to see this write, it's OK
             * because the property cache and JIT cache are thread-local.
             */
            obj->setDelegate();

            return fillByAtom(atom, obj, vshape, scopeIndex, protoIndex, vword);
        }
    }

    return fillByPC(pc, kshape, vshape, scopeIndex, protoIndex, vword);
}

JS_REQUIRES_STACK JSAtom *
JSPropertyCache::fullTest(JSContext *cx, jsbytecode *pc, JSObject **objp, JSObject **pobjp,
                         JSPropCacheEntry **entryp)
{
    JSOp op;
    const JSCodeSpec *cs;
    ptrdiff_t pcoff;
    JSAtom *atom;
    JSObject *obj, *pobj, *tmp;
    JSPropCacheEntry *entry;
    uint32 vcap;

    JS_ASSERT(uintN((cx->fp->imacpc ? cx->fp->imacpc : pc) - cx->fp->script->code)
              < cx->fp->script->length);

    op = js_GetOpcode(cx, cx->fp->script, pc);
    cs = &js_CodeSpec[op];
    if (op == JSOP_LENGTH) {
        atom = cx->runtime->atomState.lengthAtom;
    } else {
        pcoff = (JOF_TYPE(cs->format) == JOF_SLOTATOM) ? SLOTNO_LEN : 0;
        GET_ATOM_FROM_BYTECODE(cx->fp->script, pc, pcoff, atom);
    }

    obj = *objp;
    JS_ASSERT(OBJ_IS_NATIVE(obj));
    entry = &table[PROPERTY_CACHE_HASH_ATOM(atom, obj)];
    *entryp = entry;
    vcap = entry->vcap;

    if (entry->kpc != (jsbytecode *) atom) {
        PCMETER(idmisses++);

#ifdef DEBUG_notme
        entry = &table[PROPERTY_CACHE_HASH_PC(pc, OBJ_SHAPE(obj))];
        fprintf(stderr,
                "id miss for %s from %s:%u"
                " (pc %u, kpc %u, kshape %u, shape %u)\n",
                js_AtomToPrintableString(cx, atom),
                cx->fp->script->filename,
                js_PCToLineNumber(cx, cx->fp->script, pc),
                pc - cx->fp->script->code,
                entry->kpc - cx->fp->script->code,
                entry->kshape,
                OBJ_SHAPE(obj));
                js_Disassemble1(cx, cx->fp->script, pc,
                                pc - cx->fp->script->code,
                                JS_FALSE, stderr);
#endif

        return atom;
    }

    if (entry->kshape != (jsuword) obj) {
        PCMETER(komisses++);
        return atom;
    }

    pobj = obj;

    if (JOF_MODE(cs->format) == JOF_NAME) {
        while (vcap & (PCVCAP_SCOPEMASK << PCVCAP_PROTOBITS)) {
            tmp = OBJ_GET_PARENT(cx, pobj);
            if (!tmp || !OBJ_IS_NATIVE(tmp))
                break;
            pobj = tmp;
            vcap -= PCVCAP_PROTOSIZE;
        }

        *objp = pobj;
    }

    while (vcap & PCVCAP_PROTOMASK) {
        tmp = OBJ_GET_PROTO(cx, pobj);
        if (!tmp || !OBJ_IS_NATIVE(tmp))
            break;
        pobj = tmp;
        --vcap;
    }

    if (JS_LOCK_OBJ_IF_SHAPE(cx, pobj, PCVCAP_SHAPE(vcap))) {
#ifdef DEBUG
        jsid id = ATOM_TO_JSID(atom);

        id = js_CheckForStringIndex(id);
        JS_ASSERT(OBJ_SCOPE(pobj)->lookup(id));
        JS_ASSERT_IF(OBJ_SCOPE(pobj)->object, OBJ_SCOPE(pobj)->object == pobj);
#endif
        *pobjp = pobj;
        return NULL;
    }

    PCMETER(vcmisses++);
    return atom;
}

inline void
JSPropertyCache::assertEmpty()
{
#ifdef DEBUG
    JS_ASSERT(empty);
    for (uintN i = 0; i < PROPERTY_CACHE_SIZE; i++) {
        JS_ASSERT(!table[i].kpc);
        JS_ASSERT(!table[i].kshape);
        JS_ASSERT(!table[i].vcap);
        JS_ASSERT(!table[i].vword);
    }
#endif
}


JS_STATIC_ASSERT(PCVAL_NULL == 0);

void
JSPropertyCache::purge(JSContext *cx)
{
    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));

    if (empty) {
        assertEmpty();
        return;
    }

    memset(table, 0, sizeof table);
    empty = JS_TRUE;

#ifdef JS_PROPERTY_CACHE_METERING
    {
        static FILE *fp;
        if (!fp)
            fp = fopen("/tmp/propcache.stats", "w");
        if (fp) {
            fputs("Property cache stats for ", fp);
# ifdef JS_THREADSAFE
            fprintf(fp, "thread %lu, ", (unsigned long) cx->thread->id);
# endif
            fprintf(fp, "GC %u\n", cx->runtime->gcNumber);

# define P(mem) fprintf(fp, "%11s %10lu\n", #mem, (unsigned long)mem)
            P(fills);
            P(nofills);
            P(rofills);
            P(disfills);
            P(oddfills);
            P(modfills);
            P(brandfills);
            P(noprotos);
            P(longchains);
            P(recycles);
            P(pcrecycles);
            P(tests);
            P(pchits);
            P(protopchits);
            P(initests);
            P(inipchits);
            P(inipcmisses);
            P(settests);
            P(addpchits);
            P(setpchits);
            P(setpcmisses);
            P(slotchanges);
            P(setmisses);
            P(idmisses);
            P(komisses);
            P(vcmisses);
            P(misses);
            P(flushes);
            P(pcpurges);
# undef P

            fprintf(fp, "hit rates: pc %g%% (proto %g%%), set %g%%, ini %g%%, full %g%%\n",
                    (100. * pchits) / tests,
                    (100. * protopchits) / tests,
                    (100. * (addpchits + setpchits)) / settests,
                    (100. * inipchits) / initests,
                    (100. * (tests - misses)) / tests);
            fflush(fp);
        }
    }
#endif

    PCMETER(flushes++);
}

void
JSPropertyCache::purgeForScript(JSContext *cx, JSScript *script)
{
    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));

    for (JSPropCacheEntry *entry = table; entry < table + PROPERTY_CACHE_SIZE; entry++) {
        if (JS_UPTRDIFF(entry->kpc, script->code) < script->length) {
            entry->kpc = NULL;
            entry->kshape = 0;
#ifdef DEBUG
            entry->vcap = entry->vword = 0;
#endif
        }
    }
}

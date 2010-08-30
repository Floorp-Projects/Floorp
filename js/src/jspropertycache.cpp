/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=98:
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "jspropertycache.h"
#include "jscntxt.h"
#include "jsnum.h"
#include "jsobjinlines.h"
#include "jspropertycacheinlines.h"

using namespace js;

JS_STATIC_ASSERT(sizeof(PCVal) == sizeof(jsuword));

JS_REQUIRES_STACK PropertyCacheEntry *
PropertyCache::fill(JSContext *cx, JSObject *obj, uintN scopeIndex, uintN protoIndex,
                    JSObject *pobj, const Shape *shape, JSBool adding)
{
    jsbytecode *pc;
    jsuword kshape, vshape;
    JSOp op;
    const JSCodeSpec *cs;
    PCVal vword;
    PropertyCacheEntry *entry;

    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));
    JS_ASSERT(!cx->runtime->gcRunning);

    if (js_IsPropertyCacheDisabled(cx)) {
        PCMETER(disfills++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Check for fill from js_SetPropertyHelper where the setter removed shape
     * from pobj (via unwatch or delete, e.g.).
     */
    if (!pobj->nativeContains(*shape)) {
        PCMETER(oddfills++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Check for overdeep scope and prototype chain. Because resolve, getter,
     * and setter hooks can change the prototype chain using JS_SetPrototype
     * after js_LookupPropertyWithFlags has returned the nominal protoIndex,
     * we have to validate protoIndex if it is non-zero. If it is zero, then
     * we know thanks to the pobj->nativeContains test above, combined with the
     * fact that obj == pobj, that protoIndex is invariant.
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
            tmp = tmp->getParent();
        JS_ASSERT(tmp != pobj);

        protoIndex = 1;
        for (;;) {
            tmp = tmp->getProto();

            /*
             * We cannot cache properties coming from native objects behind
             * non-native ones on the prototype chain. The non-natives can
             * mutate in arbitrary way without changing any shapes.
             */
            if (!tmp || !tmp->isNative()) {
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
    pc = cx->regs->pc;
    op = js_GetOpcode(cx, cx->fp()->getScript(), pc);
    cs = &js_CodeSpec[op];
    kshape = 0;

    do {
        /*
         * Check for a prototype "plain old method" callee computation. What
         * is a plain old method? It's a function-valued property with stub
         * getter, so get of a function is idempotent.
         */
        if (cs->format & JOF_CALLOP) {
            if (shape->isMethod()) {
                /*
                 * A compiler-created function object, AKA a method, already
                 * memoized in the property tree.
                 */
                JS_ASSERT(pobj->hasMethodBarrier());
                JSObject &funobj = shape->methodObject();
                JS_ASSERT(&funobj == &pobj->lockedGetSlot(shape->slot).toObject());
                vword.setFunObj(funobj);
                break;
            }

            if (!pobj->generic() &&
                shape->hasDefaultGetter() &&
                pobj->containsSlot(shape->slot)) {
                const Value &v = pobj->lockedGetSlot(shape->slot);
                JSObject *funobj;

                if (IsFunctionObject(v, &funobj)) {
                    /*
                     * Great, we have a function-valued prototype property
                     * where the getter is JS_PropertyStub. The type id in
                     * pobj does not evolve with changes to property values,
                     * however.
                     *
                     * So here, on first cache fill for this method, we brand
                     * obj with a new shape and set the JSObject::BRANDED flag.
                     * Once this flag is set, any property assignment that
                     * changes the value from or to a different function object
                     * will result in shape being regenerated.
                     */
                    if (!pobj->branded()) {
                        PCMETER(brandfills++);
#ifdef DEBUG_notme
                        fprintf(stderr,
                                "branding %p (%s) for funobj %p (%s), shape %lu\n",
                                pobj, pobj->getClass()->name,
                                JSVAL_TO_OBJECT(v),
                                JS_GetFunctionName(GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(v))),
                                obj->shape());
#endif
                        if (!pobj->brand(cx, shape->slot, v))
                            return JS_NO_PROP_CACHE_FILL;
                    }
                    vword.setFunObj(*funobj);
                    break;
                }
            }
        }

        /*
         * If getting a value via a stub getter, or doing an INCDEC op
         * with stub getters and setters, we can cache the slot.
         */
        if (!(cs->format & (JOF_SET | JOF_FOR)) &&
            (!(cs->format & JOF_INCDEC) || shape->hasDefaultSetter()) &&
            shape->hasDefaultGetter() &&
            pobj->containsSlot(shape->slot)) {
            /* Great, let's cache shape's slot and use it on cache hit. */
            vword.setSlot(shape->slot);
        } else {
            /* Best we can do is to cache shape (still a nice speedup). */
            vword.setShape(shape);
            if (adding &&
                pobj->shape() == shape->shape) {
                /*
                 * Our caller added a new property. We also know that a setter
                 * that js_NativeSet might have run has not mutated pobj, so
                 * the added property is still the last one added, and pobj is
                 * not branded.
                 *
                 * We want to cache under pobj's shape before the property
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
                JS_ASSERT(shape == pobj->lastProperty());
                JS_ASSERT(!pobj->nativeEmpty());

                kshape = shape->previous()->shape;

                /*
                 * When adding we predict no prototype object will later gain a
                 * readonly property or setter.
                 */
                vshape = cx->runtime->protoHazardShape;
            }
        }
    } while (0);

    if (kshape == 0) {
        kshape = obj->shape();
        vshape = pobj->shape();
    }
    JS_ASSERT(kshape < SHAPE_OVERFLOW_BIT);

    if (obj == pobj) {
        JS_ASSERT(scopeIndex == 0 && protoIndex == 0);
    } else {
#ifdef DEBUG
        if (scopeIndex == 0) {
            JS_ASSERT(protoIndex != 0);
            JS_ASSERT((protoIndex == 1) == (obj->getProto() == pobj));
        }
#endif

        if (scopeIndex != 0 || protoIndex != 1) {
            /*
             * Make sure that a later shadowing assignment will enter
             * PurgeProtoChain and invalidate this entry, bug 479198.
             *
             * This is not thread-safe but we are about to make all objects
             * except multi-threaded wrappers (bug 566951) single-threaded.
             * And multi-threaded wrappers are non-native Proxy instances, so
             * they won't use the property cache.
             */
            obj->setDelegate();
        }
    }
    JS_ASSERT(vshape < SHAPE_OVERFLOW_BIT);

    entry = &table[hash(pc, kshape)];
    PCMETER(entry->vword.isNull() || recycles++);
    entry->assign(pc, kshape, vshape, scopeIndex, protoIndex, vword);

    empty = false;
    PCMETER(fills++);

    /*
     * The modfills counter is not exact. It increases if a getter or setter
     * recurse into the interpreter.
     */
    PCMETER(entry == pctestentry || modfills++);
    PCMETER(pctestentry = NULL);
    return entry;
}

static inline JSAtom *
GetAtomFromBytecode(JSContext *cx, jsbytecode *pc, JSOp op, const JSCodeSpec &cs)
{
    if (op == JSOP_LENGTH)
        return cx->runtime->atomState.lengthAtom;

    // The method JIT's implementation of instanceof contains an internal lookup
    // of the prototype property.
    if (op == JSOP_INSTANCEOF)
        return cx->runtime->atomState.classPrototypeAtom;

    ptrdiff_t pcoff = (JOF_TYPE(cs.format) == JOF_SLOTATOM) ? SLOTNO_LEN : 0;
    JSAtom *atom;
    GET_ATOM_FROM_BYTECODE(cx->fp()->getScript(), pc, pcoff, atom);
    return atom;
}

JS_REQUIRES_STACK JSAtom *
PropertyCache::fullTest(JSContext *cx, jsbytecode *pc, JSObject **objp, JSObject **pobjp,
                        PropertyCacheEntry *entry)
{
    JSObject *obj, *pobj, *tmp;
    uint32 vcap;

    JSStackFrame *fp = cx->fp();

    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));
    JS_ASSERT(uintN((fp->hasIMacroPC() ? fp->getIMacroPC() : pc) - fp->getScript()->code)
              < fp->getScript()->length);

    JSOp op = js_GetOpcode(cx, fp->getScript(), pc);
    const JSCodeSpec &cs = js_CodeSpec[op];

    obj = *objp;
    vcap = entry->vcap;

    if (entry->kpc != pc) {
        PCMETER(kpcmisses++);

        JSAtom *atom = GetAtomFromBytecode(cx, pc, op, cs);
#ifdef DEBUG_notme
        JSScript *script = cx->fp()->getScript();
        fprintf(stderr,
                "id miss for %s from %s:%u"
                " (pc %u, kpc %u, kshape %u, shape %u)\n",
                js_AtomToPrintableString(cx, atom),
                script->filename,
                js_PCToLineNumber(cx, script, pc),
                pc - script->code,
                entry->kpc - script->code,
                entry->kshape,
                obj->shape());
                js_Disassemble1(cx, script, pc,
                                pc - script->code,
                                JS_FALSE, stderr);
#endif

        return atom;
    }

    if (entry->kshape != obj->shape()) {
        PCMETER(kshapemisses++);
        return GetAtomFromBytecode(cx, pc, op, cs);
    }

    /*
     * PropertyCache::test handles only the direct and immediate-prototype hit
     * cases. All others go here. We could embed the target object in the cache
     * entry but then entry size would be 5 words. Instead we traverse chains.
     */
    pobj = obj;

    if (JOF_MODE(cs.format) == JOF_NAME) {
        while (vcap & (PCVCAP_SCOPEMASK << PCVCAP_PROTOBITS)) {
            tmp = pobj->getParent();
            if (!tmp || !tmp->isNative())
                break;
            pobj = tmp;
            vcap -= PCVCAP_PROTOSIZE;
        }

        *objp = pobj;
    }

    while (vcap & PCVCAP_PROTOMASK) {
        tmp = pobj->getProto();
        if (!tmp || !tmp->isNative())
            break;
        pobj = tmp;
        --vcap;
    }

    if (matchShape(cx, pobj, vcap >> PCVCAP_TAGBITS)) {
#ifdef DEBUG
        JSAtom *atom = GetAtomFromBytecode(cx, pc, op, cs);
        jsid id = ATOM_TO_JSID(atom);

        id = js_CheckForStringIndex(id);
        JS_ASSERT(pobj->nativeContains(id));
#endif
        *pobjp = pobj;
        return NULL;
    }

    PCMETER(vcapmisses++);
    return GetAtomFromBytecode(cx, pc, op, cs);
}

#ifdef DEBUG
void
PropertyCache::assertEmpty()
{
    JS_ASSERT(empty);
    for (uintN i = 0; i < SIZE; i++) {
        JS_ASSERT(!table[i].kpc);
        JS_ASSERT(!table[i].kshape);
        JS_ASSERT(!table[i].vcap);
        JS_ASSERT(table[i].vword.isNull());
    }
}
#endif

void
PropertyCache::purge(JSContext *cx)
{
    if (empty) {
        assertEmpty();
        return;
    }

    PodArrayZero(table);
    JS_ASSERT(table[0].vword.isNull());
    empty = true;

#ifdef JS_PROPERTY_CACHE_METERING
  { static FILE *fp;
    if (!fp)
        fp = fopen("/tmp/propcache.stats", "w");
    if (fp) {
        fputs("Property cache stats for ", fp);
#ifdef JS_THREADSAFE
        fprintf(fp, "thread %lu, ", (unsigned long) cx->thread->id);
#endif
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
        P(setmisses);
        P(kpcmisses);
        P(kshapemisses);
        P(vcapmisses);
        P(misses);
        P(flushes);
        P(pcpurges);
# undef P

        fprintf(fp, "hit rates: pc %g%% (proto %g%%), set %g%%, ini %g%%, full %g%%\n",
                (100. * pchits) / tests,
                (100. * protopchits) / tests,
                (100. * (addpchits + setpchits))
                / settests,
                (100. * inipchits) / initests,
                (100. * (tests - misses)) / tests);
        fflush(fp);
    }
  }
#endif

    PCMETER(flushes++);
}

void
PropertyCache::purgeForScript(JSScript *script)
{
    for (PropertyCacheEntry *entry = table; entry < table + SIZE; entry++) {
        if (JS_UPTRDIFF(entry->kpc, script->code) < script->length) {
            entry->kpc = NULL;
#ifdef DEBUG
            entry->kshape = entry->vcap = 0;
            entry->vword.setNull();
#endif
        }
    }
}

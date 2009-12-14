/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
 * JavaScript bytecode interpreter.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsstaticcheck.h"
#include "jstracer.h"
#include "jslibmath.h"
#include "jsvector.h"

#include "jsatominlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstrinlines.h"

#ifdef INCLUDE_MOZILLA_DTRACE
#include "jsdtracef.h"
#endif

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsautooplen.h"

/* jsinvoke_cpp___ indicates inclusion from jsinvoke.cpp. */
#if !JS_LONE_INTERPRET ^ defined jsinvoke_cpp___

JS_REQUIRES_STACK JSPropCacheEntry *
js_FillPropertyCache(JSContext *cx, JSObject *obj,
                     uintN scopeIndex, uintN protoIndex, JSObject *pobj,
                     JSScopeProperty *sprop, JSBool adding)
{
    JSPropertyCache *cache;
    jsbytecode *pc;
    JSScope *scope;
    jsuword kshape, vshape, khash;
    JSOp op;
    const JSCodeSpec *cs;
    jsuword vword;
    ptrdiff_t pcoff;
    JSAtom *atom;
    JSPropCacheEntry *entry;

    JS_ASSERT(!cx->runtime->gcRunning);
    cache = &JS_PROPERTY_CACHE(cx);

    /* FIXME bug 489098: consider enabling the property cache for eval. */
    if (js_IsPropertyCacheDisabled(cx) || (cx->fp->flags & JSFRAME_EVAL)) {
        PCMETER(cache->disfills++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Check for fill from js_SetPropertyHelper where the setter removed sprop
     * from pobj's scope (via unwatch or delete, e.g.).
     */
    scope = OBJ_SCOPE(pobj);
    if (!scope->hasProperty(sprop)) {
        PCMETER(cache->oddfills++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Check for overdeep scope and prototype chain. Because resolve, getter,
     * and setter hooks can change the prototype chain using JS_SetPrototype
     * after js_LookupPropertyWithFlags has returned the nominal protoIndex,
     * we have to validate protoIndex if it is non-zero. If it is zero, then
     * we know thanks to the scope->hasProperty test above, combined with the
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
                PCMETER(cache->noprotos++);
                return JS_NO_PROP_CACHE_FILL;
            }
            if (tmp == pobj)
                break;
            ++protoIndex;
        }
    }

    if (scopeIndex > PCVCAP_SCOPEMASK || protoIndex > PCVCAP_PROTOMASK) {
        PCMETER(cache->longchains++);
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
                        PCMETER(cache->brandfills++);
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
                sprop == scope->lastProperty() &&
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
                        protoscope->emptyScope->clasp != obj->getClass()) {
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
    JS_ASSERT(kshape < SHAPE_OVERFLOW_BIT);

    khash = PROPERTY_CACHE_HASH_PC(pc, kshape);
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
            khash = PROPERTY_CACHE_HASH_ATOM(atom, obj);
            PCMETER(if (PCVCAP_TAG(cache->table[khash].vcap) <= 1)
                        cache->pcrecycles++);
            pc = (jsbytecode *) atom;
            kshape = (jsuword) obj;

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
        }
    }
    JS_ASSERT(vshape < SHAPE_OVERFLOW_BIT);

    entry = &cache->table[khash];
    PCMETER(PCVAL_IS_NULL(entry->vword) || cache->recycles++);
    entry->kpc = pc;
    entry->kshape = kshape;
    entry->vcap = PCVCAP_MAKE(vshape, scopeIndex, protoIndex);
    entry->vword = vword;

    cache->empty = JS_FALSE;
    PCMETER(cache->fills++);

    /*
     * The modfills counter is not exact. It increases if a getter or setter
     * recurse into the interpreter.
     */
    PCMETER(entry == cache->pctestentry || cache->modfills++);
    PCMETER(cache->pctestentry = NULL);
    return entry;
}

JS_REQUIRES_STACK JSAtom *
js_FullTestPropertyCache(JSContext *cx, jsbytecode *pc,
                         JSObject **objp, JSObject **pobjp,
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
    entry = &JS_PROPERTY_CACHE(cx).table[PROPERTY_CACHE_HASH_ATOM(atom, obj)];
    *entryp = entry;
    vcap = entry->vcap;

    if (entry->kpc != (jsbytecode *) atom) {
        PCMETER(JS_PROPERTY_CACHE(cx).idmisses++);

#ifdef DEBUG_notme
        entry = &JS_PROPERTY_CACHE(cx).table[PROPERTY_CACHE_HASH_PC(pc, OBJ_SHAPE(obj))];
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
        PCMETER(JS_PROPERTY_CACHE(cx).komisses++);
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

    PCMETER(JS_PROPERTY_CACHE(cx).vcmisses++);
    return atom;
}

#ifdef DEBUG
#define ASSERT_CACHE_IS_EMPTY(cache)                                          \
    JS_BEGIN_MACRO                                                            \
        JSPropertyCache *cache_ = (cache);                                    \
        uintN i_;                                                             \
        JS_ASSERT(cache_->empty);                                             \
        for (i_ = 0; i_ < PROPERTY_CACHE_SIZE; i_++) {                        \
            JS_ASSERT(!cache_->table[i_].kpc);                                \
            JS_ASSERT(!cache_->table[i_].kshape);                             \
            JS_ASSERT(!cache_->table[i_].vcap);                               \
            JS_ASSERT(!cache_->table[i_].vword);                              \
        }                                                                     \
    JS_END_MACRO
#else
#define ASSERT_CACHE_IS_EMPTY(cache) ((void)0)
#endif

JS_STATIC_ASSERT(PCVAL_NULL == 0);

void
js_PurgePropertyCache(JSContext *cx, JSPropertyCache *cache)
{
    if (cache->empty) {
        ASSERT_CACHE_IS_EMPTY(cache);
        return;
    }

    memset(cache->table, 0, sizeof cache->table);
    cache->empty = JS_TRUE;

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

# define P(mem) fprintf(fp, "%11s %10lu\n", #mem, (unsigned long)cache->mem)
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
                (100. * cache->pchits) / cache->tests,
                (100. * cache->protopchits) / cache->tests,
                (100. * (cache->addpchits + cache->setpchits))
                / cache->settests,
                (100. * cache->inipchits) / cache->initests,
                (100. * (cache->tests - cache->misses)) / cache->tests);
        fflush(fp);
    }
  }
#endif

    PCMETER(cache->flushes++);
}

void
js_PurgePropertyCacheForScript(JSContext *cx, JSScript *script)
{
    JSPropertyCache *cache;
    JSPropCacheEntry *entry;

    cache = &JS_PROPERTY_CACHE(cx);
    for (entry = cache->table; entry < cache->table + PROPERTY_CACHE_SIZE;
         entry++) {
        if (JS_UPTRDIFF(entry->kpc, script->code) < script->length) {
            entry->kpc = NULL;
            entry->kshape = 0;
#ifdef DEBUG
            entry->vcap = entry->vword = 0;
#endif
        }
    }
}

/*
 * Check if the current arena has enough space to fit nslots after sp and, if
 * so, reserve the necessary space.
 */
static JS_REQUIRES_STACK JSBool
AllocateAfterSP(JSContext *cx, jsval *sp, uintN nslots)
{
    uintN surplus;
    jsval *sp2;

    JS_ASSERT((jsval *) cx->stackPool.current->base <= sp);
    JS_ASSERT(sp <= (jsval *) cx->stackPool.current->avail);
    surplus = (jsval *) cx->stackPool.current->avail - sp;
    if (nslots <= surplus)
        return JS_TRUE;

    /*
     * No room before current->avail, check if the arena has enough space to
     * fit the missing slots before the limit.
     */
    if (nslots > (size_t) ((jsval *) cx->stackPool.current->limit - sp))
        return JS_FALSE;

    JS_ARENA_ALLOCATE_CAST(sp2, jsval *, &cx->stackPool,
                           (nslots - surplus) * sizeof(jsval));
    JS_ASSERT(sp2 == sp + surplus);
    return JS_TRUE;
}

JS_STATIC_INTERPRET JS_REQUIRES_STACK jsval *
js_AllocRawStack(JSContext *cx, uintN nslots, void **markp)
{
    jsval *sp;

    JS_ASSERT(nslots != 0);
    JS_ASSERT_NOT_ON_TRACE(cx);

    if (!cx->stackPool.first.next) {
        int64 *timestamp;

        JS_ARENA_ALLOCATE_CAST(timestamp, int64 *,
                               &cx->stackPool, sizeof *timestamp);
        if (!timestamp) {
            js_ReportOutOfScriptQuota(cx);
            return NULL;
        }
        *timestamp = JS_Now();
    }

    if (markp)
        *markp = JS_ARENA_MARK(&cx->stackPool);
    JS_ARENA_ALLOCATE_CAST(sp, jsval *, &cx->stackPool, nslots * sizeof(jsval));
    if (!sp)
        js_ReportOutOfScriptQuota(cx);
    return sp;
}

JS_STATIC_INTERPRET JS_REQUIRES_STACK void
js_FreeRawStack(JSContext *cx, void *mark)
{
    JS_ARENA_RELEASE(&cx->stackPool, mark);
}

JS_REQUIRES_STACK JS_FRIEND_API(jsval *)
js_AllocStack(JSContext *cx, uintN nslots, void **markp)
{
    jsval *sp;
    JSArena *a;
    JSStackHeader *sh;

    /* Callers don't check for zero nslots: we do to avoid empty segments. */
    if (nslots == 0) {
        *markp = NULL;
        return (jsval *) JS_ARENA_MARK(&cx->stackPool);
    }

    /* Allocate 2 extra slots for the stack segment header we'll likely need. */
    sp = js_AllocRawStack(cx, 2 + nslots, markp);
    if (!sp)
        return NULL;

    /* Try to avoid another header if we can piggyback on the last segment. */
    a = cx->stackPool.current;
    sh = cx->stackHeaders;
    if (sh && JS_STACK_SEGMENT(sh) + sh->nslots == sp) {
        /* Extend the last stack segment, give back the 2 header slots. */
        sh->nslots += nslots;
        a->avail -= 2 * sizeof(jsval);
    } else {
        /*
         * Need a new stack segment, so allocate and push a stack segment
         * header from the 2 extra slots.
         */
        sh = (JSStackHeader *)sp;
        sh->nslots = nslots;
        sh->down = cx->stackHeaders;
        cx->stackHeaders = sh;
        sp += 2;
    }

    /*
     * Store JSVAL_NULL using memset, to let compilers optimize as they see
     * fit, in case a caller allocates and pushes GC-things one by one, which
     * could nest a last-ditch GC that will scan this segment.
     */
    memset(sp, 0, nslots * sizeof(jsval));
    return sp;
}

JS_REQUIRES_STACK JS_FRIEND_API(void)
js_FreeStack(JSContext *cx, void *mark)
{
    JSStackHeader *sh;
    jsuword slotdiff;

    /* Check for zero nslots allocation special case. */
    if (!mark)
        return;

    /* We can assert because js_FreeStack always balances js_AllocStack. */
    sh = cx->stackHeaders;
    JS_ASSERT(sh);

    /* If mark is in the current segment, reduce sh->nslots, else pop sh. */
    slotdiff = JS_UPTRDIFF(mark, JS_STACK_SEGMENT(sh)) / sizeof(jsval);
    if (slotdiff < (jsuword)sh->nslots)
        sh->nslots = slotdiff;
    else
        cx->stackHeaders = sh->down;

    /* Release the stackPool space allocated since mark was set. */
    JS_ARENA_RELEASE(&cx->stackPool, mark);
}

JSObject *
js_GetScopeChain(JSContext *cx, JSStackFrame *fp)
{
    JSObject *sharedBlock = fp->blockChain;

    if (!sharedBlock) {
        /*
         * Don't force a call object for a lightweight function call, but do
         * insist that there is a call object for a heavyweight function call.
         */
        JS_ASSERT(!fp->fun ||
                  !(fp->fun->flags & JSFUN_HEAVYWEIGHT) ||
                  fp->callobj);
        JS_ASSERT(fp->scopeChain);
        return fp->scopeChain;
    }

    /* We don't handle cloning blocks on trace.  */
    js_LeaveTrace(cx);

    /*
     * We have one or more lexical scopes to reflect into fp->scopeChain, so
     * make sure there's a call object at the current head of the scope chain,
     * if this frame is a call frame.
     *
     * Also, identify the innermost compiler-allocated block we needn't clone.
     */
    JSObject *limitBlock, *limitClone;
    if (fp->fun && !fp->callobj) {
        JS_ASSERT(OBJ_GET_CLASS(cx, fp->scopeChain) != &js_BlockClass ||
                  fp->scopeChain->getPrivate() != fp);
        if (!js_GetCallObject(cx, fp))
            return NULL;

        /* We know we must clone everything on blockChain. */
        limitBlock = limitClone = NULL;
    } else {
        /*
         * scopeChain includes all blocks whose static scope we're within that
         * have already been cloned.  Find the innermost such block.  Its
         * prototype should appear on blockChain; we'll clone blockChain up
         * to, but not including, that prototype.
         */
        limitClone = fp->scopeChain;
        while (OBJ_GET_CLASS(cx, limitClone) == &js_WithClass)
            limitClone = OBJ_GET_PARENT(cx, limitClone);
        JS_ASSERT(limitClone);

        /*
         * It may seem like we don't know enough about limitClone to be able
         * to just grab its prototype as we do here, but it's actually okay.
         *
         * If limitClone is a block object belonging to this frame, then its
         * prototype is the innermost entry in blockChain that we have already
         * cloned, and is thus the place to stop when we clone below.
         *
         * Otherwise, there are no blocks for this frame on scopeChain, and we
         * need to clone the whole blockChain.  In this case, limitBlock can
         * point to any object known not to be on blockChain, since we simply
         * loop until we hit limitBlock or NULL.  If limitClone is a block, it
         * isn't a block from this function, since blocks can't be nested
         * within themselves on scopeChain (recursion is dynamic nesting, not
         * static nesting).  If limitClone isn't a block, its prototype won't
         * be a block either.  So we can just grab limitClone's prototype here
         * regardless of its type or which frame it belongs to.
         */
        limitBlock = OBJ_GET_PROTO(cx, limitClone);

        /* If the innermost block has already been cloned, we are done. */
        if (limitBlock == sharedBlock)
            return fp->scopeChain;
    }

    /*
     * Special-case cloning the innermost block; this doesn't have enough in
     * common with subsequent steps to include in the loop.
     *
     * js_CloneBlockObject leaves the clone's parent slot uninitialized. We
     * populate it below.
     */
    JSObject *innermostNewChild = js_CloneBlockObject(cx, sharedBlock, fp);
    if (!innermostNewChild)
        return NULL;
    JSAutoTempValueRooter tvr(cx, innermostNewChild);

    /*
     * Clone our way towards outer scopes until we reach the innermost
     * enclosing function, or the innermost block we've already cloned.
     */
    JSObject *newChild = innermostNewChild;
    for (;;) {
        JS_ASSERT(OBJ_GET_PROTO(cx, newChild) == sharedBlock);
        sharedBlock = OBJ_GET_PARENT(cx, sharedBlock);

        /* Sometimes limitBlock will be NULL, so check that first.  */
        if (sharedBlock == limitBlock || !sharedBlock)
            break;

        /* As in the call above, we don't know the real parent yet.  */
        JSObject *clone
            = js_CloneBlockObject(cx, sharedBlock, fp);
        if (!clone)
            return NULL;

        /*
         * Avoid OBJ_SET_PARENT overhead as newChild cannot escape to
         * other threads.
         */
        STOBJ_SET_PARENT(newChild, clone);
        newChild = clone;
    }
    STOBJ_SET_PARENT(newChild, fp->scopeChain);


    /*
     * If we found a limit block belonging to this frame, then we should have
     * found it in blockChain.
     */
    JS_ASSERT_IF(limitBlock &&
                 OBJ_GET_CLASS(cx, limitBlock) == &js_BlockClass &&
                 limitClone->getPrivate() == fp,
                 sharedBlock);

    /* Place our newly cloned blocks at the head of the scope chain.  */
    fp->scopeChain = innermostNewChild;
    return fp->scopeChain;
}

JSBool
js_GetPrimitiveThis(JSContext *cx, jsval *vp, JSClass *clasp, jsval *thisvp)
{
    jsval v;
    JSObject *obj;

    v = vp[1];
    if (JSVAL_IS_OBJECT(v)) {
        obj = JS_THIS_OBJECT(cx, vp);
        if (!JS_InstanceOf(cx, obj, clasp, vp + 2))
            return JS_FALSE;
        v = obj->fslots[JSSLOT_PRIMITIVE_THIS];
    }
    *thisvp = v;
    return JS_TRUE;
}

/* Some objects (e.g., With) delegate 'this' to another object. */
static inline JSObject *
CallThisObjectHook(JSContext *cx, JSObject *obj, jsval *argv)
{
    JSObject *thisp = obj->thisObject(cx);
    if (!thisp)
        return NULL;
    argv[-1] = OBJECT_TO_JSVAL(thisp);
    return thisp;
}

/*
 * ECMA requires "the global object", but in embeddings such as the browser,
 * which have multiple top-level objects (windows, frames, etc. in the DOM),
 * we prefer fun's parent.  An example that causes this code to run:
 *
 *   // in window w1
 *   function f() { return this }
 *   function g() { return f }
 *
 *   // in window w2
 *   var h = w1.g()
 *   alert(h() == w1)
 *
 * The alert should display "true".
 */
JS_STATIC_INTERPRET JSObject *
js_ComputeGlobalThis(JSContext *cx, JSBool lazy, jsval *argv)
{
    JSObject *thisp;

    if (JSVAL_IS_PRIMITIVE(argv[-2]) ||
        !OBJ_GET_PARENT(cx, JSVAL_TO_OBJECT(argv[-2]))) {
        thisp = cx->globalObject;
    } else {
        JSStackFrame *fp;
        jsid id;
        jsval v;
        uintN attrs;
        JSBool ok;
        JSObject *parent;

        /*
         * Walk up the parent chain, first checking that the running script
         * has access to the callee's parent object. Note that if lazy, the
         * running script whose principals we want to check is the script
         * associated with fp->down, not with fp.
         *
         * FIXME: 417851 -- this access check should not be required, as it
         * imposes a performance penalty on all js_ComputeGlobalThis calls,
         * and it represents a maintenance hazard.
         */
        fp = js_GetTopStackFrame(cx);    /* quell GCC overwarning */
        if (lazy) {
            JS_ASSERT(fp->argv == argv);
            fp->dormantNext = cx->dormantFrameChain;
            cx->dormantFrameChain = fp;
            cx->fp = fp->down;
            fp->down = NULL;
        }
        thisp = JSVAL_TO_OBJECT(argv[-2]);
        id = ATOM_TO_JSID(cx->runtime->atomState.parentAtom);

        ok = thisp->checkAccess(cx, id, JSACC_PARENT, &v, &attrs);
        if (lazy) {
            cx->dormantFrameChain = fp->dormantNext;
            fp->dormantNext = NULL;
            fp->down = cx->fp;
            cx->fp = fp;
        }
        if (!ok)
            return NULL;

        thisp = JSVAL_IS_VOID(v)
                ? OBJ_GET_PARENT(cx, thisp)
                : JSVAL_TO_OBJECT(v);
        while ((parent = OBJ_GET_PARENT(cx, thisp)) != NULL)
            thisp = parent;
    }

    return CallThisObjectHook(cx, thisp, argv);
}

static JSObject *
ComputeThis(JSContext *cx, JSBool lazy, jsval *argv)
{
    JSObject *thisp;

    JS_ASSERT(!JSVAL_IS_NULL(argv[-1]));
    if (!JSVAL_IS_OBJECT(argv[-1])) {
        if (!js_PrimitiveToObject(cx, &argv[-1]))
            return NULL;
        thisp = JSVAL_TO_OBJECT(argv[-1]);
        return thisp;
    } 

    thisp = JSVAL_TO_OBJECT(argv[-1]);
    if (OBJ_GET_CLASS(cx, thisp) == &js_CallClass || OBJ_GET_CLASS(cx, thisp) == &js_BlockClass)
        return js_ComputeGlobalThis(cx, lazy, argv);

    return CallThisObjectHook(cx, thisp, argv);
}

JSObject *
js_ComputeThis(JSContext *cx, JSBool lazy, jsval *argv)
{
    JS_ASSERT(argv[-1] != JSVAL_HOLE);  // check for SynthesizeFrame poisoning
    if (JSVAL_IS_NULL(argv[-1]))
        return js_ComputeGlobalThis(cx, lazy, argv);
    return ComputeThis(cx, lazy, argv);
}

#if JS_HAS_NO_SUCH_METHOD

const uint32 JSSLOT_FOUND_FUNCTION  = JSSLOT_PRIVATE;
const uint32 JSSLOT_SAVED_ID        = JSSLOT_PRIVATE + 1;

JSClass js_NoSuchMethodClass = {
    "NoSuchMethod",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,    NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/*
 * When JSOP_CALLPROP or JSOP_CALLELEM does not find the method property of
 * the base object, we search for the __noSuchMethod__ method in the base.
 * If it exists, we store the method and the property's id into an object of
 * NoSuchMethod class and store this object into the callee's stack slot.
 * Later, js_Invoke will recognise such an object and transfer control to
 * NoSuchMethod that invokes the method like:
 *
 *   this.__noSuchMethod__(id, args)
 *
 * where id is the name of the method that this invocation attempted to
 * call by name, and args is an Array containing this invocation's actual
 * parameters.
 */
JS_STATIC_INTERPRET JSBool
js_OnUnknownMethod(JSContext *cx, jsval *vp)
{
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(vp[1]));

    JSObject *obj = JSVAL_TO_OBJECT(vp[1]);
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.noSuchMethodAtom);
    JSAutoTempValueRooter tvr(cx, JSVAL_NULL);
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, tvr.addr()))
        return false;
    if (JSVAL_IS_PRIMITIVE(tvr.value())) {
        vp[0] = tvr.value();
    } else {
#if JS_HAS_XML_SUPPORT
        /* Extract the function name from function::name qname. */
        if (!JSVAL_IS_PRIMITIVE(vp[0])) {
            obj = JSVAL_TO_OBJECT(vp[0]);
            if (!js_IsFunctionQName(cx, obj, &id))
                return false;
            if (id != 0)
                vp[0] = ID_TO_VALUE(id);
        }
#endif
        obj = js_NewObjectWithGivenProto(cx, &js_NoSuchMethodClass,
                                         NULL, NULL);
        if (!obj)
            return false;
        obj->fslots[JSSLOT_FOUND_FUNCTION] = tvr.value();
        obj->fslots[JSSLOT_SAVED_ID] = vp[0];
        vp[0] = OBJECT_TO_JSVAL(obj);
    }
    return true;
}

static JS_REQUIRES_STACK JSBool
NoSuchMethod(JSContext *cx, uintN argc, jsval *vp, uint32 flags)
{
    jsval *invokevp;
    void *mark;
    JSBool ok;
    JSObject *obj, *argsobj;

    invokevp = js_AllocStack(cx, 2 + 2, &mark);
    if (!invokevp)
        return JS_FALSE;

    JS_ASSERT(!JSVAL_IS_PRIMITIVE(vp[0]));
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(vp[1]));
    obj = JSVAL_TO_OBJECT(vp[0]);
    JS_ASSERT(STOBJ_GET_CLASS(obj) == &js_NoSuchMethodClass);

    invokevp[0] = obj->fslots[JSSLOT_FOUND_FUNCTION];
    invokevp[1] = vp[1];
    invokevp[2] = obj->fslots[JSSLOT_SAVED_ID];
    argsobj = js_NewArrayObject(cx, argc, vp + 2);
    if (!argsobj) {
        ok = JS_FALSE;
    } else {
        invokevp[3] = OBJECT_TO_JSVAL(argsobj);
        ok = (flags & JSINVOKE_CONSTRUCT)
             ? js_InvokeConstructor(cx, 2, JS_TRUE, invokevp)
             : js_Invoke(cx, 2, invokevp, flags);
        vp[0] = invokevp[0];
    }
    js_FreeStack(cx, mark);
    return ok;
}

#endif /* JS_HAS_NO_SUCH_METHOD */

/*
 * We check if the function accepts a primitive value as |this|. For that we
 * use a table that maps value's tag into the corresponding function flag.
 */
JS_STATIC_ASSERT(JSVAL_INT == 1);
JS_STATIC_ASSERT(JSVAL_DOUBLE == 2);
JS_STATIC_ASSERT(JSVAL_STRING == 4);
JS_STATIC_ASSERT(JSVAL_SPECIAL == 6);

const uint16 js_PrimitiveTestFlags[] = {
    JSFUN_THISP_NUMBER,     /* INT     */
    JSFUN_THISP_NUMBER,     /* DOUBLE  */
    JSFUN_THISP_NUMBER,     /* INT     */
    JSFUN_THISP_STRING,     /* STRING  */
    JSFUN_THISP_NUMBER,     /* INT     */
    JSFUN_THISP_BOOLEAN,    /* BOOLEAN */
    JSFUN_THISP_NUMBER      /* INT     */
};

/*
 * Find a function reference and its 'this' object implicit first parameter
 * under argc arguments on cx's stack, and call the function.  Push missing
 * required arguments, allocate declared local variables, and pop everything
 * when done.  Then push the return value.
 */
JS_REQUIRES_STACK JS_FRIEND_API(JSBool)
js_Invoke(JSContext *cx, uintN argc, jsval *vp, uintN flags)
{
    void *mark;
    JSStackFrame frame;
    jsval *sp, *argv, *newvp;
    jsval v;
    JSObject *funobj, *parent;
    JSBool ok;
    JSClass *clasp;
    const JSObjectOps *ops;
    JSNative native;
    JSFunction *fun;
    JSScript *script;
    uintN nslots, i;
    uint32 rootedArgsFlag;
    JSInterpreterHook hook;
    void *hookData;

    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);

    /* [vp .. vp + 2 + argc) must belong to the last JS stack arena. */
    JS_ASSERT((jsval *) cx->stackPool.current->base <= vp);
    JS_ASSERT(vp + 2 + argc <= (jsval *) cx->stackPool.current->avail);

    /* Mark the top of stack and load frequently-used registers. */
    mark = JS_ARENA_MARK(&cx->stackPool);
    MUST_FLOW_THROUGH("out2");
    v = *vp;

    if (JSVAL_IS_PRIMITIVE(v))
        goto bad;

    funobj = JSVAL_TO_OBJECT(v);
    parent = OBJ_GET_PARENT(cx, funobj);
    clasp = OBJ_GET_CLASS(cx, funobj);
    if (clasp != &js_FunctionClass) {
#if JS_HAS_NO_SUCH_METHOD
        if (clasp == &js_NoSuchMethodClass) {
            ok = NoSuchMethod(cx, argc, vp, flags);
            goto out2;
        }
#endif

        /* Function is inlined, all other classes use object ops. */
        ops = funobj->map->ops;

        /*
         * XXX this makes no sense -- why convert to function if clasp->call?
         * XXX better to call that hook without converting
         *
         * FIXME bug 408416: try converting to function, for API compatibility
         * if there is a call op defined.
         */
        if ((ops == &js_ObjectOps) ? clasp->call : ops->call) {
            ok = clasp->convert(cx, funobj, JSTYPE_FUNCTION, &v);
            if (!ok)
                goto out2;

            if (VALUE_IS_FUNCTION(cx, v)) {
                /* Make vp refer to funobj to keep it available as argv[-2]. */
                *vp = v;
                funobj = JSVAL_TO_OBJECT(v);
                parent = OBJ_GET_PARENT(cx, funobj);
                goto have_fun;
            }
        }
        fun = NULL;
        script = NULL;
        nslots = 0;

        /* Try a call or construct native object op. */
        if (flags & JSINVOKE_CONSTRUCT) {
            if (!JSVAL_IS_OBJECT(vp[1])) {
                ok = js_PrimitiveToObject(cx, &vp[1]);
                if (!ok)
                    goto out2;
            }
            native = ops->construct;
        } else {
            native = ops->call;
        }
        if (!native)
            goto bad;
    } else {
have_fun:
        /* Get private data and set derived locals from it. */
        fun = GET_FUNCTION_PRIVATE(cx, funobj);
        nslots = FUN_MINARGS(fun);
        nslots = (nslots > argc) ? nslots - argc : 0;
        if (FUN_INTERPRETED(fun)) {
            native = NULL;
            script = fun->u.i.script;
            JS_ASSERT(script);

            if (script->isEmpty()) {
                if (flags & JSINVOKE_CONSTRUCT) {
                    JS_ASSERT(!JSVAL_IS_PRIMITIVE(vp[1]));
                    *vp = vp[1];
                } else {
                    *vp = JSVAL_VOID;
                }
                ok = JS_TRUE;
                goto out2;
            }
        } else {
            native = fun->u.n.native;
            script = NULL;
            nslots += fun->u.n.extra;
        }

        if (JSFUN_BOUND_METHOD_TEST(fun->flags)) {
            /* Handle bound method special case. */
            vp[1] = OBJECT_TO_JSVAL(parent);
        } else if (!JSVAL_IS_OBJECT(vp[1])) {
            JS_ASSERT(!(flags & JSINVOKE_CONSTRUCT));
            if (PRIMITIVE_THIS_TEST(fun, vp[1]))
                goto start_call;
        }
    }

    if (flags & JSINVOKE_CONSTRUCT) {
        JS_ASSERT(!JSVAL_IS_PRIMITIVE(vp[1]));
    } else {
        /*
         * We must call js_ComputeThis in case we are not called from the
         * interpreter, where a prior bytecode has computed an appropriate
         * |this| already.
         *
         * But we need to compute |this| eagerly only for so-called "slow"
         * (i.e., not fast) native functions. Fast natives must use either
         * JS_THIS or JS_THIS_OBJECT, and scripted functions will go through
         * the appropriate this-computing bytecode, e.g., JSOP_THIS.
         */
        if (native && (!fun || !(fun->flags & JSFUN_FAST_NATIVE))) {
            if (!js_ComputeThis(cx, JS_FALSE, vp + 2)) {
                ok = JS_FALSE;
                goto out2;
            }
            flags |= JSFRAME_COMPUTED_THIS;
        }
    }

  start_call:
    if (native && fun && (fun->flags & JSFUN_FAST_NATIVE)) {
#ifdef DEBUG_NOT_THROWING
        JSBool alreadyThrowing = cx->throwing;
#endif
        JS_ASSERT(nslots == 0);
        ok = ((JSFastNative) native)(cx, argc, vp);
        JS_RUNTIME_METER(cx->runtime, nativeCalls);
#ifdef DEBUG_NOT_THROWING
        if (ok && !alreadyThrowing)
            ASSERT_NOT_THROWING(cx);
#endif
        goto out2;
    }

    argv = vp + 2;
    sp = argv + argc;

    rootedArgsFlag = JSFRAME_ROOTED_ARGV;
    if (nslots != 0) {
        /*
         * The extra slots required by the function continue with argument
         * slots. Thus, when the last stack pool arena does not have room to
         * fit nslots right after sp and AllocateAfterSP fails, we have to copy
         * [vp..vp+2+argc) slots and clear rootedArgsFlag to root the copy.
         */
        if (!AllocateAfterSP(cx, sp, nslots)) {
            rootedArgsFlag = 0;
            newvp = js_AllocRawStack(cx, 2 + argc + nslots, NULL);
            if (!newvp) {
                ok = JS_FALSE;
                goto out2;
            }
            memcpy(newvp, vp, (2 + argc) * sizeof(jsval));
            argv = newvp + 2;
            sp = argv + argc;
        }

        /* Push void to initialize missing args. */
        i = nslots;
        do {
            *sp++ = JSVAL_VOID;
        } while (--i != 0);
    }

    /* Allocate space for local variables and stack of interpreted function. */
    if (script && script->nslots != 0) {
        if (!AllocateAfterSP(cx, sp, script->nslots)) {
            /* NB: Discontinuity between argv and slots, stack slots. */
            sp = js_AllocRawStack(cx, script->nslots, NULL);
            if (!sp) {
                ok = JS_FALSE;
                goto out2;
            }
        }

        /* Push void to initialize local variables. */
        for (jsval *end = sp + fun->u.i.nvars; sp != end; ++sp)
            *sp = JSVAL_VOID;
    }

    /*
     * Initialize the frame.
     */
    frame.thisv = vp[1];
    frame.varobj = NULL;
    frame.callobj = NULL;
    frame.argsobj = NULL;
    frame.script = script;
    frame.fun = fun;
    frame.argc = argc;
    frame.argv = argv;

    /* Default return value for a constructor is the new object. */
    frame.rval = (flags & JSINVOKE_CONSTRUCT) ? vp[1] : JSVAL_VOID;
    frame.down = cx->fp;
    frame.annotation = NULL;
    frame.scopeChain = NULL;    /* set below for real, after cx->fp is set */
    frame.blockChain = NULL;
    frame.regs = NULL;
    frame.imacpc = NULL;
    frame.slots = NULL;
    frame.flags = flags | rootedArgsFlag;
    frame.dormantNext = NULL;
    frame.displaySave = NULL;

    MUST_FLOW_THROUGH("out");
    cx->fp = &frame;

    /* Init these now in case we goto out before first hook call. */
    hook = cx->debugHooks->callHook;
    hookData = NULL;

    if (native) {
        /* If native, use caller varobj and scopeChain for eval. */
        JS_ASSERT(!frame.varobj);
        JS_ASSERT(!frame.scopeChain);
        if (frame.down) {
            frame.varobj = frame.down->varobj;
            frame.scopeChain = frame.down->scopeChain;
        }

        /* But ensure that we have a scope chain. */
        if (!frame.scopeChain)
            frame.scopeChain = parent;
    } else {
        /* Use parent scope so js_GetCallObject can find the right "Call". */
        frame.scopeChain = parent;
        if (JSFUN_HEAVYWEIGHT_TEST(fun->flags)) {
            /* Scope with a call object parented by the callee's parent. */
            if (!js_GetCallObject(cx, &frame)) {
                ok = JS_FALSE;
                goto out;
            }
        }
        frame.slots = sp - fun->u.i.nvars;
    }

    /* Call the hook if present after we fully initialized the frame. */
    if (hook)
        hookData = hook(cx, &frame, JS_TRUE, 0, cx->debugHooks->callHookData);

#ifdef INCLUDE_MOZILLA_DTRACE
    /* DTrace function entry, non-inlines */
    if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
        jsdtrace_function_entry(cx, &frame, fun);
    if (JAVASCRIPT_FUNCTION_INFO_ENABLED())
        jsdtrace_function_info(cx, &frame, frame.down, fun);
    if (JAVASCRIPT_FUNCTION_ARGS_ENABLED())
        jsdtrace_function_args(cx, &frame, fun, frame.argc, frame.argv);
#endif

    /* Call the function, either a native method or an interpreted script. */
    if (native) {
#ifdef DEBUG_NOT_THROWING
        JSBool alreadyThrowing = cx->throwing;
#endif
        /* Primitive |this| should not be passed to slow natives. */
        JSObject *thisp = JSVAL_TO_OBJECT(frame.thisv);
        ok = native(cx, thisp, argc, frame.argv, &frame.rval);
        JS_RUNTIME_METER(cx->runtime, nativeCalls);
#ifdef DEBUG_NOT_THROWING
        if (ok && !alreadyThrowing)
            ASSERT_NOT_THROWING(cx);
#endif
    } else {
        JS_ASSERT(script);
        ok = js_Interpret(cx);
    }

#ifdef INCLUDE_MOZILLA_DTRACE
    /* DTrace function return, non-inlines */
    if (JAVASCRIPT_FUNCTION_RVAL_ENABLED())
        jsdtrace_function_rval(cx, &frame, fun, &frame.rval);
    if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
        jsdtrace_function_return(cx, &frame, fun);
#endif

out:
    if (hookData) {
        hook = cx->debugHooks->callHook;
        if (hook)
            hook(cx, &frame, JS_FALSE, &ok, hookData);
    }

    frame.putActivationObjects(cx);

    *vp = frame.rval;

    /* Restore cx->fp now that we're done releasing frame objects. */
    cx->fp = frame.down;

out2:
    /* Pop everything we may have allocated off the stack. */
    JS_ARENA_RELEASE(&cx->stackPool, mark);
    if (!ok)
        *vp = JSVAL_NULL;
    return ok;

bad:
    js_ReportIsNotFunction(cx, vp, flags & JSINVOKE_FUNFLAGS);
    ok = JS_FALSE;
    goto out2;
}

JSBool
js_InternalInvoke(JSContext *cx, JSObject *obj, jsval fval, uintN flags,
                  uintN argc, jsval *argv, jsval *rval)
{
    jsval *invokevp;
    void *mark;
    JSBool ok;

    js_LeaveTrace(cx);
    invokevp = js_AllocStack(cx, 2 + argc, &mark);
    if (!invokevp)
        return JS_FALSE;

    invokevp[0] = fval;
    invokevp[1] = OBJECT_TO_JSVAL(obj);
    memcpy(invokevp + 2, argv, argc * sizeof *argv);

    ok = js_Invoke(cx, argc, invokevp, flags);
    if (ok) {
        /*
         * Store *rval in the a scoped local root if a scope is open, else in
         * the lastInternalResult pigeon-hole GC root, solely so users of
         * js_InternalInvoke and its direct and indirect (js_ValueToString for
         * example) callers do not need to manage roots for local, temporary
         * references to such results.
         */
        *rval = *invokevp;
        if (JSVAL_IS_GCTHING(*rval) && *rval != JSVAL_NULL) {
            JSLocalRootStack *lrs = JS_THREAD_DATA(cx)->localRootStack;
            if (lrs) {
                if (js_PushLocalRoot(cx, lrs, *rval) < 0)
                    ok = JS_FALSE;
            } else {
                cx->weakRoots.lastInternalResult = *rval;
            }
        }
    }

    js_FreeStack(cx, mark);
    return ok;
}

JSBool
js_InternalGetOrSet(JSContext *cx, JSObject *obj, jsid id, jsval fval,
                    JSAccessMode mode, uintN argc, jsval *argv, jsval *rval)
{
    js_LeaveTrace(cx);

    /*
     * js_InternalInvoke could result in another try to get or set the same id
     * again, see bug 355497.
     */
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    return js_InternalCall(cx, obj, fval, argc, argv, rval);
}

JSBool
js_Execute(JSContext *cx, JSObject *chain, JSScript *script,
           JSStackFrame *down, uintN flags, jsval *result)
{
    JSInterpreterHook hook;
    void *hookData, *mark;
    JSStackFrame *oldfp, frame;
    JSObject *obj, *tmp;
    JSBool ok;

    if (script->isEmpty()) {
        if (result)
            *result = JSVAL_VOID;
        return JS_TRUE;
    }

    js_LeaveTrace(cx);

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_START_ENABLED())
        jsdtrace_execute_start(script);
#endif

    hook = cx->debugHooks->executeHook;
    hookData = mark = NULL;
    oldfp = js_GetTopStackFrame(cx);
    frame.script = script;
    if (down) {
        /* Propagate arg state for eval and the debugger API. */
        frame.callobj = down->callobj;
        frame.argsobj = down->argsobj;
        frame.varobj = down->varobj;
        frame.fun = (script->staticLevel > 0) ? down->fun : NULL;
        frame.thisv = down->thisv;
        if (down->flags & JSFRAME_COMPUTED_THIS)
            flags |= JSFRAME_COMPUTED_THIS;
        frame.argc = down->argc;
        frame.argv = down->argv;
        frame.annotation = down->annotation;
    } else {
        frame.callobj = NULL;
        frame.argsobj = NULL;
        obj = chain;
        if (cx->options & JSOPTION_VAROBJFIX) {
            while ((tmp = OBJ_GET_PARENT(cx, obj)) != NULL)
                obj = tmp;
        }
        frame.varobj = obj;
        frame.fun = NULL;
        frame.thisv = OBJECT_TO_JSVAL(chain);
        frame.argc = 0;
        frame.argv = NULL;
        frame.annotation = NULL;
    }

    frame.imacpc = NULL;
    if (script->nslots != 0) {
        frame.slots = js_AllocRawStack(cx, script->nslots, &mark);
        if (!frame.slots) {
            ok = JS_FALSE;
            goto out;
        }
        memset(frame.slots, 0, script->nfixed * sizeof(jsval));

#if JS_HAS_SHARP_VARS
        JS_STATIC_ASSERT(SHARP_NSLOTS == 2);

        if (script->hasSharps) {
            JS_ASSERT(script->nfixed >= SHARP_NSLOTS);
            jsval *sharps = &frame.slots[script->nfixed - SHARP_NSLOTS];

            if (down && down->script && down->script->hasSharps) {
                JS_ASSERT(down->script->nfixed >= SHARP_NSLOTS);
                int base = (down->fun && !(down->flags & JSFRAME_SPECIAL))
                           ? down->fun->sharpSlotBase(cx)
                           : down->script->nfixed - SHARP_NSLOTS;
                if (base < 0) {
                    ok = JS_FALSE;
                    goto out;
                }
                sharps[0] = down->slots[base];
                sharps[1] = down->slots[base + 1];
            } else {
                sharps[0] = sharps[1] = JSVAL_VOID;
            }
        }
#endif
    } else {
        frame.slots = NULL;
    }

    frame.rval = JSVAL_VOID;
    frame.down = down;
    frame.scopeChain = chain;
    frame.regs = NULL;
    frame.flags = flags;
    frame.dormantNext = NULL;
    frame.blockChain = NULL;

    /*
     * Here we wrap the call to js_Interpret with code to (conditionally)
     * save and restore the old stack frame chain into a chain of 'dormant'
     * frame chains.  Since we are replacing cx->fp, we were running into
     * the problem that if GC was called under this frame, some of the GC
     * things associated with the old frame chain (available here only in
     * the C variable 'oldfp') were not rooted and were being collected.
     *
     * So, now we preserve the links to these 'dormant' frame chains in cx
     * before calling js_Interpret and cleanup afterwards.  The GC walks
     * these dormant chains and marks objects in the same way that it marks
     * objects in the primary cx->fp chain.
     */
    if (oldfp && oldfp != down) {
        JS_ASSERT(!oldfp->dormantNext);
        oldfp->dormantNext = cx->dormantFrameChain;
        cx->dormantFrameChain = oldfp;
    }

    cx->fp = &frame;
    if (!down) {
        OBJ_TO_INNER_OBJECT(cx, chain);
        if (!chain)
            return JS_FALSE;
        frame.scopeChain = chain;

        JSObject *thisp = JSVAL_TO_OBJECT(frame.thisv)->thisObject(cx);
        if (!thisp) {
            ok = JS_FALSE;
            goto out2;
        }
        frame.thisv = OBJECT_TO_JSVAL(thisp);
        frame.flags |= JSFRAME_COMPUTED_THIS;
    }

    if (hook) {
        hookData = hook(cx, &frame, JS_TRUE, 0,
                        cx->debugHooks->executeHookData);
    }

    ok = js_Interpret(cx);
    if (result)
        *result = frame.rval;

    if (hookData) {
        hook = cx->debugHooks->executeHook;
        if (hook)
            hook(cx, &frame, JS_FALSE, &ok, hookData);
    }

out2:
    if (mark)
        js_FreeRawStack(cx, mark);
    cx->fp = oldfp;

    if (oldfp && oldfp != down) {
        JS_ASSERT(cx->dormantFrameChain == oldfp);
        cx->dormantFrameChain = oldfp->dormantNext;
        oldfp->dormantNext = NULL;
    }

out:
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_DONE_ENABLED())
        jsdtrace_execute_done(script);
#endif
    return ok;
}

JSBool
js_CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs,
                      JSObject **objp, JSProperty **propp)
{
    JSObject *obj2;
    JSProperty *prop;
    uintN oldAttrs, report;
    bool isFunction;
    jsval value;
    const char *type, *name;

    /*
     * Both objp and propp must be either null or given. When given, *propp
     * must be null. This way we avoid an extra "if (propp) *propp = NULL" for
     * the common case of a non-existing property.
     */
    JS_ASSERT(!objp == !propp);
    JS_ASSERT_IF(propp, !*propp);

    /* The JSPROP_INITIALIZER case below may generate a warning. Since we must
     * drop the property before reporting it, we insists on !propp to avoid
     * looking up the property again after the reporting is done.
     */
    JS_ASSERT_IF(attrs & JSPROP_INITIALIZER, attrs == JSPROP_INITIALIZER);
    JS_ASSERT_IF(attrs == JSPROP_INITIALIZER, !propp);

    if (!obj->lookupProperty(cx, id, &obj2, &prop))
        return JS_FALSE;
    if (!prop)
        return JS_TRUE;

    /* Use prop as a speedup hint to obj->getAttributes. */
    if (!obj2->getAttributes(cx, id, prop, &oldAttrs)) {
        obj2->dropProperty(cx, prop);
        return JS_FALSE;
    }

    /*
     * If our caller doesn't want prop, drop it (we don't need it any longer).
     */
    if (!propp) {
        obj2->dropProperty(cx, prop);
        prop = NULL;
    } else {
        *objp = obj2;
        *propp = prop;
    }

    if (attrs == JSPROP_INITIALIZER) {
        /* Allow the new object to override properties. */
        if (obj2 != obj)
            return JS_TRUE;

        /* The property must be dropped already. */
        JS_ASSERT(!prop);
        report = JSREPORT_WARNING | JSREPORT_STRICT;

#ifdef __GNUC__
        isFunction = false;     /* suppress bogus gcc warnings */
#endif
    } else {
        /* We allow redeclaring some non-readonly properties. */
        if (((oldAttrs | attrs) & JSPROP_READONLY) == 0) {
            /* Allow redeclaration of variables and functions. */
            if (!(attrs & (JSPROP_GETTER | JSPROP_SETTER)))
                return JS_TRUE;

            /*
             * Allow adding a getter only if a property already has a setter
             * but no getter and similarly for adding a setter. That is, we
             * allow only the following transitions:
             *
             *   no-property --> getter --> getter + setter
             *   no-property --> setter --> getter + setter
             */
            if ((~(oldAttrs ^ attrs) & (JSPROP_GETTER | JSPROP_SETTER)) == 0)
                return JS_TRUE;

            /*
             * Allow redeclaration of an impermanent property (in which case
             * anyone could delete it and redefine it, willy-nilly).
             */
            if (!(oldAttrs & JSPROP_PERMANENT))
                return JS_TRUE;
        }
        if (prop)
            obj2->dropProperty(cx, prop);

        report = JSREPORT_ERROR;
        isFunction = (oldAttrs & (JSPROP_GETTER | JSPROP_SETTER)) != 0;
        if (!isFunction) {
            if (!obj->getProperty(cx, id, &value))
                return JS_FALSE;
            isFunction = VALUE_IS_FUNCTION(cx, value);
        }
    }

    type = (attrs == JSPROP_INITIALIZER)
           ? "property"
           : (oldAttrs & attrs & JSPROP_GETTER)
           ? js_getter_str
           : (oldAttrs & attrs & JSPROP_SETTER)
           ? js_setter_str
           : (oldAttrs & JSPROP_READONLY)
           ? js_const_str
           : isFunction
           ? js_function_str
           : js_var_str;
    name = js_ValueToPrintableString(cx, ID_TO_VALUE(id));
    if (!name)
        return JS_FALSE;
    return JS_ReportErrorFlagsAndNumber(cx, report,
                                        js_GetErrorMessage, NULL,
                                        JSMSG_REDECLARED_VAR,
                                        type, name);
}

JSBool
js_StrictlyEqual(JSContext *cx, jsval lval, jsval rval)
{
    jsval ltag = JSVAL_TAG(lval), rtag = JSVAL_TAG(rval);
    jsdouble ld, rd;

    if (ltag == rtag) {
        if (ltag == JSVAL_STRING) {
            JSString *lstr = JSVAL_TO_STRING(lval),
                     *rstr = JSVAL_TO_STRING(rval);
            return js_EqualStrings(lstr, rstr);
        }
        if (ltag == JSVAL_DOUBLE) {
            ld = *JSVAL_TO_DOUBLE(lval);
            rd = *JSVAL_TO_DOUBLE(rval);
            return JSDOUBLE_COMPARE(ld, ==, rd, JS_FALSE);
        }
        if (ltag == JSVAL_OBJECT &&
            lval != rval &&
            !JSVAL_IS_NULL(lval) &&
            !JSVAL_IS_NULL(rval)) {
            JSObject *lobj, *robj;

            lobj = js_GetWrappedObject(cx, JSVAL_TO_OBJECT(lval));
            robj = js_GetWrappedObject(cx, JSVAL_TO_OBJECT(rval));
            lval = OBJECT_TO_JSVAL(lobj);
            rval = OBJECT_TO_JSVAL(robj);
        }
        return lval == rval;
    }
    if (ltag == JSVAL_DOUBLE && JSVAL_IS_INT(rval)) {
        ld = *JSVAL_TO_DOUBLE(lval);
        rd = JSVAL_TO_INT(rval);
        return JSDOUBLE_COMPARE(ld, ==, rd, JS_FALSE);
    }
    if (JSVAL_IS_INT(lval) && rtag == JSVAL_DOUBLE) {
        ld = JSVAL_TO_INT(lval);
        rd = *JSVAL_TO_DOUBLE(rval);
        return JSDOUBLE_COMPARE(ld, ==, rd, JS_FALSE);
    }
    return lval == rval;
}

static inline bool
IsNegativeZero(jsval v)
{
    return JSVAL_IS_DOUBLE(v) && JSDOUBLE_IS_NEGZERO(*JSVAL_TO_DOUBLE(v));
}

static inline bool
IsNaN(jsval v)
{
    return JSVAL_IS_DOUBLE(v) && JSDOUBLE_IS_NaN(*JSVAL_TO_DOUBLE(v));
}

JSBool
js_SameValue(jsval v1, jsval v2, JSContext *cx)
{
    if (IsNegativeZero(v1))
        return IsNegativeZero(v2);
    if (IsNegativeZero(v2))
        return JS_FALSE;
    if (IsNaN(v1) && IsNaN(v2))
        return JS_TRUE;
    return js_StrictlyEqual(cx, v1, v2);
}

JS_REQUIRES_STACK JSBool
js_InvokeConstructor(JSContext *cx, uintN argc, JSBool clampReturn, jsval *vp)
{
    JSFunction *fun, *fun2;
    JSObject *obj, *obj2, *proto, *parent;
    jsval lval, rval;
    JSClass *clasp;

    fun = NULL;
    obj2 = NULL;
    lval = *vp;
    if (!JSVAL_IS_OBJECT(lval) ||
        (obj2 = JSVAL_TO_OBJECT(lval)) == NULL ||
        /* XXX clean up to avoid special cases above ObjectOps layer */
        OBJ_GET_CLASS(cx, obj2) == &js_FunctionClass ||
        !obj2->map->ops->construct)
    {
        fun = js_ValueToFunction(cx, vp, JSV2F_CONSTRUCT);
        if (!fun)
            return JS_FALSE;
    }

    clasp = &js_ObjectClass;
    if (!obj2) {
        proto = parent = NULL;
        fun = NULL;
    } else {
        /*
         * Get the constructor prototype object for this function.
         * Use the nominal 'this' parameter slot, vp[1], as a local
         * root to protect this prototype, in case it has no other
         * strong refs.
         */
        if (!obj2->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                               &vp[1])) {
            return JS_FALSE;
        }
        rval = vp[1];
        proto = JSVAL_IS_OBJECT(rval) ? JSVAL_TO_OBJECT(rval) : NULL;
        parent = OBJ_GET_PARENT(cx, obj2);

        if (OBJ_GET_CLASS(cx, obj2) == &js_FunctionClass) {
            fun2 = GET_FUNCTION_PRIVATE(cx, obj2);
            if (!FUN_INTERPRETED(fun2) && fun2->u.n.clasp)
                clasp = fun2->u.n.clasp;
        }
    }
    obj = js_NewObject(cx, clasp, proto, parent);
    if (!obj)
        return JS_FALSE;

    /* Now we have an object with a constructor method; call it. */
    vp[1] = OBJECT_TO_JSVAL(obj);
    if (!js_Invoke(cx, argc, vp, JSINVOKE_CONSTRUCT))
        return JS_FALSE;

    /* Check the return value and if it's primitive, force it to be obj. */
    rval = *vp;
    if (clampReturn && JSVAL_IS_PRIMITIVE(rval)) {
        if (!fun) {
            /* native [[Construct]] returning primitive is error */
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_NEW_RESULT,
                                 js_ValueToPrintableString(cx, rval));
            return JS_FALSE;
        }
        *vp = OBJECT_TO_JSVAL(obj);
    }

    JS_RUNTIME_METER(cx->runtime, constructs);
    return JS_TRUE;
}

JSBool
js_InternNonIntElementId(JSContext *cx, JSObject *obj, jsval idval, jsid *idp)
{
    JS_ASSERT(!JSVAL_IS_INT(idval));

#if JS_HAS_XML_SUPPORT
    if (!JSVAL_IS_PRIMITIVE(idval)) {
        if (OBJECT_IS_XML(cx, obj)) {
            *idp = OBJECT_JSVAL_TO_JSID(idval);
            return JS_TRUE;
        }
        if (!js_IsFunctionQName(cx, JSVAL_TO_OBJECT(idval), idp))
            return JS_FALSE;
        if (*idp != 0)
            return JS_TRUE;
    }
#endif

    return js_ValueToStringId(cx, idval, idp);
}

/*
 * Enter the new with scope using an object at sp[-1] and associate the depth
 * of the with block with sp + stackIndex.
 */
JS_STATIC_INTERPRET JS_REQUIRES_STACK JSBool
js_EnterWith(JSContext *cx, jsint stackIndex)
{
    JSStackFrame *fp;
    jsval *sp;
    JSObject *obj, *parent, *withobj;

    fp = cx->fp;
    sp = fp->regs->sp;
    JS_ASSERT(stackIndex < 0);
    JS_ASSERT(StackBase(fp) <= sp + stackIndex);

    if (!JSVAL_IS_PRIMITIVE(sp[-1])) {
        obj = JSVAL_TO_OBJECT(sp[-1]);
    } else {
        obj = js_ValueToNonNullObject(cx, sp[-1]);
        if (!obj)
            return JS_FALSE;
        sp[-1] = OBJECT_TO_JSVAL(obj);
    }

    parent = js_GetScopeChain(cx, fp);
    if (!parent)
        return JS_FALSE;

    OBJ_TO_INNER_OBJECT(cx, obj);
    if (!obj)
        return JS_FALSE;

    withobj = js_NewWithObject(cx, obj, parent,
                               sp + stackIndex - StackBase(fp));
    if (!withobj)
        return JS_FALSE;

    fp->scopeChain = withobj;
    return JS_TRUE;
}

JS_STATIC_INTERPRET JS_REQUIRES_STACK void
js_LeaveWith(JSContext *cx)
{
    JSObject *withobj;

    withobj = cx->fp->scopeChain;
    JS_ASSERT(OBJ_GET_CLASS(cx, withobj) == &js_WithClass);
    JS_ASSERT(withobj->getPrivate() == cx->fp);
    JS_ASSERT(OBJ_BLOCK_DEPTH(cx, withobj) >= 0);
    cx->fp->scopeChain = OBJ_GET_PARENT(cx, withobj);
    withobj->setPrivate(NULL);
}

JS_REQUIRES_STACK JSClass *
js_IsActiveWithOrBlock(JSContext *cx, JSObject *obj, int stackDepth)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, obj);
    if ((clasp == &js_WithClass || clasp == &js_BlockClass) &&
        obj->getPrivate() == cx->fp &&
        OBJ_BLOCK_DEPTH(cx, obj) >= stackDepth) {
        return clasp;
    }
    return NULL;
}

/*
 * Unwind block and scope chains to match the given depth. The function sets
 * fp->sp on return to stackDepth.
 */
JS_REQUIRES_STACK JSBool
js_UnwindScope(JSContext *cx, JSStackFrame *fp, jsint stackDepth,
               JSBool normalUnwind)
{
    JSObject *obj;
    JSClass *clasp;

    JS_ASSERT(stackDepth >= 0);
    JS_ASSERT(StackBase(fp) + stackDepth <= fp->regs->sp);

    for (obj = fp->blockChain; obj; obj = OBJ_GET_PARENT(cx, obj)) {
        JS_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_BlockClass);
        if (OBJ_BLOCK_DEPTH(cx, obj) < stackDepth)
            break;
    }
    fp->blockChain = obj;

    for (;;) {
        obj = fp->scopeChain;
        clasp = js_IsActiveWithOrBlock(cx, obj, stackDepth);
        if (!clasp)
            break;
        if (clasp == &js_BlockClass) {
            /* Don't fail until after we've updated all stacks. */
            normalUnwind &= js_PutBlockObject(cx, normalUnwind);
        } else {
            js_LeaveWith(cx);
        }
    }

    fp->regs->sp = StackBase(fp) + stackDepth;
    return normalUnwind;
}

JS_STATIC_INTERPRET JSBool
js_DoIncDec(JSContext *cx, const JSCodeSpec *cs, jsval *vp, jsval *vp2)
{
    jsval v;
    jsdouble d;

    v = *vp;
    if (JSVAL_IS_DOUBLE(v)) {
        d = *JSVAL_TO_DOUBLE(v);
    } else if (JSVAL_IS_INT(v)) {
        d = JSVAL_TO_INT(v);
    } else {
        d = js_ValueToNumber(cx, vp);
        if (JSVAL_IS_NULL(*vp))
            return JS_FALSE;
        JS_ASSERT(JSVAL_IS_NUMBER(*vp) || *vp == JSVAL_TRUE);

        /* Store the result of v conversion back in vp for post increments. */
        if ((cs->format & JOF_POST) &&
            *vp == JSVAL_TRUE
            && !js_NewNumberInRootedValue(cx, d, vp)) {
            return JS_FALSE;
        }
    }

    (cs->format & JOF_INC) ? d++ : d--;
    if (!js_NewNumberInRootedValue(cx, d, vp2))
        return JS_FALSE;

    if (!(cs->format & JOF_POST))
        *vp = *vp2;
    return JS_TRUE;
}

jsval&
js_GetUpvar(JSContext *cx, uintN level, uintN cookie)
{
    level -= UPVAR_FRAME_SKIP(cookie);
    JS_ASSERT(level < JS_DISPLAY_SIZE);

    JSStackFrame *fp = cx->display[level];
    JS_ASSERT(fp->script);

    uintN slot = UPVAR_FRAME_SLOT(cookie);
    jsval *vp;

    if (!fp->fun) {
        vp = fp->slots + fp->script->nfixed;
    } else if (slot < fp->fun->nargs) {
        vp = fp->argv;
    } else if (slot == CALLEE_UPVAR_SLOT) {
        vp = &fp->argv[-2];
        slot = 0;
    } else {
        slot -= fp->fun->nargs;
        JS_ASSERT(slot < fp->script->nslots);
        vp = fp->slots;
    }

    return vp[slot];
}

#ifdef DEBUG

JS_STATIC_INTERPRET JS_REQUIRES_STACK void
js_TraceOpcode(JSContext *cx)
{
    FILE *tracefp;
    JSStackFrame *fp;
    JSFrameRegs *regs;
    intN ndefs, n, nuses;
    jsval *siter;
    JSString *str;
    JSOp op;

    tracefp = (FILE *) cx->tracefp;
    JS_ASSERT(tracefp);
    fp = cx->fp;
    regs = fp->regs;

    /*
     * Operations in prologues don't produce interesting values, and
     * js_DecompileValueGenerator isn't set up to handle them anyway.
     */
    if (cx->tracePrevPc && regs->pc >= fp->script->main) {
        JSOp tracePrevOp = JSOp(*cx->tracePrevPc);
        ndefs = js_GetStackDefs(cx, &js_CodeSpec[tracePrevOp], tracePrevOp,
                                fp->script, cx->tracePrevPc);

        /*
         * If there aren't that many elements on the stack, then we have
         * probably entered a new frame, and printing output would just be
         * misleading.
         */
        if (ndefs != 0 &&
            ndefs < regs->sp - fp->slots) {
            for (n = -ndefs; n < 0; n++) {
                char *bytes = js_DecompileValueGenerator(cx, n, regs->sp[n],
                                                         NULL);
                if (bytes) {
                    fprintf(tracefp, "%s %s",
                            (n == -ndefs) ? "  output:" : ",",
                            bytes);
                    cx->free(bytes);
                }
            }
            fprintf(tracefp, " @ %u\n", (uintN) (regs->sp - StackBase(fp)));
        }
        fprintf(tracefp, "  stack: ");
        for (siter = StackBase(fp); siter < regs->sp; siter++) {
            str = js_ValueToString(cx, *siter);
            if (!str)
                fputs("<null>", tracefp);
            else
                js_FileEscapedString(tracefp, str, 0);
            fputc(' ', tracefp);
        }
        fputc('\n', tracefp);
    }

    fprintf(tracefp, "%4u: ",
            js_PCToLineNumber(cx, fp->script, fp->imacpc ? fp->imacpc : regs->pc));
    js_Disassemble1(cx, fp->script, regs->pc,
                    regs->pc - fp->script->code,
                    JS_FALSE, tracefp);
    op = (JSOp) *regs->pc;
    nuses = js_GetStackUses(&js_CodeSpec[op], op, regs->pc);
    if (nuses != 0) {
        for (n = -nuses; n < 0; n++) {
            char *bytes = js_DecompileValueGenerator(cx, n, regs->sp[n],
                                                     NULL);
            if (bytes) {
                fprintf(tracefp, "%s %s",
                        (n == -nuses) ? "  inputs:" : ",",
                        bytes);
                cx->free(bytes);
            }
        }
        fprintf(tracefp, " @ %u\n", (uintN) (regs->sp - StackBase(fp)));
    }
    cx->tracePrevPc = regs->pc;

    /* It's nice to have complete traces when debugging a crash.  */
    fflush(tracefp);
}

#endif /* DEBUG */

#ifdef JS_OPMETER

# include <stdlib.h>

# define HIST_NSLOTS            8

/*
 * The second dimension is hardcoded at 256 because we know that many bits fit
 * in a byte, and mainly to optimize away multiplying by JSOP_LIMIT to address
 * any particular row.
 */
static uint32 succeeds[JSOP_LIMIT][256];
static uint32 slot_ops[JSOP_LIMIT][HIST_NSLOTS];

JS_STATIC_INTERPRET void
js_MeterOpcodePair(JSOp op1, JSOp op2)
{
    if (op1 != JSOP_STOP)
        ++succeeds[op1][op2];
}

JS_STATIC_INTERPRET void
js_MeterSlotOpcode(JSOp op, uint32 slot)
{
    if (slot < HIST_NSLOTS)
        ++slot_ops[op][slot];
}

typedef struct Edge {
    const char  *from;
    const char  *to;
    uint32      count;
} Edge;

static int
compare_edges(const void *a, const void *b)
{
    const Edge *ea = (const Edge *) a;
    const Edge *eb = (const Edge *) b;

    return (int32)eb->count - (int32)ea->count;
}

void
js_DumpOpMeters()
{
    const char *name, *from, *style;
    FILE *fp;
    uint32 total, count;
    uint32 i, j, nedges;
    Edge *graph;

    name = getenv("JS_OPMETER_FILE");
    if (!name)
        name = "/tmp/ops.dot";
    fp = fopen(name, "w");
    if (!fp) {
        perror(name);
        return;
    }

    total = nedges = 0;
    for (i = 0; i < JSOP_LIMIT; i++) {
        for (j = 0; j < JSOP_LIMIT; j++) {
            count = succeeds[i][j];
            if (count != 0) {
                total += count;
                ++nedges;
            }
        }
    }

# define SIGNIFICANT(count,total) (200. * (count) >= (total))

    graph = (Edge *) js_calloc(nedges * sizeof graph[0]);
    for (i = nedges = 0; i < JSOP_LIMIT; i++) {
        from = js_CodeName[i];
        for (j = 0; j < JSOP_LIMIT; j++) {
            count = succeeds[i][j];
            if (count != 0 && SIGNIFICANT(count, total)) {
                graph[nedges].from = from;
                graph[nedges].to = js_CodeName[j];
                graph[nedges].count = count;
                ++nedges;
            }
        }
    }
    qsort(graph, nedges, sizeof(Edge), compare_edges);

# undef SIGNIFICANT

    fputs("digraph {\n", fp);
    for (i = 0, style = NULL; i < nedges; i++) {
        JS_ASSERT(i == 0 || graph[i-1].count >= graph[i].count);
        if (!style || graph[i-1].count != graph[i].count) {
            style = (i > nedges * .75) ? "dotted" :
                    (i > nedges * .50) ? "dashed" :
                    (i > nedges * .25) ? "solid" : "bold";
        }
        fprintf(fp, "  %s -> %s [label=\"%lu\" style=%s]\n",
                graph[i].from, graph[i].to,
                (unsigned long)graph[i].count, style);
    }
    js_free(graph);
    fputs("}\n", fp);
    fclose(fp);

    name = getenv("JS_OPMETER_HIST");
    if (!name)
        name = "/tmp/ops.hist";
    fp = fopen(name, "w");
    if (!fp) {
        perror(name);
        return;
    }
    fputs("bytecode", fp);
    for (j = 0; j < HIST_NSLOTS; j++)
        fprintf(fp, "  slot %1u", (unsigned)j);
    putc('\n', fp);
    fputs("========", fp);
    for (j = 0; j < HIST_NSLOTS; j++)
        fputs(" =======", fp);
    putc('\n', fp);
    for (i = 0; i < JSOP_LIMIT; i++) {
        for (j = 0; j < HIST_NSLOTS; j++) {
            if (slot_ops[i][j] != 0) {
                /* Reuse j in the next loop, since we break after. */
                fprintf(fp, "%-8.8s", js_CodeName[i]);
                for (j = 0; j < HIST_NSLOTS; j++)
                    fprintf(fp, " %7lu", (unsigned long)slot_ops[i][j]);
                putc('\n', fp);
                break;
            }
        }
    }
    fclose(fp);
}

#endif /* JS_OPSMETER */

#endif /* !JS_LONE_INTERPRET ^ defined jsinvoke_cpp___ */

#ifndef  jsinvoke_cpp___

#define PUSH(v)         (*regs.sp++ = (v))
#define PUSH_OPND(v)    PUSH(v)
#define STORE_OPND(n,v) (regs.sp[n] = (v))
#define POP()           (*--regs.sp)
#define POP_OPND()      POP()
#define FETCH_OPND(n)   (regs.sp[n])

/*
 * Push the jsdouble d using sp from the lexical environment. Try to convert d
 * to a jsint that fits in a jsval, otherwise GC-alloc space for it and push a
 * reference.
 */
#define STORE_NUMBER(cx, n, d)                                                \
    JS_BEGIN_MACRO                                                            \
        jsint i_;                                                             \
                                                                              \
        if (JSDOUBLE_IS_INT(d, i_) && INT_FITS_IN_JSVAL(i_))                  \
            regs.sp[n] = INT_TO_JSVAL(i_);                                    \
        else if (!js_NewDoubleInRootedValue(cx, d, &regs.sp[n]))              \
            goto error;                                                       \
    JS_END_MACRO

#define STORE_INT(cx, n, i)                                                   \
    JS_BEGIN_MACRO                                                            \
        if (INT_FITS_IN_JSVAL(i))                                             \
            regs.sp[n] = INT_TO_JSVAL(i);                                     \
        else if (!js_NewDoubleInRootedValue(cx, (jsdouble) (i), &regs.sp[n])) \
            goto error;                                                       \
    JS_END_MACRO

#define STORE_UINT(cx, n, u)                                                  \
    JS_BEGIN_MACRO                                                            \
        if ((u) <= JSVAL_INT_MAX)                                             \
            regs.sp[n] = INT_TO_JSVAL(u);                                     \
        else if (!js_NewDoubleInRootedValue(cx, (jsdouble) (u), &regs.sp[n])) \
            goto error;                                                       \
    JS_END_MACRO

#define FETCH_NUMBER(cx, n, d)                                                \
    JS_BEGIN_MACRO                                                            \
        jsval v_;                                                             \
                                                                              \
        v_ = FETCH_OPND(n);                                                   \
        VALUE_TO_NUMBER(cx, n, v_, d);                                        \
    JS_END_MACRO

#define FETCH_INT(cx, n, i)                                                   \
    JS_BEGIN_MACRO                                                            \
        jsval v_;                                                             \
                                                                              \
        v_= FETCH_OPND(n);                                                    \
        if (JSVAL_IS_INT(v_)) {                                               \
            i = JSVAL_TO_INT(v_);                                             \
        } else {                                                              \
            i = js_ValueToECMAInt32(cx, &regs.sp[n]);                         \
            if (JSVAL_IS_NULL(regs.sp[n]))                                    \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#define FETCH_UINT(cx, n, ui)                                                 \
    JS_BEGIN_MACRO                                                            \
        jsval v_;                                                             \
                                                                              \
        v_= FETCH_OPND(n);                                                    \
        if (JSVAL_IS_INT(v_)) {                                               \
            ui = (uint32) JSVAL_TO_INT(v_);                                   \
        } else {                                                              \
            ui = js_ValueToECMAUint32(cx, &regs.sp[n]);                       \
            if (JSVAL_IS_NULL(regs.sp[n]))                                    \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

/*
 * Optimized conversion macros that test for the desired type in v before
 * homing sp and calling a conversion function.
 */
#define VALUE_TO_NUMBER(cx, n, v, d)                                          \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(v == regs.sp[n]);                                           \
        if (JSVAL_IS_INT(v)) {                                                \
            d = (jsdouble)JSVAL_TO_INT(v);                                    \
        } else if (JSVAL_IS_DOUBLE(v)) {                                      \
            d = *JSVAL_TO_DOUBLE(v);                                          \
        } else {                                                              \
            d = js_ValueToNumber(cx, &regs.sp[n]);                            \
            if (JSVAL_IS_NULL(regs.sp[n]))                                    \
                goto error;                                                   \
            JS_ASSERT(JSVAL_IS_NUMBER(regs.sp[n]) ||                          \
                      regs.sp[n] == JSVAL_TRUE);                              \
        }                                                                     \
    JS_END_MACRO

#define POP_BOOLEAN(cx, v, b)                                                 \
    JS_BEGIN_MACRO                                                            \
        v = FETCH_OPND(-1);                                                   \
        if (v == JSVAL_NULL) {                                                \
            b = JS_FALSE;                                                     \
        } else if (JSVAL_IS_BOOLEAN(v)) {                                     \
            b = JSVAL_TO_BOOLEAN(v);                                          \
        } else {                                                              \
            b = js_ValueToBoolean(v);                                         \
        }                                                                     \
        regs.sp--;                                                            \
    JS_END_MACRO

#define VALUE_TO_OBJECT(cx, n, v, obj)                                        \
    JS_BEGIN_MACRO                                                            \
        if (!JSVAL_IS_PRIMITIVE(v)) {                                         \
            obj = JSVAL_TO_OBJECT(v);                                         \
        } else {                                                              \
            obj = js_ValueToNonNullObject(cx, v);                             \
            if (!obj)                                                         \
                goto error;                                                   \
            STORE_OPND(n, OBJECT_TO_JSVAL(obj));                              \
        }                                                                     \
    JS_END_MACRO

#define FETCH_OBJECT(cx, n, v, obj)                                           \
    JS_BEGIN_MACRO                                                            \
        v = FETCH_OPND(n);                                                    \
        VALUE_TO_OBJECT(cx, n, v, obj);                                       \
    JS_END_MACRO

#define DEFAULT_VALUE(cx, n, hint, v)                                         \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));                                    \
        JS_ASSERT(v == regs.sp[n]);                                           \
        if (!JSVAL_TO_OBJECT(v)->defaultValue(cx, hint, &regs.sp[n]))         \
            goto error;                                                       \
        v = regs.sp[n];                                                       \
    JS_END_MACRO

/*
 * Quickly test if v is an int from the [-2**29, 2**29) range, that is, when
 * the lowest bit of v is 1 and the bits 30 and 31 are both either 0 or 1. For
 * such v we can do increment or decrement via adding or subtracting two
 * without checking that the result overflows JSVAL_INT_MIN or JSVAL_INT_MAX.
 */
#define CAN_DO_FAST_INC_DEC(v)     (((((v) << 1) ^ v) & 0x80000001) == 1)

JS_STATIC_ASSERT(JSVAL_INT == 1);
JS_STATIC_ASSERT(!CAN_DO_FAST_INC_DEC(INT_TO_JSVAL_CONSTEXPR(JSVAL_INT_MIN)));
JS_STATIC_ASSERT(!CAN_DO_FAST_INC_DEC(INT_TO_JSVAL_CONSTEXPR(JSVAL_INT_MAX)));

/*
 * Conditional assert to detect failure to clear a pending exception that is
 * suppressed (or unintentional suppression of a wanted exception).
 */
#if defined DEBUG_brendan || defined DEBUG_mrbkap || defined DEBUG_shaver
# define DEBUG_NOT_THROWING 1
#endif

#ifdef DEBUG_NOT_THROWING
# define ASSERT_NOT_THROWING(cx) JS_ASSERT(!(cx)->throwing)
#else
# define ASSERT_NOT_THROWING(cx) /* nothing */
#endif

/*
 * Define JS_OPMETER to instrument bytecode succession, generating a .dot file
 * on shutdown that shows the graph of significant predecessor/successor pairs
 * executed, where the edge labels give the succession counts.  The .dot file
 * is named by the JS_OPMETER_FILE envariable, and defaults to /tmp/ops.dot.
 *
 * Bonus feature: JS_OPMETER also enables counters for stack-addressing ops
 * such as JSOP_GETLOCAL, JSOP_INCARG, via METER_SLOT_OP. The resulting counts
 * are written to JS_OPMETER_HIST, defaulting to /tmp/ops.hist.
 */
#ifndef JS_OPMETER
# define METER_OP_INIT(op)      /* nothing */
# define METER_OP_PAIR(op1,op2) /* nothing */
# define METER_SLOT_OP(op,slot) /* nothing */
#else

/*
 * The second dimension is hardcoded at 256 because we know that many bits fit
 * in a byte, and mainly to optimize away multiplying by JSOP_LIMIT to address
 * any particular row.
 */
# define METER_OP_INIT(op)      ((op) = JSOP_STOP)
# define METER_OP_PAIR(op1,op2) (js_MeterOpcodePair(op1, op2))
# define METER_SLOT_OP(op,slot) (js_MeterSlotOpcode(op, slot))

#endif

/*
 * Threaded interpretation via computed goto appears to be well-supported by
 * GCC 3 and higher.  IBM's C compiler when run with the right options (e.g.,
 * -qlanglvl=extended) also supports threading.  Ditto the SunPro C compiler.
 * Currently it's broken for JS_VERSION < 160, though this isn't worth fixing.
 * Add your compiler support macros here.
 */
#ifndef JS_THREADED_INTERP
# if JS_VERSION >= 160 && (                                                   \
    __GNUC__ >= 3 ||                                                          \
    (__IBMC__ >= 700 && defined __IBM_COMPUTED_GOTO) ||                       \
    __SUNPRO_C >= 0x570)
#  define JS_THREADED_INTERP 1
# else
#  define JS_THREADED_INTERP 0
# endif
#endif

/*
 * Deadlocks or else bad races are likely if JS_THREADSAFE, so we must rely on
 * single-thread DEBUG js shell testing to verify property cache hits.
 */
#if defined DEBUG && !defined JS_THREADSAFE

# define ASSERT_VALID_PROPERTY_CACHE_HIT(pcoff,obj,pobj,entry)                \
    JS_BEGIN_MACRO                                                            \
        if (!AssertValidPropertyCacheHit(cx, script, regs, pcoff, obj, pobj,  \
                                         entry)) {                            \
            goto error;                                                       \
        }                                                                     \
    JS_END_MACRO

static bool
AssertValidPropertyCacheHit(JSContext *cx, JSScript *script, JSFrameRegs& regs,
                            ptrdiff_t pcoff, JSObject *start, JSObject *found,
                            JSPropCacheEntry *entry)
{
    uint32 sample = cx->runtime->gcNumber;

    JSAtom *atom;
    if (pcoff >= 0)
        GET_ATOM_FROM_BYTECODE(script, regs.pc, pcoff, atom);
    else
        atom = cx->runtime->atomState.lengthAtom;

    JSObject *obj, *pobj;
    JSProperty *prop;
    JSBool ok;

    if (JOF_OPMODE(*regs.pc) == JOF_NAME) {
        ok = js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &pobj, &prop);
    } else {
        obj = start;
        ok = js_LookupProperty(cx, obj, ATOM_TO_JSID(atom), &pobj, &prop);
    }
    if (!ok)
        return false;
    if (cx->runtime->gcNumber != sample ||
        PCVCAP_SHAPE(entry->vcap) != OBJ_SHAPE(pobj)) {
        pobj->dropProperty(cx, prop);
        return true;
    }
    JS_ASSERT(prop);
    JS_ASSERT(pobj == found);

    JSScopeProperty *sprop = (JSScopeProperty *) prop;
    if (PCVAL_IS_SLOT(entry->vword)) {
        JS_ASSERT(PCVAL_TO_SLOT(entry->vword) == sprop->slot);
        JS_ASSERT(!sprop->isMethod());
    } else if (PCVAL_IS_SPROP(entry->vword)) {
        JS_ASSERT(PCVAL_TO_SPROP(entry->vword) == sprop);
        JS_ASSERT_IF(sprop->isMethod(),
                     sprop->methodValue() == LOCKED_OBJ_GET_SLOT(pobj, sprop->slot));
    } else {
        jsval v;
        JS_ASSERT(PCVAL_IS_OBJECT(entry->vword));
        JS_ASSERT(entry->vword != PCVAL_NULL);
        JS_ASSERT(OBJ_SCOPE(pobj)->brandedOrHasMethodBarrier());
        JS_ASSERT(SPROP_HAS_STUB_GETTER_OR_IS_METHOD(sprop));
        JS_ASSERT(SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(pobj)));
        v = LOCKED_OBJ_GET_SLOT(pobj, sprop->slot);
        JS_ASSERT(VALUE_IS_FUNCTION(cx, v));
        JS_ASSERT(PCVAL_TO_OBJECT(entry->vword) == JSVAL_TO_OBJECT(v));

        if (sprop->isMethod()) {
            JS_ASSERT(js_CodeSpec[*regs.pc].format & JOF_CALLOP);
            JS_ASSERT(sprop->methodValue() == v);
        }
    }

    pobj->dropProperty(cx, prop);
    return true;
}

#else
# define ASSERT_VALID_PROPERTY_CACHE_HIT(pcoff,obj,pobj,entry) ((void) 0)
#endif

/*
 * Ensure that the intrepreter switch can close call-bytecode cases in the
 * same way as non-call bytecodes.
 */
JS_STATIC_ASSERT(JSOP_NAME_LENGTH == JSOP_CALLNAME_LENGTH);
JS_STATIC_ASSERT(JSOP_GETGVAR_LENGTH == JSOP_CALLGVAR_LENGTH);
JS_STATIC_ASSERT(JSOP_GETUPVAR_LENGTH == JSOP_CALLUPVAR_LENGTH);
JS_STATIC_ASSERT(JSOP_GETUPVAR_DBG_LENGTH == JSOP_CALLUPVAR_DBG_LENGTH);
JS_STATIC_ASSERT(JSOP_GETUPVAR_DBG_LENGTH == JSOP_GETUPVAR_LENGTH);
JS_STATIC_ASSERT(JSOP_GETDSLOT_LENGTH == JSOP_CALLDSLOT_LENGTH);
JS_STATIC_ASSERT(JSOP_GETARG_LENGTH == JSOP_CALLARG_LENGTH);
JS_STATIC_ASSERT(JSOP_GETLOCAL_LENGTH == JSOP_CALLLOCAL_LENGTH);
JS_STATIC_ASSERT(JSOP_XMLNAME_LENGTH == JSOP_CALLXMLNAME_LENGTH);

/*
 * Same for debuggable flat closures defined at top level in another function
 * or program fragment.
 */
JS_STATIC_ASSERT(JSOP_DEFFUN_FC_LENGTH == JSOP_DEFFUN_DBGFC_LENGTH);

/*
 * Same for JSOP_SETNAME and JSOP_SETPROP, which differ only slightly but
 * remain distinct for the decompiler. Likewise for JSOP_INIT{PROP,METHOD}.
 */
JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETPROP_LENGTH);
JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETMETHOD_LENGTH);
JS_STATIC_ASSERT(JSOP_INITPROP_LENGTH == JSOP_INITMETHOD_LENGTH);

/* See TRY_BRANCH_AFTER_COND. */
JS_STATIC_ASSERT(JSOP_IFNE_LENGTH == JSOP_IFEQ_LENGTH);
JS_STATIC_ASSERT(JSOP_IFNE == JSOP_IFEQ + 1);

/* For the fastest case inder JSOP_INCNAME, etc. */
JS_STATIC_ASSERT(JSOP_INCNAME_LENGTH == JSOP_DECNAME_LENGTH);
JS_STATIC_ASSERT(JSOP_INCNAME_LENGTH == JSOP_NAMEINC_LENGTH);
JS_STATIC_ASSERT(JSOP_INCNAME_LENGTH == JSOP_NAMEDEC_LENGTH);

#ifdef JS_TRACER
# define ABORT_RECORDING(cx, reason)                                          \
    JS_BEGIN_MACRO                                                            \
        if (TRACE_RECORDER(cx))                                               \
            js_AbortRecording(cx, reason);                                    \
    JS_END_MACRO
#else
# define ABORT_RECORDING(cx, reason)    ((void) 0)
#endif

JS_REQUIRES_STACK JSBool
js_Interpret(JSContext *cx)
{
#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_INTERP);
#endif

    JSRuntime *rt;
    JSStackFrame *fp;
    JSScript *script;
    uintN inlineCallCount;
    JSAtom **atoms;
    JSVersion currentVersion, originalVersion;
    JSFrameRegs regs;
    JSObject *obj, *obj2, *parent;
    JSBool ok, cond;
    jsint len;
    jsbytecode *endpc, *pc2;
    JSOp op, op2;
    jsatomid index;
    JSAtom *atom;
    uintN argc, attrs, flags;
    uint32 slot;
    jsval *vp, lval, rval, ltmp, rtmp;
    jsid id;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSString *str, *str2;
    jsint i, j;
    jsdouble d, d2;
    JSClass *clasp;
    JSFunction *fun;
    JSType type;
    jsint low, high, off, npairs;
    JSBool match;
#if JS_HAS_GETTER_SETTER
    JSPropertyOp getter, setter;
#endif
    JSAutoResolveFlags rf(cx, JSRESOLVE_INFER);

# ifdef DEBUG
    /*
     * We call this macro from BEGIN_CASE in threaded interpreters,
     * and before entering the switch in non-threaded interpreters.
     * However, reaching such points doesn't mean we've actually
     * fetched an OP from the instruction stream: some opcodes use
     * 'op=x; DO_OP()' to let another opcode's implementation finish
     * their work, and many opcodes share entry points with a run of
     * consecutive BEGIN_CASEs.
     *
     * Take care to trace OP only when it is the opcode fetched from
     * the instruction stream, so the trace matches what one would
     * expect from looking at the code.  (We do omit POPs after SETs;
     * unfortunate, but not worth fixing.)
     */
#  define TRACE_OPCODE(OP)  JS_BEGIN_MACRO                                    \
                                if (JS_UNLIKELY(cx->tracefp != NULL) &&       \
                                    (OP) == *regs.pc)                         \
                                    js_TraceOpcode(cx);                       \
                            JS_END_MACRO
# else
#  define TRACE_OPCODE(OP)  ((void) 0)
# endif

#if JS_THREADED_INTERP
    static void *const normalJumpTable[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
        JS_EXTENSION &&L_##op,
# include "jsopcode.tbl"
# undef OPDEF
    };

    static void *const interruptJumpTable[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)              \
        JS_EXTENSION &&interrupt,
# include "jsopcode.tbl"
# undef OPDEF
    };

    register void * const *jumpTable = normalJumpTable;

    METER_OP_INIT(op);      /* to nullify first METER_OP_PAIR */

# define ENABLE_INTERRUPTS() ((void) (jumpTable = interruptJumpTable))

# ifdef JS_TRACER
#  define CHECK_RECORDER()                                                    \
    JS_ASSERT_IF(TRACE_RECORDER(cx), jumpTable == interruptJumpTable)
# else
#  define CHECK_RECORDER()  ((void)0)
# endif

# define DO_OP()            JS_BEGIN_MACRO                                    \
                                CHECK_RECORDER();                             \
                                JS_EXTENSION_(goto *jumpTable[op]);           \
                            JS_END_MACRO
# define DO_NEXT_OP(n)      JS_BEGIN_MACRO                                    \
                                METER_OP_PAIR(op, regs.pc[n]);                \
                                op = (JSOp) *(regs.pc += (n));                \
                                DO_OP();                                      \
                            JS_END_MACRO

# define BEGIN_CASE(OP)     L_##OP: TRACE_OPCODE(OP); CHECK_RECORDER();
# define END_CASE(OP)       DO_NEXT_OP(OP##_LENGTH);
# define END_VARLEN_CASE    DO_NEXT_OP(len);
# define ADD_EMPTY_CASE(OP) BEGIN_CASE(OP)                                    \
                                JS_ASSERT(js_CodeSpec[OP].length == 1);       \
                                op = (JSOp) *++regs.pc;                       \
                                DO_OP();

# define END_EMPTY_CASES

#else /* !JS_THREADED_INTERP */

    register intN switchMask = 0;
    intN switchOp;

# define ENABLE_INTERRUPTS() ((void) (switchMask = -1))

# ifdef JS_TRACER
#  define CHECK_RECORDER()                                                    \
    JS_ASSERT_IF(TRACE_RECORDER(cx), switchMask == -1)
# else
#  define CHECK_RECORDER()  ((void)0)
# endif

# define DO_OP()            goto do_op
# define DO_NEXT_OP(n)      JS_BEGIN_MACRO                                    \
                                JS_ASSERT((n) == len);                        \
                                goto advance_pc;                              \
                            JS_END_MACRO

# define BEGIN_CASE(OP)     case OP: CHECK_RECORDER();
# define END_CASE(OP)       END_CASE_LEN(OP##_LENGTH)
# define END_CASE_LEN(n)    END_CASE_LENX(n)
# define END_CASE_LENX(n)   END_CASE_LEN##n

/*
 * To share the code for all len == 1 cases we use the specialized label with
 * code that falls through to advance_pc: .
 */
# define END_CASE_LEN1      goto advance_pc_by_one;
# define END_CASE_LEN2      len = 2; goto advance_pc;
# define END_CASE_LEN3      len = 3; goto advance_pc;
# define END_CASE_LEN4      len = 4; goto advance_pc;
# define END_CASE_LEN5      len = 5; goto advance_pc;
# define END_VARLEN_CASE    goto advance_pc;
# define ADD_EMPTY_CASE(OP) BEGIN_CASE(OP)
# define END_EMPTY_CASES    goto advance_pc_by_one;

#endif /* !JS_THREADED_INTERP */

    /* Check for too deep of a native thread stack. */
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    rt = cx->runtime;

    /* Set registerized frame pointer and derived script pointer. */
    fp = cx->fp;
    script = fp->script;
    JS_ASSERT(!script->isEmpty());
    JS_ASSERT(script->length > 1);

    /* Count of JS function calls that nest in this C js_Interpret frame. */
    inlineCallCount = 0;

    /*
     * Initialize the index segment register used by LOAD_ATOM and
     * GET_FULL_INDEX macros below. As a register we use a pointer based on
     * the atom map to turn frequently executed LOAD_ATOM into simple array
     * access. For less frequent object and regexp loads we have to recover
     * the segment from atoms pointer first.
     */
    atoms = script->atomMap.vector;

#define LOAD_ATOM(PCOFF)                                                      \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(fp->imacpc                                                  \
                  ? atoms == COMMON_ATOMS_START(&rt->atomState) &&            \
                    GET_INDEX(regs.pc + PCOFF) < js_common_atom_count         \
                  : (size_t)(atoms - script->atomMap.vector) <                \
                    (size_t)(script->atomMap.length -                         \
                             GET_INDEX(regs.pc + PCOFF)));                    \
        atom = atoms[GET_INDEX(regs.pc + PCOFF)];                             \
    JS_END_MACRO

#define GET_FULL_INDEX(PCOFF)                                                 \
    (atoms - script->atomMap.vector + GET_INDEX(regs.pc + PCOFF))

#define LOAD_OBJECT(PCOFF)                                                    \
    (obj = script->getObject(GET_FULL_INDEX(PCOFF)))

#define LOAD_FUNCTION(PCOFF)                                                  \
    (fun = script->getFunction(GET_FULL_INDEX(PCOFF)))

#ifdef JS_TRACER

#ifdef MOZ_TRACEVIS
#if JS_THREADED_INTERP
#define MONITOR_BRANCH_TRACEVIS                                               \
    JS_BEGIN_MACRO                                                            \
        if (jumpTable != interruptJumpTable)                                  \
            js_EnterTraceVisState(cx, S_RECORD, R_NONE);                      \
    JS_END_MACRO
#else /* !JS_THREADED_INTERP */
#define MONITOR_BRANCH_TRACEVIS                                               \
    JS_BEGIN_MACRO                                                            \
        js_EnterTraceVisState(cx, S_RECORD, R_NONE);                          \
    JS_END_MACRO
#endif
#else
#define MONITOR_BRANCH_TRACEVIS
#endif

#define RESTORE_INTERP_VARS()                                                 \
    JS_BEGIN_MACRO                                                            \
        fp = cx->fp;                                                          \
        script = fp->script;                                                  \
        atoms = FrameAtomBase(cx, fp);                                        \
        currentVersion = (JSVersion) script->version;                         \
        JS_ASSERT(fp->regs == &regs);                                         \
        if (cx->throwing)                                                     \
            goto error;                                                       \
    JS_END_MACRO

#define MONITOR_BRANCH(reason)                                                \
    JS_BEGIN_MACRO                                                            \
        if (TRACING_ENABLED(cx)) {                                            \
            if (js_MonitorLoopEdge(cx, inlineCallCount, reason)) {            \
                JS_ASSERT(TRACE_RECORDER(cx));                                \
                MONITOR_BRANCH_TRACEVIS;                                      \
                ENABLE_INTERRUPTS();                                          \
            }                                                                 \
            RESTORE_INTERP_VARS();                                            \
        }                                                                     \
    JS_END_MACRO

#else /* !JS_TRACER */

#define MONITOR_BRANCH(reason) ((void) 0)

#endif /* !JS_TRACER */

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
#define CHECK_BRANCH()                                                        \
    JS_BEGIN_MACRO                                                            \
        if (!JS_CHECK_OPERATION_LIMIT(cx))                                    \
            goto error;                                                       \
    JS_END_MACRO

#ifndef TRACE_RECORDER
#define TRACE_RECORDER(cx) (false)
#endif

#define BRANCH(n)                                                             \
    JS_BEGIN_MACRO                                                            \
        regs.pc += (n);                                                       \
        op = (JSOp) *regs.pc;                                                 \
        if ((n) <= 0) {                                                       \
            CHECK_BRANCH();                                                   \
            if (op == JSOP_NOP) {                                             \
                if (TRACE_RECORDER(cx)) {                                     \
                    MONITOR_BRANCH(Record_Branch);                           \
                    op = (JSOp) *regs.pc;                                     \
                } else {                                                      \
                    op = (JSOp) *++regs.pc;                                   \
                }                                                             \
            } else if (op == JSOP_TRACE) {                                    \
                MONITOR_BRANCH(Record_Branch);                               \
                op = (JSOp) *regs.pc;                                         \
            }                                                                 \
        }                                                                     \
        DO_OP();                                                              \
    JS_END_MACRO

    MUST_FLOW_THROUGH("exit");
    ++cx->interpLevel;

    /*
     * Optimized Get and SetVersion for proper script language versioning.
     *
     * If any native method or JSClass/JSObjectOps hook calls js_SetVersion
     * and changes cx->version, the effect will "stick" and we will stop
     * maintaining currentVersion.  This is relied upon by testsuites, for
     * the most part -- web browsers select version before compiling and not
     * at run-time.
     */
    currentVersion = (JSVersion) script->version;
    originalVersion = (JSVersion) cx->version;
    if (currentVersion != originalVersion)
        js_SetVersion(cx, currentVersion);

    /* Update the static-link display. */
    if (script->staticLevel < JS_DISPLAY_SIZE) {
        JSStackFrame **disp = &cx->display[script->staticLevel];
        fp->displaySave = *disp;
        *disp = fp;
    }

# define CHECK_INTERRUPT_HANDLER()                                            \
    JS_BEGIN_MACRO                                                            \
        if (cx->debugHooks->interruptHandler)                                 \
            ENABLE_INTERRUPTS();                                              \
    JS_END_MACRO

    /*
     * Load the debugger's interrupt hook here and after calling out to native
     * functions (but not to getters, setters, or other native hooks), so we do
     * not have to reload it each time through the interpreter loop -- we hope
     * the compiler can keep it in a register when it is non-null.
     */
    CHECK_INTERRUPT_HANDLER();

#if !JS_HAS_GENERATORS
    JS_ASSERT(!fp->regs);
#else
    /* Initialize the pc and sp registers unless we're resuming a generator. */
    if (JS_LIKELY(!fp->regs)) {
#endif
        ASSERT_NOT_THROWING(cx);
        regs.pc = script->code;
        regs.sp = StackBase(fp);
        fp->regs = &regs;
#if JS_HAS_GENERATORS
    } else {
        JSGenerator *gen;

        JS_ASSERT(fp->flags & JSFRAME_GENERATOR);
        gen = FRAME_TO_GENERATOR(fp);
        JS_ASSERT(fp->regs == &gen->savedRegs);
        regs = gen->savedRegs;
        fp->regs = &regs;
        JS_ASSERT((size_t) (regs.pc - script->code) <= script->length);
        JS_ASSERT((size_t) (regs.sp - StackBase(fp)) <= StackDepth(script));

        /*
         * To support generator_throw and to catch ignored exceptions,
         * fail if cx->throwing is set.
         */
        if (cx->throwing) {
#ifdef DEBUG_NOT_THROWING
            if (cx->exception != JSVAL_ARETURN) {
                printf("JS INTERPRETER CALLED WITH PENDING EXCEPTION %lx\n",
                       (unsigned long) cx->exception);
            }
#endif
            goto error;
        }
    }
#endif /* JS_HAS_GENERATORS */

#ifdef JS_TRACER
    /*
     * We cannot reenter the interpreter while recording; wait to abort until
     * after cx->fp->regs is set.
     */
    if (TRACE_RECORDER(cx))
        js_AbortRecording(cx, "attempt to reenter interpreter while recording");
#endif

    /*
     * It is important that "op" be initialized before calling DO_OP because
     * it is possible for "op" to be specially assigned during the normal
     * processing of an opcode while looping. We rely on DO_NEXT_OP to manage
     * "op" correctly in all other cases.
     */
    len = 0;
    DO_NEXT_OP(len);

#if JS_THREADED_INTERP
    /*
     * This is a loop, but it does not look like a loop. The loop-closing
     * jump is distributed throughout goto *jumpTable[op] inside of DO_OP.
     * When interrupts are enabled, jumpTable is set to interruptJumpTable
     * where all jumps point to the interrupt label. The latter, after
     * calling the interrupt handler, dispatches through normalJumpTable to
     * continue the normal bytecode processing.
     */

#else /* !JS_THREADED_INTERP */
    for (;;) {
      advance_pc_by_one:
        JS_ASSERT(js_CodeSpec[op].length == 1);
        len = 1;
      advance_pc:
        regs.pc += len;
        op = (JSOp) *regs.pc;

      do_op:
        CHECK_RECORDER();
        TRACE_OPCODE(op);
        switchOp = intN(op) | switchMask;
      do_switch:
        switch (switchOp) {
#endif

/********************** Here we include the operations ***********************/
#include "jsops.cpp"
/*****************************************************************************/

#if !JS_THREADED_INTERP
          default:
#endif
#ifndef JS_TRACER
        bad_opcode:
#endif
          {
            char numBuf[12];
            JS_snprintf(numBuf, sizeof numBuf, "%d", op);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_BYTECODE, numBuf);
            goto error;
          }

#if !JS_THREADED_INTERP
        } /* switch (op) */
    } /* for (;;) */
#endif /* !JS_THREADED_INTERP */

  error:
    if (fp->imacpc && cx->throwing) {
        // To keep things simple, we hard-code imacro exception handlers here.
        if (*fp->imacpc == JSOP_NEXTITER && js_ValueIsStopIteration(cx->exception)) {
            // pc may point to JSOP_DUP here due to bug 474854.
            JS_ASSERT(*regs.pc == JSOP_CALL || *regs.pc == JSOP_DUP || *regs.pc == JSOP_TRUE);
            cx->throwing = JS_FALSE;
            cx->exception = JSVAL_VOID;
            regs.sp[-1] = JSVAL_HOLE;
            PUSH(JSVAL_FALSE);
            goto end_imacro;
        }

        // Handle other exceptions as if they came from the imacro-calling pc.
        regs.pc = fp->imacpc;
        fp->imacpc = NULL;
        atoms = script->atomMap.vector;
    }

    JS_ASSERT((size_t)((fp->imacpc ? fp->imacpc : regs.pc) - script->code) < script->length);

#ifdef JS_TRACER
    /*
     * This abort could be weakened to permit tracing through exceptions that
     * are thrown and caught within a loop, with the co-operation of the tracer.
     * For now just bail on any sign of trouble.
     */
    if (TRACE_RECORDER(cx))
        js_AbortRecording(cx, "error or exception while recording");
#endif

    if (!cx->throwing) {
        /* This is an error, not a catchable exception, quit the frame ASAP. */
        ok = JS_FALSE;
    } else {
        JSTrapHandler handler;
        JSTryNote *tn, *tnlimit;
        uint32 offset;

        /* Call debugger throw hook if set. */
        handler = cx->debugHooks->throwHook;
        if (handler) {
            switch (handler(cx, script, regs.pc, &rval,
                            cx->debugHooks->throwHookData)) {
              case JSTRAP_ERROR:
                cx->throwing = JS_FALSE;
                goto error;
              case JSTRAP_RETURN:
                cx->throwing = JS_FALSE;
                fp->rval = rval;
                ok = JS_TRUE;
                goto forced_return;
              case JSTRAP_THROW:
                cx->exception = rval;
              case JSTRAP_CONTINUE:
              default:;
            }
            CHECK_INTERRUPT_HANDLER();
        }

        /*
         * Look for a try block in script that can catch this exception.
         */
        if (script->trynotesOffset == 0)
            goto no_catch;

        offset = (uint32)(regs.pc - script->main);
        tn = script->trynotes()->vector;
        tnlimit = tn + script->trynotes()->length;
        do {
            if (offset - tn->start >= tn->length)
                continue;

            /*
             * We have a note that covers the exception pc but we must check
             * whether the interpreter has already executed the corresponding
             * handler. This is possible when the executed bytecode
             * implements break or return from inside a for-in loop.
             *
             * In this case the emitter generates additional [enditer] and
             * [gosub] opcodes to close all outstanding iterators and execute
             * the finally blocks. If such an [enditer] throws an exception,
             * its pc can still be inside several nested for-in loops and
             * try-finally statements even if we have already closed the
             * corresponding iterators and invoked the finally blocks.
             *
             * To address this, we make [enditer] always decrease the stack
             * even when its implementation throws an exception. Thus already
             * executed [enditer] and [gosub] opcodes will have try notes
             * with the stack depth exceeding the current one and this
             * condition is what we use to filter them out.
             */
            if (tn->stackDepth > regs.sp - StackBase(fp))
                continue;

            /*
             * Set pc to the first bytecode after the the try note to point
             * to the beginning of catch or finally or to [enditer] closing
             * the for-in loop.
             */
            regs.pc = (script)->main + tn->start + tn->length;

            ok = js_UnwindScope(cx, fp, tn->stackDepth, JS_TRUE);
            JS_ASSERT(fp->regs->sp == StackBase(fp) + tn->stackDepth);
            if (!ok) {
                /*
                 * Restart the handler search with updated pc and stack depth
                 * to properly notify the debugger.
                 */
                goto error;
            }

            switch (tn->kind) {
              case JSTRY_CATCH:
                JS_ASSERT(js_GetOpcode(cx, fp->script, regs.pc) == JSOP_ENTERBLOCK);

#if JS_HAS_GENERATORS
                /* Catch cannot intercept the closing of a generator. */
                if (JS_UNLIKELY(cx->exception == JSVAL_ARETURN))
                    break;
#endif

                /*
                 * Don't clear cx->throwing to save cx->exception from GC
                 * until it is pushed to the stack via [exception] in the
                 * catch block.
                 */
                len = 0;
                DO_NEXT_OP(len);

              case JSTRY_FINALLY:
                /*
                 * Push (true, exception) pair for finally to indicate that
                 * [retsub] should rethrow the exception.
                 */
                PUSH(JSVAL_TRUE);
                PUSH(cx->exception);
                cx->throwing = JS_FALSE;
                len = 0;
                DO_NEXT_OP(len);

              case JSTRY_ITER:
                /*
                 * This is similar to JSOP_ENDITER in the interpreter loop,
                 * except the code now uses the stack slot normally used by
                 * JSOP_NEXTITER, namely regs.sp[-1] before the regs.sp -= 2
                 * adjustment and regs.sp[1] after, to save and restore the
                 * pending exception.
                 */
                JS_ASSERT(js_GetOpcode(cx, fp->script, regs.pc) == JSOP_ENDITER);
                regs.sp[-1] = cx->exception;
                cx->throwing = JS_FALSE;
                ok = js_CloseIterator(cx, regs.sp[-2]);
                regs.sp -= 2;
                if (!ok)
                    goto error;
                cx->throwing = JS_TRUE;
                cx->exception = regs.sp[1];
            }
        } while (++tn != tnlimit);

      no_catch:
        /*
         * Propagate the exception or error to the caller unless the exception
         * is an asynchronous return from a generator.
         */
        ok = JS_FALSE;
#if JS_HAS_GENERATORS
        if (JS_UNLIKELY(cx->throwing && cx->exception == JSVAL_ARETURN)) {
            cx->throwing = JS_FALSE;
            ok = JS_TRUE;
            fp->rval = JSVAL_VOID;
        }
#endif
    }

  forced_return:
    /*
     * Unwind the scope making sure that ok stays false even when UnwindScope
     * returns true.
     *
     * When a trap handler returns JSTRAP_RETURN, we jump here with ok set to
     * true bypassing any finally blocks.
     */
    ok &= js_UnwindScope(cx, fp, 0, ok || cx->throwing);
    JS_ASSERT(regs.sp == StackBase(fp));

#ifdef DEBUG
    cx->tracePrevPc = NULL;
#endif

    if (inlineCallCount)
        goto inline_return;

  exit:
    /*
     * At this point we are inevitably leaving an interpreted function or a
     * top-level script, and returning to one of:
     * (a) an "out of line" call made through js_Invoke;
     * (b) a js_Execute activation;
     * (c) a generator (SendToGenerator, jsiter.c).
     *
     * We must not be in an inline frame. The check above ensures that for the
     * error case and for a normal return, the code jumps directly to parent's
     * frame pc.
     */
    JS_ASSERT(inlineCallCount == 0);
    JS_ASSERT(fp->regs == &regs);
#ifdef JS_TRACER
    if (TRACE_RECORDER(cx))
        js_AbortRecording(cx, "recording out of js_Interpret");
#endif
#if JS_HAS_GENERATORS
    if (JS_UNLIKELY(fp->flags & JSFRAME_YIELDING)) {
        JSGenerator *gen;

        gen = FRAME_TO_GENERATOR(fp);
        gen->savedRegs = regs;
        gen->frame.regs = &gen->savedRegs;
    } else
#endif /* JS_HAS_GENERATORS */
    {
        JS_ASSERT(!fp->blockChain);
        JS_ASSERT(!js_IsActiveWithOrBlock(cx, fp->scopeChain, 0));
        fp->regs = NULL;
    }

    /* Undo the remaining effects committed on entry to js_Interpret. */
    if (script->staticLevel < JS_DISPLAY_SIZE)
        cx->display[script->staticLevel] = fp->displaySave;
    if (cx->version == currentVersion && currentVersion != originalVersion)
        js_SetVersion(cx, originalVersion);
    --cx->interpLevel;

    return ok;

  atom_not_defined:
    {
        const char *printable;

        printable = js_AtomToPrintableString(cx, atom);
        if (printable)
            js_ReportIsNotDefined(cx, printable);
        goto error;
    }
}

#endif /* !defined jsinvoke_cpp___ */

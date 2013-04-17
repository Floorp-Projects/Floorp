/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PodOperations.h"

#include "jscntxt.h"
#include "jsnum.h"
#include "jspropertycache.h"

#include "jsobjinlines.h"
#include "jsopcodeinlines.h"
#include "jspropertycacheinlines.h"

using namespace js;

using mozilla::PodArrayZero;

PropertyCacheEntry *
PropertyCache::fill(JSContext *cx, JSObject *obj, JSObject *pobj, Shape *shape)
{
    JS_ASSERT(this == &cx->propertyCache());
    JS_ASSERT(!cx->runtime->isHeapBusy());

    /*
     * Don't cache entries on indexed properties. Indexes can be added or
     * deleted from the dense elements of objects along the prototype chain
     * wihout any shape changes.
     */
    if (JSID_IS_INT(shape->propid()))
        return JS_NO_PROP_CACHE_FILL;

    /*
     * Check for overdeep scope and prototype chain. Because resolve, getter,
     * and setter hooks can change the prototype chain using JS_SetPrototype
     * after LookupPropertyWithFlags has returned, we calculate the protoIndex
     * here and not in LookupPropertyWithFlags.
     */

    JSObject *tmp = obj;
    unsigned protoIndex = 0;
    while (tmp != pobj) {
        /*
         * Don't cache entries across prototype lookups which can mutate in
         * arbitrary ways without a shape change.
         */
        if (tmp->hasUncacheableProto()) {
            PCMETER(noprotos++);
            return JS_NO_PROP_CACHE_FILL;
        }

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
        ++protoIndex;
    }

    typedef PropertyCacheEntry Entry;
    if (protoIndex > Entry::MaxProtoIndex) {
        PCMETER(longchains++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Optimize the cached vword based on our parameters and the current pc's
     * opcode format flags.
     */
    jsbytecode *pc;
    (void) cx->stack.currentScript(&pc);
    JSOp op = JSOp(*pc);
    const JSCodeSpec *cs = &js_CodeSpec[op];

    if ((cs->format & JOF_SET) && obj->watched())
        return JS_NO_PROP_CACHE_FILL;

    if (obj == pobj) {
        JS_ASSERT(protoIndex == 0);
    } else {
        JS_ASSERT(protoIndex != 0);
        JS_ASSERT((protoIndex == 1) == (obj->getProto() == pobj));

        if (protoIndex != 1) {
            /*
             * Make sure that a later shadowing assignment will enter
             * PurgeProtoChain and invalidate this entry, bug 479198.
             */
            if (!obj->isDelegate())
                return JS_NO_PROP_CACHE_FILL;
        }
    }

    PropertyCacheEntry *entry = &table[hash(pc, obj->lastProperty())];
    PCMETER(entry->vword.isNull() || recycles++);
    entry->assign(pc, obj->lastProperty(), pobj->lastProperty(), shape, protoIndex);

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

PropertyName *
PropertyCache::fullTest(JSContext *cx, jsbytecode *pc, JSObject **objp, JSObject **pobjp,
                        PropertyCacheEntry *entry)
{
    JSObject *obj, *pobj;
    RootedScript script(cx, cx->stack.currentScript());

    JS_ASSERT(this == &cx->propertyCache());
    JS_ASSERT(uint32_t(pc - script->code) < script->length);

    JSOp op = JSOp(*pc);

    obj = *objp;

    if (entry->kpc != pc) {
        PCMETER(kpcmisses++);

        PropertyName *name = GetNameFromBytecode(cx, script, pc, op);
#ifdef DEBUG_notme
        JSAutoByteString printable;
        fprintf(stderr,
                "id miss for %s from %s:%u"
                " (pc %u, kpc %u, kshape %p, shape %p)\n",
                js_AtomToPrintableString(cx, name, &printable),
                script->filename,
                js_PCToLineNumber(cx, script, pc),
                pc - script->code,
                entry->kpc - script->code,
                entry->kshape,
                obj->lastProperty());
                js_Disassemble1(cx, script, pc,
                                pc - script->code,
                                JS_FALSE, stderr);
#endif

        return name;
    }

    if (entry->kshape != obj->lastProperty()) {
        PCMETER(kshapemisses++);
        return GetNameFromBytecode(cx, script, pc, op);
    }

    /*
     * PropertyCache::test handles only the direct and immediate-prototype hit
     * cases. All others go here.
     */
    pobj = obj;

    uint8_t protoIndex = entry->protoIndex;
    while (protoIndex > 0) {
        JSObject *tmp = pobj->getProto();
        if (!tmp || !tmp->isNative())
            break;
        pobj = tmp;
        protoIndex--;
    }

    if (pobj->lastProperty() == entry->pshape) {
#ifdef DEBUG
        Rooted<PropertyName*> name(cx, GetNameFromBytecode(cx, script, pc, op));
        JS_ASSERT(pobj->nativeContains(cx, name));
#endif
        *pobjp = pobj;
        return NULL;
    }

    PCMETER(vcapmisses++);
    return GetNameFromBytecode(cx, script, pc, op);
}

#ifdef DEBUG
void
PropertyCache::assertEmpty()
{
    JS_ASSERT(empty);
    for (unsigned i = 0; i < SIZE; i++) {
        JS_ASSERT(!table[i].kpc);
        JS_ASSERT(!table[i].kshape);
        JS_ASSERT(!table[i].pshape);
        JS_ASSERT(!table[i].prop);
        JS_ASSERT(!table[i].protoIndex);
    }
}
#endif

void
PropertyCache::purge(JSRuntime *rt)
{
    if (empty) {
        assertEmpty();
        return;
    }

    PodArrayZero(table);
    empty = true;

#ifdef JS_PROPERTY_CACHE_METERING
  { static FILE *fp;
    if (!fp)
        fp = fopen("/tmp/propcache.stats", "w");
    if (fp) {
        fputs("Property cache stats for ", fp);
        fprintf(fp, "GC %lu\n", (unsigned long)rt->gcNumber);

# define P(mem) fprintf(fp, "%11s %10lu\n", #mem, (unsigned long)mem)
        P(fills);
        P(nofills);
        P(rofills);
        P(disfills);
        P(oddfills);
        P(add2dictfills);
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
PropertyCache::restore(PropertyCacheEntry *entry)
{
    PropertyCacheEntry *entry2;

    empty = false;

    entry2 = &table[hash(entry->kpc, entry->kshape)];
    *entry2 = *entry;
}

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

JS_REQUIRES_STACK PropertyCacheEntry *
PropertyCache::fill(JSContext *cx, JSObject *obj, uintN scopeIndex, JSObject *pobj,
                    const Shape *shape)
{
    JSOp op;
    const JSCodeSpec *cs;
    PropertyCacheEntry *entry;

    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));
    JS_ASSERT(!cx->runtime->gcRunning);

    /*
     * Check for fill from js_SetPropertyHelper where the setter removed shape
     * from pobj (via unwatch or delete, e.g.).
     */
    if (!pobj->nativeContains(cx, *shape)) {
        PCMETER(oddfills++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Check for overdeep scope and prototype chain. Because resolve, getter,
     * and setter hooks can change the prototype chain using JS_SetPrototype
     * after LookupPropertyWithFlags has returned, we calculate the protoIndex
     * here and not in LookupPropertyWithFlags.
     *
     * The scopeIndex can't be wrong. We require JS_SetParent calls to happen
     * before any running script might consult a parent-linked scope chain. If
     * this requirement is not satisfied, the fill in progress will never hit,
     * but scope shape tests ensure nothing malfunctions.
     */
    JS_ASSERT_IF(obj == pobj, scopeIndex == 0);

    JSObject *tmp = obj;
    for (uintN i = 0; i != scopeIndex; i++)
        tmp = tmp->internalScopeChain();

    uintN protoIndex = 0;
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

    if (scopeIndex > PCINDEX_SCOPEMASK || protoIndex > PCINDEX_PROTOMASK) {
        PCMETER(longchains++);
        return JS_NO_PROP_CACHE_FILL;
    }

    /*
     * Optimize the cached vword based on our parameters and the current pc's
     * opcode format flags.
     */
    jsbytecode *pc;
    JSScript *script = cx->stack.currentScript(&pc);
    op = js_GetOpcode(cx, script, pc);
    cs = &js_CodeSpec[op];

    if ((cs->format & JOF_SET) && obj->watched())
        return JS_NO_PROP_CACHE_FILL;

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
             */
            if (!obj->isDelegate())
                return JS_NO_PROP_CACHE_FILL;
        }
    }

    entry = &table[hash(pc, obj->lastProperty())];
    PCMETER(entry->vword.isNull() || recycles++);
    entry->assign(pc, obj->lastProperty(), pobj->lastProperty(), shape, scopeIndex, protoIndex);

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

    JSScript *script = cx->stack.currentScript();
    ptrdiff_t pcoff = (JOF_TYPE(cs.format) == JOF_SLOTATOM) ? SLOTNO_LEN : 0;
    JSAtom *atom;
    GET_ATOM_FROM_BYTECODE(script, pc, pcoff, atom);
    return atom;
}

JS_REQUIRES_STACK JSAtom *
PropertyCache::fullTest(JSContext *cx, jsbytecode *pc, JSObject **objp, JSObject **pobjp,
                        PropertyCacheEntry *entry)
{
    JSObject *obj, *pobj, *tmp;
    JSScript *script = cx->stack.currentScript();

    JS_ASSERT(this == &JS_PROPERTY_CACHE(cx));
    JS_ASSERT(uint32(pc - script->code) < script->length);

    JSOp op = js_GetOpcode(cx, script, pc);
    const JSCodeSpec &cs = js_CodeSpec[op];

    obj = *objp;
    uint32 vindex = entry->vindex;

    if (entry->kpc != pc) {
        PCMETER(kpcmisses++);

        JSAtom *atom = GetAtomFromBytecode(cx, pc, op, cs);
#ifdef DEBUG_notme
        JSAutoByteString printable;
        fprintf(stderr,
                "id miss for %s from %s:%u"
                " (pc %u, kpc %u, kshape %p, shape %p)\n",
                js_AtomToPrintableString(cx, atom, &printable),
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

        return atom;
    }

    if (entry->kshape != obj->lastProperty()) {
        PCMETER(kshapemisses++);
        return GetAtomFromBytecode(cx, pc, op, cs);
    }

    /*
     * PropertyCache::test handles only the direct and immediate-prototype hit
     * cases. All others go here.
     */
    pobj = obj;

    if (JOF_MODE(cs.format) == JOF_NAME) {
        while (vindex & (PCINDEX_SCOPEMASK << PCINDEX_PROTOBITS)) {
            tmp = pobj->scopeChain();
            if (!tmp || !tmp->isNative())
                break;
            pobj = tmp;
            vindex -= PCINDEX_PROTOSIZE;
        }

        *objp = pobj;
    }

    while (vindex & PCINDEX_PROTOMASK) {
        tmp = pobj->getProto();
        if (!tmp || !tmp->isNative())
            break;
        pobj = tmp;
        --vindex;
    }

    if (pobj->lastProperty() == entry->pshape) {
#ifdef DEBUG
        JSAtom *atom = GetAtomFromBytecode(cx, pc, op, cs);
        jsid id = ATOM_TO_JSID(atom);

        id = js_CheckForStringIndex(id);
        JS_ASSERT(pobj->nativeContains(cx, id));
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
        JS_ASSERT(!table[i].pshape);
        JS_ASSERT(!table[i].prop);
        JS_ASSERT(!table[i].vindex);
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

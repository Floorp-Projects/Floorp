/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * JS symbol tables.
 */
#include <new>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h"
#include "jsbit.h"
#include "jsclist.h"
#include "jsdhash.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"      /* for JS_ARGS_LENGTH_MAX */
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstracer.h"

#include "jsobjinlines.h"
#include "jsscopeinlines.h"

using namespace js;

uint32
js_GenerateShape(JSContext *cx, bool gcLocked)
{
    JSRuntime *rt;
    uint32 shape;

    rt = cx->runtime;
    shape = JS_ATOMIC_INCREMENT(&rt->shapeGen);
    JS_ASSERT(shape != 0);
    if (shape >= SHAPE_OVERFLOW_BIT) {
        /*
         * FIXME bug 440834: The shape id space has overflowed. Currently we
         * cope badly with this and schedule the GC on the every call. But
         * first we make sure that increments from other threads would not
         * have a chance to wrap around shapeGen to zero.
         */
        rt->shapeGen = SHAPE_OVERFLOW_BIT;
        shape = SHAPE_OVERFLOW_BIT;
        js_TriggerGC(cx, gcLocked);
    }
    return shape;
}

JSScope *
js_GetMutableScope(JSContext *cx, JSObject *obj)
{
    JSScope *scope = obj->scope();
    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, scope));
    if (!scope->isSharedEmpty())
        return scope;

    JSScope *newscope = JSScope::create(cx, scope->ops, obj->getClass(), obj, scope->shape);
    if (!newscope)
        return NULL;

    /* The newly allocated scope is single-threaded and, as such, is locked. */
    JS_ASSERT(CX_OWNS_SCOPE_TITLE(cx, newscope));
    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, newscope));
    obj->map = newscope;

    /*
     * Subtle dependency on objects that call js_EnsureReservedSlots either:
     * (a) never escaping anywhere an ad-hoc property could be set on them;
     * (b) having at least JSSLOT_FREE(obj->getClass()) >= JS_INITIAL_NSLOTS.
     * Note that (b) depends on fine-tuning of JS_INITIAL_NSLOTS (5).
     *
     * Block objects fall into (a); Argument, Call, and Function objects (flat
     * closures only) fall into (b). All of this goes away soon (FIXME 558451).
     */
    JS_ASSERT(newscope->freeslot >= JSSLOT_START(obj->getClass()) &&
              newscope->freeslot <= JSSLOT_FREE(obj->getClass()));
    newscope->freeslot = JSSLOT_FREE(obj->getClass());

    uint32 nslots = obj->numSlots();
    if (newscope->freeslot > nslots && !obj->allocSlots(cx, newscope->freeslot)) {
        newscope->destroy(cx);
        obj->map = scope;
        return NULL;
    }

    if (nslots > JS_INITIAL_NSLOTS && nslots > newscope->freeslot)
        newscope->freeslot = nslots;
#ifdef DEBUG
    if (newscope->freeslot < nslots)
        obj->setSlot(newscope->freeslot, JSVAL_VOID);
#endif

    JS_DROP_ALL_EMPTY_SCOPE_LOCKS(cx, scope);
    static_cast<JSEmptyScope *>(scope)->drop(cx);
    return newscope;
}

/*
 * JSScope uses multiplicative hashing, _a la_ jsdhash.[ch], but specialized
 * to minimize footprint.  But if a scope has fewer than SCOPE_HASH_THRESHOLD
 * entries, we use linear search and avoid allocating scope->table.
 */
#define SCOPE_HASH_THRESHOLD    6
#define MIN_SCOPE_SIZE_LOG2     4
#define MIN_SCOPE_SIZE          JS_BIT(MIN_SCOPE_SIZE_LOG2)
#define SCOPE_TABLE_NBYTES(n)   ((n) * sizeof(JSScopeProperty *))

void
JSScope::initMinimal(JSContext *cx, uint32 newShape)
{
    shape = newShape;
    emptyScope = NULL;
    hashShift = JS_DHASH_BITS - MIN_SCOPE_SIZE_LOG2;
    entryCount = removedCount = 0;
    table = NULL;
    lastProp = NULL;
}

#ifdef DEBUG
JS_FRIEND_DATA(JSScopeStats) js_scope_stats = {0};

# define METER(x)       JS_ATOMIC_INCREMENT(&js_scope_stats.x)
#else
# define METER(x)       /* nothing */
#endif

bool
JSScope::createTable(JSContext *cx, bool report)
{
    int sizeLog2;
    JSScopeProperty *sprop, **spp;

    JS_ASSERT(!table);
    JS_ASSERT(lastProp);

    if (entryCount > SCOPE_HASH_THRESHOLD) {
        /*
         * Either we're creating a table for a large scope that was populated
         * via property cache hit logic under JSOP_INITPROP, JSOP_SETNAME, or
         * JSOP_SETPROP; or else calloc failed at least once already. In any
         * event, let's try to grow, overallocating to hold at least twice the
         * current population.
         */
        sizeLog2 = JS_CeilingLog2(2 * entryCount);
        hashShift = JS_DHASH_BITS - sizeLog2;
    } else {
        JS_ASSERT(hashShift == JS_DHASH_BITS - MIN_SCOPE_SIZE_LOG2);
        sizeLog2 = MIN_SCOPE_SIZE_LOG2;
    }

    table = (JSScopeProperty **) js_calloc(JS_BIT(sizeLog2) * sizeof(JSScopeProperty *));
    if (!table) {
        if (report)
            JS_ReportOutOfMemory(cx);
        METER(tableAllocFails);
        return false;
    }
    cx->updateMallocCounter(JS_BIT(sizeLog2) * sizeof(JSScopeProperty *));

    hashShift = JS_DHASH_BITS - sizeLog2;
    for (sprop = lastProp; sprop; sprop = sprop->parent) {
        spp = search(sprop->id, true);
        SPROP_STORE_PRESERVING_COLLISION(spp, sprop);
    }
    return true;
}

JSScope *
JSScope::create(JSContext *cx, const JSObjectOps *ops, JSClass *clasp,
                JSObject *obj, uint32 shape)
{
    JS_ASSERT(ops->isNative());
    JS_ASSERT(obj);

    JSScope *scope = cx->create<JSScope>(ops, obj);
    if (!scope)
        return NULL;

    scope->freeslot = JSSLOT_START(clasp);
    scope->flags = cx->runtime->gcRegenShapesScopeFlag;
    scope->initMinimal(cx, shape);

#ifdef JS_THREADSAFE
    js_InitTitle(cx, &scope->title);
#endif
    JS_RUNTIME_METER(cx->runtime, liveScopes);
    JS_RUNTIME_METER(cx->runtime, totalScopes);
    return scope;
}

JSEmptyScope::JSEmptyScope(JSContext *cx, const JSObjectOps *ops,
                           JSClass *clasp)
    : JSScope(ops, NULL), clasp(clasp)
{
    /*
     * This scope holds a reference to the new empty scope. Our only caller,
     * getEmptyScope, also promises to incref on behalf of its caller.
     */
    nrefs = 2;
    freeslot = JSSLOT_START(clasp);
    flags = OWN_SHAPE | cx->runtime->gcRegenShapesScopeFlag;
    initMinimal(cx, js_GenerateShape(cx, false));

#ifdef JS_THREADSAFE
    js_InitTitle(cx, &title);
#endif
    JS_RUNTIME_METER(cx->runtime, liveScopes);
    JS_RUNTIME_METER(cx->runtime, totalScopes);
}

#ifdef DEBUG
# include "jsprf.h"
# define LIVE_SCOPE_METER(cx,expr) JS_LOCK_RUNTIME_VOID(cx->runtime,expr)
#else
# define LIVE_SCOPE_METER(cx,expr) /* nothing */
#endif

void
JSScope::destroy(JSContext *cx)
{
#ifdef JS_THREADSAFE
    js_FinishTitle(cx, &title);
#endif
    if (table)
        cx->free(table);

    /*
     * The scopes containing empty scopes are only destroyed from the GC
     * thread.
     */
    if (emptyScope)
        emptyScope->dropFromGC(cx);

    LIVE_SCOPE_METER(cx, cx->runtime->liveScopeProps -= entryCount);
    JS_RUNTIME_UNMETER(cx->runtime, liveScopes);
    cx->free(this);
}

/* static */
bool
JSScope::initRuntimeState(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

#define SCOPE(Name) rt->empty##Name##Scope
#define CLASP(Name) &js_##Name##Class

#define INIT_EMPTY_SCOPE(Name,NAME,ops)                                       \
    INIT_EMPTY_SCOPE_WITH_CLASS(Name, NAME, ops, CLASP(Name))

#define INIT_EMPTY_SCOPE_WITH_CLASS(Name,NAME,ops,clasp)                      \
    INIT_EMPTY_SCOPE_WITH_FREESLOT(Name, NAME, ops, clasp, JSSLOT_FREE(clasp))

#define INIT_EMPTY_SCOPE_WITH_FREESLOT(Name,NAME,ops,clasp,slot)              \
    SCOPE(Name) = cx->create<JSEmptyScope>(cx, ops, clasp);                   \
    if (!SCOPE(Name))                                                         \
        return false;                                                         \
    JS_ASSERT(SCOPE(Name)->shape == JSScope::EMPTY_##NAME##_SHAPE);           \
    JS_ASSERT(SCOPE(Name)->nrefs == 2);                                       \
    SCOPE(Name)->nrefs = 1;                                                   \
    SCOPE(Name)->freeslot = slot

    /*
     * NewArguments allocates dslots to have enough room for the argc of the
     * particular arguments object being created.
     *
     * Thus we fake freeslot in the shared empty scope for the many unmutated
     * arguments objects so that, until and unless a scope property is defined
     * on a particular arguments object, it can share the runtime-wide empty
     * scope with other arguments objects, whatever their initial argc values.
     *
     * This allows assertions that the arg slot being got or set by a fast path
     * is less than freeslot to succeed. As the shared emptyArgumentsScope is
     * never mutated, it's safe to pretend to have all the slots possible.
     *
     * Note how the fast paths in jsops.cpp for JSOP_LENGTH and JSOP_GETELEM
     * bypass resolution of scope properties for length and element indices on
     * arguments objects. This helps ensure that any arguments object needing
     * its own mutable scope (with unique shape) is a rare event.
     */
    INIT_EMPTY_SCOPE_WITH_FREESLOT(Arguments, ARGUMENTS, &js_ObjectOps, CLASP(Arguments),
                                   JS_INITIAL_NSLOTS + JS_ARGS_LENGTH_MAX);

    INIT_EMPTY_SCOPE(Block, BLOCK, &js_ObjectOps);

    /*
     * Initialize the shared scope for all empty Call objects so gets for args
     * and vars do not force the creation of a mutable scope for the particular
     * call object being accessed.
     *
     * See comment above for rt->emptyArgumentsScope->freeslot initialization.
     */
    INIT_EMPTY_SCOPE_WITH_FREESLOT(Call, CALL, &js_ObjectOps, CLASP(Call),
                                   JS_INITIAL_NSLOTS + JSFunction::MAX_ARGS_AND_VARS);

    /* A DeclEnv object holds the name binding for a named function expression. */
    INIT_EMPTY_SCOPE(DeclEnv, DECL_ENV, &js_ObjectOps);

    /* Non-escaping native enumerator objects share this empty scope. */
    INIT_EMPTY_SCOPE_WITH_CLASS(Enumerator, ENUMERATOR, &js_ObjectOps, &js_IteratorClass.base);

    /* Same drill for With objects. */
    INIT_EMPTY_SCOPE(With, WITH, &js_WithObjectOps);

#undef SCOPE
#undef CLASP
#undef INIT_EMPTY_SCOPE
#undef INIT_EMPTY_SCOPE_WITH_CLASS
#undef INIT_EMPTY_SCOPE_WITH_FREESLOT

    return true;
}

/* static */
void
JSScope::finishRuntimeState(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

#define FINISH_EMPTY_SCOPE(Name)                                              \
    if (rt->empty##Name##Scope) {                                             \
        rt->empty##Name##Scope->drop(cx);                                     \
        rt->empty##Name##Scope = NULL;                                        \
    }

    /* Mnemonic: ABCDEW */
    FINISH_EMPTY_SCOPE(Arguments);
    FINISH_EMPTY_SCOPE(Block);
    FINISH_EMPTY_SCOPE(Call);
    FINISH_EMPTY_SCOPE(DeclEnv);
    FINISH_EMPTY_SCOPE(Enumerator);
    FINISH_EMPTY_SCOPE(With);

#undef FINISH_EMPTY_SCOPE
}

JS_STATIC_ASSERT(sizeof(JSHashNumber) == 4);
JS_STATIC_ASSERT(sizeof(jsid) == JS_BYTES_PER_WORD);

#if JS_BYTES_PER_WORD == 4
# define HASH_ID(id) ((JSHashNumber)(id))
#elif JS_BYTES_PER_WORD == 8
# define HASH_ID(id) ((JSHashNumber)(id) ^ (JSHashNumber)((id) >> 32))
#else
# error "Unsupported configuration"
#endif

/*
 * Double hashing needs the second hash code to be relatively prime to table
 * size, so we simply make hash2 odd.  The inputs to multiplicative hash are
 * the golden ratio, expressed as a fixed-point 32 bit fraction, and the id
 * itself.
 */
#define SCOPE_HASH0(id)                 (HASH_ID(id) * JS_GOLDEN_RATIO)
#define SCOPE_HASH1(hash0,shift)        ((hash0) >> (shift))
#define SCOPE_HASH2(hash0,log2,shift)   ((((hash0) << (log2)) >> (shift)) | 1)

JSScopeProperty **
JSScope::searchTable(jsid id, bool adding)
{
    JSHashNumber hash0, hash1, hash2;
    int sizeLog2;
    JSScopeProperty *stored, *sprop, **spp, **firstRemoved;
    uint32 sizeMask;

    JS_ASSERT(table);
    JS_ASSERT(!JSVAL_IS_NULL(id));

    /* Compute the primary hash address. */
    METER(hashes);
    hash0 = SCOPE_HASH0(id);
    hash1 = SCOPE_HASH1(hash0, hashShift);
    spp = table + hash1;

    /* Miss: return space for a new entry. */
    stored = *spp;
    if (SPROP_IS_FREE(stored)) {
        METER(misses);
        return spp;
    }

    /* Hit: return entry. */
    sprop = SPROP_CLEAR_COLLISION(stored);
    if (sprop && sprop->id == id) {
        METER(hits);
        return spp;
    }

    /* Collision: double hash. */
    sizeLog2 = JS_DHASH_BITS - hashShift;
    hash2 = SCOPE_HASH2(hash0, sizeLog2, hashShift);
    sizeMask = JS_BITMASK(sizeLog2);

#ifdef DEBUG
    jsuword collision_flag = SPROP_COLLISION;
#endif

    /* Save the first removed entry pointer so we can recycle it if adding. */
    if (SPROP_IS_REMOVED(stored)) {
        firstRemoved = spp;
    } else {
        firstRemoved = NULL;
        if (adding && !SPROP_HAD_COLLISION(stored))
            SPROP_FLAG_COLLISION(spp, sprop);
#ifdef DEBUG
        collision_flag &= jsuword(*spp) & SPROP_COLLISION;
#endif
    }

    for (;;) {
        METER(steps);
        hash1 -= hash2;
        hash1 &= sizeMask;
        spp = table + hash1;

        stored = *spp;
        if (SPROP_IS_FREE(stored)) {
            METER(stepMisses);
            return (adding && firstRemoved) ? firstRemoved : spp;
        }

        sprop = SPROP_CLEAR_COLLISION(stored);
        if (sprop && sprop->id == id) {
            METER(stepHits);
            JS_ASSERT(collision_flag);
            return spp;
        }

        if (SPROP_IS_REMOVED(stored)) {
            if (!firstRemoved)
                firstRemoved = spp;
        } else {
            if (adding && !SPROP_HAD_COLLISION(stored))
                SPROP_FLAG_COLLISION(spp, sprop);
#ifdef DEBUG
            collision_flag &= jsuword(*spp) & SPROP_COLLISION;
#endif
        }
    }

    /* NOTREACHED */
    return NULL;
}

bool
JSScope::changeTable(JSContext *cx, int change)
{
    int oldlog2, newlog2;
    uint32 oldsize, newsize, nbytes;
    JSScopeProperty **newtable, **oldtable, **spp, **oldspp, *sprop;

    if (!table)
        return createTable(cx, true);

    /* Grow, shrink, or compress by changing this->table. */
    oldlog2 = JS_DHASH_BITS - hashShift;
    newlog2 = oldlog2 + change;
    oldsize = JS_BIT(oldlog2);
    newsize = JS_BIT(newlog2);
    nbytes = SCOPE_TABLE_NBYTES(newsize);
    newtable = (JSScopeProperty **) cx->calloc(nbytes);
    if (!newtable) {
        METER(tableAllocFails);
        return false;
    }

    /* Now that we have newtable allocated, update members. */
    hashShift = JS_DHASH_BITS - newlog2;
    removedCount = 0;
    oldtable = table;
    table = newtable;

    /* Treat the above calloc as a JS_malloc, to match CreateScopeTable. */
    cx->updateMallocCounter(nbytes);

    /* Copy only live entries, leaving removed and free ones behind. */
    for (oldspp = oldtable; oldsize != 0; oldspp++) {
        sprop = SPROP_FETCH(oldspp);
        if (sprop) {
            spp = search(sprop->id, true);
            JS_ASSERT(SPROP_IS_FREE(*spp));
            *spp = sprop;
        }
        oldsize--;
    }

    /* Finally, free the old table storage. */
    cx->free(oldtable);
    return true;
}

/*
 * Get or create a property-tree or dictionary child property of parent, which
 * must be lastProp if inDictionaryMode(), else parent must be one of lastProp
 * or lastProp->parent.
 */
JSScopeProperty *
JSScope::getChildProperty(JSContext *cx, JSScopeProperty *parent,
                          JSScopeProperty &child)
{
    JS_ASSERT(!JSVAL_IS_NULL(child.id));
    JS_ASSERT(!child.inDictionary());

    /*
     * Aliases share another property's slot, passed in the |slot| parameter.
     * Shared properties have no slot. Unshared properties that do not alias
     * another property's slot allocate a slot here, but may lose it due to a
     * JS_ClearScope call.
     */
    if (!child.isAlias()) {
        if (child.attrs & JSPROP_SHARED) {
            child.slot = SPROP_INVALID_SLOT;
        } else {
            /*
             * We may have set slot from a nearly-matching sprop, above.
             * If so, we're overwriting that nearly-matching sprop, so we
             * can reuse its slot -- we don't need to allocate a new one.
             * Similarly, we use a specific slot if provided by the caller.
             */
            if (child.slot == SPROP_INVALID_SLOT &&
                !js_AllocSlot(cx, object, &child.slot)) {
                return NULL;
            }
        }
    }

    if (inDictionaryMode()) {
        JS_ASSERT(parent == lastProp);
        if (newDictionaryProperty(cx, child, &lastProp)) {
            updateShape(cx);
            return lastProp;
        }
        return NULL;
    }

    JSScopeProperty *sprop = JS_PROPERTY_TREE(cx).getChild(cx, parent, shape, child);
    if (sprop) {
        JS_ASSERT(sprop->parent == parent);
        if (parent == lastProp) {
            extend(cx, sprop);
        } else {
            JS_ASSERT(parent == lastProp->parent);
            setLastProperty(sprop);
            updateShape(cx);
        }
    }
    return sprop;
}

#ifdef DEBUG_notbrendan
#define CHECK_ANCESTOR_LINE(scope, sparse)                                    \
    JS_BEGIN_MACRO                                                            \
        if ((scope)->table) CheckAncestorLine(scope);                         \
    JS_END_MACRO

static void
CheckAncestorLine(JSScope *scope)
{
    uint32 size;
    JSScopeProperty **spp, **start, **end, *ancestorLine, *sprop, *aprop;
    uint32 entryCount, ancestorCount;

    ancestorLine = scope->lastProperty();
    if (ancestorLine)
        JS_ASSERT(scope->hasProperty(ancestorLine));

    entryCount = 0;
    size = SCOPE_CAPACITY(scope);
    start = scope->table;
    for (spp = start, end = start + size; spp < end; spp++) {
        sprop = SPROP_FETCH(spp);
        if (sprop) {
            ++entryCount;
            for (aprop = ancestorLine; aprop; aprop = aprop->parent) {
                if (aprop == sprop)
                    break;
            }
            JS_ASSERT(aprop);
        }
    }
    JS_ASSERT(entryCount == scope->entryCount);

    ancestorCount = 0;
    for (sprop = ancestorLine; sprop; sprop = sprop->parent)
        ancestorCount++;
    JS_ASSERT(ancestorCount == scope->entryCount);
}
#else
#define CHECK_ANCESTOR_LINE(scope, sparse) /* nothing */
#endif

void
JSScope::reportReadOnlyScope(JSContext *cx)
{
    JSString *str;
    const char *bytes;

    str = js_ValueToString(cx, OBJECT_TO_JSVAL(object));
    if (!str)
        return;
    bytes = js_GetStringBytes(cx, str);
    if (!bytes)
        return;
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_READ_ONLY, bytes);
}

void
JSScope::generateOwnShape(JSContext *cx)
{
#ifdef JS_TRACER
    if (object) {
         LeaveTraceIfGlobalObject(cx, object);

        /*
         * The JIT must have arranged to re-guard after any unpredictable shape
         * change, so if we are on trace here, we should already be prepared to
         * bail off trace.
         */
        JS_ASSERT_IF(JS_ON_TRACE(cx), cx->bailExit);

        /*
         * If we are recording, here is where we forget already-guarded shapes.
         * Any subsequent property operation upon object on the trace currently
         * being recorded will re-guard (and re-memoize).
         */
        TraceMonitor *tm = &JS_TRACE_MONITOR(cx);
        if (TraceRecorder *tr = tm->recorder)
            tr->forgetGuardedShapesForObject(object);
    }
#endif

    shape = js_GenerateShape(cx, false);
    setOwnShape();
}

JSScopeProperty *
JSScope::newDictionaryProperty(JSContext *cx, const JSScopeProperty &child,
                               JSScopeProperty **childp)
{
    JSScopeProperty *dprop = JS_PROPERTY_TREE(cx).newScopeProperty(cx);
    if (!dprop)
        return NULL;

    new (dprop) JSScopeProperty(child.id, child.rawGetter, child.rawSetter, child.slot,
                                child.attrs, child.flags | JSScopeProperty::IN_DICTIONARY,
                                child.shortid);
    dprop->shape = js_GenerateShape(cx, false);

    dprop->childp = NULL;
    insertDictionaryProperty(dprop, childp);
    updateFlags(dprop);
    return dprop;
}

bool
JSScope::toDictionaryMode(JSContext *cx, JSScopeProperty *&aprop)
{
    JS_ASSERT(!inDictionaryMode());

    JSScopeProperty **oldTable = table;
    uint32 saveRemovedCount = removedCount;
    if (oldTable) {
        int sizeLog2 = JS_DHASH_BITS - hashShift;
        JSScopeProperty **newTable = (JSScopeProperty **)
            js_calloc(JS_BIT(sizeLog2) * sizeof(JSScopeProperty *));

        if (!newTable) {
            JS_ReportOutOfMemory(cx);
            METER(toDictFails);
            return false;
        }
        table = newTable;
        removedCount = 0;
    }

    /*
     * We are committed from here on. If we fail due to OOM in the loop below,
     * we'll restore saveEntryCount, oldTable, oldLastProp.
     */
    JSScopeProperty *oldLastProp = lastProp;
    lastProp = NULL;

    /*
     * Clear entryCount because JSScope::insertDictionaryProperty called from
     * JSScope::newDictionaryProperty bumps it.
     */
    uint32 saveEntryCount = entryCount;
    entryCount = 0;

    for (JSScopeProperty *sprop = oldLastProp, **childp = &lastProp; sprop; sprop = sprop->parent) {
        JSScopeProperty *dprop = newDictionaryProperty(cx, *sprop, childp);
        if (!dprop) {
            entryCount = saveEntryCount;
            removedCount = saveRemovedCount;
            if (table)
                js_free(table);
            table = oldTable;
            lastProp = oldLastProp;
            METER(toDictFails);
            return false;
        }

        if (table) {
            JSScopeProperty **spp = search(dprop->id, true);
            JS_ASSERT(!SPROP_FETCH(spp));
            SPROP_STORE_PRESERVING_COLLISION(spp, dprop);
        }

        if (aprop == sprop)
            aprop = dprop;
        childp = &dprop->parent;
    }

    if (oldTable)
        js_free(oldTable);
    setDictionaryMode();
    clearOwnShape();

    if (lastProp) {
        /*
         * This scope may get OWN_SHAPE set again, but for now its shape must
         * be the shape of its lastProp. If it is empty, its initial shape is
         * still valid. See JSScope::updateShape's definition in jsscope.h.
         */
        shape = lastProp->shape;
    }
    return true;
}

JSScopeProperty *
JSScope::addProperty(JSContext *cx, jsid id,
                     JSPropertyOp getter, JSPropertyOp setter,
                     uint32 slot, uintN attrs,
                     uintN flags, intN shortid)
{
    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, this));
    CHECK_ANCESTOR_LINE(this, true);

    JS_ASSERT(!JSVAL_IS_NULL(id));
    JS_ASSERT_IF(!cx->runtime->gcRegenShapes,
                 hasRegenFlag(cx->runtime->gcRegenShapesScopeFlag));

    /*
     * You can't add properties to a sealed scope. But note well that you can
     * change property attributes in a sealed scope, even though that replaces
     * a JSScopeProperty * in the scope's hash table -- but no id is added, so
     * the scope remains sealed.
     */
    if (sealed()) {
        reportReadOnlyScope(cx);
        return NULL;
    }

    /* Search for id with adding = true in order to claim its entry. */
    JSScopeProperty **spp = search(id, true);
    JS_ASSERT(!SPROP_FETCH(spp));
    return addPropertyHelper(cx, id, getter, setter, slot, attrs, flags, shortid, spp);
}

/*
 * Normalize stub getter and setter values for faster is-stub testing in the
 * SPROP_CALL_[GS]ETTER macros.
 */
static inline bool
NormalizeGetterAndSetter(JSContext *cx, JSScope *scope,
                         jsid id, uintN attrs, uintN flags,
                         JSPropertyOp &getter,
                         JSPropertyOp &setter)
{
    if (setter == JS_PropertyStub) {
        JS_ASSERT(!(attrs & JSPROP_SETTER));
        setter = NULL;
    }
    if (flags & JSScopeProperty::METHOD) {
        /* Here, getter is the method, a function object reference. */
        JS_ASSERT(getter);
        JS_ASSERT(!setter || setter == js_watch_set);
        JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
    } else {
        if (getter == JS_PropertyStub) {
            JS_ASSERT(!(attrs & JSPROP_GETTER));
            getter = NULL;
        }
    }

    /*
     * Check for a watchpoint on a deleted property; if one exists, change
     * setter to js_watch_set or js_watch_set_wrapper.
     * XXXbe this could get expensive with lots of watchpoints...
     */
    if (!JS_CLIST_IS_EMPTY(&cx->runtime->watchPointList) &&
        js_FindWatchPoint(cx->runtime, scope, id)) {
        setter = js_WrapWatchedSetter(cx, id, attrs, setter);
        if (!setter) {
            METER(wrapWatchFails);
            return false;
        }
    }
    return true;
}

JSScopeProperty *
JSScope::addPropertyHelper(JSContext *cx, jsid id,
                           JSPropertyOp getter, JSPropertyOp setter,
                           uint32 slot, uintN attrs,
                           uintN flags, intN shortid,
                           JSScopeProperty **spp)
{
    NormalizeGetterAndSetter(cx, this, id, attrs, flags, getter, setter);

    /* Check whether we need to grow, if the load factor is >= .75. */
    uint32 size = SCOPE_CAPACITY(this);
    if (entryCount + removedCount >= size - (size >> 2)) {
        int change = removedCount < size >> 2;
        if (!change)
            METER(compresses);
        else
            METER(grows);
        if (!changeTable(cx, change) && entryCount + removedCount == size - 1)
            return NULL;
        spp = search(id, true);
        JS_ASSERT(!SPROP_FETCH(spp));
    }

    /* Find or create a property tree node labeled by our arguments. */
    JSScopeProperty *sprop;
    {
        JSScopeProperty child(id, getter, setter, slot, attrs, flags, shortid);
        sprop = getChildProperty(cx, lastProp, child);
    }

    if (sprop) {
        /* Store the tree node pointer in the table entry for id. */
        if (table)
            SPROP_STORE_PRESERVING_COLLISION(spp, sprop);
        CHECK_ANCESTOR_LINE(this, false);
#ifdef DEBUG
        LIVE_SCOPE_METER(cx, ++cx->runtime->liveScopeProps);
        JS_RUNTIME_METER(cx->runtime, totalScopeProps);
#endif

        /*
         * If we reach the hashing threshold, try to allocate this->table.
         * If we can't (a rare event, preceded by swapping to death on most
         * modern OSes), stick with linear search rather than whining about
         * this little set-back.  Therefore we must test !this->table and
         * this->entryCount >= SCOPE_HASH_THRESHOLD, not merely whether the
         * entry count just reached the threshold.
         */
        if (!table && entryCount >= SCOPE_HASH_THRESHOLD)
            (void) createTable(cx, false);

        METER(adds);
        return sprop;
    }

    METER(addFails);
    return NULL;
}

JSScopeProperty *
JSScope::putProperty(JSContext *cx, jsid id,
                     JSPropertyOp getter, JSPropertyOp setter,
                     uint32 slot, uintN attrs,
                     uintN flags, intN shortid)
{
    JSScopeProperty **spp, *sprop, *overwriting;

    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, this));
    CHECK_ANCESTOR_LINE(this, true);

    JS_ASSERT(!JSVAL_IS_NULL(id));

    JS_ASSERT_IF(!cx->runtime->gcRegenShapes,
                 hasRegenFlag(cx->runtime->gcRegenShapesScopeFlag));

    if (sealed()) {
        reportReadOnlyScope(cx);
        return NULL;
    }

    /* Search for id in order to claim its entry if table has been allocated. */
    spp = search(id, true);
    sprop = SPROP_FETCH(spp);
    if (!sprop)
        return addPropertyHelper(cx, id, getter, setter, slot, attrs, flags, shortid, spp);

    /* Property exists: JSScope::search must have returned a valid *spp. */
    JS_ASSERT(!SPROP_IS_REMOVED(*spp));
    overwriting = sprop;

    NormalizeGetterAndSetter(cx, this, id, attrs, flags, getter, setter);

    /*
     * If all property members match, this is a redundant add and we can
     * return early.  If the caller wants to allocate a slot, but doesn't
     * care which slot, copy sprop->slot into slot so we can match sprop,
     * if all other members match.
     */
    if (!(attrs & JSPROP_SHARED) &&
        slot == SPROP_INVALID_SLOT &&
        SPROP_HAS_VALID_SLOT(sprop, this)) {
        slot = sprop->slot;
    }
    if (sprop->matchesParamsAfterId(getter, setter, slot, attrs, flags, shortid)) {
        METER(redundantPuts);
        return sprop;
    }

    /*
     * If we are clearing sprop to force the existing property that it
     * describes to be overwritten, then we have to unlink sprop from the
     * ancestor line at this->lastProp.
     *
     * If sprop is not lastProp and this scope is not in dictionary mode,
     * we must switch to dictionary mode so we can unlink the non-terminal
     * sprop without breaking anyone sharing the property lineage via the
     * runtime's property tree.
     */
    if (sprop == lastProp && !inDictionaryMode()) {
        removeLastProperty();
    } else {
        if (!inDictionaryMode()) {
            if (!toDictionaryMode(cx, sprop))
                return NULL;
            spp = search(id, false);
        }
        removeDictionaryProperty(sprop);
    }

    /*
     * If we fail later on trying to find or create a new sprop, we will
     * restore *spp from |overwriting|. Note that we don't bother to keep
     * this->removedCount in sync, because we will fix up both *spp and
     * this->entryCount shortly.
     */
    if (table)
        SPROP_STORE_PRESERVING_COLLISION(spp, NULL);
    CHECK_ANCESTOR_LINE(this, true);

    {
        /* Find or create a property tree node labeled by our arguments. */
        JSScopeProperty child(id, getter, setter, slot, attrs, flags, shortid);
        sprop = getChildProperty(cx, lastProp, child);
    }

    if (sprop) {
        CHECK_ANCESTOR_LINE(this, false);

        if (table) {
            /* Store the tree node pointer in the table entry for id. */
            SPROP_STORE_PRESERVING_COLLISION(spp, sprop);
        } else if (entryCount >= SCOPE_HASH_THRESHOLD) {
            /* See comment in JSScope::addPropertyHelper about ignoring OOM here. */
            (void) createTable(cx, false);
        }

        METER(puts);
        return sprop;
    }

    if (table)
        SPROP_STORE_PRESERVING_COLLISION(spp, overwriting);
    ++entryCount;
    CHECK_ANCESTOR_LINE(this, true);
    METER(putFails);
    return NULL;
}

JSScopeProperty *
JSScope::changeProperty(JSContext *cx, JSScopeProperty *sprop,
                        uintN attrs, uintN mask,
                        JSPropertyOp getter, JSPropertyOp setter)
{
    JSScopeProperty *newsprop;

    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, this));
    CHECK_ANCESTOR_LINE(this, true);

    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    JS_ASSERT(hasProperty(sprop));

    attrs |= sprop->attrs & mask;

    /* Allow only shared (slot-less) => unshared (slot-full) transition. */
    JS_ASSERT(!((attrs ^ sprop->attrs) & JSPROP_SHARED) ||
              !(attrs & JSPROP_SHARED));

    /* Don't allow method properties to be changed to have a getter. */
    JS_ASSERT_IF(getter != sprop->rawGetter, !sprop->isMethod());

    if (getter == JS_PropertyStub)
        getter = NULL;
    if (setter == JS_PropertyStub)
        setter = NULL;
    if (sprop->attrs == attrs && sprop->getter() == getter && sprop->setter() == setter)
        return sprop;

    JSScopeProperty child(sprop->id, getter, setter, sprop->slot, attrs, sprop->flags,
                          sprop->shortid);
    if (inDictionaryMode()) {
        removeDictionaryProperty(sprop);
        newsprop = newDictionaryProperty(cx, child, &lastProp);
        if (newsprop) {
            if (table) {
                JSScopeProperty **spp = search(sprop->id, false);
                SPROP_STORE_PRESERVING_COLLISION(spp, newsprop);
            }
            updateShape(cx);
        }
    } else if (sprop == lastProp) {
        newsprop = getChildProperty(cx, sprop->parent, child);
        if (newsprop) {
            if (table) {
                JSScopeProperty **spp = search(sprop->id, false);
                JS_ASSERT(SPROP_FETCH(spp) == sprop);
                SPROP_STORE_PRESERVING_COLLISION(spp, newsprop);
            }
            CHECK_ANCESTOR_LINE(this, true);
        }
    } else {
        /*
         * Let JSScope::putProperty handle this |overwriting| case, including
         * the conservation of sprop->slot (if it's valid). We must not call
         * JSScope::removeProperty because it will free a valid sprop->slot and
         * JSScope::putProperty won't re-allocate it.
         */
        newsprop = putProperty(cx, child.id, child.rawGetter, child.rawSetter, child.slot,
                               child.attrs, child.flags, child.shortid);
    }

#ifdef DEBUG
    if (newsprop)
        METER(changes);
    else
        METER(changeFails);
#endif
    return newsprop;
}

bool
JSScope::removeProperty(JSContext *cx, jsid id)
{
    JSScopeProperty **spp, *sprop;
    uint32 size;

    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, this));
    CHECK_ANCESTOR_LINE(this, true);
    if (sealed()) {
        reportReadOnlyScope(cx);
        return false;
    }

    spp = search(id, false);
    sprop = SPROP_CLEAR_COLLISION(*spp);
    if (!sprop) {
        METER(uselessRemoves);
        return true;
    }

    /* If sprop is not the last property added, switch to dictionary mode. */
    if (sprop != lastProp) {
        if (!inDictionaryMode()) {
            if (!toDictionaryMode(cx, sprop))
                return false;
            spp = search(id, false);
        }
        JS_ASSERT(SPROP_FETCH(spp) == sprop);
    }

    /* First, if sprop is unshared and not cleared, free its slot number. */
    if (SPROP_HAS_VALID_SLOT(sprop, this)) {
        js_FreeSlot(cx, object, sprop->slot);
        JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    }

    /* Next, remove id by setting its entry to a removed or free sentinel. */
    if (SPROP_HAD_COLLISION(*spp)) {
        JS_ASSERT(table);
        *spp = SPROP_REMOVED;
        ++removedCount;
    } else {
        METER(removeFrees);
        if (table) {
            *spp = NULL;
#ifdef DEBUG
            /*
             * Check the consistency of the table but limit the number of
             * checks not to alter significantly the complexity of the delete
             * in debug builds, see bug 534493.
             */
            JSScopeProperty *aprop = lastProp;
            for (unsigned n = 50; aprop && n != 0; aprop = aprop->parent, --n)
                JS_ASSERT_IF(aprop != sprop, hasProperty(aprop));
#endif
        }
    }
    LIVE_SCOPE_METER(cx, --cx->runtime->liveScopeProps);

    if (inDictionaryMode()) {
        /*
         * Remove sprop from its scope-owned doubly linked list, setting this
         * scope's OWN_SHAPE flag first if sprop is lastProp so updateShape(cx)
         * after this if-else will generate a fresh shape for this scope.
         */
        if (sprop != lastProp)
            setOwnShape();
        removeDictionaryProperty(sprop);
    } else {
        JS_ASSERT(sprop == lastProp);
        removeLastProperty();
    }
    updateShape(cx);
    CHECK_ANCESTOR_LINE(this, true);

    /* Last, consider shrinking this->table if its load factor is <= .25. */
    size = SCOPE_CAPACITY(this);
    if (size > MIN_SCOPE_SIZE && entryCount <= size >> 2) {
        METER(shrinks);
        (void) changeTable(cx, -1);
    }

    METER(removes);
    return true;
}

void
JSScope::clear(JSContext *cx)
{
    CHECK_ANCESTOR_LINE(this, true);
    LIVE_SCOPE_METER(cx, cx->runtime->liveScopeProps -= entryCount);

    if (table)
        js_free(table);
    clearDictionaryMode();
    clearOwnShape();
    LeaveTraceIfGlobalObject(cx, object);

    JSClass *clasp = object->getClass();
    JSObject *proto = object->getProto();
    JSEmptyScope *emptyScope;
    uint32 newShape;
    if (proto &&
        proto->isNative() &&
        (emptyScope = proto->scope()->emptyScope) &&
        emptyScope->clasp == clasp) {
        newShape = emptyScope->shape;
    } else {
        newShape = js_GenerateShape(cx, false);
    }
    initMinimal(cx, newShape);

    JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
}

void
JSScope::deletingShapeChange(JSContext *cx, JSScopeProperty *sprop)
{
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    generateOwnShape(cx);
}

bool
JSScope::methodShapeChange(JSContext *cx, JSScopeProperty *sprop)
{
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    if (sprop->isMethod()) {
#ifdef DEBUG
        jsval prev = object->lockedGetSlot(sprop->slot);
        JS_ASSERT(sprop->methodValue() == prev);
        JS_ASSERT(hasMethodBarrier());
        JS_ASSERT(object->getClass() == &js_ObjectClass);
        JS_ASSERT(!sprop->rawSetter || sprop->rawSetter == js_watch_set);
#endif

        /*
         * Pass null to make a stub getter, but pass along sprop->setter to
         * preserve watchpoints. Clear JSScopeProperty::METHOD from flags as we
         * are despecializing from a method memoized in the property tree to a
         * plain old function-valued property.
         */
        sprop = putProperty(cx, sprop->id, NULL, sprop->rawSetter, sprop->slot,
                            sprop->attrs,
                            sprop->getFlags() & ~JSScopeProperty::METHOD,
                            sprop->shortid);
        if (!sprop)
            return false;
    }

    generateOwnShape(cx);
    return true;
}

bool
JSScope::methodShapeChange(JSContext *cx, uint32 slot)
{
    if (!hasMethodBarrier()) {
        generateOwnShape(cx);
    } else {
        for (JSScopeProperty *sprop = lastProp; sprop; sprop = sprop->parent) {
            JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
            if (sprop->slot == slot)
                return methodShapeChange(cx, sprop);
        }
    }
    return true;
}

void
JSScope::protoShapeChange(JSContext *cx)
{
    generateOwnShape(cx);
}

void
JSScope::shadowingShapeChange(JSContext *cx, JSScopeProperty *sprop)
{
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    generateOwnShape(cx);
}

bool
JSScope::globalObjectOwnShapeChange(JSContext *cx)
{
    generateOwnShape(cx);
    return !js_IsPropertyCacheDisabled(cx);
}

void
js_TraceId(JSTracer *trc, jsid id)
{
    jsval v;

    v = ID_TO_VALUE(id);
    JS_CALL_VALUE_TRACER(trc, v, "id");
}

#ifdef DEBUG
static void
PrintPropertyGetterOrSetter(JSTracer *trc, char *buf, size_t bufsize)
{
    JSScopeProperty *sprop;
    jsid id;
    size_t n;
    const char *name;

    JS_ASSERT(trc->debugPrinter == PrintPropertyGetterOrSetter);
    sprop = (JSScopeProperty *)trc->debugPrintArg;
    id = sprop->id;
    JS_ASSERT(!JSVAL_IS_NULL(id));
    name = trc->debugPrintIndex ? js_setter_str : js_getter_str;

    if (JSID_IS_ATOM(id)) {
        n = js_PutEscapedString(buf, bufsize - 1,
                                ATOM_TO_STRING(JSID_TO_ATOM(id)), 0);
        if (n < bufsize - 1)
            JS_snprintf(buf + n, bufsize - n, " %s", name);
    } else if (JSID_IS_INT(sprop->id)) {
        JS_snprintf(buf, bufsize, "%d %s", JSID_TO_INT(id), name);
    } else {
        JS_snprintf(buf, bufsize, "<object> %s", name);
    }
}

static void
PrintPropertyMethod(JSTracer *trc, char *buf, size_t bufsize)
{
    JSScopeProperty *sprop;
    jsid id;
    size_t n;

    JS_ASSERT(trc->debugPrinter == PrintPropertyMethod);
    sprop = (JSScopeProperty *)trc->debugPrintArg;
    id = sprop->id;
    JS_ASSERT(!JSVAL_IS_NULL(id));

    JS_ASSERT(JSID_IS_ATOM(id));
    n = js_PutEscapedString(buf, bufsize - 1, ATOM_TO_STRING(JSID_TO_ATOM(id)), 0);
    if (n < bufsize - 1)
        JS_snprintf(buf + n, bufsize - n, " method");
}
#endif

void
JSScopeProperty::trace(JSTracer *trc)
{
    if (IS_GC_MARKING_TRACER(trc))
        mark();
    js_TraceId(trc, id);

    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        if ((attrs & JSPROP_GETTER) && rawGetter) {
            JS_SET_TRACING_DETAILS(trc, PrintPropertyGetterOrSetter, this, 0);
            js_CallGCMarker(trc, getterObject(), JSTRACE_OBJECT);
        }
        if ((attrs & JSPROP_SETTER) && rawSetter) {
            JS_SET_TRACING_DETAILS(trc, PrintPropertyGetterOrSetter, this, 1);
            js_CallGCMarker(trc, setterObject(), JSTRACE_OBJECT);
        }
    }

    if (isMethod()) {
        JS_SET_TRACING_DETAILS(trc, PrintPropertyMethod, this, 0);
        js_CallGCMarker(trc, methodObject(), JSTRACE_OBJECT);
    }
}

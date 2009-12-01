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
#include "jslock.h"
#include "jsnum.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstracer.h"

#include "jsscopeinlines.h"

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
    JSScope *scope, *newscope;
    JSClass *clasp;
    uint32 freeslot;

    scope = OBJ_SCOPE(obj);
    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, scope));
    if (scope->owned())
        return scope;

    /*
     * Compile-time block objects each have their own scope, created at
     * birth, and runtime clone of a block objects are never mutated.
     */
    JS_ASSERT(STOBJ_GET_CLASS(obj) != &js_BlockClass);
    newscope = JSScope::create(cx, scope->ops, obj->getClass(), obj, scope->shape);
    if (!newscope)
        return NULL;
    JS_LOCK_SCOPE(cx, newscope);
    obj->map = newscope;

    JS_ASSERT(newscope->freeslot == JSSLOT_FREE(STOBJ_GET_CLASS(obj)));
    clasp = STOBJ_GET_CLASS(obj);
    if (clasp->reserveSlots) {
        freeslot = JSSLOT_FREE(clasp) + clasp->reserveSlots(cx, obj);
        if (freeslot > STOBJ_NSLOTS(obj))
            freeslot = STOBJ_NSLOTS(obj);
        if (newscope->freeslot < freeslot)
            newscope->freeslot = freeslot;
    }
    JS_TRANSFER_SCOPE_LOCK(cx, scope, newscope);
    JS_ATOMIC_DECREMENT(&scope->nrefs);
    if (scope->nrefs == 0)
        JSScope::destroy(cx, scope);
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
    JS_ASSERT(OPS_IS_NATIVE(ops));
    JS_ASSERT(obj);

    JSScope *scope = cx->create<JSScope>(ops, obj);
    if (!scope)
        return NULL;

    scope->nrefs = 1;
    scope->freeslot = JSSLOT_FREE(clasp);
    scope->flags = cx->runtime->gcRegenShapesScopeFlag;
    scope->initMinimal(cx, shape);

#ifdef JS_THREADSAFE
    js_InitTitle(cx, &scope->title);
#endif
    JS_RUNTIME_METER(cx->runtime, liveScopes);
    JS_RUNTIME_METER(cx->runtime, totalScopes);
    return scope;
}

JSEmptyScope *
JSScope::createEmptyScope(JSContext *cx, JSClass *clasp)
{
    JS_ASSERT(!emptyScope);

    JSEmptyScope *scope = cx->create<JSEmptyScope>(ops, clasp);
    if (!scope)
        return NULL;

    /*
     * This scope holds a reference to the new empty scope. Our only caller,
     * getEmptyScope, also promises to incref on behalf of its caller.
     */
    scope->nrefs = 2;
    scope->freeslot = JSSLOT_FREE(clasp);
    scope->flags = OWN_SHAPE | cx->runtime->gcRegenShapesScopeFlag;
    scope->initMinimal(cx, js_GenerateShape(cx, false));

#ifdef JS_THREADSAFE
    js_InitTitle(cx, &scope->title);
#endif
    JS_RUNTIME_METER(cx->runtime, liveScopes);
    JS_RUNTIME_METER(cx->runtime, totalScopes);
    emptyScope = scope;
    return scope;
}

#ifdef DEBUG
# include "jsprf.h"
# define LIVE_SCOPE_METER(cx,expr) JS_LOCK_RUNTIME_VOID(cx->runtime,expr)
#else
# define LIVE_SCOPE_METER(cx,expr) /* nothing */
#endif

void
JSScope::destroy(JSContext *cx, JSScope *scope)
{
#ifdef JS_THREADSAFE
    js_FinishTitle(cx, &scope->title);
#endif
    if (scope->table)
        cx->free(scope->table);
    if (scope->emptyScope)
        scope->emptyScope->drop(cx, NULL);

    LIVE_SCOPE_METER(cx, cx->runtime->liveScopeProps -= scope->entryCount);
    JS_RUNTIME_UNMETER(cx->runtime, liveScopes);
    cx->free(scope);
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

    /* Save the first removed entry pointer so we can recycle it if adding. */
    if (SPROP_IS_REMOVED(stored)) {
        firstRemoved = spp;
    } else {
        firstRemoved = NULL;
        if (adding && !SPROP_HAD_COLLISION(stored))
            SPROP_FLAG_COLLISION(spp, sprop);
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
            return spp;
        }

        if (SPROP_IS_REMOVED(stored)) {
            if (!firstRemoved)
                firstRemoved = spp;
        } else {
            if (adding && !SPROP_HAD_COLLISION(stored))
                SPROP_FLAG_COLLISION(spp, sprop);
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
 * Take care to exclude the mark bits in case we're called from the GC.
 */
#define SPROP_FLAGS_NOT_MATCHED (SPROP_MARK | SPROP_FLAG_SHAPE_REGEN)

static JSDHashNumber
js_HashScopeProperty(JSDHashTable *table, const void *key)
{
    const JSScopeProperty *sprop = (const JSScopeProperty *)key;
    JSDHashNumber hash;
    JSPropertyOp gsop;

    /* Accumulate from least to most random so the low bits are most random. */
    hash = 0;
    JS_ASSERT_IF(sprop->isMethod(),
                 !sprop->setter || sprop->setter == js_watch_set);
    gsop = sprop->getter;
    if (gsop)
        hash = JS_ROTATE_LEFT32(hash, 4) ^ jsword(gsop);
    gsop = sprop->setter;
    if (gsop)
        hash = JS_ROTATE_LEFT32(hash, 4) ^ jsword(gsop);

    hash = JS_ROTATE_LEFT32(hash, 4)
           ^ (sprop->flags & ~SPROP_FLAGS_NOT_MATCHED);

    hash = JS_ROTATE_LEFT32(hash, 4) ^ sprop->attrs;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ sprop->shortid;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ sprop->slot;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ sprop->id;
    return hash;
}

#define SPROP_MATCH(sprop, child)                                             \
    SPROP_MATCH_PARAMS(sprop, (child)->id, (child)->getter, (child)->setter,  \
                       (child)->slot, (child)->attrs, (child)->flags,         \
                       (child)->shortid)

#define SPROP_MATCH_PARAMS(sprop, aid, agetter, asetter, aslot, aattrs,       \
                           aflags, ashortid)                                  \
    (JS_ASSERT(!JSVAL_IS_NULL((sprop)->id)), JS_ASSERT(!JSVAL_IS_NULL(aid)),  \
     (sprop)->id == (aid) &&                                                  \
     SPROP_MATCH_PARAMS_AFTER_ID(sprop, agetter, asetter, aslot, aattrs,      \
                                 aflags, ashortid))

#define SPROP_MATCH_PARAMS_AFTER_ID(sprop, agetter, asetter, aslot, aattrs,   \
                                    aflags, ashortid)                         \
    ((sprop)->getter == (agetter) &&                                          \
     (sprop)->setter == (asetter) &&                                          \
     (sprop)->slot == (aslot) &&                                              \
     (sprop)->attrs == (aattrs) &&                                            \
     (((sprop)->flags ^ (aflags)) & ~SPROP_FLAGS_NOT_MATCHED) == 0 &&         \
     (sprop)->shortid == (ashortid))

static JSBool
js_MatchScopeProperty(JSDHashTable *table,
                      const JSDHashEntryHdr *hdr,
                      const void *key)
{
    const JSPropertyTreeEntry *entry = (const JSPropertyTreeEntry *)hdr;
    const JSScopeProperty *sprop = entry->child;
    const JSScopeProperty *kprop = (const JSScopeProperty *)key;

    return SPROP_MATCH(sprop, kprop);
}

static const JSDHashTableOps PropertyTreeHashOps = {
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    js_HashScopeProperty,
    js_MatchScopeProperty,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub,
    NULL
};

/*
 * A property tree node on rt->propertyFreeList overlays the following prefix
 * struct on JSScopeProperty.
 */
typedef struct FreeNode {
    jsid                id;
    JSScopeProperty     *next;
    JSScopeProperty     **prevp;
} FreeNode;

#define FREENODE(sprop) ((FreeNode *) (sprop))

#define FREENODE_INSERT(list, sprop)                                          \
    JS_BEGIN_MACRO                                                            \
        FREENODE(sprop)->next = (list);                                       \
        FREENODE(sprop)->prevp = &(list);                                     \
        if (list)                                                             \
            FREENODE(list)->prevp = &FREENODE(sprop)->next;                   \
        (list) = (sprop);                                                     \
    JS_END_MACRO

#define FREENODE_REMOVE(sprop)                                                \
    JS_BEGIN_MACRO                                                            \
        *FREENODE(sprop)->prevp = FREENODE(sprop)->next;                      \
        if (FREENODE(sprop)->next)                                            \
            FREENODE(FREENODE(sprop)->next)->prevp = FREENODE(sprop)->prevp;  \
    JS_END_MACRO

/* NB: Called with rt->gcLock held. */
static JSScopeProperty *
NewScopeProperty(JSRuntime *rt)
{
    JSScopeProperty *sprop;

    sprop = rt->propertyFreeList;
    if (sprop) {
        FREENODE_REMOVE(sprop);
    } else {
        JS_ARENA_ALLOCATE_CAST(sprop, JSScopeProperty *,
                               &rt->propertyArenaPool,
                               sizeof(JSScopeProperty));
        if (!sprop)
            return NULL;
    }

    JS_RUNTIME_METER(rt, livePropTreeNodes);
    JS_RUNTIME_METER(rt, totalPropTreeNodes);
    return sprop;
}

#define CHUNKY_KIDS_TAG         ((jsuword)1)
#define KIDS_IS_CHUNKY(kids)    ((jsuword)(kids) & CHUNKY_KIDS_TAG)
#define KIDS_TO_CHUNK(kids)     ((PropTreeKidsChunk *)                        \
                                 ((jsuword)(kids) & ~CHUNKY_KIDS_TAG))
#define CHUNK_TO_KIDS(chunk)    ((JSScopeProperty *)                          \
                                 ((jsuword)(chunk) | CHUNKY_KIDS_TAG))
#define MAX_KIDS_PER_CHUNK      10
#define CHUNK_HASH_THRESHOLD    30

typedef struct PropTreeKidsChunk PropTreeKidsChunk;

struct PropTreeKidsChunk {
    JSScopeProperty     *kids[MAX_KIDS_PER_CHUNK];
    JSDHashTable        *table;
    PropTreeKidsChunk   *next;
};

static PropTreeKidsChunk *
NewPropTreeKidsChunk(JSRuntime *rt)
{
    PropTreeKidsChunk *chunk;

    chunk = (PropTreeKidsChunk *) js_calloc(sizeof *chunk);
    if (!chunk)
        return NULL;
    JS_ASSERT(((jsuword)chunk & CHUNKY_KIDS_TAG) == 0);
    JS_RUNTIME_METER(rt, propTreeKidsChunks);
    return chunk;
}

static void
DestroyPropTreeKidsChunk(JSRuntime *rt, PropTreeKidsChunk *chunk)
{
    JS_RUNTIME_UNMETER(rt, propTreeKidsChunks);
    if (chunk->table)
        JS_DHashTableDestroy(chunk->table);
    js_free(chunk);
}

/* NB: Called with rt->gcLock held. */
static bool
InsertPropertyTreeChild(JSRuntime *rt, JSScopeProperty *parent,
                        JSScopeProperty *child, PropTreeKidsChunk *sweptChunk)
{
    JSDHashTable *table;
    JSPropertyTreeEntry *entry;
    JSScopeProperty **childp, *kids, *sprop;
    PropTreeKidsChunk *chunk, **chunkp;
    uintN i;

    JS_ASSERT(!parent || child->parent != parent);
    JS_ASSERT(!JSVAL_IS_NULL(child->id));

    if (!parent) {
        table = &rt->propertyTreeHash;
        entry = (JSPropertyTreeEntry *)
                JS_DHashTableOperate(table, child, JS_DHASH_ADD);
        if (!entry)
            return false;
        childp = &entry->child;
        sprop = *childp;
        if (!sprop) {
            *childp = child;
        } else {
            /*
             * A "Duplicate child" case.
             *
             * We can't do away with child, as at least one live scope entry
             * still points at it.  What's more, that scope's lastProp chains
             * through an ancestor line to reach child, and js_Enumerate and
             * others count on this linkage.  We must leave child out of the
             * hash table, and not require it to be there when we eventually
             * GC it (see RemovePropertyTreeChild, below).
             *
             * It is necessary to leave the duplicate child out of the hash
             * table to preserve entry uniqueness.  It is safe to leave the
             * child out of the hash table (unlike the duplicate child cases
             * below), because the child's parent link will be null, which
             * can't dangle.
             */
            JS_ASSERT(sprop != child && SPROP_MATCH(sprop, child));
            JS_RUNTIME_METER(rt, duplicatePropTreeNodes);
        }
    } else {
        JS_ASSERT(!JSVAL_IS_NULL(parent->id));
        childp = &parent->kids;
        kids = *childp;
        if (kids) {
            if (KIDS_IS_CHUNKY(kids)) {
                chunk = KIDS_TO_CHUNK(kids);

                table = chunk->table;
                if (table) {
                    entry = (JSPropertyTreeEntry *)
                            JS_DHashTableOperate(table, child, JS_DHASH_ADD);
                    if (!entry)
                        return false;
                    if (!entry->child) {
                        entry->child = child;
                        while (chunk->next)
                            chunk = chunk->next;
                        for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                            childp = &chunk->kids[i];
                            sprop = *childp;
                            if (!sprop)
                                goto insert;
                        }
                        chunkp = &chunk->next;
                        goto new_chunk;
                    }
                }

                do {
                    for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                        childp = &chunk->kids[i];
                        sprop = *childp;
                        if (!sprop)
                            goto insert;

                        JS_ASSERT(sprop != child);
                        if (SPROP_MATCH(sprop, child)) {
                            /*
                             * Duplicate child, see comment above.  In this
                             * case, we must let the duplicate be inserted at
                             * this level in the tree, so we keep iterating,
                             * looking for an empty slot in which to insert.
                             */
                            JS_ASSERT(sprop != child);
                            JS_RUNTIME_METER(rt, duplicatePropTreeNodes);
                        }
                    }
                    chunkp = &chunk->next;
                } while ((chunk = *chunkp) != NULL);

            new_chunk:
                if (sweptChunk) {
                    chunk = sweptChunk;
                } else {
                    chunk = NewPropTreeKidsChunk(rt);
                    if (!chunk)
                        return false;
                }
                *chunkp = chunk;
                childp = &chunk->kids[0];
            } else {
                sprop = kids;
                JS_ASSERT(sprop != child);
                if (SPROP_MATCH(sprop, child)) {
                    /*
                     * Duplicate child, see comment above.  Once again, we
                     * must let duplicates created by deletion pile up in a
                     * kids-chunk-list, in order to find them when sweeping
                     * and thereby avoid dangling parent pointers.
                     */
                    JS_RUNTIME_METER(rt, duplicatePropTreeNodes);
                }
                if (sweptChunk) {
                    chunk = sweptChunk;
                } else {
                    chunk = NewPropTreeKidsChunk(rt);
                    if (!chunk)
                        return false;
                }
                parent->kids = CHUNK_TO_KIDS(chunk);
                chunk->kids[0] = sprop;
                childp = &chunk->kids[1];
            }
        }
    insert:
        *childp = child;
    }

    child->parent = parent;
    return true;
}

/* NB: Called with rt->gcLock held. */
static PropTreeKidsChunk *
RemovePropertyTreeChild(JSRuntime *rt, JSScopeProperty *child)
{
    PropTreeKidsChunk *freeChunk;
    JSScopeProperty *parent, *kids, *kid;
    JSDHashTable *table;
    PropTreeKidsChunk *list, *chunk, **chunkp, *lastChunk;
    uintN i, j;
    JSPropertyTreeEntry *entry;

    freeChunk = NULL;
    parent = child->parent;
    if (!parent) {
        /*
         * Don't remove child if it is not in rt->propertyTreeHash, but only
         * matches a root child in the table that has compatible members. See
         * the "Duplicate child" comments in InsertPropertyTreeChild, above.
         */
        table = &rt->propertyTreeHash;
    } else {
        JS_ASSERT(!JSVAL_IS_NULL(parent->id));
        kids = parent->kids;
        if (KIDS_IS_CHUNKY(kids)) {
            list = chunk = KIDS_TO_CHUNK(kids);
            chunkp = &list;
            table = chunk->table;

            do {
                for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                    if (chunk->kids[i] == child) {
                        lastChunk = chunk;
                        if (!lastChunk->next) {
                            j = i + 1;
                        } else {
                            j = 0;
                            do {
                                chunkp = &lastChunk->next;
                                lastChunk = *chunkp;
                            } while (lastChunk->next);
                        }
                        for (; j < MAX_KIDS_PER_CHUNK; j++) {
                            if (!lastChunk->kids[j])
                                break;
                        }
                        --j;
                        if (chunk != lastChunk || j > i)
                            chunk->kids[i] = lastChunk->kids[j];
                        lastChunk->kids[j] = NULL;
                        if (j == 0) {
                            *chunkp = NULL;
                            if (!list)
                                parent->kids = NULL;
                            freeChunk = lastChunk;
                        }
                        goto out;
                    }
                }

                chunkp = &chunk->next;
            } while ((chunk = *chunkp) != NULL);
        } else {
            table = NULL;
            kid = kids;
            if (kid == child)
                parent->kids = NULL;
        }
    }

out:
    if (table) {
        entry = (JSPropertyTreeEntry *)
                JS_DHashTableOperate(table, child, JS_DHASH_LOOKUP);

        if (entry->child == child)
            JS_DHashTableRawRemove(table, &entry->hdr);
    }
    return freeChunk;
}

static JSDHashTable *
HashChunks(PropTreeKidsChunk *chunk, uintN n)
{
    JSDHashTable *table;
    uintN i;
    JSScopeProperty *sprop;
    JSPropertyTreeEntry *entry;

    table = JS_NewDHashTable(&PropertyTreeHashOps, NULL,
                             sizeof(JSPropertyTreeEntry),
                             JS_DHASH_DEFAULT_CAPACITY(n + 1));
    if (!table)
        return NULL;
    do {
        for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
            sprop = chunk->kids[i];
            if (!sprop)
                break;
            entry = (JSPropertyTreeEntry *)
                    JS_DHashTableOperate(table, sprop, JS_DHASH_ADD);
            entry->child = sprop;
        }
    } while ((chunk = chunk->next) != NULL);
    return table;
}

/*
 * Called without cx->runtime->gcLock held. This function acquires that lock
 * only when inserting a new child.  Thus there may be races to find or add a
 * node that result in duplicates.  We expect such races to be rare!
 *
 * We use rt->gcLock, not rt->rtLock, to avoid nesting the former inside the
 * latter in js_GenerateShape below.
 */
static JSScopeProperty *
GetPropertyTreeChild(JSContext *cx, JSScopeProperty *parent,
                     const JSScopeProperty &child)
{
    JSRuntime *rt;
    JSDHashTable *table;
    JSPropertyTreeEntry *entry;
    JSScopeProperty *sprop;
    PropTreeKidsChunk *chunk;
    uintN i, n;

    rt = cx->runtime;
    if (!parent) {
        JS_LOCK_GC(rt);

        table = &rt->propertyTreeHash;
        entry = (JSPropertyTreeEntry *)
                JS_DHashTableOperate(table, &child, JS_DHASH_ADD);
        if (!entry)
            goto out_of_memory;

        sprop = entry->child;
        if (sprop)
            goto out;
    } else {
        JS_ASSERT(!JSVAL_IS_NULL(parent->id));

        /*
         * Because chunks are appended at the end and never deleted except by
         * the GC, we can search without taking the runtime's GC lock.  We may
         * miss a matching sprop added by another thread, and make a duplicate
         * one, but that is an unlikely, therefore small, cost.  The property
         * tree has extremely low fan-out below its root in popular embeddings
         * with real-world workloads.
         *
         * Patterns such as defining closures that capture a constructor's
         * environment as getters or setters on the new object that is passed
         * in as |this| can significantly increase fan-out below the property
         * tree root -- see bug 335700 for details.
         */
        entry = NULL;
        sprop = parent->kids;
        if (sprop) {
            if (KIDS_IS_CHUNKY(sprop)) {
                chunk = KIDS_TO_CHUNK(sprop);

                table = chunk->table;
                if (table) {
                    JS_LOCK_GC(rt);
                    entry = (JSPropertyTreeEntry *)
                            JS_DHashTableOperate(table, &child, JS_DHASH_LOOKUP);
                    sprop = entry->child;
                    if (sprop)
                        goto out;
                    goto locked_not_found;
                }

                n = 0;
                do {
                    for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                        sprop = chunk->kids[i];
                        if (!sprop) {
                            n += i;
                            if (n >= CHUNK_HASH_THRESHOLD) {
                                chunk = KIDS_TO_CHUNK(parent->kids);
                                if (!chunk->table) {
                                    table = HashChunks(chunk, n);
                                    JS_LOCK_GC(rt);
                                    if (!table)
                                        goto out_of_memory;
                                    if (chunk->table)
                                        JS_DHashTableDestroy(table);
                                    else
                                        chunk->table = table;
                                    goto locked_not_found;
                                }
                            }
                            goto not_found;
                        }

                        if (SPROP_MATCH(sprop, &child))
                            return sprop;
                    }
                    n += MAX_KIDS_PER_CHUNK;
                } while ((chunk = chunk->next) != NULL);
            } else {
                if (SPROP_MATCH(sprop, &child))
                    return sprop;
            }
        }

    not_found:
        JS_LOCK_GC(rt);
    }

locked_not_found:
    sprop = NewScopeProperty(rt);
    if (!sprop)
        goto out_of_memory;

    sprop->id = child.id;
    sprop->getter = child.getter;
    sprop->setter = child.setter;
    sprop->slot = child.slot;
    sprop->attrs = child.attrs;
    sprop->flags = child.flags;
    sprop->shortid = child.shortid;
    sprop->parent = sprop->kids = NULL;
    sprop->shape = js_GenerateShape(cx, true);

    if (!parent) {
        entry->child = sprop;
    } else {
        if (!InsertPropertyTreeChild(rt, parent, sprop, NULL))
            goto out_of_memory;
    }

  out:
    JS_UNLOCK_GC(rt);
    return sprop;

  out_of_memory:
    JS_UNLOCK_GC(rt);
    JS_ReportOutOfMemory(cx);
    return NULL;
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

    /*
     * Aliases share another property's slot, passed in the |slot| parameter.
     * Shared properties have no slot. Unshared properties that do not alias
     * another property's slot allocate a slot here, but may lose it due to a
     * JS_ClearScope call.
     */
    if (!(child.flags & SPROP_IS_ALIAS)) {
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
    
    JSScopeProperty *sprop = GetPropertyTreeChild(cx, parent, child);
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
         js_LeaveTraceIfGlobalObject(cx, object);

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
        JSTraceMonitor *tm = &JS_TRACE_MONITOR(cx);
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
    JSScopeProperty *dprop = NewScopeProperty(cx->runtime);
    if (!dprop) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    dprop->id = child.id;
    dprop->getter = child.getter;
    dprop->setter = child.setter;
    dprop->slot = child.slot;
    dprop->attrs = child.attrs;
    dprop->flags = child.flags | SPROP_IN_DICTIONARY;
    dprop->shortid = child.shortid;
    dprop->shape = js_GenerateShape(cx, false);

    dprop->childp = NULL;
    insertDictionaryProperty(dprop, childp);
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
    JS_ASSERT_IF(attrs & JSPROP_GETTER, getter);
    JS_ASSERT_IF(attrs & JSPROP_SETTER, setter);

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
    if (setter == JS_PropertyStub)
        setter = NULL;
    if (flags & SPROP_IS_METHOD) {
        /* Here, getter is the method, a function object reference. */
        JS_ASSERT(getter);
        JS_ASSERT(!setter || setter == js_watch_set);
        JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
    } else {
        if (getter == JS_PropertyStub)
            getter = NULL;
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
        JSScopeProperty child;

        child.id = id;
        child.getter = getter;
        child.setter = setter;
        child.slot = slot;
        child.attrs = attrs;
        child.flags = flags;
        child.shortid = shortid;
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
    JS_ASSERT_IF(attrs & JSPROP_GETTER, getter);
    JS_ASSERT_IF(attrs & JSPROP_SETTER, setter);

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
    if (SPROP_MATCH_PARAMS_AFTER_ID(sprop, getter, setter, slot, attrs,
                                    flags, shortid)) {
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
        JSScopeProperty child;

        /* Find or create a property tree node labeled by our arguments. */
        child.id = id;
        child.getter = getter;
        child.setter = setter;
        child.slot = slot;
        child.attrs = attrs;
        child.flags = flags;
        child.shortid = shortid;
        sprop = getChildProperty(cx, lastProp, child);
    }

    if (sprop) {
        /* Store the tree node pointer in the table entry for id. */
        if (table)
            SPROP_STORE_PRESERVING_COLLISION(spp, sprop);
        CHECK_ANCESTOR_LINE(this, false);

        /* See comment in JSScope::addPropertyHelper about ignoring OOM here. */
        if (!table && entryCount >= SCOPE_HASH_THRESHOLD)
            (void) createTable(cx, false);

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
    JSScopeProperty child, *newsprop;

    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, this));
    CHECK_ANCESTOR_LINE(this, true);

    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    JS_ASSERT(hasProperty(sprop));

    attrs |= sprop->attrs & mask;

    /* Allow only shared (slot-less) => unshared (slot-full) transition. */
    JS_ASSERT(!((attrs ^ sprop->attrs) & JSPROP_SHARED) ||
              !(attrs & JSPROP_SHARED));

    /* Don't allow method properties to be changed to accessor properties. */
    JS_ASSERT(!(sprop->flags & SPROP_IS_METHOD));

    if (getter == JS_PropertyStub)
        getter = NULL;
    if (setter == JS_PropertyStub)
        setter = NULL;
    if (sprop->attrs == attrs &&
        sprop->getter == getter &&
        sprop->setter == setter) {
        return sprop;
    }

    child.id = sprop->id;
    child.getter = getter;
    child.setter = setter;
    child.slot = sprop->slot;
    child.attrs = attrs;
    child.flags = sprop->flags;
    child.shortid = sprop->shortid;

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
         * JSScope::remove here, because it will free a valid sprop->slot and
         * JSScope::putProperty won't re-allocate it.
         */
        newsprop = putProperty(cx, child.id, child.getter, child.setter, child.slot,
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
    JSScopeProperty **spp, *stored, *sprop;
    uint32 size;

    JS_ASSERT(JS_IS_SCOPE_LOCKED(cx, this));
    CHECK_ANCESTOR_LINE(this, true);
    if (sealed()) {
        reportReadOnlyScope(cx);
        return false;
    }

    spp = search(id, false);
    stored = *spp;
    sprop = SPROP_CLEAR_COLLISION(stored);
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
    if (SPROP_HAD_COLLISION(stored)) {
        JS_ASSERT(table);
        *spp = SPROP_REMOVED;
        ++removedCount;
    } else {
        METER(removeFrees);
        if (table)
            *spp = NULL;
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
    js_LeaveTraceIfGlobalObject(cx, object);

    JSClass *clasp = object->getClass();
    JSObject *proto = object->getProto();
    JSEmptyScope *emptyScope;
    uint32 newShape;
    if (proto &&
        OBJ_IS_NATIVE(proto) &&
        (emptyScope = OBJ_SCOPE(proto)->emptyScope) &&
        emptyScope->clasp == clasp) {
        newShape = emptyScope->shape;
    } else {
        newShape = js_GenerateShape(cx, false);
    }
    initMinimal(cx, newShape);

    JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
}

void
JSScope::brandingShapeChange(JSContext *cx, uint32 slot, jsval v)
{
    generateOwnShape(cx);
}

void
JSScope::deletingShapeChange(JSContext *cx, JSScopeProperty *sprop)
{
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    generateOwnShape(cx);
}

bool
JSScope::methodShapeChange(JSContext *cx, JSScopeProperty *sprop, jsval toval)
{
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    if (sprop->isMethod()) {
#ifdef DEBUG
        jsval prev = LOCKED_OBJ_GET_SLOT(object, sprop->slot);
        JS_ASSERT(sprop->methodValue() == prev);
        JS_ASSERT(hasMethodBarrier());
        JS_ASSERT(object->getClass() == &js_ObjectClass);
        JS_ASSERT(!sprop->setter || sprop->setter == js_watch_set);
#endif

        /*
         * Pass null to make a stub getter, but pass along sprop->setter to
         * preserve watchpoints. Clear SPROP_IS_METHOD from flags as we are
         * despecializing from a method memoized in the property tree to a
         * plain old function-valued property.
         */
        sprop = putProperty(cx, sprop->id, NULL, sprop->setter, sprop->slot,
                            sprop->attrs, sprop->flags & ~SPROP_IS_METHOD,
                            sprop->shortid);
        if (!sprop)
            return false;
    }

    generateOwnShape(cx);
    return true;
}

bool
JSScope::methodShapeChange(JSContext *cx, uint32 slot, jsval toval)
{
    if (!hasMethodBarrier()) {
        generateOwnShape(cx);
    } else {
        for (JSScopeProperty *sprop = lastProp; sprop; sprop = sprop->parent) {
            JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
            if (sprop->slot == slot)
                return methodShapeChange(cx, sprop, toval);
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
JSScope::sealingShapeChange(JSContext *cx)
{
    generateOwnShape(cx);
}

void
JSScope::shadowingShapeChange(JSContext *cx, JSScopeProperty *sprop)
{
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    generateOwnShape(cx);
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
        flags |= SPROP_MARK;
    js_TraceId(trc, id);

#if JS_HAS_GETTER_SETTER
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        if (attrs & JSPROP_GETTER) {
            JS_SET_TRACING_DETAILS(trc, PrintPropertyGetterOrSetter, this, 0);
            JS_CallTracer(trc, getterObject(), JSTRACE_OBJECT);
        }
        if (attrs & JSPROP_SETTER) {
            JS_SET_TRACING_DETAILS(trc, PrintPropertyGetterOrSetter, this, 1);
            JS_CallTracer(trc, setterObject(), JSTRACE_OBJECT);
        }
    }
#endif /* JS_HAS_GETTER_SETTER */

    if (isMethod()) {
        JS_SET_TRACING_DETAILS(trc, PrintPropertyMethod, this, 0);
        JS_CallTracer(trc, methodObject(), JSTRACE_OBJECT);
    }
}

#ifdef DEBUG

#include <stdio.h>

static void
MeterKidCount(JSBasicStats *bs, uintN nkids)
{
    JS_BASIC_STATS_ACCUM(bs, nkids);
    bs->hist[JS_MIN(nkids, 10)]++;
}

static void
MeterPropertyTree(JSBasicStats *bs, JSScopeProperty *node)
{
    uintN i, nkids;
    JSScopeProperty *kids, *kid;
    PropTreeKidsChunk *chunk;

    nkids = 0;
    kids = node->kids;
    if (kids) {
        if (KIDS_IS_CHUNKY(kids)) {
            for (chunk = KIDS_TO_CHUNK(kids); chunk; chunk = chunk->next) {
                for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                    kid = chunk->kids[i];
                    if (!kid)
                        break;
                    MeterPropertyTree(bs, kid);
                    nkids++;
                }
            }
        } else {
            MeterPropertyTree(bs, kids);
            nkids = 1;
        }
    }

    MeterKidCount(bs, nkids);
}

static JSDHashOperator
js_MeterPropertyTree(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number,
                     void *arg)
{
    JSPropertyTreeEntry *entry = (JSPropertyTreeEntry *)hdr;
    JSBasicStats *bs = (JSBasicStats *)arg;

    MeterPropertyTree(bs, entry->child);
    return JS_DHASH_NEXT;
}

static void
DumpSubtree(JSContext *cx, JSScopeProperty *sprop, int level, FILE *fp)
{
    jsval v;
    JSString *str;
    JSScopeProperty *kids, *kid;
    PropTreeKidsChunk *chunk;
    uintN i;

    fprintf(fp, "%*sid ", level, "");
    v = ID_TO_VALUE(sprop->id);
    JS_ASSERT(!JSVAL_IS_NULL(v));
    if (JSID_IS_INT(sprop->id)) {
        fprintf(fp, "%d", JSVAL_TO_INT(v));
    } else {
        if (JSID_IS_ATOM(sprop->id)) {
            str = JSVAL_TO_STRING(v);
        } else {
            JS_ASSERT(JSID_IS_OBJECT(sprop->id));
            str = js_ValueToString(cx, v);
            fputs("object ", fp);
        }
        if (!str)
            fputs("<error>", fp);
        else
            js_FileEscapedString(fp, str, '"');
    }

    fprintf(fp, " g/s %p/%p slot %u attrs %x flags %x shortid %d\n",
            (void *) sprop->getter, (void *) sprop->setter, sprop->slot,
            sprop->attrs, sprop->flags, sprop->shortid);
    kids = sprop->kids;
    if (kids) {
        ++level;
        if (KIDS_IS_CHUNKY(kids)) {
            chunk = KIDS_TO_CHUNK(kids);
            do {
                for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                    kid = chunk->kids[i];
                    if (!kid)
                        break;
                    JS_ASSERT(kid->parent == sprop);
                    DumpSubtree(cx, kid, level, fp);
                }
            } while ((chunk = chunk->next) != NULL);
        } else {
            kid = kids;
            DumpSubtree(cx, kid, level, fp);
        }
    }
}

#endif /* DEBUG */

void
js_SweepScopeProperties(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JSArena **ap, *a;
    JSScopeProperty *limit, *sprop, *parent, *kids, *kid;
    uintN liveCount;
    PropTreeKidsChunk *chunk, *nextChunk, *freeChunk;
    uintN i;

#ifdef DEBUG
    JSBasicStats bs;
    uint32 livePropCapacity = 0, totalLiveCount = 0;
    static FILE *logfp;
    if (!logfp) {
        if (const char *filename = getenv("JS_PROPTREE_STATFILE"))
            logfp = fopen(filename, "w");
    }

    if (logfp) {
        JS_BASIC_STATS_INIT(&bs);
        MeterKidCount(&bs, rt->propertyTreeHash.entryCount);
        JS_DHashTableEnumerate(&rt->propertyTreeHash, js_MeterPropertyTree, &bs);

        double props, nodes, mean, sigma;

        props = rt->liveScopePropsPreSweep;
        nodes = rt->livePropTreeNodes;
        JS_ASSERT(nodes == bs.sum);
        mean = JS_MeanAndStdDevBS(&bs, &sigma);

        fprintf(logfp,
                "props %g nodes %g beta %g meankids %g sigma %g max %u\n",
                props, nodes, nodes / props, mean, sigma, bs.max);

        JS_DumpHistogram(&bs, logfp);
    }
#endif

    ap = &rt->propertyArenaPool.first.next;
    while ((a = *ap) != NULL) {
        limit = (JSScopeProperty *) a->avail;
        liveCount = 0;
        for (sprop = (JSScopeProperty *) a->base; sprop < limit; sprop++) {
            /* If the id is null, sprop is already on the freelist. */
            if (sprop->id == JSVAL_NULL)
                continue;

            /*
             * If the mark bit is set, sprop is alive, so clear the mark bit
             * and continue the while loop.
             *
             * Regenerate sprop->shape if it hasn't already been refreshed
             * during the mark phase, when live scopes' lastProp members are
             * followed to update both scope->shape and lastProp->shape.
             */
            if (sprop->flags & SPROP_MARK) {
                sprop->flags &= ~SPROP_MARK;
                if (rt->gcRegenShapes) {
                    if (sprop->flags & SPROP_FLAG_SHAPE_REGEN)
                        sprop->flags &= ~SPROP_FLAG_SHAPE_REGEN;
                    else
                        sprop->shape = js_RegenerateShapeForGC(cx);
                }
                liveCount++;
                continue;
            }

            if (!(sprop->flags & SPROP_IN_DICTIONARY)) {
                /* Ok, sprop is garbage to collect: unlink it from its parent. */
                freeChunk = RemovePropertyTreeChild(rt, sprop);

                /*
                 * Take care to reparent all sprop's kids to their grandparent.
                 * InsertPropertyTreeChild can potentially fail for two reasons:
                 *
                 * 1. If parent is null, insertion into the root property hash
                 *    table may fail. We are forced to leave the kid out of the
                 *    table (as can already happen with duplicates) but ensure
                 *    that the kid's parent pointer is set to null.
                 *
                 * 2. If parent is non-null, allocation of a new KidsChunk can
                 *    fail. To prevent this from happening, we allow sprops's own
                 *    chunks to be reused by the grandparent, which removes the
                 *    need for InsertPropertyTreeChild to malloc a new KidsChunk.
                 *
                 *    If sprop does not have chunky kids, then we rely on the
                 *    RemovePropertyTreeChild call above (which removed sprop from
                 *    its parent) either leaving one free entry, or else returning
                 *    the now-unused chunk to us so we can reuse it.
                 *
                 * We also require the grandparent to have either no kids or else
                 * chunky kids. A single non-chunky kid would force a new chunk to
                 * be malloced in some cases (if sprop had a single non-chunky
                 * kid, or a multiple of MAX_KIDS_PER_CHUNK kids). Note that
                 * RemovePropertyTreeChild never converts a single-entry chunky
                 * kid back to a non-chunky kid, so we are assured of correct
                 * behaviour.
                 */
                kids = sprop->kids;
                if (kids) {
                    sprop->kids = NULL;
                    parent = sprop->parent;

                    /* The grandparent must have either no kids or chunky kids. */
                    JS_ASSERT(!parent || !parent->kids ||
                              KIDS_IS_CHUNKY(parent->kids));
                    if (KIDS_IS_CHUNKY(kids)) {
                        chunk = KIDS_TO_CHUNK(kids);
                        do {
                            nextChunk = chunk->next;
                            chunk->next = NULL;
                            for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                                kid = chunk->kids[i];
                                if (!kid)
                                    break;
                                JS_ASSERT(kid->parent == sprop);

                                /*
                                 * Clear a space in the kids array for possible
                                 * re-use by InsertPropertyTreeChild.
                                 */
                                chunk->kids[i] = NULL;
                                if (!InsertPropertyTreeChild(rt, parent, kid, chunk)) {
                                    /*
                                     * This can happen only if we failed to add an
                                     * entry to the root property hash table.
                                     */
                                    JS_ASSERT(!parent);
                                    kid->parent = NULL;
                                }
                            }
                            if (!chunk->kids[0]) {
                                /* The chunk wasn't reused, so we must free it. */
                                DestroyPropTreeKidsChunk(rt, chunk);
                            }
                        } while ((chunk = nextChunk) != NULL);
                    } else {
                        kid = kids;
                        if (!InsertPropertyTreeChild(rt, parent, kid, freeChunk)) {
                            /*
                             * This can happen only if we failed to add an entry
                             * to the root property hash table.
                             */
                            JS_ASSERT(!parent);
                            kid->parent = NULL;
                        }
                    }
                }

                if (freeChunk && !freeChunk->kids[0]) {
                    /* The chunk wasn't reused, so we must free it. */
                    DestroyPropTreeKidsChunk(rt, freeChunk);
                }
            }

            /* Clear id so we know (above) that sprop is on the freelist. */
            sprop->id = JSVAL_NULL;
            FREENODE_INSERT(rt->propertyFreeList, sprop);
            JS_RUNTIME_UNMETER(rt, livePropTreeNodes);
        }

        /* If a contains no live properties, return it to the malloc heap. */
        if (liveCount == 0) {
            for (sprop = (JSScopeProperty *) a->base; sprop < limit; sprop++)
                FREENODE_REMOVE(sprop);
            JS_ARENA_DESTROY(&rt->propertyArenaPool, a, ap);
        } else {
#ifdef DEBUG
            livePropCapacity += limit - (JSScopeProperty *) a->base;
            totalLiveCount += liveCount;
#endif
            ap = &a->next;
        }
    }

#ifdef DEBUG
    if (logfp) {
        fprintf(logfp,
                "\nProperty tree stats for gcNumber %lu\n",
                (unsigned long) rt->gcNumber);

        fprintf(logfp, "arenautil %g%%\n",
                (totalLiveCount && livePropCapacity)
                ? (totalLiveCount * 100.0) / livePropCapacity
                : 0.0);

#define RATE(f1, f2) (((double)js_scope_stats.f1 / js_scope_stats.f2) * 100.0)

        fprintf(logfp,
                "Scope search stats:\n"
                "  searches:       %6u\n"
                "  hits:           %6u %5.2f%% of searches\n"
                "  misses:         %6u %5.2f%%\n"
                "  hashes:         %6u %5.2f%%\n"
                "  steps:          %6u %5.2f%% %5.2f%% of hashes\n"
                "  stepHits:       %6u %5.2f%% %5.2f%%\n"
                "  stepMisses:     %6u %5.2f%% %5.2f%%\n"
                "  tableAllocFails %6u\n"
                "  toDictFails     %6u\n"
                "  wrapWatchFails  %6u\n"
                "  adds:           %6u\n"
                "  addFails:       %6u\n"
                "  puts:           %6u\n"
                "  redundantPuts:  %6u\n"
                "  putFails:       %6u\n"
                "  changes:        %6u\n"
                "  changeFails:    %6u\n"
                "  compresses:     %6u\n"
                "  grows:          %6u\n"
                "  removes:        %6u\n"
                "  removeFrees:    %6u\n"
                "  uselessRemoves: %6u\n"
                "  shrinks:        %6u\n",
                js_scope_stats.searches,
                js_scope_stats.hits, RATE(hits, searches),
                js_scope_stats.misses, RATE(misses, searches),
                js_scope_stats.hashes, RATE(hashes, searches),
                js_scope_stats.steps, RATE(steps, searches), RATE(steps, hashes),
                js_scope_stats.stepHits,
                RATE(stepHits, searches), RATE(stepHits, hashes),
                js_scope_stats.stepMisses,
                RATE(stepMisses, searches), RATE(stepMisses, hashes),
                js_scope_stats.tableAllocFails,
                js_scope_stats.toDictFails,
                js_scope_stats.wrapWatchFails,
                js_scope_stats.adds,
                js_scope_stats.addFails,
                js_scope_stats.puts,
                js_scope_stats.redundantPuts,
                js_scope_stats.putFails,
                js_scope_stats.changes,
                js_scope_stats.changeFails,
                js_scope_stats.compresses,
                js_scope_stats.grows,
                js_scope_stats.removes,
                js_scope_stats.removeFrees,
                js_scope_stats.uselessRemoves,
                js_scope_stats.shrinks);

#undef RATE

        fflush(logfp);
    }

    if (const char *filename = getenv("JS_PROPTREE_DUMPFILE")) {
        char pathname[1024];
        JS_snprintf(pathname, sizeof pathname, "%s.%lu", filename, (unsigned long)rt->gcNumber);
        FILE *dumpfp = fopen(pathname, "w");
        if (dumpfp) {
            JSPropertyTreeEntry *pte, *end;

            pte = (JSPropertyTreeEntry *) rt->propertyTreeHash.entryStore;
            end = pte + JS_DHASH_TABLE_SIZE(&rt->propertyTreeHash);
            while (pte < end) {
                if (pte->child)
                    DumpSubtree(cx, pte->child, 0, dumpfp);
                pte++;
            }
            fclose(dumpfp);
        }
    }
#endif /* DEBUG */
}

bool
js_InitPropertyTree(JSRuntime *rt)
{
    if (!JS_DHashTableInit(&rt->propertyTreeHash, &PropertyTreeHashOps, NULL,
                           sizeof(JSPropertyTreeEntry), JS_DHASH_MIN_SIZE)) {
        rt->propertyTreeHash.ops = NULL;
        return false;
    }
    JS_InitArenaPool(&rt->propertyArenaPool, "properties",
                     256 * sizeof(JSScopeProperty), sizeof(void *), NULL);
    return true;
}

void
js_FinishPropertyTree(JSRuntime *rt)
{
    if (rt->propertyTreeHash.ops) {
        JS_DHashTableFinish(&rt->propertyTreeHash);
        rt->propertyTreeHash.ops = NULL;
    }
    JS_FinishArenaPool(&rt->propertyArenaPool);
}

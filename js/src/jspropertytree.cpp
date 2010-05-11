/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SpiderMonkey property tree implementation
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2002-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org>
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

#include "jstypes.h"
#include "jsarena.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jspropertytree.h"
#include "jsscope.h"

#include "jsscopeinlines.h"

using namespace js;

struct PropertyRootKey
{
    const JSScopeProperty *firstProp;
    uint32                emptyShape;

    PropertyRootKey(const JSScopeProperty *child, uint32 shape)
      : firstProp(child), emptyShape(shape) {}

    static JSDHashNumber hash(JSDHashTable *table, const void *key) {
        const PropertyRootKey *rkey = (const PropertyRootKey *)key;

        return rkey->firstProp->hash() ^ rkey->emptyShape;
    }
};

struct PropertyRootEntry : public JSDHashEntryHdr
{
    JSScopeProperty *firstProp;
    uint32          emptyShape;
    uint32          newEmptyShape;

    static JSBool match(JSDHashTable *table, const JSDHashEntryHdr *hdr, const void *key) {
        const PropertyRootEntry *rent = (const PropertyRootEntry *)hdr;
        const PropertyRootKey *rkey = (const PropertyRootKey *)key;

        return rent->firstProp->matches(rkey->firstProp) &&
               rent->emptyShape == rkey->emptyShape;
    }
};

static const JSDHashTableOps PropertyRootHashOps = {
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    PropertyRootKey::hash,
    PropertyRootEntry::match,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub,
    NULL
};

static JSDHashNumber
HashScopeProperty(JSDHashTable *table, const void *key)
{
    const JSScopeProperty *sprop = (const JSScopeProperty *)key;
    return sprop->hash();
}

static JSBool
MatchScopeProperty(JSDHashTable *table,
                   const JSDHashEntryHdr *hdr,
                   const void *key)
{
    const JSPropertyTreeEntry *entry = (const JSPropertyTreeEntry *)hdr;
    const JSScopeProperty *sprop = entry->child;
    const JSScopeProperty *kprop = (const JSScopeProperty *)key;

    return sprop->matches(kprop);
}

static const JSDHashTableOps PropertyTreeHashOps = {
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    HashScopeProperty,
    MatchScopeProperty,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub,
    NULL
};

bool
PropertyTree::init()
{
    if (!JS_DHashTableInit(&hash, &PropertyRootHashOps, NULL,
                           sizeof(PropertyRootEntry), JS_DHASH_MIN_SIZE)) {
        hash.ops = NULL;
        return false;
    }
    JS_InitArenaPool(&arenaPool, "properties",
                     256 * sizeof(JSScopeProperty), sizeof(void *), NULL);
    emptyShapeChanges = 0;
    return true;
}

void
PropertyTree::finish()
{
    if (hash.ops) {
        JS_DHashTableFinish(&hash);
        hash.ops = NULL;
    }
    JS_FinishArenaPool(&arenaPool);
}

/*
 * NB: Called with cx->runtime->gcLock held if gcLocked is true.
 * On failure, return null after unlocking the GC and reporting out of memory.
 */
JSScopeProperty *
PropertyTree::newScopeProperty(JSContext *cx, bool gcLocked)
{
    JSScopeProperty *sprop;

    if (!gcLocked)
        JS_LOCK_GC(cx->runtime);
    sprop = freeList;
    if (sprop) {
        sprop->removeFree();
    } else {
        JS_ARENA_ALLOCATE_CAST(sprop, JSScopeProperty *, &arenaPool,
                               sizeof(JSScopeProperty));
        if (!sprop) {
            JS_UNLOCK_GC(cx->runtime);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }
    if (!gcLocked)
        JS_UNLOCK_GC(cx->runtime);

    JS_RUNTIME_METER(cx->runtime, livePropTreeNodes);
    JS_RUNTIME_METER(cx->runtime, totalPropTreeNodes);
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

struct PropTreeKidsChunk {
    JSScopeProperty     *kids[MAX_KIDS_PER_CHUNK];
    JSDHashTable        *table;
    PropTreeKidsChunk   *next;
};

/*
 * NB: Called with cx->runtime->gcLock held, always.
 * On failure, return null after unlocking the GC and reporting out of memory.
 */
static PropTreeKidsChunk *
NewPropTreeKidsChunk(JSContext *cx)
{
    PropTreeKidsChunk *chunk;

    chunk = (PropTreeKidsChunk *) js_calloc(sizeof *chunk);
    if (!chunk) {
        JS_UNLOCK_GC(cx->runtime);
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    JS_ASSERT(((jsuword)chunk & CHUNKY_KIDS_TAG) == 0);
    JS_RUNTIME_METER(cx->runtime, propTreeKidsChunks);
    return chunk;
}

static PropTreeKidsChunk *
DestroyPropTreeKidsChunk(JSContext *cx, PropTreeKidsChunk *chunk)
{
    JS_RUNTIME_UNMETER(cx->runtime, propTreeKidsChunks);
    if (chunk->table)
        JS_DHashTableDestroy(chunk->table);

    PropTreeKidsChunk *nextChunk = chunk->next;
    js_free(chunk);
    return nextChunk;
}

/*
 * NB: Called with cx->runtime->gcLock held, always.
 * On failure, return null after unlocking the GC and reporting out of memory.
 */
bool
PropertyTree::insertChild(JSContext *cx, JSScopeProperty *parent,
                          JSScopeProperty *child)
{
    JS_ASSERT(parent);
    JS_ASSERT(!child->parent);
    JS_ASSERT(!JSVAL_IS_NULL(parent->id));
    JS_ASSERT(!JSVAL_IS_NULL(child->id));

    JSScopeProperty **childp = &parent->kids;
    if (JSScopeProperty *kids = *childp) {
        JSScopeProperty *sprop;
        PropTreeKidsChunk *chunk;

        if (!KIDS_IS_CHUNKY(kids)) {
            sprop = kids;
            JS_ASSERT(sprop != child);
            if (sprop->matches(child)) {
                /*
                 * Duplicate child created while racing to getChild on the same
                 * node label. See PropertyTree::getChild, further below.
                 */
                JS_RUNTIME_METER(cx->runtime, duplicatePropTreeNodes);
            }
            chunk = NewPropTreeKidsChunk(cx);
            if (!chunk)
                return false;
            parent->kids = CHUNK_TO_KIDS(chunk);
            chunk->kids[0] = sprop;
            childp = &chunk->kids[1];
        } else {
            PropTreeKidsChunk **chunkp;

            chunk = KIDS_TO_CHUNK(kids);
            if (JSDHashTable *table = chunk->table) {
                JSPropertyTreeEntry *entry = (JSPropertyTreeEntry *)
                    JS_DHashTableOperate(table, child, JS_DHASH_ADD);
                if (!entry) {
                    JS_UNLOCK_GC(cx->runtime);
                    JS_ReportOutOfMemory(cx);
                    return false;
                }
                if (!entry->child) {
                    entry->child = child;
                    while (chunk->next)
                        chunk = chunk->next;
                    for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
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
                for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                    childp = &chunk->kids[i];
                    sprop = *childp;
                    if (!sprop)
                        goto insert;

                    JS_ASSERT(sprop != child);
                    if (sprop->matches(child)) {
                        /*
                         * Duplicate child, see comment above.  In this
                         * case, we must let the duplicate be inserted at
                         * this level in the tree, so we keep iterating,
                         * looking for an empty slot in which to insert.
                         */
                        JS_ASSERT(sprop != child);
                        JS_RUNTIME_METER(cx->runtime, duplicatePropTreeNodes);
                    }
                }
                chunkp = &chunk->next;
            } while ((chunk = *chunkp) != NULL);

          new_chunk:
            chunk = NewPropTreeKidsChunk(cx);
            if (!chunk)
                return false;
            *chunkp = chunk;
            childp = &chunk->kids[0];
        }
    }

  insert:
    *childp = child;
    child->parent = parent;
    return true;
}

/* NB: Called with cx->runtime->gcLock held. */
void
PropertyTree::removeChild(JSContext *cx, JSScopeProperty *child)
{
    uintN i, j;
    JSPropertyTreeEntry *entry;

    JSScopeProperty *parent = child->parent;
    JS_ASSERT(parent);
    JS_ASSERT(!JSVAL_IS_NULL(parent->id));

    JSScopeProperty *kids = parent->kids;
    if (!KIDS_IS_CHUNKY(kids)) {
        JSScopeProperty *kid = kids;
        if (kid == child)
            parent->kids = NULL;
        return;
    }

    PropTreeKidsChunk *list = KIDS_TO_CHUNK(kids);
    PropTreeKidsChunk *chunk = list;
    PropTreeKidsChunk **chunkp = &list;

    JSDHashTable *table = chunk->table;
    PropTreeKidsChunk *freeChunk = NULL;

    do {
        for (i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
            if (chunk->kids[i] == child) {
                PropTreeKidsChunk *lastChunk = chunk;
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

  out:
    if (table) {
        entry = (JSPropertyTreeEntry *)
                JS_DHashTableOperate(table, child, JS_DHASH_LOOKUP);

        if (entry->child == child)
            JS_DHashTableRawRemove(table, &entry->hdr);
    }
    if (freeChunk)
        DestroyPropTreeKidsChunk(cx, freeChunk);
}

void
PropertyTree::emptyShapeChange(uint32 oldEmptyShape, uint32 newEmptyShape)
{
    if (oldEmptyShape == newEmptyShape)
        return;

    PropertyRootEntry *rent = (PropertyRootEntry *) hash.entryStore;
    PropertyRootEntry *rend = rent + JS_DHASH_TABLE_SIZE(&hash);

    while (rent < rend) {
        if (rent->emptyShape == oldEmptyShape)
            rent->newEmptyShape = newEmptyShape;
        rent++;
    }

    ++emptyShapeChanges;
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
 * We use cx->runtime->gcLock, not ...->rtLock, to avoid nesting the former
 * inside the latter in js_GenerateShape below.
 */
JSScopeProperty *
PropertyTree::getChild(JSContext *cx, JSScopeProperty *parent, uint32 shape,
                       const JSScopeProperty &child)
{
    PropertyRootEntry *rent;
    JSScopeProperty *sprop;

    if (!parent) {
        PropertyRootKey rkey(&child, shape);

        JS_LOCK_GC(cx->runtime);
        rent = (PropertyRootEntry *) JS_DHashTableOperate(&hash, &rkey, JS_DHASH_ADD);
        if (!rent) {
            JS_UNLOCK_GC(cx->runtime);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }

        sprop = rent->firstProp;
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
        rent = NULL;
        sprop = parent->kids;
        if (sprop) {
            if (!KIDS_IS_CHUNKY(sprop)) {
                if (sprop->matches(&child))
                    return sprop;
            } else {
                PropTreeKidsChunk *chunk = KIDS_TO_CHUNK(sprop);

                if (JSDHashTable *table = chunk->table) {
                    JS_LOCK_GC(cx->runtime);
                    JSPropertyTreeEntry *entry = (JSPropertyTreeEntry *)
                        JS_DHashTableOperate(table, &child, JS_DHASH_LOOKUP);
                    sprop = entry->child;
                    if (sprop)
                        goto out;
                    goto locked_not_found;
                }

                uintN n = 0;
                do {
                    for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                        sprop = chunk->kids[i];
                        if (!sprop) {
                            n += i;
                            if (n >= CHUNK_HASH_THRESHOLD) {
                                chunk = KIDS_TO_CHUNK(parent->kids);
                                if (!chunk->table) {
                                    JSDHashTable *table = HashChunks(chunk, n);
                                    if (!table) {
                                        JS_ReportOutOfMemory(cx);
                                        return NULL;
                                    }

                                    JS_LOCK_GC(cx->runtime);
                                    if (chunk->table)
                                        JS_DHashTableDestroy(table);
                                    else
                                        chunk->table = table;
                                    goto locked_not_found;
                                }
                            }
                            goto not_found;
                        }

                        if (sprop->matches(&child))
                            return sprop;
                    }
                    n += MAX_KIDS_PER_CHUNK;
                } while ((chunk = chunk->next) != NULL);
            }
        }

      not_found:
        JS_LOCK_GC(cx->runtime);
    }

  locked_not_found:
    sprop = newScopeProperty(cx, true);
    if (!sprop)
        return NULL;

    new (sprop) JSScopeProperty(child.id, child.rawGetter, child.rawSetter, child.slot,
                                child.attrs, child.flags, child.shortid);
    sprop->parent = sprop->kids = NULL;
    sprop->shape = js_GenerateShape(cx, true);

    if (!parent) {
        rent->firstProp = sprop;
        rent->emptyShape = shape;
        rent->newEmptyShape = 0;
    } else {
        if (!PropertyTree::insertChild(cx, parent, sprop))
            return NULL;
    }

  out:
    JS_UNLOCK_GC(cx->runtime);
    return sprop;
}

#ifdef DEBUG

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
    PropertyRootEntry *rent = (PropertyRootEntry *)hdr;
    JSBasicStats *bs = (JSBasicStats *)arg;

    MeterPropertyTree(bs, rent->firstProp);
    return JS_DHASH_NEXT;
}

void
JSScopeProperty::dump(JSContext *cx, FILE *fp)
{
    JS_ASSERT(!JSVAL_IS_NULL(id));

    jsval idval = ID_TO_VALUE(id);
    if (JSVAL_IS_INT(idval)) {
        fprintf(fp, "[%ld]", (long) JSVAL_TO_INT(idval));
    } else {
        JSString *str;
        if (JSVAL_IS_STRING(idval)) {
            str = JSVAL_TO_STRING(idval);
        } else {
            JS_ASSERT(JSVAL_IS_OBJECT(idval));
            str = js_ValueToString(cx, idval);
            fputs("object ", fp);
        }
        if (!str)
            fputs("<error>", fp);
        else
            js_FileEscapedString(fp, str, '"');
    }

    fprintf(fp, " g/s %p/%p slot %u attrs %x ",
            JS_FUNC_TO_DATA_PTR(void *, rawGetter),
            JS_FUNC_TO_DATA_PTR(void *, rawSetter),
            slot, attrs);
    if (attrs) {
        int first = 1;
        fputs("(", fp);
#define DUMP_ATTR(name, display) if (attrs & JSPROP_##name) fputs(" " #display + first, fp), first = 0
        DUMP_ATTR(ENUMERATE, enumerate);
        DUMP_ATTR(READONLY, readonly);
        DUMP_ATTR(PERMANENT, permanent);
        DUMP_ATTR(GETTER, getter);
        DUMP_ATTR(SETTER, setter);
        DUMP_ATTR(SHARED, shared);
#undef  DUMP_ATTR
        fputs(") ", fp);
    }

    fprintf(fp, "flags %x ", flags);
    if (flags) {
        int first = 1;
        fputs("(", fp);
#define DUMP_FLAG(name, display) if (flags & name) fputs(" " #display + first, fp), first = 0
        DUMP_FLAG(ALIAS, alias);
        DUMP_FLAG(HAS_SHORTID, has_shortid);
        DUMP_FLAG(METHOD, method);
        DUMP_FLAG(MARK, mark);
        DUMP_FLAG(SHAPE_REGEN, shape_regen);
        DUMP_FLAG(IN_DICTIONARY, in_dictionary);
#undef  DUMP_FLAG
        fputs(") ", fp);
    }

    fprintf(fp, "shortid %d\n", shortid);
}

void
JSScopeProperty::dumpSubtree(JSContext *cx, int level, FILE *fp)
{
    fprintf(fp, "%*sid ", level, "");
    dump(cx, fp);

    if (kids) {
        ++level;
        if (KIDS_IS_CHUNKY(kids)) {
            PropTreeKidsChunk *chunk = KIDS_TO_CHUNK(kids);
            do {
                for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                    JSScopeProperty *kid = chunk->kids[i];
                    if (!kid)
                        break;
                    JS_ASSERT(kid->parent == this);
                    kid->dumpSubtree(cx, level, fp);
                }
            } while ((chunk = chunk->next) != NULL);
        } else {
            JSScopeProperty *kid = kids;
            JS_ASSERT(kid->parent == this);
            kid->dumpSubtree(cx, level, fp);
        }
    }
}

#endif /* DEBUG */

static void
OrphanNodeKids(JSContext *cx, JSScopeProperty *sprop)
{
    JSScopeProperty *kids = sprop->kids;

    JS_ASSERT(kids);
    sprop->kids = NULL;

    /*
     * The grandparent must have either no kids or (still, after the
     * removeChild call above) chunky kids.
     */
    JS_ASSERT(!sprop->parent || !sprop->parent->kids ||
              KIDS_IS_CHUNKY(sprop->parent->kids));

    if (KIDS_IS_CHUNKY(kids)) {
        PropTreeKidsChunk *chunk = KIDS_TO_CHUNK(kids);

        do {
            for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                JSScopeProperty *kid = chunk->kids[i];
                if (!kid)
                    break;

                if (!JSVAL_IS_NULL(kid->id)) {
                    JS_ASSERT(kid->parent == sprop);
                    kid->parent = NULL;
                }
            }
        } while ((chunk = DestroyPropTreeKidsChunk(cx, chunk)) != NULL);
    } else {
        JSScopeProperty *kid = kids;

        if (!JSVAL_IS_NULL(kid->id)) {
            JS_ASSERT(kid->parent == sprop);
            kid->parent = NULL;
        }
    }
}

JSDHashOperator
js::RemoveNodeIfDead(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number, void *arg)
{
    PropertyRootEntry *rent = (PropertyRootEntry *)hdr;
    JSScopeProperty *sprop = rent->firstProp;

    JS_ASSERT(!sprop->parent);
    if (!sprop->marked()) {
        if (sprop->kids)
            OrphanNodeKids((JSContext *)arg, sprop);
        return JS_DHASH_REMOVE;
    }
    return JS_DHASH_NEXT;
}

void
js::SweepScopeProperties(JSContext *cx)
{
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
        MeterKidCount(&bs, JS_PROPERTY_TREE(cx).hash.entryCount);
        JS_DHashTableEnumerate(&JS_PROPERTY_TREE(cx).hash, js_MeterPropertyTree, &bs);

        double props, nodes, mean, sigma;

        props = cx->runtime->liveScopePropsPreSweep;
        nodes = cx->runtime->livePropTreeNodes;
        JS_ASSERT(nodes == bs.sum);
        mean = JS_MeanAndStdDevBS(&bs, &sigma);

        fprintf(logfp,
                "props %g nodes %g beta %g meankids %g sigma %g max %u\n",
                props, nodes, nodes / props, mean, sigma, bs.max);

        JS_DumpHistogram(&bs, logfp);
    }
#endif

    /*
     * First, remove unmarked nodes from JS_PROPERTY_TREE(cx).hash. This table
     * requires special handling up front, rather than removal during regular
     * heap sweeping, because we cannot find an entry in it from the firstProp
     * node pointer alone -- we would need the emptyShape too.
     *
     * Rather than encode emptyShape in firstProp somehow (a tagged overlay on
     * parent, perhaps, but that would slow down JSScope::search and other hot
     * paths), we simply orphan kids of garbage nodes in the property tree's
     * root-ply before sweeping the node heap.
     */
    JS_DHashTableEnumerate(&JS_PROPERTY_TREE(cx).hash, RemoveNodeIfDead, cx);

    /*
     * Second, if any empty scopes have been reshaped, rehash the root ply of
     * this tree using the new empty shape numbers as key-halves. If we run out
     * of memory trying to allocate the new hash, disable the property cache by
     * setting SHAPE_OVERFLOW_BIT in rt->shapeGen. The next GC will therefore
     * renumber shapes as well as (we hope, eventually) free sufficient memory
     * for a successful re-run through this code.
     */
    if (JS_PROPERTY_TREE(cx).emptyShapeChanges) {
        JSDHashTable &oldHash = JS_PROPERTY_TREE(cx).hash;
        uint32 tableSize = JS_DHASH_TABLE_SIZE(&oldHash);
        JSDHashTable newHash;

        if (!JS_DHashTableInit(&newHash, &PropertyRootHashOps, NULL,
                               sizeof(PropertyRootEntry), tableSize)) {
            cx->runtime->shapeGen |= SHAPE_OVERFLOW_BIT;
        } else {
            PropertyRootEntry *rent = (PropertyRootEntry *) oldHash.entryStore;
            PropertyRootEntry *rend = rent + tableSize;

            while (rent < rend) {
                if (rent->firstProp) {
                    uint32 emptyShape = rent->newEmptyShape;
                    if (emptyShape == 0)
                        emptyShape = rent->emptyShape;

                    PropertyRootKey rkey(rent->firstProp, emptyShape);
                    PropertyRootEntry *newRent =
                        (PropertyRootEntry *) JS_DHashTableOperate(&newHash, &rkey, JS_DHASH_ADD);

                    newRent->firstProp = rent->firstProp;
                    newRent->emptyShape = emptyShape;
                    newRent->newEmptyShape = 0;
                }
                rent++;
            }

            JS_ASSERT(newHash.generation == 0);
            JS_DHashTableFinish(&oldHash);
            JS_PROPERTY_TREE(cx).hash = newHash;
            JS_PROPERTY_TREE(cx).emptyShapeChanges = 0;
        }
    }

    /*
     * Third, sweep the heap clean of all unmarked nodes. Here we will find
     * nodes already GC'ed from the root ply, but we will avoid re-orphaning
     * their kids, because the kids member will already be null.
     */
    JSArena **ap = &JS_PROPERTY_TREE(cx).arenaPool.first.next;
    while (JSArena *a = *ap) {
        JSScopeProperty *limit = (JSScopeProperty *) a->avail;
        uintN liveCount = 0;

        for (JSScopeProperty *sprop = (JSScopeProperty *) a->base; sprop < limit; sprop++) {
            /* If the id is null, sprop is already on the freelist. */
            if (JSVAL_IS_NULL(sprop->id))
                continue;

            /*
             * If the mark bit is set, sprop is alive, so clear the mark bit
             * and continue the while loop.
             *
             * Regenerate sprop->shape if it hasn't already been refreshed
             * during the mark phase, when live scopes' lastProp members are
             * followed to update both scope->shape and lastProp->shape.
             */
            if (sprop->marked()) {
                sprop->clearMark();
                if (cx->runtime->gcRegenShapes) {
                    if (sprop->hasRegenFlag())
                        sprop->clearRegenFlag();
                    else
                        sprop->shape = js_RegenerateShapeForGC(cx);
                }
                liveCount++;
                continue;
            }

            if (!sprop->inDictionary()) {
                /*
                 * Here, sprop is garbage to collect, but its parent might not
                 * be, so we may have to remove it from its parent's kids chunk
                 * list or kid singleton pointer set.
                 *
                 * Without a separate mark-clearing pass, we can't tell whether
                 * sprop->parent is live at this point, so we must remove sprop
                 * if its parent member is non-null. A saving grace: if sprop's
                 * parent is dead and swept by this point, sprop->parent will
                 * be null -- in the next paragraph, we null all of a property
                 * tree node's kids' parent links when sweeping that node.
                 */
                if (sprop->parent)
                    JS_PROPERTY_TREE(cx).removeChild(cx, sprop);

                if (sprop->kids)
                    OrphanNodeKids(cx, sprop);
            }

            /*
             * Note that JSScopeProperty::insertFree nulls sprop->id so we know
             * that sprop is on the freelist.
             */
            sprop->insertFree(JS_PROPERTY_TREE(cx).freeList);
            JS_RUNTIME_UNMETER(cx->runtime, livePropTreeNodes);
        }

        /* If a contains no live properties, return it to the malloc heap. */
        if (liveCount == 0) {
            for (JSScopeProperty *sprop = (JSScopeProperty *) a->base; sprop < limit; sprop++)
                sprop->removeFree();
            JS_ARENA_DESTROY(&JS_PROPERTY_TREE(cx).arenaPool, a, ap);
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
                (unsigned long) cx->runtime->gcNumber);

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
        JS_snprintf(pathname, sizeof pathname, "%s.%lu",
                    filename, (unsigned long)cx->runtime->gcNumber);
        FILE *dumpfp = fopen(pathname, "w");
        if (dumpfp) {
            PropertyRootEntry *rent = (PropertyRootEntry *) JS_PROPERTY_TREE(cx).hash.entryStore;
            PropertyRootEntry *rend = rent + JS_DHASH_TABLE_SIZE(&JS_PROPERTY_TREE(cx).hash);

            while (rent < rend) {
                if (rent->firstProp) {
                    fprintf(dumpfp, "emptyShape %u ", rent->emptyShape);
                    rent->firstProp->dumpSubtree(cx, 0, dumpfp);
                }
                rent++;
            }
            fclose(dumpfp);
        }
    }
#endif /* DEBUG */
}

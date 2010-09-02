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

#include <new>

#include "jstypes.h"
#include "jsarena.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jspropertytree.h"
#include "jsscope.h"

#include "jsobjinlines.h"
#include "jsscopeinlines.h"

using namespace js;

inline HashNumber
ShapeHasher::hash(const Lookup l)
{
    return l->hash();
}

inline bool
ShapeHasher::match(const Key k, const Lookup l)
{
    return l->matches(k);
}

bool
PropertyTree::init()
{
    JS_InitArenaPool(&arenaPool, "properties",
                     256 * sizeof(Shape), sizeof(void *), NULL);
    return true;
}

void
PropertyTree::finish()
{
    JS_FinishArenaPool(&arenaPool);
}

/*
 * NB: Called with cx->runtime->gcLock held if gcLocked is true.
 * On failure, return null after unlocking the GC and reporting out of memory.
 */
Shape *
PropertyTree::newShape(JSContext *cx, bool gcLocked)
{
    Shape *shape;

    if (!gcLocked)
        JS_LOCK_GC(cx->runtime);
    shape = freeList;
    if (shape) {
        shape->removeFree();
    } else {
        JS_ARENA_ALLOCATE_CAST(shape, Shape *, &arenaPool, sizeof(Shape));
        if (!shape) {
            JS_UNLOCK_GC(cx->runtime);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }
    if (!gcLocked)
        JS_UNLOCK_GC(cx->runtime);

    JS_RUNTIME_METER(cx->runtime, livePropTreeNodes);
    JS_RUNTIME_METER(cx->runtime, totalPropTreeNodes);
    return shape;
}

/*
 * NB: Called with cx->runtime->gcLock held, always.
 * On failure, return null after unlocking the GC and reporting out of memory.
 */
KidsChunk *
KidsChunk::create(JSContext *cx)
{
    KidsChunk *chunk;

    chunk = (KidsChunk *) js_calloc(sizeof *chunk);
    if (!chunk) {
        JS_UNLOCK_GC(cx->runtime);
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    JS_RUNTIME_METER(cx->runtime, propTreeKidsChunks);
    return chunk;
}

KidsChunk *
KidsChunk::destroy(JSContext *cx, KidsChunk *chunk)
{
    JS_RUNTIME_UNMETER(cx->runtime, propTreeKidsChunks);

    KidsChunk *nextChunk = chunk->next;
    js_free(chunk);
    return nextChunk;
}

/*
 * NB: Called with cx->runtime->gcLock held, always.
 * On failure, return null after unlocking the GC and reporting out of memory.
 */
bool
PropertyTree::insertChild(JSContext *cx, Shape *parent, Shape *child)
{
    JS_ASSERT(!parent->inDictionary());
    JS_ASSERT(!child->parent);
    JS_ASSERT(!child->inDictionary());
    JS_ASSERT(!JSID_IS_VOID(parent->id));
    JS_ASSERT(!JSID_IS_VOID(child->id));

    child->setParent(parent);

    KidsPointer *kidp = &parent->kids;
    if (kidp->isNull()) {
        kidp->setShape(child);
        return true;
    }

    Shape *shape;

    if (kidp->isShape()) {
        shape = kidp->toShape();
        JS_ASSERT(shape != child);
        if (shape->matches(child)) {
            /*
             * Duplicate child created while racing to getChild on the same
             * node label. See PropertyTree::getChild, further below.
             */
            JS_RUNTIME_METER(cx->runtime, duplicatePropTreeNodes);
        }

        KidsChunk *chunk = KidsChunk::create(cx);
        if (!chunk)
            return false;
        parent->kids.setChunk(chunk);
        chunk->kids[0] = shape;
        chunk->kids[1] = child;
        return true;
    }

    if (kidp->isChunk()) {
        KidsChunk **chunkp;
        KidsChunk *chunk = kidp->toChunk();

        do {
            for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                shape = chunk->kids[i];
                if (!shape) {
                    chunk->kids[i] = child;
                    return true;
                }

                JS_ASSERT(shape != child);
                if (shape->matches(child)) {
                    /*
                     * Duplicate child, see comment above. In this case, we
                     * must let the duplicate be inserted at this level in the
                     * tree, so we keep iterating, looking for an empty slot in
                     * which to insert.
                     */
                    JS_ASSERT(shape != child);
                    JS_RUNTIME_METER(cx->runtime, duplicatePropTreeNodes);
                }
            }
            chunkp = &chunk->next;
        } while ((chunk = *chunkp) != NULL);

        chunk = KidsChunk::create(cx);
        if (!chunk)
            return false;
        *chunkp = chunk;
        chunk->kids[0] = child;
        return true;
    }
   
    KidsHash *hash = kidp->toHash();
    KidsHash::AddPtr addPtr = hash->lookupForAdd(child);
    if (!addPtr) {
        if (!hash->add(addPtr, child))
            return false;
    } else {
        // FIXME ignore duplicate child case here, going thread-local soon!
    }
    return true;
}

/* NB: Called with cx->runtime->gcLock held. */
void
PropertyTree::removeChild(JSContext *cx, Shape *child)
{
    JS_ASSERT(!child->inDictionary());

    Shape *parent = child->parent;
    JS_ASSERT(parent);
    JS_ASSERT(!JSID_IS_VOID(parent->id));

    KidsPointer *kidp = &parent->kids;
    if (kidp->isShape()) {
        Shape *kid = kidp->toShape();
        if (kid == child)
            parent->kids.setNull();
        return;
    }

    if (kidp->isChunk()) {
        KidsChunk *list = kidp->toChunk();
        KidsChunk *chunk = list;
        KidsChunk **chunkp = &list;

        do {
            for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                if (chunk->kids[i] == child) {
                    KidsChunk *lastChunk = chunk;

                    uintN j;
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
                            parent->kids.setNull();
                        KidsChunk::destroy(cx, lastChunk);
                    }
                    return;
                }
            }

            chunkp = &chunk->next;
        } while ((chunk = *chunkp) != NULL);
        return;
    }

    kidp->toHash()->remove(child);
}

static KidsHash *
HashChunks(KidsChunk *chunk, uintN n)
{
    void *mem = js_malloc(sizeof(KidsHash));
    if (!mem)
        return NULL;

    KidsHash *hash = new (mem) KidsHash();
    if (!hash->init(n)) {
        js_free(hash);
        return NULL;
    }

    do {
        for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
            Shape *shape = chunk->kids[i];
            if (!shape)
                break;
            KidsHash::AddPtr addPtr = hash->lookupForAdd(shape);
            if (!addPtr) {
                /*
                 * Infallible, we right-sized via hash->init(n) just above.
                 * Assert just in case jshashtable.h ever regresses.
                 */
                JS_ALWAYS_TRUE(hash->add(addPtr, shape));
            } else {
                /*
                 * Duplicate child case, we don't handle this race,
                 * multi-threaded shapes are going away...
                 */
            }
        }
    } while ((chunk = chunk->next) != NULL);
    return hash;
}

/*
 * Called without cx->runtime->gcLock held. This function acquires that lock
 * only when inserting a new child.  Thus there may be races to find or add a
 * node that result in duplicates.  We expect such races to be rare!
 *
 * We use cx->runtime->gcLock, not ...->rtLock, to avoid nesting the former
 * inside the latter in js_GenerateShape below.
 */
Shape *
PropertyTree::getChild(JSContext *cx, Shape *parent, const Shape &child)
{
    Shape *shape;

    JS_ASSERT(parent);
    JS_ASSERT(!JSID_IS_VOID(parent->id));

    /*
     * Because chunks are appended at the end and never deleted except by
     * the GC, we can search without taking the runtime's GC lock.  We may
     * miss a matching shape added by another thread, and make a duplicate
     * one, but that is an unlikely, therefore small, cost.  The property
     * tree has extremely low fan-out below its root in popular embeddings
     * with real-world workloads.
     *
     * Patterns such as defining closures that capture a constructor's
     * environment as getters or setters on the new object that is passed
     * in as |this| can significantly increase fan-out below the property
     * tree root -- see bug 335700 for details.
     */
    KidsPointer *kidp = &parent->kids;
    if (!kidp->isNull()) {
        if (kidp->isShape()) {
            shape = kidp->toShape();
            if (shape->matches(&child))
                return shape;
        } else if (kidp->isChunk()) {
            KidsChunk *chunk = kidp->toChunk();

            uintN n = 0;
            do {
                for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                    shape = chunk->kids[i];
                    if (!shape) {
                        n += i;
                        if (n >= CHUNK_HASH_THRESHOLD) {
                            /*
                             * kidp->isChunk() was true, but if we're racing it
                             * may not be by this point. FIXME: thread "safety"
                             * is for the birds!
                             */
                            if (!kidp->isHash()) {
                                chunk = kidp->toChunk();

                                KidsHash *hash = HashChunks(chunk, n);
                                if (!hash) {
                                    JS_ReportOutOfMemory(cx);
                                    return NULL;
                                }

                                JS_LOCK_GC(cx->runtime);
                                if (kidp->isHash()) {
                                    hash->KidsHash::~KidsHash();
                                    js_free(hash);
                                } else {
                                    // FIXME unsafe race with kidp->is/toChunk() above.
                                    // But this is all going single-threaded soon...
                                    while (chunk)
                                        chunk = KidsChunk::destroy(cx, chunk);
                                    kidp->setHash(hash);
                                }
                                goto locked_not_found;
                            }
                        }
                        goto not_found;
                    }

                    if (shape->matches(&child))
                        return shape;
                }
                n += MAX_KIDS_PER_CHUNK;
            } while ((chunk = chunk->next) != NULL);
        } else {
            JS_LOCK_GC(cx->runtime);
            shape = *kidp->toHash()->lookup(&child);
            if (shape)
                goto out;
            goto locked_not_found;
        }
    }

  not_found:
    JS_LOCK_GC(cx->runtime);

  locked_not_found:
    shape = newShape(cx, true);
    if (!shape)
        return NULL;

    new (shape) Shape(child.id, child.rawGetter, child.rawSetter, child.slot, child.attrs,
                      child.flags, child.shortid, js_GenerateShape(cx, true));

    if (!insertChild(cx, parent, shape))
        return NULL;

  out:
    JS_UNLOCK_GC(cx->runtime);
    return shape;
}

#ifdef DEBUG
void
Shape::dump(JSContext *cx, FILE *fp) const
{
    JS_ASSERT(!JSID_IS_VOID(id));

    if (JSID_IS_INT(id)) {
        fprintf(fp, "[%ld]", (long) JSID_TO_INT(id));
    } else {
        JSString *str;
        if (JSID_IS_ATOM(id)) {
            str = JSID_TO_STRING(id);
        } else {
            JS_ASSERT(JSID_IS_OBJECT(id));
            str = js_ValueToString(cx, IdToValue(id));
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
#endif

#ifdef DEBUG

static void
MeterKidCount(JSBasicStats *bs, uintN nkids)
{
    JS_BASIC_STATS_ACCUM(bs, nkids);
}

void
js::PropertyTree::meter(JSBasicStats *bs, Shape *node)
{
    uintN nkids = 0;
    const KidsPointer &kids = node->kids;
    if (!kids.isNull()) {
        if (kids.isShape()) {
            meter(bs, kids.toShape());
            nkids = 1;
        } else if (kids.isChunk()) {
            for (KidsChunk *chunk = kids.toChunk(); chunk; chunk = chunk->next) {
                for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                    Shape *kid = chunk->kids[i];
                    if (!kid)
                        break;
                    meter(bs, kid);
                    nkids++;
                }
            }
        } else {
            const KidsHash &hash = *kids.toHash();
            for (KidsHash::Range range = hash.all(); !range.empty(); range.popFront()) {
                Shape *kid = range.front();

                meter(bs, kid);
                nkids++;
            }
        }
    }

    MeterKidCount(bs, nkids);
}

void
Shape::dumpSubtree(JSContext *cx, int level, FILE *fp) const
{
    if (!parent) {
        JS_ASSERT(level == 0);
        JS_ASSERT(JSID_IS_EMPTY(id));
        fprintf(fp, "class %s emptyShape %u\n", clasp->name, shape);
    } else {
        fprintf(fp, "%*sid ", level, "");
        dump(cx, fp);
    }

    if (!kids.isNull()) {
        ++level;
        if (kids.isShape()) {
            Shape *kid = kids.toShape();
            JS_ASSERT(kid->parent == this);
            kid->dumpSubtree(cx, level, fp);
        } else if (kids.isChunk()) {
            KidsChunk *chunk = kids.toChunk();
            do {
                for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                    Shape *kid = chunk->kids[i];
                    if (!kid)
                        break;
                    JS_ASSERT(kid->parent == this);
                    kid->dumpSubtree(cx, level, fp);
                }
            } while ((chunk = chunk->next) != NULL);
        } else {
            const KidsHash &hash = *kids.toHash();
            for (KidsHash::Range range = hash.all(); !range.empty(); range.popFront()) {
                Shape *kid = range.front();

                JS_ASSERT(kid->parent == this);
                kid->dumpSubtree(cx, level, fp);
            }
        }
    }
}

#endif /* DEBUG */

JS_ALWAYS_INLINE void
js::PropertyTree::orphanKids(JSContext *cx, Shape *shape)
{
    KidsPointer *kidp = &shape->kids;

    JS_ASSERT(!kidp->isNull());

    /*
     * Note that JS_PROPERTY_TREE(cx).removeChild(cx, shape) precedes the call
     * to orphanKids in sweepShapes, below. Therefore the grandparent must have
     * either no kids left, or else space in chunks or a hash for more than one
     * kid.
     */
    JS_ASSERT_IF(shape->parent, !shape->parent->kids.isShape());

    if (kidp->isShape()) {
        Shape *kid = kidp->toShape();

        if (!JSID_IS_VOID(kid->id)) {
            JS_ASSERT(kid->parent == shape);
            kid->parent = NULL;
        }
    } else if (kidp->isChunk()) {
        KidsChunk *chunk = kidp->toChunk();

        do {
            for (uintN i = 0; i < MAX_KIDS_PER_CHUNK; i++) {
                Shape *kid = chunk->kids[i];
                if (!kid)
                    break;

                if (!JSID_IS_VOID(kid->id)) {
                    JS_ASSERT(kid->parent == shape);
                    kid->parent = NULL;
                }
            }
        } while ((chunk = KidsChunk::destroy(cx, chunk)) != NULL);
    } else {
        KidsHash *hash = kidp->toHash();

        for (KidsHash::Range range = hash->all(); !range.empty(); range.popFront()) {
            Shape *kid = range.front();
            if (!JSID_IS_VOID(kid->id)) {
                JS_ASSERT(kid->parent == shape);
                kid->parent = NULL;
            }
        }

        hash->KidsHash::~KidsHash();
        js_free(hash);
    }

    kidp->setNull();
}

void
js::PropertyTree::sweepShapes(JSContext *cx)
{
#ifdef DEBUG
    JSBasicStats bs;
    uint32 livePropCapacity = 0, totalLiveCount = 0;
    static FILE *logfp;
    if (!logfp) {
        if (const char *filename = cx->runtime->propTreeStatFilename)
            logfp = fopen(filename, "w");
    }

    if (logfp) {
        JS_BASIC_STATS_INIT(&bs);

        uint32 empties;
        {
            typedef JSRuntime::EmptyShapeSet HS;

            HS &h = cx->runtime->emptyShapes;
            empties = h.count();
            MeterKidCount(&bs, empties);
            for (HS::Range r = h.all(); !r.empty(); r.popFront())
                meter(&bs, r.front());
        }

        double props = cx->runtime->liveObjectPropsPreSweep;
        double nodes = cx->runtime->livePropTreeNodes;
        double dicts = cx->runtime->liveDictModeNodes;

        /* Empty scope nodes are never hashed, so subtract them from nodes. */
        JS_ASSERT(nodes - dicts == bs.sum);
        nodes -= empties;

        double sigma;
        double mean = JS_MeanAndStdDevBS(&bs, &sigma);

        fprintf(logfp,
                "props %g nodes %g (dicts %g) beta %g meankids %g sigma %g max %u\n",
                props, nodes, dicts, nodes / props, mean, sigma, bs.max);

        JS_DumpHistogram(&bs, logfp);
    }
#endif

    /*
     * Sweep the heap clean of all unmarked nodes. Here we will find nodes
     * already GC'ed from the root ply, but we will avoid re-orphaning their
     * kids, because the kids member will already be null.
     */
    JSArena **ap = &JS_PROPERTY_TREE(cx).arenaPool.first.next;
    while (JSArena *a = *ap) {
        Shape *limit = (Shape *) a->avail;
        uintN liveCount = 0;

        for (Shape *shape = (Shape *) a->base; shape < limit; shape++) {
            /* If the id is null, shape is already on the freelist. */
            if (JSID_IS_VOID(shape->id))
                continue;

            /*
             * If the mark bit is set, shape is alive, so clear the mark bit
             * and continue the while loop.
             *
             * Regenerate shape->shape if it hasn't already been refreshed
             * during the mark phase, when live scopes' lastProp members are
             * followed to update both scope->shape and lastProp->shape.
             */
            if (shape->marked()) {
                shape->clearMark();
                if (cx->runtime->gcRegenShapes) {
                    if (shape->hasRegenFlag())
                        shape->clearRegenFlag();
                    else
                        shape->shape = js_RegenerateShapeForGC(cx);
                }
                liveCount++;
                continue;
            }

#ifdef DEBUG
            if ((shape->flags & Shape::SHARED_EMPTY) &&
                cx->runtime->meterEmptyShapes()) {
                cx->runtime->emptyShapes.remove((EmptyShape *) shape);
            }
#endif

            if (shape->inDictionary()) {
                JS_RUNTIME_UNMETER(cx->runtime, liveDictModeNodes);
            } else {
                /*
                 * Here, shape is garbage to collect, but its parent might not
                 * be, so we may have to remove it from its parent's kids hash,
                 * chunk list, or kid singleton pointer set.
                 *
                 * Without a separate mark-clearing pass, we can't tell whether
                 * shape->parent is live at this point, so we must remove shape
                 * if its parent member is non-null. A saving grace: if shape's
                 * parent is dead and swept by this point, shape->parent will
                 * be null -- in the next paragraph, we null all of a property
                 * tree node's kids' parent links when sweeping that node.
                 */
                if (shape->parent)
                    JS_PROPERTY_TREE(cx).removeChild(cx, shape);

                if (!shape->kids.isNull())
                    orphanKids(cx, shape);
            }

            /*
             * Note that Shape::insertFree nulls shape->id so we know that
             * shape is on the freelist.
             */
            shape->freeTable(cx);
            shape->insertFree(&JS_PROPERTY_TREE(cx).freeList);
            JS_RUNTIME_UNMETER(cx->runtime, livePropTreeNodes);
        }

        /* If a contains no live properties, return it to the malloc heap. */
        if (liveCount == 0) {
            for (Shape *shape = (Shape *) a->base; shape < limit; shape++)
                shape->removeFree();
            JS_ARENA_DESTROY(&JS_PROPERTY_TREE(cx).arenaPool, a, ap);
        } else {
#ifdef DEBUG
            livePropCapacity += limit - (Shape *) a->base;
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
                "  searches:        %6u\n"
                "  hits:            %6u %5.2f%% of searches\n"
                "  misses:          %6u %5.2f%%\n"
                "  hashes:          %6u %5.2f%%\n"
                "  hashHits:        %6u %5.2f%% (%5.2f%% of hashes)\n"
                "  hashMisses:      %6u %5.2f%% (%5.2f%%)\n"
                "  steps:           %6u %5.2f%% (%5.2f%%)\n"
                "  stepHits:        %6u %5.2f%% (%5.2f%%)\n"
                "  stepMisses:      %6u %5.2f%% (%5.2f%%)\n"
                "  initSearches:    %6u\n"
                "  changeSearches:  %6u\n"
                "  tableAllocFails: %6u\n"
                "  toDictFails:     %6u\n"
                "  wrapWatchFails:  %6u\n"
                "  adds:            %6u\n"
                "  addFails:        %6u\n"
                "  puts:            %6u\n"
                "  redundantPuts:   %6u\n"
                "  putFails:        %6u\n"
                "  changes:         %6u\n"
                "  changeFails:     %6u\n"
                "  compresses:      %6u\n"
                "  grows:           %6u\n"
                "  removes:         %6u\n"
                "  removeFrees:     %6u\n"
                "  uselessRemoves:  %6u\n"
                "  shrinks:         %6u\n",
                js_scope_stats.searches,
                js_scope_stats.hits, RATE(hits, searches),
                js_scope_stats.misses, RATE(misses, searches),
                js_scope_stats.hashes, RATE(hashes, searches),
                js_scope_stats.hashHits, RATE(hashHits, searches), RATE(hashHits, hashes),
                js_scope_stats.hashMisses, RATE(hashMisses, searches), RATE(hashMisses, hashes),
                js_scope_stats.steps, RATE(steps, searches), RATE(steps, hashes),
                js_scope_stats.stepHits, RATE(stepHits, searches), RATE(stepHits, hashes),
                js_scope_stats.stepMisses, RATE(stepMisses, searches), RATE(stepMisses, hashes),
                js_scope_stats.initSearches,
                js_scope_stats.changeSearches,
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

    if (const char *filename = cx->runtime->propTreeDumpFilename) {
        char pathname[1024];
        JS_snprintf(pathname, sizeof pathname, "%s.%lu",
                    filename, (unsigned long)cx->runtime->gcNumber);
        FILE *dumpfp = fopen(pathname, "w");
        if (dumpfp) {
            typedef JSRuntime::EmptyShapeSet HS;

            HS &h = cx->runtime->emptyShapes;
            for (HS::Range r = h.all(); !r.empty(); r.popFront()) {
                Shape *empty = r.front();
                empty->dumpSubtree(cx, 0, dumpfp);
                putc('\n', dumpfp);
            }

            fclose(dumpfp);
        }
    }
#endif /* DEBUG */
}

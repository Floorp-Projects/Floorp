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
 * JS Mark-and-Sweep Garbage Collector.
 *
 * This GC allocates fixed-sized things with sizes up to GC_NBYTES_MAX (see
 * jsgc.h). It allocates from a special GC arena pool with each arena allocated
 * using malloc. It uses an ideally parallel array of flag bytes to hold the
 * mark bit, finalizer type index, etc.
 *
 * XXX swizzle page to freelist for better locality of reference
 */
#include <math.h>
#include <string.h>     /* for memset used when DEBUG */

#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jshash.h"
#include "jsclist.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscompartment.h"
#include "jscrashreport.h"
#include "jscrashformat.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcchunk.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsprobes.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jswatchpoint.h"
#include "jsweakmap.h"
#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "frontend/Parser.h"
#include "methodjit/MethodJIT.h"
#include "vm/Debugger.h"
#include "vm/String.h"

#include "jsobjinlines.h"

#include "vm/CallObject-inl.h"
#include "vm/String-inl.h"

#ifdef MOZ_VALGRIND
# define JS_VALGRIND
#endif
#ifdef JS_VALGRIND
# include <valgrind/memcheck.h>
#endif

using namespace mozilla;
using namespace js;
using namespace js::gc;

namespace js {
namespace gc {

#ifdef JS_GC_ZEAL
static void
StartVerifyBarriers(JSContext *cx);

static void
EndVerifyBarriers(JSContext *cx);
#endif

/* This array should be const, but that doesn't link right under GCC. */
AllocKind slotsToThingKind[] = {
    /* 0 */  FINALIZE_OBJECT0,  FINALIZE_OBJECT2,  FINALIZE_OBJECT2,  FINALIZE_OBJECT4,
    /* 4 */  FINALIZE_OBJECT4,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,
    /* 8 */  FINALIZE_OBJECT8,  FINALIZE_OBJECT12, FINALIZE_OBJECT12, FINALIZE_OBJECT12,
    /* 12 */ FINALIZE_OBJECT12, FINALIZE_OBJECT16, FINALIZE_OBJECT16, FINALIZE_OBJECT16,
    /* 16 */ FINALIZE_OBJECT16
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(slotsToThingKind) == SLOTS_TO_THING_KIND_LIMIT);

const uint32 Arena::ThingSizes[] = {
    sizeof(JSObject),           /* FINALIZE_OBJECT0             */
    sizeof(JSObject),           /* FINALIZE_OBJECT0_BACKGROUND  */
    sizeof(JSObject_Slots2),    /* FINALIZE_OBJECT2             */
    sizeof(JSObject_Slots2),    /* FINALIZE_OBJECT2_BACKGROUND  */
    sizeof(JSObject_Slots4),    /* FINALIZE_OBJECT4             */
    sizeof(JSObject_Slots4),    /* FINALIZE_OBJECT4_BACKGROUND  */
    sizeof(JSObject_Slots8),    /* FINALIZE_OBJECT8             */
    sizeof(JSObject_Slots8),    /* FINALIZE_OBJECT8_BACKGROUND  */
    sizeof(JSObject_Slots12),   /* FINALIZE_OBJECT12            */
    sizeof(JSObject_Slots12),   /* FINALIZE_OBJECT12_BACKGROUND */
    sizeof(JSObject_Slots16),   /* FINALIZE_OBJECT16            */
    sizeof(JSObject_Slots16),   /* FINALIZE_OBJECT16_BACKGROUND */
    sizeof(JSScript),           /* FINALIZE_SCRIPT              */
    sizeof(Shape),              /* FINALIZE_SHAPE               */
    sizeof(BaseShape),          /* FINALIZE_BASE_SHAPE          */
    sizeof(types::TypeObject),  /* FINALIZE_TYPE_OBJECT         */
#if JS_HAS_XML_SUPPORT
    sizeof(JSXML),              /* FINALIZE_XML                 */
#endif
    sizeof(JSShortString),      /* FINALIZE_SHORT_STRING        */
    sizeof(JSString),           /* FINALIZE_STRING              */
    sizeof(JSExternalString),   /* FINALIZE_EXTERNAL_STRING     */
};

#define OFFSET(type) uint32(sizeof(ArenaHeader) + (ArenaSize - sizeof(ArenaHeader)) % sizeof(type))

const uint32 Arena::FirstThingOffsets[] = {
    OFFSET(JSObject),           /* FINALIZE_OBJECT0             */
    OFFSET(JSObject),           /* FINALIZE_OBJECT0_BACKGROUND  */
    OFFSET(JSObject_Slots2),    /* FINALIZE_OBJECT2             */
    OFFSET(JSObject_Slots2),    /* FINALIZE_OBJECT2_BACKGROUND  */
    OFFSET(JSObject_Slots4),    /* FINALIZE_OBJECT4             */
    OFFSET(JSObject_Slots4),    /* FINALIZE_OBJECT4_BACKGROUND  */
    OFFSET(JSObject_Slots8),    /* FINALIZE_OBJECT8             */
    OFFSET(JSObject_Slots8),    /* FINALIZE_OBJECT8_BACKGROUND  */
    OFFSET(JSObject_Slots12),   /* FINALIZE_OBJECT12            */
    OFFSET(JSObject_Slots12),   /* FINALIZE_OBJECT12_BACKGROUND */
    OFFSET(JSObject_Slots16),   /* FINALIZE_OBJECT16            */
    OFFSET(JSObject_Slots16),   /* FINALIZE_OBJECT16_BACKGROUND */
    OFFSET(JSScript),           /* FINALIZE_SCRIPT              */
    OFFSET(Shape),              /* FINALIZE_SHAPE               */
    OFFSET(BaseShape),          /* FINALIZE_BASE_SHAPE          */
    OFFSET(types::TypeObject),  /* FINALIZE_TYPE_OBJECT         */
#if JS_HAS_XML_SUPPORT
    OFFSET(JSXML),              /* FINALIZE_XML                 */
#endif
    OFFSET(JSShortString),      /* FINALIZE_SHORT_STRING        */
    OFFSET(JSString),           /* FINALIZE_STRING              */
    OFFSET(JSExternalString),   /* FINALIZE_EXTERNAL_STRING     */
};

#undef OFFSET

class GCCompartmentsIter {
  private:
    JSCompartment **it, **end;

  public:
    GCCompartmentsIter(JSRuntime *rt) {
        if (rt->gcCurrentCompartment) {
            it = &rt->gcCurrentCompartment;
            end = &rt->gcCurrentCompartment + 1;
        } else {
            it = rt->compartments.begin();
            end = rt->compartments.end();
        }
    }

    bool done() const { return it == end; }

    void next() {
        JS_ASSERT(!done());
        it++;
    }

    JSCompartment *get() const {
        JS_ASSERT(!done());
        return *it;
    }

    operator JSCompartment *() const { return get(); }
    JSCompartment *operator->() const { return get(); }
};

#ifdef DEBUG
void
ArenaHeader::checkSynchronizedWithFreeList() const
{
    /*
     * Do not allow to access the free list when its real head is still stored
     * in FreeLists and is not synchronized with this one.
     */
    JS_ASSERT(allocated());

    /*
     * We can be called from the background finalization thread when the free
     * list in the compartment can mutate at any moment. We cannot do any
     * checks in this case.
     */
    if (!compartment->rt->gcRunning)
        return;

    FreeSpan firstSpan = FreeSpan::decodeOffsets(arenaAddress(), firstFreeSpanOffsets);
    if (firstSpan.isEmpty())
        return;
    const FreeSpan *list = compartment->arenas.getFreeList(getAllocKind());
    if (list->isEmpty() || firstSpan.arenaAddress() != list->arenaAddress())
        return;

    /*
     * Here this arena has free things, FreeList::lists[thingKind] is not
     * empty and also points to this arena. Thus they must the same.
     */
    JS_ASSERT(firstSpan.isSameNonEmptySpan(list));
}
#endif

/* static */ void
Arena::staticAsserts()
{
    JS_STATIC_ASSERT(sizeof(Arena) == ArenaSize);
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(ThingSizes) == FINALIZE_LIMIT);
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(FirstThingOffsets) == FINALIZE_LIMIT);
}

template<typename T>
inline bool
Arena::finalize(JSContext *cx, AllocKind thingKind, size_t thingSize, bool background)
{
    /* Enforce requirements on size of T. */
    JS_ASSERT(thingSize % Cell::CellSize == 0);
    JS_ASSERT(thingSize <= 255);

    JS_ASSERT(aheader.allocated());
    JS_ASSERT(thingKind == aheader.getAllocKind());
    JS_ASSERT(thingSize == aheader.getThingSize());
    JS_ASSERT(!aheader.hasDelayedMarking);

    uintptr_t thing = thingsStart(thingKind);
    uintptr_t lastByte = thingsEnd() - 1;

    FreeSpan nextFree(aheader.getFirstFreeSpan());
    nextFree.checkSpan();

    FreeSpan newListHead;
    FreeSpan *newListTail = &newListHead;
    uintptr_t newFreeSpanStart = 0;
    bool allClear = true;
    DebugOnly<size_t> nmarked = 0;
    for (;; thing += thingSize) {
        JS_ASSERT(thing <= lastByte + 1);
        if (thing == nextFree.first) {
            JS_ASSERT(nextFree.last <= lastByte);
            if (nextFree.last == lastByte)
                break;
            JS_ASSERT(Arena::isAligned(nextFree.last, thingSize));
            if (!newFreeSpanStart)
                newFreeSpanStart = thing;
            thing = nextFree.last;
            nextFree = *nextFree.nextSpan();
            nextFree.checkSpan();
        } else {
            T *t = reinterpret_cast<T *>(thing);
            if (t->isMarked()) {
                allClear = false;
                nmarked++;
                if (newFreeSpanStart) {
                    JS_ASSERT(thing >= thingsStart(thingKind) + thingSize);
                    newListTail->first = newFreeSpanStart;
                    newListTail->last = thing - thingSize;
                    newListTail = newListTail->nextSpanUnchecked(thingSize);
                    newFreeSpanStart = 0;
                }
            } else {
                if (!newFreeSpanStart)
                    newFreeSpanStart = thing;
                t->finalize(cx, background);
                JS_POISON(t, JS_FREE_PATTERN, thingSize);
            }
        }
    }

    if (allClear) {
        JS_ASSERT(newListTail == &newListHead);
        JS_ASSERT(newFreeSpanStart == thingsStart(thingKind));
        return true;
    }

    newListTail->first = newFreeSpanStart ? newFreeSpanStart : nextFree.first;
    JS_ASSERT(Arena::isAligned(newListTail->first, thingSize));
    newListTail->last = lastByte;

#ifdef DEBUG
    size_t nfree = 0;
    for (const FreeSpan *span = &newListHead; span != newListTail; span = span->nextSpan()) {
        span->checkSpan();
        JS_ASSERT(Arena::isAligned(span->first, thingSize));
        JS_ASSERT(Arena::isAligned(span->last, thingSize));
        nfree += (span->last - span->first) / thingSize + 1;
        JS_ASSERT(nfree + nmarked <= thingsPerArena(thingSize));
    }
    nfree += (newListTail->last + 1 - newListTail->first) / thingSize;
    JS_ASSERT(nfree + nmarked == thingsPerArena(thingSize));
#endif
    aheader.setFirstFreeSpan(&newListHead);

    return false;
}

template<typename T>
inline void
FinalizeTypedArenas(JSContext *cx, ArenaLists::ArenaList *al, AllocKind thingKind, bool background)
{
    /*
     * Release empty arenas and move non-full arenas with some free things into
     * a separated list that we append to al after the loop to ensure that any
     * arena before al->cursor is full.
     */
    JS_ASSERT_IF(!al->head, al->cursor == &al->head);
    ArenaLists::ArenaList available;
    ArenaHeader **ap = &al->head;
    size_t thingSize = Arena::thingSize(thingKind);
    while (ArenaHeader *aheader = *ap) {
        bool allClear = aheader->getArena()->finalize<T>(cx, thingKind, thingSize, background);
        if (allClear) {
            *ap = aheader->next;
            aheader->chunk()->releaseArena(aheader);
        } else if (aheader->hasFreeThings()) {
            *ap = aheader->next;
            *available.cursor = aheader;
            available.cursor = &aheader->next;
        } else {
            ap = &aheader->next;
        }
    }

    /* Terminate the available list and append it to al. */
    *available.cursor = NULL;
    *ap = available.head;
    al->cursor = ap;
    JS_ASSERT_IF(!al->head, al->cursor == &al->head);
}

/*
 * Finalize the list. On return al->cursor points to the first non-empty arena
 * after the al->head.
 */
static void
FinalizeArenas(JSContext *cx, ArenaLists::ArenaList *al, AllocKind thingKind, bool background)
{
    switch(thingKind) {
      case FINALIZE_OBJECT0:
      case FINALIZE_OBJECT0_BACKGROUND:
      case FINALIZE_OBJECT2:
      case FINALIZE_OBJECT2_BACKGROUND:
      case FINALIZE_OBJECT4:
      case FINALIZE_OBJECT4_BACKGROUND:
      case FINALIZE_OBJECT8:
      case FINALIZE_OBJECT8_BACKGROUND:
      case FINALIZE_OBJECT12:
      case FINALIZE_OBJECT12_BACKGROUND:
      case FINALIZE_OBJECT16:
      case FINALIZE_OBJECT16_BACKGROUND:
        FinalizeTypedArenas<JSObject>(cx, al, thingKind, background);
        break;
      case FINALIZE_SCRIPT:
	FinalizeTypedArenas<JSScript>(cx, al, thingKind, background);
        break;
      case FINALIZE_SHAPE:
	FinalizeTypedArenas<Shape>(cx, al, thingKind, background);
        break;
      case FINALIZE_BASE_SHAPE:
        FinalizeTypedArenas<BaseShape>(cx, al, thingKind, background);
        break;
      case FINALIZE_TYPE_OBJECT:
	FinalizeTypedArenas<types::TypeObject>(cx, al, thingKind, background);
        break;
#if JS_HAS_XML_SUPPORT
      case FINALIZE_XML:
	FinalizeTypedArenas<JSXML>(cx, al, thingKind, background);
        break;
#endif
      case FINALIZE_STRING:
	FinalizeTypedArenas<JSString>(cx, al, thingKind, background);
        break;
      case FINALIZE_SHORT_STRING:
	FinalizeTypedArenas<JSShortString>(cx, al, thingKind, background);
        break;
      case FINALIZE_EXTERNAL_STRING:
	FinalizeTypedArenas<JSExternalString>(cx, al, thingKind, background);
        break;
    }
}

#ifdef JS_THREADSAFE
inline bool
ChunkPool::wantBackgroundAllocation(JSRuntime *rt) const
{
    /*
     * To minimize memory waste we do not want to run the background chunk
     * allocation if we have empty chunks or when the runtime needs just few
     * of them.
     */
    return rt->gcHelperThread.canBackgroundAllocate() &&
           emptyCount == 0 &&
           rt->gcChunkSet.count() >= 4;
}
#endif

/* Must be called with the GC lock taken. */
inline Chunk *
ChunkPool::get(JSRuntime *rt)
{
    JS_ASSERT(this == &rt->gcChunkPool);

    Chunk *chunk = emptyChunkListHead;
    if (chunk) {
        JS_ASSERT(emptyCount);
        emptyChunkListHead = chunk->info.next;
        --emptyCount;
    } else {
        JS_ASSERT(!emptyCount);
        chunk = Chunk::allocate(rt);
        if (!chunk)
            return NULL;
    }
    JS_ASSERT(chunk->unused());
    JS_ASSERT(!rt->gcChunkSet.has(chunk));

#ifdef JS_THREADSAFE
    if (wantBackgroundAllocation(rt))
        rt->gcHelperThread.startBackgroundAllocationIfIdle();
#endif

    return chunk;
}

/* Must be called either during the GC or with the GC lock taken. */
inline void
ChunkPool::put(JSRuntime *rt, Chunk *chunk)
{
    JS_ASSERT(this == &rt->gcChunkPool);

    size_t initialAge = 0;
#ifdef JS_THREADSAFE
    /*
     * When we have not yet started the background finalization, we must keep
     * empty chunks until we are done with all the sweeping and finalization
     * that cannot be done in the background even if shouldShrink() is true.
     * This way we can safely call IsAboutToBeFinalized and Cell::isMarked for
     * finalized GC things in empty chunks. So we only release the chunk if we
     * are called from the background thread.
     */
    if (rt->gcHelperThread.sweeping()) {
        if (rt->gcHelperThread.shouldShrink()) {
            Chunk::release(rt, chunk);
            return;
        }

        /*
         * Set the age to one as we expire chunks early during the background
         * sweep so this chunk already survived one GC cycle.
         */
        initialAge = 1;
    }
#endif

    chunk->info.age = initialAge;
    chunk->info.next = emptyChunkListHead;
    emptyChunkListHead = chunk;
    emptyCount++;
}

/* Must be called either during the GC or with the GC lock taken. */
void
ChunkPool::expire(JSRuntime *rt, bool releaseAll)
{
    JS_ASSERT(this == &rt->gcChunkPool);

    /*
     * Return old empty chunks to the system while preserving the order of
     * other chunks in the list. This way, if the GC runs several times
     * without emptying the list, the older chunks will stay at the tail
     * and are more likely to reach the max age.
     */
    for (Chunk **chunkp = &emptyChunkListHead; *chunkp; ) {
        JS_ASSERT(emptyCount);
        Chunk *chunk = *chunkp;
        JS_ASSERT(chunk->unused());
        JS_ASSERT(!rt->gcChunkSet.has(chunk));
        JS_ASSERT(chunk->info.age <= MAX_EMPTY_CHUNK_AGE);
        if (releaseAll || chunk->info.age == MAX_EMPTY_CHUNK_AGE) {
            *chunkp = chunk->info.next;
            --emptyCount;
            Chunk::release(rt, chunk);
        } else {
            /* Keep the chunk but increase its age. */
            ++chunk->info.age;
            chunkp = &chunk->info.next;
        }
    }
    JS_ASSERT_IF(releaseAll, !emptyCount);
}

JS_FRIEND_API(int64)
ChunkPool::countDecommittedArenas(JSRuntime *rt)
{
    JS_ASSERT(this == &rt->gcChunkPool);

    int64 numDecommitted = 0;
    Chunk *chunk = emptyChunkListHead;
    while (chunk) {
        for (uint32 i = 0; i < ArenasPerChunk; ++i)
            if (chunk->decommittedArenas.get(i))
                ++numDecommitted;
        chunk = chunk->info.next;
    }
    return numDecommitted;
}

/* static */ Chunk *
Chunk::allocate(JSRuntime *rt)
{
    Chunk *chunk = static_cast<Chunk *>(AllocChunk());
    if (!chunk)
        return NULL;
    chunk->init(rt);
    rt->gcStats.count(gcstats::STAT_NEW_CHUNK);
    return chunk;
}

/* static */ inline void
Chunk::release(JSRuntime *rt, Chunk *chunk)
{
    JS_ASSERT(chunk);
    rt->gcStats.count(gcstats::STAT_DESTROY_CHUNK);
    FreeChunk(chunk);
}

void
Chunk::init(JSRuntime *rt)
{
    JS_POISON(this, JS_FREE_PATTERN, ChunkSize);

    /*
     * We clear the bitmap to guard against xpc_IsGrayGCThing being called on
     * uninitialized data, which would happen before the first GC cycle.
     */
    bitmap.clear();

    /* Initialize the arena tracking bitmap. */ 
    decommittedArenas.clear(false);

    /* Initialize the chunk info. */
    info.freeArenasHead = &arenas[0].aheader;
    info.lastDecommittedArenaOffset = 0;
    info.numArenasFree = ArenasPerChunk;
    info.numArenasFreeCommitted = ArenasPerChunk;
    info.age = 0;
    rt->gcNumFreeArenas += ArenasPerChunk;

    /* Initialize the arena header state. */
    for (jsuint i = 0; i < ArenasPerChunk; i++) {
        arenas[i].aheader.setAsNotAllocated();
        arenas[i].aheader.next = (i + 1 < ArenasPerChunk)
                                 ? &arenas[i + 1].aheader
                                 : NULL;
    }

    /* The rest of info fields are initialized in PickChunk. */
}

inline Chunk **
GetAvailableChunkList(JSCompartment *comp)
{
    JSRuntime *rt = comp->rt;
    return comp->isSystemCompartment
           ? &rt->gcSystemAvailableChunkListHead
           : &rt->gcUserAvailableChunkListHead;
}

inline void
Chunk::addToAvailableList(JSCompartment *comp)
{
    Chunk **listHeadp = GetAvailableChunkList(comp);
    JS_ASSERT(!info.prevp);
    JS_ASSERT(!info.next);
    info.prevp = listHeadp;
    Chunk *head = *listHeadp;
    if (head) {
        JS_ASSERT(head->info.prevp == listHeadp);
        head->info.prevp = &info.next;
    }
    info.next = head;
    *listHeadp = this;
}

inline void
Chunk::removeFromAvailableList()
{
    JS_ASSERT(info.prevp);
    *info.prevp = info.next;
    if (info.next) {
        JS_ASSERT(info.next->info.prevp == &info.next);
        info.next->info.prevp = info.prevp;
    }
    info.prevp = NULL;
    info.next = NULL;
}

/*
 * Search for and return the next decommitted Arena. Our goal is to keep 
 * lastDecommittedArenaOffset "close" to a free arena. We do this by setting
 * it to the most recently freed arena when we free, and forcing it to
 * the last alloc + 1 when we allocate.
 */
jsuint
Chunk::findDecommittedArenaOffset()
{
    /* Note: lastFreeArenaOffset can be past the end of the list. */
    for (jsuint i = info.lastDecommittedArenaOffset; i < ArenasPerChunk; i++)
        if (decommittedArenas.get(i))
            return i;
    for (jsuint i = 0; i < info.lastDecommittedArenaOffset; i++)
        if (decommittedArenas.get(i))
            return i;
    JS_NOT_REACHED("No decommitted arenas found.");
    return -1;
}

ArenaHeader *
Chunk::fetchNextDecommittedArena()
{
    JS_ASSERT(info.numArenasFreeCommitted < info.numArenasFree);

    jsuint offset = findDecommittedArenaOffset();
    info.lastDecommittedArenaOffset = offset + 1;
    --info.numArenasFree;
    decommittedArenas.unset(offset);

    Arena *arena = &arenas[offset];
    CommitMemory(arena, ArenaSize);
    arena->aheader.setAsNotAllocated();

    return &arena->aheader;
}

inline ArenaHeader *
Chunk::fetchNextFreeArena(JSRuntime *rt)
{
    JS_ASSERT(info.numArenasFreeCommitted > 0);

    ArenaHeader *aheader = info.freeArenasHead;
    info.freeArenasHead = aheader->next;
    --info.numArenasFreeCommitted;
    --info.numArenasFree;
    --rt->gcNumFreeArenas;

    return aheader;
}

ArenaHeader *
Chunk::allocateArena(JSCompartment *comp, AllocKind thingKind)
{
    JS_ASSERT(!noAvailableArenas());

    JSRuntime *rt = comp->rt;

    ArenaHeader *aheader = JS_LIKELY(info.numArenasFreeCommitted > 0)
                           ? fetchNextFreeArena(rt)
                           : fetchNextDecommittedArena();
    aheader->init(comp, thingKind);
    if (JS_UNLIKELY(noAvailableArenas()))
        removeFromAvailableList();

    Probes::resizeHeap(comp, rt->gcBytes, rt->gcBytes + ArenaSize);
    JS_ATOMIC_ADD(&rt->gcBytes, ArenaSize);
    JS_ATOMIC_ADD(&comp->gcBytes, ArenaSize);
    if (comp->gcBytes >= comp->gcTriggerBytes)
        TriggerCompartmentGC(comp, gcstats::ALLOCTRIGGER);

    return aheader;
}

void
Chunk::releaseArena(ArenaHeader *aheader)
{
    JS_ASSERT(aheader->allocated());
    JS_ASSERT(!aheader->hasDelayedMarking);
    JSCompartment *comp = aheader->compartment;
    JSRuntime *rt = comp->rt;
#ifdef JS_THREADSAFE
    AutoLockGC maybeLock;
    if (rt->gcHelperThread.sweeping())
        maybeLock.lock(rt);
#endif

    Probes::resizeHeap(comp, rt->gcBytes, rt->gcBytes - ArenaSize);
    JS_ASSERT(size_t(rt->gcBytes) >= ArenaSize);
    JS_ASSERT(size_t(comp->gcBytes) >= ArenaSize);
#ifdef JS_THREADSAFE
    if (rt->gcHelperThread.sweeping()) {
        rt->reduceGCTriggerBytes(GC_HEAP_GROWTH_FACTOR * ArenaSize);
        comp->reduceGCTriggerBytes(GC_HEAP_GROWTH_FACTOR * ArenaSize);
    }
#endif
    JS_ATOMIC_ADD(&rt->gcBytes, -int32(ArenaSize));
    JS_ATOMIC_ADD(&comp->gcBytes, -int32(ArenaSize));

    aheader->setAsNotAllocated();
    aheader->next = info.freeArenasHead;
    info.freeArenasHead = aheader;
    ++info.numArenasFreeCommitted;
    ++info.numArenasFree;
    ++rt->gcNumFreeArenas;

    if (info.numArenasFree == 1) {
        JS_ASSERT(!info.prevp);
        JS_ASSERT(!info.next);
        addToAvailableList(comp);
    } else if (!unused()) {
        JS_ASSERT(info.prevp);
    } else {
        rt->gcChunkSet.remove(this);
        removeFromAvailableList();
        rt->gcChunkPool.put(rt, this);
    }
}

} /* namespace gc */
} /* namespace js */

/* The caller must hold the GC lock. */
static Chunk *
PickChunk(JSCompartment *comp)
{
    JSRuntime *rt = comp->rt;
    Chunk **listHeadp = GetAvailableChunkList(comp);
    Chunk *chunk = *listHeadp;
    if (chunk)
        return chunk;

    chunk = rt->gcChunkPool.get(rt);
    if (!chunk)
        return NULL;

    rt->gcChunkAllocationSinceLastGC = true;

    /*
     * FIXME bug 583732 - chunk is newly allocated and cannot be present in
     * the table so using ordinary lookupForAdd is suboptimal here.
     */
    GCChunkSet::AddPtr p = rt->gcChunkSet.lookupForAdd(chunk);
    JS_ASSERT(!p);
    if (!rt->gcChunkSet.add(p, chunk)) {
        Chunk::release(rt, chunk);
        return NULL;
    }

    chunk->info.prevp = NULL;
    chunk->info.next = NULL;
    chunk->addToAvailableList(comp);

    return chunk;
}

JS_FRIEND_API(bool)
IsAboutToBeFinalized(JSContext *cx, const Cell *thing)
{
    JS_ASSERT(cx);

    JSCompartment *thingCompartment = reinterpret_cast<const Cell *>(thing)->compartment();
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(rt == thingCompartment->rt);
    if (rt->gcCurrentCompartment != NULL && rt->gcCurrentCompartment != thingCompartment)
        return false;

    return !reinterpret_cast<const Cell *>(thing)->isMarked();
}

bool
IsAboutToBeFinalized(JSContext *cx, const Value &v)
{
    JS_ASSERT(v.isMarkable());
    return IsAboutToBeFinalized(cx, (Cell *)v.toGCThing());
}

JS_FRIEND_API(bool)
js_GCThingIsMarked(void *thing, uintN color = BLACK)
{
    JS_ASSERT(thing);
    AssertValidColor(thing, color);
    return reinterpret_cast<Cell *>(thing)->isMarked(color);
}

/* Lifetime for type sets attached to scripts containing observed types. */
static const int64 JIT_SCRIPT_RELEASE_TYPES_INTERVAL = 60 * 1000 * 1000;

JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes)
{
    if (!rt->gcChunkSet.init(INITIAL_CHUNK_CAPACITY))
        return false;

    if (!rt->gcRootsHash.init(256))
        return false;

    if (!rt->gcLocksHash.init(256))
        return false;

#ifdef JS_THREADSAFE
    rt->gcLock = JS_NEW_LOCK();
    if (!rt->gcLock)
        return false;
    rt->gcDone = JS_NEW_CONDVAR(rt->gcLock);
    if (!rt->gcDone)
        return false;
    rt->requestDone = JS_NEW_CONDVAR(rt->gcLock);
    if (!rt->requestDone)
        return false;
    if (!rt->gcHelperThread.init())
        return false;
#endif

    /*
     * Separate gcMaxMallocBytes from gcMaxBytes but initialize to maxbytes
     * for default backward API compatibility.
     */
    rt->gcMaxBytes = maxbytes;
    rt->setGCMaxMallocBytes(maxbytes);
    rt->gcEmptyArenaPoolLifespan = 30000;

    /*
     * The assigned value prevents GC from running when GC memory is too low
     * (during JS engine start).
     */
    rt->setGCLastBytes(8192, GC_NORMAL);

    rt->gcJitReleaseTime = PRMJ_Now() + JIT_SCRIPT_RELEASE_TYPES_INTERVAL;
    return true;
}

namespace js {

inline bool
InFreeList(ArenaHeader *aheader, uintptr_t addr)
{
    if (!aheader->hasFreeThings())
        return false;

    FreeSpan firstSpan(aheader->getFirstFreeSpan());

    for (const FreeSpan *span = &firstSpan;;) {
        /* If the thing comes fore the current span, it's not free. */
        if (addr < span->first)
            return false;

        /*
         * If we find it inside the span, it's dead. We use here "<=" and not
         * "<" even for the last span as we know that thing is inside the
         * arena. Thus for the last span thing < span->end.
         */
        if (addr <= span->last)
            return true;

        /*
         * The last possible empty span is an the end of the arena. Here
         * span->end < thing < thingsEnd and so we must have more spans.
         */
        span = span->nextSpan();
    }
}

/*
 * Returns CGCT_VALID and mark it if the w can be a  live GC thing and sets
 * thingKind accordingly. Otherwise returns the reason for rejection.
 */
inline ConservativeGCTest
MarkIfGCThingWord(JSTracer *trc, jsuword w)
{
    /*
     * We assume that the compiler never uses sub-word alignment to store
     * pointers and does not tag pointers on its own. Additionally, the value
     * representation for all values and the jsid representation for GC-things
     * do not touch the low two bits. Thus any word with the low two bits set
     * is not a valid GC-thing.
     */
    JS_STATIC_ASSERT(JSID_TYPE_STRING == 0 && JSID_TYPE_OBJECT == 4);
    if (w & 0x3)
        return CGCT_LOWBITSET;

    /*
     * An object jsid has its low bits tagged. In the value representation on
     * 64-bit, the high bits are tagged.
     */
    const jsuword JSID_PAYLOAD_MASK = ~jsuword(JSID_TYPE_MASK);
#if JS_BITS_PER_WORD == 32
    jsuword addr = w & JSID_PAYLOAD_MASK;
#elif JS_BITS_PER_WORD == 64
    jsuword addr = w & JSID_PAYLOAD_MASK & JSVAL_PAYLOAD_MASK;
#endif

    Chunk *chunk = Chunk::fromAddress(addr);

    if (!trc->runtime->gcChunkSet.has(chunk))
        return CGCT_NOTCHUNK;

    /*
     * We query for pointers outside the arena array after checking for an
     * allocated chunk. Such pointers are rare and we want to reject them
     * after doing more likely rejections.
     */
    if (!Chunk::withinArenasRange(addr))
        return CGCT_NOTARENA;

    /* If the arena is not currently allocated, don't access the header. */
    size_t arenaOffset = Chunk::arenaIndex(addr);
    if (chunk->decommittedArenas.get(arenaOffset))
        return CGCT_FREEARENA;

    ArenaHeader *aheader = &chunk->arenas[arenaOffset].aheader;

    if (!aheader->allocated())
        return CGCT_FREEARENA;

    JSCompartment *curComp = trc->runtime->gcCurrentCompartment;
    if (curComp && curComp != aheader->compartment)
        return CGCT_OTHERCOMPARTMENT;

    AllocKind thingKind = aheader->getAllocKind();
    uintptr_t offset = addr & ArenaMask;
    uintptr_t minOffset = Arena::firstThingOffset(thingKind);
    if (offset < minOffset)
        return CGCT_NOTARENA;

    /* addr can point inside the thing so we must align the address. */
    uintptr_t shift = (offset - minOffset) % Arena::thingSize(thingKind);
    addr -= shift;

    /*
     * Check if the thing is free. We must use the list of free spans as at
     * this point we no longer have the mark bits from the previous GC run and
     * we must account for newly allocated things.
     */
    if (InFreeList(aheader, addr))
        return CGCT_NOTLIVE;

    void *thing = reinterpret_cast<void *>(addr);

#ifdef DEBUG
    const char pattern[] = "machine_stack %lx";
    char nameBuf[sizeof(pattern) - 3 + sizeof(addr) * 2];
    JS_snprintf(nameBuf, sizeof(nameBuf), "machine_stack %lx", (unsigned long) addr);
    JS_SET_TRACING_NAME(trc, nameBuf);
#endif
    MarkKind(trc, thing, MapAllocToTraceKind(thingKind));

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    if (IS_GC_MARKING_TRACER(trc)) {
        GCMarker *marker = static_cast<GCMarker *>(trc);
        if (marker->conservativeDumpFileName)
            marker->conservativeRoots.append(thing);
        if (shift)
            marker->conservativeStats.unaligned++;
    }
#endif
    return CGCT_VALID;
}

static void
MarkWordConservatively(JSTracer *trc, jsuword w)
{
    /*
     * The conservative scanner may access words that valgrind considers as
     * undefined. To avoid false positives and not to alter valgrind view of
     * the memory we make as memcheck-defined the argument, a copy of the
     * original word. See bug 572678.
     */
#ifdef JS_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(&w, sizeof(w));
#endif

    MarkIfGCThingWord(trc, w);
}

static void
MarkRangeConservatively(JSTracer *trc, const jsuword *begin, const jsuword *end)
{
    JS_ASSERT(begin <= end);
    for (const jsuword *i = begin; i != end; ++i)
        MarkWordConservatively(trc, *i);
}

static void
MarkThreadDataConservatively(JSTracer *trc, ThreadData *td)
{
    ConservativeGCThreadData *ctd = &td->conservativeGC;
    JS_ASSERT(ctd->hasStackToScan());
    jsuword *stackMin, *stackEnd;
#if JS_STACK_GROWTH_DIRECTION > 0
    stackMin = td->nativeStackBase;
    stackEnd = ctd->nativeStackTop;
#else
    stackMin = ctd->nativeStackTop + 1;
    stackEnd = td->nativeStackBase;
#endif
    JS_ASSERT(stackMin <= stackEnd);
    MarkRangeConservatively(trc, stackMin, stackEnd);
    MarkRangeConservatively(trc, ctd->registerSnapshot.words,
                            ArrayEnd(ctd->registerSnapshot.words));
}

void
MarkStackRangeConservatively(JSTracer *trc, Value *beginv, Value *endv)
{
    /*
     * Normally, the drainMarkStack phase of marking will never trace outside
     * of the compartment currently being collected. However, conservative
     * scanning during drainMarkStack (as is done for generators) can break
     * this invariant. So we disable the compartment assertions in this
     * situation.
     */
    struct AutoSkipChecking {
        JSRuntime *runtime;
        JSCompartment *savedCompartment;

        AutoSkipChecking(JSRuntime *rt)
          : runtime(rt), savedCompartment(rt->gcCheckCompartment) {
            rt->gcCheckCompartment = NULL;
        }
        ~AutoSkipChecking() { runtime->gcCheckCompartment = savedCompartment; }
    } as(trc->runtime);

    const jsuword *begin = beginv->payloadWord();
    const jsuword *end = endv->payloadWord();
#ifdef JS_NUNBOX32
    /*
     * With 64-bit jsvals on 32-bit systems, we can optimize a bit by
     * scanning only the payloads.
     */
    JS_ASSERT(begin <= end);
    for (const jsuword *i = begin; i != end; i += sizeof(Value)/sizeof(jsuword))
        MarkWordConservatively(trc, *i);
#else
    MarkRangeConservatively(trc, begin, end);
#endif
}

void
MarkConservativeStackRoots(JSTracer *trc)
{
#ifdef JS_THREADSAFE
    for (JSThread::Map::Range r = trc->runtime->threads.all(); !r.empty(); r.popFront()) {
        JSThread *thread = r.front().value;
        ConservativeGCThreadData *ctd = &thread->data.conservativeGC;
        if (ctd->hasStackToScan()) {
            JS_ASSERT_IF(!thread->data.requestDepth, thread->suspendCount);
            MarkThreadDataConservatively(trc, &thread->data);
        } else {
            JS_ASSERT(!thread->suspendCount);
            JS_ASSERT(thread->data.requestDepth <= ctd->requestThreshold);
        }
    }
#else
    MarkThreadDataConservatively(trc, &trc->runtime->threadData);
#endif
}

JS_NEVER_INLINE void
ConservativeGCThreadData::recordStackTop()
{
    /* Update the native stack pointer if it points to a bigger stack. */
    jsuword dummy;
    nativeStackTop = &dummy;

    /*
     * To record and update the register snapshot for the conservative
     * scanning with the latest values we use setjmp.
     */
#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4611)
#endif
    (void) setjmp(registerSnapshot.jmpbuf);
#if defined(_MSC_VER)
# pragma warning(pop)
#endif
}

static inline void
RecordNativeStackTopForGC(JSContext *cx)
{
    ConservativeGCThreadData *ctd = &JS_THREAD_DATA(cx)->conservativeGC;

#ifdef JS_THREADSAFE
    /* Record the stack top here only if we are called from a request. */
    JS_ASSERT(cx->thread()->data.requestDepth >= ctd->requestThreshold);
    if (cx->thread()->data.requestDepth == ctd->requestThreshold)
        return;
#endif
    ctd->recordStackTop();
}

} /* namespace js */

#ifdef DEBUG
static void
CheckLeakedRoots(JSRuntime *rt);
#endif

void
js_FinishGC(JSRuntime *rt)
{
    /* Delete all remaining Compartments. */
    for (CompartmentsIter c(rt); !c.done(); c.next())
        Foreground::delete_(c.get());
    rt->compartments.clear();
    rt->atomsCompartment = NULL;

    rt->gcSystemAvailableChunkListHead = NULL;
    rt->gcUserAvailableChunkListHead = NULL;
    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        Chunk::release(rt, r.front());
    rt->gcChunkSet.clear();

#ifdef JS_THREADSAFE
    rt->gcHelperThread.finish();
#endif

    /*
     * Finish the pool after the background thread stops in case it was doing
     * the background sweeping.
     */
    rt->gcChunkPool.expire(rt, true);

#ifdef DEBUG
    if (!rt->gcRootsHash.empty())
        CheckLeakedRoots(rt);
#endif
    rt->gcRootsHash.clear();
    rt->gcLocksHash.clear();
}

JSBool
js_AddRoot(JSContext *cx, Value *vp, const char *name)
{
    JSBool ok = js_AddRootRT(cx->runtime, vp, name);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

JSBool
js_AddGCThingRoot(JSContext *cx, void **rp, const char *name)
{
    JSBool ok = js_AddGCThingRootRT(cx->runtime, rp, name);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

JS_FRIEND_API(JSBool)
js_AddRootRT(JSRuntime *rt, jsval *vp, const char *name)
{
    /*
     * Due to the long-standing, but now removed, use of rt->gcLock across the
     * bulk of js_GC, API users have come to depend on JS_AddRoot etc. locking
     * properly with a racing GC, without calling JS_AddRoot from a request.
     * We have to preserve API compatibility here, now that we avoid holding
     * rt->gcLock across the mark phase (including the root hashtable mark).
     */
    AutoLockGC lock(rt);
    js_WaitForGC(rt);

    return !!rt->gcRootsHash.put((void *)vp,
                                 RootInfo(name, JS_GC_ROOT_VALUE_PTR));
}

JS_FRIEND_API(JSBool)
js_AddGCThingRootRT(JSRuntime *rt, void **rp, const char *name)
{
    /*
     * Due to the long-standing, but now removed, use of rt->gcLock across the
     * bulk of js_GC, API users have come to depend on JS_AddRoot etc. locking
     * properly with a racing GC, without calling JS_AddRoot from a request.
     * We have to preserve API compatibility here, now that we avoid holding
     * rt->gcLock across the mark phase (including the root hashtable mark).
     */
    AutoLockGC lock(rt);
    js_WaitForGC(rt);

    return !!rt->gcRootsHash.put((void *)rp,
                                 RootInfo(name, JS_GC_ROOT_GCTHING_PTR));
}

JS_FRIEND_API(JSBool)
js_RemoveRoot(JSRuntime *rt, void *rp)
{
    /*
     * Due to the JS_RemoveRootRT API, we may be called outside of a request.
     * Same synchronization drill as above in js_AddRoot.
     */
    AutoLockGC lock(rt);
    js_WaitForGC(rt);
    rt->gcRootsHash.remove(rp);
    rt->gcPoke = JS_TRUE;
    return JS_TRUE;
}

typedef RootedValueMap::Range RootRange;
typedef RootedValueMap::Entry RootEntry;
typedef RootedValueMap::Enum RootEnum;

#ifdef DEBUG

static void
CheckLeakedRoots(JSRuntime *rt)
{
    uint32 leakedroots = 0;

    /* Warn (but don't assert) debug builds of any remaining roots. */
    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront()) {
        RootEntry &entry = r.front();
        leakedroots++;
        fprintf(stderr,
                "JS engine warning: leaking GC root \'%s\' at %p\n",
                entry.value.name ? entry.value.name : "", entry.key);
    }

    if (leakedroots > 0) {
        if (leakedroots == 1) {
            fprintf(stderr,
"JS engine warning: 1 GC root remains after destroying the JSRuntime at %p.\n"
"                   This root may point to freed memory. Objects reachable\n"
"                   through it have not been finalized.\n",
                    (void *) rt);
        } else {
            fprintf(stderr,
"JS engine warning: %lu GC roots remain after destroying the JSRuntime at %p.\n"
"                   These roots may point to freed memory. Objects reachable\n"
"                   through them have not been finalized.\n",
                    (unsigned long) leakedroots, (void *) rt);
        }
    }
}

void
js_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, JSGCRootType type, void *data),
                  void *data)
{
    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront()) {
        RootEntry &entry = r.front();
        if (const char *name = entry.value.name)
            dump(name, entry.key, entry.value.type, data);
    }
}

#endif /* DEBUG */

uint32
js_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data)
{
    AutoLockGC lock(rt);
    int ct = 0;
    for (RootEnum e(rt->gcRootsHash); !e.empty(); e.popFront()) {
        RootEntry &entry = e.front();

        ct++;
        intN mapflags = map(entry.key, entry.value.type, entry.value.name, data);

        if (mapflags & JS_MAP_GCROOT_REMOVE)
            e.removeFront();
        if (mapflags & JS_MAP_GCROOT_STOP)
            break;
    }

    return ct;
}

void
JSRuntime::setGCLastBytes(size_t lastBytes, JSGCInvocationKind gckind)
{
    gcLastBytes = lastBytes;

    size_t base = gckind == GC_SHRINK ? lastBytes : Max(lastBytes, GC_ALLOCATION_THRESHOLD);
    float trigger = float(base) * GC_HEAP_GROWTH_FACTOR;
    gcTriggerBytes = size_t(Min(float(gcMaxBytes), trigger));
}

void
JSRuntime::reduceGCTriggerBytes(uint32 amount) {
    JS_ASSERT(amount > 0);
    JS_ASSERT(gcTriggerBytes - amount >= 0);
    if (gcTriggerBytes - amount < GC_ALLOCATION_THRESHOLD * GC_HEAP_GROWTH_FACTOR)
        return;
    gcTriggerBytes -= amount;
}

void
JSCompartment::setGCLastBytes(size_t lastBytes, JSGCInvocationKind gckind)
{
    gcLastBytes = lastBytes;

    size_t base = gckind == GC_SHRINK ? lastBytes : Max(lastBytes, GC_ALLOCATION_THRESHOLD);
    float trigger = float(base) * GC_HEAP_GROWTH_FACTOR;
    gcTriggerBytes = size_t(Min(float(rt->gcMaxBytes), trigger));
}

void
JSCompartment::reduceGCTriggerBytes(uint32 amount) {
    JS_ASSERT(amount > 0);
    JS_ASSERT(gcTriggerBytes - amount >= 0);
    if (gcTriggerBytes - amount < GC_ALLOCATION_THRESHOLD * GC_HEAP_GROWTH_FACTOR)
        return;
    gcTriggerBytes -= amount;
}

namespace js {
namespace gc {

inline void *
ArenaLists::allocateFromArena(JSCompartment *comp, AllocKind thingKind)
{
    Chunk *chunk = NULL;

    ArenaList *al = &arenaLists[thingKind];
    AutoLockGC maybeLock;

#ifdef JS_THREADSAFE
    volatile uintptr_t *bfs = &backgroundFinalizeState[thingKind];
    if (*bfs != BFS_DONE) {
        /*
         * We cannot search the arena list for free things while the
         * background finalization runs and can modify head or cursor at any
         * moment. So we always allocate a new arena in that case.
         */
        maybeLock.lock(comp->rt);
        for (;;) {
            if (*bfs == BFS_DONE)
                break;

            if (*bfs == BFS_JUST_FINISHED) {
                /*
                 * Before we took the GC lock or while waiting for the
                 * background finalization to finish the latter added new
                 * arenas to the list.
                 */
                *bfs = BFS_DONE;
                break;
            }

            JS_ASSERT(!*al->cursor);
            chunk = PickChunk(comp);
            if (chunk)
                break;

            /*
             * If the background finalization still runs, wait for it to
             * finish and retry to check if it populated the arena list or
             * added new empty arenas.
             */
            JS_ASSERT(*bfs == BFS_RUN);
            comp->rt->gcHelperThread.waitBackgroundSweepEnd();
            JS_ASSERT(*bfs == BFS_JUST_FINISHED || *bfs == BFS_DONE);
        }
    }
#endif /* JS_THREADSAFE */

    if (!chunk) {
        if (ArenaHeader *aheader = *al->cursor) {
            JS_ASSERT(aheader->hasFreeThings());

            /*
             * The empty arenas are returned to the chunk and should not present on
             * the list.
             */
            JS_ASSERT(!aheader->isEmpty());
            al->cursor = &aheader->next;

            /*
             * Move the free span stored in the arena to the free list and
             * allocate from it.
             */
            freeLists[thingKind] = aheader->getFirstFreeSpan();
            aheader->setAsFullyUsed();
            return freeLists[thingKind].infallibleAllocate(Arena::thingSize(thingKind));
        }

        /* Make sure we hold the GC lock before we call PickChunk. */
        if (!maybeLock.locked())
            maybeLock.lock(comp->rt);
        chunk = PickChunk(comp);
        if (!chunk)
            return NULL;
    }

    /*
     * While we still hold the GC lock get an arena from some chunk, mark it
     * as full as its single free span is moved to the free lits, and insert
     * it to the list as a fully allocated arena.
     *
     * We add the arena before the the head, not after the tail pointed by the
     * cursor, so after the GC the most recently added arena will be used first
     * for allocations improving cache locality.
     */
    JS_ASSERT(!*al->cursor);
    ArenaHeader *aheader = chunk->allocateArena(comp, thingKind);
    aheader->next = al->head;
    if (!al->head) {
        JS_ASSERT(al->cursor == &al->head);
        al->cursor = &aheader->next;
    }
    al->head = aheader;

    /* See comments before allocateFromNewArena about this assert. */
    JS_ASSERT(!aheader->hasFreeThings());
    uintptr_t arenaAddr = aheader->arenaAddress();
    return freeLists[thingKind].allocateFromNewArena(arenaAddr,
                                                     Arena::firstThingOffset(thingKind),
                                                     Arena::thingSize(thingKind));
}

void
ArenaLists::finalizeNow(JSContext *cx, AllocKind thingKind)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE);
#endif
    FinalizeArenas(cx, &arenaLists[thingKind], thingKind, false);
}

inline void
ArenaLists::finalizeLater(JSContext *cx, AllocKind thingKind)
{
    JS_ASSERT(thingKind == FINALIZE_OBJECT0_BACKGROUND  ||
              thingKind == FINALIZE_OBJECT2_BACKGROUND  ||
              thingKind == FINALIZE_OBJECT4_BACKGROUND  ||
              thingKind == FINALIZE_OBJECT8_BACKGROUND  ||
              thingKind == FINALIZE_OBJECT12_BACKGROUND ||
              thingKind == FINALIZE_OBJECT16_BACKGROUND ||
              thingKind == FINALIZE_SHORT_STRING        ||
              thingKind == FINALIZE_STRING);

#ifdef JS_THREADSAFE
    JS_ASSERT(!cx->runtime->gcHelperThread.sweeping());

    ArenaList *al = &arenaLists[thingKind];
    if (!al->head) {
        JS_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE);
        JS_ASSERT(al->cursor == &al->head);
        return;
    }

    /*
     * The state can be just-finished if we have not allocated any GC things
     * from the arena list after the previous background finalization.
     */
    JS_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE ||
              backgroundFinalizeState[thingKind] == BFS_JUST_FINISHED);

    if (cx->gcBackgroundFree) {
        /*
         * To ensure the finalization order even during the background GC we
         * must use infallibleAppend so arenas scheduled for background
         * finalization would not be finalized now if the append fails.
         */
        cx->gcBackgroundFree->finalizeVector.infallibleAppend(al->head);
        al->clear();
        backgroundFinalizeState[thingKind] = BFS_RUN;
    } else {
        FinalizeArenas(cx, al, thingKind, false);
        backgroundFinalizeState[thingKind] = BFS_DONE;
    }

#else /* !JS_THREADSAFE */

    finalizeNow(cx, thingKind);

#endif
}

#ifdef JS_THREADSAFE
/*static*/ void
ArenaLists::backgroundFinalize(JSContext *cx, ArenaHeader *listHead)
{
    JS_ASSERT(listHead);
    AllocKind thingKind = listHead->getAllocKind();
    JSCompartment *comp = listHead->compartment;
    ArenaList finalized;
    finalized.head = listHead;
    FinalizeArenas(cx, &finalized, thingKind, true);

    /*
     * After we finish the finalization al->cursor must point to the end of
     * the head list as we emptied the list before the background finalization
     * and the allocation adds new arenas before the cursor.
     */
    ArenaLists *lists = &comp->arenas;
    ArenaList *al = &lists->arenaLists[thingKind];

    AutoLockGC lock(cx->runtime);
    JS_ASSERT(lists->backgroundFinalizeState[thingKind] == BFS_RUN);
    JS_ASSERT(!*al->cursor);

    /*
     * We must set the state to BFS_JUST_FINISHED if we touch arenaList list,
     * even if we add to the list only fully allocated arenas without any free
     * things. It ensures that the allocation thread takes the GC lock and all
     * writes to the free list elements are propagated. As we always take the
     * GC lock when allocating new arenas from the chunks we can set the state
     * to BFS_DONE if we have released all finalized arenas back to their
     * chunks.
     */
    if (finalized.head) {
        *al->cursor = finalized.head;
        if (finalized.cursor != &finalized.head)
            al->cursor = finalized.cursor;
        lists->backgroundFinalizeState[thingKind] = BFS_JUST_FINISHED;
    } else {
        lists->backgroundFinalizeState[thingKind] = BFS_DONE;
    }
}
#endif /* JS_THREADSAFE */

void
ArenaLists::finalizeObjects(JSContext *cx)
{
    finalizeNow(cx, FINALIZE_OBJECT0);
    finalizeNow(cx, FINALIZE_OBJECT2);
    finalizeNow(cx, FINALIZE_OBJECT4);
    finalizeNow(cx, FINALIZE_OBJECT8);
    finalizeNow(cx, FINALIZE_OBJECT12);
    finalizeNow(cx, FINALIZE_OBJECT16);

#ifdef JS_THREADSAFE
    finalizeLater(cx, FINALIZE_OBJECT0_BACKGROUND);
    finalizeLater(cx, FINALIZE_OBJECT2_BACKGROUND);
    finalizeLater(cx, FINALIZE_OBJECT4_BACKGROUND);
    finalizeLater(cx, FINALIZE_OBJECT8_BACKGROUND);
    finalizeLater(cx, FINALIZE_OBJECT12_BACKGROUND);
    finalizeLater(cx, FINALIZE_OBJECT16_BACKGROUND);
#endif

#if JS_HAS_XML_SUPPORT
    finalizeNow(cx, FINALIZE_XML);
#endif
}

void
ArenaLists::finalizeStrings(JSContext *cx)
{
    finalizeLater(cx, FINALIZE_SHORT_STRING);
    finalizeLater(cx, FINALIZE_STRING);

    finalizeNow(cx, FINALIZE_EXTERNAL_STRING);
}

void
ArenaLists::finalizeShapes(JSContext *cx)
{
    finalizeNow(cx, FINALIZE_SHAPE);
    finalizeNow(cx, FINALIZE_BASE_SHAPE);
    finalizeNow(cx, FINALIZE_TYPE_OBJECT);
}

void
ArenaLists::finalizeScripts(JSContext *cx)
{
    finalizeNow(cx, FINALIZE_SCRIPT);
}

static void
RunLastDitchGC(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
#ifdef JS_THREADSAFE
    Maybe<AutoUnlockAtomsCompartment> maybeUnlockAtomsCompartment;
    if (cx->compartment == rt->atomsCompartment && rt->atomsCompartmentIsLocked)
        maybeUnlockAtomsCompartment.construct(cx);
#endif
    /* The last ditch GC preserves all atoms. */
    AutoKeepAtoms keep(rt);
    js_GC(cx, rt->gcTriggerCompartment, GC_NORMAL, gcstats::LASTDITCH);

#ifdef JS_THREADSAFE
    if (rt->gcBytes >= rt->gcMaxBytes) {
        AutoLockGC lock(rt);
        cx->runtime->gcHelperThread.waitBackgroundSweepEnd();
    }
#endif
}

inline bool
IsGCAllowed(JSContext *cx)
{
    return !JS_THREAD_DATA(cx)->waiveGCQuota;
}

/* static */ void *
ArenaLists::refillFreeList(JSContext *cx, AllocKind thingKind)
{
    JS_ASSERT(cx->compartment->arenas.freeLists[thingKind].isEmpty());

    JSCompartment *comp = cx->compartment;
    JSRuntime *rt = comp->rt;
    JS_ASSERT(!rt->gcRunning);

    bool runGC = !!rt->gcIsNeeded;
    for (;;) {
        if (JS_UNLIKELY(runGC) && IsGCAllowed(cx)) {
            RunLastDitchGC(cx);

            /* Report OOM of the GC failed to free enough memory. */
            if (rt->gcBytes > rt->gcMaxBytes)
                break;

            /*
             * The JSGC_END callback can legitimately allocate new GC
             * things and populate the free list. If that happens, just
             * return that list head.
             */
            size_t thingSize = Arena::thingSize(thingKind);
            if (void *thing = comp->arenas.allocateFromFreeList(thingKind, thingSize))
                return thing;
        }
        void *thing = comp->arenas.allocateFromArena(comp, thingKind);
        if (JS_LIKELY(!!thing))
            return thing;

        /*
         * We failed to allocate. Run the GC if we can unless we have done it
         * already. Otherwise report OOM but first schedule a new GC soon.
         */
        if (runGC || !IsGCAllowed(cx)) {
            AutoLockGC lock(rt);
            TriggerGC(rt, gcstats::REFILL);
            break;
        }
        runGC = true;
    }

    js_ReportOutOfMemory(cx);
    return NULL;
}

} /* namespace gc */
} /* namespace js */

JSGCTraceKind
js_GetGCThingTraceKind(void *thing)
{
    return GetGCThingTraceKind(thing);
}

JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing)
{
    if (!thing)
        return true;

    AutoLockGC lock(rt);
    if (GCLocks::Ptr p = rt->gcLocksHash.lookupWithDefault(thing, 0)) {
        p->value++;
        return true;
    }

    return false;
}

void
js_UnlockGCThingRT(JSRuntime *rt, void *thing)
{
    if (!thing)
        return;

    AutoLockGC lock(rt);
    GCLocks::Ptr p = rt->gcLocksHash.lookup(thing);

    if (p) {
        rt->gcPoke = true;
        if (--p->value == 0)
            rt->gcLocksHash.remove(p);
    }
}

namespace js {

/*
 * When the native stack is low, the GC does not call JS_TraceChildren to mark
 * the reachable "children" of the thing. Rather the thing is put aside and
 * JS_TraceChildren is called later with more space on the C stack.
 *
 * To implement such delayed marking of the children with minimal overhead for
 * the normal case of sufficient native stack, the code adds a field per
 * arena. The field markingDelay->link links all arenas with delayed things
 * into a stack list with the pointer to stack top in
 * GCMarker::unmarkedArenaStackTop. delayMarkingChildren adds
 * arenas to the stack as necessary while markDelayedChildren pops the arenas
 * from the stack until it empties.
 */

GCMarker::GCMarker(JSContext *cx)
  : color(BLACK),
    unmarkedArenaStackTop(NULL),
    objStack(cx->runtime->gcMarkStackObjs, sizeof(cx->runtime->gcMarkStackObjs)),
    ropeStack(cx->runtime->gcMarkStackRopes, sizeof(cx->runtime->gcMarkStackRopes)),
    typeStack(cx->runtime->gcMarkStackTypes, sizeof(cx->runtime->gcMarkStackTypes)),
    xmlStack(cx->runtime->gcMarkStackXMLs, sizeof(cx->runtime->gcMarkStackXMLs)),
    largeStack(cx->runtime->gcMarkStackLarges, sizeof(cx->runtime->gcMarkStackLarges))
{
    JS_TRACER_INIT(this, cx, NULL);
    markLaterArenas = 0;
#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    conservativeDumpFileName = getenv("JS_DUMP_CONSERVATIVE_GC_ROOTS");
    memset(&conservativeStats, 0, sizeof(conservativeStats));
#endif

    /*
     * The GC is recomputing the liveness of WeakMap entries, so we
     * delay visting entries.
     */
    eagerlyTraceWeakMaps = JS_FALSE;
}

GCMarker::~GCMarker()
{
#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    dumpConservativeRoots();
#endif
}

void
GCMarker::delayMarkingChildren(const void *thing)
{
    const Cell *cell = reinterpret_cast<const Cell *>(thing);
    ArenaHeader *aheader = cell->arenaHeader();
    if (aheader->hasDelayedMarking) {
        /* Arena already scheduled to be marked later */
        return;
    }
    aheader->setNextDelayedMarking(unmarkedArenaStackTop);
    unmarkedArenaStackTop = aheader->getArena();
    markLaterArenas++;
}

static void
MarkDelayedChildren(GCMarker *trc, Arena *a)
{
    AllocKind allocKind = a->aheader.getAllocKind();
    JSGCTraceKind traceKind = MapAllocToTraceKind(allocKind);
    size_t thingSize = Arena::thingSize(allocKind);
    uintptr_t end = a->thingsEnd();
    for (uintptr_t thing = a->thingsStart(allocKind); thing != end; thing += thingSize) {
        Cell *t = reinterpret_cast<Cell *>(thing);
        if (t->isMarked())
            JS_TraceChildren(trc, t, traceKind);
    }
}

void
GCMarker::markDelayedChildren()
{
    while (unmarkedArenaStackTop) {
        /*
         * If marking gets delayed at the same arena again, we must repeat
         * marking of its things. For that we pop arena from the stack and
         * clear its hasDelayedMarking flag before we begin the marking.
         */
        Arena *a = unmarkedArenaStackTop;
        JS_ASSERT(a->aheader.hasDelayedMarking);
        JS_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = a->aheader.getNextDelayedMarking();
        a->aheader.hasDelayedMarking = 0;
        markLaterArenas--;
        MarkDelayedChildren(this, a);
    }
    JS_ASSERT(!markLaterArenas);
}

} /* namespace js */

#ifdef DEBUG
static void
EmptyMarkCallback(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
}
#endif

static void
gc_root_traversal(JSTracer *trc, const RootEntry &entry)
{
#ifdef DEBUG
    void *ptr;
    if (entry.value.type == JS_GC_ROOT_GCTHING_PTR) {
        ptr = *reinterpret_cast<void **>(entry.key);
    } else {
        Value *vp = reinterpret_cast<Value *>(entry.key);
        ptr = vp->isGCThing() ? vp->toGCThing() : NULL;
    }

    if (ptr && !trc->runtime->gcCurrentCompartment) {
        /*
         * Use conservative machinery to find if ptr is a valid GC thing.
         * We only do this during global GCs, to preserve the invariant
         * that mark callbacks are not in place during compartment GCs.
         */
        JSTracer checker;
        JS_TRACER_INIT(&checker, trc->context, EmptyMarkCallback);
        ConservativeGCTest test = MarkIfGCThingWord(&checker, reinterpret_cast<jsuword>(ptr));
        if (test != CGCT_VALID && entry.value.name) {
            fprintf(stderr,
"JS API usage error: the address passed to JS_AddNamedRoot currently holds an\n"
"invalid gcthing.  This is usually caused by a missing call to JS_RemoveRoot.\n"
"The root's name is \"%s\".\n",
                    entry.value.name);
        }
        JS_ASSERT(test == CGCT_VALID);
    }
#endif
    const char *name = entry.value.name ? entry.value.name : "root";
    if (entry.value.type == JS_GC_ROOT_GCTHING_PTR)
        MarkRootGCThing(trc, *reinterpret_cast<void **>(entry.key), name);
    else
        MarkRoot(trc, *reinterpret_cast<Value *>(entry.key), name);
}

static void
gc_lock_traversal(const GCLocks::Entry &entry, JSTracer *trc)
{
    JS_ASSERT(entry.value >= 1);
    MarkRootGCThing(trc, entry.key, "locked object");
}

void
js_TraceStackFrame(JSTracer *trc, StackFrame *fp)
{
    MarkRoot(trc, &fp->scopeChain(), "scope chain");
    if (fp->isDummyFrame())
        return;
    if (fp->hasArgsObj())
        MarkRoot(trc, &fp->argsObj(), "arguments");
    MarkRoot(trc, fp->script(), "script");
    fp->script()->compartment()->active = true;
    MarkRoot(trc, fp->returnValue(), "rval");
}

void
AutoIdArray::trace(JSTracer *trc)
{
    JS_ASSERT(tag == IDARRAY);
    gc::MarkIdRange(trc, idArray->vector, idArray->vector + idArray->length,
                    "JSAutoIdArray.idArray");
}

void
AutoEnumStateRooter::trace(JSTracer *trc)
{
    gc::MarkRoot(trc, obj, "js::AutoEnumStateRooter.obj");
}

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag) {
      case JSVAL:
        MarkRoot(trc, static_cast<AutoValueRooter *>(this)->val, "js::AutoValueRooter.val");
        return;

      case PARSER:
        static_cast<Parser *>(this)->trace(trc);
        return;

      case ENUMERATOR:
        static_cast<AutoEnumStateRooter *>(this)->trace(trc);
        return;

      case IDARRAY: {
        JSIdArray *ida = static_cast<AutoIdArray *>(this)->idArray;
        MarkIdRange(trc, ida->vector, ida->vector + ida->length, "js::AutoIdArray.idArray");
        return;
      }

      case DESCRIPTORS: {
        PropDescArray &descriptors =
            static_cast<AutoPropDescArrayRooter *>(this)->descriptors;
        for (size_t i = 0, len = descriptors.length(); i < len; i++) {
            PropDesc &desc = descriptors[i];
            MarkRoot(trc, desc.pd, "PropDesc::pd");
            MarkRoot(trc, desc.value, "PropDesc::value");
            MarkRoot(trc, desc.get, "PropDesc::get");
            MarkRoot(trc, desc.set, "PropDesc::set");
        }
        return;
      }

      case DESCRIPTOR : {
        PropertyDescriptor &desc = *static_cast<AutoPropertyDescriptorRooter *>(this);
        if (desc.obj)
            MarkRoot(trc, desc.obj, "Descriptor::obj");
        MarkRoot(trc, desc.value, "Descriptor::value");
        if ((desc.attrs & JSPROP_GETTER) && desc.getter)
            MarkRoot(trc, CastAsObject(desc.getter), "Descriptor::get");
        if (desc.attrs & JSPROP_SETTER && desc.setter)
            MarkRoot(trc, CastAsObject(desc.setter), "Descriptor::set");
        return;
      }

      case NAMESPACES: {
        JSXMLArray<JSObject> &array = static_cast<AutoNamespaceArray *>(this)->array;
        MarkObjectRange(trc, array.length, array.vector, "JSXMLArray.vector");
        js_XMLArrayCursorTrace(trc, array.cursors);
        return;
      }

      case XML:
        js_TraceXML(trc, static_cast<AutoXMLRooter *>(this)->xml);
        return;

      case OBJECT:
        if (JSObject *obj = static_cast<AutoObjectRooter *>(this)->obj)
            MarkRoot(trc, obj, "js::AutoObjectRooter.obj");
        return;

      case ID:
        MarkRoot(trc, static_cast<AutoIdRooter *>(this)->id_, "js::AutoIdRooter.val");
        return;

      case VALVECTOR: {
        AutoValueVector::VectorImpl &vector = static_cast<AutoValueVector *>(this)->vector;
        MarkRootRange(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }

      case STRING:
        if (JSString *str = static_cast<AutoStringRooter *>(this)->str)
            MarkRoot(trc, str, "js::AutoStringRooter.str");
        return;

      case IDVECTOR: {
        AutoIdVector::VectorImpl &vector = static_cast<AutoIdVector *>(this)->vector;
        MarkRootRange(trc, vector.length(), vector.begin(), "js::AutoIdVector.vector");
        return;
      }

      case SHAPEVECTOR: {
        AutoShapeVector::VectorImpl &vector = static_cast<js::AutoShapeVector *>(this)->vector;
        MarkRootRange(trc, vector.length(), vector.begin(), "js::AutoShapeVector.vector");
        return;
      }

      case OBJVECTOR: {
        AutoObjectVector::VectorImpl &vector = static_cast<AutoObjectVector *>(this)->vector;
        MarkRootRange(trc, vector.length(), vector.begin(), "js::AutoObjectVector.vector");
        return;
      }

      case VALARRAY: {
        AutoValueArray *array = static_cast<AutoValueArray *>(this);
        MarkRootRange(trc, array->length(), array->start(), "js::AutoValueArray");
        return;
      }
    }

    JS_ASSERT(tag >= 0);
    MarkRootRange(trc, tag, static_cast<AutoArrayRooter *>(this)->array,
                  "js::AutoArrayRooter.array");
}

void
AutoGCRooter::traceAll(JSTracer *trc)
{
    for (js::AutoGCRooter *gcr = this; gcr; gcr = gcr->down)
        gcr->trace(trc);
}

namespace js {

JS_FRIEND_API(void)
MarkContext(JSTracer *trc, JSContext *acx)
{
    /* Stack frames and slots are traced by StackSpace::mark. */

    /* Mark other roots-by-definition in acx. */
    if (acx->globalObject && !acx->hasRunOption(JSOPTION_UNROOTED_GLOBAL))
        MarkRoot(trc, acx->globalObject, "global object");
    if (acx->isExceptionPending())
        MarkRoot(trc, acx->getPendingException(), "exception");

    if (acx->autoGCRooters)
        acx->autoGCRooters->traceAll(trc);

    if (acx->sharpObjectMap.depth > 0)
        js_TraceSharpMap(trc, &acx->sharpObjectMap);

    MarkRoot(trc, acx->iterValue, "iterValue");
}

void
MarkWeakReferences(GCMarker *gcmarker)
{
    JS_ASSERT(gcmarker->isMarkStackEmpty());
    while (WatchpointMap::markAllIteratively(gcmarker) ||
           WeakMapBase::markAllIteratively(gcmarker) ||
           Debugger::markAllIteratively(gcmarker)) {
        gcmarker->drainMarkStack();
    }
    JS_ASSERT(gcmarker->isMarkStackEmpty());
}

JS_REQUIRES_STACK void
MarkRuntime(JSTracer *trc)
{
    JSRuntime *rt = trc->runtime;

    if (rt->state != JSRTS_LANDING)
        MarkConservativeStackRoots(trc);

    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront())
        gc_root_traversal(trc, r.front());

    for (GCLocks::Range r = rt->gcLocksHash.all(); !r.empty(); r.popFront())
        gc_lock_traversal(r.front(), trc);

    js_TraceAtomState(trc);
    rt->staticStrings.trace(trc);

    JSContext *iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
        MarkContext(trc, acx);

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->activeAnalysis)
            c->markTypes(trc);
    }

    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->mark(trc);

    /* The embedding can register additional roots here. */
    if (JSTraceDataOp op = rt->gcBlackRootsTraceOp)
        (*op)(trc, rt->gcBlackRootsData);

    if (!IS_GC_MARKING_TRACER(trc)) {
        /* We don't want to miss these when called from TraceRuntime. */
        if (JSTraceDataOp op = rt->gcGrayRootsTraceOp)
            (*op)(trc, rt->gcGrayRootsData);
    }
}

void
TriggerGC(JSRuntime *rt, gcstats::Reason reason)
{
    JS_ASSERT(!rt->gcRunning);
    if (rt->gcIsNeeded)
        return;

    /*
     * Trigger the GC when it is safe to call an operation callback on any
     * thread.
     */
    rt->gcIsNeeded = true;
    rt->gcTriggerCompartment = NULL;
    rt->gcTriggerReason = reason;
    TriggerAllOperationCallbacks(rt);
}

void
TriggerCompartmentGC(JSCompartment *comp, gcstats::Reason reason)
{
    JSRuntime *rt = comp->rt;
    JS_ASSERT(!rt->gcRunning);

    if (rt->gcZeal()) {
        TriggerGC(rt, reason);
        return;
    }

    if (rt->gcMode != JSGC_MODE_COMPARTMENT || comp == rt->atomsCompartment) {
        /* We can't do a compartmental GC of the default compartment. */
        TriggerGC(rt, reason);
        return;
    }

    if (rt->gcIsNeeded) {
        /* If we need to GC more than one compartment, run a full GC. */
        if (rt->gcTriggerCompartment != comp)
            rt->gcTriggerCompartment = NULL;
        return;
    }

    if (rt->gcBytes > 8192 && rt->gcBytes >= 3 * (rt->gcTriggerBytes / 2)) {
        /* If we're using significantly more than our quota, do a full GC. */
        TriggerGC(rt, reason);
        return;
    }

    /*
     * Trigger the GC when it is safe to call an operation callback on any
     * thread.
     */
    rt->gcIsNeeded = true;
    rt->gcTriggerCompartment = comp;
    rt->gcTriggerReason = reason;
    TriggerAllOperationCallbacks(comp->rt);
}

void
MaybeGC(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(rt->onOwnerThread());

    if (rt->gcZeal()) {
        js_GC(cx, NULL, GC_NORMAL, gcstats::MAYBEGC);
        return;
    }

    JSCompartment *comp = cx->compartment;
    if (rt->gcIsNeeded) {
        js_GC(cx, (comp == rt->gcTriggerCompartment) ? comp : NULL, GC_NORMAL, gcstats::MAYBEGC);
        return;
    }

    if (comp->gcBytes > 8192 && comp->gcBytes >= 3 * (comp->gcTriggerBytes / 4)) {
        js_GC(cx, (rt->gcMode == JSGC_MODE_COMPARTMENT) ? comp : NULL, GC_NORMAL, gcstats::MAYBEGC);
        return;
    }

    /*
     * On 32 bit setting gcNextFullGCTime below is not atomic and a race condition
     * could trigger an GC. We tolerate this.
     */
    int64 now = PRMJ_Now();
    if (rt->gcNextFullGCTime && rt->gcNextFullGCTime <= now) {
        if (rt->gcChunkAllocationSinceLastGC || rt->gcNumFreeArenas > MaxFreeCommittedArenas)
            js_GC(cx, NULL, GC_SHRINK, gcstats::MAYBEGC);
        else
            rt->gcNextFullGCTime = now + GC_IDLE_FULL_SPAN;
    }
}

} /* namespace js */

#ifdef JS_THREADSAFE

namespace js {

bool
GCHelperThread::init()
{
    if (!(wakeup = PR_NewCondVar(rt->gcLock)))
        return false;
    if (!(done = PR_NewCondVar(rt->gcLock)))
        return false;

    thread = PR_CreateThread(PR_USER_THREAD, threadMain, this, PR_PRIORITY_NORMAL,
                             PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (!thread)
        return false;

    backgroundAllocation = (js_GetCPUCount() >= 2);
    return true;
}

void
GCHelperThread::finish()
{
    PRThread *join = NULL;
    {
        AutoLockGC lock(rt);
        if (thread && state != SHUTDOWN) {
            /* The allocation should have been stopped during the last GC. */
            JS_ASSERT(state == IDLE || state == SWEEPING);
            if (state == IDLE)
                PR_NotifyCondVar(wakeup);
            state = SHUTDOWN;
            join = thread;
        }
    }
    if (join) {
        /* PR_DestroyThread is not necessary. */
        PR_JoinThread(join);
    }
    if (wakeup)
        PR_DestroyCondVar(wakeup);
    if (done)
        PR_DestroyCondVar(done);
}

/* static */
void
GCHelperThread::threadMain(void *arg)
{
    static_cast<GCHelperThread *>(arg)->threadLoop();
}

void
GCHelperThread::threadLoop()
{
    AutoLockGC lock(rt);

    /*
     * Even on the first iteration the state can be SHUTDOWN or SWEEPING if
     * the stop request or the GC and the corresponding startBackgroundSweep call
     * happen before this thread has a chance to run.
     */
    for (;;) {
        switch (state) {
          case SHUTDOWN:
            return;
          case IDLE:
            PR_WaitCondVar(wakeup, PR_INTERVAL_NO_TIMEOUT);
            break;
          case SWEEPING:
            doSweep();
            if (state == SWEEPING)
                state = IDLE;
            PR_NotifyAllCondVar(done);
            break;
          case ALLOCATING:
            do {
                Chunk *chunk;
                {
                    AutoUnlockGC unlock(rt);
                    chunk = Chunk::allocate(rt);
                }

                /* OOM stops the background allocation. */
                if (!chunk)
                    break;
                rt->gcChunkPool.put(rt, chunk);
            } while (state == ALLOCATING && rt->gcChunkPool.wantBackgroundAllocation(rt));
            if (state == ALLOCATING)
                state = IDLE;
            break;
          case CANCEL_ALLOCATION:
            state = IDLE;
            PR_NotifyAllCondVar(done);
            break;
        }
    }
}

bool
GCHelperThread::prepareForBackgroundSweep(JSContext *cx)
{
    JS_ASSERT(cx->runtime == rt);
    JS_ASSERT(state == IDLE);
    size_t maxArenaLists = MAX_BACKGROUND_FINALIZE_KINDS * rt->compartments.length();
    if (!finalizeVector.reserve(maxArenaLists))
        return false;
    context = cx;
    return true;
}

/* Must be called with the GC lock taken. */
inline void
GCHelperThread::startBackgroundSweep(bool shouldShrink)
{
    /* The caller takes the GC lock. */
    JS_ASSERT(state == IDLE);
    shrinkFlag = shouldShrink;
    state = SWEEPING;
    PR_NotifyCondVar(wakeup);
}

/* Must be called with the GC lock taken. */
void
GCHelperThread::waitBackgroundSweepEnd()
{
    while (state == SWEEPING)
        PR_WaitCondVar(done, PR_INTERVAL_NO_TIMEOUT);
}

/* Must be called with the GC lock taken. */
void
GCHelperThread::waitBackgroundSweepOrAllocEnd()
{
    if (state == ALLOCATING)
        state = CANCEL_ALLOCATION;
    while (state == SWEEPING || state == CANCEL_ALLOCATION)
        PR_WaitCondVar(done, PR_INTERVAL_NO_TIMEOUT);
}

/* Must be called with the GC lock taken. */
inline void
GCHelperThread::startBackgroundAllocationIfIdle()
{
    if (state == IDLE) {
        state = ALLOCATING;
        PR_NotifyCondVar(wakeup);
    }
}

JS_FRIEND_API(void)
GCHelperThread::replenishAndFreeLater(void *ptr)
{
    JS_ASSERT(freeCursor == freeCursorEnd);
    do {
        if (freeCursor && !freeVector.append(freeCursorEnd - FREE_ARRAY_LENGTH))
            break;
        freeCursor = (void **) OffTheBooks::malloc_(FREE_ARRAY_SIZE);
        if (!freeCursor) {
            freeCursorEnd = NULL;
            break;
        }
        freeCursorEnd = freeCursor + FREE_ARRAY_LENGTH;
        *freeCursor++ = ptr;
        return;
    } while (false);
    Foreground::free_(ptr);
}

/* Must be called with the GC lock taken. */
void
GCHelperThread::doSweep()
{
    JS_ASSERT(context);

    /*
     * Expire the chunks released during the GC so they will be available to
     * the rest of the system immediately.
     */
    rt->gcChunkPool.expire(rt, shouldShrink());

    AutoUnlockGC unlock(rt);

    /*
     * We must finalize in the insert order, see comments in
     * finalizeObjects.
     */
    for (ArenaHeader **i = finalizeVector.begin(); i != finalizeVector.end(); ++i)
        ArenaLists::backgroundFinalize(context, *i);
    finalizeVector.resize(0);

    context = NULL;

    if (freeCursor) {
        void **array = freeCursorEnd - FREE_ARRAY_LENGTH;
        freeElementsAndArray(array, freeCursor);
        freeCursor = freeCursorEnd = NULL;
    } else {
        JS_ASSERT(!freeCursorEnd);
    }
    for (void ***iter = freeVector.begin(); iter != freeVector.end(); ++iter) {
        void **array = *iter;
        freeElementsAndArray(array, array + FREE_ARRAY_LENGTH);
    }
    freeVector.resize(0);
}

} /* namespace js */

#endif /* JS_THREADSAFE */

static bool
ReleaseObservedTypes(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    bool releaseTypes = false;
    int64 now = PRMJ_Now();
    if (now >= rt->gcJitReleaseTime) {
        releaseTypes = true;
        rt->gcJitReleaseTime = now + JIT_SCRIPT_RELEASE_TYPES_INTERVAL;
    }

    return releaseTypes;
}

static void
DecommitFreePages(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront()) {
        Chunk *chunk = r.front();
        ArenaHeader *aheader = chunk->info.freeArenasHead;

        /*
         * In the non-failure case, the list will be gone at the end of
         * the loop. In the case where we fail, we relink all failed
         * decommits into a new list on freeArenasHead.
         */
        chunk->info.freeArenasHead = NULL;

        while (aheader) {
            /* Store aside everything we will need after decommit. */
            ArenaHeader *next = aheader->next;

            bool success = DecommitMemory(aheader, ArenaSize);
            if (!success) {
                aheader->next = chunk->info.freeArenasHead;
                chunk->info.freeArenasHead = aheader;
                continue;
            }

            size_t arenaIndex = Chunk::arenaIndex(aheader->arenaAddress());
            chunk->decommittedArenas.set(arenaIndex);
            --chunk->info.numArenasFreeCommitted;
            --rt->gcNumFreeArenas;

            aheader = next;
        }
    }
}

static void
SweepCompartments(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;
    JSCompartmentCallback callback = rt->compartmentCallback;

    /* Skip the atomsCompartment. */
    JSCompartment **read = rt->compartments.begin() + 1;
    JSCompartment **end = rt->compartments.end();
    JSCompartment **write = read;
    JS_ASSERT(rt->compartments.length() >= 1);
    JS_ASSERT(*rt->compartments.begin() == rt->atomsCompartment);

    while (read < end) {
        JSCompartment *compartment = *read++;

        if (!compartment->hold &&
            (compartment->arenas.arenaListsAreEmpty() || gckind == GC_LAST_CONTEXT))
        {
            compartment->arenas.checkEmptyFreeLists();
            if (callback)
                JS_ALWAYS_TRUE(callback(cx, compartment, JSCOMPARTMENT_DESTROY));
            if (compartment->principals)
                JSPRINCIPALS_DROP(cx, compartment->principals);
            cx->delete_(compartment);
            continue;
        }
        *write++ = compartment;
    }
    rt->compartments.resize(write - rt->compartments.begin());
}

static void
BeginMarkPhase(JSContext *cx, GCMarker *gcmarker, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    for (GCCompartmentsIter c(rt); !c.done(); c.next())
        c->purge(cx);

    js_PurgeThreads(cx);

    {
        JSContext *iter = NULL;
        while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
            acx->purge();
    }

    /*
     * Mark phase.
     */
    rt->gcStats.beginPhase(gcstats::PHASE_MARK);

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    if (rt->gcCurrentCompartment) {
        for (CompartmentsIter c(rt); !c.done(); c.next())
            c->markCrossCompartmentWrappers(gcmarker);
        Debugger::markCrossCompartmentDebuggerObjectReferents(gcmarker);
    }

    MarkRuntime(gcmarker);
}

static void
EndMarkPhase(JSContext *cx, GCMarker *gcmarker, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    JS_ASSERT(gcmarker->isMarkStackEmpty());
    MarkWeakReferences(gcmarker);

    if (JSTraceDataOp op = rt->gcGrayRootsTraceOp) {
        gcmarker->setMarkColorGray();
        (*op)(gcmarker, rt->gcGrayRootsData);
        gcmarker->drainMarkStack();
        MarkWeakReferences(gcmarker);
    }

    JS_ASSERT(gcmarker->isMarkStackEmpty());
    rt->gcIncrementalTracer = NULL;

    rt->gcStats.endPhase(gcstats::PHASE_MARK);

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_MARK_END);

#ifdef DEBUG
    /* Make sure that we didn't mark an object in another compartment */
    if (rt->gcCurrentCompartment) {
        for (CompartmentsIter c(rt); !c.done(); c.next()) {
            JS_ASSERT_IF(c != rt->gcCurrentCompartment && c != rt->atomsCompartment,
                         c->arenas.checkArenaListAllUnmarked());
        }
    }
#endif
}

static void
SweepPhase(JSContext *cx, GCMarker *gcmarker, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of rt->gcLock but with rt->gcRunning set
     * so that any attempt to allocate a GC-thing from a finalizer will fail,
     * rather than nest badly and leave the unmarked newborn to be swept.
     *
     * We first sweep atom state so we can use IsAboutToBeFinalized on
     * JSString held in a hashtable to check if the hashtable entry can be
     * freed. Note that even after the entry is freed, JSObject finalizers can
     * continue to access the corresponding JSString* assuming that they are
     * unique. This works since the atomization API must not be called during
     * the GC.
     */
    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP);

    /* Finalize unreachable (key,value) pairs in all weak maps. */
    WeakMapBase::sweepAll(gcmarker);

    js_SweepAtomState(cx);

    /* Collect watch points associated with unreachable objects. */
    WatchpointMap::sweepAll(cx);

    if (!rt->gcCurrentCompartment)
        Debugger::sweepAll(cx);

    bool releaseTypes = !rt->gcCurrentCompartment && ReleaseObservedTypes(cx);
    for (GCCompartmentsIter c(rt); !c.done(); c.next())
        c->sweep(cx, releaseTypes);

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_OBJECT);

        /*
         * We finalize objects before other GC things to ensure that the object's
         * finalizer can access the other things even if they will be freed.
         */
        for (GCCompartmentsIter c(rt); !c.done(); c.next())
            c->arenas.finalizeObjects(cx);
    }

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_STRING);
        for (GCCompartmentsIter c(rt); !c.done(); c.next())
            c->arenas.finalizeStrings(cx);
    }

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_SCRIPT);
        for (GCCompartmentsIter c(rt); !c.done(); c.next())
            c->arenas.finalizeScripts(cx);
    }

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_SHAPE);
        for (GCCompartmentsIter c(rt); !c.done(); c.next())
            c->arenas.finalizeShapes(cx);
    }

#ifdef DEBUG
     PropertyTree::dumpShapes(cx);
#endif

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_DESTROY);

        /*
         * Sweep script filenames after sweeping functions in the generic loop
         * above. In this way when a scripted function's finalizer destroys the
         * script and calls rt->destroyScriptHook, the hook can still access the
         * script's filename. See bug 323267.
         */
        for (GCCompartmentsIter c(rt); !c.done(); c.next())
            js_SweepScriptFilenames(c);

        /*
         * This removes compartments from rt->compartment, so we do it last to make
         * sure we don't miss sweeping any compartments.
         */
        if (!rt->gcCurrentCompartment)
            SweepCompartments(cx, gckind);

#ifndef JS_THREADSAFE
        /*
         * Destroy arenas after we finished the sweeping so finalizers can safely
         * use IsAboutToBeFinalized().
         * This is done on the GCHelperThread if JS_THREADSAFE is defined.
         */
        rt->gcChunkPool.expire(rt, gckind == GC_SHRINK);
#endif

        if (gckind == GC_SHRINK)
            DecommitFreePages(cx);
    }

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_XPCONNECT);
        if (rt->gcCallback)
            (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
    }
}

/*
 * Perform mark-and-sweep GC.
 *
 * In a JS_THREADSAFE build, the calling thread must be rt->gcThread and each
 * other thread must be either outside all requests or blocked waiting for GC
 * to finish. The caller must hold rt->gcLock.
 */
static void
MarkAndSweep(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;
    rt->gcNumber++;

    /* Clear gcIsNeeded now, when we are about to start a normal GC cycle. */
    rt->gcIsNeeded = false;
    rt->gcTriggerCompartment = NULL;

    /* Reset weak map list. */
    rt->gcWeakMapList = NULL;

    /* Reset malloc counter. */
    rt->resetGCMallocBytes();

    AutoUnlockGC unlock(rt);

    GCMarker gcmarker(cx);
    JS_ASSERT(IS_GC_MARKING_TRACER(&gcmarker));
    JS_ASSERT(gcmarker.getMarkColor() == BLACK);
    rt->gcIncrementalTracer = &gcmarker;

    BeginMarkPhase(cx, &gcmarker, gckind);
    gcmarker.drainMarkStack();
    EndMarkPhase(cx, &gcmarker, gckind);
    SweepPhase(cx, &gcmarker, gckind);
}

#ifdef JS_THREADSAFE

/*
 * If the GC is running and we're called on another thread, wait for this GC
 * activation to finish. We can safely wait here without fear of deadlock (in
 * the case where we are called within a request on another thread's context)
 * because the GC doesn't set rt->gcRunning until after it has waited for all
 * active requests to end.
 *
 * We call here js_CurrentThreadId() after checking for rt->gcState to avoid
 * an expensive call when the GC is not running.
 */
void
js_WaitForGC(JSRuntime *rt)
{
    if (rt->gcRunning && rt->gcThread->id != js_CurrentThreadId()) {
        do {
            JS_AWAIT_GC_DONE(rt);
        } while (rt->gcRunning);
    }
}

/*
 * GC is running on another thread. Temporarily suspend all requests running
 * on the current thread and wait until the GC is done.
 */
static void
LetOtherGCFinish(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(rt->gcThread);
    JS_ASSERT(cx->thread() != rt->gcThread);

    size_t requestDebit = cx->thread()->data.requestDepth ? 1 : 0;
    JS_ASSERT(requestDebit <= rt->requestCount);
    if (requestDebit != 0) {
        rt->requestCount -= requestDebit;
        if (rt->requestCount == 0)
            JS_NOTIFY_REQUEST_DONE(rt);

        /*
         * Update the native stack before we wait so the GC thread see the
         * correct stack bounds.
         */
        RecordNativeStackTopForGC(cx);
    }

    /*
     * Check that we did not release the GC lock above and let the GC to
     * finish before we wait.
     */
    JS_ASSERT(rt->gcThread);

    /*
     * Wait for GC to finish on the other thread, even if requestDebit is 0
     * and even if GC has not started yet because the gcThread is waiting in
     * AutoGCSession. This ensures that js_GC never returns without a full GC
     * cycle happening.
     */
    do {
        JS_AWAIT_GC_DONE(rt);
    } while (rt->gcThread);

    rt->requestCount += requestDebit;
}

#endif

class AutoGCSession {
  public:
    explicit AutoGCSession(JSContext *cx);
    ~AutoGCSession();

  private:
    JSContext   *context;

    /* Disable copy constructor or assignments */
    AutoGCSession(const AutoGCSession&);
    void operator=(const AutoGCSession&);
};

/*
 * Start a new GC session. Together with LetOtherGCFinish this function
 * contains the rendezvous algorithm by which we stop the world for GC.
 *
 * This thread becomes the GC thread. Wait for all other threads to quiesce.
 * Then set rt->gcRunning and return.
 */
AutoGCSession::AutoGCSession(JSContext *cx)
  : context(cx)
{
    JS_ASSERT(!JS_THREAD_DATA(cx)->noGCOrAllocationCheck);
    JSRuntime *rt = cx->runtime;

#ifdef JS_THREADSAFE
    if (rt->gcThread && rt->gcThread != cx->thread())
        LetOtherGCFinish(cx);
#endif

    JS_ASSERT(!rt->gcRunning);

#ifdef JS_THREADSAFE
    /* No other thread is in GC, so indicate that we're now in GC. */
    JS_ASSERT(!rt->gcThread);
    rt->gcThread = cx->thread();

    /*
     * Notify operation callbacks on other threads, which will give them a
     * chance to yield their requests. Threads without requests perform their
     * callback at some later point, which then will be unnecessary, but
     * harmless.
     */
    for (JSThread::Map::Range r = rt->threads.all(); !r.empty(); r.popFront()) {
        JSThread *thread = r.front().value;
        if (thread != cx->thread())
            thread->data.triggerOperationCallback(rt);
    }

    /*
     * Discount the request on the current thread from contributing to
     * rt->requestCount before we wait for all other requests to finish.
     * JS_NOTIFY_REQUEST_DONE, which will wake us up, is only called on
     * rt->requestCount transitions to 0.
     */
    size_t requestDebit = cx->thread()->data.requestDepth ? 1 : 0;
    JS_ASSERT(requestDebit <= rt->requestCount);
    if (requestDebit != rt->requestCount) {
        rt->requestCount -= requestDebit;

        do {
            JS_AWAIT_REQUEST_DONE(rt);
        } while (rt->requestCount > 0);
        rt->requestCount += requestDebit;
    }

#endif /* JS_THREADSAFE */

    /*
     * Set rt->gcRunning here within the GC lock, and after waiting for any
     * active requests to end. This way js_WaitForGC called outside a request
     * would not block on the GC that is waiting for other requests to finish
     * with rt->gcThread set while JS_BeginRequest would do such wait.
     */
    rt->gcRunning = true;
}

/* End the current GC session and allow other threads to proceed. */
AutoGCSession::~AutoGCSession()
{
    JSRuntime *rt = context->runtime;
    rt->gcRunning = false;
#ifdef JS_THREADSAFE
    JS_ASSERT(rt->gcThread == context->thread());
    rt->gcThread = NULL;
    JS_NOTIFY_GC_DONE(rt);
#endif
}

/*
 * GC, repeatedly if necessary, until we think we have not created any new
 * garbage and no other threads are demanding more GC. We disable inlining
 * to ensure that the bottom of the stack with possible GC roots recorded in
 * js_GC excludes any pointers we use during the marking implementation.
 */
static JS_NEVER_INLINE void
GCCycle(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    JS_ASSERT_IF(comp, gckind != GC_LAST_CONTEXT);
    JS_ASSERT_IF(comp, comp != rt->atomsCompartment);
    JS_ASSERT_IF(comp, rt->gcMode == JSGC_MODE_COMPARTMENT);

    /*
     * Recursive GC is no-op and a call from another thread waits the started
     * GC cycle to finish.
     */
    if (rt->gcMarkAndSweep) {
#ifdef JS_THREADSAFE
        JS_ASSERT(rt->gcThread);
        if (rt->gcThread != cx->thread()) {
            /* We do not return until another GC finishes. */
            LetOtherGCFinish(cx);
        }
#endif
        return;
    }

    AutoGCSession gcsession(cx);

    /*
     * Don't GC if any thread is reporting an OOM. We check the flag after we
     * have set up the GC session and know that the thread that reported OOM
     * is either the current thread or waits for the GC to complete on this
     * thread.
     */
    if (rt->inOOMReport) {
        JS_ASSERT(gckind != GC_LAST_CONTEXT);
        return;
    }

    /*
     * We should not be depending on cx->compartment in the GC, so set it to
     * NULL to look for violations.
     */
    SwitchToCompartment sc(cx, (JSCompartment *)NULL);

    JS_ASSERT(!rt->gcCurrentCompartment);
    rt->gcCurrentCompartment = comp;

    rt->gcMarkAndSweep = true;

#ifdef JS_THREADSAFE
    /*
     * As we about to purge caches and clear the mark bits we must wait for
     * any background finalization to finish. We must also wait for the
     * background allocation to finish so we can avoid taking the GC lock
     * when manipulating the chunks during the GC.
     */
    JS_ASSERT(!cx->gcBackgroundFree);
    rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();
    if (gckind != GC_LAST_CONTEXT && rt->state != JSRTS_LANDING) {
        if (rt->gcHelperThread.prepareForBackgroundSweep(cx))
            cx->gcBackgroundFree = &rt->gcHelperThread;
    }
#endif

    MarkAndSweep(cx, gckind);

    if (!comp)
        js_PurgeThreads_PostGlobalSweep(cx);

#ifdef JS_THREADSAFE
    if (gckind != GC_LAST_CONTEXT && rt->state != JSRTS_LANDING) {
        JS_ASSERT(cx->gcBackgroundFree == &rt->gcHelperThread);
        cx->gcBackgroundFree = NULL;
        rt->gcHelperThread.startBackgroundSweep(gckind == GC_SHRINK);
    } else {
        JS_ASSERT(!cx->gcBackgroundFree);
    }
#endif

    rt->gcMarkAndSweep = false;
    rt->setGCLastBytes(rt->gcBytes, gckind);
    rt->gcCurrentCompartment = NULL;

    for (CompartmentsIter c(rt); !c.done(); c.next())
        c->setGCLastBytes(c->gcBytes, gckind);
}

void
js_GC(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind, gcstats::Reason reason)
{
    JSRuntime *rt = cx->runtime;
    JS_AbortIfWrongThread(rt);

    /*
     * Don't collect garbage if the runtime isn't up, and cx is not the last
     * context in the runtime.  The last context must force a GC, and nothing
     * should suppress that final collection or there may be shutdown leaks,
     * or runtime bloat until the next context is created.
     */
    if (rt->state != JSRTS_UP && gckind != GC_LAST_CONTEXT)
        return;

#ifdef JS_GC_ZEAL
    struct AutoVerifyBarriers {
        JSContext *cx;
        bool inVerify;
        AutoVerifyBarriers(JSContext *cx) : cx(cx), inVerify(cx->runtime->gcVerifyData) {
            if (inVerify) EndVerifyBarriers(cx);
        }
        ~AutoVerifyBarriers() { if (inVerify) StartVerifyBarriers(cx); }
    } av(cx);
#endif

    RecordNativeStackTopForGC(cx);

    gcstats::AutoGC agc(rt->gcStats, comp, reason);

    do {
        /*
         * Let the API user decide to defer a GC if it wants to (unless this
         * is the last context).  Invoke the callback regardless. Sample the
         * callback in case we are freely racing with a JS_SetGCCallback{,RT}
         * on another thread.
         */
        if (JSGCCallback callback = rt->gcCallback) {
            if (!callback(cx, JSGC_BEGIN) && gckind != GC_LAST_CONTEXT)
                return;
        }

        {
            /* Lock out other GC allocator and collector invocations. */
            AutoLockGC lock(rt);
            rt->gcPoke = false;
            GCCycle(cx, comp, gckind);
        }

        /* We re-sample the callback again as the finalizers can change it. */
        if (JSGCCallback callback = rt->gcCallback)
            (void) callback(cx, JSGC_END);

        /*
         * On shutdown, iterate until finalizers or the JSGC_END callback
         * stop creating garbage.
         */
    } while (gckind == GC_LAST_CONTEXT && rt->gcPoke);

    rt->gcNextFullGCTime = PRMJ_Now() + GC_IDLE_FULL_SPAN;

    rt->gcChunkAllocationSinceLastGC = false;
}

namespace js {

class AutoCopyFreeListToArenas {
    JSRuntime *rt;

  public:
    AutoCopyFreeListToArenas(JSRuntime *rt)
      : rt(rt) {
        for (CompartmentsIter c(rt); !c.done(); c.next())
            c->arenas.copyFreeListsToArenas();
    }

    ~AutoCopyFreeListToArenas() {
        for (CompartmentsIter c(rt); !c.done(); c.next())
            c->arenas.clearFreeListsInArenas();
    }
};

void
TraceRuntime(JSTracer *trc)
{
    JS_ASSERT(!IS_GC_MARKING_TRACER(trc));

#ifdef JS_THREADSAFE
    {
        JSContext *cx = trc->context;
        JSRuntime *rt = cx->runtime;
        if (rt->gcThread != cx->thread()) {
            AutoLockGC lock(rt);
            AutoGCSession gcsession(cx);

            rt->gcHelperThread.waitBackgroundSweepEnd();
            AutoUnlockGC unlock(rt);

            AutoCopyFreeListToArenas copy(rt);
            RecordNativeStackTopForGC(trc->context);
            MarkRuntime(trc);
            return;
        }
    }
#else
    AutoCopyFreeListToArenas copy(trc->runtime);
    RecordNativeStackTopForGC(trc->context);
#endif

    /*
     * Calls from inside a normal GC or a recursive calls are OK and do not
     * require session setup.
     */
    MarkRuntime(trc);
}

struct IterateArenaCallbackOp
{
    JSContext *cx;
    void *data;
    IterateArenaCallback callback;
    JSGCTraceKind traceKind;
    size_t thingSize;
    IterateArenaCallbackOp(JSContext *cx, void *data, IterateArenaCallback callback,
                           JSGCTraceKind traceKind, size_t thingSize)
        : cx(cx), data(data), callback(callback), traceKind(traceKind), thingSize(thingSize)
    {}
    void operator()(Arena *arena) { (*callback)(cx, data, arena, traceKind, thingSize); }
};

struct IterateCellCallbackOp
{
    JSContext *cx;
    void *data;
    IterateCellCallback callback;
    JSGCTraceKind traceKind;
    size_t thingSize;
    IterateCellCallbackOp(JSContext *cx, void *data, IterateCellCallback callback,
                          JSGCTraceKind traceKind, size_t thingSize)
        : cx(cx), data(data), callback(callback), traceKind(traceKind), thingSize(thingSize)
    {}
    void operator()(Cell *cell) { (*callback)(cx, data, cell, traceKind, thingSize); }
};

void
IterateCompartmentsArenasCells(JSContext *cx, void *data,
                               IterateCompartmentCallback compartmentCallback,
                               IterateArenaCallback arenaCallback,
                               IterateCellCallback cellCallback)
{
    CHECK_REQUEST(cx);

    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        (*compartmentCallback)(cx, data, c);

        for (size_t thingKind = 0; thingKind != FINALIZE_LIMIT; thingKind++) {
            JSGCTraceKind traceKind = MapAllocToTraceKind(AllocKind(thingKind));
            size_t thingSize = Arena::thingSize(AllocKind(thingKind));
            IterateArenaCallbackOp arenaOp(cx, data, arenaCallback, traceKind, thingSize);
            IterateCellCallbackOp cellOp(cx, data, cellCallback, traceKind, thingSize);
            ForEachArenaAndCell(c, AllocKind(thingKind), arenaOp, cellOp);
        }
    }
}

void
IterateChunks(JSContext *cx, void *data, IterateChunkCallback chunkCallback)
{
    /* :XXX: Any way to common this preamble with IterateCompartmentsArenasCells? */
    CHECK_REQUEST(cx);

    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
    AutoUnlockGC unlock(rt);

    for (js::GCChunkSet::Range r = rt->gcChunkSet.all(); !r.empty(); r.popFront())
        chunkCallback(cx, data, r.front());
}

void
IterateCells(JSContext *cx, JSCompartment *compartment, AllocKind thingKind,
             void *data, IterateCellCallback cellCallback)
{
    /* :XXX: Any way to common this preamble with IterateCompartmentsArenasCells? */
    CHECK_REQUEST(cx);

    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);

    JSGCTraceKind traceKind = MapAllocToTraceKind(thingKind);
    size_t thingSize = Arena::thingSize(thingKind);

    if (compartment) {
        for (CellIterUnderGC i(compartment, thingKind); !i.done(); i.next())
            cellCallback(cx, data, i.getCell(), traceKind, thingSize);
    } else {
        for (CompartmentsIter c(rt); !c.done(); c.next()) {
            for (CellIterUnderGC i(c, thingKind); !i.done(); i.next())
                cellCallback(cx, data, i.getCell(), traceKind, thingSize);
        }
    }
}

namespace gc {

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals)
{
    JSRuntime *rt = cx->runtime;
    JS_AbortIfWrongThread(rt);

    JSCompartment *compartment = cx->new_<JSCompartment>(rt);
    if (compartment && compartment->init(cx)) {
        // Any compartment with the trusted principals -- and there can be
        // multiple -- is a system compartment.
        compartment->isSystemCompartment = principals && rt->trustedPrincipals() == principals;
        if (principals) {
            compartment->principals = principals;
            JSPRINCIPALS_HOLD(cx, principals);
        }

        compartment->setGCLastBytes(8192, GC_NORMAL);

        /*
         * Before reporting the OOM condition, |lock| needs to be cleaned up,
         * hence the scoping.
         */
        {
            AutoLockGC lock(rt);
            if (rt->compartments.append(compartment))
                return compartment;
        }

        js_ReportOutOfMemory(cx);
    }
    Foreground::delete_(compartment);
    return NULL;
}

void
RunDebugGC(JSContext *cx)
{
#ifdef JS_GC_ZEAL
    if (IsGCAllowed(cx)) {
        JSRuntime *rt = cx->runtime;

        /*
         * If rt->gcDebugCompartmentGC is true, only GC the current
         * compartment. But don't GC the atoms compartment.
         */
        rt->gcTriggerCompartment = rt->gcDebugCompartmentGC ? cx->compartment : NULL;
        if (rt->gcTriggerCompartment == rt->atomsCompartment)
            rt->gcTriggerCompartment = NULL;

        RunLastDitchGC(cx);
    }
#endif
}

#ifdef JS_GC_ZEAL

/*
 * Write barrier verification
 *
 * The next few functions are for incremental write barrier verification. When
 * StartVerifyBarriers is called, a snapshot is taken of all objects in the GC
 * heap and saved in an explicit graph data structure. Later, EndVerifyBarriers
 * traverses the heap again. Any pointer values that were in the snapshot and
 * are no longer found must be marked; otherwise an assertion triggers. Note
 * that we must not GC in between starting and finishing a verification phase.
 *
 * The VerifyBarriers function is a shorthand. It checks if a verification phase
 * is currently running. If not, it starts one. Otherwise, it ends the current
 * phase and starts a new one.
 *
 * The user can adjust the frequency of verifications, which causes
 * VerifyBarriers to be a no-op all but one out of N calls. However, if the
 * |always| parameter is true, it starts a new phase no matter what.
 */

struct EdgeValue
{
    void *thing;
    JSGCTraceKind kind;
    char *label;
};

struct VerifyNode
{
    void *thing;
    JSGCTraceKind kind;
    uint32 count;
    EdgeValue edges[1];
};

typedef HashMap<void *, VerifyNode *> NodeMap;

/*
 * The verifier data structures are simple. The entire graph is stored in a
 * single block of memory. At the beginning is a VerifyNode for the root
 * node. It is followed by a sequence of EdgeValues--the exact number is given
 * in the node. After the edges come more nodes and their edges.
 *
 * The edgeptr and term fields are used to allocate out of the block of memory
 * for the graph. If we run out of memory (i.e., if edgeptr goes beyond term),
 * we just abandon the verification.
 *
 * The nodemap field is a hashtable that maps from the address of the GC thing
 * to the VerifyNode that represents it.
 */
struct VerifyTracer : JSTracer {
    /* The gcNumber when the verification began. */
    uint32 number;

    /* This counts up to JS_VERIFIER_FREQ to decide whether to verify. */
    uint32 count;

    /* This graph represents the initial GC "snapshot". */
    VerifyNode *curnode;
    VerifyNode *root;
    char *edgeptr;
    char *term;
    NodeMap nodemap;

    /* A dummy marker used for the write barriers; stored in gcMarkingTracer. */
    GCMarker gcmarker;

    VerifyTracer(JSContext *cx) : nodemap(cx), gcmarker(cx) {}
};

/*
 * This function builds up the heap snapshot by adding edges to the current
 * node.
 */
static void
AccumulateEdge(JSTracer *jstrc, void *thing, JSGCTraceKind kind)
{
    VerifyTracer *trc = (VerifyTracer *)jstrc;

    trc->edgeptr += sizeof(EdgeValue);
    if (trc->edgeptr >= trc->term) {
        trc->edgeptr = trc->term;
        return;
    }

    VerifyNode *node = trc->curnode;
    uint32 i = node->count;

    node->edges[i].thing = thing;
    node->edges[i].kind = kind;
    node->edges[i].label = trc->debugPrinter ? NULL : (char *)trc->debugPrintArg;
    node->count++;
}

static VerifyNode *
MakeNode(VerifyTracer *trc, void *thing, JSGCTraceKind kind)
{
    NodeMap::AddPtr p = trc->nodemap.lookupForAdd(thing);
    if (!p) {
        VerifyNode *node = (VerifyNode *)trc->edgeptr;
        trc->edgeptr += sizeof(VerifyNode) - sizeof(EdgeValue);
        if (trc->edgeptr >= trc->term) {
            trc->edgeptr = trc->term;
            return NULL;
        }

        node->thing = thing;
        node->count = 0;
        node->kind = kind;
        trc->nodemap.add(p, thing, node);
        return node;
    }
    return NULL;
}

static
VerifyNode *
NextNode(VerifyNode *node)
{
    if (node->count == 0)
        return (VerifyNode *)((char *)node + sizeof(VerifyNode) - sizeof(EdgeValue));
    else
        return (VerifyNode *)((char *)node + sizeof(VerifyNode) +
			      sizeof(EdgeValue)*(node->count - 1));
}

static void
StartVerifyBarriers(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    if (rt->gcVerifyData)
        return;

    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);

#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();
#endif

    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);
    RecordNativeStackTopForGC(cx);

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    /*
     * Kick all frames on the stack into the interpreter, and release all JIT
     * code in the compartment.
     */
#ifdef JS_METHODJIT
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        mjit::ClearAllFrames(c);

        for (CellIterUnderGC i(c, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            mjit::ReleaseScriptCode(cx, script);

            /*
             * Use counts for scripts are reset on GC. After discarding code we
             * need to let it warm back up to get information like which opcodes
             * are setting array holes or accessing getter properties.
             */
            script->resetUseCount();
        }
    }
#endif

    VerifyTracer *trc = new (js_malloc(sizeof(VerifyTracer))) VerifyTracer(cx);

    rt->gcNumber++;
    trc->number = rt->gcNumber;
    trc->count = 0;

    JS_TRACER_INIT(trc, cx, AccumulateEdge);

    const size_t size = 64 * 1024 * 1024;
    trc->root = (VerifyNode *)js_malloc(size);
    JS_ASSERT(trc->root);
    trc->edgeptr = (char *)trc->root;
    trc->term = trc->edgeptr + size;

    trc->nodemap.init();

    /* Create the root node. */
    trc->curnode = MakeNode(trc, NULL, JSGCTraceKind(0));

    /* Make all the roots be edges emanating from the root node. */
    MarkRuntime(trc);

    VerifyNode *node = trc->curnode;
    if (trc->edgeptr == trc->term)
        goto oom;

    /* For each edge, make a node for it if one doesn't already exist. */
    while ((char *)node < trc->edgeptr) {
        for (uint32 i = 0; i < node->count; i++) {
            EdgeValue &e = node->edges[i];
            VerifyNode *child = MakeNode(trc, e.thing, e.kind);
            if (child) {
                trc->curnode = child;
                JS_TraceChildren(trc, e.thing, e.kind);
            }
            if (trc->edgeptr == trc->term)
                goto oom;
        }

        node = NextNode(node);
    }

    rt->gcVerifyData = trc;
    rt->gcIncrementalTracer = &trc->gcmarker;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->gcIncrementalTracer = &trc->gcmarker;
        c->needsBarrier_ = true;
    }

    return;

oom:
    js_free(trc->root);
    trc->~VerifyTracer();
    js_free(trc);
}

static void
CheckAutorooter(JSTracer *jstrc, void *thing, JSGCTraceKind kind)
{
    static_cast<Cell *>(thing)->markIfUnmarked();
}

/*
 * This function is called by EndVerifyBarriers for every heap edge. If the edge
 * already existed in the original snapshot, we "cancel it out" by overwriting
 * it with NULL. EndVerifyBarriers later asserts that the remaining non-NULL
 * edges (i.e., the ones from the original snapshot that must have been
 * modified) must point to marked objects.
 */
static void
CheckEdge(JSTracer *jstrc, void *thing, JSGCTraceKind kind)
{
    VerifyTracer *trc = (VerifyTracer *)jstrc;
    VerifyNode *node = trc->curnode;

    for (uint32 i = 0; i < node->count; i++) {
        if (node->edges[i].thing == thing) {
            JS_ASSERT(node->edges[i].kind == kind);
            node->edges[i].thing = NULL;
            return;
        }
    }
}

static void
EndVerifyBarriers(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);

#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();
#endif

    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);
    RecordNativeStackTopForGC(cx);

    VerifyTracer *trc = (VerifyTracer *)rt->gcVerifyData;

    if (!trc)
        return;

    JS_ASSERT(trc->number == rt->gcNumber);

    rt->gcIncrementalTracer->markDelayedChildren();

    rt->gcVerifyData = NULL;
    rt->gcIncrementalTracer = NULL;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->gcIncrementalTracer = NULL;
        c->needsBarrier_ = false;
    }

    JS_TRACER_INIT(trc, cx, CheckAutorooter);

    JSContext *iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter)) {
        if (acx->autoGCRooters)
            acx->autoGCRooters->traceAll(trc);
    }

    JS_TRACER_INIT(trc, cx, CheckEdge);

    /* Start after the roots. */
    VerifyNode *node = NextNode(trc->root);
    int count = 0;

    while ((char *)node < trc->edgeptr) {
        trc->curnode = node;
        JS_TraceChildren(trc, node->thing, node->kind);

        for (uint32 i = 0; i < node->count; i++) {
            void *thing = node->edges[i].thing;
            JS_ASSERT_IF(thing, static_cast<Cell *>(thing)->isMarked());
        }

        count++;
        node = NextNode(node);
    }

    js_free(trc->root);
    trc->~VerifyTracer();
    js_free(trc);
}

void
VerifyBarriers(JSContext *cx, bool always)
{
    if (cx->runtime->gcZeal() < ZealVerifierThreshold)
        return;

    uint32 freq = cx->runtime->gcZealFrequency;

    JSRuntime *rt = cx->runtime;
    if (VerifyTracer *trc = (VerifyTracer *)rt->gcVerifyData) {
        if (++trc->count < freq && !always)
            return;

        EndVerifyBarriers(cx);
    }
    StartVerifyBarriers(cx);
}

#endif /* JS_GC_ZEAL */

} /* namespace gc */

} /* namespace js */

#if JS_HAS_XML_SUPPORT
extern size_t sE4XObjectsCreated;

JSXML *
js_NewGCXML(JSContext *cx)
{
    if (!cx->runningWithTrustedPrincipals())
        ++sE4XObjectsCreated;

    return NewGCThing<JSXML>(cx, js::gc::FINALIZE_XML, sizeof(JSXML));
}
#endif

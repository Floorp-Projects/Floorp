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

/* JS Mark-and-Sweep Garbage Collector. */

#include "mozilla/Attributes.h"
#include "mozilla/Util.h"

/*
 * This code implements a mark-and-sweep garbage collector. The mark phase is
 * incremental. Most sweeping is done on a background thread. A GC is divided
 * into slices as follows:
 *
 * Slice 1: Roots pushed onto the mark stack. The mark stack is processed by
 * popping an element, marking it, and pushing its children.
 *   ... JS code runs ...
 * Slice 2: More mark stack processing.
 *   ... JS code runs ...
 * Slice n-1: More mark stack processing.
 *   ... JS code runs ...
 * Slice n: Mark stack is completely drained. Some sweeping is done.
 *   ... JS code runs, remaining sweeping done on background thread ...
 *
 * When background sweeping finishes the GC is complete.
 *
 * Incremental GC requires close collaboration with the mutator (i.e., JS code):
 *
 * 1. During an incremental GC, if a memory location (except a root) is written
 * to, then the value it previously held must be marked. Write barriers ensure
 * this.
 * 2. Any object that is allocated during incremental GC must start out marked.
 * 3. Roots are special memory locations that don't need write
 * barriers. However, they must be marked in the first slice. Roots are things
 * like the C stack and the VM stack, since it would be too expensive to put
 * barriers on them.
 *
 * Write barriers are handled using the compartment's barrierMarker_
 * JSTracer. This includes a per-compartment stack of GC things that have been
 * write-barriered. This stack is processed in each GC slice. The barrierMarker_
 * is also used during write barrier verification (VerifyBarriers below).
 */

#include <math.h>
#include <string.h>     /* for memset used when DEBUG */

#include "jstypes.h"
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
#include "gc/Memory.h"
#include "methodjit/MethodJIT.h"
#include "vm/Debugger.h"
#include "vm/String.h"

#include "jsinterpinlines.h"
#include "jsobjinlines.h"

#include "vm/ScopeObject-inl.h"
#include "vm/String-inl.h"

#ifdef MOZ_VALGRIND
# define JS_VALGRIND
#endif
#ifdef JS_VALGRIND
# include <valgrind/memcheck.h>
#endif

#ifdef XP_WIN
# include "jswin.h"
#else
# include <unistd.h>
#endif

using namespace mozilla;
using namespace js;
using namespace js::gc;

namespace js {
namespace gc {

/*
 * Lower limit after which we limit the heap growth
 */
const size_t GC_ALLOCATION_THRESHOLD = 30 * 1024 * 1024;

/*
 * A GC is triggered once the number of newly allocated arenas is
 * GC_HEAP_GROWTH_FACTOR times the number of live arenas after the last GC
 * starting after the lower limit of GC_ALLOCATION_THRESHOLD. This number is
 * used for non-incremental GCs.
 */
const float GC_HEAP_GROWTH_FACTOR = 3.0f;

/* Perform a Full GC every 20 seconds if MaybeGC is called */
static const uint64_t GC_IDLE_FULL_SPAN = 20 * 1000 * 1000;

#ifdef JS_GC_ZEAL
static void
StartVerifyBarriers(JSContext *cx);

static void
EndVerifyBarriers(JSContext *cx);

void
FinishVerifier(JSRuntime *rt);
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

const uint32_t Arena::ThingSizes[] = {
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

#define OFFSET(type) uint32_t(sizeof(ArenaHeader) + (ArenaSize - sizeof(ArenaHeader)) % sizeof(type))

const uint32_t Arena::FirstThingOffsets[] = {
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
    JS_ASSERT(!aheader.markOverflow);
    JS_ASSERT(!aheader.allocatedDuringIncremental);

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

static inline Chunk *
AllocChunk() {
    return static_cast<Chunk *>(MapAlignedPages(ChunkSize, ChunkSize));
}

static inline void
FreeChunk(Chunk *p) {
    UnmapPages(static_cast<void *>(p), ChunkSize);
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
        JS_ASSERT(chunk->info.numArenasFreeCommitted == ArenasPerChunk);
        rt->gcNumArenasFreeCommitted += ArenasPerChunk;
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
ChunkPool::put(Chunk *chunk)
{
    chunk->info.age = 0;
    chunk->info.next = emptyChunkListHead;
    emptyChunkListHead = chunk;
    emptyCount++;
}

/* Must be called either during the GC or with the GC lock taken. */
Chunk *
ChunkPool::expire(JSRuntime *rt, bool releaseAll)
{
    JS_ASSERT(this == &rt->gcChunkPool);

    /*
     * Return old empty chunks to the system while preserving the order of
     * other chunks in the list. This way, if the GC runs several times
     * without emptying the list, the older chunks will stay at the tail
     * and are more likely to reach the max age.
     */
    Chunk *freeList = NULL;
    for (Chunk **chunkp = &emptyChunkListHead; *chunkp; ) {
        JS_ASSERT(emptyCount);
        Chunk *chunk = *chunkp;
        JS_ASSERT(chunk->unused());
        JS_ASSERT(!rt->gcChunkSet.has(chunk));
        JS_ASSERT(chunk->info.age <= MAX_EMPTY_CHUNK_AGE);
        if (releaseAll || chunk->info.age == MAX_EMPTY_CHUNK_AGE) {
            *chunkp = chunk->info.next;
            --emptyCount;
            chunk->prepareToBeFreed(rt);
            chunk->info.next = freeList;
            freeList = chunk;
        } else {
            /* Keep the chunk but increase its age. */
            ++chunk->info.age;
            chunkp = &chunk->info.next;
        }
    }
    JS_ASSERT_IF(releaseAll, !emptyCount);
    return freeList;
}

static void
FreeChunkList(Chunk *chunkListHead)
{
    while (Chunk *chunk = chunkListHead) {
        JS_ASSERT(!chunk->info.numArenasFreeCommitted);
        chunkListHead = chunk->info.next;
        FreeChunk(chunk);
    }
}

void
ChunkPool::expireAndFree(JSRuntime *rt, bool releaseAll)
{
    FreeChunkList(expire(rt, releaseAll));
}

JS_FRIEND_API(int64_t)
ChunkPool::countCleanDecommittedArenas(JSRuntime *rt)
{
    JS_ASSERT(this == &rt->gcChunkPool);

    int64_t numDecommitted = 0;
    Chunk *chunk = emptyChunkListHead;
    while (chunk) {
        for (uint32_t i = 0; i < ArenasPerChunk; ++i)
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
    chunk->init();
    rt->gcStats.count(gcstats::STAT_NEW_CHUNK);
    return chunk;
}

/* Must be called with the GC lock taken. */
/* static */ inline void
Chunk::release(JSRuntime *rt, Chunk *chunk)
{
    JS_ASSERT(chunk);
    chunk->prepareToBeFreed(rt);
    FreeChunk(chunk);
}

inline void
Chunk::prepareToBeFreed(JSRuntime *rt)
{
    JS_ASSERT(rt->gcNumArenasFreeCommitted >= info.numArenasFreeCommitted);
    rt->gcNumArenasFreeCommitted -= info.numArenasFreeCommitted;
    rt->gcStats.count(gcstats::STAT_DESTROY_CHUNK);

#ifdef DEBUG
    /*
     * Let FreeChunkList detect a missing prepareToBeFreed call before it
     * frees chunk.
     */
    info.numArenasFreeCommitted = 0;
#endif
}

void
Chunk::init()
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
    insertToAvailableList(GetAvailableChunkList(comp));
}

inline void
Chunk::insertToAvailableList(Chunk **insertPoint)
{
    JS_ASSERT(hasAvailableArenas());
    JS_ASSERT(!info.prevp);
    JS_ASSERT(!info.next);
    info.prevp = insertPoint;
    Chunk *insertBefore = *insertPoint;
    if (insertBefore) {
        JS_ASSERT(insertBefore->info.prevp == insertPoint);
        insertBefore->info.prevp = &info.next;
    }
    info.next = insertBefore;
    *insertPoint = this;
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
    JS_ASSERT(info.numArenasFreeCommitted == 0);
    JS_ASSERT(info.numArenasFree > 0);

    jsuint offset = findDecommittedArenaOffset();
    info.lastDecommittedArenaOffset = offset + 1;
    --info.numArenasFree;
    decommittedArenas.unset(offset);

    Arena *arena = &arenas[offset];
    MarkPagesInUse(arena, ArenaSize);
    arena->aheader.setAsNotAllocated();

    return &arena->aheader;
}

inline ArenaHeader *
Chunk::fetchNextFreeArena(JSRuntime *rt)
{
    JS_ASSERT(info.numArenasFreeCommitted > 0);
    JS_ASSERT(info.numArenasFreeCommitted <= info.numArenasFree);
    JS_ASSERT(info.numArenasFreeCommitted <= rt->gcNumArenasFreeCommitted);

    ArenaHeader *aheader = info.freeArenasHead;
    info.freeArenasHead = aheader->next;
    --info.numArenasFreeCommitted;
    --info.numArenasFree;
    --rt->gcNumArenasFreeCommitted;

    return aheader;
}

ArenaHeader *
Chunk::allocateArena(JSCompartment *comp, AllocKind thingKind)
{
    JS_ASSERT(hasAvailableArenas());

    JSRuntime *rt = comp->rt;
    JS_ASSERT(rt->gcBytes <= rt->gcMaxBytes);
    if (rt->gcMaxBytes - rt->gcBytes < ArenaSize)
        return NULL;

    ArenaHeader *aheader = JS_LIKELY(info.numArenasFreeCommitted > 0)
                           ? fetchNextFreeArena(rt)
                           : fetchNextDecommittedArena();
    aheader->init(comp, thingKind);
    if (JS_UNLIKELY(!hasAvailableArenas()))
        removeFromAvailableList();

    Probes::resizeHeap(comp, rt->gcBytes, rt->gcBytes + ArenaSize);
    rt->gcBytes += ArenaSize;
    comp->gcBytes += ArenaSize;
    if (comp->gcBytes >= comp->gcTriggerBytes)
        TriggerCompartmentGC(comp, gcreason::ALLOC_TRIGGER);

    return aheader;
}

inline void
Chunk::addArenaToFreeList(JSRuntime *rt, ArenaHeader *aheader)
{
    JS_ASSERT(!aheader->allocated());
    aheader->next = info.freeArenasHead;
    info.freeArenasHead = aheader;
    ++info.numArenasFreeCommitted;
    ++info.numArenasFree;
    ++rt->gcNumArenasFreeCommitted;
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
    JS_ASSERT(rt->gcBytes >= ArenaSize);
    JS_ASSERT(comp->gcBytes >= ArenaSize);
#ifdef JS_THREADSAFE
    if (rt->gcHelperThread.sweeping())
        comp->reduceGCTriggerBytes(GC_HEAP_GROWTH_FACTOR * ArenaSize);
#endif
    rt->gcBytes -= ArenaSize;
    comp->gcBytes -= ArenaSize;

    aheader->setAsNotAllocated();
    addArenaToFreeList(rt, aheader);

    if (info.numArenasFree == 1) {
        JS_ASSERT(!info.prevp);
        JS_ASSERT(!info.next);
        addToAvailableList(comp);
    } else if (!unused()) {
        JS_ASSERT(info.prevp);
    } else {
        rt->gcChunkSet.remove(this);
        removeFromAvailableList();
        rt->gcChunkPool.put(this);
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
IsAboutToBeFinalized(const Cell *thing)
{
    JSCompartment *thingCompartment = reinterpret_cast<const Cell *>(thing)->compartment();
    JSRuntime *rt = thingCompartment->rt;
    if (rt->gcCurrentCompartment != NULL && rt->gcCurrentCompartment != thingCompartment)
        return false;
    return !reinterpret_cast<const Cell *>(thing)->isMarked();
}

bool
IsAboutToBeFinalized(const Value &v)
{
    JS_ASSERT(v.isMarkable());
    return IsAboutToBeFinalized((Cell *)v.toGCThing());
}

/* Lifetime for type sets attached to scripts containing observed types. */
static const int64_t JIT_SCRIPT_RELEASE_TYPES_INTERVAL = 60 * 1000 * 1000;

JSBool
js_InitGC(JSRuntime *rt, uint32_t maxbytes)
{
    if (!rt->gcChunkSet.init(INITIAL_CHUNK_CAPACITY))
        return false;

    if (!rt->gcRootsHash.init(256))
        return false;

    if (!rt->gcLocksHash.init(256))
        return false;

#ifdef JS_THREADSAFE
    rt->gcLock = PR_NewLock();
    if (!rt->gcLock)
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

enum ConservativeGCTest
{
    CGCT_VALID,
    CGCT_LOWBITSET, /* excluded because one of the low bits was set */
    CGCT_NOTARENA,  /* not within arena range in a chunk */
    CGCT_OTHERCOMPARTMENT,  /* in another compartment */
    CGCT_NOTCHUNK,  /* not within a valid chunk */
    CGCT_FREEARENA, /* within arena containing only free things */
    CGCT_NOTLIVE,   /* gcthing is not allocated */
    CGCT_END
};

/*
 * Tests whether w is a (possibly dead) GC thing. Returns CGCT_VALID and
 * details about the thing if so. On failure, returns the reason for rejection.
 */
inline ConservativeGCTest
IsAddressableGCThing(JSRuntime *rt, uintptr_t w,
                     gc::AllocKind *thingKindPtr, ArenaHeader **arenaHeader, void **thing)
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
    const uintptr_t JSID_PAYLOAD_MASK = ~uintptr_t(JSID_TYPE_MASK);
#if JS_BITS_PER_WORD == 32
    uintptr_t addr = w & JSID_PAYLOAD_MASK;
#elif JS_BITS_PER_WORD == 64
    uintptr_t addr = w & JSID_PAYLOAD_MASK & JSVAL_PAYLOAD_MASK;
#endif

    Chunk *chunk = Chunk::fromAddress(addr);

    if (!rt->gcChunkSet.has(chunk))
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

    JSCompartment *curComp = rt->gcCurrentCompartment;
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

    if (thing)
        *thing = reinterpret_cast<void *>(addr);
    if (arenaHeader)
        *arenaHeader = aheader;
    if (thingKindPtr)
        *thingKindPtr = thingKind;
    return CGCT_VALID;
}

/*
 * Returns CGCT_VALID and mark it if the w can be a  live GC thing and sets
 * thingKind accordingly. Otherwise returns the reason for rejection.
 */
inline ConservativeGCTest
MarkIfGCThingWord(JSTracer *trc, uintptr_t w)
{
    void *thing;
    ArenaHeader *aheader;
    AllocKind thingKind;
    ConservativeGCTest status = IsAddressableGCThing(trc->runtime, w, &thingKind, &aheader, &thing);
    if (status != CGCT_VALID)
        return status;

    /*
     * Check if the thing is free. We must use the list of free spans as at
     * this point we no longer have the mark bits from the previous GC run and
     * we must account for newly allocated things.
     */
    if (InFreeList(aheader, uintptr_t(thing)))
        return CGCT_NOTLIVE;

    JSGCTraceKind traceKind = MapAllocToTraceKind(thingKind);
#ifdef DEBUG
    const char pattern[] = "machine_stack %p";
    char nameBuf[sizeof(pattern) - 2 + sizeof(thing) * 2];
    JS_snprintf(nameBuf, sizeof(nameBuf), pattern, thing);
    JS_SET_TRACING_NAME(trc, nameBuf);
#endif
    MarkKind(trc, thing, traceKind);

#ifdef DEBUG
    if (trc->runtime->gcIncrementalState == MARK_ROOTS)
        trc->runtime->gcSavedRoots.append(JSRuntime::SavedGCRoot(thing, traceKind));
#endif

    return CGCT_VALID;
}

static void
MarkWordConservatively(JSTracer *trc, uintptr_t w)
{
    /*
     * The conservative scanner may access words that valgrind considers as
     * undefined. To avoid false positives and not to alter valgrind view of
     * the memory we make as memcheck-defined the argument, a copy of the
     * original word. See bug 572678.
     */
#ifdef JS_VALGRIND
    JS_SILENCE_UNUSED_VALUE_IN_EXPR(VALGRIND_MAKE_MEM_DEFINED(&w, sizeof(w)));
#endif

    MarkIfGCThingWord(trc, w);
}

static void
MarkRangeConservatively(JSTracer *trc, const uintptr_t *begin, const uintptr_t *end)
{
    JS_ASSERT(begin <= end);
    for (const uintptr_t *i = begin; i < end; ++i)
        MarkWordConservatively(trc, *i);
}

static JS_NEVER_INLINE void
MarkConservativeStackRoots(JSTracer *trc, bool useSavedRoots)
{
    JSRuntime *rt = trc->runtime;

#ifdef DEBUG
    if (useSavedRoots) {
        for (JSRuntime::SavedGCRoot *root = rt->gcSavedRoots.begin();
             root != rt->gcSavedRoots.end();
             root++)
        {
            JS_SET_TRACING_NAME(trc, "cstack");
            MarkKind(trc, root->thing, root->kind);
        }
        return;
    }

    if (rt->gcIncrementalState == MARK_ROOTS)
        rt->gcSavedRoots.clearAndFree();
#endif

    ConservativeGCData *cgcd = &rt->conservativeGC;
    if (!cgcd->hasStackToScan()) {
#ifdef JS_THREADSAFE
        JS_ASSERT(!rt->suspendCount);
        JS_ASSERT(rt->requestDepth <= cgcd->requestThreshold);
#endif
        return;
    }

    uintptr_t *stackMin, *stackEnd;
#if JS_STACK_GROWTH_DIRECTION > 0
    stackMin = rt->nativeStackBase;
    stackEnd = cgcd->nativeStackTop;
#else
    stackMin = cgcd->nativeStackTop + 1;
    stackEnd = reinterpret_cast<uintptr_t *>(rt->nativeStackBase);
#endif

    JS_ASSERT(stackMin <= stackEnd);
    MarkRangeConservatively(trc, stackMin, stackEnd);
    MarkRangeConservatively(trc, cgcd->registerSnapshot.words,
                            ArrayEnd(cgcd->registerSnapshot.words));
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

    const uintptr_t *begin = beginv->payloadWord();
    const uintptr_t *end = endv->payloadWord();
#ifdef JS_NUNBOX32
    /*
     * With 64-bit jsvals on 32-bit systems, we can optimize a bit by
     * scanning only the payloads.
     */
    JS_ASSERT(begin <= end);
    for (const uintptr_t *i = begin; i < end; i += sizeof(Value) / sizeof(uintptr_t))
        MarkWordConservatively(trc, *i);
#else
    MarkRangeConservatively(trc, begin, end);
#endif
}



JS_NEVER_INLINE void
ConservativeGCData::recordStackTop()
{
    /* Update the native stack pointer if it points to a bigger stack. */
    uintptr_t dummy;
    nativeStackTop = &dummy;

    /*
     * To record and update the register snapshot for the conservative scanning
     * with the latest values we use setjmp.
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

static void
RecordNativeStackTopForGC(JSRuntime *rt)
{
    ConservativeGCData *cgcd = &rt->conservativeGC;

#ifdef JS_THREADSAFE
    /* Record the stack top here only if we are called from a request. */
    JS_ASSERT(rt->requestDepth >= cgcd->requestThreshold);
    if (rt->requestDepth == cgcd->requestThreshold)
        return;
#endif
    cgcd->recordStackTop();
}

} /* namespace js */

bool
js_IsAddressableGCThing(JSRuntime *rt, uintptr_t w, gc::AllocKind *thingKind, void **thing)
{
    return js::IsAddressableGCThing(rt, w, thingKind, NULL, thing) == CGCT_VALID;
}

#ifdef DEBUG
static void
CheckLeakedRoots(JSRuntime *rt);
#endif

void
js_FinishGC(JSRuntime *rt)
{
    /*
     * Wait until the background finalization stops and the helper thread
     * shuts down before we forcefully release any remaining GC memory.
     */
#ifdef JS_THREADSAFE
    rt->gcHelperThread.finish();
#endif

#ifdef JS_GC_ZEAL
    /* Free memory associated with GC verification. */
    FinishVerifier(rt);
#endif

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

    rt->gcChunkPool.expireAndFree(rt, true);

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
     * bulk of js::GC, API users have come to depend on JS_AddRoot etc. locking
     * properly with a racing GC, without calling JS_AddRoot from a request.
     * We have to preserve API compatibility here, now that we avoid holding
     * rt->gcLock across the mark phase (including the root hashtable mark).
     */
    AutoLockGC lock(rt);

    return !!rt->gcRootsHash.put((void *)vp,
                                 RootInfo(name, JS_GC_ROOT_VALUE_PTR));
}

JS_FRIEND_API(JSBool)
js_AddGCThingRootRT(JSRuntime *rt, void **rp, const char *name)
{
    /*
     * Due to the long-standing, but now removed, use of rt->gcLock across the
     * bulk of js::GC, API users have come to depend on JS_AddRoot etc. locking
     * properly with a racing GC, without calling JS_AddRoot from a request.
     * We have to preserve API compatibility here, now that we avoid holding
     * rt->gcLock across the mark phase (including the root hashtable mark).
     */
    AutoLockGC lock(rt);

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
    uint32_t leakedroots = 0;

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

uint32_t
js_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data)
{
    AutoLockGC lock(rt);
    int ct = 0;
    for (RootEnum e(rt->gcRootsHash); !e.empty(); e.popFront()) {
        RootEntry &entry = e.front();

        ct++;
        int mapflags = map(entry.key, entry.value.type, entry.value.name, data);

        if (mapflags & JS_MAP_GCROOT_REMOVE)
            e.removeFront();
        if (mapflags & JS_MAP_GCROOT_STOP)
            break;
    }

    return ct;
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
JSCompartment::reduceGCTriggerBytes(size_t amount)
{
    JS_ASSERT(amount > 0);
    JS_ASSERT(gcTriggerBytes - amount >= 0);
    if (gcTriggerBytes - amount < GC_ALLOCATION_THRESHOLD * GC_HEAP_GROWTH_FACTOR)
        return;
    gcTriggerBytes -= amount;
}

namespace js {
namespace gc {

inline void
ArenaLists::prepareForIncrementalGC(JSCompartment *comp)
{
    for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
        FreeSpan *headSpan = &freeLists[i];
        if (!headSpan->isEmpty()) {
            ArenaHeader *aheader = headSpan->arenaHeader();
            aheader->allocatedDuringIncremental = true;
            comp->barrierMarker_.delayMarkingArena(aheader);
        }
    }
}

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
        if (*bfs == BFS_RUN) {
            JS_ASSERT(!*al->cursor);
            chunk = PickChunk(comp);
            if (!chunk) {
                /*
                 * Let the caller to wait for the background allocation to
                 * finish and restart the allocation attempt.
                 */
                return NULL;
            }
        } else if (*bfs == BFS_JUST_FINISHED) {
            /* See comments before BackgroundFinalizeState definition. */
            *bfs = BFS_DONE;
        } else {
            JS_ASSERT(*bfs == BFS_DONE);
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
            if (JS_UNLIKELY(comp->needsBarrier())) {
                aheader->allocatedDuringIncremental = true;
                comp->barrierMarker_.delayMarkingArena(aheader);
            }
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
    if (!aheader)
        return NULL;

    if (JS_UNLIKELY(comp->needsBarrier())) {
        aheader->allocatedDuringIncremental = true;
        comp->barrierMarker_.delayMarkingArena(aheader);
    }
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

    /* The last ditch GC preserves all atoms. */
    AutoKeepAtoms keep(rt);
    GC(cx, rt->gcTriggerCompartment, GC_NORMAL, gcreason::LAST_DITCH);
}

/* static */ void *
ArenaLists::refillFreeList(JSContext *cx, AllocKind thingKind)
{
    JS_ASSERT(cx->compartment->arenas.freeLists[thingKind].isEmpty());

    JSCompartment *comp = cx->compartment;
    JSRuntime *rt = comp->rt;
    JS_ASSERT(!rt->gcRunning);

    bool runGC = rt->gcIncrementalState != NO_INCREMENTAL && comp->gcBytes > comp->gcTriggerBytes;
    for (;;) {
        if (JS_UNLIKELY(runGC)) {
            RunLastDitchGC(cx);

            /*
             * The JSGC_END callback can legitimately allocate new GC
             * things and populate the free list. If that happens, just
             * return that list head.
             */
            size_t thingSize = Arena::thingSize(thingKind);
            if (void *thing = comp->arenas.allocateFromFreeList(thingKind, thingSize))
                return thing;
        }

        /*
         * allocateFromArena may fail while the background finalization still
         * run. In that case we want to wait for it to finish and restart.
         * However, checking for that is racy as the background finalization
         * could free some things after allocateFromArena decided to fail but
         * at this point it may have already stopped. To avoid this race we
         * always try to allocate twice.
         */
        for (bool secondAttempt = false; ; secondAttempt = true) {
            void *thing = comp->arenas.allocateFromArena(comp, thingKind);
            if (JS_LIKELY(!!thing))
                return thing;
            if (secondAttempt)
                break;

            AutoLockGC lock(rt);
#ifdef JS_THREADSAFE
            rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
        }

        /*
         * We failed to allocate. Run the GC if we haven't done it already.
         * Otherwise report OOM.
         */
        if (runGC)
            break;
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

void
InitTracer(JSTracer *trc, JSRuntime *rt, JSContext *cx, JSTraceCallback callback)
{
    trc->runtime = rt;
    trc->context = cx;
    trc->callback = callback;
    trc->debugPrinter = NULL;
    trc->debugPrintArg = NULL;
    trc->debugPrintIndex = size_t(-1);
    trc->eagerlyTraceWeakMaps = true;
}

/* static */ int64_t
SliceBudget::TimeBudget(int64_t millis)
{
    return millis * PRMJ_USEC_PER_MSEC;
}

/* static */ int64_t
SliceBudget::WorkBudget(int64_t work)
{
    return -work;
}

SliceBudget::SliceBudget()
  : deadline(INT64_MAX),
    counter(INTPTR_MAX)
{
}

SliceBudget::SliceBudget(int64_t budget)
{
    if (budget == Unlimited) {
        deadline = INT64_MAX;
        counter = INTPTR_MAX;
    } else if (budget > 0) {
        deadline = PRMJ_Now() + budget;
        counter = CounterReset;
    } else {
        deadline = 0;
        counter = -budget;
    }
}

bool
SliceBudget::checkOverBudget()
{
    bool over = PRMJ_Now() > deadline;
    if (!over)
        counter = CounterReset;
    return over;
}

GCMarker::GCMarker(size_t sizeLimit)
  : stack(sizeLimit),
    color(BLACK),
    started(false),
    unmarkedArenaStackTop(NULL),
    markLaterArenas(0),
    grayFailed(false)
{
}

bool
GCMarker::init(bool lazy)
{
    if (!stack.init(lazy ? 0 : MARK_STACK_LENGTH))
        return false;
    return true;
}

void
GCMarker::start(JSRuntime *rt, JSContext *cx)
{
    InitTracer(this, rt, cx, NULL);
    JS_ASSERT(!started);
    started = true;
    color = BLACK;

    JS_ASSERT(!unmarkedArenaStackTop);
    JS_ASSERT(markLaterArenas == 0);

    JS_ASSERT(grayRoots.empty());
    JS_ASSERT(!grayFailed);

    /*
     * The GC is recomputing the liveness of WeakMap entries, so we delay
     * visting entries.
     */
    eagerlyTraceWeakMaps = JS_FALSE;
}

void
GCMarker::stop()
{
    JS_ASSERT(isDrained());

    JS_ASSERT(started);
    started = false;

    JS_ASSERT(!unmarkedArenaStackTop);
    JS_ASSERT(markLaterArenas == 0);

    JS_ASSERT(grayRoots.empty());
    grayFailed = false;
}

void
GCMarker::reset()
{
    color = BLACK;

    stack.reset();
    JS_ASSERT(isMarkStackEmpty());

    while (unmarkedArenaStackTop) {
        ArenaHeader *aheader = unmarkedArenaStackTop;
        JS_ASSERT(aheader->hasDelayedMarking);
        JS_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = aheader->getNextDelayedMarking();
        aheader->hasDelayedMarking = 0;
        aheader->markOverflow = 0;
        aheader->allocatedDuringIncremental = 0;
        markLaterArenas--;
    }
    JS_ASSERT(isDrained());
    JS_ASSERT(!markLaterArenas);

    grayRoots.clearAndFree();
    grayFailed = false;
}

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

inline void
GCMarker::delayMarkingArena(ArenaHeader *aheader)
{
    if (aheader->hasDelayedMarking) {
        /* Arena already scheduled to be marked later */
        return;
    }
    aheader->setNextDelayedMarking(unmarkedArenaStackTop);
    unmarkedArenaStackTop = aheader;
    markLaterArenas++;
}

void
GCMarker::delayMarkingChildren(const void *thing)
{
    const Cell *cell = reinterpret_cast<const Cell *>(thing);
    cell->arenaHeader()->markOverflow = 1;
    delayMarkingArena(cell->arenaHeader());
}

void
GCMarker::markDelayedChildren(ArenaHeader *aheader)
{
    if (aheader->markOverflow) {
        bool always = aheader->allocatedDuringIncremental;
        aheader->markOverflow = 0;

        for (CellIterUnderGC i(aheader); !i.done(); i.next()) {
            Cell *t = i.getCell();
            if (always || t->isMarked()) {
                t->markIfUnmarked();
                JS_TraceChildren(this, t, MapAllocToTraceKind(aheader->getAllocKind()));
            }
        }
    } else {
        JS_ASSERT(aheader->allocatedDuringIncremental);
        PushArena(this, aheader);
    }
    aheader->allocatedDuringIncremental = 0;
}

bool
GCMarker::markDelayedChildren(SliceBudget &budget)
{
    gcstats::AutoPhase ap(runtime->gcStats, gcstats::PHASE_MARK_DELAYED);

    JS_ASSERT(unmarkedArenaStackTop);
    do {
        /*
         * If marking gets delayed at the same arena again, we must repeat
         * marking of its things. For that we pop arena from the stack and
         * clear its hasDelayedMarking flag before we begin the marking.
         */
        ArenaHeader *aheader = unmarkedArenaStackTop;
        JS_ASSERT(aheader->hasDelayedMarking);
        JS_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = aheader->getNextDelayedMarking();
        aheader->hasDelayedMarking = 0;
        markLaterArenas--;
        markDelayedChildren(aheader);

        if (budget.checkOverBudget())
            return false;
    } while (unmarkedArenaStackTop);
    JS_ASSERT(!markLaterArenas);

    return true;
}

#ifdef DEBUG
void
GCMarker::checkCompartment(void *p)
{
    JS_ASSERT(started);

    Cell *cell = static_cast<Cell *>(p);
    if (runtime->gcRunning && runtime->gcCurrentCompartment)
        JS_ASSERT(cell->compartment() == runtime->gcCurrentCompartment);
    else if (runtime->gcIncrementalCompartment)
        JS_ASSERT(cell->compartment() == runtime->gcIncrementalCompartment);
}
#endif

bool
GCMarker::hasBufferedGrayRoots() const
{
    return !grayFailed;
}

void
GCMarker::startBufferingGrayRoots()
{
    JS_ASSERT(!callback);
    callback = GrayCallback;
    JS_ASSERT(IS_GC_MARKING_TRACER(this));
}

void
GCMarker::endBufferingGrayRoots()
{
    JS_ASSERT(callback == GrayCallback);
    callback = NULL;
    JS_ASSERT(IS_GC_MARKING_TRACER(this));
}

void
GCMarker::markBufferedGrayRoots()
{
    JS_ASSERT(!grayFailed);

    for (GrayRoot *elem = grayRoots.begin(); elem != grayRoots.end(); elem++) {
#ifdef DEBUG
        debugPrinter = elem->debugPrinter;
        debugPrintArg = elem->debugPrintArg;
        debugPrintIndex = elem->debugPrintIndex;
#endif
        MarkKind(this, elem->thing, elem->kind);
    }

    grayRoots.clearAndFree();
}

void
GCMarker::appendGrayRoot(void *thing, JSGCTraceKind kind)
{
    JS_ASSERT(started);

    if (grayFailed)
        return;

    GrayRoot root(thing, kind);
#ifdef DEBUG
    root.debugPrinter = debugPrinter;
    root.debugPrintArg = debugPrintArg;
    root.debugPrintIndex = debugPrintIndex;
#endif

    if (!grayRoots.append(root)) {
        grayRoots.clearAndFree();
        grayFailed = true;
    }
}

void
GCMarker::GrayCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    GCMarker *gcmarker = static_cast<GCMarker *>(trc);
    gcmarker->appendGrayRoot(*thingp, kind);
}

size_t
GCMarker::sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf) const
{
    return stack.sizeOfExcludingThis(mallocSizeOf) +
           grayRoots.sizeOfExcludingThis(mallocSizeOf);
}

void
SetMarkStackLimit(JSRuntime *rt, size_t limit)
{
    JS_ASSERT(!rt->gcRunning);
    rt->gcMarker.setSizeLimit(limit);
    for (CompartmentsIter c(rt); !c.done(); c.next())
        c->barrierMarker_.setSizeLimit(limit);
}

} /* namespace js */

#ifdef DEBUG
static void
EmptyMarkCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind)
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
        JS_ASSERT(trc->runtime == trc->context->runtime);
        JS_TracerInit(&checker, trc->context, EmptyMarkCallback);
        ConservativeGCTest test = MarkIfGCThingWord(&checker, reinterpret_cast<uintptr_t>(ptr));
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
        MarkGCThingRoot(trc, *reinterpret_cast<void **>(entry.key), name);
    else
        MarkValueRoot(trc, reinterpret_cast<Value *>(entry.key), name);
}

static void
gc_lock_traversal(const GCLocks::Entry &entry, JSTracer *trc)
{
    JS_ASSERT(entry.value >= 1);
    MarkGCThingRoot(trc, entry.key, "locked object");
}

namespace js {

void
MarkCompartmentActive(StackFrame *fp)
{
    if (fp->isScriptFrame())
        fp->script()->compartment()->active = true;
}

} /* namespace js */

void
AutoIdArray::trace(JSTracer *trc)
{
    JS_ASSERT(tag == IDARRAY);
    gc::MarkIdRange(trc, idArray->length, idArray->vector, "JSAutoIdArray.idArray");
}

void
AutoEnumStateRooter::trace(JSTracer *trc)
{
    gc::MarkObjectRoot(trc, &obj, "JS::AutoEnumStateRooter.obj");
}

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag) {
      case JSVAL:
        MarkValueRoot(trc, &static_cast<AutoValueRooter *>(this)->val, "JS::AutoValueRooter.val");
        return;

      case PARSER:
        static_cast<Parser *>(this)->trace(trc);
        return;

      case ENUMERATOR:
        static_cast<AutoEnumStateRooter *>(this)->trace(trc);
        return;

      case IDARRAY: {
        JSIdArray *ida = static_cast<AutoIdArray *>(this)->idArray;
        MarkIdRange(trc, ida->length, ida->vector, "JS::AutoIdArray.idArray");
        return;
      }

      case DESCRIPTORS: {
        PropDescArray &descriptors =
            static_cast<AutoPropDescArrayRooter *>(this)->descriptors;
        for (size_t i = 0, len = descriptors.length(); i < len; i++) {
            PropDesc &desc = descriptors[i];
            MarkValueRoot(trc, &desc.pd, "PropDesc::pd");
            MarkValueRoot(trc, &desc.value, "PropDesc::value");
            MarkValueRoot(trc, &desc.get, "PropDesc::get");
            MarkValueRoot(trc, &desc.set, "PropDesc::set");
        }
        return;
      }

      case DESCRIPTOR : {
        PropertyDescriptor &desc = *static_cast<AutoPropertyDescriptorRooter *>(this);
        if (desc.obj)
            MarkObjectRoot(trc, &desc.obj, "Descriptor::obj");
        MarkValueRoot(trc, &desc.value, "Descriptor::value");
        if ((desc.attrs & JSPROP_GETTER) && desc.getter) {
            JSObject *tmp = JS_FUNC_TO_DATA_PTR(JSObject *, desc.getter);
            MarkObjectRoot(trc, &tmp, "Descriptor::get");
            desc.getter = JS_DATA_TO_FUNC_PTR(JSPropertyOp, tmp);
        }
        if (desc.attrs & JSPROP_SETTER && desc.setter) {
            JSObject *tmp = JS_FUNC_TO_DATA_PTR(JSObject *, desc.setter);
            MarkObjectRoot(trc, &tmp, "Descriptor::set");
            desc.setter = JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, tmp);
        }
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
        if (static_cast<AutoObjectRooter *>(this)->obj)
            MarkObjectRoot(trc, &static_cast<AutoObjectRooter *>(this)->obj,
                           "JS::AutoObjectRooter.obj");
        return;

      case ID:
        MarkIdRoot(trc, &static_cast<AutoIdRooter *>(this)->id_, "JS::AutoIdRooter.id_");
        return;

      case VALVECTOR: {
        AutoValueVector::VectorImpl &vector = static_cast<AutoValueVector *>(this)->vector;
        MarkValueRootRange(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }

      case STRING:
        if (static_cast<AutoStringRooter *>(this)->str)
            MarkStringRoot(trc, &static_cast<AutoStringRooter *>(this)->str,
                           "JS::AutoStringRooter.str");
        return;

      case IDVECTOR: {
        AutoIdVector::VectorImpl &vector = static_cast<AutoIdVector *>(this)->vector;
        MarkIdRootRange(trc, vector.length(), vector.begin(), "js::AutoIdVector.vector");
        return;
      }

      case SHAPEVECTOR: {
        AutoShapeVector::VectorImpl &vector = static_cast<js::AutoShapeVector *>(this)->vector;
        MarkShapeRootRange(trc, vector.length(), const_cast<Shape **>(vector.begin()), 
                           "js::AutoShapeVector.vector");
        return;
      }

      case OBJVECTOR: {
        AutoObjectVector::VectorImpl &vector = static_cast<AutoObjectVector *>(this)->vector;
        MarkObjectRootRange(trc, vector.length(), vector.begin(), "js::AutoObjectVector.vector");
        return;
      }

      case VALARRAY: {
        AutoValueArray *array = static_cast<AutoValueArray *>(this);
        MarkValueRootRange(trc, array->length(), array->start(), "js::AutoValueArray");
        return;
      }
    }

    JS_ASSERT(tag >= 0);
    MarkValueRootRange(trc, tag, static_cast<AutoArrayRooter *>(this)->array,
                       "JS::AutoArrayRooter.array");
}

/* static */ void
AutoGCRooter::traceAll(JSTracer *trc)
{
    for (js::AutoGCRooter *gcr = trc->runtime->autoGCRooters; gcr; gcr = gcr->down)
        gcr->trace(trc);
}

namespace js {

static void
MarkRuntime(JSTracer *trc, bool useSavedRoots = false)
{
    JSRuntime *rt = trc->runtime;
    JS_ASSERT(trc->callback != GCMarker::GrayCallback);
    if (rt->gcCurrentCompartment) {
        for (CompartmentsIter c(rt); !c.done(); c.next())
            c->markCrossCompartmentWrappers(trc);
        Debugger::markCrossCompartmentDebuggerObjectReferents(trc);
    }

    AutoGCRooter::traceAll(trc);

    if (rt->hasContexts())
        MarkConservativeStackRoots(trc, useSavedRoots);

    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront())
        gc_root_traversal(trc, r.front());

    for (GCLocks::Range r = rt->gcLocksHash.all(); !r.empty(); r.popFront())
        gc_lock_traversal(r.front(), trc);

    if (rt->scriptPCCounters) {
        ScriptOpcodeCountsVector &vec = *rt->scriptPCCounters;
        for (size_t i = 0; i < vec.length(); i++)
            MarkScriptRoot(trc, &vec[i].script, "scriptPCCounters");
    }

    js_TraceAtomState(trc);
    rt->staticStrings.trace(trc);

    for (ContextIter acx(rt); !acx.done(); acx.next())
        acx->mark(trc);

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->activeAnalysis)
            c->markTypes(trc);

        /* During a GC, these are treated as weak pointers. */
        if (!IS_GC_MARKING_TRACER(trc)) {
            if (c->watchpointMap)
                c->watchpointMap->markAll(trc);
        }

        /* Do not discard scripts with counters while profiling. */
        if (rt->profilingScripts) {
            for (CellIterUnderGC i(c, FINALIZE_SCRIPT); !i.done(); i.next()) {
                JSScript *script = i.get<JSScript>();
                if (script->pcCounters) {
                    MarkScriptRoot(trc, &script, "profilingScripts");
                    JS_ASSERT(script == i.get<JSScript>());
                }
            }
        }
    }

#ifdef JS_METHODJIT
    /* We need to expand inline frames before stack scanning. */
    for (CompartmentsIter c(rt); !c.done(); c.next())
        mjit::ExpandInlineFrames(c);
#endif

    rt->stackSpace.mark(trc);

    /* The embedding can register additional roots here. */
    if (JSTraceDataOp op = rt->gcBlackRootsTraceOp)
        (*op)(trc, rt->gcBlackRootsData);

    /* During GC, this buffers up the gray roots and doesn't mark them. */
    if (JSTraceDataOp op = rt->gcGrayRootsTraceOp) {
        if (IS_GC_MARKING_TRACER(trc)) {
            GCMarker *gcmarker = static_cast<GCMarker *>(trc);
            gcmarker->startBufferingGrayRoots();
            (*op)(trc, rt->gcGrayRootsData);
            gcmarker->endBufferingGrayRoots();
        } else {
            (*op)(trc, rt->gcGrayRootsData);
        }
    }
}
void
TriggerGC(JSRuntime *rt, gcreason::Reason reason)
{
    JS_ASSERT(rt->onOwnerThread());

    if (rt->gcRunning || rt->gcIsNeeded)
        return;

    /* Trigger the GC when it is safe to call an operation callback. */
    rt->gcIsNeeded = true;
    rt->gcTriggerCompartment = NULL;
    rt->gcTriggerReason = reason;
    rt->triggerOperationCallback();
}

void
TriggerCompartmentGC(JSCompartment *comp, gcreason::Reason reason)
{
    JSRuntime *rt = comp->rt;
    JS_ASSERT(!rt->gcRunning);

    if (rt->gcZeal() == ZealAllocValue) {
        TriggerGC(rt, reason);
        return;
    }

    if (rt->gcMode == JSGC_MODE_GLOBAL || comp == rt->atomsCompartment) {
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

    /*
     * Trigger the GC when it is safe to call an operation callback on any
     * thread.
     */
    rt->gcIsNeeded = true;
    rt->gcTriggerCompartment = comp;
    rt->gcTriggerReason = reason;
    comp->rt->triggerOperationCallback();
}

void
MaybeGC(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(rt->onOwnerThread());

    if (rt->gcZeal() == ZealAllocValue || rt->gcZeal() == ZealPokeValue) {
        GC(cx, NULL, GC_NORMAL, gcreason::MAYBEGC);
        return;
    }

    JSCompartment *comp = cx->compartment;
    if (rt->gcIsNeeded) {
        GCSlice(cx, (comp == rt->gcTriggerCompartment) ? comp : NULL,
                GC_NORMAL, gcreason::MAYBEGC);
        return;
    }

    if (comp->gcBytes > 8192 &&
        comp->gcBytes >= 3 * (comp->gcTriggerBytes / 4) &&
        rt->gcIncrementalState == NO_INCREMENTAL)
    {
        GCSlice(cx, NULL, GC_NORMAL, gcreason::MAYBEGC);
        return;
    }

    /*
     * Access to the counters and, on 32 bit, setting gcNextFullGCTime below
     * is not atomic and a race condition could trigger or suppress the GC. We
     * tolerate this.
     */
    int64_t now = PRMJ_Now();
    if (rt->gcNextFullGCTime && rt->gcNextFullGCTime <= now) {
        if (rt->gcChunkAllocationSinceLastGC ||
            rt->gcNumArenasFreeCommitted > FreeCommittedArenasThreshold)
        {
            GCSlice(cx, NULL, GC_SHRINK, gcreason::MAYBEGC);
        } else {
            rt->gcNextFullGCTime = now + GC_IDLE_FULL_SPAN;
        }
    }
}

static void
DecommitArenasFromAvailableList(JSRuntime *rt, Chunk **availableListHeadp)
{
    Chunk *chunk = *availableListHeadp;
    if (!chunk)
        return;

    /*
     * Decommit is expensive so we avoid holding the GC lock while calling it.
     *
     * We decommit from the tail of the list to minimize interference with the
     * main thread that may start to allocate things at this point.
     *
     * The arena that is been decommitted outside the GC lock must not be
     * available for allocations either via the free list or via the
     * decommittedArenas bitmap. For that we just fetch the arena from the
     * free list before the decommit pretending as it was allocated. If this
     * arena also is the single free arena in the chunk, then we must remove
     * from the available list before we release the lock so the allocation
     * thread would not see chunks with no free arenas on the available list.
     *
     * After we retake the lock, we mark the arena as free and decommitted if
     * the decommit was successful. We must also add the chunk back to the
     * available list if we removed it previously or when the main thread
     * have allocated all remaining free arenas in the chunk.
     *
     * We also must make sure that the aheader is not accessed again after we
     * decommit the arena.
     */
    JS_ASSERT(chunk->info.prevp == availableListHeadp);
    while (Chunk *next = chunk->info.next) {
        JS_ASSERT(next->info.prevp == &chunk->info.next);
        chunk = next;
    }

    for (;;) {
        while (chunk->info.numArenasFreeCommitted != 0) {
            ArenaHeader *aheader = chunk->fetchNextFreeArena(rt);

            Chunk **savedPrevp = chunk->info.prevp;
            if (!chunk->hasAvailableArenas())
                chunk->removeFromAvailableList();

            size_t arenaIndex = Chunk::arenaIndex(aheader->arenaAddress());
            bool ok;
            {
                /*
                 * If the main thread waits for the decommit to finish, skip
                 * potentially expensive unlock/lock pair on the contested
                 * lock.
                 */
                Maybe<AutoUnlockGC> maybeUnlock;
                if (!rt->gcRunning)
                    maybeUnlock.construct(rt);
                ok = MarkPagesUnused(aheader->getArena(), ArenaSize);
            }

            if (ok) {
                ++chunk->info.numArenasFree;
                chunk->decommittedArenas.set(arenaIndex);
            } else {
                chunk->addArenaToFreeList(rt, aheader);
            }
            JS_ASSERT(chunk->hasAvailableArenas());
            JS_ASSERT(!chunk->unused());
            if (chunk->info.numArenasFree == 1) {
                /*
                 * Put the chunk back to the available list either at the
                 * point where it was before to preserve the available list
                 * that we enumerate, or, when the allocation thread has fully
                 * used all the previous chunks, at the beginning of the
                 * available list.
                 */
                Chunk **insertPoint = savedPrevp;
                if (savedPrevp != availableListHeadp) {
                    Chunk *prev = Chunk::fromPointerToNext(savedPrevp);
                    if (!prev->hasAvailableArenas())
                        insertPoint = availableListHeadp;
                }
                chunk->insertToAvailableList(insertPoint);
            } else {
                JS_ASSERT(chunk->info.prevp);
            }

            if (rt->gcChunkAllocationSinceLastGC) {
                /*
                 * The allocator thread has started to get new chunks. We should stop
                 * to avoid decommitting arenas in just allocated chunks.
                 */
                return;
            }
        }

        /*
         * chunk->info.prevp becomes null when the allocator thread consumed
         * all chunks from the available list.
         */
        JS_ASSERT_IF(chunk->info.prevp, *chunk->info.prevp == chunk);
        if (chunk->info.prevp == availableListHeadp || !chunk->info.prevp)
            break;

        /*
         * prevp exists and is not the list head. It must point to the next
         * field of the previous chunk.
         */
        chunk = chunk->getPrevious();
    }
}

static void
DecommitArenas(JSRuntime *rt)
{
    DecommitArenasFromAvailableList(rt, &rt->gcSystemAvailableChunkListHead);
    DecommitArenasFromAvailableList(rt, &rt->gcUserAvailableChunkListHead);
}

/* Must be called with the GC lock taken. */
static void
ExpireChunksAndArenas(JSRuntime *rt, bool shouldShrink)
{
    if (Chunk *toFree = rt->gcChunkPool.expire(rt, shouldShrink)) {
        AutoUnlockGC unlock(rt);
        FreeChunkList(toFree);
    }

    if (shouldShrink)
        DecommitArenas(rt);
}

#ifdef JS_THREADSAFE

static unsigned
GetCPUCount()
{
    static unsigned ncpus = 0;
    if (ncpus == 0) {
# ifdef XP_WIN
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        ncpus = unsigned(sysinfo.dwNumberOfProcessors);
# else
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        ncpus = (n > 0) ? unsigned(n) : 1;
# endif
    }
    return ncpus;
}

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

    backgroundAllocation = (GetCPUCount() >= 2);
    return true;
}

void
GCHelperThread::finish()
{
    PRThread *join = NULL;
    {
        AutoLockGC lock(rt);
        if (thread && state != SHUTDOWN) {
            /*
             * We cannot be in the ALLOCATING or CANCEL_ALLOCATION states as
             * the allocations should have been stopped during the last GC.
             */
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
                JS_ASSERT(chunk->info.numArenasFreeCommitted == ArenasPerChunk);
                rt->gcNumArenasFreeCommitted += ArenasPerChunk;
                rt->gcChunkPool.put(chunk);
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
GCHelperThread::prepareForBackgroundSweep()
{
    JS_ASSERT(state == IDLE);
    size_t maxArenaLists = MAX_BACKGROUND_FINALIZE_KINDS * rt->compartments.length();
    return finalizeVector.reserve(maxArenaLists);
}

/* Must be called with the GC lock taken. */
void
GCHelperThread::startBackgroundSweep(JSContext *cx, bool shouldShrink)
{
    /* The caller takes the GC lock. */
    JS_ASSERT(state == IDLE);
    JS_ASSERT(cx);
    JS_ASSERT(!finalizationContext);
    finalizationContext = cx;
    shrinkFlag = shouldShrink;
    state = SWEEPING;
    PR_NotifyCondVar(wakeup);
}

/* Must be called with the GC lock taken. */
void
GCHelperThread::startBackgroundShrink()
{
    switch (state) {
      case IDLE:
        JS_ASSERT(!finalizationContext);
        shrinkFlag = true;
        state = SWEEPING;
        PR_NotifyCondVar(wakeup);
        break;
      case SWEEPING:
        shrinkFlag = true;
        break;
      case ALLOCATING:
      case CANCEL_ALLOCATION:
        /*
         * If we have started background allocation there is nothing to
         * shrink.
         */
        break;
      case SHUTDOWN:
        JS_NOT_REACHED("No shrink on shutdown");
    }
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
    if (JSContext *cx = finalizationContext) {
        finalizationContext = NULL;
        AutoUnlockGC unlock(rt);

        /*
         * We must finalize in the insert order, see comments in
         * finalizeObjects.
         */
        for (ArenaHeader **i = finalizeVector.begin(); i != finalizeVector.end(); ++i)
            ArenaLists::backgroundFinalize(cx, *i);
        finalizeVector.resize(0);

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

    bool shrinking = shrinkFlag;
    ExpireChunksAndArenas(rt, shrinking);

    /*
     * The main thread may have called ShrinkGCBuffers while
     * ExpireChunksAndArenas(rt, false) was running, so we recheck the flag
     * afterwards.
     */
    if (!shrinking && shrinkFlag) {
        shrinkFlag = false;
        ExpireChunksAndArenas(rt, true);
    }
}

#endif /* JS_THREADSAFE */

} /* namespace js */

static bool
ReleaseObservedTypes(JSRuntime *rt)
{
    bool releaseTypes = false;
    int64_t now = PRMJ_Now();
    if (now >= rt->gcJitReleaseTime) {
        releaseTypes = true;
        rt->gcJitReleaseTime = now + JIT_SCRIPT_RELEASE_TYPES_INTERVAL;
    }

    return releaseTypes;
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
            (compartment->arenas.arenaListsAreEmpty() || !rt->hasContexts()))
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
PurgeRuntime(JSRuntime *rt)
{
    for (GCCompartmentsIter c(rt); !c.done(); c.next())
        c->purge();

    rt->tempLifoAlloc.freeUnused();
    rt->gsnCache.purge();

    /* FIXME: bug 506341 */
    rt->propertyCache.purge(rt);

    for (ContextIter acx(rt); !acx.done(); acx.next())
        acx->purge();
}

static void
BeginMarkPhase(JSRuntime *rt)
{
    GCMarker *gcmarker = &rt->gcMarker;

    rt->gcStartNumber = rt->gcNumber;

    /* Reset weak map list. */
    WeakMapBase::resetWeakMapList(rt);

    /*
     * We must purge the runtime at the beginning of an incremental GC. The
     * danger if we purge later is that the snapshot invariant of incremental
     * GC will be broken, as follows. If some object is reachable only through
     * some cache (say the dtoaCache) then it will not be part of the snapshot.
     * If we purge after root marking, then the mutator could obtain a pointer
     * to the object and start using it. This object might never be marked, so
     * a GC hazard would exist.
     */
    PurgeRuntime(rt);

    /*
     * Mark phase.
     */
    gcstats::AutoPhase ap1(rt->gcStats, gcstats::PHASE_MARK);
    gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_MARK_ROOTS);

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    MarkRuntime(gcmarker);
}

void
MarkWeakReferences(GCMarker *gcmarker)
{
    JS_ASSERT(gcmarker->isDrained());
    while (WatchpointMap::markAllIteratively(gcmarker) ||
           WeakMapBase::markAllIteratively(gcmarker) ||
           Debugger::markAllIteratively(gcmarker))
    {
        SliceBudget budget;
        gcmarker->drainMarkStack(budget);
    }
    JS_ASSERT(gcmarker->isDrained());
}

static void
MarkGrayAndWeak(JSRuntime *rt)
{
    FullGCMarker *gcmarker = &rt->gcMarker;

    JS_ASSERT(gcmarker->isDrained());
    MarkWeakReferences(gcmarker);

    gcmarker->setMarkColorGray();
    if (gcmarker->hasBufferedGrayRoots()) {
        gcmarker->markBufferedGrayRoots();
    } else {
        if (JSTraceDataOp op = rt->gcGrayRootsTraceOp)
            (*op)(gcmarker, rt->gcGrayRootsData);
    }
    SliceBudget budget;
    gcmarker->drainMarkStack(budget);
    MarkWeakReferences(gcmarker);
    JS_ASSERT(gcmarker->isDrained());
}

#ifdef DEBUG
static void
ValidateIncrementalMarking(JSContext *cx);
#endif

static void
EndMarkPhase(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    {
        gcstats::AutoPhase ap1(rt->gcStats, gcstats::PHASE_MARK);
        gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_MARK_OTHER);
        MarkGrayAndWeak(rt);
    }

    JS_ASSERT(rt->gcMarker.isDrained());

#ifdef DEBUG
    if (rt->gcIncrementalState != NO_INCREMENTAL)
        ValidateIncrementalMarking(cx);
#endif

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

#ifdef DEBUG
static void
ValidateIncrementalMarking(JSContext *cx)
{
    typedef HashMap<Chunk *, uintptr_t *, GCChunkHasher, SystemAllocPolicy> BitmapMap;
    BitmapMap map;
    if (!map.init())
        return;

    JSRuntime *rt = cx->runtime;
    FullGCMarker *gcmarker = &rt->gcMarker;

    /* Save existing mark bits. */
    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront()) {
        ChunkBitmap *bitmap = &r.front()->bitmap;
        uintptr_t *entry = (uintptr_t *)js_malloc(sizeof(bitmap->bitmap));
        if (!entry)
            return;

        memcpy(entry, bitmap->bitmap, sizeof(bitmap->bitmap));
        if (!map.putNew(r.front(), entry))
            return;
    }

    /* Save the existing weakmaps. */
    WeakMapVector weakmaps;
    if (!WeakMapBase::saveWeakMapList(rt, weakmaps))
        return;

    /*
     * After this point, the function should run to completion, so we shouldn't
     * do anything fallible.
     */

    /* Re-do all the marking, but non-incrementally. */
    js::gc::State state = rt->gcIncrementalState;
    rt->gcIncrementalState = NO_INCREMENTAL;

    /* As we're re-doing marking, we need to reset the weak map list. */
    WeakMapBase::resetWeakMapList(rt);

    JS_ASSERT(gcmarker->isDrained());
    gcmarker->reset();

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    MarkRuntime(gcmarker, true);
    SliceBudget budget;
    rt->gcMarker.drainMarkStack(budget);
    MarkGrayAndWeak(rt);

    /* Now verify that we have the same mark bits as before. */
    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront()) {
        Chunk *chunk = r.front();
        ChunkBitmap *bitmap = &chunk->bitmap;
        uintptr_t *entry = map.lookup(r.front())->value;
        ChunkBitmap incBitmap;

        memcpy(incBitmap.bitmap, entry, sizeof(incBitmap.bitmap));
        js_free(entry);

        for (size_t i = 0; i < ArenasPerChunk; i++) {
            Arena *arena = &chunk->arenas[i];
            if (!arena->aheader.allocated())
                continue;
            if (rt->gcCurrentCompartment && arena->aheader.compartment != rt->gcCurrentCompartment)
                continue;
            if (arena->aheader.allocatedDuringIncremental)
                continue;

            AllocKind kind = arena->aheader.getAllocKind();
            uintptr_t thing = arena->thingsStart(kind);
            uintptr_t end = arena->thingsEnd();
            while (thing < end) {
                Cell *cell = (Cell *)thing;
                if (bitmap->isMarked(cell, BLACK) && !incBitmap.isMarked(cell, BLACK)) {
                    JS_DumpHeap(cx, stdout, NULL, JSGCTraceKind(0), NULL, 100000, NULL);
                    printf("Assertion cell: %p (%d)\n", (void *)cell, cell->getAllocKind());
                }
                JS_ASSERT_IF(bitmap->isMarked(cell, BLACK), incBitmap.isMarked(cell, BLACK));
                thing += Arena::thingSize(kind);
            }
        }

        memcpy(bitmap->bitmap, incBitmap.bitmap, sizeof(incBitmap.bitmap));
    }

    /* Restore the weak map list. */
    WeakMapBase::resetWeakMapList(rt);
    WeakMapBase::restoreWeakMapList(rt, weakmaps);

    rt->gcIncrementalState = state;
}
#endif

static void
SweepPhase(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

#ifdef JS_THREADSAFE
    if (rt->hasContexts() && rt->gcHelperThread.prepareForBackgroundSweep())
        cx->gcBackgroundFree = &rt->gcHelperThread;
#endif

    /* Purge the ArenaLists before sweeping. */
    for (GCCompartmentsIter c(rt); !c.done(); c.next())
        c->arenas.purge();

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
    WeakMapBase::sweepAll(&rt->gcMarker);

    js_SweepAtomState(rt);

    /* Collect watch points associated with unreachable objects. */
    WatchpointMap::sweepAll(rt);

    if (!rt->gcCurrentCompartment)
        Debugger::sweepAll(cx);

    bool releaseTypes = !rt->gcCurrentCompartment && ReleaseObservedTypes(rt);
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
        ExpireChunksAndArenas(rt, gckind == GC_SHRINK);
#endif
    }

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_XPCONNECT);
        if (rt->gcCallback)
            (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
    }

    for (CompartmentsIter c(rt); !c.done(); c.next())
        c->setGCLastBytes(c->gcBytes, gckind);
}

/* Perform mark-and-sweep GC. If comp is set, we perform a single-compartment GC. */
static void
MarkAndSweep(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    AutoUnlockGC unlock(rt);

    rt->gcMarker.start(rt, cx);
    JS_ASSERT(!rt->gcMarker.callback);

    BeginMarkPhase(rt);
    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_MARK);
        SliceBudget budget;
        rt->gcMarker.drainMarkStack(budget);
    }
    EndMarkPhase(cx);
    SweepPhase(cx, gckind);

    rt->gcMarker.stop();
}

/*
 * This class should be used by any code that needs to exclusive access to the
 * heap in order to trace through it...
 */
class AutoHeapSession {
  public:
    explicit AutoHeapSession(JSRuntime *rt);
    ~AutoHeapSession();

  protected:
    JSRuntime *runtime;

  private:
    AutoHeapSession(const AutoHeapSession&) MOZ_DELETE;
    void operator=(const AutoHeapSession&) MOZ_DELETE;
};

/* ...while this class is to be used only for garbage collection. */
class AutoGCSession : AutoHeapSession {
  public:
    explicit AutoGCSession(JSContext *cx, JSCompartment *comp);
    ~AutoGCSession();

  private:
    /*
     * We should not be depending on cx->compartment in the GC, so set it to
     * NULL to look for violations.
     */
    SwitchToCompartment switcher;
};

/* Start a new heap session. */
AutoHeapSession::AutoHeapSession(JSRuntime *rt)
  : runtime(rt)
{
    JS_ASSERT(!rt->noGCOrAllocationCheck);
    JS_ASSERT(!rt->gcRunning);
    rt->gcRunning = true;
}

AutoHeapSession::~AutoHeapSession()
{
    JS_ASSERT(runtime->gcRunning);
    runtime->gcRunning = false;
}

AutoGCSession::AutoGCSession(JSContext *cx, JSCompartment *comp)
  : AutoHeapSession(cx->runtime),
    switcher(cx, (JSCompartment *)NULL)
{
    JS_ASSERT(!runtime->gcCurrentCompartment);
    runtime->gcCurrentCompartment = comp;

    runtime->gcIsNeeded = false;
    runtime->gcTriggerCompartment = NULL;
    runtime->gcInterFrameGC = true;

    runtime->gcNumber++;

    runtime->resetGCMallocBytes();

    /* Clear gcMallocBytes for all compartments */
    for (CompartmentsIter c(runtime); !c.done(); c.next())
        c->resetGCMallocBytes();
}

AutoGCSession::~AutoGCSession()
{
    runtime->gcCurrentCompartment = NULL;
    runtime->gcNextFullGCTime = PRMJ_Now() + GC_IDLE_FULL_SPAN;
    runtime->gcChunkAllocationSinceLastGC = false;
}

static void
ResetIncrementalGC(JSRuntime *rt, const char *reason)
{
    if (rt->gcIncrementalState == NO_INCREMENTAL)
        return;

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (!rt->gcIncrementalCompartment || rt->gcIncrementalCompartment == c) {
            c->needsBarrier_ = false;
            c->barrierMarker_.reset();
            c->barrierMarker_.stop();
        }
        JS_ASSERT(!c->needsBarrier_);
    }

    rt->gcIncrementalCompartment = NULL;
    rt->gcMarker.reset();
    rt->gcMarker.stop();
    rt->gcIncrementalState = NO_INCREMENTAL;

    rt->gcStats.reset(reason);
}

class AutoGCSlice {
  public:
    AutoGCSlice(JSContext *cx);
    ~AutoGCSlice();

  private:
    JSContext *context;
};

AutoGCSlice::AutoGCSlice(JSContext *cx)
  : context(cx)
{
    JSRuntime *rt = context->runtime;

    /*
     * During incremental GC, the compartment's active flag determines whether
     * there are stack frames active for any of its scripts. Normally this flag
     * is set at the beginning of the mark phase. During incremental GC, we also
     * set it at the start of every phase.
     */
    rt->stackSpace.markActiveCompartments();

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        /* Clear this early so we don't do any write barriers during GC. */
        if (rt->gcIncrementalState == MARK)
            c->needsBarrier_ = false;
        else
            JS_ASSERT(!c->needsBarrier_);
    }
}

AutoGCSlice::~AutoGCSlice()
{
    JSRuntime *rt = context->runtime;

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        if (rt->gcIncrementalState == MARK) {
            c->needsBarrier_ = true;
            c->arenas.prepareForIncrementalGC(c);
        } else {
            JS_ASSERT(rt->gcIncrementalState == NO_INCREMENTAL);

            c->needsBarrier_ = false;
        }
    }
}

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

static void
IncrementalGCSlice(JSContext *cx, int64_t budget, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    AutoUnlockGC unlock(rt);
    AutoGCSlice slice(cx);

    gc::State initialState = rt->gcIncrementalState;

    if (rt->gcIncrementalState == NO_INCREMENTAL) {
        JS_ASSERT(!rt->gcIncrementalCompartment);
        rt->gcIncrementalCompartment = rt->gcCurrentCompartment;
        rt->gcIncrementalState = MARK_ROOTS;
        rt->gcLastMarkSlice = false;
    }

    if (rt->gcIncrementalState == MARK_ROOTS) {
        rt->gcMarker.start(rt, cx);
        JS_ASSERT(IS_GC_MARKING_TRACER(&rt->gcMarker));

        for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
            c->discardJitCode(cx);
            c->barrierMarker_.start(rt, NULL);
        }

        BeginMarkPhase(rt);

        rt->gcIncrementalState = MARK;
    }

    if (rt->gcIncrementalState == MARK) {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_MARK);
        SliceBudget sliceBudget(budget);

        /* If we needed delayed marking for gray roots, then collect until done. */
        if (!rt->gcMarker.hasBufferedGrayRoots())
            sliceBudget.reset();

        rt->gcMarker.context = cx;
        bool finished = rt->gcMarker.drainMarkStack(sliceBudget);

        for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
            c->barrierMarker_.context = cx;
            finished &= c->barrierMarker_.drainMarkStack(sliceBudget);
            c->barrierMarker_.context = NULL;
        }

        if (finished) {
            JS_ASSERT(rt->gcMarker.isDrained());
#ifdef DEBUG
            for (GCCompartmentsIter c(rt); !c.done(); c.next())
                JS_ASSERT(c->barrierMarker_.isDrained());
#endif
            if (initialState == MARK && !rt->gcLastMarkSlice)
                rt->gcLastMarkSlice = true;
            else
                rt->gcIncrementalState = SWEEP;
        }
    }

    if (rt->gcIncrementalState == SWEEP) {
        EndMarkPhase(cx);
        SweepPhase(cx, gckind);

        rt->gcMarker.stop();

        /* JIT code was already discarded during sweeping. */
        for (GCCompartmentsIter c(rt); !c.done(); c.next())
            c->barrierMarker_.stop();

        rt->gcIncrementalCompartment = NULL;

        rt->gcIncrementalState = NO_INCREMENTAL;
    }
}

class IncrementalSafety
{
    const char *reason_;

    IncrementalSafety(const char *reason) : reason_(reason) {}

  public:
    static IncrementalSafety Safe() { return IncrementalSafety(NULL); }
    static IncrementalSafety Unsafe(const char *reason) { return IncrementalSafety(reason); }

    typedef void (IncrementalSafety::* ConvertibleToBool)();
    void nonNull() {}

    operator ConvertibleToBool() const {
        return reason_ == NULL ? &IncrementalSafety::nonNull : 0;
    }

    const char *reason() {
        JS_ASSERT(reason_);
        return reason_;
    }
};

static IncrementalSafety
IsIncrementalGCSafe(JSRuntime *rt)
{
    if (rt->gcCompartmentCreated) {
        rt->gcCompartmentCreated = false;
        return IncrementalSafety::Unsafe("compartment created");
    }

    if (rt->gcKeepAtoms)
        return IncrementalSafety::Unsafe("gcKeepAtoms set");

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->activeAnalysis)
            return IncrementalSafety::Unsafe("activeAnalysis set");
    }

    if (!rt->gcIncrementalEnabled)
        return IncrementalSafety::Unsafe("incremental permanently disabled");

    return IncrementalSafety::Safe();
}

static void
BudgetIncrementalGC(JSRuntime *rt, int64_t *budget)
{
    IncrementalSafety safe = IsIncrementalGCSafe(rt);
    if (!safe) {
        ResetIncrementalGC(rt, safe.reason());
        *budget = SliceBudget::Unlimited;
        rt->gcStats.nonincremental(safe.reason());
        return;
    }

    if (rt->gcMode != JSGC_MODE_INCREMENTAL) {
        ResetIncrementalGC(rt, "GC mode change");
        *budget = SliceBudget::Unlimited;
        rt->gcStats.nonincremental("GC mode");
        return;
    }

#ifdef ANDROID
    JS_ASSERT(rt->gcIncrementalState == NO_INCREMENTAL);
    *budget = SliceBudget::Unlimited;
    rt->gcStats.nonincremental("Android");
    return;
#endif

    if (rt->gcIncrementalState != NO_INCREMENTAL &&
        rt->gcCurrentCompartment != rt->gcIncrementalCompartment)
    {
        ResetIncrementalGC(rt, "compartment change");
        return;
    }

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->gcBytes > c->gcTriggerBytes) {
            *budget = SliceBudget::Unlimited;
            rt->gcStats.nonincremental("allocation trigger");
            return;
        }
    }
}

/*
 * GC, repeatedly if necessary, until we think we have not created any new
 * garbage. We disable inlining to ensure that the bottom of the stack with
 * possible GC roots recorded in js_GC excludes any pointers we use during the
 * marking implementation.
 */
static JS_NEVER_INLINE void
GCCycle(JSContext *cx, JSCompartment *comp, int64_t budget, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    JS_ASSERT_IF(comp, comp != rt->atomsCompartment);
    JS_ASSERT_IF(comp, rt->gcMode != JSGC_MODE_GLOBAL);

    /* Recursive GC is no-op. */
    if (rt->gcRunning)
        return;

    AutoGCSession gcsession(cx, comp);

    /* Don't GC if we are reporting an OOM. */
    if (rt->inOOMReport)
        return;

#ifdef JS_THREADSAFE
    /*
     * As we about to purge caches and clear the mark bits we must wait for
     * any background finalization to finish. We must also wait for the
     * background allocation to finish so we can avoid taking the GC lock
     * when manipulating the chunks during the GC.
     */
    JS_ASSERT(!cx->gcBackgroundFree);
    rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();
#endif

    if (budget == SliceBudget::Unlimited) {
        /* If non-incremental GC was requested, reset incremental GC. */
        ResetIncrementalGC(rt, "requested");
        rt->gcStats.nonincremental("requested");
    } else {
        BudgetIncrementalGC(rt, &budget);
    }

    AutoCopyFreeListToArenas copy(rt);

    if (budget == SliceBudget::Unlimited && rt->gcIncrementalState == NO_INCREMENTAL)
        MarkAndSweep(cx, gckind);
    else
        IncrementalGCSlice(cx, budget, gckind);

#ifdef DEBUG
    if (rt->gcIncrementalState == NO_INCREMENTAL) {
        for (CompartmentsIter c(rt); !c.done(); c.next())
            JS_ASSERT(!c->needsBarrier_);
    }
#endif
#ifdef JS_THREADSAFE
    if (rt->gcIncrementalState == NO_INCREMENTAL) {
        if (cx->gcBackgroundFree) {
            JS_ASSERT(cx->gcBackgroundFree == &rt->gcHelperThread);
            cx->gcBackgroundFree = NULL;
            rt->gcHelperThread.startBackgroundSweep(cx, gckind == GC_SHRINK);
        }
    }
#endif
}

static void
Collect(JSContext *cx, JSCompartment *comp, int64_t budget,
        JSGCInvocationKind gckind, gcreason::Reason reason)
{
    JSRuntime *rt = cx->runtime;
    JS_AbortIfWrongThread(rt);

    JS_ASSERT_IF(budget != SliceBudget::Unlimited, JSGC_INCREMENTAL);

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

    RecordNativeStackTopForGC(rt);

    /* This is a heuristic to avoid resets. */
    if (rt->gcIncrementalState != NO_INCREMENTAL && !rt->gcIncrementalCompartment)
        comp = NULL;

    gcstats::AutoGCSlice agc(rt->gcStats, comp, reason);

    do {
        /*
         * Let the API user decide to defer a GC if it wants to (unless this
         * is the last context). Invoke the callback regardless.
         */
        if (rt->gcIncrementalState == NO_INCREMENTAL) {
            if (JSGCCallback callback = rt->gcCallback) {
                if (!callback(cx, JSGC_BEGIN) && rt->hasContexts())
                    return;
            }
        }

        {
            /* Lock out other GC allocator and collector invocations. */
            AutoLockGC lock(rt);
            rt->gcPoke = false;
            GCCycle(cx, comp, budget, gckind);
        }

        if (rt->gcIncrementalState == NO_INCREMENTAL) {
            if (JSGCCallback callback = rt->gcCallback)
                (void) callback(cx, JSGC_END);
        }

        /*
         * On shutdown, iterate until finalizers or the JSGC_END callback
         * stop creating garbage.
         */
    } while (!rt->hasContexts() && rt->gcPoke);
}

namespace js {

void
GC(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind, gcreason::Reason reason)
{
    Collect(cx, comp, SliceBudget::Unlimited, gckind, reason);
}

void
GCSlice(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind, gcreason::Reason reason)
{
    Collect(cx, comp, cx->runtime->gcSliceBudget, gckind, reason);
}

void
GCDebugSlice(JSContext *cx, int64_t objCount)
{
    Collect(cx, NULL, SliceBudget::WorkBudget(objCount), GC_NORMAL, gcreason::API);
}

void
ShrinkGCBuffers(JSRuntime *rt)
{
    AutoLockGC lock(rt);
    JS_ASSERT(!rt->gcRunning);
#ifndef JS_THREADSAFE
    ExpireChunksAndArenas(rt, true);
#else
    rt->gcHelperThread.startBackgroundShrink();
#endif
}

void
TraceRuntime(JSTracer *trc)
{
    JS_ASSERT(!IS_GC_MARKING_TRACER(trc));

#ifdef JS_THREADSAFE
    {
        JSRuntime *rt = trc->runtime;
        if (!rt->gcRunning) {
            AutoLockGC lock(rt);
            AutoHeapSession session(rt);

            rt->gcHelperThread.waitBackgroundSweepEnd();
            AutoUnlockGC unlock(rt);

            AutoCopyFreeListToArenas copy(rt);
            RecordNativeStackTopForGC(rt);
            MarkRuntime(trc);
            return;
        }
    }
#else
    AutoCopyFreeListToArenas copy(trc->runtime);
    RecordNativeStackTopForGC(trc->runtime);
#endif

    /*
     * Calls from inside a normal GC or a recursive calls are OK and do not
     * require session setup.
     */
    MarkRuntime(trc);
}

struct IterateArenaCallbackOp
{
    JSRuntime *rt;
    void *data;
    IterateArenaCallback callback;
    JSGCTraceKind traceKind;
    size_t thingSize;
    IterateArenaCallbackOp(JSRuntime *rt, void *data, IterateArenaCallback callback,
                           JSGCTraceKind traceKind, size_t thingSize)
        : rt(rt), data(data), callback(callback), traceKind(traceKind), thingSize(thingSize)
    {}
    void operator()(Arena *arena) { (*callback)(rt, data, arena, traceKind, thingSize); }
};

struct IterateCellCallbackOp
{
    JSRuntime *rt;
    void *data;
    IterateCellCallback callback;
    JSGCTraceKind traceKind;
    size_t thingSize;
    IterateCellCallbackOp(JSRuntime *rt, void *data, IterateCellCallback callback,
                          JSGCTraceKind traceKind, size_t thingSize)
        : rt(rt), data(data), callback(callback), traceKind(traceKind), thingSize(thingSize)
    {}
    void operator()(Cell *cell) { (*callback)(rt, data, cell, traceKind, thingSize); }
};

void
IterateCompartmentsArenasCells(JSRuntime *rt, void *data,
                               JSIterateCompartmentCallback compartmentCallback,
                               IterateArenaCallback arenaCallback,
                               IterateCellCallback cellCallback)
{
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoHeapSession session(rt);
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        (*compartmentCallback)(rt, data, c);

        for (size_t thingKind = 0; thingKind != FINALIZE_LIMIT; thingKind++) {
            JSGCTraceKind traceKind = MapAllocToTraceKind(AllocKind(thingKind));
            size_t thingSize = Arena::thingSize(AllocKind(thingKind));
            IterateArenaCallbackOp arenaOp(rt, data, arenaCallback, traceKind, thingSize);
            IterateCellCallbackOp cellOp(rt, data, cellCallback, traceKind, thingSize);
            ForEachArenaAndCell(c, AllocKind(thingKind), arenaOp, cellOp);
        }
    }
}

void
IterateChunks(JSRuntime *rt, void *data, IterateChunkCallback chunkCallback)
{
    /* :XXX: Any way to common this preamble with IterateCompartmentsArenasCells? */
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoHeapSession session(rt);
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
    AutoUnlockGC unlock(rt);

    for (js::GCChunkSet::Range r = rt->gcChunkSet.all(); !r.empty(); r.popFront())
        chunkCallback(rt, data, r.front());
}

void
IterateCells(JSRuntime *rt, JSCompartment *compartment, AllocKind thingKind,
             void *data, IterateCellCallback cellCallback)
{
    /* :XXX: Any way to common this preamble with IterateCompartmentsArenasCells? */
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoHeapSession session(rt);
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);

    JSGCTraceKind traceKind = MapAllocToTraceKind(thingKind);
    size_t thingSize = Arena::thingSize(thingKind);

    if (compartment) {
        for (CellIterUnderGC i(compartment, thingKind); !i.done(); i.next())
            cellCallback(rt, data, i.getCell(), traceKind, thingSize);
    } else {
        for (CompartmentsIter c(rt); !c.done(); c.next()) {
            for (CellIterUnderGC i(c, thingKind); !i.done(); i.next())
                cellCallback(rt, data, i.getCell(), traceKind, thingSize);
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

            /*
             * If we're in the middle of an incremental GC, we cancel
             * it. Otherwise we might fail the mark the newly created
             * compartment fully.
             */
            if (rt->gcIncrementalState == MARK) {
                rt->gcCompartmentCreated = true;

                /*
                 * Start the tracer so that it's legal to stop() it when
                 * resetting the GC.
                 */
                if (!rt->gcIncrementalCompartment)
                    compartment->barrierMarker_.start(rt, NULL);
            }

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
    JSRuntime *rt = cx->runtime;

    /*
     * If rt->gcDebugCompartmentGC is true, only GC the current
     * compartment. But don't GC the atoms compartment.
     */
    rt->gcTriggerCompartment = rt->gcDebugCompartmentGC ? cx->compartment : NULL;
    if (rt->gcTriggerCompartment == rt->atomsCompartment)
        rt->gcTriggerCompartment = NULL;

    RunLastDitchGC(cx);
#endif
}

#if defined(DEBUG) && defined(JSGC_ROOT_ANALYSIS) && !defined(JS_THREADSAFE)

static void
CheckStackRoot(JSTracer *trc, uintptr_t *w)
{
    /* Mark memory as defined for valgrind, as in MarkWordConservatively. */
#ifdef JS_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(&w, sizeof(w));
#endif

    ConservativeGCTest test = MarkIfGCThingWord(trc, *w, DONT_MARK_THING);

    if (test == CGCT_VALID) {
        JSContext *iter = NULL;
        bool matched = false;
        JSRuntime *rt = trc->runtime;
        for (unsigned i = 0; i < THING_ROOT_COUNT; i++) {
            Root<Cell*> *rooter = rt->thingGCRooters[i];
            while (rooter) {
                if (rooter->address() == (Cell **) w)
                    matched = true;
                rooter = rooter->previous();
            }
        }
        CheckRoot *check = rt->checkGCRooters;
        while (check) {
            if (check->contains(static_cast<uint8_t*>(w), sizeof(w)))
                matched = true;
            check = check->previous();
        }
        if (!matched) {
            /*
             * Only poison the last byte in the word. It is easy to get
             * accidental collisions when a value that does not occupy a full
             * word is used to overwrite a now-dead GC thing pointer. In this
             * case we want to avoid damaging the smaller value.
             */
            PoisonPtr(w);
        }
    }
}

static void
CheckStackRootsRange(JSTracer *trc, uintptr_t *begin, uintptr_t *end)
{
    JS_ASSERT(begin <= end);
    for (uintptr_t *i = begin; i != end; ++i)
        CheckStackRoot(trc, i);
}

void
CheckStackRoots(JSContext *cx)
{
    AutoCopyFreeListToArenas copy(cx->runtime);

    JSTracer checker;
    JS_TracerInit(&checker, cx, EmptyMarkCallback);

    ThreadData *td = JS_THREAD_DATA(cx);

    ConservativeGCThreadData *ctd = &td->conservativeGC;
    ctd->recordStackTop();

    JS_ASSERT(ctd->hasStackToScan());
    uintptr_t *stackMin, *stackEnd;
#if JS_STACK_GROWTH_DIRECTION > 0
    stackMin = td->nativeStackBase;
    stackEnd = ctd->nativeStackTop;
#else
    stackMin = ctd->nativeStackTop + 1;
    stackEnd = td->nativeStackBase;
#endif

    JS_ASSERT(stackMin <= stackEnd);
    CheckStackRootsRange(&checker, stackMin, stackEnd);
    CheckStackRootsRange(&checker, ctd->registerSnapshot.words,
                         ArrayEnd(ctd->registerSnapshot.words));
}

#endif /* DEBUG && JSGC_ROOT_ANALYSIS && !JS_THREADSAFE */

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
    uint32_t count;
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
    uint64_t number;

    /* This counts up to JS_VERIFIER_FREQ to decide whether to verify. */
    uint32_t count;

    /* This graph represents the initial GC "snapshot". */
    VerifyNode *curnode;
    VerifyNode *root;
    char *edgeptr;
    char *term;
    NodeMap nodemap;

    VerifyTracer(JSContext *cx) : root(NULL), nodemap(cx) {}
    ~VerifyTracer() { js_free(root); }
};

/*
 * This function builds up the heap snapshot by adding edges to the current
 * node.
 */
static void
AccumulateEdge(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    VerifyTracer *trc = (VerifyTracer *)jstrc;

    trc->edgeptr += sizeof(EdgeValue);
    if (trc->edgeptr >= trc->term) {
        trc->edgeptr = trc->term;
        return;
    }

    VerifyNode *node = trc->curnode;
    uint32_t i = node->count;

    node->edges[i].thing = *thingp;
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

    if (rt->gcVerifyData || rt->gcIncrementalState != NO_INCREMENTAL)
        return;

    AutoLockGC lock(rt);
    AutoHeapSession session(rt);

    if (!IsIncrementalGCSafe(rt))
        return;

#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();
#endif

    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);
    RecordNativeStackTopForGC(rt);

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    for (CompartmentsIter c(rt); !c.done(); c.next())
        c->discardJitCode(cx);

    PurgeRuntime(rt);

    VerifyTracer *trc = new (js_malloc(sizeof(VerifyTracer))) VerifyTracer(cx);

    rt->gcNumber++;
    trc->number = rt->gcNumber;
    trc->count = 0;

    JS_TracerInit(trc, cx, AccumulateEdge);

    const size_t size = 64 * 1024 * 1024;
    trc->root = (VerifyNode *)js_malloc(size);
    JS_ASSERT(trc->root);
    trc->edgeptr = (char *)trc->root;
    trc->term = trc->edgeptr + size;

    trc->nodemap.init();

    /* Create the root node. */
    trc->curnode = MakeNode(trc, NULL, JSGCTraceKind(0));

    /* We want MarkRuntime to save the roots to gcSavedRoots. */
    rt->gcIncrementalState = MARK_ROOTS;

    /* Make all the roots be edges emanating from the root node. */
    MarkRuntime(trc);

    VerifyNode *node = trc->curnode;
    if (trc->edgeptr == trc->term)
        goto oom;

    /* For each edge, make a node for it if one doesn't already exist. */
    while ((char *)node < trc->edgeptr) {
        for (uint32_t i = 0; i < node->count; i++) {
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
    rt->gcIncrementalState = MARK;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->needsBarrier_ = true;
        c->barrierMarker_.start(rt, NULL);
        c->arenas.prepareForIncrementalGC(c);
    }

    return;

oom:
    rt->gcIncrementalState = NO_INCREMENTAL;
    trc->~VerifyTracer();
    js_free(trc);
}

static void
MarkFromAutorooter(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    static_cast<Cell *>(*thingp)->markIfUnmarked();
}

static bool
IsMarkedOrAllocated(Cell *cell)
{
    return cell->isMarked() || cell->arenaHeader()->allocatedDuringIncremental;
}

const static uint32_t MAX_VERIFIER_EDGES = 1000;

/*
 * This function is called by EndVerifyBarriers for every heap edge. If the edge
 * already existed in the original snapshot, we "cancel it out" by overwriting
 * it with NULL. EndVerifyBarriers later asserts that the remaining non-NULL
 * edges (i.e., the ones from the original snapshot that must have been
 * modified) must point to marked objects.
 */
static void
CheckEdge(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    VerifyTracer *trc = (VerifyTracer *)jstrc;
    VerifyNode *node = trc->curnode;

    /* Avoid n^2 behavior. */
    if (node->count > MAX_VERIFIER_EDGES)
        return;

    for (uint32_t i = 0; i < node->count; i++) {
        if (node->edges[i].thing == *thingp) {
            JS_ASSERT(node->edges[i].kind == kind);
            node->edges[i].thing = NULL;
            return;
        }
    }

    /*
     * Anything that is reachable now should have been reachable before, or else
     * it should be marked.
     */
    NodeMap::Ptr p = trc->nodemap.lookup(*thingp);
    JS_ASSERT_IF(!p, IsMarkedOrAllocated(static_cast<Cell *>(*thingp)));
}

static void
CheckReachable(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    VerifyTracer *trc = (VerifyTracer *)jstrc;
    NodeMap::Ptr p = trc->nodemap.lookup(*thingp);
    JS_ASSERT_IF(!p, IsMarkedOrAllocated(static_cast<Cell *>(*thingp)));
}

static void
EndVerifyBarriers(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    AutoLockGC lock(rt);
    AutoHeapSession session(rt);

#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();
#endif

    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);
    RecordNativeStackTopForGC(rt);

    VerifyTracer *trc = (VerifyTracer *)rt->gcVerifyData;

    if (!trc)
        return;

    /*
     * We need to bump gcNumber so that the methodjit knows that jitcode has
     * been discarded.
     */
    JS_ASSERT(trc->number == rt->gcNumber);
    rt->gcNumber++;

    /* We need to disable barriers before tracing, which may invoke barriers. */
    for (CompartmentsIter c(rt); !c.done(); c.next())
        c->needsBarrier_ = false;

    for (CompartmentsIter c(rt); !c.done(); c.next())
        c->discardJitCode(cx);

    rt->gcVerifyData = NULL;
    rt->gcIncrementalState = NO_INCREMENTAL;

    JS_TracerInit(trc, cx, MarkFromAutorooter);

    AutoGCRooter::traceAll(trc);

    if (IsIncrementalGCSafe(rt)) {
        /*
         * Verify that all the current roots were reachable previously, or else
         * are marked.
         */
        JS_TracerInit(trc, cx, CheckReachable);
        MarkRuntime(trc, true);

        JS_TracerInit(trc, cx, CheckEdge);

        /* Start after the roots. */
        VerifyNode *node = NextNode(trc->root);
        while ((char *)node < trc->edgeptr) {
            trc->curnode = node;
            JS_TraceChildren(trc, node->thing, node->kind);

            if (node->count <= MAX_VERIFIER_EDGES) {
                for (uint32_t i = 0; i < node->count; i++) {
                    void *thing = node->edges[i].thing;
                    JS_ASSERT_IF(thing, IsMarkedOrAllocated(static_cast<Cell *>(thing)));
                }
            }

            node = NextNode(node);
        }
    }

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->barrierMarker_.reset();
        c->barrierMarker_.stop();
    }

    trc->~VerifyTracer();
    js_free(trc);
}

void
FinishVerifier(JSRuntime *rt)
{
    if (VerifyTracer *trc = (VerifyTracer *)rt->gcVerifyData) {
        trc->~VerifyTracer();
        js_free(trc);
    }
}

void
VerifyBarriers(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    if (rt->gcVerifyData)
        EndVerifyBarriers(cx);
    else
        StartVerifyBarriers(cx);
}

void
MaybeVerifyBarriers(JSContext *cx, bool always)
{
    if (cx->runtime->gcZeal() != ZealVerifierValue)
        return;

    uint32_t freq = cx->runtime->gcZealFrequency;

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

static void ReleaseAllJITCode(JSContext *cx)
{
#ifdef JS_METHODJIT
    for (GCCompartmentsIter c(cx->runtime); !c.done(); c.next()) {
        mjit::ClearAllFrames(c);
        for (CellIter i(c, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            mjit::ReleaseScriptCode(cx, script);
        }
    }
#endif
}

/*
 * There are three possible PCCount profiling states:
 *
 * 1. None: Neither scripts nor the runtime have counter information.
 * 2. Profile: Active scripts have counter information, the runtime does not.
 * 3. Query: Scripts do not have counter information, the runtime does.
 *
 * When starting to profile scripts, counting begins immediately, with all JIT
 * code discarded and recompiled with counters as necessary. Active interpreter
 * frames will not begin profiling until they begin executing another script
 * (via a call or return).
 *
 * The below API functions manage transitions to new states, according
 * to the table below.
 *
 *                                  Old State
 *                          -------------------------
 * Function                 None      Profile   Query
 * --------
 * StartPCCountProfiling    Profile   Profile   Profile
 * StopPCCountProfiling     None      Query     Query
 * PurgePCCounts            None      None      None
 */

static void
ReleaseScriptPCCounters(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(rt->scriptPCCounters);

    ScriptOpcodeCountsVector &vec = *rt->scriptPCCounters;

    for (size_t i = 0; i < vec.length(); i++)
        vec[i].counters.destroy(cx);

    cx->delete_(rt->scriptPCCounters);
    rt->scriptPCCounters = NULL;
}

JS_FRIEND_API(void)
StartPCCountProfiling(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);

    if (rt->profilingScripts)
        return;

    if (rt->scriptPCCounters)
        ReleaseScriptPCCounters(cx);

    ReleaseAllJITCode(cx);

    rt->profilingScripts = true;
}

JS_FRIEND_API(void)
StopPCCountProfiling(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);

    if (!rt->profilingScripts)
        return;
    JS_ASSERT(!rt->scriptPCCounters);

    ReleaseAllJITCode(cx);

    ScriptOpcodeCountsVector *vec = cx->new_<ScriptOpcodeCountsVector>(SystemAllocPolicy());
    if (!vec)
        return;

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        for (CellIter i(c, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            if (script->pcCounters && script->types) {
                ScriptOpcodeCountsPair info;
                info.script = script;
                info.counters.steal(script->pcCounters);
                if (!vec->append(info))
                    info.counters.destroy(cx);
            }
        }
    }

    rt->profilingScripts = false;
    rt->scriptPCCounters = vec;
}

JS_FRIEND_API(void)
PurgePCCounts(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);

    if (!rt->scriptPCCounters)
        return;
    JS_ASSERT(!rt->profilingScripts);

    ReleaseScriptPCCounters(cx);
}

} /* namespace js */

JS_PUBLIC_API(void)
JS_IterateCompartments(JSRuntime *rt, void *data,
                       JSIterateCompartmentCallback compartmentCallback)
{
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoHeapSession session(rt);

    for (CompartmentsIter c(rt); !c.done(); c.next())
        (*compartmentCallback)(rt, data, c);
}

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


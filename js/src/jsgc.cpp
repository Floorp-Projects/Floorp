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
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jshash.h"
#include "jsbit.h"
#include "jsclist.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
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
#include "jsparse.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsstr.h"
#include "methodjit/MethodJIT.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsprobes.h"
#include "jsobjinlines.h"
#include "jshashtable.h"
#include "jsweakmap.h"

#include "jsstrinlines.h"
#include "jscompartment.h"

#ifdef MOZ_VALGRIND
# define JS_VALGRIND
#endif
#ifdef JS_VALGRIND
# include <valgrind/memcheck.h>
#endif

using namespace js;
using namespace js::gc;

/*
 * Check that JSTRACE_XML follows JSTRACE_OBJECT and JSTRACE_STRING.
 */
JS_STATIC_ASSERT(JSTRACE_OBJECT == 0);
JS_STATIC_ASSERT(JSTRACE_STRING == 1);
JS_STATIC_ASSERT(JSTRACE_SHAPE  == 2);
JS_STATIC_ASSERT(JSTRACE_XML    == 3);

/*
 * JS_IS_VALID_TRACE_KIND assumes that JSTRACE_SHAPE is the last non-xml
 * trace kind when JS_HAS_XML_SUPPORT is false.
 */
JS_STATIC_ASSERT(JSTRACE_SHAPE + 1 == JSTRACE_XML);

#ifdef JS_GCMETER
# define METER(x)               ((void) (x))
# define METER_IF(condition, x) ((void) ((condition) && (x)))
#else
# define METER(x)               ((void) 0)
# define METER_IF(condition, x) ((void) 0)
#endif

# define METER_UPDATE_MAX(maxLval, rval)                                       \
    METER_IF((maxLval) < (rval), (maxLval) = (rval))

namespace js {
namespace gc {

/* This array should be const, but that doesn't link right under GCC. */
FinalizeKind slotsToThingKind[] = {
    /* 0 */  FINALIZE_OBJECT0,  FINALIZE_OBJECT2,  FINALIZE_OBJECT2,  FINALIZE_OBJECT4,
    /* 4 */  FINALIZE_OBJECT4,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,
    /* 8 */  FINALIZE_OBJECT8,  FINALIZE_OBJECT12, FINALIZE_OBJECT12, FINALIZE_OBJECT12,
    /* 12 */ FINALIZE_OBJECT12, FINALIZE_OBJECT16, FINALIZE_OBJECT16, FINALIZE_OBJECT16,
    /* 16 */ FINALIZE_OBJECT16
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(slotsToThingKind) == SLOTS_TO_THING_KIND_LIMIT);

#ifdef DEBUG
const uint8 GCThingSizeMap[] = {
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
    sizeof(JSFunction),         /* FINALIZE_FUNCTION            */
    sizeof(Shape),              /* FINALIZE_SHAPE               */
#if JS_HAS_XML_SUPPORT
    sizeof(JSXML),              /* FINALIZE_XML                 */
#endif
    sizeof(JSShortString),      /* FINALIZE_SHORT_STRING        */
    sizeof(JSString),           /* FINALIZE_STRING              */
    sizeof(JSString),           /* FINALIZE_EXTERNAL_STRING     */
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(GCThingSizeMap) == FINALIZE_LIMIT);

JS_FRIEND_API(size_t)
ArenaHeader::getThingSize() const
{
    return GCThingSizeMap[getThingKind()];
}
#endif

/* Initialize the arena and setup the free list. */
template<typename T>
inline FreeCell *
Arena<T>::buildFreeList()
{
    T *first = &t.things[0];
    T *last = &t.things[JS_ARRAY_LENGTH(t.things) - 1];
    for (T *thing = first; thing != last;) {
        T *following = thing + 1;
        thing->asFreeCell()->link = following->asFreeCell();
        thing = following;
    }
    last->asFreeCell()->link = NULL;
    return first->asFreeCell();
}

template<typename T>
inline bool
Arena<T>::finalize(JSContext *cx)
{
    JS_ASSERT(aheader.compartment);
    JS_ASSERT(!aheader.getMarkingDelay()->link);

    FreeCell *nextFree = aheader.freeList;
    FreeCell *freeList = NULL;
    FreeCell **tailp = &freeList;
    bool allClear = true;

    T *thingsEnd = &t.things[ThingsPerArena-1];
    T *thing = &t.things[0];
    thingsEnd++;

    if (!nextFree) {
        nextFree = thingsEnd->asFreeCell();
    } else {
        JS_ASSERT(thing->asFreeCell() <= nextFree);
        JS_ASSERT(nextFree < thingsEnd->asFreeCell());
    }

    for (;; thing++) {
        if (thing->asFreeCell() == nextFree) {
            if (thing == thingsEnd)
                break;
            nextFree = nextFree->link;
            if (!nextFree) {
                nextFree = thingsEnd->asFreeCell();
            } else {
                JS_ASSERT(thing->asFreeCell() < nextFree);
                JS_ASSERT(nextFree < thingsEnd->asFreeCell());
            }
        } else if (thing->asFreeCell()->isMarked()) {
            allClear = false;
            continue;
        } else {
            thing->finalize(cx);
#ifdef DEBUG
            memset(thing, JS_FREE_PATTERN, sizeof(T));
#endif
        }
        FreeCell *t = thing->asFreeCell();
        *tailp = t;
        tailp = &t->link;
    }

#ifdef DEBUG
    /* Check that the free list is consistent. */
    unsigned nfree = 0;
    if (freeList) {
        JS_ASSERT(tailp != &freeList);
        FreeCell *t = freeList;
        for (;;) {
            ++nfree;
            if (&t->link == tailp)
                break;
            JS_ASSERT(t < t->link);
            t = t->link;
        }
    }
    if (allClear) {
        JS_ASSERT(nfree == ThingsPerArena);
        JS_ASSERT(freeList == static_cast<Cell *>(&t.things[0]));
        JS_ASSERT(tailp == &t.things[ThingsPerArena-1].asFreeCell()->link);
    } else {
        JS_ASSERT(nfree < ThingsPerArena);
    }
#endif
    *tailp = NULL;
    aheader.freeList = freeList;
    return allClear;
}

/*
 * Finalize arenas from the list. On return listHeadp points to the list of
 * non-empty arenas.
 */
template<typename T>
static void
FinalizeArenas(JSContext *cx, ArenaHeader **listHeadp)
{
    ArenaHeader **ap = listHeadp;
    while (ArenaHeader *aheader = *ap) {
        bool allClear = aheader->getArena<T>()->finalize(cx);
        if (allClear) {
            *ap = aheader->next;
            aheader->chunk()->releaseArena(aheader);
        } else {
            ap = &aheader->next;
        }
    }
}

#ifdef DEBUG
bool
checkArenaListAllUnmarked(JSCompartment *comp)
{
    for (unsigned i = 0; i < FINALIZE_LIMIT; i++) {
        if (comp->arenas[i].markedThingsInArenaList())
            return false;
    }
    return true;
}
#endif

} /* namespace gc */
} /* namespace js */

void
JSCompartment::finishArenaLists()
{
    for (unsigned i = 0; i < FINALIZE_LIMIT; i++)
        arenas[i].releaseAll(i);
}

void
Chunk::init(JSRuntime *rt)
{
    info.runtime = rt;
    info.age = 0;
    info.emptyArenaLists.init();
    info.emptyArenaLists.cellFreeList = &arenas[0].aheader;
    ArenaHeader *aheader = &arenas[0].aheader;
    ArenaHeader *last = &arenas[JS_ARRAY_LENGTH(arenas) - 1].aheader;
    while (aheader < last) {
        ArenaHeader *following = reinterpret_cast<ArenaHeader *>(aheader->address() + ArenaSize);
        aheader->next = following;
        aheader->compartment = NULL;
        aheader = following;
    }
    last->next = NULL;
    last->compartment = NULL;
    info.numFree = ArenasPerChunk;
    for (size_t i = 0; i != JS_ARRAY_LENGTH(markingDelay); ++i)
        markingDelay[i].init();
}

bool
Chunk::unused()
{
    return info.numFree == ArenasPerChunk;
}

bool
Chunk::hasAvailableArenas()
{
    return info.numFree > 0;
}

bool
Chunk::withinArenasRange(Cell *cell)
{
    uintptr_t addr = uintptr_t(cell);
    if (addr >= uintptr_t(&arenas[0]) && addr < uintptr_t(&arenas[ArenasPerChunk]))
        return true;
    return false;
}

template <typename T>
ArenaHeader *
Chunk::allocateArena(JSContext *cx, unsigned thingKind)
{
    JSCompartment *comp = cx->compartment;
    JS_ASSERT(hasAvailableArenas());
    ArenaHeader *aheader = info.emptyArenaLists.getTypedFreeList(thingKind);
    if (!aheader) {
        aheader = info.emptyArenaLists.getOtherArena();
        aheader->freeList = aheader->getArena<T>()->buildFreeList();
    }
    JS_ASSERT(!aheader->compartment);
    JS_ASSERT(!aheader->getMarkingDelay()->link);
    aheader->compartment = comp;
    aheader->setThingKind(thingKind);
    --info.numFree;
    JSRuntime *rt = info.runtime;

    JS_ATOMIC_ADD(&rt->gcBytes, ArenaSize);
    JS_ATOMIC_ADD(&comp->gcBytes, ArenaSize);
    METER(JS_ATOMIC_INCREMENT(&rt->gcStats.nallarenas));
    if (comp->gcBytes >= comp->gcTriggerBytes)
        TriggerCompartmentGC(comp);

    return aheader;
}

void
Chunk::releaseArena(ArenaHeader *aheader)
{
    JSRuntime *rt = info.runtime;
#ifdef JS_THREADSAFE
    Maybe<AutoLockGC> maybeLock;
    if (rt->gcHelperThread.sweeping)
        maybeLock.construct(info.runtime);
#endif
    JSCompartment *comp = aheader->compartment;
    METER(rt->gcStats.afree++);
    JS_ASSERT(rt->gcStats.nallarenas != 0);
    METER(JS_ATOMIC_DECREMENT(&rt->gcStats.nallarenas));

    JS_ASSERT(size_t(rt->gcBytes) >= ArenaSize);
    JS_ASSERT(size_t(comp->gcBytes) >= ArenaSize);
#ifdef JS_THREADSAFE
    if (rt->gcHelperThread.sweeping) {
        rt->reduceGCTriggerBytes(GC_HEAP_GROWTH_FACTOR * ArenaSize);
        comp->reduceGCTriggerBytes(GC_HEAP_GROWTH_FACTOR * ArenaSize);
    }
#endif
    JS_ATOMIC_ADD(&rt->gcBytes, -ArenaSize);
    JS_ATOMIC_ADD(&comp->gcBytes, -ArenaSize);
    info.emptyArenaLists.insert(aheader);
    aheader->compartment = NULL;
    ++info.numFree;
    if (unused())
        info.age = 0;
}

JSRuntime *
Chunk::getRuntime()
{
    return info.runtime;
}

inline jsuword
GetGCChunk(JSRuntime *rt)
{
    void *p = rt->gcChunkAllocator->alloc();
#ifdef MOZ_GCTIMER
    if (p)
        JS_ATOMIC_INCREMENT(&newChunkCount);
#endif
    METER_IF(p, rt->gcStats.nchunks++);
    METER_UPDATE_MAX(rt->gcStats.maxnchunks, rt->gcStats.nchunks);
    return reinterpret_cast<jsuword>(p);
}

inline void
ReleaseGCChunk(JSRuntime *rt, jsuword chunk)
{
    void *p = reinterpret_cast<void *>(chunk);
    JS_ASSERT(p);
#ifdef MOZ_GCTIMER
    JS_ATOMIC_INCREMENT(&destroyChunkCount);
#endif
    JS_ASSERT(rt->gcStats.nchunks != 0);
    METER(rt->gcStats.nchunks--);
    rt->gcChunkAllocator->free_(p);
}

inline Chunk *
AllocateGCChunk(JSRuntime *rt)
{
    Chunk *p = (Chunk *)rt->gcChunkAllocator->alloc();
#ifdef MOZ_GCTIMER
    if (p)
        JS_ATOMIC_INCREMENT(&newChunkCount);
#endif
    METER_IF(p, rt->gcStats.nchunks++);
    return p;
}

inline void
ReleaseGCChunk(JSRuntime *rt, Chunk *p)
{
    JS_ASSERT(p);
#ifdef MOZ_GCTIMER
    JS_ATOMIC_INCREMENT(&destroyChunkCount);
#endif
    JS_ASSERT(rt->gcStats.nchunks != 0);
    METER(rt->gcStats.nchunks--);
    rt->gcChunkAllocator->free_(p);
}

inline Chunk *
PickChunk(JSContext *cx)
{
    Chunk *chunk = cx->compartment->chunk;
    if (chunk && chunk->hasAvailableArenas())
        return chunk;

    /*
     * The chunk used for the last allocation is full, search all chunks for
     * free arenas.
     */
    JSRuntime *rt = cx->runtime;
    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront()) {
        chunk = r.front();
        if (chunk->hasAvailableArenas()) {
            cx->compartment->chunk = chunk;
            return chunk;
        }
    }

    chunk = AllocateGCChunk(rt);
    if (!chunk)
        return NULL;

    /*
     * FIXME bug 583732 - chunk is newly allocated and cannot be present in
     * the table so using ordinary lookupForAdd is suboptimal here.
     */
    GCChunkSet::AddPtr p = rt->gcChunkSet.lookupForAdd(chunk);
    JS_ASSERT(!p);
    if (!rt->gcChunkSet.add(p, chunk)) {
        ReleaseGCChunk(rt, chunk);
        return NULL;
    }

    chunk->init(rt);
    cx->compartment->chunk = chunk;
    return chunk;
}

static void
ExpireGCChunks(JSRuntime *rt)
{
    static const size_t MaxAge = 3;

    /* Remove unused chunks. */
    AutoLockGC lock(rt);

    rt->gcChunksWaitingToExpire = 0;
    for (GCChunkSet::Enum e(rt->gcChunkSet); !e.empty(); e.popFront()) {
        Chunk *chunk = e.front();
        JS_ASSERT(chunk->info.runtime == rt);
        if (chunk->unused()) {
            if (chunk->info.age++ > MaxAge) {
                e.removeFront();
                ReleaseGCChunk(rt, chunk);
                continue;
            }
            rt->gcChunksWaitingToExpire++;
        }
    }
}

JS_FRIEND_API(bool)
IsAboutToBeFinalized(JSContext *cx, const void *thing)
{
    if (JSAtom::isStatic(thing))
        return false;
    JS_ASSERT(cx);

    JSCompartment *thingCompartment = reinterpret_cast<const Cell *>(thing)->compartment();
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(rt == thingCompartment->rt);
    if (rt->gcCurrentCompartment != NULL && rt->gcCurrentCompartment != thingCompartment)
        return false;

    return !reinterpret_cast<const Cell *>(thing)->isMarked();
}

JS_FRIEND_API(bool)
js_GCThingIsMarked(void *thing, uintN color = BLACK)
{
    JS_ASSERT(thing);
    AssertValidColor(thing, color);
    JS_ASSERT(!JSAtom::isStatic(thing));
    return reinterpret_cast<Cell *>(thing)->isMarked(color);
}

/*
 * 1/8 life for JIT code. After this number of microseconds have passed, 1/8 of all
 * JIT code is discarded in inactive compartments, regardless of how often that
 * code runs.
 */
static const int64 JIT_SCRIPT_EIGHTH_LIFETIME = 120 * 1000 * 1000;

JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes)
{
    /*
     * Make room for at least 16 chunks so the table would not grow before
     * the browser starts up.
     */
    if (!rt->gcChunkSet.init(16))
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
    if (!rt->gcHelperThread.init(rt))
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
    rt->setGCLastBytes(8192);

    rt->gcJitReleaseTime = PRMJ_Now() + JIT_SCRIPT_EIGHTH_LIFETIME;

    METER(PodZero(&rt->gcStats));
    return true;
}

namespace js {

inline bool
InFreeList(ArenaHeader *aheader, void *thing)
{
    for (FreeCell *cursor = aheader->freeList; cursor; cursor = cursor->link) {
        JS_ASSERT(!cursor->isMarked());
        JS_ASSERT_IF(cursor->link, cursor < cursor->link);

        /* If the cursor moves past the thing, it's not in the freelist. */
        if (thing < cursor)
            break;

        /* If we find it on the freelist, it's dead. */
        if (thing == cursor)
            return true;
    }
    return false;
}

template <typename T>
inline ConservativeGCTest
MarkArenaPtrConservatively(JSTracer *trc, ArenaHeader *aheader, uintptr_t addr)
{
    JS_ASSERT(aheader->compartment);
    JS_ASSERT(sizeof(T) == aheader->getThingSize());

    uintptr_t offset = (addr & ArenaMask) - Arena<T>::FirstThingOffset;
    if (offset >= Arena<T>::ThingsSpan)
        return CGCT_NOTARENA;

    /* addr can point inside the thing so we must align the address. */
    uintptr_t shift = offset % sizeof(T);
    T *thing = reinterpret_cast<T *>(addr - shift);

    if (InFreeList(aheader, thing))
        return CGCT_NOTLIVE;

    MarkRoot(trc, thing, "machine stack");

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    if (IS_GC_MARKING_TRACER(trc) && static_cast<GCMarker *>(trc)->conservativeDumpFileName)
        static_cast<GCMarker *>(trc)->conservativeRoots.append(thing);
#endif

#if defined JS_DUMP_CONSERVATIVE_GC_ROOTS || defined JS_GCMETER
    if (IS_GC_MARKING_TRACER(trc) && shift)
        static_cast<GCMarker *>(trc)->conservativeStats.unaligned++;
#endif
    return CGCT_VALID;
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

    if (!trc->context->runtime->gcChunkSet.has(chunk))
        return CGCT_NOTCHUNK;

    /*
     * We query for pointers outside the arena array after checking for an
     * allocated chunk. Such pointers are rare and we want to reject them
     * after doing more likely rejections.
     */
    if (!Chunk::withinArenasRange(addr))
        return CGCT_NOTARENA;

    ArenaHeader *aheader = &chunk->arenas[Chunk::arenaIndex(addr)].aheader;

    if (!aheader->compartment)
        return CGCT_FREEARENA;

    ConservativeGCTest test;
    unsigned thingKind = aheader->getThingKind();

    switch (thingKind) {
      case FINALIZE_OBJECT0:
      case FINALIZE_OBJECT0_BACKGROUND:
        test = MarkArenaPtrConservatively<JSObject>(trc, aheader, addr);
        break;
      case FINALIZE_OBJECT2:
      case FINALIZE_OBJECT2_BACKGROUND:
        test = MarkArenaPtrConservatively<JSObject_Slots2>(trc, aheader, addr);
        break;
      case FINALIZE_OBJECT4:
      case FINALIZE_OBJECT4_BACKGROUND:
        test = MarkArenaPtrConservatively<JSObject_Slots4>(trc, aheader, addr);
        break;
      case FINALIZE_OBJECT8:
      case FINALIZE_OBJECT8_BACKGROUND:
        test = MarkArenaPtrConservatively<JSObject_Slots8>(trc, aheader, addr);
        break;
      case FINALIZE_OBJECT12:
      case FINALIZE_OBJECT12_BACKGROUND:
        test = MarkArenaPtrConservatively<JSObject_Slots12>(trc, aheader, addr);
        break;
      case FINALIZE_OBJECT16:
      case FINALIZE_OBJECT16_BACKGROUND:
        test = MarkArenaPtrConservatively<JSObject_Slots16>(trc, aheader, addr);
        break;
      case FINALIZE_STRING:
        test = MarkArenaPtrConservatively<JSString>(trc, aheader, addr);
        break;
      case FINALIZE_EXTERNAL_STRING:
        test = MarkArenaPtrConservatively<JSExternalString>(trc, aheader, addr);
        break;
      case FINALIZE_SHORT_STRING:
        test = MarkArenaPtrConservatively<JSShortString>(trc, aheader, addr);
        break;
      case FINALIZE_FUNCTION:
        test = MarkArenaPtrConservatively<JSFunction>(trc, aheader, addr);
        break;
      case FINALIZE_SHAPE:
        test = MarkArenaPtrConservatively<Shape>(trc, aheader, addr);
        break;
#if JS_HAS_XML_SUPPORT
      case FINALIZE_XML:
        test = MarkArenaPtrConservatively<JSXML>(trc, aheader, addr);
        break;
#endif
      default:
        test = CGCT_WRONGTAG;
        JS_NOT_REACHED("wrong tag");
    }

    return test;
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
                            JS_ARRAY_END(ctd->registerSnapshot.words));
}

void
MarkStackRangeConservatively(JSTracer *trc, Value *beginv, Value *endv)
{
    const jsuword *begin = beginv->payloadWord();
    const jsuword *end = endv->payloadWord();;
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
    for (JSThread::Map::Range r = trc->context->runtime->threads.all(); !r.empty(); r.popFront()) {
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
    MarkThreadDataConservatively(trc, &trc->context->runtime->threadData);
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
#ifdef JS_ARENAMETER
    JS_DumpArenaStats(stdout);
#endif
#ifdef JS_GCMETER
    if (JS_WANT_GC_METER_PRINT)
        js_DumpGCStats(rt, stdout);
#endif

    /* Delete all remaining Compartments. */
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c) {
        JSCompartment *comp = *c;
        comp->finishArenaLists();
        Foreground::delete_(comp);
    }
    rt->compartments.clear();
    rt->atomsCompartment = NULL;

    rt->gcWeakMapList = NULL;

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        ReleaseGCChunk(rt, r.front());
    rt->gcChunkSet.clear();

#ifdef JS_THREADSAFE
    rt->gcHelperThread.finish(rt);
#endif

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
    JSBool ok = js_AddRootRT(cx->runtime, Jsvalify(vp), name);
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
JSRuntime::setGCLastBytes(size_t lastBytes)
{
    gcLastBytes = lastBytes;
    float trigger = float(Max(lastBytes, GC_ARENA_ALLOCATION_TRIGGER)) * GC_HEAP_GROWTH_FACTOR;
    gcTriggerBytes = size_t(Min(float(gcMaxBytes), trigger));
}

void
JSRuntime::reduceGCTriggerBytes(uint32 amount) {
    JS_ASSERT(amount > 0);
    JS_ASSERT((gcTriggerBytes - amount) > 0);
    if (gcTriggerBytes - amount < GC_ARENA_ALLOCATION_TRIGGER * GC_HEAP_GROWTH_FACTOR)
        return;
    gcTriggerBytes -= amount;
}

void
JSCompartment::setGCLastBytes(size_t lastBytes)
{
    gcLastBytes = lastBytes;
    float trigger = float(Max(lastBytes, GC_ARENA_ALLOCATION_TRIGGER)) * GC_HEAP_GROWTH_FACTOR;
    gcTriggerBytes = size_t(Min(float(rt->gcMaxBytes), trigger));
}

void
JSCompartment::reduceGCTriggerBytes(uint32 amount) {
    JS_ASSERT(amount > 0);
    JS_ASSERT((gcTriggerBytes - amount) > 0);
    if (gcTriggerBytes - amount < GC_ARENA_ALLOCATION_TRIGGER * GC_HEAP_GROWTH_FACTOR)
        return;
    gcTriggerBytes -= amount;
}

namespace js {
namespace gc {

void
FreeLists::purge()
{
    /*
     * Return the free list back to the arena so the GC finalization will not
     * run the finalizers over unitialized bytes from free things.
     */
    for (FreeCell **p = finalizables; p != JS_ARRAY_END(finalizables); ++p) {
        if (FreeCell *thing = *p) {
            JS_ASSERT(!thing->arenaHeader()->freeList);
            thing->arenaHeader()->freeList = thing;
            *p = NULL;
        }
    }
}

inline ArenaHeader *
ArenaList::searchForFreeArena()
{
    while (ArenaHeader *aheader = *cursor) {
        cursor = &aheader->next;
        if (aheader->freeList)
            return aheader;
    }
    return NULL;
}

template <typename T>
inline ArenaHeader *
ArenaList::getArenaWithFreeList(JSContext *cx, unsigned thingKind)
{
    Chunk *chunk;

#ifdef JS_THREADSAFE
    /*
     * We cannot search the arena list for free things while the
     * background finalization runs and can modify head or cursor at any
     * moment.
     */
    if (backgroundFinalizeState == BFS_DONE) {
      check_arena_list:
        if (ArenaHeader *aheader = searchForFreeArena())
            return aheader;
    }

    AutoLockGC lock(cx->runtime);

    for (;;) {
        if (backgroundFinalizeState == BFS_JUST_FINISHED) {
            /*
             * Before we took the GC lock or while waiting for the background
             * finalization to finish the latter added new arenas to the list.
             * Check the list again for free things outside the GC lock.
             */
            JS_ASSERT(*cursor);
            backgroundFinalizeState = BFS_DONE;
            goto check_arena_list;
        }

        JS_ASSERT(!*cursor);
        chunk = PickChunk(cx);
        if (chunk || backgroundFinalizeState == BFS_DONE)
            break;

        /*
         * If the background finalization still runs, wait for it to
         * finish and retry to check if it populated the arena list or
         * added new empty arenas.
         */
        JS_ASSERT(backgroundFinalizeState == BFS_RUN);
        cx->runtime->gcHelperThread.waitBackgroundSweepEnd(cx->runtime, false);
        JS_ASSERT(backgroundFinalizeState == BFS_JUST_FINISHED ||
                  backgroundFinalizeState == BFS_DONE);
    }

#else /* !JS_THREADSAFE */

    if (ArenaHeader *aheader = searchForFreeArena())
        return aheader;
    chunk = PickChunk(cx);

#endif /* !JS_THREADSAFE */

    if (!chunk) {
        TriggerGC(cx->runtime);
        return NULL;
    }

    /*
     * While we still hold the GC lock get the arena from the chunk and add it
     * to the head of the list before the cursor to prevent checking the arena
     * for the free things.
     */
    ArenaHeader *aheader = chunk->allocateArena<T>(cx, thingKind);
    aheader->next = head;
    if (cursor == &head)
        cursor = &aheader->next;
    head = aheader;
    return aheader;
}

template<typename T>
void
ArenaList::finalizeNow(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(backgroundFinalizeState == BFS_DONE);
#endif
    METER(stats.narenas = uint32(ArenaHeader::CountListLength(head)));
    FinalizeArenas<T>(cx, &head);
    METER(stats.livearenas = uint32(ArenaHeader::CountListLength(head)));
    cursor = &head;
}

#ifdef JS_THREADSAFE
template<typename T>
inline void
ArenaList::finalizeLater(JSContext *cx)
{
    JS_ASSERT_IF(head,
                 head->getThingKind() == FINALIZE_OBJECT0_BACKGROUND  ||
                 head->getThingKind() == FINALIZE_OBJECT2_BACKGROUND  ||
                 head->getThingKind() == FINALIZE_OBJECT4_BACKGROUND  ||
                 head->getThingKind() == FINALIZE_OBJECT8_BACKGROUND  ||
                 head->getThingKind() == FINALIZE_OBJECT12_BACKGROUND ||
                 head->getThingKind() == FINALIZE_OBJECT16_BACKGROUND ||
                 head->getThingKind() == FINALIZE_SHORT_STRING        ||
                 head->getThingKind() == FINALIZE_STRING);
    JS_ASSERT(!cx->runtime->gcHelperThread.sweeping);

    /*
     * The state can be just-finished if we have not allocated any GC things
     * from the arena list after the previous background finalization.
     */
    JS_ASSERT(backgroundFinalizeState == BFS_DONE ||
              backgroundFinalizeState == BFS_JUST_FINISHED);

    if (head && cx->gcBackgroundFree && cx->gcBackgroundFree->finalizeVector.append(head)) {
        head = NULL;
        cursor = &head;
        backgroundFinalizeState = BFS_RUN;
    } else {
        backgroundFinalizeState = BFS_DONE;
        finalizeNow<T>(cx);
    }
}

/*static*/ void
ArenaList::backgroundFinalize(JSContext *cx, ArenaHeader *listHead)
{
    JS_ASSERT(listHead);
    unsigned thingKind = listHead->getThingKind();
    JSCompartment *comp = listHead->compartment;
    ArenaList *al = &comp->arenas[thingKind];
    METER(al->stats.narenas = uint32(ArenaHeader::CountListLength(listHead)));

    switch (thingKind) {
      default:
        JS_NOT_REACHED("wrong kind");
        break;
      case FINALIZE_OBJECT0_BACKGROUND:
        FinalizeArenas<JSObject>(cx, &listHead);
        break;
      case FINALIZE_OBJECT2_BACKGROUND:
        FinalizeArenas<JSObject_Slots2>(cx, &listHead);
        break;
      case FINALIZE_OBJECT4_BACKGROUND:
        FinalizeArenas<JSObject_Slots4>(cx, &listHead);
        break;
      case FINALIZE_OBJECT8_BACKGROUND:
        FinalizeArenas<JSObject_Slots8>(cx, &listHead);
        break;
      case FINALIZE_OBJECT12_BACKGROUND:
        FinalizeArenas<JSObject_Slots12>(cx, &listHead);
        break;
      case FINALIZE_OBJECT16_BACKGROUND:
        FinalizeArenas<JSObject_Slots16>(cx, &listHead);
        break;
      case FINALIZE_STRING:
        FinalizeArenas<JSString>(cx, &listHead);
        break;
      case FINALIZE_SHORT_STRING:
        FinalizeArenas<JSShortString>(cx, &listHead);
        break;
    }

    /*
     * In stats we should not reflect the arenas allocated after the GC has
     * finished. So we do not add to livearenas the arenas from al->head.
     */
    METER(al->stats.livearenas = uint32(ArenaHeader::CountListLength(listHead)));

    /*
     * After we finish the finalization al->cursor must point to the end of
     * the head list as we emptied the list before the background finalization
     * and the allocation adds new arenas before the cursor.
     */
    AutoLockGC lock(cx->runtime);
    JS_ASSERT(al->backgroundFinalizeState == BFS_RUN);
    JS_ASSERT(!*al->cursor);
    if (listHead) {
        *al->cursor = listHead;
        al->backgroundFinalizeState = BFS_JUST_FINISHED;
    } else {
        al->backgroundFinalizeState = BFS_DONE;
    }
    METER(UpdateCompartmentGCStats(comp, thingKind));
}

#endif /* JS_THREADSAFE */

#ifdef DEBUG
bool
CheckAllocation(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->thread());
#endif
    JS_ASSERT(!cx->runtime->gcRunning);
    return true;
}
#endif

inline bool
NeedLastDitchGC(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
#ifdef JS_GC_ZEAL
    if (rt->gcZeal >= 1)
        return true;
#endif
    return rt->gcIsNeeded;
}

/*
 * Return false only if the GC run but could not bring its memory usage under
 * JSRuntime::gcMaxBytes.
 */
static bool
RunLastDitchGC(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    METER(rt->gcStats.lastditch++);
#ifdef JS_THREADSAFE
    Maybe<AutoUnlockAtomsCompartment> maybeUnlockAtomsCompartment;
    if (cx->compartment == rt->atomsCompartment && rt->atomsCompartmentIsLocked)
        maybeUnlockAtomsCompartment.construct(cx);
#endif
    /* The last ditch GC preserves all atoms. */
    AutoKeepAtoms keep(rt);
    js_GC(cx, rt->gcTriggerCompartment, GC_NORMAL);

#ifdef JS_THREADSAFE
    if (rt->gcBytes >= rt->gcMaxBytes)
        cx->runtime->gcHelperThread.waitBackgroundSweepEnd(cx->runtime);
#endif

    return rt->gcBytes < rt->gcMaxBytes;
}

template <typename T>
inline Cell *
RefillTypedFreeList(JSContext *cx, unsigned thingKind)
{
    JSCompartment *compartment = cx->compartment;
    JS_ASSERT(!compartment->freeLists.finalizables[thingKind]);

    bool canGC = !JS_ON_TRACE(cx) && !JS_THREAD_DATA(cx)->waiveGCQuota;
    bool runGC = canGC && JS_UNLIKELY(NeedLastDitchGC(cx));
    for (;;) {
        if (runGC) {
            if (!RunLastDitchGC(cx))
                break;

            /*
             * The JSGC_END callback can legitimately allocate new GC
             * things and populate the free list. If that happens, just
             * return that list head.
             */
            if (Cell *thing = compartment->freeLists.getNext(thingKind))
                return thing;
        }
        ArenaHeader *aheader = compartment->arenas[thingKind].getArenaWithFreeList<T>(cx, thingKind);
        if (aheader) {
            JS_ASSERT(sizeof(T) == aheader->getThingSize());
            return compartment->freeLists.populate(aheader, thingKind);
        }

        /*
         * We failed to allocate any arena. Run the GC if we can unless we
         * have done it already.
         */
        if (!canGC || runGC)
            break;
        runGC = true;
    }

    METER(cx->runtime->gcStats.fail++);
    js_ReportOutOfMemory(cx);
    return NULL;
}

Cell *
RefillFinalizableFreeList(JSContext *cx, unsigned thingKind)
{
    switch (thingKind) {
      case FINALIZE_OBJECT0:
      case FINALIZE_OBJECT0_BACKGROUND:
        return RefillTypedFreeList<JSObject>(cx, thingKind);
      case FINALIZE_OBJECT2:
      case FINALIZE_OBJECT2_BACKGROUND:
        return RefillTypedFreeList<JSObject_Slots2>(cx, thingKind);
      case FINALIZE_OBJECT4:
      case FINALIZE_OBJECT4_BACKGROUND:
        return RefillTypedFreeList<JSObject_Slots4>(cx, thingKind);
      case FINALIZE_OBJECT8:
      case FINALIZE_OBJECT8_BACKGROUND:
        return RefillTypedFreeList<JSObject_Slots8>(cx, thingKind);
      case FINALIZE_OBJECT12:
      case FINALIZE_OBJECT12_BACKGROUND:
        return RefillTypedFreeList<JSObject_Slots12>(cx, thingKind);
      case FINALIZE_OBJECT16:
      case FINALIZE_OBJECT16_BACKGROUND:
        return RefillTypedFreeList<JSObject_Slots16>(cx, thingKind);
      case FINALIZE_STRING:
        return RefillTypedFreeList<JSString>(cx, thingKind);
      case FINALIZE_EXTERNAL_STRING:
        return RefillTypedFreeList<JSExternalString>(cx, thingKind);
      case FINALIZE_SHORT_STRING:
        return RefillTypedFreeList<JSShortString>(cx, thingKind);
      case FINALIZE_FUNCTION:
        return RefillTypedFreeList<JSFunction>(cx, thingKind);
      case FINALIZE_SHAPE:
        return RefillTypedFreeList<Shape>(cx, thingKind);
#if JS_HAS_XML_SUPPORT
      case FINALIZE_XML:
        return RefillTypedFreeList<JSXML>(cx, thingKind);
#endif
      default:
        JS_NOT_REACHED("bad finalize kind");
        return NULL;
    }
}

} /* namespace gc */
} /* namespace js */

uint32
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
    if (GCLocks::Ptr p = rt->gcLocksHash.lookupWithDefault(thing, 0))
        p->value++;
    else
        return false;

    METER(rt->gcStats.lock++);
    return true;
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

        METER(rt->gcStats.unlock++);
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
 * arena. The field marlingdelay->link links all arenas with delayed things
 * into a stack list with the pointer to stack top in
 * GCMarker::unmarkedArenaStackTop. delayMarkingChildren adds
 * arenas to the stack as necessary while markDelayedChildren pops the arenas
 * from the stack until it empties.
 */

GCMarker::GCMarker(JSContext *cx)
  : color(0),
    unmarkedArenaStackTop(MarkingDelay::stackBottom()),
    objStack(cx->runtime->gcMarkStackObjs, sizeof(cx->runtime->gcMarkStackObjs)),
    xmlStack(cx->runtime->gcMarkStackXMLs, sizeof(cx->runtime->gcMarkStackXMLs)),
    largeStack(cx->runtime->gcMarkStackLarges, sizeof(cx->runtime->gcMarkStackLarges))
{
    JS_TRACER_INIT(this, cx, NULL);
#ifdef DEBUG
    markLaterArenas = 0;
#endif
#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    conservativeDumpFileName = getenv("JS_DUMP_CONSERVATIVE_GC_ROOTS");
    memset(&conservativeStats, 0, sizeof(conservativeStats));
#endif
}

GCMarker::~GCMarker()
{
#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    dumpConservativeRoots();
#endif
#ifdef JS_GCMETER
    /* Update total stats. */
    context->runtime->gcStats.conservative.add(conservativeStats);
#endif
}

void
GCMarker::delayMarkingChildren(const void *thing)
{
    const Cell *cell = reinterpret_cast<const Cell *>(thing);
    ArenaHeader *aheader = cell->arenaHeader();
    if (aheader->getMarkingDelay()->link) {
        /* Arena already scheduled to be marked later */
        return;
    }
    aheader->getMarkingDelay()->link = unmarkedArenaStackTop;
    unmarkedArenaStackTop = aheader;
    METER(markLaterArenas++);
    METER_UPDATE_MAX(cell->compartment()->rt->gcStats.maxunmarked, markLaterArenas);
}

template<typename T>
static void
MarkDelayedChilderen(JSTracer *trc, ArenaHeader *aheader)
{
    Arena<T> *a = aheader->getArena<T>();
    T *end = &a->t.things[Arena<T>::ThingsPerArena];
    for (T* thing = &a->t.things[0]; thing != end; ++thing) {
        if (thing->isMarked())
            js::gc::MarkChildren(trc, thing);
    }
}

void
GCMarker::markDelayedChildren()
{
    while (unmarkedArenaStackTop != MarkingDelay::stackBottom()) {
        /*
         * If marking gets delayed at the same arena again, we must repeat
         * marking of its things. For that we pop arena from the stack and
         * clear its nextDelayedMarking before we begin the marking.
         */
        ArenaHeader *aheader = unmarkedArenaStackTop;
        unmarkedArenaStackTop = aheader->getMarkingDelay()->link;
        JS_ASSERT(unmarkedArenaStackTop);
        aheader->getMarkingDelay()->link = NULL;
#ifdef DEBUG
        JS_ASSERT(markLaterArenas);
        markLaterArenas--;
#endif

        switch (aheader->getThingKind()) {
          case FINALIZE_OBJECT0:
          case FINALIZE_OBJECT0_BACKGROUND:
            MarkDelayedChilderen<JSObject>(this, aheader);
            break;
          case FINALIZE_OBJECT2:
          case FINALIZE_OBJECT2_BACKGROUND:
            MarkDelayedChilderen<JSObject_Slots2>(this, aheader);
            break;
          case FINALIZE_OBJECT4:
          case FINALIZE_OBJECT4_BACKGROUND:
            MarkDelayedChilderen<JSObject_Slots4>(this, aheader);
            break;
          case FINALIZE_OBJECT8:
          case FINALIZE_OBJECT8_BACKGROUND:
            MarkDelayedChilderen<JSObject_Slots8>(this, aheader);
            break;
          case FINALIZE_OBJECT12:
          case FINALIZE_OBJECT12_BACKGROUND:
            MarkDelayedChilderen<JSObject_Slots12>(this, aheader);
            break;
          case FINALIZE_OBJECT16:
          case FINALIZE_OBJECT16_BACKGROUND:
            MarkDelayedChilderen<JSObject_Slots16>(this, aheader);
            break;
          case FINALIZE_STRING:
            MarkDelayedChilderen<JSString>(this, aheader);
            break;
          case FINALIZE_EXTERNAL_STRING:
            MarkDelayedChilderen<JSExternalString>(this, aheader);
            break;
          case FINALIZE_SHORT_STRING:
            JS_NOT_REACHED("no delayed marking");
            break;
          case FINALIZE_FUNCTION:
            MarkDelayedChilderen<JSFunction>(this, aheader);
            break;
          case FINALIZE_SHAPE:
            MarkDelayedChilderen<Shape>(this, aheader);
            break;
#if JS_HAS_XML_SUPPORT
          case FINALIZE_XML:
            MarkDelayedChilderen<JSXML>(this, aheader);
            break;
#endif
          default:
            JS_NOT_REACHED("wrong thingkind");
        }
    }
    JS_ASSERT(!markLaterArenas);
}

} /* namespace js */

#ifdef DEBUG
static void
EmptyMarkCallback(JSTracer *trc, void *thing, uint32 kind)
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

    if (ptr) {
        if (!JSAtom::isStatic(ptr)) {
            /* Use conservative machinery to find if ptr is a valid GC thing. */
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
    }
#endif
    JS_SET_TRACING_NAME(trc, entry.value.name ? entry.value.name : "root");
    if (entry.value.type == JS_GC_ROOT_GCTHING_PTR)
        MarkGCThing(trc, *reinterpret_cast<void **>(entry.key));
    else
        MarkValueRaw(trc, *reinterpret_cast<Value *>(entry.key));
}

static void
gc_lock_traversal(const GCLocks::Entry &entry, JSTracer *trc)
{
    JS_ASSERT(entry.value >= 1);
    MarkGCThing(trc, entry.key, "locked object");
}

void
js_TraceStackFrame(JSTracer *trc, StackFrame *fp)
{
    MarkObject(trc, fp->scopeChain(), "scope chain");
    if (fp->isDummyFrame())
        return;
    if (fp->hasArgsObj())
        MarkObject(trc, fp->argsObj(), "arguments");
    js_TraceScript(trc, fp->script());
    fp->script()->compartment->active = true;
    MarkValue(trc, fp->returnValue(), "rval");
}

void
AutoIdArray::trace(JSTracer *trc)
{
    JS_ASSERT(tag == IDARRAY);
    gc::MarkIdRange(trc, idArray->length, idArray->vector, "JSAutoIdArray.idArray");
}

void
AutoEnumStateRooter::trace(JSTracer *trc)
{
    gc::MarkObject(trc, *obj, "js::AutoEnumStateRooter.obj");
}

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag) {
      case JSVAL:
        MarkValue(trc, static_cast<AutoValueRooter *>(this)->val, "js::AutoValueRooter.val");
        return;

      case SHAPE:
        MarkShape(trc, static_cast<AutoShapeRooter *>(this)->shape, "js::AutoShapeRooter.val");
        return;

      case PARSER:
        static_cast<Parser *>(this)->trace(trc);
        return;

      case SCRIPT:
        if (JSScript *script = static_cast<AutoScriptRooter *>(this)->script)
            js_TraceScript(trc, script);
        return;

      case ENUMERATOR:
        static_cast<AutoEnumStateRooter *>(this)->trace(trc);
        return;

      case IDARRAY: {
        JSIdArray *ida = static_cast<AutoIdArray *>(this)->idArray;
        MarkIdRange(trc, ida->length, ida->vector, "js::AutoIdArray.idArray");
        return;
      }

      case DESCRIPTORS: {
        PropDescArray &descriptors =
            static_cast<AutoPropDescArrayRooter *>(this)->descriptors;
        for (size_t i = 0, len = descriptors.length(); i < len; i++) {
            PropDesc &desc = descriptors[i];
            MarkValue(trc, desc.pd, "PropDesc::pd");
            MarkValue(trc, desc.value, "PropDesc::value");
            MarkValue(trc, desc.get, "PropDesc::get");
            MarkValue(trc, desc.set, "PropDesc::set");
        }
        return;
      }

      case DESCRIPTOR : {
        PropertyDescriptor &desc = *static_cast<AutoPropertyDescriptorRooter *>(this);
        if (desc.obj)
            MarkObject(trc, *desc.obj, "Descriptor::obj");
        MarkValue(trc, desc.value, "Descriptor::value");
        if ((desc.attrs & JSPROP_GETTER) && desc.getter)
            MarkObject(trc, *CastAsObject(desc.getter), "Descriptor::get");
        if (desc.attrs & JSPROP_SETTER && desc.setter)
            MarkObject(trc, *CastAsObject(desc.setter), "Descriptor::set");
        return;
      }

      case NAMESPACES: {
        JSXMLArray &array = static_cast<AutoNamespaceArray *>(this)->array;
        MarkObjectRange(trc, array.length, reinterpret_cast<JSObject **>(array.vector),
                        "JSXMLArray.vector");
        array.cursors->trace(trc);
        return;
      }

      case XML:
        js_TraceXML(trc, static_cast<AutoXMLRooter *>(this)->xml);
        return;

      case OBJECT:
        if (JSObject *obj = static_cast<AutoObjectRooter *>(this)->obj)
            MarkObject(trc, *obj, "js::AutoObjectRooter.obj");
        return;

      case ID:
        MarkId(trc, static_cast<AutoIdRooter *>(this)->id_, "js::AutoIdRooter.val");
        return;

      case VALVECTOR: {
        AutoValueVector::VectorImpl &vector = static_cast<AutoValueVector *>(this)->vector;
        MarkValueRange(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }

      case STRING:
        if (JSString *str = static_cast<AutoStringRooter *>(this)->str)
            MarkString(trc, str, "js::AutoStringRooter.str");
        return;

      case IDVECTOR: {
        AutoIdVector::VectorImpl &vector = static_cast<AutoIdVector *>(this)->vector;
        MarkIdRange(trc, vector.length(), vector.begin(), "js::AutoIdVector.vector");
        return;
      }

      case SHAPEVECTOR: {
        AutoShapeVector::VectorImpl &vector = static_cast<js::AutoShapeVector *>(this)->vector;
        MarkShapeRange(trc, vector.length(), vector.begin(), "js::AutoShapeVector.vector");
        return;
      }

      case BINDINGS: {
        static_cast<js::AutoBindingsRooter *>(this)->bindings.trace(trc);
        return;
      }
    }

    JS_ASSERT(tag >= 0);
    MarkValueRange(trc, tag, static_cast<AutoArrayRooter *>(this)->array, "js::AutoArrayRooter.array");
}

namespace js {

JS_FRIEND_API(void)
MarkContext(JSTracer *trc, JSContext *acx)
{
    /* Stack frames and slots are traced by StackSpace::mark. */

    /* Mark other roots-by-definition in acx. */
    if (acx->globalObject && !acx->hasRunOption(JSOPTION_UNROOTED_GLOBAL))
        MarkObject(trc, *acx->globalObject, "global object");
    if (acx->isExceptionPending())
        MarkValue(trc, acx->getPendingException(), "exception");

    for (js::AutoGCRooter *gcr = acx->autoGCRooters; gcr; gcr = gcr->down)
        gcr->trace(trc);

    if (acx->sharpObjectMap.depth > 0)
        js_TraceSharpMap(trc, &acx->sharpObjectMap);

    MarkValue(trc, acx->iterValue, "iterValue");
}

JS_REQUIRES_STACK void
MarkRuntime(JSTracer *trc)
{
    JSRuntime *rt = trc->context->runtime;

    if (rt->state != JSRTS_LANDING)
        MarkConservativeStackRoots(trc);

    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront())
        gc_root_traversal(trc, r.front());

    for (GCLocks::Range r = rt->gcLocksHash.all(); !r.empty(); r.popFront())
        gc_lock_traversal(r.front(), trc);

    js_TraceAtomState(trc);
    js_MarkTraps(trc);

    JSContext *iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
        MarkContext(trc, acx);

#ifdef JS_TRACER
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
        (*c)->traceMonitor.mark(trc);
#endif

    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->mark(trc);

    /*
     * We mark extra roots at the last thing so it can use use additional
     * colors to implement cycle collection.
     */
    if (rt->gcExtraRootsTraceOp)
        rt->gcExtraRootsTraceOp(trc, rt->gcExtraRootsData);

#ifdef DEBUG
    if (rt->functionMeterFilename) {
        for (int k = 0; k < 2; k++) {
            typedef JSRuntime::FunctionCountMap HM;
            HM &h = (k == 0) ? rt->methodReadBarrierCountMap : rt->unjoinedFunctionCountMap;
            for (HM::Range r = h.all(); !r.empty(); r.popFront()) {
                JSFunction *fun = r.front().key;
                JS_CALL_OBJECT_TRACER(trc, fun, "FunctionCountMap key");
            }
        }
    }
#endif
}

void
TriggerGC(JSRuntime *rt)
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
    TriggerAllOperationCallbacks(rt);
}

void
TriggerCompartmentGC(JSCompartment *comp)
{
    JSRuntime *rt = comp->rt;
    JS_ASSERT(!rt->gcRunning);

#ifdef JS_GC_ZEAL
    if (rt->gcZeal >= 1) {
        TriggerGC(rt);
        return;
    }
#endif

    if (rt->gcMode != JSGC_MODE_COMPARTMENT || comp == rt->atomsCompartment) {
        /* We can't do a compartmental GC of the default compartment. */
        TriggerGC(rt);
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
        TriggerGC(rt);
        return;
    }

    /*
     * Trigger the GC when it is safe to call an operation callback on any
     * thread.
     */
    rt->gcIsNeeded = true;
    rt->gcTriggerCompartment = comp;
    TriggerAllOperationCallbacks(comp->rt);
}

void
MaybeGC(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

#ifdef JS_GC_ZEAL
    if (rt->gcZeal > 0) {
        js_GC(cx, NULL, GC_NORMAL);
        return;
    }
#endif

    JSCompartment *comp = cx->compartment;
    if (rt->gcIsNeeded) {
        js_GC(cx, (comp == rt->gcTriggerCompartment) ? comp : NULL, GC_NORMAL);
        return;
    }

    if (comp->gcBytes > 8192 && comp->gcBytes >= 3 * (comp->gcTriggerBytes / 4))
        js_GC(cx, (rt->gcMode == JSGC_MODE_COMPARTMENT) ? comp : NULL, GC_NORMAL);
}

} /* namespace js */

void
js_DestroyScriptsToGC(JSContext *cx, JSCompartment *comp)
{
    JSScript **listp, *script;

    for (size_t i = 0; i != JS_ARRAY_LENGTH(comp->scriptsToGC); ++i) {
        listp = &comp->scriptsToGC[i];
        while ((script = *listp) != NULL) {
            *listp = script->u.nextToGC;
            script->u.nextToGC = NULL;
            js_DestroyCachedScript(cx, script);
        }
    }
}

void
JSCompartment::finalizeObjectArenaLists(JSContext *cx)
{
    arenas[FINALIZE_OBJECT0]. finalizeNow<JSObject>(cx);
    arenas[FINALIZE_OBJECT2]. finalizeNow<JSObject_Slots2>(cx);
    arenas[FINALIZE_OBJECT4]. finalizeNow<JSObject_Slots4>(cx);
    arenas[FINALIZE_OBJECT8]. finalizeNow<JSObject_Slots8>(cx);
    arenas[FINALIZE_OBJECT12].finalizeNow<JSObject_Slots12>(cx);
    arenas[FINALIZE_OBJECT16].finalizeNow<JSObject_Slots16>(cx);
    arenas[FINALIZE_FUNCTION].finalizeNow<JSFunction>(cx);

#ifdef JS_THREADSAFE
    arenas[FINALIZE_OBJECT0_BACKGROUND]. finalizeLater<JSObject>(cx);
    arenas[FINALIZE_OBJECT2_BACKGROUND]. finalizeLater<JSObject_Slots2>(cx);
    arenas[FINALIZE_OBJECT4_BACKGROUND]. finalizeLater<JSObject_Slots4>(cx);
    arenas[FINALIZE_OBJECT8_BACKGROUND]. finalizeLater<JSObject_Slots8>(cx);
    arenas[FINALIZE_OBJECT12_BACKGROUND].finalizeLater<JSObject_Slots12>(cx);
    arenas[FINALIZE_OBJECT16_BACKGROUND].finalizeLater<JSObject_Slots16>(cx);
#endif

#if JS_HAS_XML_SUPPORT
    arenas[FINALIZE_XML].finalizeNow<JSXML>(cx);
#endif
}

void
JSCompartment::finalizeStringArenaLists(JSContext *cx)
{
#ifdef JS_THREADSAFE
    arenas[FINALIZE_SHORT_STRING].finalizeLater<JSShortString>(cx);
    arenas[FINALIZE_STRING].finalizeLater<JSString>(cx);
#else
    arenas[FINALIZE_SHORT_STRING].finalizeNow<JSShortString>(cx);
    arenas[FINALIZE_STRING].finalizeNow<JSString>(cx);
#endif
    arenas[FINALIZE_EXTERNAL_STRING].finalizeNow<JSExternalString>(cx);
}

void
JSCompartment::finalizeShapeArenaLists(JSContext *cx)
{
    arenas[FINALIZE_SHAPE].finalizeNow<Shape>(cx);
}

#ifdef JS_THREADSAFE

namespace js {

bool
GCHelperThread::init(JSRuntime *rt)
{
    if (!(wakeup = PR_NewCondVar(rt->gcLock)))
        return false;
    if (!(sweepingDone = PR_NewCondVar(rt->gcLock)))
        return false;

    thread = PR_CreateThread(PR_USER_THREAD, threadMain, rt, PR_PRIORITY_NORMAL,
                             PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    return !!thread;

}

void
GCHelperThread::finish(JSRuntime *rt)
{
    PRThread *join = NULL;
    {
        AutoLockGC lock(rt);
        if (thread && !shutdown) {
            shutdown = true;
            PR_NotifyCondVar(wakeup);
            join = thread;
        }
    }
    if (join) {
        /* PR_DestroyThread is not necessary. */
        PR_JoinThread(join);
    }
    if (wakeup)
        PR_DestroyCondVar(wakeup);
    if (sweepingDone)
        PR_DestroyCondVar(sweepingDone);
}

/* static */
void
GCHelperThread::threadMain(void *arg)
{
    JSRuntime *rt = static_cast<JSRuntime *>(arg);
    rt->gcHelperThread.threadLoop(rt);
}

void
GCHelperThread::threadLoop(JSRuntime *rt)
{
    AutoLockGC lock(rt);
    while (!shutdown) {
        /*
         * Sweeping can be true here on the first iteration if a GC and the
         * corresponding startBackgroundSweep call happen before this thread
         * has a chance to run.
         */
        if (!sweeping)
            PR_WaitCondVar(wakeup, PR_INTERVAL_NO_TIMEOUT);
        if (sweeping) {
            AutoUnlockGC unlock(rt);
            doSweep();
        }
        sweeping = false;
        PR_NotifyAllCondVar(sweepingDone);
    }
}

void
GCHelperThread::startBackgroundSweep(JSRuntime *rt)
{
    /* The caller takes the GC lock. */
    JS_ASSERT(!sweeping);
    sweeping = true;
    PR_NotifyCondVar(wakeup);
}

void
GCHelperThread::waitBackgroundSweepEnd(JSRuntime *rt, bool gcUnlocked)
{
    Maybe<AutoLockGC> lock;
    if (gcUnlocked)
        lock.construct(rt);
    while (sweeping)
        PR_WaitCondVar(sweepingDone, PR_INTERVAL_NO_TIMEOUT);
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

void
GCHelperThread::doSweep()
{
    JS_ASSERT(cx);
    for (ArenaHeader **i = finalizeVector.begin(); i != finalizeVector.end(); ++i)
        ArenaList::backgroundFinalize(cx, *i);
    finalizeVector.resize(0);
    ExpireGCChunks(cx->runtime);
    cx = NULL;

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

}

#endif /* JS_THREADSAFE */

static void
SweepCrossCompartmentWrappers(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    /*
     * Figure out how much JIT code should be released from inactive compartments.
     * If multiple eighth-lives have passed, compound the release interval linearly;
     * if enough time has passed, all inactive JIT code will be released.
     */
    uint32 releaseInterval = 0;
    int64 now = PRMJ_Now();
    if (now >= rt->gcJitReleaseTime) {
        releaseInterval = 8;
        while (now >= rt->gcJitReleaseTime) {
            if (--releaseInterval == 1)
                rt->gcJitReleaseTime = now;
            rt->gcJitReleaseTime += JIT_SCRIPT_EIGHTH_LIFETIME;
        }
    }

    /*
     * Sweep the compartment:
     * (1) Remove dead wrappers from the compartment map.
     * (2) Finalize any unused empty shapes.
     * (3) Sweep the trace JIT of unused code.
     * (4) Sweep the method JIT ICs and release infrequently used JIT code.
     */
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
        (*c)->sweep(cx, releaseInterval);
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
            (compartment->arenaListsAreEmpty() || gckind == GC_LAST_CONTEXT))
        {
            JS_ASSERT(compartment->freeLists.isEmpty());
            if (callback)
                (void) callback(cx, compartment, JSCOMPARTMENT_DESTROY);
            if (compartment->principals)
                JSPRINCIPALS_DROP(cx, compartment->principals);
            cx->delete_(compartment);
            continue;
        }
        *write++ = compartment;
    }
    rt->compartments.resize(write - rt->compartments.begin());
}

/*
 * Perform mark-and-sweep GC.
 *
 * In a JS_THREADSAFE build, the calling thread must be rt->gcThread and each
 * other thread must be either outside all requests or blocked waiting for GC
 * to finish. Note that the caller does not hold rt->gcLock.
 * If comp is set, we perform a single-compartment GC.
 */
static void
MarkAndSweep(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind GCTIMER_PARAM)
{
    JS_ASSERT_IF(comp, gckind != GC_LAST_CONTEXT);
    JS_ASSERT_IF(comp, comp != comp->rt->atomsCompartment);
    JS_ASSERT_IF(comp, comp->rt->gcMode == JSGC_MODE_COMPARTMENT);

    JSRuntime *rt = cx->runtime;
    rt->gcNumber++;

    /* Clear gcIsNeeded now, when we are about to start a normal GC cycle. */
    rt->gcIsNeeded = false;
    rt->gcTriggerCompartment = NULL;

    /* Reset malloc counter. */
    rt->resetGCMallocBytes();

#ifdef JS_DUMP_SCOPE_METERS
    {
        extern void js_DumpScopeMeters(JSRuntime *rt);
        js_DumpScopeMeters(rt);
    }
#endif

    /*
     * Reset the property cache's type id generator so we can compress ids.
     * Same for the protoHazardShape proxy-shape standing in for all object
     * prototypes having readonly or setter properties.
     */
    if (rt->shapeGen & SHAPE_OVERFLOW_BIT
#ifdef JS_GC_ZEAL
        || rt->gcZeal >= 1
#endif
        ) {
        rt->gcRegenShapes = true;
        rt->shapeGen = 0;
        rt->protoHazardShape = 0;
    }

    if (rt->gcCurrentCompartment) {
        rt->gcCurrentCompartment->purge(cx);
    } else {
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            (*c)->purge(cx);
    }

    js_PurgeThreads(cx);
    {
        JSContext *iter = NULL;
        while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
            acx->purge();
    }

    JS_ASSERT_IF(comp, !rt->gcRegenShapes);

    /*
     * Mark phase.
     */
    TIMESTAMP(startMark);
    GCMarker gcmarker(cx);
    JS_ASSERT(IS_GC_MARKING_TRACER(&gcmarker));
    JS_ASSERT(gcmarker.getMarkColor() == BLACK);
    rt->gcMarkingTracer = &gcmarker;

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
         r.front()->bitmap.clear();

    if (comp) {
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            (*c)->markCrossCompartmentWrappers(&gcmarker);
    } else {
        js_MarkScriptFilenames(rt);
    }

    MarkRuntime(&gcmarker);

    gcmarker.drainMarkStack();

    /*
     * Mark weak roots.
     */
    while (true) {
        if (!js_TraceWatchPoints(&gcmarker) && !WeakMap::markIteratively(&gcmarker))
            break;
        gcmarker.drainMarkStack();
    }

    rt->gcMarkingTracer = NULL;

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_MARK_END);

#ifdef DEBUG
    /* Make sure that we didn't mark an object in another compartment */
    if (comp) {
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            JS_ASSERT_IF(*c != comp && *c != rt->atomsCompartment, checkArenaListAllUnmarked(*c));
    }
#endif

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
    TIMESTAMP(startSweep);

    /* Finalize unreachable (key,value) pairs in all weak maps. */
    WeakMap::sweep(cx);

    js_SweepAtomState(cx);

    /* Finalize watch points associated with unreachable objects. */
    js_SweepWatchPoints(cx);

    /*
     * We finalize objects before other GC things to ensure that object's finalizer
     * can access them even if they will be freed. Sweep the runtime's property trees
     * after finalizing objects, in case any had watchpoints referencing tree nodes.
     * Do this before sweeping compartments, so that we sweep all shapes in
     * unreachable compartments.
     */
    if (comp) {
        comp->sweep(cx, 0);
        comp->finalizeObjectArenaLists(cx);
        TIMESTAMP(sweepObjectEnd);
        comp->finalizeStringArenaLists(cx);
        TIMESTAMP(sweepStringEnd);
        comp->finalizeShapeArenaLists(cx);
        TIMESTAMP(sweepShapeEnd);
    } else {
        SweepCrossCompartmentWrappers(cx);
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++)
            (*c)->finalizeObjectArenaLists(cx);

        TIMESTAMP(sweepObjectEnd);

        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++)
            (*c)->finalizeStringArenaLists(cx);

        TIMESTAMP(sweepStringEnd);

        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++)
            (*c)->finalizeShapeArenaLists(cx);

        TIMESTAMP(sweepShapeEnd);

#ifdef DEBUG
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            (*c)->propertyTree.dumpShapeStats();
#endif
#ifdef JS_GCMETER
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            UpdateAllCompartmentGCStats(*c);
#endif
    }

#ifdef DEBUG
     PropertyTree::dumpShapes(cx);
#endif

    if (!comp) {
        SweepCompartments(cx, gckind);

        /*
         * Sweep script filenames after sweeping functions in the generic loop
         * above. In this way when a scripted function's finalizer destroys the
         * script and calls rt->destroyScriptHook, the hook can still access the
         * script's filename. See bug 323267.
         */
        js_SweepScriptFilenames(rt);
    }

#ifndef JS_THREADSAFE
    /*
     * Destroy arenas after we finished the sweeping so finalizers can safely
     * use IsAboutToBeFinalized().
     * This is done on the GCHelperThread if JS_THREADSAFE is defined.
     */
    ExpireGCChunks(rt);
#endif
    TIMESTAMP(sweepDestroyEnd);

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
#ifdef DEBUG_srcnotesize
  { extern void DumpSrcNoteSizeHist();
    DumpSrcNoteSizeHist();
    printf("GC HEAP SIZE %lu\n", (unsigned long)rt->gcBytes);
  }
#endif
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
#ifdef JS_TRACER
    JS_ASSERT_IF(requestDebit == 0, !JS_ON_TRACE(cx));
#endif
    if (requestDebit != 0) {
#ifdef JS_TRACER
        if (JS_ON_TRACE(cx)) {
            /*
             * Leave trace before we decrease rt->requestCount and notify the
             * GC. Otherwise the GC may start immediately after we unlock while
             * this thread is still on trace.
             */
            AutoUnlockGC unlock(rt);
            LeaveTrace(cx);
        }
#endif
        rt->requestCount -= requestDebit;
        if (rt->requestCount == 0)
            JS_NOTIFY_REQUEST_DONE(rt);
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
GCCycle(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind  GCTIMER_PARAM)
{
    if (JS_ON_TRACE(cx))
        return;

    JSRuntime *rt = cx->runtime;

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
     * We should not be depending on cx->compartment in the GC, so set it to
     * NULL to look for violations.
     */
    SwitchToCompartment sc(cx, (JSCompartment *)NULL);

    JS_ASSERT(!rt->gcCurrentCompartment);
    rt->gcCurrentCompartment = comp;

    rt->gcMarkAndSweep = true;
    {
        AutoUnlockGC unlock(rt);

#ifdef JS_THREADSAFE
        /*
         * As we about to purge caches and clear the mark bits we must wait
         * for any background finalization to finish.
         */
        JS_ASSERT(!cx->gcBackgroundFree);
        rt->gcHelperThread.waitBackgroundSweepEnd(rt);
        if (gckind != GC_LAST_CONTEXT && rt->state != JSRTS_LANDING) {
            cx->gcBackgroundFree = &rt->gcHelperThread;
            cx->gcBackgroundFree->setContext(cx);
        }
#endif
        MarkAndSweep(cx, comp, gckind  GCTIMER_ARG);
    }

#ifdef JS_THREADSAFE
    if (gckind != GC_LAST_CONTEXT && rt->state != JSRTS_LANDING) {
        JS_ASSERT(cx->gcBackgroundFree == &rt->gcHelperThread);
        cx->gcBackgroundFree = NULL;
        rt->gcHelperThread.startBackgroundSweep(rt);
    } else {
        JS_ASSERT(!cx->gcBackgroundFree);
    }
#endif

    rt->gcMarkAndSweep = false;
    rt->gcRegenShapes = false;
    rt->setGCLastBytes(rt->gcBytes);
    rt->gcCurrentCompartment = NULL;

    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
        (*c)->setGCLastBytes((*c)->gcBytes);
}

void
js_GC(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    /*
     * Don't collect garbage if the runtime isn't up, and cx is not the last
     * context in the runtime.  The last context must force a GC, and nothing
     * should suppress that final collection or there may be shutdown leaks,
     * or runtime bloat until the next context is created.
     */
    if (rt->state != JSRTS_UP && gckind != GC_LAST_CONTEXT)
        return;

    RecordNativeStackTopForGC(cx);

#ifdef DEBUG
    int stackDummy;
# if JS_STACK_GROWTH_DIRECTION > 0
    /* cx->stackLimit is set to jsuword(-1) by default. */
    JS_ASSERT_IF(cx->stackLimit != jsuword(-1),
                 JS_CHECK_STACK_SIZE(cx->stackLimit + (1 << 14), &stackDummy));
# else
    /* -16k because it is possible to perform a GC during an overrecursion report. */
    JS_ASSERT_IF(cx->stackLimit, JS_CHECK_STACK_SIZE(cx->stackLimit - (1 << 14), &stackDummy));
# endif
#endif

    GCTIMER_BEGIN();

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
#ifdef JS_THREADSAFE
            rt->gcHelperThread.waitBackgroundSweepEnd(rt);
#endif
            /* Lock out other GC allocator and collector invocations. */
            AutoLockGC lock(rt);
            rt->gcPoke = false;
            GCCycle(cx, comp, gckind  GCTIMER_ARG);
        }

        /* We re-sample the callback again as the finalizers can change it. */
        if (JSGCCallback callback = rt->gcCallback)
            (void) callback(cx, JSGC_END);

        /*
         * On shutdown, iterate until finalizers or the JSGC_END callback
         * stop creating garbage.
         */
    } while (gckind == GC_LAST_CONTEXT && rt->gcPoke);
#ifdef JS_GCMETER
    js_DumpGCStats(cx->runtime, stderr);
#endif
    GCTIMER_END(gckind == GC_LAST_CONTEXT);
}

namespace js {
namespace gc {

bool
SetProtoCheckingForCycles(JSContext *cx, JSObject *obj, JSObject *proto)
{
    /*
     * This function cannot be called during the GC and always requires a
     * request.
     */
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->thread()->data.requestDepth);

    /*
     * This is only necessary if AutoGCSession below would wait for GC to
     * finish on another thread, but to capture the minimal stack space and
     * for code simplicity we do it here unconditionally.
     */
    RecordNativeStackTopForGC(cx);
#endif

    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);
    AutoUnlockGC unlock(rt);

    bool cycle = false;
    for (JSObject *obj2 = proto; obj2;) {
        if (obj2 == obj) {
            cycle = true;
            break;
        }
        obj2 = obj2->getProto();
    }
    if (!cycle)
        obj->setProto(proto);

    return !cycle;
}

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals)
{
    JSRuntime *rt = cx->runtime;
    JSCompartment *compartment = cx->new_<JSCompartment>(rt);
    if (!compartment || !compartment->init()) {
        Foreground::delete_(compartment);
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    if (principals) {
        compartment->principals = principals;
        JSPRINCIPALS_HOLD(cx, principals);
    }

    compartment->setGCLastBytes(8192);

    {
        AutoLockGC lock(rt);

        if (!rt->compartments.append(compartment)) {
            AutoUnlockGC unlock(rt);
            Foreground::delete_(compartment);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }

    JSCompartmentCallback callback = rt->compartmentCallback;
    if (callback && !callback(cx, compartment, JSCOMPARTMENT_NEW)) {
        AutoLockGC lock(rt);
        rt->compartments.popBack();
        Foreground::delete_(compartment);
        return NULL;
    }
    return compartment;
}

} /* namespace gc */

void
TraceRuntime(JSTracer *trc)
{
    LeaveTrace(trc->context);

#ifdef JS_THREADSAFE
    {
        JSContext *cx = trc->context;
        JSRuntime *rt = cx->runtime;
        AutoLockGC lock(rt);

        if (rt->gcThread != cx->thread()) {
            AutoGCSession gcsession(cx);
            AutoUnlockGC unlock(rt);
            RecordNativeStackTopForGC(trc->context);
            MarkRuntime(trc);
            return;
        }
    }
#else
    RecordNativeStackTopForGC(trc->context);
#endif

    /*
     * Calls from inside a normal GC or a recursive calls are OK and do not
     * require session setup.
     */
    MarkRuntime(trc);
}

} /* namespace js */

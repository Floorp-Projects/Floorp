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
#include <stdlib.h>     /* for free */
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
#include "jstracer.h"
#include "methodjit/MethodJIT.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsprobes.h"
#include "jscntxtinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jshashtable.h"

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
JS_STATIC_ASSERT(JSTRACE_XML    == 2);

/*
 * JS_IS_VALID_TRACE_KIND assumes that JSTRACE_STRING is the last non-xml
 * trace kind when JS_HAS_XML_SUPPORT is false.
 */
JS_STATIC_ASSERT(JSTRACE_STRING + 1 == JSTRACE_XML);

/*
 * Check consistency of external string constants from JSFinalizeGCThingKind.
 */
JS_STATIC_ASSERT(FINALIZE_EXTERNAL_STRING_LAST - FINALIZE_EXTERNAL_STRING0 ==
                 JS_EXTERNAL_STRING_LIMIT - 1);

/*
 * Everything we store in the heap must be a multiple of the cell size.
 */
JS_STATIC_ASSERT(sizeof(JSString)       % sizeof(FreeCell) == 0);
JS_STATIC_ASSERT(sizeof(JSShortString)  % sizeof(FreeCell) == 0);
JS_STATIC_ASSERT(sizeof(JSObject)       % sizeof(FreeCell) == 0);
JS_STATIC_ASSERT(sizeof(JSFunction)     % sizeof(FreeCell) == 0);
#ifdef JSXML
JS_STATIC_ASSERT(sizeof(JSXML)          % sizeof(FreeCell) == 0);
#endif

/*
 * All arenas must be exactly 4k.
 */
JS_STATIC_ASSERT(sizeof(Arena<JSString>)        == 4096);
JS_STATIC_ASSERT(sizeof(Arena<JSShortString>)   == 4096);
JS_STATIC_ASSERT(sizeof(Arena<JSObject>)        == 4096);
JS_STATIC_ASSERT(sizeof(Arena<JSFunction>)      == 4096);
JS_STATIC_ASSERT(sizeof(Arena<JSXML>)           == 4096);

#ifdef JS_GCMETER
# define METER(x)               ((void) (x))
# define METER_IF(condition, x) ((void) ((condition) && (x)))
#else
# define METER(x)               ((void) 0)
# define METER_IF(condition, x) ((void) 0)
#endif

# define METER_UPDATE_MAX(maxLval, rval)                                       \
    METER_IF((maxLval) < (rval), (maxLval) = (rval))

namespace js{
namespace gc{

/* This array should be const, but that doesn't link right under GCC. */
FinalizeKind slotsToThingKind[] = {
    /* 0 */  FINALIZE_OBJECT0,  FINALIZE_OBJECT2,  FINALIZE_OBJECT2,  FINALIZE_OBJECT4,
    /* 4 */  FINALIZE_OBJECT4,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,
    /* 8 */  FINALIZE_OBJECT8,  FINALIZE_OBJECT12, FINALIZE_OBJECT12, FINALIZE_OBJECT12,
    /* 12 */ FINALIZE_OBJECT12, FINALIZE_OBJECT16, FINALIZE_OBJECT16, FINALIZE_OBJECT16,
    /* 16 */ FINALIZE_OBJECT16
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(slotsToThingKind) == SLOTS_TO_THING_KIND_LIMIT);

/* Initialize the arena and setup the free list. */
template <typename T>
void
Arena<T>::init(JSCompartment *compartment, unsigned thingKind)
{
    aheader.compartment = compartment;
    aheader.thingKind = thingKind;
    aheader.freeList = &t.things[0].cell;
    aheader.thingSize = sizeof(T);
    aheader.isUsed = true;
    JS_ASSERT(sizeof(T) == sizeof(ThingOrCell<T>));
    ThingOrCell<T> *thing = &t.things[0];
    ThingOrCell<T> *last = &t.things[JS_ARRAY_LENGTH(t.things) - 1];
    while (thing < last) {
        thing->cell.link = &(thing + 1)->cell;
        ++thing;
    }
    last->cell.link = NULL;
#ifdef DEBUG
    aheader.hasFreeThings = true;
#endif
}

template <typename T>
bool
Arena<T>::inFreeList(void *thing) const
{
    FreeCell *cursor = aheader.freeList;
    while (cursor) {
        JS_ASSERT(aheader.thingSize == sizeof(T));
        JS_ASSERT(!cursor->isMarked());

        /* If the cursor moves past the thing, it's not in the freelist. */
        if (thing < cursor)
            break;

        /* If we find it on the freelist, it's dead. */
        if (thing == cursor)
            return true;
        JS_ASSERT_IF(cursor->link, cursor < cursor->link);
        cursor = cursor->link;
    }
    return false;
}

template<typename T>
inline ConservativeGCTest
Arena<T>::mark(T *thing, JSTracer *trc)
{
    JS_ASSERT(sizeof(T) == aheader.thingSize);

    thing = getAlignedThing(thing);

    if (thing > &t.things[ThingsPerArena-1].t || thing < &t.things[0].t)
        return CGCT_NOTARENA;

    if (!aheader.isUsed || inFreeList(thing))
        return CGCT_NOTLIVE;

    JS_ASSERT(assureThingIsAligned(thing));

    JS_SET_TRACING_NAME(trc, "machine stack");
    Mark(trc, thing);

    return CGCT_VALID;
}

#ifdef DEBUG
bool
checkArenaListsForThing(JSCompartment *comp, void *thing) {
    if (comp->arenas[FINALIZE_OBJECT0].arenasContainThing<JSObject>(thing) ||
        comp->arenas[FINALIZE_OBJECT2].arenasContainThing<JSObject_Slots2>(thing) ||
        comp->arenas[FINALIZE_OBJECT4].arenasContainThing<JSObject_Slots4>(thing) ||
        comp->arenas[FINALIZE_OBJECT8].arenasContainThing<JSObject_Slots8>(thing) ||
        comp->arenas[FINALIZE_OBJECT12].arenasContainThing<JSObject_Slots12>(thing) ||
        comp->arenas[FINALIZE_OBJECT16].arenasContainThing<JSObject_Slots16>(thing) ||
        comp->arenas[FINALIZE_FUNCTION].arenasContainThing<JSFunction>(thing) ||
#if JS_HAS_XML_SUPPORT
        comp->arenas[FINALIZE_XML].arenasContainThing<JSXML>(thing) ||
#endif
        comp->arenas[FINALIZE_SHORT_STRING].arenasContainThing<JSShortString>(thing)) {
            return true;
    }
    for (unsigned i = FINALIZE_STRING; i <= FINALIZE_EXTERNAL_STRING_LAST; i++) {
        if (comp->arenas[i].arenasContainThing<JSString>(thing))
            return true;
    }
    return false;
}
#endif

} /* namespace gc */
} /* namespace js */

void
JSCompartment::finishArenaLists()
{
    for (int i = 0; i < FINALIZE_LIMIT; i++)
        arenas[i].releaseAll();
}

void
Chunk::clearMarkBitmap()
{
    PodZero(&bitmaps[0], ArenasPerChunk);
}

void
Chunk::init(JSRuntime *rt)
{
    info.runtime = rt;
    info.age = 0;
    info.emptyArenaLists.init();
    info.emptyArenaLists.cellFreeList = &arenas[0];
    Arena<FreeCell> *arena = &arenas[0];
    Arena<FreeCell> *last = &arenas[JS_ARRAY_LENGTH(arenas) - 1];
    while (arena < last) {
        arena->header()->next = arena + 1;
        arena->header()->isUsed = false;
        ++arena;
    }
    last->header()->next = NULL;
    last->header()->isUsed = false;
    info.numFree = ArenasPerChunk;
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
Arena<T> *
Chunk::allocateArena(JSCompartment *comp, unsigned thingKind)
{
    JSRuntime *rt = info.runtime;
    JS_ASSERT(hasAvailableArenas());
    Arena<T> *arena = info.emptyArenaLists.getNext<T>(comp, thingKind);
    JS_ASSERT(arena);
    JS_ASSERT(arena->header()->isUsed);
    --info.numFree;
    rt->gcBytes += sizeof(Arena<T>);
    METER(rt->gcStats.nallarenas++);
    return arena;
}

template <typename T>
void
Chunk::releaseArena(Arena<T> *arena)
{
    JSRuntime *rt = info.runtime;
    METER(rt->gcStats.afree++);
    JS_ASSERT(rt->gcStats.nallarenas != 0);
    METER(rt->gcStats.nallarenas--);
    JS_ASSERT(rt->gcBytes >= sizeof(Arena<T>));

    rt->gcBytes -= sizeof(Arena<T>);
    info.emptyArenaLists.insert((Arena<Cell> *)arena);
    arena->header()->isUsed = false;
    ++info.numFree;
    if (unused())
        info.age = 0;
}

bool
Chunk::expire()
{
    if (!unused())
        return false;
    return info.age++ > MaxAge;
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
    rt->gcChunkAllocator->free(p);
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
    rt->gcChunkAllocator->free(p);
}

static Chunk *
PickChunk(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    Chunk *chunk;
    if (!JS_THREAD_DATA(cx)->waiveGCQuota && 
        (rt->gcBytes >= rt->gcMaxBytes ||
        rt->gcBytes > GC_HEAP_GROWTH_FACTOR * rt->gcNewArenaTriggerBytes)) {
        /*
         * FIXME bug 524051 We cannot run a last-ditch GC on trace for now, so
         * just pretend we are out of memory which will throw us off trace and
         * we will re-try this code path from the interpreter.
         */
        if (!JS_ON_TRACE(cx))
            return NULL;
        TriggerGC(cx->runtime);
    }

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront()) {
        if (r.front()->hasAvailableArenas())
            return r.front();
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

    return chunk;
}

static void
ExpireGCChunks(JSRuntime *rt)
{
    /* Remove unused chunks. */
    AutoLockGC lock(rt);

    for (GCChunkSet::Enum e(rt->gcChunkSet); !e.empty(); e.popFront()) {
        Chunk *chunk = e.front();
        JS_ASSERT(chunk->info.runtime == rt);
        if (chunk->expire()) {
            e.removeFront();
            ReleaseGCChunk(rt, chunk);
            continue;
        }
    }
}

template <typename T>
static Arena<T> *
AllocateArena(JSContext *cx, unsigned thingKind)
{
    JSRuntime *rt = cx->runtime;
    Chunk *chunk;
    Arena<T> *arena;
    {
        AutoLockGC lock(rt);
        if (cx->compartment->chunk && cx->compartment->chunk->hasAvailableArenas()) {
            chunk = cx->compartment->chunk;
        } else {
            if (!(chunk = PickChunk(cx))) {
                return NULL;
            } else {
                cx->compartment->chunk = chunk;
            }
        }
        arena = chunk->allocateArena<T>(cx->compartment, thingKind);
    }
    return arena;
}

JS_FRIEND_API(bool)
IsAboutToBeFinalized(void *thing)
{
    if (JSString::isStatic(thing))
        return false;

    return !reinterpret_cast<Cell *>(thing)->isMarked();
}

JS_FRIEND_API(bool)
js_GCThingIsMarked(void *thing, uint32 color = BLACK)
{
    JS_ASSERT(thing);
    AssertValidColor(thing, color);
    return reinterpret_cast<Cell *>(thing)->isMarked(color);
}

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
     * By default the trigger factor gets maximum possible value. This
     * means that GC will not be triggered by growth of GC memory (gcBytes).
     */
    rt->setGCTriggerFactor((uint32) -1);

    /*
     * The assigned value prevents GC from running when GC memory is too low
     * (during JS engine start).
     */
    rt->setGCLastBytes(8192);
    rt->gcNewArenaTriggerBytes = GC_ARENA_ALLOCATION_TRIGGER;

    METER(PodZero(&rt->gcStats));
    return true;
}

namespace js {

template <typename T>
static inline ConservativeGCTest
MarkCell(Cell *cell, JSTracer *trc)
{
    return GetArena<T>(cell)->mark((T *)cell, trc);
}

/*
 * Returns CGCT_VALID and mark it if the w can be a live GC thing and sets traceKind
 * accordingly. Otherwise returns the reason for rejection.
 */
inline ConservativeGCTest
MarkIfGCThingWord(JSTracer *trc, jsuword w, uint32 &traceKind)
{
    JSRuntime *rt = trc->context->runtime;
    /*
     * The conservative scanner may access words that valgrind considers as
     * undefined. To avoid false positives and not to alter valgrind view of
     * the memory we make as memcheck-defined the argument, a copy of the
     * original word. See bug 572678.
     */
#ifdef JS_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(&w, sizeof(w));
#endif

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
    jsuword payload = w & JSID_PAYLOAD_MASK;
#elif JS_BITS_PER_WORD == 64
    jsuword payload = w & JSID_PAYLOAD_MASK & JSVAL_PAYLOAD_MASK;
#endif

    Cell *cell = reinterpret_cast<Cell *>(payload);
    Chunk *chunk = cell->chunk();

    if (!rt->gcChunkSet.has(chunk))
        return CGCT_NOTCHUNK;

    if (!chunk->withinArenasRange(cell))
        return CGCT_NOTARENA;

    ArenaHeader *aheader = cell->arena()->header();

    if (!aheader->isUsed)
        return CGCT_FREEARENA;

    ConservativeGCTest test;
    traceKind = aheader->thingKind;

    switch (traceKind) {
        case FINALIZE_OBJECT0:
            test = MarkCell<JSObject>(cell, trc);
            break;
        case FINALIZE_OBJECT2:
            test = MarkCell<JSObject_Slots2>(cell, trc);
            break;
        case FINALIZE_OBJECT4:
            test = MarkCell<JSObject_Slots4>(cell, trc);
            break;
        case FINALIZE_OBJECT8:
            test = MarkCell<JSObject_Slots8>(cell, trc);
            break;
        case FINALIZE_OBJECT12:
            test = MarkCell<JSObject_Slots12>(cell, trc);
            break;
        case FINALIZE_OBJECT16:
            test = MarkCell<JSObject_Slots16>(cell, trc);
            break;
        case FINALIZE_STRING:
        case FINALIZE_EXTERNAL_STRING0:
        case FINALIZE_EXTERNAL_STRING1:
        case FINALIZE_EXTERNAL_STRING2:
        case FINALIZE_EXTERNAL_STRING3:
        case FINALIZE_EXTERNAL_STRING4:
        case FINALIZE_EXTERNAL_STRING5:
        case FINALIZE_EXTERNAL_STRING6:
        case FINALIZE_EXTERNAL_STRING7:
            test = MarkCell<JSString>(cell, trc);
            break;
        case FINALIZE_SHORT_STRING:
            test = MarkCell<JSShortString>(cell, trc);
            break;
        case FINALIZE_FUNCTION:
            test = MarkCell<JSFunction>(cell, trc);
            break;
#if JS_HAS_XML_SUPPORT
        case FINALIZE_XML:
            test = MarkCell<JSXML>(cell, trc);
            break;
#endif
        default:
            test = CGCT_WRONGTAG;
            JS_NOT_REACHED("wrong tag");
    }

    return test;
}

inline ConservativeGCTest
MarkIfGCThingWord(JSTracer *trc, jsuword w)
{
    uint32 traceKind;
    return MarkIfGCThingWord(trc, w, traceKind);
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

    uint32 traceKind;
#if defined JS_DUMP_CONSERVATIVE_GC_ROOTS || defined JS_GCMETER
    ConservativeGCTest test = 
#endif
    MarkIfGCThingWord(trc, w, traceKind);

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    if (test == CGCT_VALID) {
        if (IS_GC_MARKING_TRACER(trc) && static_cast<GCMarker *>(trc)->conservativeDumpFileName) {
            GCMarker::ConservativeRoot root = {(void *)w, traceKind};
            static_cast<GCMarker *>(trc)->conservativeRoots.append(root);
        }
    }
#endif

#if defined JS_DUMP_CONSERVATIVE_GC_ROOTS || defined JS_GCMETER
    if (IS_GC_MARKING_TRACER(trc))
        static_cast<GCMarker *>(trc)->conservativeStats.counter[test]++;
#endif
}

static void
MarkRangeConservatively(JSTracer *trc, jsuword *begin, jsuword *end)
{
    JS_ASSERT(begin <= end);
    for (jsuword *i = begin; i != end; ++i)
        MarkWordConservatively(trc, *i);
}

static void
MarkThreadDataConservatively(JSTracer *trc, JSThreadData *td)
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
    jsuword *begin = (jsuword *) beginv;
    jsuword *end = (jsuword *) endv;
#ifdef JS_NUNBOX32
    /*
     * With 64-bit jsvals on 32-bit systems, we can optimize a bit by
     * scanning only the payloads.
     */
    JS_ASSERT(begin <= end);
    for (jsuword *i = begin; i != end; i += 2)
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

    /* Update the register snapshot with the latest values. */
#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4611)
#endif
    setjmp(registerSnapshot.jmpbuf);
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
    JS_ASSERT(cx->thread->data.requestDepth >= ctd->requestThreshold);
    if (cx->thread->data.requestDepth == ctd->requestThreshold)
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

    /* Delete all remaining Compartments. Ideally only the defaultCompartment should be left. */
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c) {
        JSCompartment *comp = *c;
        comp->finishArenaLists();
        delete comp;
    }
    rt->compartments.clear();
    rt->defaultCompartment = NULL;

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
JSRuntime::setGCTriggerFactor(uint32 factor)
{
    JS_ASSERT(factor >= 100);

    gcTriggerFactor = factor;
    setGCLastBytes(gcLastBytes);
}

void
JSRuntime::setGCLastBytes(size_t lastBytes)
{
    gcLastBytes = lastBytes;
    uint64 triggerBytes = uint64(lastBytes) * uint64(gcTriggerFactor / 100);
    if (triggerBytes != size_t(triggerBytes))
        triggerBytes = size_t(-1);
    gcTriggerBytes = size_t(triggerBytes);
}

void
FreeLists::purge()
{
    /*
     * Return the free list back to the arena so the GC finalization will not
     * run the finalizers over unitialized bytes from free things.
     */
    for (FreeCell ***p = finalizables; p != JS_ARRAY_END(finalizables); ++p)
        *p = NULL;
}

static inline bool
IsGCThresholdReached(JSRuntime *rt)
{
#ifdef JS_GC_ZEAL
    if (rt->gcZeal >= 1)
        return true;
#endif

    /*
     * Since the initial value of the gcLastBytes parameter is not equal to
     * zero (see the js_InitGC function) the return value is false when
     * the gcBytes value is close to zero at the JS engine start.
     */
    return rt->isGCMallocLimitReached() || rt->gcBytes >= rt->gcTriggerBytes;
}

struct JSShortString;

ArenaList *
GetFinalizableArenaList(JSCompartment *c, unsigned thingKind) {
    JS_ASSERT(thingKind < FINALIZE_LIMIT);
    return &c->arenas[thingKind];
}

#ifdef DEBUG
bool
CheckAllocation(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->thread);
#endif
    JS_ASSERT(!cx->runtime->gcRunning);
    return true;
}
#endif

template <typename T>
inline bool
RefillTypedFreeList(JSContext *cx, unsigned thingKind)
{
    JSCompartment *compartment = cx->compartment;
    JS_ASSERT_IF(compartment->freeLists.finalizables[thingKind],
                 !*compartment->freeLists.finalizables[thingKind]);
    JSRuntime *rt = cx->runtime;

    ArenaList *arenaList;
    Arena<T> *a;

    JS_ASSERT(!rt->gcRunning);
    if (rt->gcRunning)
        return false;

    bool canGC = !JS_ON_TRACE(cx) && !JS_THREAD_DATA(cx)->waiveGCQuota;
    bool doGC = canGC && IsGCThresholdReached(rt);

    arenaList = GetFinalizableArenaList(cx->compartment, thingKind);
    do {
        if (doGC) {
            JS_ASSERT(!JS_ON_TRACE(cx));
#ifdef JS_THREADSAFE
            Conditionally<AutoUnlockDefaultCompartment> unlockDefaultCompartmentIf(cx->compartment == cx->runtime->defaultCompartment &&
                                                                           cx->runtime->defaultCompartmentIsLocked, cx);
#endif
            /* The last ditch GC preserves all atoms. */
            AutoKeepAtoms keep(cx->runtime);
            js_GC(cx, GC_NORMAL);
            METER(cx->runtime->gcStats.retry++);
            canGC = false;
            /*
             * The JSGC_END callback can legitimately allocate new GC
             * things and populate the free list. If that happens, just
             * return that list head.
             */
            if (compartment->freeLists.finalizables[thingKind])
                return true;
        }
        if ((a = (Arena<T> *) arenaList->getNextWithFreeList())) {
            JS_ASSERT(a->header()->freeList);
            JS_ASSERT(sizeof(T) == a->header()->thingSize);
            compartment->freeLists.populate(a, thingKind);
            return true;
        }
        a = AllocateArena<T>(cx, thingKind);
        if (a) {
            compartment->freeLists.populate(a, thingKind);
            arenaList->insert((Arena<FreeCell> *) a);
            a->getMarkingDelay()->init();
            return true;
        }
        if (!canGC) {
            METER(cx->runtime->gcStats.fail++);
            js_ReportOutOfMemory(cx);
            return false;
        }
        doGC = true;
    } while (true);
}

bool
RefillFinalizableFreeList(JSContext *cx, unsigned thingKind)
{
    switch (thingKind) {
      case FINALIZE_OBJECT0:
        return RefillTypedFreeList<JSObject>(cx, thingKind);
      case FINALIZE_OBJECT2:
        return RefillTypedFreeList<JSObject_Slots2>(cx, thingKind);
      case FINALIZE_OBJECT4:
        return RefillTypedFreeList<JSObject_Slots4>(cx, thingKind);
      case FINALIZE_OBJECT8:
        return RefillTypedFreeList<JSObject_Slots8>(cx, thingKind);
      case FINALIZE_OBJECT12:
        return RefillTypedFreeList<JSObject_Slots12>(cx, thingKind);
      case FINALIZE_OBJECT16:
        return RefillTypedFreeList<JSObject_Slots16>(cx, thingKind);
      case FINALIZE_STRING:
      case FINALIZE_EXTERNAL_STRING0:
      case FINALIZE_EXTERNAL_STRING1:
      case FINALIZE_EXTERNAL_STRING2:
      case FINALIZE_EXTERNAL_STRING3:
      case FINALIZE_EXTERNAL_STRING4:
      case FINALIZE_EXTERNAL_STRING5:
      case FINALIZE_EXTERNAL_STRING6:
      case FINALIZE_EXTERNAL_STRING7:
        return RefillTypedFreeList<JSString>(cx, thingKind);
      case FINALIZE_SHORT_STRING:
        return RefillTypedFreeList<JSShortString>(cx, thingKind);
      case FINALIZE_FUNCTION:
        return RefillTypedFreeList<JSFunction>(cx, thingKind);
#if JS_HAS_XML_SUPPORT
      case FINALIZE_XML:
        return RefillTypedFreeList<JSXML>(cx, thingKind);
#endif
      default:
        JS_NOT_REACHED("bad finalize kind");
        return false;
    }
}

intN
js_GetExternalStringGCType(JSString *str) {
    return GetExternalStringGCType(str);
}

uint32
js_GetGCThingTraceKind(void *thing) {
    return GetGCThingTraceKind(thing);
}

JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing)
{
    GCLocks *locks;

    if (!thing)
        return true;
    locks = &rt->gcLocksHash;
    AutoLockGC lock(rt);
    GCLocks::AddPtr p = locks->lookupForAdd(thing);

    if (!p) {
        if (!locks->add(p, thing, 1))
            return false;
    } else {
        JS_ASSERT(p->value >= 1);
        p->value++;
    }

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

JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer *trc, void *thing, uint32 kind)
{
    switch (kind) {
      case JSTRACE_OBJECT: {
        MarkChildren(trc, (JSObject *)thing);
        break;
      }

      case JSTRACE_STRING: {
        MarkChildren(trc, (JSString *)thing);
        break;
      }

#if JS_HAS_XML_SUPPORT
      case JSTRACE_XML:
        MarkChildren(trc, (JSXML *)thing);
        break;
#endif
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
  : color(0), stackLimit(0), unmarkedArenaStackTop(NULL)
{
    JS_TRACER_INIT(this, cx, NULL);
#ifdef DEBUG
    markLaterCount = 0;
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
GCMarker::delayMarkingChildren(void *thing)
{
    Cell *cell = reinterpret_cast<Cell *>(thing);
    Arena<Cell> *a = cell->arena();
    JS_ASSERT(cell->isMarked());
    METER(cell->compartment()->rt->gcStats.unmarked++);
    MarkingDelay *markingDelay = a->getMarkingDelay();

    if (markingDelay->link) {
        if (markingDelay->start > (jsuword)cell)
            markingDelay->start = (jsuword)cell;
        /* Arena already scheduled to be marked again */
        return;
    }
    markingDelay->start = (jsuword)cell;
    Arena<Cell> *tos = unmarkedArenaStackTop;
    markingDelay->link = tos ? tos : a;
    unmarkedArenaStackTop = a;
#ifdef DEBUG
    JSCompartment *comp = cell->compartment();
    markLaterCount += Arena<FreeCell>::ThingsPerArena;
    METER_UPDATE_MAX(comp->rt->gcStats.maxunmarked, markLaterCount);
#endif
}

template<typename T>
void
Arena<T>::markDelayedChildren(JSTracer *trc)
{
    T* thing = (T *)getMarkingDelay()->start;
    T *thingsEnd = &t.things[ThingsPerArena-1].t;
    JS_ASSERT(thing == getAlignedThing(thing));
    while (thing <= thingsEnd) {
        if (thing->asCell()->isMarked())
            MarkChildren(trc, thing);

        thing++;
    }
}

void
GCMarker::markDelayedChildren()
{
    while (Arena<Cell> *a = unmarkedArenaStackTop) {
        /*
         * markingDelay->link == current arena indicates last arena on stack.
         * If marking gets delayed at the same arena again, the arena is pushed 
         * again in delayMarkingChildren. markingDelay->link has to be cleared, 
         * otherwise the arena is not pushed again.
         */
        MarkingDelay *markingDelay = a->getMarkingDelay();
        unmarkedArenaStackTop = (markingDelay->link != a)
            ? markingDelay->link
            : NULL;
        markingDelay->link = NULL;
#ifdef DEBUG
        markLaterCount -= Arena<FreeCell>::ThingsPerArena;
#endif

        switch (a->header()->thingKind) {
            case FINALIZE_OBJECT0:
                reinterpret_cast<Arena<JSObject> *>(a)->markDelayedChildren(this);
                break;
            case FINALIZE_OBJECT2:
                reinterpret_cast<Arena<JSObject_Slots2> *>(a)->markDelayedChildren(this);
                break;
            case FINALIZE_OBJECT4:
                reinterpret_cast<Arena<JSObject_Slots4> *>(a)->markDelayedChildren(this);
                break;
            case FINALIZE_OBJECT8:
                reinterpret_cast<Arena<JSObject_Slots8> *>(a)->markDelayedChildren(this);
                break;
            case FINALIZE_OBJECT12:
                reinterpret_cast<Arena<JSObject_Slots12> *>(a)->markDelayedChildren(this);
                break;
            case FINALIZE_OBJECT16:
                reinterpret_cast<Arena<JSObject_Slots16> *>(a)->markDelayedChildren(this);
                break;
            case FINALIZE_STRING:
            case FINALIZE_EXTERNAL_STRING0:
            case FINALIZE_EXTERNAL_STRING1:
            case FINALIZE_EXTERNAL_STRING2:
            case FINALIZE_EXTERNAL_STRING3:
            case FINALIZE_EXTERNAL_STRING4:
            case FINALIZE_EXTERNAL_STRING5:
            case FINALIZE_EXTERNAL_STRING6:
            case FINALIZE_EXTERNAL_STRING7:
                reinterpret_cast<Arena<JSString> *>(a)->markDelayedChildren(this);
                break;
            case FINALIZE_SHORT_STRING:
                JS_ASSERT(false);
                break;
            case FINALIZE_FUNCTION:
                reinterpret_cast<Arena<JSFunction> *>(a)->markDelayedChildren(this);
                break;
#if JS_HAS_XML_SUPPORT
            case FINALIZE_XML:
                reinterpret_cast<Arena<JSXML> *>(a)->markDelayedChildren(this);
                break;
#endif
            default:
                JS_NOT_REACHED("wrong thingkind");
        }
    }
    JS_ASSERT(markLaterCount == 0);
    JS_ASSERT(!unmarkedArenaStackTop);
}

void
GCMarker::slowifyArrays()
{
    while (!arraysToSlowify.empty()) {
        JSObject *obj = arraysToSlowify.back();
        arraysToSlowify.popBack();
        if (obj->isMarked())
            obj->makeDenseArraySlow(context);
    }
}
} /* namespace js */

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
        if (!JSString::isStatic(ptr)) {
            bool root_points_to_gcArenaList = false;
            JSCompartment **c = trc->context->runtime->compartments.begin();
            for (; c != trc->context->runtime->compartments.end(); ++c) {
                JSCompartment *comp = *c;
                if (checkArenaListsForThing(comp, ptr)) {
                    root_points_to_gcArenaList = true;
                    break;
                }
            }
            if (!root_points_to_gcArenaList && entry.value.name) {
                fprintf(stderr,
"JS API usage error: the address passed to JS_AddNamedRoot currently holds an\n"
"invalid gcthing.  This is usually caused by a missing call to JS_RemoveRoot.\n"
"The root's name is \"%s\".\n",
                        entry.value.name);
            }
            JS_ASSERT(root_points_to_gcArenaList);
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
js_TraceStackFrame(JSTracer *trc, JSStackFrame *fp)
{
    MarkObject(trc, fp->scopeChain(), "scope chain");
    if (fp->isDummyFrame())
        return;

    if (fp->hasCallObj())
        MarkObject(trc, fp->callObj(), "call");
    if (fp->hasArgsObj())
        MarkObject(trc, fp->argsObj(), "arguments");
    if (fp->isScriptFrame())
        js_TraceScript(trc, fp->script());

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
    js::gc::MarkObject(trc, *obj, "js::AutoEnumStateRooter.obj");
}

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag) {
      case JSVAL:
        MarkValue(trc, static_cast<AutoValueRooter *>(this)->val, "js::AutoValueRooter.val");
        return;

      case SHAPE:
        static_cast<AutoShapeRooter *>(this)->shape->trace(trc);
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
            MarkId(trc, desc.id, "PropDesc::id");
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
        Vector<Value, 8> &vector = static_cast<js::AutoValueVector *>(this)->vector;
        MarkValueRange(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }

      case STRING:
        if (JSString *str = static_cast<js::AutoStringRooter *>(this)->str)
            MarkString(trc, str, "js::AutoStringRooter.str");
        return;

      case IDVECTOR: {
        Vector<jsid, 8> &vector = static_cast<js::AutoIdVector *>(this)->vector;
        MarkIdRange(trc, vector.length(), vector.begin(), "js::AutoIdVector.vector");
        return;
      }
    }

    JS_ASSERT(tag >= 0);
    MarkValueRange(trc, tag, static_cast<AutoArrayRooter *>(this)->array, "js::AutoArrayRooter.array");
}

namespace js {

void
MarkContext(JSTracer *trc, JSContext *acx)
{
    /* Stack frames and slots are traced by StackSpace::mark. */

    /* Mark other roots-by-definition in acx. */
    if (acx->globalObject && !JS_HAS_OPTION(acx, JSOPTION_UNROOTED_GLOBAL))
        MarkObject(trc, *acx->globalObject, "global object");
    if (acx->throwing) {
        MarkValue(trc, acx->exception, "exception");
    } else {
        /* Avoid keeping GC-ed junk stored in JSContext.exception. */
        acx->exception.setNull();
    }

    for (js::AutoGCRooter *gcr = acx->autoGCRooters; gcr; gcr = gcr->down)
        gcr->trace(trc);

    if (acx->sharpObjectMap.depth > 0)
        js_TraceSharpMap(trc, &acx->sharpObjectMap);

    MarkValue(trc, acx->iterValue, "iterValue");

    acx->compartment->marked = true;

#ifdef JS_TRACER
    TracerState* state = acx->tracerState;
    while (state) {
        if (state->nativeVp)
            MarkValueRange(trc, state->nativeVpLen, state->nativeVp, "nativeVp");
        state = state->prev;
    }
#endif
}

JS_REQUIRES_STACK void
MarkRuntime(JSTracer *trc)
{
    JSRuntime *rt = trc->context->runtime;

    if (rt->state != JSRTS_LANDING)
        MarkConservativeStackRoots(trc);

    /*
     * Verify that we do not have at this point unmarked GC things stored in
     * autorooters. To maximize test coverage we abort even in non-debug
     * builds for now, see bug 574313.
     */
    JSContext *iter;
#if 1
    iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter)) {
        for (AutoGCRooter *gcr = acx->autoGCRooters; gcr; gcr = gcr->down) {
#ifdef JS_THREADSAFE
            JS_ASSERT_IF(!acx->thread->data.requestDepth, acx->thread->suspendCount);
#endif
            JS_ASSERT(JS_THREAD_DATA(acx)->conservativeGC.hasStackToScan());
            void *thing;
            switch (gcr->tag) {
              default:
                continue;
              case AutoGCRooter::JSVAL: {
                const Value &v = static_cast<AutoValueRooter *>(gcr)->val;
                if (!v.isMarkable())
                    continue;
                thing = v.toGCThing();
                break;
              }
              case AutoGCRooter::XML:
                thing = static_cast<AutoXMLRooter *>(gcr)->xml;
                break;
              case AutoGCRooter::OBJECT:
                thing = static_cast<AutoObjectRooter *>(gcr)->obj;
                if (!thing)
                    continue;
                break;
              case AutoGCRooter::ID: {
                jsid id = static_cast<AutoIdRooter *>(gcr)->id();
                if (!JSID_IS_GCTHING(id))
                    continue;
                thing = JSID_TO_GCTHING(id);
                break;
              }
            }

            if (JSString::isStatic(thing))
                continue;

            if (!reinterpret_cast<Cell *>(thing)->isMarked()) {
                ConservativeGCTest test = MarkIfGCThingWord(trc, reinterpret_cast<jsuword>(thing));
                fprintf(stderr,
                        "Conservative GC scanner has missed the root 0x%p with tag %ld"
                        " on the stack due to %d. The root location 0x%p, distance from"
                        " the stack base %ld, conservative gc span %ld."
                        " Consevtaive GC status for the thread %d."
                        " Aborting.\n",
                        thing, (long) gcr->tag, int(test), (void *) gcr,
                        (long) ((jsword) JS_THREAD_DATA(acx)->nativeStackBase - (jsword) gcr),
                        (long) ((jsword) JS_THREAD_DATA(acx)->nativeStackBase -
                                (jsword) JS_THREAD_DATA(acx)->conservativeGC.nativeStackTop),
                        int(JS_THREAD_DATA(acx)->conservativeGC.hasStackToScan()));
                JS_ASSERT(false);
                abort();
            }
        }
    }
#endif

    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront())
        gc_root_traversal(trc, r.front());

    for (GCLocks::Range r = rt->gcLocksHash.all(); !r.empty(); r.popFront())
        gc_lock_traversal(r.front(), trc);

    js_TraceAtomState(trc);
    js_MarkTraps(trc);

    iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
        MarkContext(trc, acx);

    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->mark(trc);

    if (rt->emptyArgumentsShape)
        rt->emptyArgumentsShape->trace(trc);
    if (rt->emptyBlockShape)
        rt->emptyBlockShape->trace(trc);
    if (rt->emptyCallShape)
        rt->emptyCallShape->trace(trc);
    if (rt->emptyDeclEnvShape)
        rt->emptyDeclEnvShape->trace(trc);
    if (rt->emptyEnumeratorShape)
        rt->emptyEnumeratorShape->trace(trc);
    if (rt->emptyWithShape)
        rt->emptyWithShape->trace(trc);

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
    TriggerAllOperationCallbacks(rt);
}

} /* namespace js */

void
js_DestroyScriptsToGC(JSContext *cx, JSThreadData *data)
{
    JSScript **listp, *script;

    for (size_t i = 0; i != JS_ARRAY_LENGTH(data->scriptsToGC); ++i) {
        listp = &data->scriptsToGC[i];
        while ((script = *listp) != NULL) {
            *listp = script->u.nextToGC;
            script->u.nextToGC = NULL;
            js_DestroyScript(cx, script);
        }
    }
}

intN
js_ChangeExternalStringFinalizer(JSStringFinalizeOp oldop,
                                 JSStringFinalizeOp newop)
{
    for (uintN i = 0; i != JS_ARRAY_LENGTH(str_finalizers); i++) {
        if (str_finalizers[i] == oldop) {
            str_finalizers[i] = newop;
            return intN(i);
        }
    }
    return -1;
}

/*
 * This function is called from js_FinishAtomState to force the finalization
 * of the permanently interned strings when cx is not available.
 */
void
js_FinalizeStringRT(JSRuntime *rt, JSString *str)
{
    JS_RUNTIME_UNMETER(rt, liveStrings);
    JS_ASSERT(!JSString::isStatic(str));
    JS_ASSERT(!str->isRope());

    if (str->isDependent()) {
        /* A dependent string can not be external and must be valid. */
        JS_ASSERT(str->asCell()->arena()->header()->thingKind == FINALIZE_STRING);
        JS_ASSERT(str->dependentBase());
        JS_RUNTIME_UNMETER(rt, liveDependentStrings);
    } else {
        unsigned thingKind = str->asCell()->arena()->header()->thingKind;
        JS_ASSERT(IsFinalizableStringKind(thingKind));

        /* A stillborn string has null chars, so is not valid. */
        jschar *chars = str->flatChars();
        if (!chars)
            return;
        if (thingKind == FINALIZE_STRING) {
            rt->free(chars);
        } else if (thingKind != FINALIZE_SHORT_STRING) {
            unsigned type = thingKind - FINALIZE_EXTERNAL_STRING0;
            JS_ASSERT(type < JS_ARRAY_LENGTH(str_finalizers));
            JSStringFinalizeOp finalizer = str_finalizers[type];
            if (finalizer) {
                /*
                 * Assume that the finalizer for the permanently interned
                 * string knows how to deal with null context.
                 */
                finalizer(NULL, str);
            }
        }
    }
}

template<typename T>
static void
FinalizeArenaList(JSCompartment *comp, JSContext *cx, unsigned thingKind)
{
    JS_STATIC_ASSERT(!(sizeof(T) & Cell::CellMask));
    ArenaList *arenaList = GetFinalizableArenaList(comp, thingKind);
    Arena<FreeCell> **ap = &arenaList->head;
    Arena<T> *a = (Arena<T> *) *ap;
    if (!a)
        return;
    JS_ASSERT(sizeof(T) == arenaList->head->header()->thingSize);

#ifdef JS_GCMETER
    uint32 nlivearenas = 0, nkilledarenas = 0, nthings = 0;
#endif
    for (;;) {
        ArenaHeader *header = a->header();
        JS_ASSERT_IF(header->hasFreeThings, header->freeList);
        JS_ASSERT(header->thingKind == thingKind);
        JS_ASSERT(!a->getMarkingDelay()->link);
        JS_ASSERT(a->getMarkingDelay()->unmarkedChildren == 0);
        JS_ASSERT(a->header()->isUsed);

        FreeCell *nextFree = header->freeList;
        FreeCell *freeList = NULL;
        FreeCell **tailp = &freeList;
        bool allClear = true;

        T *thingsEnd = &a->t.things[a->ThingsPerArena-1].t;
        T *thing = &a->t.things[0].t;
        thingsEnd++;

        if (!nextFree) {
            nextFree = thingsEnd->asFreeCell();
        } else {
            JS_ASSERT(thing->asCell() <= nextFree);
            JS_ASSERT(nextFree < thingsEnd->asCell());
        }

        for (;; thing++) {
            if (thing->asCell() == nextFree) {
                if (thing == thingsEnd)
                    break;
                nextFree = nextFree->link;
                if (!nextFree) {
                    nextFree = thingsEnd->asFreeCell();
                } else {
                    JS_ASSERT(thing->asCell() < nextFree);
                    JS_ASSERT(nextFree < thingsEnd->asFreeCell());
                }
            } else if (thing->asCell()->isMarked()) {
                allClear = false;
                METER(nthings++);
                continue;
            } else {
                thing->finalize(cx, thingKind);
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
#endif
        if (allClear) {
            /*
             * Forget just assembled free list head for the arena and
             * add the arena itself to the destroy list.
             */
            JS_ASSERT(nfree == a->ThingsPerArena);
            JS_ASSERT((T *)tailp == &a->t.things[a->ThingsPerArena-1].t);
            *tailp = NULL;
            header->freeList = freeList;
#ifdef DEBUG
            header->hasFreeThings = true;
#endif
            *ap = (header->next);
            JS_ASSERT((T *)header->freeList == &a->t.things[0].t);
            a->chunk()->releaseArena(a);
            METER(nkilledarenas++);
        } else {
            JS_ASSERT(nfree < a->ThingsPerArena);
            *tailp = NULL;
            header->freeList = freeList;
#ifdef DEBUG
            header->hasFreeThings = (nfree == 0) ? false : true;
#endif
            ap = &header->next;
            METER(nlivearenas++);
        }
        if (!(a = (Arena<T> *) *ap))
            break;
    }
    arenaList->cursor = arenaList->head;
    METER(UpdateCompartmentStats(comp, thingKind, nlivearenas, nkilledarenas, nthings));
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
GCHelperThread::waitBackgroundSweepEnd(JSRuntime *rt)
{
    AutoLockGC lock(rt);
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
        freeCursor = (void **) js_malloc(FREE_ARRAY_SIZE);
        if (!freeCursor) {
            freeCursorEnd = NULL;
            break;
        }
        freeCursorEnd = freeCursor + FREE_ARRAY_LENGTH;
        *freeCursor++ = ptr;
        return;
    } while (false);
    js_free(ptr);
}

void
GCHelperThread::doSweep()
{
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
SweepCompartments(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;
    JSCompartmentCallback callback = rt->compartmentCallback;
    JSCompartment **read = rt->compartments.begin();
    JSCompartment **end = rt->compartments.end();
    JSCompartment **write = read;

    /* Delete defaultCompartment only during runtime shutdown */
    rt->defaultCompartment->marked = true;

    while (read < end) {
        JSCompartment *compartment = (*read++);
        if (compartment->marked) {
            compartment->marked = false;
            *write++ = compartment;
            /* Remove dead wrappers from the compartment map. */
            compartment->sweep(cx);
        } else {
            JS_ASSERT(compartment->freeLists.isEmpty());
            if (compartment->arenaListsAreEmpty() || gckind == GC_LAST_CONTEXT) {
                if (callback)
                    (void) callback(cx, compartment, JSCOMPARTMENT_DESTROY);
                if (compartment->principals)
                    JSPRINCIPALS_DROP(cx, compartment->principals);
                delete compartment;
            } else {
                compartment->marked = false;
                *write++ = compartment;
                compartment->sweep(cx);
            }
        }
    }
    rt->compartments.resize(write - rt->compartments.begin());
}

/*
 * Common cache invalidation and so forth that must be done before GC. Even if
 * GCUntilDone calls GC several times, this work needs to be done only once.
 */
static void
PreGCCleanup(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    /* Clear gcIsNeeded now, when we are about to start a normal GC cycle. */
    rt->gcIsNeeded = JS_FALSE;

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
        rt->shapeGen = Shape::LAST_RESERVED_SHAPE;
        rt->protoHazardShape = 0;
    }
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
        (*c)->purge(cx);

    js_PurgeThreads(cx);
    {
        JSContext *iter = NULL;
        while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
            acx->purge();
    }
}

/*
 * Perform mark-and-sweep GC.
 *
 * In a JS_THREADSAFE build, the calling thread must be rt->gcThread and each
 * other thread must be either outside all requests or blocked waiting for GC
 * to finish. Note that the caller does not hold rt->gcLock.
 */
static void
MarkAndSweep(JSContext *cx, JSGCInvocationKind gckind GCTIMER_PARAM)
{
    JSRuntime *rt = cx->runtime;
    rt->gcNumber++;

    /*
     * Mark phase.
     */
    GCMarker gcmarker(cx);
    JS_ASSERT(IS_GC_MARKING_TRACER(&gcmarker));
    JS_ASSERT(gcmarker.getMarkColor() == BLACK);
    rt->gcMarkingTracer = &gcmarker;
    gcmarker.stackLimit = cx->stackLimit;

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
         r.front()->clearMarkBitmap();

    MarkRuntime(&gcmarker);
    js_MarkScriptFilenames(rt);

    /*
     * Mark children of things that caused too deep recursion during the above
     * tracing.
     */
    gcmarker.markDelayedChildren();

    rt->gcMarkingTracer = NULL;

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_MARK_END);

#ifdef JS_THREADSAFE
    /*
     * cx->gcBackgroundFree is set if we need several mark-and-sweep loops to
     * finish the GC.
     */
    if(!cx->gcBackgroundFree) {
        /* Wait until the sweeping from the previois GC finishes. */
        rt->gcHelperThread.waitBackgroundSweepEnd(rt);
        cx->gcBackgroundFree = &rt->gcHelperThread;
    }
#endif

    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of rt->gcLock but with rt->gcRunning set
     * so that any attempt to allocate a GC-thing from a finalizer will fail,
     * rather than nest badly and leave the unmarked newborn to be swept.
     *
     * We first sweep atom state so we can use js_IsAboutToBeFinalized on
     * JSString held in a hashtable to check if the hashtable entry can be
     * freed. Note that even after the entry is freed, JSObject finalizers can
     * continue to access the corresponding JSString* assuming that they are
     * unique. This works since the atomization API must not be called during
     * the GC.
     */
    TIMESTAMP(startSweep);
    js_SweepAtomState(cx);

    /* Finalize watch points associated with unreachable objects. */
    js_SweepWatchPoints(cx);

#ifdef DEBUG
    /* Save the pre-sweep count of scope-mapped properties. */
    rt->liveObjectPropsPreSweep = rt->liveObjectProps;
#endif

#ifdef JS_TRACER
    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->traceMonitor.sweep();
#endif

    /*
     * We finalize iterators before other objects so the iterator can use the
     * object which properties it enumerates over to finalize the enumeration
     * state. We finalize objects before other GC things to ensure that
     * object's finalizer can access them even if they will be freed.
     */

    for (JSCompartment **comp = rt->compartments.begin(); comp != rt->compartments.end(); comp++) {
        FinalizeArenaList<JSObject>(*comp, cx, FINALIZE_OBJECT0);
        FinalizeArenaList<JSObject_Slots2>(*comp, cx, FINALIZE_OBJECT2);
        FinalizeArenaList<JSObject_Slots4>(*comp, cx, FINALIZE_OBJECT4);
        FinalizeArenaList<JSObject_Slots8>(*comp, cx, FINALIZE_OBJECT8);
        FinalizeArenaList<JSObject_Slots12>(*comp, cx, FINALIZE_OBJECT12);
        FinalizeArenaList<JSObject_Slots16>(*comp, cx, FINALIZE_OBJECT16);
        FinalizeArenaList<JSFunction>(*comp, cx, FINALIZE_FUNCTION);
#if JS_HAS_XML_SUPPORT
        FinalizeArenaList<JSXML>(*comp, cx, FINALIZE_XML);
#endif
    }
    TIMESTAMP(sweepObjectEnd);

    /*
     * We sweep the deflated cache before we finalize the strings so the
     * cache can safely use js_IsAboutToBeFinalized..
     */
    rt->deflatedStringCache->sweep(cx);

    for (JSCompartment **comp = rt->compartments.begin(); comp != rt->compartments.end(); comp++) {
        FinalizeArenaList<JSShortString>(*comp, cx, FINALIZE_SHORT_STRING);
        FinalizeArenaList<JSString>(*comp, cx, FINALIZE_STRING);
        for (unsigned i = FINALIZE_EXTERNAL_STRING0; i <= FINALIZE_EXTERNAL_STRING_LAST; ++i)
            FinalizeArenaList<JSString>(*comp, cx, i);
    }

    rt->gcNewArenaTriggerBytes = rt->gcBytes < GC_ARENA_ALLOCATION_TRIGGER ?
                                 GC_ARENA_ALLOCATION_TRIGGER :
                                 rt->gcBytes;

    TIMESTAMP(sweepStringEnd);

    SweepCompartments(cx, gckind);

    /*
     * Sweep the runtime's property trees after finalizing objects, in case any
     * had watchpoints referencing tree nodes.
     */
    js::PropertyTree::sweepShapes(cx);

    /*
     * Sweep script filenames after sweeping functions in the generic loop
     * above. In this way when a scripted function's finalizer destroys the
     * script and calls rt->destroyScriptHook, the hook can still access the
     * script's filename. See bug 323267.
     */
    js_SweepScriptFilenames(rt);

    /* Slowify arrays we have accumulated. */
    gcmarker.slowifyArrays();

    /*
     * Destroy arenas after we finished the sweeping so finalizers can safely
     * use js_IsAboutToBeFinalized().
     */
    ExpireGCChunks(rt);
    TIMESTAMP(sweepDestroyEnd);

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
#ifdef DEBUG_srcnotesize
  { extern void DumpSrcNoteSizeHist();
    DumpSrcNoteSizeHist();
    printf("GC HEAP SIZE %lu\n", (unsigned long)rt->gcBytes);
  }
#endif

#ifdef JS_SCOPE_DEPTH_METER
    DumpScopeDepthMeter(rt);
#endif

#ifdef JS_DUMP_LOOP_STATS
    DumpLoopStats(rt);
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
    JS_ASSERT(cx->thread != rt->gcThread);

    size_t requestDebit = cx->thread->data.requestDepth ? 1 : 0;
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

    /* See comments before another call to js_ShareWaitingTitles below. */
    cx->thread->gcWaiting = true;
    js_ShareWaitingTitles(cx);

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

    cx->thread->gcWaiting = false;
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
    if (rt->gcThread && rt->gcThread != cx->thread)
        LetOtherGCFinish(cx);
#endif

    JS_ASSERT(!rt->gcRunning);

#ifdef JS_THREADSAFE
    /* No other thread is in GC, so indicate that we're now in GC. */
    JS_ASSERT(!rt->gcThread);
    rt->gcThread = cx->thread;

    /*
     * Notify operation callbacks on other threads, which will give them a
     * chance to yield their requests. Threads without requests perform their
     * callback at some later point, which then will be unnecessary, but
     * harmless.
     */
    for (JSThread::Map::Range r = rt->threads.all(); !r.empty(); r.popFront()) {
        JSThread *thread = r.front().value;
        if (thread != cx->thread)
            thread->data.triggerOperationCallback(rt);
    }

    /*
     * Discount the request on the current thread from contributing to
     * rt->requestCount before we wait for all other requests to finish.
     * JS_NOTIFY_REQUEST_DONE, which will wake us up, is only called on
     * rt->requestCount transitions to 0.
     */
    size_t requestDebit = cx->thread->data.requestDepth ? 1 : 0;
    JS_ASSERT(requestDebit <= rt->requestCount);
    if (requestDebit != rt->requestCount) {
        rt->requestCount -= requestDebit;

        /*
         * Share any title that is owned by the GC thread before we wait, to
         * avoid a deadlock with ClaimTitle. We also set the gcWaiting flag so
         * that ClaimTitle can claim the title ownership from the GC thread if
         * that function is called while the GC is waiting.
         */
        cx->thread->gcWaiting = true;
        js_ShareWaitingTitles(cx);
        do {
            JS_AWAIT_REQUEST_DONE(rt);
        } while (rt->requestCount > 0);
        cx->thread->gcWaiting = false;
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
    JS_ASSERT(rt->gcThread == context->thread);
    rt->gcThread = NULL;
    JS_NOTIFY_GC_DONE(rt);
#endif
}

/*
 * GC, repeatedly if necessary, until we think we have not created any new
 * garbage and no other threads are demanding more GC.
 */
static void
GCUntilDone(JSContext *cx, JSGCInvocationKind gckind  GCTIMER_PARAM)
{
    if (JS_ON_TRACE(cx))
        return;

    JSRuntime *rt = cx->runtime;

    /* Recursive GC or a call from another thread restarts the GC cycle. */
    if (rt->gcMarkAndSweep) {
        rt->gcPoke = true;
#ifdef JS_THREADSAFE
        JS_ASSERT(rt->gcThread);
        if (rt->gcThread != cx->thread) {
            /* We do not return until another GC finishes. */
            LetOtherGCFinish(cx);
        }
#endif
        return;
    }

    AutoGCSession gcsession(cx);

    METER(rt->gcStats.poke++);

    bool firstRun = true;
    rt->gcMarkAndSweep = true;
#ifdef JS_THREADSAFE
    JS_ASSERT(!cx->gcBackgroundFree);
#endif
    do {
        rt->gcPoke = false;

        AutoUnlockGC unlock(rt);
        if (firstRun) {
            PreGCCleanup(cx, gckind);
            TIMESTAMP(startMark);
            firstRun = false;
        }
        MarkAndSweep(cx, gckind  GCTIMER_ARG);

        // GC again if:
        //   - another thread, not in a request, called js_GC
        //   - js_GC was called recursively
        //   - a finalizer called js_RemoveRoot or js_UnlockGCThingRT.
    } while (rt->gcPoke);

#ifdef JS_THREADSAFE
    JS_ASSERT(cx->gcBackgroundFree == &rt->gcHelperThread);
    cx->gcBackgroundFree = NULL;
    rt->gcHelperThread.startBackgroundSweep(rt);
#endif

    rt->gcMarkAndSweep = false;
    rt->gcRegenShapes = false;
    rt->setGCLastBytes(rt->gcBytes);
}

/*
 * The gckind flag bit GC_LOCK_HELD indicates a call from js_NewGCThing with
 * rt->gcLock already held, so the lock should be kept on return.
 */
void
js_GC(JSContext *cx, JSGCInvocationKind gckind)
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
    JS_ASSERT(JS_CHECK_STACK_SIZE(cx->stackLimit, &stackDummy));
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
            Conditionally<AutoUnlockGC> unlockIf(!!(gckind & GC_LOCK_HELD), rt);
            if (!callback(cx, JSGC_BEGIN) && gckind != GC_LAST_CONTEXT)
                return;
        }

        {
            /* Lock out other GC allocator and collector invocations. */
            Conditionally<AutoLockGC> lockIf(!(gckind & GC_LOCK_HELD), rt);

            GCUntilDone(cx, gckind  GCTIMER_ARG);
        }

        /* We re-sample the callback again as the finalizers can change it. */
        if (JSGCCallback callback = rt->gcCallback) {
            Conditionally<AutoUnlockGC> unlockIf(gckind & GC_LOCK_HELD, rt);

            (void) callback(cx, JSGC_END);
        }

        /*
         * On shutdown, iterate until the JSGC_END callback stops creating
         * garbage.
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
    JS_ASSERT(cx->thread->data.requestDepth);

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
        obj2 = obj2->wrappedObject(cx);
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
    JSCompartment *compartment = new JSCompartment(rt);
    if (!compartment || !compartment->init()) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    if (principals) {
        compartment->principals = principals;
        JSPRINCIPALS_HOLD(cx, principals);
    }

    {
        AutoLockGC lock(rt);

        if (!rt->compartments.append(compartment)) {
            AutoUnlockGC unlock(rt);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }

    JSCompartmentCallback callback = rt->compartmentCallback;
    if (callback && !callback(cx, compartment, JSCOMPARTMENT_NEW)) {
        AutoLockGC lock(rt);
        rt->compartments.popBack();
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
      
        if (rt->gcThread != cx->thread) {
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

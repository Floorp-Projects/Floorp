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
#include "jshashtable.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsprobes.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jswatchpoint.h"
#include "jsweakmap.h"
#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "methodjit/MethodJIT.h"
#include "vm/String.h"

#include "jsobjinlines.h"

#include "vm/String-inl.h"
#include "ion/IonCode.h"
#include "ion/IonMacroAssembler.h"

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
JS_STATIC_ASSERT(JSTRACE_OBJECT      == 0);
JS_STATIC_ASSERT(JSTRACE_STRING      == 1);
JS_STATIC_ASSERT(JSTRACE_SHAPE       == 2);
JS_STATIC_ASSERT(JSTRACE_TYPE_OBJECT == 3);
JS_STATIC_ASSERT(JSTRACE_IONCODE     == 4);
JS_STATIC_ASSERT(JSTRACE_XML         == 5);

/*
 * JS_IS_VALID_TRACE_KIND assumes that JSTRACE_IONCODE is the last non-xml
 * trace kind when JS_HAS_XML_SUPPORT is false.
 */
JS_STATIC_ASSERT(JSTRACE_IONCODE + 1 == JSTRACE_XML);

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
    sizeof(types::TypeObject),  /* FINALIZE_TYPE_OBJECT         */
#if JS_HAS_XML_SUPPORT
    sizeof(JSXML),              /* FINALIZE_XML                 */
#endif
    sizeof(JSShortString),      /* FINALIZE_SHORT_STRING        */
    sizeof(JSString),           /* FINALIZE_STRING              */
    sizeof(JSExternalString),   /* FINALIZE_EXTERNAL_STRING     */
    sizeof(ion::IonCode),       /* FINALIZE_IONCODE             */
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(GCThingSizeMap) == FINALIZE_LIMIT);

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
    FreeSpan *list = &compartment->freeLists.lists[getThingKind()];
    if (list->isEmpty() || firstSpan.arenaAddress() != list->arenaAddress())
        return;

    /*
     * Here this arena has free things, FreeList::lists[thingKind] is not
     * empty and also points to this arena. Thus they must the same.
     */
    JS_ASSERT(firstSpan.isSameNonEmptySpan(list));
}
#endif

template<typename T>
inline bool
Arena::finalize(JSContext *cx)
{
    JS_ASSERT(aheader.allocated());
    JS_ASSERT(!aheader.getMarkingDelay()->link);

    uintptr_t thing = thingsStart(sizeof(T));
    uintptr_t lastByte = thingsEnd() - 1;

    FreeSpan nextFree(aheader.getFirstFreeSpan());
    nextFree.checkSpan();

    FreeSpan newListHead;
    FreeSpan *newListTail = &newListHead;
    uintptr_t newFreeSpanStart = 0;
    bool allClear = true;
#ifdef DEBUG
    size_t nmarked = 0;
#endif
    for (;; thing += sizeof(T)) {
        JS_ASSERT(thing <= lastByte + 1);
        if (thing == nextFree.first) {
            JS_ASSERT(nextFree.last <= lastByte);
            if (nextFree.last == lastByte)
                break;
            JS_ASSERT(Arena::isAligned(nextFree.last, sizeof(T)));
            if (!newFreeSpanStart)
                newFreeSpanStart = thing;
            thing = nextFree.last;
            nextFree = *nextFree.nextSpan();
            nextFree.checkSpan();
        } else {
            T *t = reinterpret_cast<T *>(thing);
            if (t->isMarked()) {
                allClear = false;
#ifdef DEBUG
                nmarked++;
#endif
                if (newFreeSpanStart) {
                    JS_ASSERT(thing >= thingsStart(sizeof(T)) + sizeof(T));
                    newListTail->first = newFreeSpanStart;
                    newListTail->last = thing - sizeof(T);
                    newListTail = newListTail->nextSpanUnchecked(sizeof(T));
                    newFreeSpanStart = NULL;
                }
            } else {
                if (!newFreeSpanStart)
                    newFreeSpanStart = thing;
                t->finalize(cx);
                memset(t, JS_FREE_PATTERN, sizeof(T));
            }
        }
    }

    if (allClear) {
        JS_ASSERT(newListTail == &newListHead);
        JS_ASSERT(newFreeSpanStart == thingsStart(sizeof(T)));
        return true;
    }

    newListTail->first = newFreeSpanStart ? newFreeSpanStart : nextFree.first;
    JS_ASSERT(Arena::isAligned(newListTail->first, sizeof(T)));
    newListTail->last = lastByte;

#ifdef DEBUG
    size_t nfree = 0;
    for (const FreeSpan *span = &newListHead; span != newListTail; span = span->nextSpan()) {
        span->checkSpan();
        JS_ASSERT(Arena::isAligned(span->first, sizeof(T)));
        JS_ASSERT(Arena::isAligned(span->last, sizeof(T)));
        nfree += (span->last - span->first) / sizeof(T) + 1;
        JS_ASSERT(nfree + nmarked <= thingsPerArena(sizeof(T)));
    }
    nfree += (newListTail->last + 1 - newListTail->first) / sizeof(T);
    JS_ASSERT(nfree + nmarked == thingsPerArena(sizeof(T)));
#endif
    aheader.setFirstFreeSpan(&newListHead);

    return false;
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
        bool allClear = aheader->getArena()->finalize<T>(cx);
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
    info.numFree = ArenasPerChunk;

    /* Assemble all arenas into a linked list and mark them as not allocated. */
    ArenaHeader **prevp = &info.emptyArenaListHead;
    Arena *end = &arenas[JS_ARRAY_LENGTH(arenas)];
    for (Arena *a = &arenas[0]; a != end; ++a) {
#ifdef DEBUG
        memset(a, ArenaSize, JS_FREE_PATTERN);
#endif
        *prevp = &a->aheader;
        a->aheader.setAsNotAllocated();
        prevp = &a->aheader.next;
    }
    *prevp = NULL;

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

template <size_t thingSize>
ArenaHeader *
Chunk::allocateArena(JSContext *cx, unsigned thingKind)
{
    JSCompartment *comp = cx->compartment;
    JS_ASSERT(hasAvailableArenas());
    ArenaHeader *aheader = info.emptyArenaListHead;
    info.emptyArenaListHead = aheader->next;
    aheader->init(comp, thingKind, thingSize);
    --info.numFree;

    JSRuntime *rt = info.runtime;
    Probes::resizeHeap(comp, rt->gcBytes, rt->gcBytes + ArenaSize);
    JS_ATOMIC_ADD(&rt->gcBytes, ArenaSize);
    JS_ATOMIC_ADD(&comp->gcBytes, ArenaSize);
    if (comp->gcBytes >= comp->gcTriggerBytes)
        TriggerCompartmentGC(comp);

    return aheader;
}

void
Chunk::releaseArena(ArenaHeader *aheader)
{
    JS_ASSERT(aheader->allocated());
    JSRuntime *rt = info.runtime;
#ifdef JS_THREADSAFE
    Maybe<AutoLockGC> maybeLock;
    if (rt->gcHelperThread.sweeping)
        maybeLock.construct(info.runtime);
#endif
    JSCompartment *comp = aheader->compartment;

    Probes::resizeHeap(comp, rt->gcBytes, rt->gcBytes - ArenaSize);
    JS_ASSERT(size_t(rt->gcBytes) >= ArenaSize);
    JS_ASSERT(size_t(comp->gcBytes) >= ArenaSize);
#ifdef JS_THREADSAFE
    if (rt->gcHelperThread.sweeping) {
        rt->reduceGCTriggerBytes(GC_HEAP_GROWTH_FACTOR * ArenaSize);
        comp->reduceGCTriggerBytes(GC_HEAP_GROWTH_FACTOR * ArenaSize);
    }
#endif
    JS_ATOMIC_ADD(&rt->gcBytes, -int32(ArenaSize));
    JS_ATOMIC_ADD(&comp->gcBytes, -int32(ArenaSize));

    aheader->setAsNotAllocated();
    aheader->next = info.emptyArenaListHead;
    info.emptyArenaListHead = aheader;
    ++info.numFree;
    if (unused())
        info.age = 0;
}

inline Chunk *
AllocateGCChunk(JSRuntime *rt)
{
    Chunk *p = (Chunk *)rt->gcChunkAllocator->alloc();
#ifdef MOZ_GCTIMER
    if (p)
        JS_ATOMIC_INCREMENT(&newChunkCount);
#endif
    return p;
}

inline void
ReleaseGCChunk(JSRuntime *rt, Chunk *p)
{
    JS_ASSERT(p);
#ifdef MOZ_GCTIMER
    JS_ATOMIC_INCREMENT(&destroyChunkCount);
#endif
    rt->gcChunkAllocator->free_(p);
}

inline Chunk *
PickChunk(JSContext *cx)
{
    Chunk *chunk = cx->compartment->chunk;
    if (chunk && chunk->hasAvailableArenas())
        return chunk;

    JSRuntime *rt = cx->runtime;
    bool isSystemCompartment = cx->compartment->isSystemCompartment;

    /*
     * The chunk used for the last allocation is full, search all chunks for
     * free arenas.
     */
    GCChunkSet::Range
        r(isSystemCompartment ? rt->gcSystemChunkSet.all() : rt->gcUserChunkSet.all());
    for (; !r.empty(); r.popFront()) {
        chunk = r.front();
        if (chunk->hasAvailableArenas()) {
            cx->compartment->chunk = chunk;
            return chunk;
        }
    }

    chunk = AllocateGCChunk(rt);
    if (!chunk) {
        /* Our last chance is to find an empty chunk in the other chunk set. */
        GCChunkSet::Enum e(isSystemCompartment ? rt->gcUserChunkSet : rt->gcSystemChunkSet);
        for (; !e.empty(); e.popFront()) {
            if (e.front()->info.numFree == ArenasPerChunk) {
                chunk = e.front();
                e.removeFront();
                break;
            }
        }

        if (!chunk)
            return NULL;
    }

    /*
     * FIXME bug 583732 - chunk is newly allocated and cannot be present in
     * the table so using ordinary lookupForAdd is suboptimal here.
     */
    GCChunkSet::AddPtr p = isSystemCompartment ?
                           rt->gcSystemChunkSet.lookupForAdd(chunk) :
                           rt->gcUserChunkSet.lookupForAdd(chunk);
    JS_ASSERT(!p);
    if (isSystemCompartment) {
        if (!rt->gcSystemChunkSet.add(p, chunk)) {
            ReleaseGCChunk(rt, chunk);
            return NULL;
        }
    } else {
        if (!rt->gcUserChunkSet.add(p, chunk)) {
            ReleaseGCChunk(rt, chunk);
            return NULL;
        }
    }

    chunk->init(rt);
    cx->compartment->chunk = chunk;
    rt->gcChunkAllocationSinceLastGC = true;
    return chunk;
}

static void
ExpireGCChunks(JSRuntime *rt, JSGCInvocationKind gckind)
{
    static const size_t MaxAge = 3;

    /* Remove unused chunks. */
    AutoLockGC lock(rt);

    rt->gcChunksWaitingToExpire = 0;
    for (GCChunkSet::Enum e(rt->gcUserChunkSet); !e.empty(); e.popFront()) {
        Chunk *chunk = e.front();
        JS_ASSERT(chunk->info.runtime == rt);
        if (chunk->unused()) {
            if (gckind == GC_SHRINK || chunk->info.age++ > MaxAge) {
                e.removeFront();
                ReleaseGCChunk(rt, chunk);
                continue;
            }
            rt->gcChunksWaitingToExpire++;
        }
    }
    for (GCChunkSet::Enum e(rt->gcSystemChunkSet); !e.empty(); e.popFront()) {
        Chunk *chunk = e.front();
        JS_ASSERT(chunk->info.runtime == rt);
        if (chunk->unused()) {
            if (gckind == GC_SHRINK || chunk->info.age++ > MaxAge) {
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
    if (!rt->gcUserChunkSet.init(16))
        return false;

    if (!rt->gcSystemChunkSet.init(16))
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
    rt->setGCLastBytes(8192, GC_NORMAL);

    rt->gcJitReleaseTime = PRMJ_Now() + JIT_SCRIPT_EIGHTH_LIFETIME;
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

template <typename T>
inline ConservativeGCTest
MarkArenaPtrConservatively(JSTracer *trc, ArenaHeader *aheader, uintptr_t addr)
{
    JS_ASSERT(aheader->allocated());
    JS_ASSERT(sizeof(T) == aheader->getThingSize());

    uintptr_t offset = addr & ArenaMask;
    uintptr_t minOffset = Arena::thingsStartOffset(sizeof(T));
    if (offset < minOffset)
        return CGCT_NOTARENA;

    /* addr can point inside the thing so we must align the address. */
    uintptr_t shift = (offset - minOffset) % sizeof(T);
    addr -= shift;

    /*
     * Check if the thing is free. We must use the list of free spans as at
     * this point we no longer have the mark bits from the previous GC run and
     * we must account for newly allocated things.
     */
    if (InFreeList(aheader, addr))
        return CGCT_NOTLIVE;

    T *thing = reinterpret_cast<T *>(addr);
    MarkRoot(trc, thing, "machine stack");

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

    if (!trc->context->runtime->gcUserChunkSet.has(chunk) &&
        !trc->context->runtime->gcSystemChunkSet.has(chunk))
        return CGCT_NOTCHUNK;

    /*
     * We query for pointers outside the arena array after checking for an
     * allocated chunk. Such pointers are rare and we want to reject them
     * after doing more likely rejections.
     */
    if (!Chunk::withinArenasRange(addr))
        return CGCT_NOTARENA;

    ArenaHeader *aheader = &chunk->arenas[Chunk::arenaIndex(addr)].aheader;

    if (!aheader->allocated())
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
      case FINALIZE_TYPE_OBJECT:
        test = MarkArenaPtrConservatively<types::TypeObject>(trc, aheader, addr);
        break;
#if JS_HAS_XML_SUPPORT
      case FINALIZE_XML:
        test = MarkArenaPtrConservatively<JSXML>(trc, aheader, addr);
        break;
#endif
      case FINALIZE_IONCODE:
        test = MarkArenaPtrConservatively<ion::IonCode>(trc, aheader, addr);
        break;
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
    /* Delete all remaining Compartments. */
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c) {
        JSCompartment *comp = *c;
        comp->finishArenaLists();
        Foreground::delete_(comp);
    }
    rt->compartments.clear();
    rt->atomsCompartment = NULL;

    for (GCChunkSet::Range r(rt->gcUserChunkSet.all()); !r.empty(); r.popFront())
        ReleaseGCChunk(rt, r.front());
    for (GCChunkSet::Range r(rt->gcSystemChunkSet.all()); !r.empty(); r.popFront())
        ReleaseGCChunk(rt, r.front());
    rt->gcUserChunkSet.clear();
    rt->gcSystemChunkSet.clear();

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
JSRuntime::setGCLastBytes(size_t lastBytes, JSGCInvocationKind gckind)
{
    gcLastBytes = lastBytes;

    size_t base = gckind == GC_SHRINK ? lastBytes : Max(lastBytes, GC_ARENA_ALLOCATION_TRIGGER);
    float trigger = float(base) * GC_HEAP_GROWTH_FACTOR;
    gcTriggerBytes = size_t(Min(float(gcMaxBytes), trigger));
}

void
JSRuntime::reduceGCTriggerBytes(uint32 amount) {
    JS_ASSERT(amount > 0);
    JS_ASSERT(gcTriggerBytes - amount >= 0);
    if (gcTriggerBytes - amount < GC_ARENA_ALLOCATION_TRIGGER * GC_HEAP_GROWTH_FACTOR)
        return;
    gcTriggerBytes -= amount;
}

void
JSCompartment::setGCLastBytes(size_t lastBytes, JSGCInvocationKind gckind)
{
    gcLastBytes = lastBytes;

    size_t base = gckind == GC_SHRINK ? lastBytes : Max(lastBytes, GC_ARENA_ALLOCATION_TRIGGER);
    float trigger = float(base) * GC_HEAP_GROWTH_FACTOR;
    gcTriggerBytes = size_t(Min(float(rt->gcMaxBytes), trigger));
}

void
JSCompartment::reduceGCTriggerBytes(uint32 amount) {
    JS_ASSERT(amount > 0);
    JS_ASSERT(gcTriggerBytes - amount >= 0);
    if (gcTriggerBytes - amount < GC_ARENA_ALLOCATION_TRIGGER * GC_HEAP_GROWTH_FACTOR)
        return;
    gcTriggerBytes -= amount;
}

namespace js {
namespace gc {

inline ArenaHeader *
ArenaList::searchForFreeArena()
{
    while (ArenaHeader *aheader = *cursor) {
        cursor = &aheader->next;
        if (aheader->hasFreeThings())
            return aheader;
    }
    return NULL;
}

template <size_t thingSize>
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
        GCREASON(CHUNK);
        TriggerGC(cx->runtime);
        return NULL;
    }

    /*
     * While we still hold the GC lock get the arena from the chunk and add it
     * to the head of the list before the cursor to prevent checking the arena
     * for the free things.
     */
    ArenaHeader *aheader = chunk->allocateArena<thingSize>(cx, thingKind);
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
    FinalizeArenas<T>(cx, &head);
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
        JS_ASSERT_IF(!head, cursor == &head);
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
#ifdef JS_THREADSAFE
    Maybe<AutoUnlockAtomsCompartment> maybeUnlockAtomsCompartment;
    if (cx->compartment == rt->atomsCompartment && rt->atomsCompartmentIsLocked)
        maybeUnlockAtomsCompartment.construct(cx);
#endif
    /* The last ditch GC preserves all atoms. */
    AutoKeepAtoms keep(rt);
    GCREASON(LASTDITCH);
    js_GC(cx, rt->gcTriggerCompartment, GC_NORMAL);

#ifdef JS_THREADSAFE
    if (rt->gcBytes >= rt->gcMaxBytes)
        cx->runtime->gcHelperThread.waitBackgroundSweepEnd(cx->runtime);
#endif

    return rt->gcBytes < rt->gcMaxBytes;
}

static inline bool
IsGCAllowed(JSContext *cx)
{
    return !JS_ON_TRACE(cx) && !JS_THREAD_DATA(cx)->waiveGCQuota;
}

template <typename T>
inline void *
RefillTypedFreeList(JSContext *cx, unsigned thingKind)
{
    JS_ASSERT(!cx->runtime->gcRunning);

    /*
     * For compatibility with older code we tolerate calling the allocator
     * during the GC in optimized builds.
     */
    if (cx->runtime->gcRunning)
        return NULL;

    JSCompartment *compartment = cx->compartment;
    JS_ASSERT(compartment->freeLists.lists[thingKind].isEmpty());

    bool canGC = IsGCAllowed(cx);
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
            if (void *thing = compartment->freeLists.getNext(thingKind, sizeof(T)))
                return thing;
        }
        ArenaHeader *aheader =
            compartment->arenas[thingKind].getArenaWithFreeList<sizeof(T)>(cx, thingKind);
        if (aheader) {
            JS_ASSERT(sizeof(T) == aheader->getThingSize());
            return compartment->freeLists.populate(aheader, thingKind, sizeof(T));
        }

        /*
         * We failed to allocate any arena. Run the GC if we can unless we
         * have done it already.
         */
        if (!canGC || runGC)
            break;
        runGC = true;
    }

    js_ReportOutOfMemory(cx);
    return NULL;
}

void *
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
      case FINALIZE_TYPE_OBJECT:
        return RefillTypedFreeList<types::TypeObject>(cx, thingKind);
#if JS_HAS_XML_SUPPORT
      case FINALIZE_XML:
        return RefillTypedFreeList<JSXML>(cx, thingKind);
#endif
      case FINALIZE_IONCODE:
        return RefillTypedFreeList<ion::IonCode>(cx, thingKind);
      default:
        JS_NOT_REACHED("bad finalize kind");
        return 0;
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
  : color(0),
    unmarkedArenaStackTop(MarkingDelay::stackBottom()),
    objStack(cx->runtime->gcMarkStackObjs, sizeof(cx->runtime->gcMarkStackObjs)),
    ropeStack(cx->runtime->gcMarkStackRopes, sizeof(cx->runtime->gcMarkStackRopes)),
    xmlStack(cx->runtime->gcMarkStackXMLs, sizeof(cx->runtime->gcMarkStackXMLs)),
    largeStack(cx->runtime->gcMarkStackLarges, sizeof(cx->runtime->gcMarkStackLarges)),
    ionCodeStack(cx->runtime->gcMarkStackIonCode, sizeof(cx->runtime->gcMarkStackIonCode))
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
#ifdef DEBUG
    markLaterArenas++;
#endif
}

static void
MarkDelayedChildren(JSTracer *trc, ArenaHeader *aheader)
{
    unsigned traceKind = GetFinalizableTraceKind(aheader->getThingKind());
    size_t thingSize = aheader->getThingSize();
    Arena *a = aheader->getArena();
    uintptr_t end = a->thingsEnd();
    for (uintptr_t thing = a->thingsStart(thingSize); thing != end; thing += thingSize) {
        Cell *t = reinterpret_cast<Cell *>(thing);
        if (t->isMarked())
            JS_TraceChildren(trc, t, traceKind);
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
        MarkDelayedChildren(this, aheader);
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

    if (ptr && !trc->context->runtime->gcCurrentCompartment) {
        if (!JSAtom::isStatic(ptr)) {
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
    js_TraceScript(trc, fp->script(), NULL);
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
            js_TraceScript(trc, script, NULL);
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

      case TYPE: {
        types::TypeObject *type = static_cast<types::AutoTypeRooter *>(this)->type;
        if (!type->isMarked())
            type->trace(trc);
        return;
      }

      case VALARRAY: {
        AutoValueArray *array = static_cast<AutoValueArray *>(this);
        MarkValueRange(trc, array->length(), array->start(), "js::AutoValueArray");
        return;
      }

      case IONMASM: {
#ifdef JS_ION
        static_cast<js::ion::MacroAssembler::AutoRooter *>(this)->masm()->trace(trc);
#endif
        break;
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

#define PER_COMPARTMENT_OP(rt, op)                                      \
    if ((rt)->gcCurrentCompartment) {                                   \
        JSCompartment *c = (rt)->gcCurrentCompartment;                  \
        op;                                                             \
    } else {                                                            \
        for (JSCompartment **i = rt->compartments.begin(); i != rt->compartments.end(); ++i) { \
            JSCompartment *c = *i;                                      \
            op;                                                         \
        }                                                               \
    }

JS_REQUIRES_STACK void
MarkRuntime(JSTracer *trc, JSCompartment *comp)
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

    PER_COMPARTMENT_OP(rt, if (c->activeAnalysis) c->markTypes(trc));

#ifdef JS_TRACER
    PER_COMPARTMENT_OP(rt, if (c->hasTraceMonitor()) c->traceMonitor()->mark(trc));
#endif

    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->mark(trc);

    if (comp) {
        comp->mark(trc);
    } else {
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            (*c)->mark(trc);
    }

    /*
     * We mark extra roots at the last thing so it can use use additional
     * colors to implement cycle collection.
     */
    if (rt->gcExtraRootsTraceOp)
        rt->gcExtraRootsTraceOp(trc, rt->gcExtraRootsData);
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
    GCREASON(COMPARTMENT);

    if (rt->gcZeal()) {
        TriggerGC(rt);
        return;
    }

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

    if (rt->gcZeal()) {
        GCREASON(MAYBEGC);
        js_GC(cx, NULL, GC_NORMAL);
        return;
    }

    JSCompartment *comp = cx->compartment;
    if (rt->gcIsNeeded) {
        GCREASON(MAYBEGC);
        js_GC(cx, (comp == rt->gcTriggerCompartment) ? comp : NULL, GC_NORMAL);
        return;
    }

    if (comp->gcBytes > 8192 && comp->gcBytes >= 3 * (comp->gcTriggerBytes / 4)) {
        GCREASON(MAYBEGC);
        js_GC(cx, (rt->gcMode == JSGC_MODE_COMPARTMENT) ? comp : NULL, GC_NORMAL);
        return;
    }

    /*
     * On 32 bit setting gcNextFullGCTime below is not atomic and a race condition
     * could trigger an GC. We tolerate this.
     */
    int64 now = PRMJ_Now();
    if (rt->gcNextFullGCTime && rt->gcNextFullGCTime <= now) {
        if (rt->gcChunkAllocationSinceLastGC || rt->gcChunksWaitingToExpire) {
            GCREASON(MAYBEGC);
            js_GC(cx, NULL, GC_SHRINK);
        } else {
            rt->gcNextFullGCTime = now + GC_IDLE_FULL_SPAN;
        }
    }
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
    arenas[FINALIZE_TYPE_OBJECT].finalizeNow<types::TypeObject>(cx);
    arenas[FINALIZE_SHAPE].finalizeNow<Shape>(cx);
}

void
JSCompartment::finalizeIonCodeArenaLists(JSContext *cx)
{
    arenas[FINALIZE_IONCODE].finalizeNow<ion::IonCode>(cx);
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
GCHelperThread::startBackgroundSweep(JSRuntime *rt, JSGCInvocationKind gckind)
{
    /* The caller takes the GC lock. */
    JS_ASSERT(!sweeping);
    lastGCKind = gckind;
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
    ExpireGCChunks(cx->runtime, lastGCKind);
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
            compartment->freeLists.checkEmpty();
            Probes::GCEndSweepPhase(compartment);
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

    /*
     * Reset the property cache's type id generator so we can compress ids.
     * Same for the protoHazardShape proxy-shape standing in for all object
     * prototypes having readonly or setter properties.
     */
    if (rt->shapeGen & SHAPE_OVERFLOW_BIT || (rt->gcZeal() && !rt->gcCurrentCompartment)) {
        rt->gcRegenShapes = true;
        rt->shapeGen = 0;
        rt->protoHazardShape = 0;
    }

    PER_COMPARTMENT_OP(rt, c->purge(cx));

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
    GCTIMESTAMP(startMark);
    GCMarker gcmarker(cx);
    JS_ASSERT(IS_GC_MARKING_TRACER(&gcmarker));
    JS_ASSERT(gcmarker.getMarkColor() == BLACK);
    rt->gcMarkingTracer = &gcmarker;

    for (GCChunkSet::Range r(rt->gcUserChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    for (GCChunkSet::Range r(rt->gcSystemChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    if (comp) {
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            (*c)->markCrossCompartmentWrappers(&gcmarker);
    }

    MarkRuntime(&gcmarker, comp);

    gcmarker.drainMarkStack();

    /*
     * Mark weak roots.
     */
    while (true) {
        if (!WatchpointMap::markAllIteratively(&gcmarker) && !WeakMapBase::markAllIteratively(&gcmarker))
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
    GCTIMESTAMP(startSweep);

    /* Finalize unreachable (key,value) pairs in all weak maps. */
    WeakMapBase::sweepAll(&gcmarker);

    js_SweepAtomState(cx);

    /* Collect watch points associated with unreachable objects. */
    WatchpointMap::sweepAll(rt);

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
        GCTIMESTAMP(sweepObjectEnd);
        comp->finalizeStringArenaLists(cx);
        GCTIMESTAMP(sweepStringEnd);
        comp->finalizeIonCodeArenaLists(cx);
        GCTIMESTAMP(sweepIonCodeEnd);
        comp->finalizeShapeArenaLists(cx);
        GCTIMESTAMP(sweepShapeEnd);
    } else {
        /*
         * Some sweeping is not compartment-specific. Start a NULL-compartment
         * phase to demarcate all of that. (The compartment sweeps will nest
         * within.)
         */
        Probes::GCStartSweepPhase(NULL);
        SweepCrossCompartmentWrappers(cx);
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
            Probes::GCStartSweepPhase(*c);
            (*c)->finalizeObjectArenaLists(cx);
        }

        GCTIMESTAMP(sweepObjectEnd);

        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++)
            (*c)->finalizeStringArenaLists(cx);

        GCTIMESTAMP(sweepStringEnd);

        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++)
            (*c)->finalizeIonCodeArenaLists(cx);

        GCTIMESTAMP(sweepIonCodeEnd);

        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
            (*c)->finalizeShapeArenaLists(cx);
            Probes::GCEndSweepPhase(*c);
        }

        GCTIMESTAMP(sweepShapeEnd);
    }

#ifdef DEBUG
     PropertyTree::dumpShapes(cx);
#endif

    /*
     * Sweep script filenames after sweeping functions in the generic loop
     * above. In this way when a scripted function's finalizer destroys the
     * script and calls rt->destroyScriptHook, the hook can still access the
     * script's filename. See bug 323267.
     */
    PER_COMPARTMENT_OP(rt, js_SweepScriptFilenames(c));

    if (!comp) {
        SweepCompartments(cx, gckind);

        /* non-compartmental sweep pieces */
        Probes::GCEndSweepPhase(NULL);
    }

#ifndef JS_THREADSAFE
    /*
     * Destroy arenas after we finished the sweeping so finalizers can safely
     * use IsAboutToBeFinalized().
     * This is done on the GCHelperThread if JS_THREADSAFE is defined.
     */
    ExpireGCChunks(rt, gckind);
#endif
    GCTIMESTAMP(sweepDestroyEnd);

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
#ifdef DEBUG_srcnotesize
  { extern void DumpSrcNoteSizeHist();
    DumpSrcNoteSizeHist();
    printf("GC HEAP SIZE %lu\n", (unsigned long)rt->gcBytes);
  }
#endif

    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++)
        js::CheckCompartmentScripts(*c);
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
        rt->gcHelperThread.startBackgroundSweep(rt, gckind);
    } else {
        JS_ASSERT(!cx->gcBackgroundFree);
    }
#endif

    rt->gcMarkAndSweep = false;
    rt->gcRegenShapes = false;
    rt->setGCLastBytes(rt->gcBytes, gckind);
    rt->gcCurrentCompartment = NULL;
    rt->gcWeakMapList = NULL;

    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
        (*c)->setGCLastBytes((*c)->gcBytes, gckind);
}

struct GCCrashData
{
    int isRegen;
    int isCompartment;
};

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

    if (JS_ON_TRACE(cx)) {
        JS_ASSERT(gckind != GC_LAST_CONTEXT);
        return;
    }

    RecordNativeStackTopForGC(cx);

    GCCrashData crashData;
    crashData.isRegen = rt->shapeGen & SHAPE_OVERFLOW_BIT;
    crashData.isCompartment = !!comp;
    js_SaveCrashData(crash::JS_CRASH_TAG_GC, &crashData, sizeof(crashData));

    GCTIMER_BEGIN(rt, comp);

    struct AutoGCProbe {
        JSCompartment *comp;
        AutoGCProbe(JSCompartment *comp) : comp(comp) {
            Probes::GCStart(comp);
        }
        ~AutoGCProbe() {
            Probes::GCEnd(comp); /* background thread may still be sweeping */
        }
    } autoGCProbe(comp);

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

    rt->gcNextFullGCTime = PRMJ_Now() + GC_IDLE_FULL_SPAN;

    rt->gcChunkAllocationSinceLastGC = false;
    GCTIMER_END(gckind == GC_LAST_CONTEXT);

    js_SnapshotGCStack();
}

namespace js {

class AutoCopyFreeListToArenas {
    JSRuntime *rt;

  public:
    AutoCopyFreeListToArenas(JSRuntime *rt)
      : rt(rt) {
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            (*c)->freeLists.copyToArenas();
    }

    ~AutoCopyFreeListToArenas() {
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            (*c)->freeLists.clearInArenas();
    }
};

void
TraceRuntime(JSTracer *trc)
{
    LeaveTrace(trc->context);

#ifdef JS_THREADSAFE
    {
        JSContext *cx = trc->context;
        JSRuntime *rt = cx->runtime;
        if (rt->gcThread != cx->thread()) {
            AutoLockGC lock(rt);
            AutoGCSession gcsession(cx);

            rt->gcHelperThread.waitBackgroundSweepEnd(rt, false);
            AutoUnlockGC unlock(rt);

            AutoCopyFreeListToArenas copy(rt);
            RecordNativeStackTopForGC(trc->context);
            MarkRuntime(trc, NULL);
            return;
        }
    }
#else
    AutoCopyFreeListToArenas copy(trc->context->runtime);
    RecordNativeStackTopForGC(trc->context);
#endif

    /*
     * Calls from inside a normal GC or a recursive calls are OK and do not
     * require session setup.
     */
    MarkRuntime(trc, NULL);
}

void
IterateCompartmentsArenasCells(JSContext *cx, void *data,
                               IterateCompartmentCallback compartmentCallback,
                               IterateArenaCallback arenaCallback,
                               IterateCellCallback cellCallback)
{
    CHECK_REQUEST(cx);
    LeaveTrace(cx);

    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd(rt, false);
#endif
    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c) {
        JSCompartment *compartment = *c;
        (*compartmentCallback)(cx, data, compartment);

        for (unsigned thingKind = 0; thingKind < FINALIZE_LIMIT; thingKind++) {
            size_t traceKind = GetFinalizableTraceKind(thingKind);
            size_t thingSize = GCThingSizeMap[thingKind];
            ArenaHeader *aheader = compartment->arenas[thingKind].getHead();

            for (; aheader; aheader = aheader->next) {
                Arena *arena = aheader->getArena();
                (*arenaCallback)(cx, data, arena, traceKind, thingSize);
                FreeSpan firstSpan(aheader->getFirstFreeSpan());
                const FreeSpan *span = &firstSpan;

                for (uintptr_t thing = arena->thingsStart(thingSize); ; thing += thingSize) {
                    JS_ASSERT(thing <= arena->thingsEnd());
                    if (thing == span->first) {
                        if (!span->hasNext())
                            break;
                        thing = span->last;
                        span = span->nextSpan();
                    } else {
                        void *t = reinterpret_cast<void *>(thing);
                        (*cellCallback)(cx, data, t, traceKind, thingSize);
                    }
                }
            }
        }
    }
}

void
IterateCells(JSContext *cx, JSCompartment *compartment, FinalizeKind thingKind,
             void *data, IterateCellCallback cellCallback)
{
    /* :XXX: Any way to common this preamble with IterateCompartmentsArenasCells? */
    CHECK_REQUEST(cx);

    LeaveTrace(cx);

    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd(rt, false);
#endif
    AutoUnlockGC unlock(rt);

    AutoCopyFreeListToArenas copy(rt);

    size_t traceKind = GetFinalizableTraceKind(thingKind);
    size_t thingSize = GCThingSizeMap[thingKind];
    ArenaHeader *aheader = compartment->arenas[thingKind].getHead();

    for (; aheader; aheader = aheader->next) {
        Arena *arena = aheader->getArena();
        FreeSpan firstSpan(aheader->getFirstFreeSpan());
        const FreeSpan *span = &firstSpan;

        for (uintptr_t thing = arena->thingsStart(thingSize); ; thing += thingSize) {
            JS_ASSERT(thing <= arena->thingsEnd());
            if (thing == span->first) {
                if (!span->hasNext())
                    break;
                thing = span->last;
                span = span->nextSpan();
            } else {
                (*cellCallback)(cx, data, reinterpret_cast<void *>(thing), traceKind,
                                thingSize);
            }
        }
    }
}

namespace gc {

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals)
{
    JSRuntime *rt = cx->runtime;
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

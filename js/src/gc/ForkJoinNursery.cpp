/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_FJGENERATIONAL

#include "gc/ForkJoinNursery-inl.h"

#include "mozilla/IntegerPrintfMacros.h"

#include "prmjtime.h"

#include "gc/Heap.h"
#include "jit/IonFrames.h"
#include "jit/RematerializedFrame.h"
#include "vm/ArrayObject.h"
#include "vm/ForkJoin.h"

#include "jsgcinlines.h"
#include "gc/Nursery-inl.h"
#include "vm/ObjectImpl-inl.h"

// The ForkJoinNursery provides an object nursery for movable object
// types for one ForkJoin worker thread.  There is a one-to-one
// correspondence between ForkJoinNursery and ForkJoinContext.
//
// For a general overview of how the ForkJoinNursery fits into the
// overall PJS system, see the comment block in vm/ForkJoin.h.
//
//
// Invariants on the ForkJoinNursery:
//
// Let "the tenured area" from the point of view of one
// ForkJoinNursery comprise the global tenured area and the nursery's
// owning worker's private tenured area.  Then:
//
// - There can be pointers from the tenured area into a ForkJoinNursery,
//   and from the ForkJoinNursery into the tenured area
//
// - There *cannot* be a pointer from one ForkJoinNursery into
//   another, or from one private tenured area into another, or from a
//   ForkJoinNursery into another worker's private tenured are or vice
//   versa, or from any ForkJoinNursery or private tenured area into
//   the normal Nursery.
//
// For those invariants to hold the normal Nursery must be empty before
// a ForkJoin section.
//
//
// General description:
//
// The nursery maintains a space into which small, movable objects
// are allocated.  Other objects are allocated directly in the private
// tenured area for the worker.
//
// If an allocation request can't be satisfied because the nursery is
// full then a /minor collection/ is triggered without bailouts.  This
// collection copies nursery-allocated objects reachable from the
// worker's roots into a fresh space.  Then the old space is
// discarded.
//
// Nurseries are maintained in 1MB chunks.  If the live data in a
// nursery after a collection exceeds some set fraction (currently
// 1/3) then the nursery is grown, independently of other nurseries.
//
// There is an upper limit on the number of chunks in a nursery.  If
// the live data in a nursery after a collection exceeds the set
// fraction and the nursery can't grow, then the next collection will
// be an /evacuating collection/.
//
// An evacuating collection copies nursery-allocated objects reachable
// from the worker's roots into the worker's private tenured area.
//
// If an allocation request in the tenured area - whether the request
// comes from the mutator or from the garbage collector during
// evacuation - can't be satisified because the tenured area is full,
// then the worker bails out and triggers a full collection in the
// ForkJoin worker's zone.  This is expected to happen very rarely in
// practice.
//
// The roots for a collection in the ForkJoinNursery are: the frames
// of the execution stack, any registered roots on the execution
// stack, any objects in the private tenured area, and the ForkJoin
// result object in the common tenured area.
//
// The entire private tenured area is considered to be rooted in order
// not to have to run write barriers during the ForkJoin section.
// During a minor or evacuating collection in a worker the GC will
// step through the worker's tenured area, examining each object for
// pointers into the nursery.
//
// The ForkJoinNursery contains its own object tracing machinery for
// most of the types that can be allocated in the nursery.  But it
// does not handle all types, and there are two places where the code
// in ForkJoinNursery loses control of the tracing:
//
// - When calling clasp->trace() in traceObject()
// - When calling MarkForkJoinStack() in forwardFromStack()
//
// In both cases:
//
// - We pass a ForkJoinNurseryCollectionTracer object with a callback
//   to ForkJoinNursery::MinorGCCallback
//
// - We should only ever end up in MarkInternal() in Marking.cpp, in
//   the case in that code that calls back to trc->callback.  We
//   should /never/ end up in functions that trigger use of the mark
//   stack internal to the general GC's marker.
//
// - Any function along the path to MarkInternal() that asks about
//   whether something is in the nursery or is tenured /must/ be aware
//   that there can be multiple nursery and tenured areas; assertions
//   get this wrong a lot of the time and must be fixed when they do.
//   In practice, such code either must have a case for each nursery
//   kind or must use the IsInsideNursery(Cell*) method, which looks
//   only at the chunk tag.
//
//
// Terminological note:
//
// - While the mutator is running it is allocating in what's known as
//   the nursery's "newspace".  The mutator may also allocate directly
//   in the tenured space, but the tenured space is not part of the
//   newspace.
//
// - While the gc is running, the previous "newspace" has been renamed
//   as the gc's "fromspace", and the space that objects are copied
//   into is known as the "tospace".  The tospace may be a nursery
//   space (during a minor collection), or it may be a tenured space
//   (during an evacuation collection), but it's always one or the
//   other, never a combination.  After gc the fromspace is always
//   discarded.
//
// - If the gc copies objects into a nursery tospace then this tospace
//   becomes known as the "newspace" following gc.  Otherwise, a new
//   newspace won't be needed (if the parallel section is finished) or
//   can be created empty (if the gc just needed to evacuate).
//

namespace js {
namespace gc {

ForkJoinNursery::ForkJoinNursery(ForkJoinContext *cx, ForkJoinGCShared *shared, Allocator *tenured)
  : cx_(cx)
  , tenured_(tenured)
  , shared_(shared)
  , evacuationZone_(nullptr)
  , currentStart_(0)
  , currentEnd_(0)
  , position_(0)
  , currentChunk_(0)
  , numActiveChunks_(0)
  , numFromspaceChunks_(0)
  , mustEvacuate_(false)
  , isEvacuating_(false)
  , movedSize_(0)
  , head_(nullptr)
  , tail_(&head_)
  , hugeSlotsNew(0)
  , hugeSlotsFrom(1)
{
    for ( size_t i=0 ; i < MaxNurseryChunks ; i++ ) {
        newspace[i] = nullptr;
        fromspace[i] = nullptr;
    }
}

ForkJoinNursery::~ForkJoinNursery()
{
    for ( size_t i=0 ; i < numActiveChunks_ ; i++ ) {
        if (newspace[i])
            shared_->freeNurseryChunk(newspace[i]);
    }
}

bool
ForkJoinNursery::initialize()
{
    if (!hugeSlots[hugeSlotsNew].init() || !hugeSlots[hugeSlotsFrom].init())
        return false;
    if (!initNewspace())
        return false;
    return true;
}

void
ForkJoinNursery::minorGC()
{
    if (mustEvacuate_) {
        mustEvacuate_ = false;
        pjsCollection(Evacuate|Recreate);
    } else {
        pjsCollection(Collect|Recreate);
    }
}

void
ForkJoinNursery::evacuatingGC()
{
    pjsCollection(Evacuate);
}

#define TIME_START(name) int64_t timstampStart_##name = PRMJ_Now()
#define TIME_END(name) int64_t timstampEnd_##name = PRMJ_Now()
#define TIME_TOTAL(name) (timstampEnd_##name - timstampStart_##name)

void
ForkJoinNursery::pjsCollection(int op)
{
    JS_ASSERT((op & Collect) != (op & Evacuate));

    bool evacuate = op & Evacuate;
    bool recreate = op & Recreate;

    JS_ASSERT(!isEvacuating_);
    JS_ASSERT(!evacuationZone_);
    JS_ASSERT(!head_);
    JS_ASSERT(tail_ == &head_);

    JSRuntime *const rt = shared_->runtime();
    const unsigned currentNumActiveChunks_ = numActiveChunks_;
    const char *msg = "";

    JS_ASSERT(!rt->needsIncrementalBarrier());

    TIME_START(pjsCollection);

    rt->gc.incFJMinorCollecting();
    if (evacuate) {
        isEvacuating_ = true;
        evacuationZone_ = shared_->zone();
    }

    flip();
    if (recreate) {
        if (!initNewspace())
            CrashAtUnhandlableOOM("Cannot expand PJS nursery during GC");
        // newspace must be at least as large as fromSpace
        numActiveChunks_ = currentNumActiveChunks_;
    }
    ForkJoinNurseryCollectionTracer trc(rt, this);
    forwardFromRoots(&trc);
    collectToFixedPoint(&trc);
    jit::UpdateJitActivationsForMinorGC<ForkJoinNursery>(TlsPerThreadData.get(), &trc);
    freeFromspace();

    size_t live = movedSize_;
    computeNurserySizeAfterGC(live, &msg);

    sweepHugeSlots();
    JS_ASSERT(hugeSlots[hugeSlotsFrom].empty());
    JS_ASSERT_IF(isEvacuating_, hugeSlots[hugeSlotsNew].empty());

    isEvacuating_ = false;
    evacuationZone_ = nullptr;
    head_ = nullptr;
    tail_ = &head_;
    movedSize_ = 0;

    rt->gc.decFJMinorCollecting();

    TIME_END(pjsCollection);

    // Note, the spew is awk-friendly, non-underlined words serve as markers:
    //   FJGC _tag_ us _value_ copied _value_ size _value_ _message-word_ ...
    shared_->spewGC("FJGC %s us %5" PRId64 "  copied %7" PRIu64 "  size %" PRIu64 "  %s",
                    (evacuate ? "evacuate " : "collect  "),
                    TIME_TOTAL(pjsCollection),
                    (uint64_t)live,
                    (uint64_t)numActiveChunks_*1024*1024,
                    msg);
}

#undef TIME_START
#undef TIME_END
#undef TIME_TOTAL

void
ForkJoinNursery::computeNurserySizeAfterGC(size_t live, const char **msg)
{
    // Grow the nursery if it is too full.  Do not bother to shrink it - lazy
    // chunk allocation means that a too-large nursery will not really be a problem,
    // the entire nursery will be deallocated soon anyway.
    if (live * NurseryLoadFactor > numActiveChunks_ * ForkJoinNurseryChunk::UsableSize) {
        if (numActiveChunks_ < MaxNurseryChunks) {
            while (numActiveChunks_ < MaxNurseryChunks &&
                   live * NurseryLoadFactor > numActiveChunks_ * ForkJoinNurseryChunk::UsableSize)
            {
                ++numActiveChunks_;
            }
        } else {
            // Evacuation will tend to drive us toward the cliff of a bailout GC, which
            // is not good, probably worse than working within the thread at a higher load
            // than desirable.
            //
            // Thus it's possible to be more sophisticated than this:
            //
            // - evacuate only after several minor GCs in a row exceeded the set load
            // - evacuate only if significantly less space than required is available, eg,
            //   if only 1/2 the required free space is available
            *msg = "  Overfull, will evacuate next";
            mustEvacuate_ = true;
        }
    }
}

void
ForkJoinNursery::flip()
{
    size_t i;
    for (i=0; i < numActiveChunks_; i++) {
        if (!newspace[i])
            break;
        fromspace[i] = newspace[i];
        newspace[i] = nullptr;
        fromspace[i]->trailer.location = gc::ChunkLocationBitPJSFromspace;
    }
    numFromspaceChunks_ = i;
    numActiveChunks_ = 0;

    int tmp = hugeSlotsNew;
    hugeSlotsNew = hugeSlotsFrom;
    hugeSlotsFrom = tmp;

    JS_ASSERT(hugeSlots[hugeSlotsNew].empty());
}

void
ForkJoinNursery::freeFromspace()
{
    for (size_t i=0; i < numFromspaceChunks_; i++) {
        shared_->freeNurseryChunk(fromspace[i]);
        fromspace[i] = nullptr;
    }
    numFromspaceChunks_ = 0;
}

bool
ForkJoinNursery::initNewspace()
{
    JS_ASSERT(newspace[0] == nullptr);
    JS_ASSERT(numActiveChunks_ == 0);

    numActiveChunks_ = 1;
    return setCurrentChunk(0);
}

MOZ_ALWAYS_INLINE bool
ForkJoinNursery::shouldMoveObject(void **thingp)
{
    // Note that thingp must really be a T** where T is some GCThing,
    // ie, something that lives in a chunk (or nullptr).  This should
    // be the case because the MinorGCCallback is only called on exact
    // roots on the stack or slots within in tenured objects and not
    // on slot/element arrays that can be malloc'd; they are forwarded
    // using the forwardBufferPointer() mechanism.
    //
    // The main reason for that restriction is so that we can call a
    // method here that can check the chunk trailer for the cell (a
    // future optimization).
    Cell *cell = static_cast<Cell *>(*thingp);
    return isInsideFromspace(cell) && !getForwardedPointer(thingp);
}

/* static */ void
ForkJoinNursery::MinorGCCallback(JSTracer *trcArg, void **thingp, JSGCTraceKind traceKind)
{
    // traceKind can be all sorts of things, when we're marking from stack roots
    ForkJoinNursery *nursery = static_cast<ForkJoinNurseryCollectionTracer *>(trcArg)->nursery_;
    if (nursery->shouldMoveObject(thingp)) {
        // When other types of objects become nursery-allocable then the static_cast
        // to JSObject * will no longer be valid.
        JS_ASSERT(traceKind == JSTRACE_OBJECT);
        *thingp = nursery->moveObjectToTospace(static_cast<JSObject *>(*thingp));
    }
}

void
ForkJoinNursery::forwardFromRoots(ForkJoinNurseryCollectionTracer *trc)
{
    // There should be no other roots as a result of effect-freedom.
    forwardFromUpdatable(trc);
    forwardFromStack(trc);
    forwardFromTenured(trc);
    forwardFromRematerializedFrames(trc);
}

void
ForkJoinNursery::forwardFromUpdatable(ForkJoinNurseryCollectionTracer *trc)
{
    JSObject *obj = shared_->updatable();
    if (obj)
        traceObject(trc, obj);
}

void
ForkJoinNursery::forwardFromStack(ForkJoinNurseryCollectionTracer *trc)
{
    MarkForkJoinStack(trc);
}

void
ForkJoinNursery::forwardFromTenured(ForkJoinNurseryCollectionTracer *trc)
{
    JSObject *objs[ArenaCellCount];
    ArenaLists &lists = tenured_->arenas;
    for (size_t k=0; k < FINALIZE_LIMIT; k++) {
        AllocKind kind = (AllocKind)k;
        if (!IsFJNurseryAllocable(kind))
            continue;

        // When non-JSObject types become nursery-allocable the assumptions in the
        // loops below will no longer hold; other types than JSObject must be
        // handled.
        JS_ASSERT(kind <= FINALIZE_OBJECT_LAST);

        // Clear the free list that we're currently allocating out of.
        lists.purge(kind);

        // Since we only purge once, there must not currently be any partially
        // full arenas left to allocate out of, or we would break out early.
        JS_ASSERT(!lists.getArenaAfterCursor(kind));

        ArenaIter ai;
        ai.init(const_cast<Allocator *>(tenured_), kind);
        for (; !ai.done(); ai.next()) {
            if (isEvacuating_ && lists.arenaIsInUse(ai.get(), kind))
                break;
            // Use ArenaCellIterUnderFinalize, not ...UnderGC, because that side-steps
            // some assertions in the latter that are wrong for PJS collection.
            size_t numObjs = 0;
            for (ArenaCellIterUnderFinalize i(ai.get()); !i.done(); i.next())
                objs[numObjs++] = i.get<JSObject>();
            for (size_t i=0; i < numObjs; i++)
                traceObject(trc, objs[i]);
        }
    }
}

void
ForkJoinNursery::forwardFromRematerializedFrames(ForkJoinNurseryCollectionTracer *trc)
{
    if (cx_->bailoutRecord->hasFrames())
        jit::RematerializedFrame::MarkInVector(trc, cx_->bailoutRecord->frames());
}

/*static*/ void
ForkJoinNursery::forwardBufferPointer(JSTracer *trc, HeapSlot **pSlotsElems)
{
    ForkJoinNursery *nursery = static_cast<ForkJoinNurseryCollectionTracer *>(trc)->nursery_;
    HeapSlot *old = *pSlotsElems;

    if (!nursery->isInsideFromspace(old))
        return;

    // If the elements buffer is zero length, the "first" item could be inside
    // of the next object or past the end of the allocable area.  However,
    // since we always store the runtime as the last word in a nursery chunk,
    // isInsideFromspace will still be true, even if this zero-size allocation
    // abuts the end of the allocable area. Thus, it is always safe to read the
    // first word of |old| here.
    *pSlotsElems = *reinterpret_cast<HeapSlot **>(old);
    JS_ASSERT(!nursery->isInsideFromspace(*pSlotsElems));
}

void
ForkJoinNursery::collectToFixedPoint(ForkJoinNurseryCollectionTracer *trc)
{
    for (RelocationOverlay *p = head_; p; p = p->next())
        traceObject(trc, static_cast<JSObject *>(p->forwardingAddress()));
}

inline bool
ForkJoinNursery::setCurrentChunk(int index)
{
    JS_ASSERT((size_t)index < numActiveChunks_);
    JS_ASSERT(!newspace[index]);

    currentChunk_ = index;
    ForkJoinNurseryChunk *c = shared_->allocateNurseryChunk();
    if (!c)
        return false;
    c->trailer.runtime = shared_->runtime();
    c->trailer.location = gc::ChunkLocationBitPJSNewspace;
    c->trailer.storeBuffer = nullptr;
    currentStart_ = c->start();
    currentEnd_ = c->end();
    position_ = currentStart_;
    newspace[index] = c;
    return true;
}

void *
ForkJoinNursery::allocate(size_t size)
{
    JS_ASSERT(position_ >= currentStart_);

    if (currentEnd_ - position_ < size) {
        if (currentChunk_ + 1 == numActiveChunks_)
            return nullptr;
        // Failure to allocate on growth is treated the same as exhaustion
        // of the nursery.  If this happens during normal execution then
        // we'll trigger a minor collection.  That collection will likely
        // fail to obtain a block for the new tospace, and we'll go OOM
        // immediately; that's expected and acceptable.  If we do continue
        // to run (because some other thread or process has freed the memory)
        // then so much the better.
        if (!setCurrentChunk(currentChunk_ + 1))
            return nullptr;
    }

    void *thing = reinterpret_cast<void *>(position_);
    position_ += size;

    JS_POISON(thing, JS_ALLOCATED_NURSERY_PATTERN, size);
    return thing;
}

JSObject *
ForkJoinNursery::allocateObject(size_t baseSize, size_t numDynamic, bool& tooLarge)
{
    // Ensure there's enough space to replace the contents with a RelocationOverlay.
    JS_ASSERT(baseSize >= sizeof(js::gc::RelocationOverlay));

    // Too-large slot arrays cannot be accomodated.
    if (numDynamic > MaxNurserySlots) {
        tooLarge = true;
        return nullptr;
    }

    // Allocate slots contiguously after the object.
    size_t totalSize = baseSize + sizeof(HeapSlot) * numDynamic;
    JSObject *obj = static_cast<JSObject *>(allocate(totalSize));
    if (!obj) {
        tooLarge = false;
        return nullptr;
    }
    obj->setInitialSlots(numDynamic
                         ? reinterpret_cast<HeapSlot *>(size_t(obj) + baseSize)
                         : nullptr);
    return obj;
}

HeapSlot *
ForkJoinNursery::allocateSlots(JSObject *obj, uint32_t nslots)
{
    JS_ASSERT(obj);
    JS_ASSERT(nslots > 0);

    if (nslots & mozilla::tl::MulOverflowMask<sizeof(HeapSlot)>::value)
        return nullptr;

    if (!isInsideNewspace(obj))
        return obj->zone()->pod_malloc<HeapSlot>(nslots);

    if (nslots > MaxNurserySlots)
        return allocateHugeSlots(obj, nslots);

    size_t size = nslots * sizeof(HeapSlot);
    HeapSlot *slots = static_cast<HeapSlot *>(allocate(size));
    if (slots)
        return slots;

    return allocateHugeSlots(obj, nslots);
}

HeapSlot *
ForkJoinNursery::reallocateSlots(JSObject *obj, HeapSlot *oldSlots,
                                 uint32_t oldCount, uint32_t newCount)
{
    if (newCount & mozilla::tl::MulOverflowMask<sizeof(HeapSlot)>::value)
        return nullptr;

    if (!isInsideNewspace(obj)) {
        JS_ASSERT_IF(oldSlots, !isInsideNewspace(oldSlots));
        return obj->zone()->pod_realloc<HeapSlot>(oldSlots, oldCount, newCount);
    }

    if (!isInsideNewspace(oldSlots))
        return reallocateHugeSlots(obj, oldSlots, oldCount, newCount);

    // No-op if we're shrinking, we can't make use of the freed portion.
    if (newCount < oldCount)
        return oldSlots;

    HeapSlot *newSlots = allocateSlots(obj, newCount);
    if (!newSlots)
        return nullptr;

    size_t oldSize = oldCount * sizeof(HeapSlot);
    js_memcpy(newSlots, oldSlots, oldSize);
    return newSlots;
}

ObjectElements *
ForkJoinNursery::allocateElements(JSObject *obj, uint32_t nelems)
{
    JS_ASSERT(nelems >= ObjectElements::VALUES_PER_HEADER);
    return reinterpret_cast<ObjectElements *>(allocateSlots(obj, nelems));
}

ObjectElements *
ForkJoinNursery::reallocateElements(JSObject *obj, ObjectElements *oldHeader,
                                    uint32_t oldCount, uint32_t newCount)
{
    HeapSlot *slots = reallocateSlots(obj, reinterpret_cast<HeapSlot *>(oldHeader),
                                      oldCount, newCount);
    return reinterpret_cast<ObjectElements *>(slots);
}

void
ForkJoinNursery::freeSlots(HeapSlot *slots)
{
    if (!isInsideNewspace(slots)) {
        hugeSlots[hugeSlotsNew].remove(slots);
        js_free(slots);
    }
}

HeapSlot *
ForkJoinNursery::allocateHugeSlots(JSObject *obj, size_t nslots)
{
    if (nslots & mozilla::tl::MulOverflowMask<sizeof(HeapSlot)>::value)
        return nullptr;

    HeapSlot *slots = obj->zone()->pod_malloc<HeapSlot>(nslots);
    if (!slots)
        return slots;

    // If this put fails, we will only leak the slots.
    (void)hugeSlots[hugeSlotsNew].put(slots);
    return slots;
}

HeapSlot *
ForkJoinNursery::reallocateHugeSlots(JSObject *obj, HeapSlot *oldSlots,
                                     uint32_t oldCount, uint32_t newCount)
{
    HeapSlot *newSlots = obj->zone()->pod_realloc<HeapSlot>(oldSlots, oldCount, newCount);
    if (!newSlots)
        return newSlots;

    if (oldSlots != newSlots) {
        hugeSlots[hugeSlotsNew].remove(oldSlots);
        // If this put fails, we will only leak the slots.
        (void)hugeSlots[hugeSlotsNew].put(newSlots);
    }
    return newSlots;
}

void
ForkJoinNursery::sweepHugeSlots()
{
    for (HugeSlotsSet::Range r = hugeSlots[hugeSlotsFrom].all(); !r.empty(); r.popFront())
        js_free(r.front());
    hugeSlots[hugeSlotsFrom].clear();
}

MOZ_ALWAYS_INLINE void
ForkJoinNursery::traceObject(ForkJoinNurseryCollectionTracer *trc, JSObject *obj)
{
    const Class *clasp = obj->getClass();
    if (clasp->trace)
        clasp->trace(trc, obj);

    if (!obj->isNative())
        return;

    if (!obj->hasEmptyElements())
        markSlots(obj->getDenseElements(), obj->getDenseInitializedLength());

    HeapSlot *fixedStart, *fixedEnd, *dynStart, *dynEnd;
    obj->getSlotRange(0, obj->slotSpan(), &fixedStart, &fixedEnd, &dynStart, &dynEnd);
    markSlots(fixedStart, fixedEnd);
    markSlots(dynStart, dynEnd);
}

MOZ_ALWAYS_INLINE void
ForkJoinNursery::markSlots(HeapSlot *vp, uint32_t nslots)
{
    markSlots(vp, vp + nslots);
}

MOZ_ALWAYS_INLINE void
ForkJoinNursery::markSlots(HeapSlot *vp, HeapSlot *end)
{
    for (; vp != end; ++vp)
        markSlot(vp);
}

MOZ_ALWAYS_INLINE void
ForkJoinNursery::markSlot(HeapSlot *slotp)
{
    if (!slotp->isObject())
        return;

    JSObject *obj = &slotp->toObject();
    if (!isInsideFromspace(obj))
        return;

    if (getForwardedPointer(&obj)) {
        slotp->unsafeGet()->setObject(*obj);
        return;
    }

    JSObject *moved = static_cast<JSObject *>(moveObjectToTospace(obj));
    slotp->unsafeGet()->setObject(*moved);
}

AllocKind
ForkJoinNursery::getObjectAllocKind(JSObject *obj)
{
    if (obj->is<ArrayObject>()) {
        JS_ASSERT(obj->numFixedSlots() == 0);

        // Use minimal size object if we are just going to copy the pointer.
        if (!isInsideFromspace((void *)obj->getElementsHeader()))
            return FINALIZE_OBJECT0_BACKGROUND;

        size_t nelements = obj->getDenseCapacity();
        return GetBackgroundAllocKind(GetGCArrayKind(nelements));
    }

    if (obj->is<JSFunction>())
        return obj->as<JSFunction>().getAllocKind();

    AllocKind kind = GetGCObjectFixedSlotsKind(obj->numFixedSlots());
    JS_ASSERT(!IsBackgroundFinalized(kind));
    JS_ASSERT(CanBeFinalizedInBackground(kind, obj->getClass()));
    return GetBackgroundAllocKind(kind);
}

// Nursery allocation will never fail during GC - apart from true OOM - since
// newspace is at least as large as fromspace, ergo a nullptr return from the
// allocator means true OOM, which we catch and signal here.
void *
ForkJoinNursery::allocateInTospaceInfallible(size_t thingSize)
{
    void *p = allocate(thingSize);
    if (!p)
        CrashAtUnhandlableOOM("Cannot expand PJS nursery during GC");
    return p;
}

void *
ForkJoinNursery::allocateInTospace(gc::AllocKind thingKind)
{
    size_t thingSize = Arena::thingSize(thingKind);
    if (isEvacuating_) {
        void *t = tenured_->arenas.allocateFromFreeList(thingKind, thingSize);
        if (t)
            return t;
        tenured_->arenas.checkEmptyFreeList(thingKind);
        // This call may return NULL but should do so only if memory
        // is truly exhausted.  However, allocateFromArena() can fail
        // either because memory is exhausted or if the allocation
        // budget is used up.  There is a guard in
        // Chunk::allocateArena() against the latter case.
        return tenured_->arenas.allocateFromArena(evacuationZone_, thingKind);
    }
    return allocateInTospaceInfallible(thingSize);
}

template <typename T>
T *
ForkJoinNursery::allocateInTospace(size_t nelem)
{
    if (isEvacuating_)
        return evacuationZone_->pod_malloc<T>(nelem);
    return static_cast<T *>(allocateInTospaceInfallible(nelem * sizeof(T)));
}

MOZ_ALWAYS_INLINE void
ForkJoinNursery::insertIntoFixupList(RelocationOverlay *entry)
{
    *tail_ = entry;
    tail_ = &entry->next_;
    *tail_ = nullptr;
}

void *
ForkJoinNursery::moveObjectToTospace(JSObject *src)
{
    AllocKind dstKind = getObjectAllocKind(src);
    JSObject *dst = static_cast<JSObject *>(allocateInTospace(dstKind));
    if (!dst)
        CrashAtUnhandlableOOM("Failed to allocate object while moving object.");

    movedSize_ += copyObjectToTospace(dst, src, dstKind);

    RelocationOverlay *overlay = reinterpret_cast<RelocationOverlay *>(src);
    overlay->forwardTo(dst);
    insertIntoFixupList(overlay);

    return static_cast<void *>(dst);
}

size_t
ForkJoinNursery::copyObjectToTospace(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    size_t srcSize = Arena::thingSize(dstKind);
    size_t movedSize = srcSize;

    // Arrays do not necessarily have the same AllocKind between src and dst.
    // We deal with this by copying elements manually, possibly re-inlining
    // them if there is adequate room inline in dst.
    if (src->is<ArrayObject>())
        srcSize = movedSize = sizeof(ObjectImpl);

    js_memcpy(dst, src, srcSize);
    movedSize += copySlotsToTospace(dst, src, dstKind);
    movedSize += copyElementsToTospace(dst, src, dstKind);

    // The shape's list head may point into the old object.
    if (&src->shape_ == dst->shape_->listp) {
        JS_ASSERT(cx_->isThreadLocal(dst->shape_.get()));
        dst->shape_->listp = &dst->shape_;
    }

    return movedSize;
}

size_t
ForkJoinNursery::copySlotsToTospace(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    // Fixed slots have already been copied over.
    if (!src->hasDynamicSlots())
        return 0;

    if (!isInsideFromspace(src->slots)) {
        hugeSlots[hugeSlotsFrom].remove(src->slots);
        if (!isEvacuating_)
            hugeSlots[hugeSlotsNew].put(src->slots);
        return 0;
    }

    size_t count = src->numDynamicSlots();
    dst->slots = allocateInTospace<HeapSlot>(count);
    if (!dst->slots)
        CrashAtUnhandlableOOM("Failed to allocate slots while moving object.");
    js_memcpy(dst->slots, src->slots, count * sizeof(HeapSlot));
    setSlotsForwardingPointer(src->slots, dst->slots, count);
    return count * sizeof(HeapSlot);
}

size_t
ForkJoinNursery::copyElementsToTospace(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    if (src->hasEmptyElements() || src->denseElementsAreCopyOnWrite())
        return 0;

    ObjectElements *srcHeader = src->getElementsHeader();
    ObjectElements *dstHeader;

    // TODO Bug 874151: Prefer to put element data inline if we have space.
    // (Note, not a correctness issue.)
    if (!isInsideFromspace(srcHeader)) {
        JS_ASSERT(src->elements == dst->elements);
        hugeSlots[hugeSlotsFrom].remove(reinterpret_cast<HeapSlot*>(srcHeader));
        if (!isEvacuating_)
            hugeSlots[hugeSlotsNew].put(reinterpret_cast<HeapSlot*>(srcHeader));
        return 0;
    }

    size_t nslots = ObjectElements::VALUES_PER_HEADER + srcHeader->capacity;

    // Unlike other objects, Arrays can have fixed elements.
    if (src->is<ArrayObject>() && nslots <= GetGCKindSlots(dstKind)) {
        dst->setFixedElements();
        dstHeader = dst->getElementsHeader();
        js_memcpy(dstHeader, srcHeader, nslots * sizeof(HeapSlot));
        setElementsForwardingPointer(srcHeader, dstHeader, nslots);
        return nslots * sizeof(HeapSlot);
    }

    JS_ASSERT(nslots >= 2);
    dstHeader = reinterpret_cast<ObjectElements *>(allocateInTospace<HeapSlot>(nslots));
    if (!dstHeader)
        CrashAtUnhandlableOOM("Failed to allocate elements while moving object.");
    js_memcpy(dstHeader, srcHeader, nslots * sizeof(HeapSlot));
    setElementsForwardingPointer(srcHeader, dstHeader, nslots);
    dst->elements = dstHeader->elements();
    return nslots * sizeof(HeapSlot);
}

void
ForkJoinNursery::setSlotsForwardingPointer(HeapSlot *oldSlots, HeapSlot *newSlots, uint32_t nslots)
{
    JS_ASSERT(nslots > 0);
    JS_ASSERT(isInsideFromspace(oldSlots));
    JS_ASSERT(!isInsideFromspace(newSlots));
    *reinterpret_cast<HeapSlot **>(oldSlots) = newSlots;
}

void
ForkJoinNursery::setElementsForwardingPointer(ObjectElements *oldHeader, ObjectElements *newHeader,
                                             uint32_t nelems)
{
    // If the JIT has hoisted a zero length pointer, then we do not need to
    // relocate it because reads and writes to/from this pointer are invalid.
    if (nelems - ObjectElements::VALUES_PER_HEADER < 1)
        return;
    JS_ASSERT(isInsideFromspace(oldHeader));
    JS_ASSERT(!isInsideFromspace(newHeader));
    *reinterpret_cast<HeapSlot **>(oldHeader->elements()) = newHeader->elements();
}

ForkJoinNurseryCollectionTracer::ForkJoinNurseryCollectionTracer(JSRuntime *rt,
                                                                 ForkJoinNursery *nursery)
  : JSTracer(rt, ForkJoinNursery::MinorGCCallback, TraceWeakMapKeysValues)
  , nursery_(nursery)
{
    JS_ASSERT(rt);
    JS_ASSERT(nursery);
}

} // namespace gc
} // namespace js

#endif /* JSGC_FJGENERATIONAL */

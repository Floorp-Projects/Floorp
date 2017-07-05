/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Nursery-inl.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/Unused.h"

#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsutil.h"

#include "gc/GCInternals.h"
#include "gc/Memory.h"
#include "jit/JitFrames.h"
#include "vm/ArrayObject.h"
#include "vm/Debugger.h"
#if defined(DEBUG)
#include "vm/EnvironmentObject.h"
#endif
#include "vm/JSONPrinter.h"
#include "vm/Time.h"
#include "vm/TypedArrayObject.h"
#include "vm/TypeInference.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;
using namespace gc;

using mozilla::ArrayLength;
using mozilla::DebugOnly;
using mozilla::PodCopy;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

constexpr uintptr_t CanaryMagicValue = 0xDEADB15D;

struct js::Nursery::FreeMallocedBuffersTask : public GCParallelTask
{
    explicit FreeMallocedBuffersTask(FreeOp* fop) : GCParallelTask(fop->runtime()), fop_(fop) {}
    bool init() { return buffers_.init(); }
    void transferBuffersToFree(MallocedBuffersSet& buffersToFree,
                               const AutoLockHelperThreadState& lock);
    ~FreeMallocedBuffersTask() override { join(); }

  private:
    FreeOp* fop_;
    MallocedBuffersSet buffers_;

    virtual void run() override;
};

#ifdef JS_GC_ZEAL
struct js::Nursery::Canary
{
    uintptr_t magicValue;
    Canary* next;
};
#endif

inline void
js::Nursery::NurseryChunk::poisonAndInit(JSRuntime* rt, uint8_t poison)
{
    JS_POISON(this, poison, ChunkSize);
    init(rt);
}

inline void
js::Nursery::NurseryChunk::init(JSRuntime* rt)
{
    new (&trailer) gc::ChunkTrailer(rt, &rt->gc.storeBuffer());
}

/* static */ inline js::Nursery::NurseryChunk*
js::Nursery::NurseryChunk::fromChunk(Chunk* chunk)
{
    return reinterpret_cast<NurseryChunk*>(chunk);
}

inline Chunk*
js::Nursery::NurseryChunk::toChunk(JSRuntime* rt)
{
    auto chunk = reinterpret_cast<Chunk*>(this);
    chunk->init(rt);
    return chunk;
}

js::Nursery::Nursery(JSRuntime* rt)
  : runtime_(rt)
  , position_(0)
  , currentStartChunk_(0)
  , currentStartPosition_(0)
  , currentEnd_(0)
  , currentChunk_(0)
  , maxNurseryChunks_(0)
  , previousPromotionRate_(0)
  , profileThreshold_(0)
  , enableProfiling_(false)
  , reportTenurings_(0)
  , minorGCTriggerReason_(JS::gcreason::NO_REASON)
  , minorGcCount_(0)
  , freeMallocedBuffersTask(nullptr)
#ifdef JS_GC_ZEAL
  , lastCanary_(nullptr)
#endif
{}

bool
js::Nursery::init(uint32_t maxNurseryBytes, AutoLockGC& lock)
{
    if (!mallocedBuffers.init())
        return false;

    freeMallocedBuffersTask = js_new<FreeMallocedBuffersTask>(runtime()->defaultFreeOp());
    if (!freeMallocedBuffersTask || !freeMallocedBuffersTask->init())
        return false;

    /* maxNurseryBytes parameter is rounded down to a multiple of chunk size. */
    maxNurseryChunks_ = maxNurseryBytes >> ChunkShift;

    /* If no chunks are specified then the nursery is permanently disabled. */
    if (maxNurseryChunks_ == 0)
        return true;

    AutoMaybeStartBackgroundAllocation maybeBgAlloc;
    updateNumChunksLocked(1, maybeBgAlloc, lock);
    if (numChunks() == 0)
        return false;

    setCurrentChunk(0);
    setStartPosition();

    char* env = getenv("JS_GC_PROFILE_NURSERY");
    if (env) {
        if (0 == strcmp(env, "help")) {
            fprintf(stderr, "JS_GC_PROFILE_NURSERY=N\n"
                    "\tReport minor GC's taking at least N microseconds.\n");
            exit(0);
        }
        enableProfiling_ = true;
        profileThreshold_ = TimeDuration::FromMicroseconds(atoi(env));
    }

    env = getenv("JS_GC_REPORT_TENURING");
    if (env) {
        if (0 == strcmp(env, "help")) {
            fprintf(stderr, "JS_GC_REPORT_TENURING=N\n"
                    "\tAfter a minor GC, report any ObjectGroups with at least N instances tenured.\n");
            exit(0);
        }
        reportTenurings_ = atoi(env);
    }

    if (!runtime()->gc.storeBuffer().enable())
        return false;

    MOZ_ASSERT(isEnabled());
    return true;
}

js::Nursery::~Nursery()
{
    disable();
    js_delete(freeMallocedBuffersTask);
}

void
js::Nursery::enable()
{
    MOZ_ASSERT(isEmpty());
    MOZ_ASSERT(!runtime()->gc.isVerifyPreBarriersEnabled());
    if (isEnabled() || !maxChunks())
        return;

    updateNumChunks(1);
    if (numChunks() == 0)
        return;

    setCurrentChunk(0);
    setStartPosition();
#ifdef JS_GC_ZEAL
    if (runtime()->hasZealMode(ZealMode::GenerationalGC))
        enterZealMode();
#endif

    MOZ_ALWAYS_TRUE(runtime()->gc.storeBuffer().enable());
    return;
}

void
js::Nursery::disable()
{
    MOZ_ASSERT(isEmpty());
    if (!isEnabled())
        return;
    updateNumChunks(0);
    currentEnd_ = 0;
    runtime()->gc.storeBuffer().disable();
}

bool
js::Nursery::isEmpty() const
{
    if (!isEnabled())
        return true;

    if (!runtime()->hasZealMode(ZealMode::GenerationalGC)) {
        MOZ_ASSERT(currentStartChunk_ == 0);
        MOZ_ASSERT(currentStartPosition_ == chunk(0).start());
    }
    return position() == currentStartPosition_;
}

#ifdef JS_GC_ZEAL
void
js::Nursery::enterZealMode() {
    if (isEnabled())
        updateNumChunks(maxNurseryChunks_);
}

void
js::Nursery::leaveZealMode() {
    if (isEnabled()) {
        MOZ_ASSERT(isEmpty());
        setCurrentChunk(0);
        setStartPosition();
    }
}
#endif // JS_GC_ZEAL

JSObject*
js::Nursery::allocateObject(JSContext* cx, size_t size, size_t numDynamic, const js::Class* clasp)
{
    /* Ensure there's enough space to replace the contents with a RelocationOverlay. */
    MOZ_ASSERT(size >= sizeof(RelocationOverlay));

    /* Sanity check the finalizer. */
    MOZ_ASSERT_IF(clasp->hasFinalize(), CanNurseryAllocateFinalizedClass(clasp) ||
                                        clasp->isProxy());

    /* Make the object allocation. */
    JSObject* obj = static_cast<JSObject*>(allocate(size));
    if (!obj)
        return nullptr;

    /* If we want external slots, add them. */
    HeapSlot* slots = nullptr;
    if (numDynamic) {
        MOZ_ASSERT(clasp->isNative());
        slots = static_cast<HeapSlot*>(allocateBuffer(cx->zone(), numDynamic * sizeof(HeapSlot)));
        if (!slots) {
            /*
             * It is safe to leave the allocated object uninitialized, since we
             * do not visit unallocated things in the nursery.
             */
            return nullptr;
        }
    }

    /* Always initialize the slots field to match the JIT behavior. */
    obj->setInitialSlotsMaybeNonNative(slots);

    TraceNurseryAlloc(obj, size);
    return obj;
}

void*
js::Nursery::allocate(size_t size)
{
    MOZ_ASSERT(isEnabled());
    MOZ_ASSERT(!JS::CurrentThreadIsHeapBusy());
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
    MOZ_ASSERT_IF(currentChunk_ == currentStartChunk_, position() >= currentStartPosition_);
    MOZ_ASSERT(position() % CellAlignBytes == 0);
    MOZ_ASSERT(size % CellAlignBytes == 0);

#ifdef JS_GC_ZEAL
    static const size_t CanarySize = (sizeof(Nursery::Canary) + CellAlignBytes - 1) & ~CellAlignMask;
    if (runtime()->gc.hasZealMode(ZealMode::CheckNursery))
        size += CanarySize;
#endif

    if (currentEnd() < position() + size) {
        if (currentChunk_ + 1 == numChunks())
            return nullptr;
        setCurrentChunk(currentChunk_ + 1);
    }

    void* thing = (void*)position();
    position_ = position() + size;

    JS_EXTRA_POISON(thing, JS_ALLOCATED_NURSERY_PATTERN, size);

#ifdef JS_GC_ZEAL
    if (runtime()->gc.hasZealMode(ZealMode::CheckNursery)) {
        auto canary = reinterpret_cast<Canary*>(position() - CanarySize);
        canary->magicValue = CanaryMagicValue;
        canary->next = nullptr;
        if (lastCanary_) {
            MOZ_ASSERT(!lastCanary_->next);
            lastCanary_->next = canary;
        }
        lastCanary_ = canary;
    }
#endif

    MemProfiler::SampleNursery(reinterpret_cast<void*>(thing), size);
    return thing;
}

void*
js::Nursery::allocateBuffer(Zone* zone, size_t nbytes)
{
    MOZ_ASSERT(nbytes > 0);

    if (nbytes <= MaxNurseryBufferSize) {
        void* buffer = allocate(nbytes);
        if (buffer)
            return buffer;
    }

    void* buffer = zone->pod_malloc<uint8_t>(nbytes);
    if (buffer && !mallocedBuffers.putNew(buffer)) {
        js_free(buffer);
        return nullptr;
    }
    return buffer;
}

void*
js::Nursery::allocateBuffer(JSObject* obj, size_t nbytes)
{
    MOZ_ASSERT(obj);
    MOZ_ASSERT(nbytes > 0);

    if (!IsInsideNursery(obj))
        return obj->zone()->pod_malloc<uint8_t>(nbytes);
    return allocateBuffer(obj->zone(), nbytes);
}

void*
js::Nursery::reallocateBuffer(JSObject* obj, void* oldBuffer,
                              size_t oldBytes, size_t newBytes)
{
    if (!IsInsideNursery(obj))
        return obj->zone()->pod_realloc<uint8_t>((uint8_t*)oldBuffer, oldBytes, newBytes);

    if (!isInside(oldBuffer)) {
        void* newBuffer = obj->zone()->pod_realloc<uint8_t>((uint8_t*)oldBuffer, oldBytes, newBytes);
        if (newBuffer && oldBuffer != newBuffer)
            MOZ_ALWAYS_TRUE(mallocedBuffers.rekeyAs(oldBuffer, newBuffer, newBuffer));
        return newBuffer;
    }

    /* The nursery cannot make use of the returned slots data. */
    if (newBytes < oldBytes)
        return oldBuffer;

    void* newBuffer = allocateBuffer(obj->zone(), newBytes);
    if (newBuffer)
        PodCopy((uint8_t*)newBuffer, (uint8_t*)oldBuffer, oldBytes);
    return newBuffer;
}

void
js::Nursery::freeBuffer(void* buffer)
{
    if (!isInside(buffer)) {
        removeMallocedBuffer(buffer);
        js_free(buffer);
    }
}

void
Nursery::setForwardingPointer(void* oldData, void* newData, bool direct)
{
    MOZ_ASSERT(isInside(oldData));

    // Bug 1196210: If a zero-capacity header lands in the last 2 words of a
    // jemalloc chunk abutting the start of a nursery chunk, the (invalid)
    // newData pointer will appear to be "inside" the nursery.
    MOZ_ASSERT(!isInside(newData) || (uintptr_t(newData) & ChunkMask) == 0);

    if (direct) {
        *reinterpret_cast<void**>(oldData) = newData;
    } else {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!forwardedBuffers.initialized() && !forwardedBuffers.init())
            oomUnsafe.crash("Nursery::setForwardingPointer");
#ifdef DEBUG
        if (ForwardedBufferMap::Ptr p = forwardedBuffers.lookup(oldData))
            MOZ_ASSERT(p->value() == newData);
#endif
        if (!forwardedBuffers.put(oldData, newData))
            oomUnsafe.crash("Nursery::setForwardingPointer");
    }
}

void
Nursery::setSlotsForwardingPointer(HeapSlot* oldSlots, HeapSlot* newSlots, uint32_t nslots)
{
    // Slot arrays always have enough space for a forwarding pointer, since the
    // number of slots is never zero.
    MOZ_ASSERT(nslots > 0);
    setForwardingPointer(oldSlots, newSlots, /* direct = */ true);
}

void
Nursery::setElementsForwardingPointer(ObjectElements* oldHeader, ObjectElements* newHeader,
                                      uint32_t capacity)
{
    // Only use a direct forwarding pointer if there is enough space for one.
    setForwardingPointer(oldHeader->elements(), newHeader->elements(),
                         capacity > 0);
}

#ifdef DEBUG
static bool IsWriteableAddress(void* ptr)
{
    volatile uint64_t* vPtr = reinterpret_cast<volatile uint64_t*>(ptr);
    *vPtr = *vPtr;
    return true;
}
#endif

void
js::Nursery::forwardBufferPointer(HeapSlot** pSlotsElems)
{
    HeapSlot* old = *pSlotsElems;

    if (!isInside(old))
        return;

    // The new location for this buffer is either stored inline with it or in
    // the forwardedBuffers table.
    do {
        if (forwardedBuffers.initialized()) {
            if (ForwardedBufferMap::Ptr p = forwardedBuffers.lookup(old)) {
                *pSlotsElems = reinterpret_cast<HeapSlot*>(p->value());
                break;
            }
        }

        *pSlotsElems = *reinterpret_cast<HeapSlot**>(old);
    } while (false);

    MOZ_ASSERT(!isInside(*pSlotsElems));
    MOZ_ASSERT(IsWriteableAddress(*pSlotsElems));
}

js::TenuringTracer::TenuringTracer(JSRuntime* rt, Nursery* nursery)
  : JSTracer(rt, JSTracer::TracerKindTag::Tenuring, TraceWeakMapKeysValues)
  , nursery_(*nursery)
  , tenuredSize(0)
  , head(nullptr)
  , tail(&head)
{
}

void
js::Nursery::renderProfileJSON(JSONPrinter& json) const
{
    if (!isEnabled()) {
        json.beginObject();
        json.property("status", "nursery disabled");
        json.endObject();
        return;
    }

    json.beginObject();
#define EXTRACT_NAME(name, text) #name,
    static const char* names[] = {
FOR_EACH_NURSERY_PROFILE_TIME(EXTRACT_NAME)
#undef EXTRACT_NAME
    "" };

    size_t i = 0;
    for (auto time : profileDurations_)
        json.property(names[i++], time, json.MICROSECONDS);

    json.endObject();
}

/* static */ void
js::Nursery::printProfileHeader()
{
    fprintf(stderr, "MinorGC:               Reason  PRate Size ");
#define PRINT_HEADER(name, text)                                              \
    fprintf(stderr, " %6s", text);
FOR_EACH_NURSERY_PROFILE_TIME(PRINT_HEADER)
#undef PRINT_HEADER
    fprintf(stderr, "\n");
}

/* static */ void
js::Nursery::printProfileDurations(const ProfileDurations& times)
{
    for (auto time : times)
        fprintf(stderr, " %6" PRIi64, static_cast<int64_t>(time.ToMicroseconds()));
    fprintf(stderr, "\n");
}

void
js::Nursery::printTotalProfileTimes()
{
    if (enableProfiling_) {
        fprintf(stderr, "MinorGC TOTALS: %7" PRIu64 " collections:      ", minorGcCount_);
        printProfileDurations(totalDurations_);
    }
}

void
js::Nursery::maybeClearProfileDurations()
{
    for (auto& duration : profileDurations_)
        duration = mozilla::TimeDuration();
}

inline void
js::Nursery::startProfile(ProfileKey key)
{
    startTimes_[key] = TimeStamp::Now();
}

inline void
js::Nursery::endProfile(ProfileKey key)
{
    profileDurations_[key] = TimeStamp::Now() - startTimes_[key];
    totalDurations_[key] += profileDurations_[key];
}

void
js::Nursery::collect(JS::gcreason::Reason reason)
{
    MOZ_ASSERT(!TlsContext.get()->suppressGC);

    if (!isEnabled() || isEmpty()) {
        // Our barriers are not always exact, and there may be entries in the
        // storebuffer even when the nursery is disabled or empty. It's not safe
        // to keep these entries as they may refer to tenured cells which may be
        // freed after this point.
        runtime()->gc.storeBuffer().clear();
    }

    if (!isEnabled())
        return;

    JSRuntime* rt = runtime();
    rt->gc.incMinorGcNumber();

#ifdef JS_GC_ZEAL
    if (rt->gc.hasZealMode(ZealMode::CheckNursery)) {
        for (auto canary = lastCanary_; canary; canary = canary->next)
            MOZ_ASSERT(canary->magicValue == CanaryMagicValue);
    }
    lastCanary_ = nullptr;
#endif

    rt->gc.stats().beginNurseryCollection(reason);
    TraceMinorGCStart();

    maybeClearProfileDurations();
    startProfile(ProfileKey::Total);

    // The hazard analysis thinks doCollection can invalidate pointers in
    // tenureCounts below.
    JS::AutoSuppressGCAnalysis nogc;

    TenureCountCache tenureCounts;
    double promotionRate = 0;
    if (!isEmpty())
        promotionRate = doCollection(reason, tenureCounts);

    // Resize the nursery.
    startProfile(ProfileKey::Resize);
    maybeResizeNursery(reason, promotionRate);
    endProfile(ProfileKey::Resize);

    // If we are promoting the nursery, or exhausted the store buffer with
    // pointers to nursery things, which will force a collection well before
    // the nursery is full, look for object groups that are getting promoted
    // excessively and try to pretenure them.
    startProfile(ProfileKey::Pretenure);
    uint32_t pretenureCount = 0;
    if (promotionRate > 0.8 || reason == JS::gcreason::FULL_STORE_BUFFER) {
        JSContext* cx = TlsContext.get();
        for (auto& entry : tenureCounts.entries) {
            if (entry.count >= 3000) {
                ObjectGroup* group = entry.group;
                if (group->canPreTenure()) {
                    AutoCompartment ac(cx, group);
                    group->setShouldPreTenure(cx);
                    pretenureCount++;
                }
            }
        }
    }
    endProfile(ProfileKey::Pretenure);

    // We ignore gcMaxBytes when allocating for minor collection. However, if we
    // overflowed, we disable the nursery. The next time we allocate, we'll fail
    // because gcBytes >= gcMaxBytes.
    if (rt->gc.usage.gcBytes() >= rt->gc.tunables.gcMaxBytes())
        disable();

    endProfile(ProfileKey::Total);
    minorGcCount_++;

    TimeDuration totalTime = profileDurations_[ProfileKey::Total];
    rt->addTelemetry(JS_TELEMETRY_GC_MINOR_US, totalTime.ToMicroseconds());
    rt->addTelemetry(JS_TELEMETRY_GC_MINOR_REASON, reason);
    if (totalTime.ToMilliseconds() > 1.0)
        rt->addTelemetry(JS_TELEMETRY_GC_MINOR_REASON_LONG, reason);
    rt->addTelemetry(JS_TELEMETRY_GC_NURSERY_BYTES, sizeOfHeapCommitted());
    rt->addTelemetry(JS_TELEMETRY_GC_PRETENURE_COUNT, pretenureCount);

    rt->gc.stats().endNurseryCollection(reason);
    TraceMinorGCEnd();

    if (enableProfiling_ && totalTime >= profileThreshold_) {
        rt->gc.stats().maybePrintProfileHeaders();

        fprintf(stderr, "MinorGC: %20s %5.1f%% %4u ",
                JS::gcreason::ExplainReason(reason),
                promotionRate * 100,
                numChunks());
        printProfileDurations(profileDurations_);

        if (reportTenurings_) {
            for (auto& entry : tenureCounts.entries) {
                if (entry.count >= reportTenurings_) {
                    fprintf(stderr, "  %d x ", entry.count);
                    entry.group->print();
                }
            }
        }
    }
}

double
js::Nursery::doCollection(JS::gcreason::Reason reason,
                          TenureCountCache& tenureCounts)
{
    JSRuntime* rt = runtime();
    AutoTraceSession session(rt, JS::HeapState::MinorCollecting);
    AutoSetThreadIsPerformingGC performingGC;
    AutoStopVerifyingBarriers av(rt, false);
    AutoDisableProxyCheck disableStrictProxyChecking;
    mozilla::DebugOnly<AutoEnterOOMUnsafeRegion> oomUnsafeRegion;

    size_t initialNurserySize = spaceToEnd();

    // Move objects pointed to by roots from the nursery to the major heap.
    TenuringTracer mover(rt, this);

    // Mark the store buffer. This must happen first.
    StoreBuffer& sb = runtime()->gc.storeBuffer();

    // The MIR graph only contains nursery pointers if cancelIonCompilations()
    // is set on the store buffer, in which case we cancel all compilations.
    startProfile(ProfileKey::CancelIonCompilations);
    if (sb.cancelIonCompilations())
        js::CancelOffThreadIonCompile(rt);
    endProfile(ProfileKey::CancelIonCompilations);

    startProfile(ProfileKey::TraceValues);
    sb.traceValues(mover);
    endProfile(ProfileKey::TraceValues);

    startProfile(ProfileKey::TraceCells);
    sb.traceCells(mover);
    endProfile(ProfileKey::TraceCells);

    startProfile(ProfileKey::TraceSlots);
    sb.traceSlots(mover);
    endProfile(ProfileKey::TraceSlots);

    startProfile(ProfileKey::TraceWholeCells);
    sb.traceWholeCells(mover);
    endProfile(ProfileKey::TraceWholeCells);

    startProfile(ProfileKey::TraceGenericEntries);
    sb.traceGenericEntries(&mover);
    endProfile(ProfileKey::TraceGenericEntries);

    startProfile(ProfileKey::MarkRuntime);
    rt->gc.traceRuntimeForMinorGC(&mover, session.lock);
    endProfile(ProfileKey::MarkRuntime);

    startProfile(ProfileKey::MarkDebugger);
    {
        gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::MARK_ROOTS);
        Debugger::traceAllForMovingGC(&mover);
    }
    endProfile(ProfileKey::MarkDebugger);

    startProfile(ProfileKey::ClearNewObjectCache);
    rt->caches().newObjectCache.clearNurseryObjects(rt);
    endProfile(ProfileKey::ClearNewObjectCache);

    // Most of the work is done here. This loop iterates over objects that have
    // been moved to the major heap. If these objects have any outgoing pointers
    // to the nursery, then those nursery objects get moved as well, until no
    // objects are left to move. That is, we iterate to a fixed point.
    startProfile(ProfileKey::CollectToFP);
    collectToFixedPoint(mover, tenureCounts);
    endProfile(ProfileKey::CollectToFP);

    // Sweep compartments to update the array buffer object's view lists.
    startProfile(ProfileKey::SweepArrayBufferViewList);
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
        c->sweepAfterMinorGC(&mover);
    endProfile(ProfileKey::SweepArrayBufferViewList);

    // Update any slot or element pointers whose destination has been tenured.
    startProfile(ProfileKey::UpdateJitActivations);
    js::jit::UpdateJitActivationsForMinorGC(rt, &mover);
    forwardedBuffers.finish();
    endProfile(ProfileKey::UpdateJitActivations);

    startProfile(ProfileKey::ObjectsTenuredCallback);
    rt->gc.callObjectsTenuredCallback();
    endProfile(ProfileKey::ObjectsTenuredCallback);

    // Sweep.
    startProfile(ProfileKey::FreeMallocedBuffers);
    freeMallocedBuffers();
    endProfile(ProfileKey::FreeMallocedBuffers);

    startProfile(ProfileKey::Sweep);
    sweep();
    endProfile(ProfileKey::Sweep);

    startProfile(ProfileKey::ClearStoreBuffer);
    runtime()->gc.storeBuffer().clear();
    endProfile(ProfileKey::ClearStoreBuffer);

    // Make sure hashtables have been updated after the collection.
    startProfile(ProfileKey::CheckHashTables);
#ifdef JS_GC_ZEAL
    if (rt->hasZealMode(ZealMode::CheckHashTablesOnMinorGC))
        CheckHashTablesAfterMovingGC(rt);
#endif
    endProfile(ProfileKey::CheckHashTables);

    // Calculate and return the promotion rate.
    return mover.tenuredSize / double(initialNurserySize);
}

void
js::Nursery::FreeMallocedBuffersTask::transferBuffersToFree(MallocedBuffersSet& buffersToFree,
                                                            const AutoLockHelperThreadState& lock)
{
    // Transfer the contents of the source set to the task's buffers_ member by
    // swapping the sets, which also clears the source.
    MOZ_ASSERT(!isRunningWithLockHeld(lock));
    MOZ_ASSERT(buffers_.empty());
    mozilla::Swap(buffers_, buffersToFree);
}

void
js::Nursery::FreeMallocedBuffersTask::run()
{
    for (MallocedBuffersSet::Range r = buffers_.all(); !r.empty(); r.popFront())
        fop_->free_(r.front());
    buffers_.clear();
}

void
js::Nursery::freeMallocedBuffers()
{
    if (mallocedBuffers.empty())
        return;

    bool started;
    {
        AutoLockHelperThreadState lock;
        freeMallocedBuffersTask->joinWithLockHeld(lock);
        freeMallocedBuffersTask->transferBuffersToFree(mallocedBuffers, lock);
        started = freeMallocedBuffersTask->startWithLockHeld(lock);
    }

    if (!started)
        freeMallocedBuffersTask->runFromActiveCooperatingThread(runtime());

    MOZ_ASSERT(mallocedBuffers.empty());
}

void
js::Nursery::waitBackgroundFreeEnd()
{
    // We may finishRoots before nursery init if runtime init fails.
    if (!isEnabled())
        return;

    MOZ_ASSERT(freeMallocedBuffersTask);
    freeMallocedBuffersTask->join();
}

void
js::Nursery::sweep()
{
    /* Sweep unique id's in all in-use chunks. */
    for (Cell* cell : cellsWithUid_) {
        JSObject* obj = static_cast<JSObject*>(cell);
        if (!IsForwarded(obj))
            obj->zone()->removeUniqueId(obj);
        else
            MOZ_ASSERT(Forwarded(obj)->zone()->hasUniqueId(Forwarded(obj)));
    }
    cellsWithUid_.clear();

    sweepDictionaryModeObjects();

#ifdef JS_GC_ZEAL
    /* Poison the nursery contents so touching a freed object will crash. */
    for (unsigned i = 0; i < numChunks(); i++)
        chunk(i).poisonAndInit(runtime(), JS_SWEPT_NURSERY_PATTERN);

    if (runtime()->hasZealMode(ZealMode::GenerationalGC)) {
        /* Only reset the alloc point when we are close to the end. */
        if (currentChunk_ + 1 == numChunks())
            setCurrentChunk(0);
    } else
#endif
    {
#ifdef JS_CRASH_DIAGNOSTICS
        for (unsigned i = 0; i < numChunks(); ++i)
            chunk(i).poisonAndInit(runtime(), JS_SWEPT_NURSERY_PATTERN);
#endif
        setCurrentChunk(0);
    }

    /* Set current start position for isEmpty checks. */
    setStartPosition();
    MemProfiler::SweepNursery(runtime());
}

size_t
js::Nursery::spaceToEnd() const
{
    unsigned lastChunk = numChunks() - 1;

    MOZ_ASSERT(lastChunk >= currentStartChunk_);
    MOZ_ASSERT(currentStartPosition_ - chunk(currentStartChunk_).start() <= NurseryChunkUsableSize);

    size_t bytes = (chunk(currentStartChunk_).end() - currentStartPosition_) +
                   ((lastChunk - currentStartChunk_) * NurseryChunkUsableSize);

    MOZ_ASSERT(bytes <= numChunks() * NurseryChunkUsableSize);

    return bytes;
}

MOZ_ALWAYS_INLINE void
js::Nursery::setCurrentChunk(unsigned chunkno)
{
    MOZ_ASSERT(chunkno < maxChunks());
    MOZ_ASSERT(chunkno < numChunks());
    currentChunk_ = chunkno;
    position_ = chunk(chunkno).start();
    currentEnd_ = chunk(chunkno).end();
    chunk(chunkno).poisonAndInit(runtime(), JS_FRESH_NURSERY_PATTERN);
}

MOZ_ALWAYS_INLINE void
js::Nursery::setStartPosition()
{
    currentStartChunk_ = currentChunk_;
    currentStartPosition_ = position();
}

void
js::Nursery::maybeResizeNursery(JS::gcreason::Reason reason, double promotionRate)
{
    static const double GrowThreshold   = 0.05;
    static const double ShrinkThreshold = 0.01;

    // Shrink the nursery to its minimum size of we ran out of memory or
    // received a memory pressure event.
    if (gc::IsOOMReason(reason)) {
        minimizeAllocableSpace();
        return;
    }

    if (promotionRate > GrowThreshold)
        growAllocableSpace();
    else if (promotionRate < ShrinkThreshold && previousPromotionRate_ < ShrinkThreshold)
        shrinkAllocableSpace();

    previousPromotionRate_ = promotionRate;
}

void
js::Nursery::growAllocableSpace()
{
    updateNumChunks(Min(numChunks() * 2, maxNurseryChunks_));
}

void
js::Nursery::shrinkAllocableSpace()
{
#ifdef JS_GC_ZEAL
    if (runtime()->hasZealMode(ZealMode::GenerationalGC))
        return;
#endif
    updateNumChunks(Max(numChunks() - 1, 1u));
}

void
js::Nursery::minimizeAllocableSpace()
{
#ifdef JS_GC_ZEAL
    if (runtime()->hasZealMode(ZealMode::GenerationalGC))
        return;
#endif
    updateNumChunks(1);
}

void
js::Nursery::updateNumChunks(unsigned newCount)
{
    if (numChunks() != newCount) {
        AutoMaybeStartBackgroundAllocation maybeBgAlloc;
        AutoLockGC lock(runtime());
        updateNumChunksLocked(newCount, maybeBgAlloc, lock);
    }
}

void
js::Nursery::updateNumChunksLocked(unsigned newCount,
                                   AutoMaybeStartBackgroundAllocation& maybeBgAlloc,
                                   AutoLockGC& lock)
{
    // The GC nursery is an optimization and so if we fail to allocate nursery
    // chunks we do not report an error.

    MOZ_ASSERT(newCount <= maxChunks());

    unsigned priorCount = numChunks();
    MOZ_ASSERT(priorCount != newCount);

    if (newCount < priorCount) {
        // Shrink the nursery and free unused chunks.
        for (unsigned i = newCount; i < priorCount; i++)
            runtime()->gc.recycleChunk(chunk(i).toChunk(runtime()), lock);
        chunks_.shrinkTo(newCount);
        return;
    }

    // Grow the nursery and allocate new chunks.
    if (!chunks_.resize(newCount))
        return;

    for (unsigned i = priorCount; i < newCount; i++) {
        auto newChunk = runtime()->gc.getOrAllocChunk(lock, maybeBgAlloc);
        if (!newChunk) {
            chunks_.shrinkTo(i);
            return;
        }

        chunks_[i] = NurseryChunk::fromChunk(newChunk);
        chunk(i).poisonAndInit(runtime(), JS_FRESH_NURSERY_PATTERN);
    }
}

bool
js::Nursery::queueDictionaryModeObjectToSweep(NativeObject* obj)
{
    MOZ_ASSERT(IsInsideNursery(obj));
    return dictionaryModeObjects_.append(obj);
}

void
js::Nursery::sweepDictionaryModeObjects()
{
    for (auto obj : dictionaryModeObjects_) {
        if (!IsForwarded(obj))
            obj->sweepDictionaryListPointer();
    }
    dictionaryModeObjects_.clear();
}

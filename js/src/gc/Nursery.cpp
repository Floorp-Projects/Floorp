/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Nursery-inl.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/Unused.h"

#include "jsutil.h"

#include "builtin/MapObject.h"
#include "gc/FreeOp.h"
#include "gc/GCInternals.h"
#include "gc/Memory.h"
#include "gc/PublicIterators.h"
#include "jit/JitFrames.h"
#include "jit/JitRealm.h"
#include "vm/ArrayObject.h"
#include "vm/Debugger.h"
#if defined(DEBUG)
#  include "vm/EnvironmentObject.h"
#endif
#include "vm/JSONPrinter.h"
#include "vm/Realm.h"
#include "vm/Time.h"
#include "vm/TypedArrayObject.h"
#include "vm/TypeInference.h"

#include "gc/Marking-inl.h"
#include "gc/Zone-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace gc;

using mozilla::DebugOnly;
using mozilla::PodCopy;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

constexpr uintptr_t CanaryMagicValue = 0xDEADB15D;

#ifdef JS_GC_ZEAL
struct js::Nursery::Canary {
  uintptr_t magicValue;
  Canary* next;
};
#endif

namespace js {
struct NurseryChunk {
  char data[Nursery::NurseryChunkUsableSize];
  gc::ChunkTrailer trailer;
  static NurseryChunk* fromChunk(gc::Chunk* chunk);
  void poisonAndInit(JSRuntime* rt, size_t extent = ChunkSize);
  void poisonAfterSweep(size_t extent = ChunkSize);
  uintptr_t start() const { return uintptr_t(&data); }
  uintptr_t end() const { return uintptr_t(&trailer); }
  gc::Chunk* toChunk(JSRuntime* rt);
};
static_assert(sizeof(js::NurseryChunk) == gc::ChunkSize,
              "Nursery chunk size must match gc::Chunk size.");

} /* namespace js */

inline void js::NurseryChunk::poisonAndInit(JSRuntime* rt, size_t extent) {
  MOZ_ASSERT(extent <= ChunkSize);
  MOZ_MAKE_MEM_UNDEFINED(this, extent);

  Poison(this, JS_FRESH_NURSERY_PATTERN, extent, MemCheckKind::MakeUndefined);

  new (&trailer) gc::ChunkTrailer(rt, &rt->gc.storeBuffer());
}

inline void js::NurseryChunk::poisonAfterSweep(size_t extent) {
  MOZ_ASSERT(extent <= ChunkSize);
  // We can poison the same chunk more than once, so first make sure memory
  // sanitizers will let us poison it.
  MOZ_MAKE_MEM_UNDEFINED(this, extent);

  Poison(this, JS_SWEPT_NURSERY_PATTERN, extent, MemCheckKind::MakeNoAccess);
}

/* static */
inline js::NurseryChunk* js::NurseryChunk::fromChunk(Chunk* chunk) {
  return reinterpret_cast<NurseryChunk*>(chunk);
}

inline Chunk* js::NurseryChunk::toChunk(JSRuntime* rt) {
  auto chunk = reinterpret_cast<Chunk*>(this);
  chunk->init(rt);
  return chunk;
}

js::Nursery::Nursery(JSRuntime* rt)
    : runtime_(rt),
      position_(0),
      currentStartChunk_(0),
      currentStartPosition_(0),
      currentEnd_(0),
      currentStringEnd_(0),
      currentChunk_(0),
      capacity_(0),
      chunkCountLimit_(0),
      timeInChunkAlloc_(0),
      profileThreshold_(0),
      enableProfiling_(false),
      canAllocateStrings_(true),
      reportTenurings_(0),
      minorGCTriggerReason_(JS::GCReason::NO_REASON)
#ifdef JS_GC_ZEAL
      ,
      lastCanary_(nullptr)
#endif
{
  const char* env = getenv("MOZ_NURSERY_STRINGS");
  if (env && *env) {
    canAllocateStrings_ = (*env == '1');
  }
}

bool js::Nursery::init(uint32_t maxNurseryBytes, AutoLockGCBgAlloc& lock) {
  // The nursery is permanently disabled when recording or replaying. Nursery
  // collections may occur at non-deterministic points in execution.
  if (mozilla::recordreplay::IsRecordingOrReplaying()) {
    maxNurseryBytes = 0;
  }

  /* maxNurseryBytes parameter is rounded down to a multiple of chunk size. */
  chunkCountLimit_ = maxNurseryBytes >> ChunkShift;

  /* If no chunks are specified then the nursery is permanently disabled. */
  if (chunkCountLimit_ == 0) {
    return true;
  }

  if (!allocateNextChunk(0, lock)) {
    return false;
  }
  capacity_ = roundSize(tunables().gcMinNurseryBytes());
  MOZ_ASSERT(capacity_ >= ArenaSize);
  /* After this point the Nursery has been enabled */

  setCurrentChunk(0, true);
  setStartPosition();

  char* env = getenv("JS_GC_PROFILE_NURSERY");
  if (env) {
    if (0 == strcmp(env, "help")) {
      fprintf(stderr,
              "JS_GC_PROFILE_NURSERY=N\n"
              "\tReport minor GC's taking at least N microseconds.\n");
      exit(0);
    }
    enableProfiling_ = true;
    profileThreshold_ = TimeDuration::FromMicroseconds(atoi(env));
  }

  env = getenv("JS_GC_REPORT_TENURING");
  if (env) {
    if (0 == strcmp(env, "help")) {
      fprintf(stderr,
              "JS_GC_REPORT_TENURING=N\n"
              "\tAfter a minor GC, report any ObjectGroups with at least N "
              "instances tenured.\n");
      exit(0);
    }
    reportTenurings_ = atoi(env);
  }

  if (!runtime()->gc.storeBuffer().enable()) {
    return false;
  }

  MOZ_ASSERT(isEnabled());
  return true;
}

js::Nursery::~Nursery() { disable(); }

void js::Nursery::enable() {
  MOZ_ASSERT(isEmpty());
  MOZ_ASSERT(!runtime()->gc.isVerifyPreBarriersEnabled());
  if (isEnabled() || !chunkCountLimit()) {
    return;
  }

  {
    AutoLockGCBgAlloc lock(runtime());
    if (!allocateNextChunk(0, lock)) {
      return;
    }
    capacity_ = roundSize(tunables().gcMinNurseryBytes());
    MOZ_ASSERT(capacity_ >= ArenaSize);
  }

  setCurrentChunk(0, true);
  setStartPosition();
#ifdef JS_GC_ZEAL
  if (runtime()->hasZealMode(ZealMode::GenerationalGC)) {
    enterZealMode();
  }
#endif

  MOZ_ALWAYS_TRUE(runtime()->gc.storeBuffer().enable());
}

void js::Nursery::disable() {
  MOZ_ASSERT(isEmpty());
  if (!isEnabled()) {
    return;
  }

  freeChunksFrom(0);
  capacity_ = 0;

  // We must reset currentEnd_ so that there is no space for anything in the
  // nursery.  JIT'd code uses this even if the nursery is disabled.
  currentEnd_ = 0;
  currentStringEnd_ = 0;
  position_ = 0;
  runtime()->gc.storeBuffer().disable();
}

void js::Nursery::enableStrings() {
  MOZ_ASSERT(isEmpty());
  canAllocateStrings_ = true;
  currentStringEnd_ = currentEnd_;
}

void js::Nursery::disableStrings() {
  MOZ_ASSERT(isEmpty());
  canAllocateStrings_ = false;
  currentStringEnd_ = 0;
}

bool js::Nursery::isEmpty() const {
  if (!isEnabled()) {
    return true;
  }

  if (!runtime()->hasZealMode(ZealMode::GenerationalGC)) {
    MOZ_ASSERT(currentStartChunk_ == 0);
    MOZ_ASSERT(currentStartPosition_ == chunk(0).start());
  }
  return position() == currentStartPosition_;
}

#ifdef JS_GC_ZEAL
void js::Nursery::enterZealMode() {
  if (isEnabled()) {
    capacity_ = chunkCountLimit() * ChunkSize;
    setCurrentEnd();
  }
}

void js::Nursery::leaveZealMode() {
  if (isEnabled()) {
    MOZ_ASSERT(isEmpty());
    setCurrentChunk(0, true);
    setStartPosition();
  }
}
#endif  // JS_GC_ZEAL

JSObject* js::Nursery::allocateObject(JSContext* cx, size_t size,
                                      size_t nDynamicSlots,
                                      const js::Class* clasp) {
  // Ensure there's enough space to replace the contents with a
  // RelocationOverlay.
  MOZ_ASSERT(size >= sizeof(RelocationOverlay));

  // Sanity check the finalizer.
  MOZ_ASSERT_IF(clasp->hasFinalize(),
                CanNurseryAllocateFinalizedClass(clasp) || clasp->isProxy());

  // Make the object allocation.
  JSObject* obj = static_cast<JSObject*>(allocate(size));
  if (!obj) {
    return nullptr;
  }

  // If we want external slots, add them.
  HeapSlot* slots = nullptr;
  if (nDynamicSlots) {
    MOZ_ASSERT(clasp->isNative());
    slots = static_cast<HeapSlot*>(
        allocateBuffer(cx->zone(), nDynamicSlots * sizeof(HeapSlot)));
    if (!slots) {
      // It is safe to leave the allocated object uninitialized, since we
      // do not visit unallocated things in the nursery.
      return nullptr;
    }
  }

  // Store slots pointer directly in new object. If no dynamic slots were
  // requested, caller must initialize slots_ field itself as needed. We
  // don't know if the caller was a native object or not.
  if (nDynamicSlots) {
    static_cast<NativeObject*>(obj)->initSlots(slots);
  }

  gcTracer.traceNurseryAlloc(obj, size);
  return obj;
}

Cell* js::Nursery::allocateString(Zone* zone, size_t size, AllocKind kind) {
  // Ensure there's enough space to replace the contents with a
  // RelocationOverlay.
  MOZ_ASSERT(size >= sizeof(RelocationOverlay));

  size_t allocSize =
      JS_ROUNDUP(sizeof(StringLayout) - 1 + size, CellAlignBytes);
  auto header = static_cast<StringLayout*>(allocate(allocSize));
  if (!header) {
    return nullptr;
  }
  header->zone = zone;

  auto cell = reinterpret_cast<Cell*>(&header->cell);
  gcTracer.traceNurseryAlloc(cell, kind);
  return cell;
}

void* js::Nursery::allocate(size_t size) {
  MOZ_ASSERT(isEnabled());
  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
  MOZ_ASSERT_IF(currentChunk_ == currentStartChunk_,
                position() >= currentStartPosition_);
  MOZ_ASSERT(position() % CellAlignBytes == 0);
  MOZ_ASSERT(size % CellAlignBytes == 0);

#ifdef JS_GC_ZEAL
  static const size_t CanarySize =
      (sizeof(Nursery::Canary) + CellAlignBytes - 1) & ~CellAlignMask;
  if (runtime()->gc.hasZealMode(ZealMode::CheckNursery)) {
    size += CanarySize;
  }
#endif

  if (currentEnd() < position() + size) {
    unsigned chunkno = currentChunk_ + 1;
    MOZ_ASSERT(chunkno <= chunkCountLimit());
    MOZ_ASSERT(chunkno <= maxChunkCount());
    MOZ_ASSERT(chunkno <= allocatedChunkCount());
    if (chunkno == maxChunkCount()) {
      return nullptr;
    }
    if (MOZ_UNLIKELY(chunkno == allocatedChunkCount())) {
      mozilla::TimeStamp start = ReallyNow();
      {
        AutoLockGCBgAlloc lock(runtime());
        if (!allocateNextChunk(chunkno, lock)) {
          return nullptr;
        }
      }
      timeInChunkAlloc_ += ReallyNow() - start;
      MOZ_ASSERT(chunkno < allocatedChunkCount());
    }
    setCurrentChunk(chunkno);
  }

  void* thing = (void*)position();
  position_ = position() + size;
  // We count this regardless of the profiler's state, assuming that it costs
  // just as much to count it, as to check the profiler's state and decide not
  // to count it.
  stats().noteNurseryAlloc();

  DebugOnlyPoison(thing, JS_ALLOCATED_NURSERY_PATTERN, size,
                  MemCheckKind::MakeUndefined);

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

  return thing;
}

void* js::Nursery::allocateBuffer(Zone* zone, size_t nbytes) {
  MOZ_ASSERT(nbytes > 0);

  if (nbytes <= MaxNurseryBufferSize) {
    void* buffer = allocate(nbytes);
    if (buffer) {
      return buffer;
    }
  }

  void* buffer = zone->pod_malloc<uint8_t>(nbytes);
  if (buffer && !registerMallocedBuffer(buffer)) {
    js_free(buffer);
    return nullptr;
  }
  return buffer;
}

void* js::Nursery::allocateBuffer(JSObject* obj, size_t nbytes) {
  MOZ_ASSERT(obj);
  MOZ_ASSERT(nbytes > 0);

  if (!IsInsideNursery(obj)) {
    return obj->zone()->pod_malloc<uint8_t>(nbytes);
  }
  return allocateBuffer(obj->zone(), nbytes);
}

void* js::Nursery::allocateBufferSameLocation(JSObject* obj, size_t nbytes) {
  MOZ_ASSERT(obj);
  MOZ_ASSERT(nbytes > 0);
  MOZ_ASSERT(nbytes <= MaxNurseryBufferSize);

  if (!IsInsideNursery(obj)) {
    return obj->zone()->pod_malloc<uint8_t>(nbytes);
  }

  return allocate(nbytes);
}

void* js::Nursery::allocateZeroedBuffer(
    Zone* zone, size_t nbytes, arena_id_t arena /*= js::MallocArena*/) {
  MOZ_ASSERT(nbytes > 0);

  if (nbytes <= MaxNurseryBufferSize) {
    void* buffer = allocate(nbytes);
    if (buffer) {
      memset(buffer, 0, nbytes);
      return buffer;
    }
  }

  void* buffer = zone->pod_calloc<uint8_t>(nbytes, arena);
  if (buffer && !registerMallocedBuffer(buffer)) {
    js_free(buffer);
    return nullptr;
  }
  return buffer;
}

void* js::Nursery::allocateZeroedBuffer(
    JSObject* obj, size_t nbytes, arena_id_t arena /*= js::MallocArena*/) {
  MOZ_ASSERT(obj);
  MOZ_ASSERT(nbytes > 0);

  if (!IsInsideNursery(obj)) {
    return obj->zone()->pod_calloc<uint8_t>(nbytes, arena);
  }
  return allocateZeroedBuffer(obj->zone(), nbytes, arena);
}

void* js::Nursery::reallocateBuffer(JSObject* obj, void* oldBuffer,
                                    size_t oldBytes, size_t newBytes) {
  if (!IsInsideNursery(obj)) {
    return obj->zone()->pod_realloc<uint8_t>((uint8_t*)oldBuffer, oldBytes,
                                             newBytes);
  }

  if (!isInside(oldBuffer)) {
    void* newBuffer = obj->zone()->pod_realloc<uint8_t>((uint8_t*)oldBuffer,
                                                        oldBytes, newBytes);
    if (newBuffer && oldBuffer != newBuffer) {
      MOZ_ALWAYS_TRUE(mallocedBuffers.rekeyAs(oldBuffer, newBuffer, newBuffer));
    }
    return newBuffer;
  }

  /* The nursery cannot make use of the returned slots data. */
  if (newBytes < oldBytes) {
    return oldBuffer;
  }

  void* newBuffer = allocateBuffer(obj->zone(), newBytes);
  if (newBuffer) {
    PodCopy((uint8_t*)newBuffer, (uint8_t*)oldBuffer, oldBytes);
  }
  return newBuffer;
}

void js::Nursery::freeBuffer(void* buffer) {
  if (!isInside(buffer)) {
    removeMallocedBuffer(buffer);
    js_free(buffer);
  }
}

void Nursery::setIndirectForwardingPointer(void* oldData, void* newData) {
  MOZ_ASSERT(isInside(oldData));

  // Bug 1196210: If a zero-capacity header lands in the last 2 words of a
  // jemalloc chunk abutting the start of a nursery chunk, the (invalid)
  // newData pointer will appear to be "inside" the nursery.
  MOZ_ASSERT(!isInside(newData) || (uintptr_t(newData) & ChunkMask) == 0);

  AutoEnterOOMUnsafeRegion oomUnsafe;
#ifdef DEBUG
  if (ForwardedBufferMap::Ptr p = forwardedBuffers.lookup(oldData)) {
    MOZ_ASSERT(p->value() == newData);
  }
#endif
  if (!forwardedBuffers.put(oldData, newData)) {
    oomUnsafe.crash("Nursery::setForwardingPointer");
  }
}

#ifdef DEBUG
static bool IsWriteableAddress(void* ptr) {
  volatile uint64_t* vPtr = reinterpret_cast<volatile uint64_t*>(ptr);
  *vPtr = *vPtr;
  return true;
}
#endif

void js::Nursery::forwardBufferPointer(HeapSlot** pSlotsElems) {
  HeapSlot* old = *pSlotsElems;

  if (!isInside(old)) {
    return;
  }

  // The new location for this buffer is either stored inline with it or in
  // the forwardedBuffers table.
  do {
    if (ForwardedBufferMap::Ptr p = forwardedBuffers.lookup(old)) {
      *pSlotsElems = reinterpret_cast<HeapSlot*>(p->value());
      break;
    }

    *pSlotsElems = *reinterpret_cast<HeapSlot**>(old);
  } while (false);

  MOZ_ASSERT(!isInside(*pSlotsElems));
  MOZ_ASSERT(IsWriteableAddress(*pSlotsElems));
}

js::TenuringTracer::TenuringTracer(JSRuntime* rt, Nursery* nursery)
    : JSTracer(rt, JSTracer::TracerKindTag::Tenuring, TraceWeakMapKeysValues),
      nursery_(*nursery),
      tenuredSize(0),
      tenuredCells(0),
      objHead(nullptr),
      objTail(&objHead),
      stringHead(nullptr),
      stringTail(&stringHead) {}

inline float js::Nursery::calcPromotionRate(bool* validForTenuring) const {
  float used = float(previousGC.nurseryUsedBytes);
  float capacity = float(previousGC.nurseryCapacity);
  float tenured = float(previousGC.tenuredBytes);
  float rate;

  if (previousGC.nurseryUsedBytes > 0) {
    if (validForTenuring) {
      /*
       * We can only use promotion rates if they're likely to be valid,
       * they're only valid if the nursury was at least 90% full.
       */
      *validForTenuring = used > capacity * 0.9f;
    }
    rate = tenured / used;
  } else {
    if (validForTenuring) {
      *validForTenuring = false;
    }
    rate = 0.0f;
  }

  return rate;
}

void js::Nursery::renderProfileJSON(JSONPrinter& json) const {
  if (!isEnabled()) {
    json.beginObject();
    json.property("status", "nursery disabled");
    json.endObject();
    return;
  }

  if (previousGC.reason == JS::GCReason::NO_REASON) {
    // If the nursery was empty when the last minorGC was requested, then
    // no nursery collection will have been performed but JSON may still be
    // requested. (And as a public API, this function should not crash in
    // such a case.)
    json.beginObject();
    json.property("status", "nursery empty");
    json.endObject();
    return;
  }

  json.beginObject();

  json.property("status", "complete");

  json.property("reason", JS::ExplainGCReason(previousGC.reason));
  json.property("bytes_tenured", previousGC.tenuredBytes);
  json.property("cells_tenured", previousGC.tenuredCells);
  json.property("strings_tenured",
                stats().getStat(gcstats::STAT_STRINGS_TENURED));
  json.property("bytes_used", previousGC.nurseryUsedBytes);
  json.property("cur_capacity", previousGC.nurseryCapacity);
  const size_t newCapacity = capacity();
  if (newCapacity != previousGC.nurseryCapacity) {
    json.property("new_capacity", newCapacity);
  }
  if (previousGC.nurseryCommitted != previousGC.nurseryCapacity) {
    json.property("lazy_capacity", previousGC.nurseryCommitted);
  }
  if (!timeInChunkAlloc_.IsZero()) {
    json.property("chunk_alloc_us", timeInChunkAlloc_, json.MICROSECONDS);
  }

  // These counters only contain consistent data if the profiler is enabled,
  // and then there's no guarentee.
  if (runtime()->geckoProfiler().enabled()) {
    json.property("cells_allocated_nursery",
                  stats().allocsSinceMinorGCNursery());
    json.property("cells_allocated_tenured",
                  stats().allocsSinceMinorGCTenured());
  }

  if (stats().getStat(gcstats::STAT_OBJECT_GROUPS_PRETENURED)) {
    json.property("groups_pretenured",
                  stats().getStat(gcstats::STAT_OBJECT_GROUPS_PRETENURED));
  }
  if (stats().getStat(gcstats::STAT_NURSERY_STRING_REALMS_DISABLED)) {
    json.property(
        "nursery_string_realms_disabled",
        stats().getStat(gcstats::STAT_NURSERY_STRING_REALMS_DISABLED));
  }

  json.beginObjectProperty("phase_times");

#define EXTRACT_NAME(name, text) #name,
  static const char* const names[] = {
      FOR_EACH_NURSERY_PROFILE_TIME(EXTRACT_NAME)
#undef EXTRACT_NAME
          ""};

  size_t i = 0;
  for (auto time : profileDurations_) {
    json.property(names[i++], time, json.MICROSECONDS);
  }

  json.endObject();  // timings value

  json.endObject();
}

/* static */
void js::Nursery::printProfileHeader() {
  fprintf(stderr, "MinorGC:               Reason  PRate Size        ");
#define PRINT_HEADER(name, text) fprintf(stderr, " %6s", text);
  FOR_EACH_NURSERY_PROFILE_TIME(PRINT_HEADER)
#undef PRINT_HEADER
  fprintf(stderr, "\n");
}

/* static */
void js::Nursery::printProfileDurations(const ProfileDurations& times) {
  for (auto time : times) {
    fprintf(stderr, " %6" PRIi64, static_cast<int64_t>(time.ToMicroseconds()));
  }
  fprintf(stderr, "\n");
}

void js::Nursery::printTotalProfileTimes() {
  if (enableProfiling_) {
    fprintf(stderr, "MinorGC TOTALS: %7" PRIu64 " collections:             ",
            runtime()->gc.minorGCCount());
    printProfileDurations(totalDurations_);
  }
}

void js::Nursery::maybeClearProfileDurations() {
  for (auto& duration : profileDurations_) {
    duration = mozilla::TimeDuration();
  }
}

inline void js::Nursery::startProfile(ProfileKey key) {
  startTimes_[key] = ReallyNow();
}

inline void js::Nursery::endProfile(ProfileKey key) {
  profileDurations_[key] = ReallyNow() - startTimes_[key];
  totalDurations_[key] += profileDurations_[key];
}

bool js::Nursery::shouldCollect() const {
  if (minorGCRequested()) {
    return true;
  }

  bool belowBytesThreshold =
      freeSpace() < tunables().nurseryFreeThresholdForIdleCollection();
  bool belowFractionThreshold =
      float(freeSpace()) / float(capacity()) <
      tunables().nurseryFreeThresholdForIdleCollectionFraction();

  // We want to use belowBytesThreshold when the nursery is sufficiently large,
  // and belowFractionThreshold when it's small.
  //
  // When the nursery is small then belowBytesThreshold is a lower threshold
  // (triggered earlier) than belowFractionThreshold.  So if the fraction
  // threshold is true, the bytes one will be true also.  The opposite is true
  // when the nursery is large.
  //
  // Therefore, by the time we cross the threshold we care about, we've already
  // crossed the other one, and we can boolean AND to use either condition
  // without encoding any "is the nursery big/small" test/threshold.  The point
  // at which they cross is when the nursery is:  BytesThreshold /
  // FractionThreshold large.
  //
  // With defaults that's:
  //
  //   1MB = 256KB / 0.25
  //
  return belowBytesThreshold && belowFractionThreshold;
}

static inline bool IsFullStoreBufferReason(JS::GCReason reason) {
  return reason == JS::GCReason::FULL_WHOLE_CELL_BUFFER ||
         reason == JS::GCReason::FULL_GENERIC_BUFFER ||
         reason == JS::GCReason::FULL_VALUE_BUFFER ||
         reason == JS::GCReason::FULL_CELL_PTR_BUFFER ||
         reason == JS::GCReason::FULL_SLOT_BUFFER ||
         reason == JS::GCReason::FULL_SHAPE_BUFFER;
}

void js::Nursery::collect(JS::GCReason reason) {
  JSRuntime* rt = runtime();
  MOZ_ASSERT(!rt->mainContextFromOwnThread()->suppressGC);

  mozilla::recordreplay::AutoDisallowThreadEvents disallow;

  if (!isEnabled() || isEmpty()) {
    // Our barriers are not always exact, and there may be entries in the
    // storebuffer even when the nursery is disabled or empty. It's not safe
    // to keep these entries as they may refer to tenured cells which may be
    // freed after this point.
    rt->gc.storeBuffer().clear();
  }

  if (!isEnabled()) {
    return;
  }

#ifdef JS_GC_ZEAL
  if (rt->gc.hasZealMode(ZealMode::CheckNursery)) {
    for (auto canary = lastCanary_; canary; canary = canary->next) {
      MOZ_ASSERT(canary->magicValue == CanaryMagicValue);
    }
  }
  lastCanary_ = nullptr;
#endif

  stats().beginNurseryCollection(reason);
  gcTracer.traceMinorGCStart();

  maybeClearProfileDurations();
  startProfile(ProfileKey::Total);

  // The analysis marks TenureCount as not problematic for GC hazards because
  // it is only used here, and ObjectGroup pointers are never
  // nursery-allocated.
  MOZ_ASSERT(!IsNurseryAllocable(AllocKind::OBJECT_GROUP));

  TenureCountCache tenureCounts;
  previousGC.reason = JS::GCReason::NO_REASON;
  if (!isEmpty()) {
    doCollection(reason, tenureCounts);
  } else {
    previousGC.nurseryUsedBytes = 0;
    previousGC.nurseryCapacity = capacity();
    previousGC.nurseryCommitted = committed();
    previousGC.tenuredBytes = 0;
    previousGC.tenuredCells = 0;
  }

  // Resize the nursery.
  maybeResizeNursery(reason);

  // If we are promoting the nursery, or exhausted the store buffer with
  // pointers to nursery things, which will force a collection well before
  // the nursery is full, look for object groups that are getting promoted
  // excessively and try to pretenure them.
  startProfile(ProfileKey::Pretenure);
  bool validPromotionRate;
  const float promotionRate = calcPromotionRate(&validPromotionRate);
  uint32_t pretenureCount = 0;
  bool shouldPretenure =
      tunables().attemptPretenuring() &&
      ((validPromotionRate && promotionRate > tunables().pretenureThreshold() &&
        previousGC.nurseryUsedBytes >= 4 * 1024 * 1024) ||
       IsFullStoreBufferReason(reason));

  if (shouldPretenure) {
    JSContext* cx = rt->mainContextFromOwnThread();
    for (auto& entry : tenureCounts.entries) {
      if (entry.count >= tunables().pretenureGroupThreshold()) {
        ObjectGroup* group = entry.group;
        AutoMaybeLeaveAtomsZone leaveAtomsZone(cx);
        AutoRealm ar(cx, group);
        AutoSweepObjectGroup sweep(group);
        if (group->canPreTenure(sweep)) {
          group->setShouldPreTenure(sweep, cx);
          pretenureCount++;
        }
      }
    }
  }
  stats().setStat(gcstats::STAT_OBJECT_GROUPS_PRETENURED, pretenureCount);

  mozilla::Maybe<AutoGCSession> session;
  uint32_t numStringsTenured = 0;
  uint32_t numNurseryStringRealmsDisabled = 0;
  for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
    if (shouldPretenure && zone->allocNurseryStrings &&
        zone->tenuredStrings >= 30 * 1000) {
      if (!session.isSome()) {
        session.emplace(rt, JS::HeapState::MinorCollecting);
      }
      CancelOffThreadIonCompile(zone);
      bool preserving = zone->isPreservingCode();
      zone->setPreservingCode(false);
      zone->discardJitCode(rt->defaultFreeOp());
      zone->setPreservingCode(preserving);
      for (RealmsInZoneIter r(zone); !r.done(); r.next()) {
        if (jit::JitRealm* jitRealm = r->jitRealm()) {
          jitRealm->discardStubs();
          jitRealm->setStringsCanBeInNursery(false);
          numNurseryStringRealmsDisabled++;
        }
      }
      zone->allocNurseryStrings = false;
    }
    numStringsTenured += zone->tenuredStrings;
    zone->tenuredStrings = 0;
  }
  session.reset();  // End the minor GC session, if running one.
  stats().setStat(gcstats::STAT_NURSERY_STRING_REALMS_DISABLED,
                  numNurseryStringRealmsDisabled);
  stats().setStat(gcstats::STAT_STRINGS_TENURED, numStringsTenured);
  endProfile(ProfileKey::Pretenure);

  // We ignore gcMaxBytes when allocating for minor collection. However, if we
  // overflowed, we disable the nursery. The next time we allocate, we'll fail
  // because gcBytes >= gcMaxBytes.
  if (rt->gc.heapSize.gcBytes() >= tunables().gcMaxBytes()) {
    disable();
  }

  endProfile(ProfileKey::Total);
  rt->gc.incMinorGcNumber();

  TimeDuration totalTime = profileDurations_[ProfileKey::Total];
  rt->addTelemetry(JS_TELEMETRY_GC_MINOR_US, totalTime.ToMicroseconds());
  rt->addTelemetry(JS_TELEMETRY_GC_MINOR_REASON, uint32_t(reason));
  if (totalTime.ToMilliseconds() > 1.0) {
    rt->addTelemetry(JS_TELEMETRY_GC_MINOR_REASON_LONG, uint32_t(reason));
  }
  rt->addTelemetry(JS_TELEMETRY_GC_NURSERY_BYTES, committed());
  rt->addTelemetry(JS_TELEMETRY_GC_PRETENURE_COUNT, pretenureCount);
  rt->addTelemetry(JS_TELEMETRY_GC_NURSERY_PROMOTION_RATE, promotionRate * 100);

  stats().endNurseryCollection(reason);
  gcTracer.traceMinorGCEnd();
  timeInChunkAlloc_ = mozilla::TimeDuration();

  if (enableProfiling_ && totalTime >= profileThreshold_) {
    stats().maybePrintProfileHeaders();

    fprintf(stderr, "MinorGC: %20s %5.1f%% %5zu       ",
            JS::ExplainGCReason(reason), promotionRate * 100,
            capacity() / 1024);
    printProfileDurations(profileDurations_);

    if (reportTenurings_) {
      for (auto& entry : tenureCounts.entries) {
        if (entry.count >= reportTenurings_) {
          fprintf(stderr, "  %d x ", entry.count);
          AutoSweepObjectGroup sweep(entry.group);
          entry.group->print(sweep);
        }
      }
    }
  }
}

void js::Nursery::doCollection(JS::GCReason reason,
                               TenureCountCache& tenureCounts) {
  JSRuntime* rt = runtime();
  AutoGCSession session(rt, JS::HeapState::MinorCollecting);
  AutoSetThreadIsPerformingGC performingGC;
  AutoStopVerifyingBarriers av(rt, false);
  AutoDisableProxyCheck disableStrictProxyChecking;
  mozilla::DebugOnly<AutoEnterOOMUnsafeRegion> oomUnsafeRegion;

  const size_t initialNurseryCapacity = capacity();
  const size_t initialNurseryUsedBytes = usedSpace();

  // Move objects pointed to by roots from the nursery to the major heap.
  TenuringTracer mover(rt, this);

  // Mark the store buffer. This must happen first.
  StoreBuffer& sb = runtime()->gc.storeBuffer();

  // The MIR graph only contains nursery pointers if cancelIonCompilations()
  // is set on the store buffer, in which case we cancel all compilations
  // of such graphs.
  startProfile(ProfileKey::CancelIonCompilations);
  if (sb.cancelIonCompilations()) {
    js::CancelOffThreadIonCompilesUsingNurseryPointers(rt);
  }
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
  rt->gc.traceRuntimeForMinorGC(&mover, session);
  endProfile(ProfileKey::MarkRuntime);

  startProfile(ProfileKey::MarkDebugger);
  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_ROOTS);
    Debugger::traceAllForMovingGC(&mover);
  }
  endProfile(ProfileKey::MarkDebugger);

  startProfile(ProfileKey::SweepCaches);
  rt->gc.purgeRuntimeForMinorGC();
  endProfile(ProfileKey::SweepCaches);

  // Most of the work is done here. This loop iterates over objects that have
  // been moved to the major heap. If these objects have any outgoing pointers
  // to the nursery, then those nursery objects get moved as well, until no
  // objects are left to move. That is, we iterate to a fixed point.
  startProfile(ProfileKey::CollectToFP);
  collectToFixedPoint(mover, tenureCounts);
  endProfile(ProfileKey::CollectToFP);

  // Sweep to update any pointers to nursery objects that have now been
  // tenured.
  startProfile(ProfileKey::Sweep);
  sweep(&mover);
  endProfile(ProfileKey::Sweep);

  // Update any slot or element pointers whose destination has been tenured.
  startProfile(ProfileKey::UpdateJitActivations);
  js::jit::UpdateJitActivationsForMinorGC(rt);
  forwardedBuffers.clearAndCompact();
  endProfile(ProfileKey::UpdateJitActivations);

  startProfile(ProfileKey::ObjectsTenuredCallback);
  rt->gc.callObjectsTenuredCallback();
  endProfile(ProfileKey::ObjectsTenuredCallback);

  // Sweep.
  startProfile(ProfileKey::FreeMallocedBuffers);
  rt->gc.queueBuffersForFreeAfterMinorGC(mallocedBuffers);
  endProfile(ProfileKey::FreeMallocedBuffers);

  startProfile(ProfileKey::ClearNursery);
  clear();
  endProfile(ProfileKey::ClearNursery);

  startProfile(ProfileKey::ClearStoreBuffer);
  runtime()->gc.storeBuffer().clear();
  endProfile(ProfileKey::ClearStoreBuffer);

  // Make sure hashtables have been updated after the collection.
  startProfile(ProfileKey::CheckHashTables);
#ifdef JS_GC_ZEAL
  if (rt->hasZealMode(ZealMode::CheckHashTablesOnMinorGC)) {
    CheckHashTablesAfterMovingGC(rt);
  }
#endif
  endProfile(ProfileKey::CheckHashTables);

  previousGC.reason = reason;
  previousGC.nurseryCapacity = initialNurseryCapacity;
  previousGC.nurseryCommitted = spaceToEnd(allocatedChunkCount());
  previousGC.nurseryUsedBytes = initialNurseryUsedBytes;
  previousGC.tenuredBytes = mover.tenuredSize;
  previousGC.tenuredCells = mover.tenuredCells;
}

bool js::Nursery::registerMallocedBuffer(void* buffer) {
  MOZ_ASSERT(buffer);
  return mallocedBuffers.putNew(buffer);
}

void js::Nursery::sweep(JSTracer* trc) {
  // Sweep unique IDs first before we sweep any tables that may be keyed based
  // on them.
  for (Cell* cell : cellsWithUid_) {
    JSObject* obj = static_cast<JSObject*>(cell);
    if (!IsForwarded(obj)) {
      obj->zone()->removeUniqueId(obj);
    } else {
      JSObject* dst = Forwarded(obj);
      dst->zone()->transferUniqueId(dst, obj);
    }
  }
  cellsWithUid_.clear();

  for (CompartmentsIter c(runtime()); !c.done(); c.next()) {
    c->sweepAfterMinorGC(trc);
  }

  sweepDictionaryModeObjects();
  sweepMapAndSetObjects();
}

void js::Nursery::clear() {
#if defined(JS_GC_ZEAL) || defined(JS_CRASH_DIAGNOSTICS)
  /* Poison the nursery contents so touching a freed object will crash. */
  for (unsigned i = currentStartChunk_; i < currentChunk_; ++i) {
    chunk(i).poisonAfterSweep();
  }
  MOZ_ASSERT(maxChunkCount() > 0);
  chunk(currentChunk_)
      .poisonAfterSweep(position() - chunk(currentChunk_).start());
#endif

  if (runtime()->hasZealMode(ZealMode::GenerationalGC)) {
    /* Only reset the alloc point when we are close to the end. */
    if (currentChunk_ + 1 == maxChunkCount()) {
      setCurrentChunk(0, true);
    } else {
      // poisonAfterSweep poisons the chunk trailer. Ensure it's
      // initialized.
      chunk(currentChunk_).poisonAndInit(runtime());
    }
  } else {
    setCurrentChunk(0);
  }

  /* Set current start position for isEmpty checks. */
  setStartPosition();
}

size_t js::Nursery::spaceToEnd(unsigned chunkCount) const {
  if (chunkCount == 0) {
    return 0;
  }

  unsigned lastChunk = chunkCount - 1;

  MOZ_ASSERT(lastChunk >= currentStartChunk_);
  MOZ_ASSERT(currentStartPosition_ - chunk(currentStartChunk_).start() <=
             NurseryChunkUsableSize);

  size_t bytes;

  if (chunkCount != 1) {
    // In the general case we have to add:
    //  + the bytes used in the first
    //    chunk which may be less than the total size of a chunk since in some
    //    zeal modes we start the first chunk at some later position
    //    (currentStartPosition_).
    //  + the size of all the other chunks.
    bytes = (chunk(currentStartChunk_).end() - currentStartPosition_) +
            ((lastChunk - currentStartChunk_) * ChunkSize);
  } else {
    // In sub-chunk mode, but it also works whenever chunkCount == 1, we need to
    // use currentEnd_ since it may not refer to a full chunk.
    bytes = currentEnd_ - currentStartPosition_;
  }

  MOZ_ASSERT(bytes <= maxChunkCount() * ChunkSize);

  return bytes;
}

MOZ_ALWAYS_INLINE void js::Nursery::setCurrentChunk(unsigned chunkno,
                                                    bool fullPoison) {
  MOZ_ASSERT(chunkno < chunkCountLimit());
  MOZ_ASSERT(chunkno < allocatedChunkCount());

  if (!fullPoison && chunkno == currentChunk_ &&
      position_ < chunk(chunkno).end() && position_ >= chunk(chunkno).start()) {
    // When we setup a new chunk the whole chunk must be poisoned with the
    // correct value (JS_FRESH_NURSERY_PATTERN).
    //  1. The first time it was used it was fully poisoned with the
    //     correct value.
    //  2. When it is swept, only the used part is poisoned with the swept
    //     value.
    //  3. We repoison the swept part here, with the correct value.
    chunk(chunkno).poisonAndInit(runtime(), position_ - chunk(chunkno).start());
  } else {
    chunk(chunkno).poisonAndInit(runtime());
  }

  currentChunk_ = chunkno;
  position_ = chunk(chunkno).start();
  setCurrentEnd();
}

MOZ_ALWAYS_INLINE void js::Nursery::setCurrentEnd() {
  MOZ_ASSERT_IF(isSubChunkMode(),
                currentChunk_ == 0 && currentEnd_ <= chunk(0).end());
  currentEnd_ =
      chunk(currentChunk_).start() + Min(capacity_, NurseryChunkUsableSize);
  if (canAllocateStrings_) {
    currentStringEnd_ = currentEnd_;
  }
}

bool js::Nursery::allocateNextChunk(const unsigned chunkno,
                                    AutoLockGCBgAlloc& lock) {
  const unsigned priorCount = allocatedChunkCount();
  const unsigned newCount = priorCount + 1;

  MOZ_ASSERT((chunkno == currentChunk_ + 1) ||
             (chunkno == 0 && allocatedChunkCount() == 0));
  MOZ_ASSERT(chunkno == allocatedChunkCount());
  MOZ_ASSERT(chunkno < chunkCountLimit());

  if (!chunks_.resize(newCount)) {
    return false;
  }

  Chunk* newChunk;
  newChunk = runtime()->gc.getOrAllocChunk(lock);
  if (!newChunk) {
    chunks_.shrinkTo(priorCount);
    return false;
  }

  chunks_[chunkno] = NurseryChunk::fromChunk(newChunk);
  return true;
}

MOZ_ALWAYS_INLINE void js::Nursery::setStartPosition() {
  currentStartChunk_ = currentChunk_;
  currentStartPosition_ = position();
}

void js::Nursery::maybeResizeNursery(JS::GCReason reason) {
  if (maybeResizeExact(reason)) {
    return;
  }

  /*
   * This incorrect promotion rate results in better nursery sizing
   * decisions, however we should to better tuning based on the real
   * promotion rate in the future.
   */
  const float promotionRate =
      float(previousGC.tenuredBytes) / float(previousGC.nurseryCapacity);

  /*
   * Object lifetimes aren't going to behave linearly, but a better
   * relationship that works for all programs and can be predicted in
   * advance doesn't exist.
   */
  static const float GrowThreshold = 0.03f;
  static const float ShrinkThreshold = 0.01f;
  static const float PromotionGoal = (GrowThreshold + ShrinkThreshold) / 2.0f;
  const float factor = promotionRate / PromotionGoal;
  MOZ_ASSERT(factor >= 0.0f);

  MOZ_ASSERT((float(capacity()) * factor) <= SIZE_MAX);
  size_t newCapacity = size_t(float(capacity()) * factor);

  const size_t minNurseryBytes = roundSize(tunables().gcMinNurseryBytes());
  MOZ_ASSERT(minNurseryBytes >= ArenaSize);

  // If one of these conditions is true then we always shrink or grow the
  // nursery.  This way the thresholds still have an effect even if the goal
  // seeking says the current size is ideal.
  size_t lowLimit = Max(minNurseryBytes, capacity() / 2);
  size_t highLimit =
      Min((CheckedInt<size_t>(chunkCountLimit()) * ChunkSize).value(),
          (CheckedInt<size_t>(capacity()) * 2).value());
  newCapacity = roundSize(mozilla::Clamp(newCapacity, lowLimit, highLimit));

  if (maxChunkCount() < chunkCountLimit() && promotionRate > GrowThreshold &&
      newCapacity > capacity()) {
    growAllocableSpace(newCapacity);
  } else if (capacity() >= minNurseryBytes + SubChunkStep &&
             promotionRate < ShrinkThreshold && newCapacity < capacity()) {
    shrinkAllocableSpace(newCapacity);
  }
}

bool js::Nursery::maybeResizeExact(JS::GCReason reason) {
  // Disable the nursery if the user changed the configuration setting. The
  // nursery can only be re-enabled by resetting the configuration and
  // restarting firefox.
  if (tunables().gcMaxNurseryBytes() == 0) {
    disable();
    return true;
  }

  // Shrink the nursery to its minimum size of we ran out of memory or
  // received a memory pressure event.
  if (gc::IsOOMReason(reason)) {
    minimizeAllocableSpace();
    return true;
  }

#ifdef JS_GC_ZEAL
  // This zeal mode disabled nursery resizing.
  if (runtime()->hasZealMode(ZealMode::GenerationalGC)) {
    return true;
  }
#endif

  CheckedInt<unsigned> newMaxNurseryChunksChecked =
      (JS_ROUND(CheckedInt<size_t>(tunables().gcMaxNurseryBytes()), ChunkSize) /
       ChunkSize)
          .toChecked<unsigned>();
  if (!newMaxNurseryChunksChecked.isValid()) {
    // The above calculation probably overflowed (I don't think it can
    // underflow).
    newMaxNurseryChunksChecked = 1;
  }
  unsigned newMaxNurseryChunks = newMaxNurseryChunksChecked.value();
  if (newMaxNurseryChunks == 0) {
    // The above code rounded down, but don't round down all the way to zero.
    newMaxNurseryChunks = 1;
  }
  if (newMaxNurseryChunks != chunkCountLimit_) {
    chunkCountLimit_ = newMaxNurseryChunks;
    /* The configured maximum nursery size is changing */
    if (JS_HOWMANY(capacity_, gc::ChunkSize) > newMaxNurseryChunks) {
      /* We need to shrink the nursery */
      static_assert(NurseryChunkUsableSize < ChunkSize,
                    "Usable size must be smaller than total size or this "
                    "calculation might overflow");
      shrinkAllocableSpace(newMaxNurseryChunks * ChunkSize);
      return true;
    }
  }

  const size_t minNurseryBytes = roundSize(tunables().gcMinNurseryBytes());
  MOZ_ASSERT(minNurseryBytes >= ArenaSize);

  if (minNurseryBytes > capacity()) {
    /*
     * the configured minimum nursery size is changing and we need to grow the
     * nursery
     */
    MOZ_ASSERT(minNurseryBytes <= roundSize(tunables().gcMaxNurseryBytes()));
    growAllocableSpace(minNurseryBytes);
    return true;
  }

  return false;
}

size_t js::Nursery::roundSize(size_t size) const {
  if (size >= ChunkSize) {
    size = JS_ROUND(size, ChunkSize);
  } else {
    size = Min(JS_ROUND(size, SubChunkStep),
               JS_ROUNDDOWN(NurseryChunkUsableSize, SubChunkStep));
  }
  return size;
}

void js::Nursery::growAllocableSpace(size_t newCapacity) {
  MOZ_ASSERT_IF(!isSubChunkMode(), newCapacity > currentChunk_ * ChunkSize);
  MOZ_ASSERT(newCapacity <= chunkCountLimit_ * ChunkSize);
  capacity_ = newCapacity;
  setCurrentEnd();
}

void js::Nursery::freeChunksFrom(unsigned firstFreeChunk) {
  MOZ_ASSERT(firstFreeChunk < chunks_.length());
  {
    AutoLockGC lock(runtime());
    for (unsigned i = firstFreeChunk; i < chunks_.length(); i++) {
      runtime()->gc.recycleChunk(chunk(i).toChunk(runtime()), lock);
    }
  }
  chunks_.shrinkTo(firstFreeChunk);
}

void js::Nursery::shrinkAllocableSpace(size_t newCapacity) {
#ifdef JS_GC_ZEAL
  if (runtime()->hasZealMode(ZealMode::GenerationalGC)) {
    return;
  }
#endif

  // Don't shrink the nursery to zero (use Nursery::disable() instead)
  // This can't happen due to the rounding-down performed above because of the
  // clamping in maybeResizeNursery().
  MOZ_ASSERT(newCapacity != 0);
  // Don't attempt to shrink it to the same size.
  if (newCapacity == capacity_) {
    return;
  }
  MOZ_ASSERT(newCapacity < capacity_);

  unsigned newCount = JS_HOWMANY(newCapacity, ChunkSize);
  if (newCount < allocatedChunkCount()) {
    freeChunksFrom(newCount);
  }

  capacity_ = newCapacity;
  MOZ_ASSERT(capacity_ >= ArenaSize);
  setCurrentEnd();
}

void js::Nursery::minimizeAllocableSpace() {
  shrinkAllocableSpace(tunables().gcMinNurseryBytes());
}

bool js::Nursery::queueDictionaryModeObjectToSweep(NativeObject* obj) {
  MOZ_ASSERT(IsInsideNursery(obj));
  return dictionaryModeObjects_.append(obj);
}

uintptr_t js::Nursery::currentEnd() const {
  // These are separate asserts because it can be useful to see which one
  // failed.
  MOZ_ASSERT_IF(isSubChunkMode(), currentChunk_ == 0);
  MOZ_ASSERT_IF(isSubChunkMode(), currentEnd_ <= chunk(currentChunk_).end());
  MOZ_ASSERT_IF(!isSubChunkMode(), currentEnd_ == chunk(currentChunk_).end());
  MOZ_ASSERT(currentEnd_ != chunk(currentChunk_).start());
  return currentEnd_;
}

gcstats::Statistics& js::Nursery::stats() const {
  return runtime()->gc.stats();
}

MOZ_ALWAYS_INLINE const js::gc::GCSchedulingTunables& js::Nursery::tunables()
    const {
  return runtime()->gc.tunables;
}

bool js::Nursery::isSubChunkMode() const {
  return capacity() <= NurseryChunkUsableSize;
}

void js::Nursery::sweepDictionaryModeObjects() {
  for (auto obj : dictionaryModeObjects_) {
    if (!IsForwarded(obj)) {
      obj->sweepDictionaryListPointer();
    } else {
      Forwarded(obj)->updateDictionaryListPointerAfterMinorGC(obj);
    }
  }
  dictionaryModeObjects_.clear();
}

void js::Nursery::sweepMapAndSetObjects() {
  auto fop = runtime_->defaultFreeOp();

  for (auto mapobj : mapsWithNurseryMemory_) {
    MapObject::sweepAfterMinorGC(fop, mapobj);
  }
  mapsWithNurseryMemory_.clearAndFree();

  for (auto setobj : setsWithNurseryMemory_) {
    SetObject::sweepAfterMinorGC(fop, setobj);
  }
  setsWithNurseryMemory_.clearAndFree();
}

JS_PUBLIC_API void JS::EnableNurseryStrings(JSContext* cx) {
  AutoEmptyNursery empty(cx);
  ReleaseAllJITCode(cx->runtime()->defaultFreeOp());
  cx->runtime()->gc.nursery().enableStrings();
}

JS_PUBLIC_API void JS::DisableNurseryStrings(JSContext* cx) {
  AutoEmptyNursery empty(cx);
  ReleaseAllJITCode(cx->runtime()->defaultFreeOp());
  cx->runtime()->gc.nursery().disableStrings();
}

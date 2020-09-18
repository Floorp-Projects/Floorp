/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Nursery-inl.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "builtin/MapObject.h"
#include "debugger/DebugAPI.h"
#include "gc/FreeOp.h"
#include "gc/GCInternals.h"
#include "gc/GCLock.h"
#include "gc/Memory.h"
#include "gc/PublicIterators.h"
#include "jit/JitFrames.h"
#include "jit/JitRealm.h"
#include "util/Poison.h"
#include "vm/ArrayObject.h"
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

#ifdef JS_GC_ZEAL
constexpr uint32_t CanaryMagicValue = 0xDEADB15D;

struct alignas(gc::CellAlignBytes) js::Nursery::Canary {
  uint32_t magicValue;
  Canary* next;
};
#endif

namespace js {
struct NurseryChunk {
  char data[Nursery::NurseryChunkUsableSize];
  gc::ChunkTrailer trailer;
  static NurseryChunk* fromChunk(gc::Chunk* chunk);
  void poisonAndInit(JSRuntime* rt, size_t size = ChunkSize);
  void poisonRange(size_t from, size_t size, uint8_t value,
                   MemCheckKind checkKind);
  void poisonAfterEvict(size_t extent = ChunkSize);

  // The end of the range is always ChunkSize - ArenaSize.
  void markPagesUnusedHard(size_t from);
  // The start of the range is always the beginning of the chunk.
  MOZ_MUST_USE bool markPagesInUseHard(size_t to);

  uintptr_t start() const { return uintptr_t(&data); }
  uintptr_t end() const { return uintptr_t(&trailer); }
  gc::Chunk* toChunk(GCRuntime* gc);
};
static_assert(sizeof(js::NurseryChunk) == gc::ChunkSize,
              "Nursery chunk size must match gc::Chunk size.");

}  // namespace js

inline void js::NurseryChunk::poisonAndInit(JSRuntime* rt, size_t size) {
  poisonRange(0, size, JS_FRESH_NURSERY_PATTERN, MemCheckKind::MakeUndefined);
  MOZ_MAKE_MEM_UNDEFINED(&trailer, sizeof(trailer));
  new (&trailer) gc::ChunkTrailer(rt, &rt->gc.storeBuffer());
}

inline void js::NurseryChunk::poisonRange(size_t from, size_t size,
                                          uint8_t value,
                                          MemCheckKind checkKind) {
  MOZ_ASSERT(from <= js::Nursery::NurseryChunkUsableSize);
  MOZ_ASSERT(from + size <= ChunkSize);

  auto* start = reinterpret_cast<uint8_t*>(this) + from;

  // We can poison the same chunk more than once, so first make sure memory
  // sanitizers will let us poison it.
  MOZ_MAKE_MEM_UNDEFINED(start, size);
  Poison(start, value, size, checkKind);
}

inline void js::NurseryChunk::poisonAfterEvict(size_t extent) {
  MOZ_ASSERT(extent <= ChunkSize);
  poisonRange(0, extent, JS_SWEPT_NURSERY_PATTERN, MemCheckKind::MakeNoAccess);
}

inline void js::NurseryChunk::markPagesUnusedHard(size_t from) {
  MOZ_ASSERT(from < ChunkSize - ArenaSize);
  MarkPagesUnusedHard(reinterpret_cast<void*>(start() + from),
                      ChunkSize - ArenaSize - from);
}

inline bool js::NurseryChunk::markPagesInUseHard(size_t to) {
  MOZ_ASSERT(to <= ChunkSize - ArenaSize);
  return MarkPagesInUseHard(reinterpret_cast<void*>(start()), to);
}

// static
inline js::NurseryChunk* js::NurseryChunk::fromChunk(Chunk* chunk) {
  return reinterpret_cast<NurseryChunk*>(chunk);
}

inline Chunk* js::NurseryChunk::toChunk(GCRuntime* gc) {
  auto* chunk = reinterpret_cast<Chunk*>(this);
  chunk->init(gc);
  return chunk;
}

void js::NurseryDecommitTask::queueChunk(
    NurseryChunk* nchunk, const AutoLockHelperThreadState& lock) {
  // Using the chunk pointers to build the queue is infallible.
  Chunk* chunk = nchunk->toChunk(gc);
  chunk->info.prev = nullptr;
  chunk->info.next = queue;
  queue = chunk;
}

void js::NurseryDecommitTask::queueRange(
    size_t newCapacity, NurseryChunk& newChunk,
    const AutoLockHelperThreadState& lock) {
  MOZ_ASSERT(!partialChunk || partialChunk == &newChunk);

  // Only save this to decommit later if there's at least one page to
  // decommit.
  if (RoundUp(newCapacity, SystemPageSize()) >=
      RoundDown(Nursery::NurseryChunkUsableSize, SystemPageSize())) {
    // Clear the existing decommit request because it may be a larger request
    // for the same chunk.
    partialChunk = nullptr;
    return;
  }
  partialChunk = &newChunk;
  partialCapacity = newCapacity;
}

Chunk* js::NurseryDecommitTask::popChunk(
    const AutoLockHelperThreadState& lock) {
  if (!queue) {
    return nullptr;
  }

  Chunk* chunk = queue;
  queue = chunk->info.next;
  chunk->info.next = nullptr;
  MOZ_ASSERT(chunk->info.prev == nullptr);
  return chunk;
}

void js::NurseryDecommitTask::run(AutoLockHelperThreadState& lock) {
  Chunk* chunk;
  while ((chunk = popChunk(lock)) || partialChunk) {
    if (chunk) {
      AutoUnlockHelperThreadState unlock(lock);
      decommitChunk(chunk);
      continue;
    }

    if (partialChunk) {
      decommitRange(lock);
      continue;
    }
  }

  setFinishing(lock);
}

void js::NurseryDecommitTask::decommitChunk(Chunk* chunk) {
  chunk->decommitAllArenas();
  {
    AutoLockGC lock(gc);
    gc->recycleChunk(chunk, lock);
  }
}

void js::NurseryDecommitTask::decommitRange(AutoLockHelperThreadState& lock) {
  // Clear this field here before releasing the lock. While the lock is
  // released the main thread may make new decommit requests or update the range
  // of the current requested chunk, but it won't attempt to use any
  // might-be-decommitted-soon memory.
  NurseryChunk* thisPartialChunk = partialChunk;
  size_t thisPartialCapacity = partialCapacity;
  partialChunk = nullptr;
  {
    AutoUnlockHelperThreadState unlock(lock);
    thisPartialChunk->markPagesUnusedHard(thisPartialCapacity);
  }
}

js::Nursery::Nursery(GCRuntime* gc)
    : gc(gc),
      position_(0),
      currentStartChunk_(0),
      currentStartPosition_(0),
      currentEnd_(0),
      currentStringEnd_(0),
      currentBigIntEnd_(0),
      currentChunk_(0),
      capacity_(0),
      timeInChunkAlloc_(0),
      profileThreshold_(0),
      enableProfiling_(false),
      canAllocateStrings_(true),
      canAllocateBigInts_(true),
      reportTenurings_(0),
      minorGCTriggerReason_(JS::GCReason::NO_REASON),
#ifndef JS_MORE_DETERMINISTIC
      smoothedGrowthFactor(1.0),
#endif
      decommitTask(gc)
#ifdef JS_GC_ZEAL
      ,
      lastCanary_(nullptr)
#endif
{
  const char* env = getenv("MOZ_NURSERY_STRINGS");
  if (env && *env) {
    canAllocateStrings_ = (*env == '1');
  }
  env = getenv("MOZ_NURSERY_BIGINTS");
  if (env && *env) {
    canAllocateBigInts_ = (*env == '1');
  }
}

bool js::Nursery::init(AutoLockGCBgAlloc& lock) {
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

  if (!gc->storeBuffer().enable()) {
    return false;
  }

  return initFirstChunk(lock);
}

js::Nursery::~Nursery() { disable(); }

void js::Nursery::enable() {
  MOZ_ASSERT(isEmpty());
  MOZ_ASSERT(!gc->isVerifyPreBarriersEnabled());
  if (isEnabled()) {
    return;
  }

  {
    AutoLockGCBgAlloc lock(gc);
    if (!initFirstChunk(lock)) {
      // If we fail to allocate memory, the nursery will not be enabled.
      return;
    }
  }

#ifdef JS_GC_ZEAL
  if (gc->hasZealMode(ZealMode::GenerationalGC)) {
    enterZealMode();
  }
#endif

  // This should always succeed after the first time it's called.
  MOZ_ALWAYS_TRUE(gc->storeBuffer().enable());
}

bool js::Nursery::initFirstChunk(AutoLockGCBgAlloc& lock) {
  MOZ_ASSERT(!isEnabled());

  capacity_ = tunables().gcMinNurseryBytes();
  if (!allocateNextChunk(0, lock)) {
    capacity_ = 0;
    return false;
  }

  setCurrentChunk(0);
  setStartPosition();
  poisonAndInitCurrentChunk();

  // Clear any information about previous collections.
  clearRecentGrowthData();

  return true;
}

void js::Nursery::disable() {
  stringDeDupSet.reset();
  MOZ_ASSERT(isEmpty());
  if (!isEnabled()) {
    return;
  }

  // Freeing the chunks must not race with decommitting part of one of our
  // chunks. So join the decommitTask here and also below.
  decommitTask.join();
  freeChunksFrom(0);
  capacity_ = 0;

  // We must reset currentEnd_ so that there is no space for anything in the
  // nursery. JIT'd code uses this even if the nursery is disabled.
  currentEnd_ = 0;
  currentStringEnd_ = 0;
  currentBigIntEnd_ = 0;
  position_ = 0;
  gc->storeBuffer().disable();

  decommitTask.join();
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

void js::Nursery::enableBigInts() {
  MOZ_ASSERT(isEmpty());
  canAllocateBigInts_ = true;
  currentBigIntEnd_ = currentEnd_;
}

void js::Nursery::disableBigInts() {
  MOZ_ASSERT(isEmpty());
  canAllocateBigInts_ = false;
  currentBigIntEnd_ = 0;
}

bool js::Nursery::isEmpty() const {
  if (!isEnabled()) {
    return true;
  }

  if (!gc->hasZealMode(ZealMode::GenerationalGC)) {
    MOZ_ASSERT(currentStartChunk_ == 0);
    MOZ_ASSERT(currentStartPosition_ == chunk(0).start());
  }
  return position() == currentStartPosition_;
}

#ifdef JS_GC_ZEAL
void js::Nursery::enterZealMode() {
  if (isEnabled()) {
    MOZ_ASSERT(isEmpty());
    if (isSubChunkMode()) {
      // The poisoning call below must not race with background decommit,
      // which could be attempting to decommit the currently-unused part of this
      // chunk.
      decommitTask.join();
      {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!chunk(0).markPagesInUseHard(ChunkSize - ArenaSize)) {
          oomUnsafe.crash("Out of memory trying to extend chunk for zeal mode");
        }
      }

      // It'd be simpler to poison the whole chunk, but we can't do that
      // because the nursery might be partially used.
      chunk(0).poisonRange(capacity_, NurseryChunkUsableSize - capacity_,
                           JS_FRESH_NURSERY_PATTERN,
                           MemCheckKind::MakeUndefined);
    }
    capacity_ = RoundUp(tunables().gcMaxNurseryBytes(), ChunkSize);
    setCurrentEnd();
  }
}

void js::Nursery::leaveZealMode() {
  if (isEnabled()) {
    MOZ_ASSERT(isEmpty());
    setCurrentChunk(0);
    setStartPosition();
    poisonAndInitCurrentChunk();
  }
}
#endif  // JS_GC_ZEAL

JSObject* js::Nursery::allocateObject(JSContext* cx, size_t size,
                                      size_t nDynamicSlots,
                                      const JSClass* clasp) {
  // Ensure there's enough space to replace the contents with a
  // RelocationOverlay.
  MOZ_ASSERT(size >= sizeof(RelocationOverlay));

  // Sanity check the finalizer.
  MOZ_ASSERT_IF(clasp->hasFinalize(),
                CanNurseryAllocateFinalizedClass(clasp) || clasp->isProxy());

  auto* obj = reinterpret_cast<JSObject*>(
      allocateCell(cx->zone(), size, JS::TraceKind::Object));
  if (!obj) {
    return nullptr;
  }

  // If we want external slots, add them.
  ObjectSlots* slotsHeader = nullptr;
  if (nDynamicSlots) {
    MOZ_ASSERT(clasp->isNative());
    void* allocation =
        allocateBuffer(cx->zone(), ObjectSlots::allocSize(nDynamicSlots));
    if (!allocation) {
      // It is safe to leave the allocated object uninitialized, since we
      // do not visit unallocated things in the nursery.
      return nullptr;
    }
    slotsHeader = new (allocation) ObjectSlots(nDynamicSlots, 0);
  }

  // Store slots pointer directly in new object. If no dynamic slots were
  // requested, caller must initialize slots_ field itself as needed. We
  // don't know if the caller was a native object or not.
  if (nDynamicSlots) {
    static_cast<NativeObject*>(obj)->initSlots(slotsHeader->slots());
  }

  gcprobes::NurseryAlloc(obj, size);
  return obj;
}

Cell* js::Nursery::allocateCell(Zone* zone, size_t size, JS::TraceKind kind) {
  // Ensure there's enough space to replace the contents with a
  // RelocationOverlay.
  MOZ_ASSERT(size >= sizeof(RelocationOverlay));
  MOZ_ASSERT(size % CellAlignBytes == 0);

  void* ptr = allocate(sizeof(NurseryCellHeader) + size);
  if (!ptr) {
    return nullptr;
  }

  new (ptr) NurseryCellHeader(zone, kind);

  auto cell =
      reinterpret_cast<Cell*>(uintptr_t(ptr) + sizeof(NurseryCellHeader));
  gcprobes::NurseryAlloc(cell, kind);
  return cell;
}

inline void* js::Nursery::allocate(size_t size) {
  MOZ_ASSERT(isEnabled());
  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
  MOZ_ASSERT_IF(currentChunk_ == currentStartChunk_,
                position() >= currentStartPosition_);
  MOZ_ASSERT(position() % CellAlignBytes == 0);
  MOZ_ASSERT(size % CellAlignBytes == 0);

#ifdef JS_GC_ZEAL
  if (gc->hasZealMode(ZealMode::CheckNursery)) {
    size += sizeof(Canary);
  }
#endif

  if (MOZ_UNLIKELY(currentEnd() < position() + size)) {
    return moveToNextChunkAndAllocate(size);
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
  if (gc->hasZealMode(ZealMode::CheckNursery)) {
    writeCanary(position() - sizeof(Canary));
  }
#endif

  return thing;
}

void* Nursery::moveToNextChunkAndAllocate(size_t size) {
  MOZ_ASSERT(currentEnd() < position() + size);

  unsigned chunkno = currentChunk_ + 1;
  MOZ_ASSERT(chunkno <= maxChunkCount());
  MOZ_ASSERT(chunkno <= allocatedChunkCount());
  if (chunkno == maxChunkCount()) {
    return nullptr;
  }
  if (chunkno == allocatedChunkCount()) {
    mozilla::TimeStamp start = ReallyNow();
    {
      AutoLockGCBgAlloc lock(gc);
      if (!allocateNextChunk(chunkno, lock)) {
        return nullptr;
      }
    }
    timeInChunkAlloc_ += ReallyNow() - start;
    MOZ_ASSERT(chunkno < allocatedChunkCount());
  }
  setCurrentChunk(chunkno);
  poisonAndInitCurrentChunk();

  // We know there's enough space to allocate now so we can call allocate()
  // recursively. Adjust the size for the nursery canary which it will add on.
  MOZ_ASSERT(currentEnd() >= position() + size);
#ifdef JS_GC_ZEAL
  if (gc->hasZealMode(ZealMode::CheckNursery)) {
    size -= sizeof(Canary);
  }
#endif
  return allocate(size);
}

#ifdef JS_GC_ZEAL
inline void Nursery::writeCanary(uintptr_t address) {
  auto* canary = reinterpret_cast<Canary*>(address);
  new (canary) Canary{CanaryMagicValue, nullptr};
  if (lastCanary_) {
    MOZ_ASSERT(!lastCanary_->next);
    lastCanary_->next = canary;
  }
  lastCanary_ = canary;
}
#endif

void* js::Nursery::allocateBuffer(Zone* zone, size_t nbytes) {
  MOZ_ASSERT(nbytes > 0);

  if (nbytes <= MaxNurseryBufferSize) {
    void* buffer = allocate(nbytes);
    if (buffer) {
      return buffer;
    }
  }

  void* buffer = zone->pod_malloc<uint8_t>(nbytes);
  if (buffer && !registerMallocedBuffer(buffer, nbytes)) {
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

  void* buffer = zone->pod_arena_calloc<uint8_t>(arena, nbytes);
  if (buffer && !registerMallocedBuffer(buffer, nbytes)) {
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
    return obj->zone()->pod_arena_calloc<uint8_t>(arena, nbytes);
  }
  return allocateZeroedBuffer(obj->zone(), nbytes, arena);
}

void* js::Nursery::reallocateBuffer(Zone* zone, Cell* cell, void* oldBuffer,
                                    size_t oldBytes, size_t newBytes) {
  if (!IsInsideNursery(cell)) {
    return zone->pod_realloc<uint8_t>((uint8_t*)oldBuffer, oldBytes, newBytes);
  }

  if (!isInside(oldBuffer)) {
    MOZ_ASSERT(mallocedBufferBytes >= oldBytes);
    void* newBuffer =
        zone->pod_realloc<uint8_t>((uint8_t*)oldBuffer, oldBytes, newBytes);
    if (newBuffer) {
      if (oldBuffer != newBuffer) {
        MOZ_ALWAYS_TRUE(
            mallocedBuffers.rekeyAs(oldBuffer, newBuffer, newBuffer));
      }
      mallocedBufferBytes -= oldBytes;
      mallocedBufferBytes += newBytes;
    }
    return newBuffer;
  }

  // The nursery cannot make use of the returned slots data.
  if (newBytes < oldBytes) {
    return oldBuffer;
  }

  void* newBuffer = allocateBuffer(zone, newBytes);
  if (newBuffer) {
    PodCopy((uint8_t*)newBuffer, (uint8_t*)oldBuffer, oldBytes);
  }
  return newBuffer;
}

void* js::Nursery::allocateBuffer(JS::BigInt* bi, size_t nbytes) {
  MOZ_ASSERT(bi);
  MOZ_ASSERT(nbytes > 0);

  if (!IsInsideNursery(bi)) {
    return bi->zone()->pod_malloc<uint8_t>(nbytes);
  }
  return allocateBuffer(bi->zone(), nbytes);
}

void js::Nursery::freeBuffer(void* buffer, size_t nbytes) {
  if (!isInside(buffer)) {
    removeMallocedBuffer(buffer, nbytes);
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
  auto* vPtr = reinterpret_cast<volatile uint64_t*>(ptr);
  *vPtr = *vPtr;
  return true;
}
#endif

void js::Nursery::forwardBufferPointer(uintptr_t* pSlotsElems) {
  // Read the current pointer value which may be one of:
  //  - Non-nursery pointer
  //  - Nursery-allocated buffer
  //  - A BufferRelocationOverlay inside the nursery
  //
  // Note: The buffer has already be relocated. We are just patching stale
  //       pointers now.
  auto* buffer = reinterpret_cast<void*>(*pSlotsElems);

  if (!isInside(buffer)) {
    return;
  }

  // The new location for this buffer is either stored inline with it or in
  // the forwardedBuffers table.
  if (ForwardedBufferMap::Ptr p = forwardedBuffers.lookup(buffer)) {
    buffer = p->value();
    // It's not valid to assert IsWriteableAddress for indirect forwarding
    // pointers because the size of the allocation could be less than a word.
  } else {
    BufferRelocationOverlay* reloc =
        static_cast<BufferRelocationOverlay*>(buffer);
    buffer = *reloc;
    MOZ_ASSERT(IsWriteableAddress(buffer));
  }

  MOZ_ASSERT(!isInside(buffer));
  *pSlotsElems = reinterpret_cast<uintptr_t>(buffer);
}

js::TenuringTracer::TenuringTracer(JSRuntime* rt, Nursery* nursery)
    : JSTracer(rt, JSTracer::TracerKindTag::Tenuring, TraceWeakMapKeysValues),
      nursery_(*nursery),
      tenuredSize(0),
      tenuredCells(0),
      objHead(nullptr),
      objTail(&objHead),
      stringHead(nullptr),
      stringTail(&stringHead),
      bigIntHead(nullptr),
      bigIntTail(&bigIntHead) {}

inline double js::Nursery::calcPromotionRate(bool* validForTenuring) const {
  double used = double(previousGC.nurseryUsedBytes);
  double capacity = double(previousGC.nurseryCapacity);
  double tenured = double(previousGC.tenuredBytes);
  double rate;

  if (previousGC.nurseryUsedBytes > 0) {
    if (validForTenuring) {
      // We can only use promotion rates if they're likely to be valid,
      // they're only valid if the nursery was at least 90% full.
      *validForTenuring = used > capacity * 0.9;
    }
    rate = tenured / used;
  } else {
    if (validForTenuring) {
      *validForTenuring = false;
    }
    rate = 0.0;
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
  json.property("bigints_tenured",
                stats().getStat(gcstats::STAT_BIGINTS_TENURED));
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
  if (stats().getStat(gcstats::STAT_NURSERY_BIGINT_REALMS_DISABLED)) {
    json.property(
        "nursery_bigint_realms_disabled",
        stats().getStat(gcstats::STAT_NURSERY_BIGINT_REALMS_DISABLED));
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

// static
void js::Nursery::printProfileHeader() {
  fprintf(stderr, "MinorGC: Timestamp  Reason               PRate  Size ");
#define PRINT_HEADER(name, text) fprintf(stderr, " %6s", text);
  FOR_EACH_NURSERY_PROFILE_TIME(PRINT_HEADER)
#undef PRINT_HEADER
  fprintf(stderr, "\n");
}

// static
void js::Nursery::printProfileDurations(const ProfileDurations& times) {
  for (auto time : times) {
    fprintf(stderr, " %6" PRIi64, static_cast<int64_t>(time.ToMicroseconds()));
  }
  fprintf(stderr, "\n");
}

void js::Nursery::printTotalProfileTimes() {
  if (enableProfiling_) {
    fprintf(stderr,
            "MinorGC TOTALS: %7" PRIu64 " collections:                 ",
            gc->minorGCCount());
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
  if (isEmpty()) {
    return false;
  }

  if (minorGCRequested()) {
    return true;
  }

  bool belowBytesThreshold =
      freeSpace() < tunables().nurseryFreeThresholdForIdleCollection();
  bool belowFractionThreshold =
      double(freeSpace()) / double(capacity()) <
      tunables().nurseryFreeThresholdForIdleCollectionFraction();

  // We want to use belowBytesThreshold when the nursery is sufficiently large,
  // and belowFractionThreshold when it's small.
  //
  // When the nursery is small then belowBytesThreshold is a lower threshold
  // (triggered earlier) than belowFractionThreshold. So if the fraction
  // threshold is true, the bytes one will be true also. The opposite is true
  // when the nursery is large.
  //
  // Therefore, by the time we cross the threshold we care about, we've already
  // crossed the other one, and we can boolean AND to use either condition
  // without encoding any "is the nursery big/small" test/threshold. The point
  // at which they cross is when the nursery is: BytesThreshold /
  // FractionThreshold large.
  //
  // With defaults that's:
  //
  //   1MB = 256KB / 0.25
  //
  return belowBytesThreshold && belowFractionThreshold;
}

// typeReason is the gcReason for specified type, for example,
// FULL_CELL_PTR_OBJ_BUFFER is the gcReason for JSObject.
static inline bool IsFullStoreBufferReason(JS::GCReason reason,
                                           JS::GCReason typeReason) {
  return reason == typeReason ||
         reason == JS::GCReason::FULL_WHOLE_CELL_BUFFER ||
         reason == JS::GCReason::FULL_GENERIC_BUFFER ||
         reason == JS::GCReason::FULL_VALUE_BUFFER ||
         reason == JS::GCReason::FULL_SLOT_BUFFER ||
         reason == JS::GCReason::FULL_SHAPE_BUFFER;
}

void js::Nursery::collect(JSGCInvocationKind kind, JS::GCReason reason) {
  JSRuntime* rt = runtime();
  MOZ_ASSERT(!rt->mainContextFromOwnThread()->suppressGC);

  if (!isEnabled() || isEmpty()) {
    // Our barriers are not always exact, and there may be entries in the
    // storebuffer even when the nursery is disabled or empty. It's not safe
    // to keep these entries as they may refer to tenured cells which may be
    // freed after this point.
    gc->storeBuffer().clear();
  }

  if (!isEnabled()) {
    return;
  }

#ifdef JS_GC_ZEAL
  if (gc->hasZealMode(ZealMode::CheckNursery)) {
    for (auto canary = lastCanary_; canary; canary = canary->next) {
      MOZ_ASSERT(canary->magicValue == CanaryMagicValue);
    }
  }
  lastCanary_ = nullptr;
#endif

  stats().beginNurseryCollection(reason);
  gcprobes::MinorGCStart();

  stringDeDupSet.emplace();
  auto guardStringDedupSet =
      mozilla::MakeScopeExit([&] { stringDeDupSet.reset(); });

  maybeClearProfileDurations();
  startProfile(ProfileKey::Total);

  // The analysis marks TenureCount as not problematic for GC hazards because
  // it is only used here, and ObjectGroup pointers are never
  // nursery-allocated.
  MOZ_ASSERT(!IsNurseryAllocable(AllocKind::OBJECT_GROUP));

  previousGC.reason = JS::GCReason::NO_REASON;
  previousGC.nurseryUsedBytes = usedSpace();
  previousGC.nurseryCapacity = capacity();
  previousGC.nurseryCommitted = committed();
  previousGC.tenuredBytes = 0;
  previousGC.tenuredCells = 0;

  // If it isn't empty, it will call doCollection, and possibly after that
  // isEmpty() will become true, so use another variable to keep track of the
  // old empty state.
  bool wasEmpty = isEmpty();
  TenureCountCache tenureCounts;
  if (!wasEmpty) {
    CollectionResult result = doCollection(reason, tenureCounts);
    previousGC.reason = reason;
    previousGC.tenuredBytes = result.tenuredBytes;
    previousGC.tenuredCells = result.tenuredCells;
  }

  // Resize the nursery.
  maybeResizeNursery(kind, reason);

  // Poison/initialise the first chunk.
  if (previousGC.nurseryUsedBytes) {
    // In most cases Nursery::clear() has not poisoned this chunk or marked it
    // as NoAccess; so we only need to poison the region used during the last
    // cycle.  Also, if the heap was recently expanded we don't want to
    // re-poison the new memory.  In both cases we only need to poison until
    // previousGC.nurseryUsedBytes.
    //
    // In cases where this is not true, like generational zeal mode or subchunk
    // mode, poisonAndInitCurrentChunk() will ignore its parameter.  It will
    // also clamp the parameter.
    poisonAndInitCurrentChunk(previousGC.nurseryUsedBytes);
  }

  bool validPromotionRate;
  const double promotionRate = calcPromotionRate(&validPromotionRate);
  bool highPromotionRate =
      validPromotionRate && promotionRate > tunables().pretenureThreshold();

  startProfile(ProfileKey::Pretenure);
  size_t pretenureCount =
      doPretenuring(rt, reason, tenureCounts, highPromotionRate);
  endProfile(ProfileKey::Pretenure);

  // We ignore gcMaxBytes when allocating for minor collection. However, if we
  // overflowed, we disable the nursery. The next time we allocate, we'll fail
  // because bytes >= gcMaxBytes.
  if (gc->heapSize.bytes() >= tunables().gcMaxBytes()) {
    disable();
  }

  endProfile(ProfileKey::Total);
  gc->incMinorGcNumber();

  TimeDuration totalTime = profileDurations_[ProfileKey::Total];
  sendTelemetry(reason, totalTime, wasEmpty, pretenureCount, promotionRate);

  stats().endNurseryCollection(reason);
  gcprobes::MinorGCEnd();

  timeInChunkAlloc_ = mozilla::TimeDuration();

  if (enableProfiling_ && totalTime >= profileThreshold_) {
    printCollectionProfile(reason, promotionRate);
  }

  if (reportTenurings_) {
    printTenuringData(tenureCounts);
  }
}

void js::Nursery::sendTelemetry(JS::GCReason reason, TimeDuration totalTime,
                                bool wasEmpty, size_t pretenureCount,
                                double promotionRate) {
  JSRuntime* rt = runtime();
  rt->addTelemetry(JS_TELEMETRY_GC_MINOR_REASON, uint32_t(reason));
  if (totalTime.ToMilliseconds() > 1.0) {
    rt->addTelemetry(JS_TELEMETRY_GC_MINOR_REASON_LONG, uint32_t(reason));
  }
  rt->addTelemetry(JS_TELEMETRY_GC_MINOR_US, totalTime.ToMicroseconds());
  rt->addTelemetry(JS_TELEMETRY_GC_NURSERY_BYTES, committed());

  if (!wasEmpty) {
    rt->addTelemetry(JS_TELEMETRY_GC_PRETENURE_COUNT_2, pretenureCount);
    rt->addTelemetry(JS_TELEMETRY_GC_NURSERY_PROMOTION_RATE,
                     promotionRate * 100);
  }
}

void js::Nursery::printCollectionProfile(JS::GCReason reason,
                                         double promotionRate) {
  stats().maybePrintProfileHeaders();

  TimeDuration ts = startTimes_[ProfileKey::Total] - stats().creationTime();

  fprintf(stderr, "MinorGC: %10.6f %-20.20s %5.1f%% %5zu", ts.ToSeconds(),
          JS::ExplainGCReason(reason), promotionRate * 100,
          previousGC.nurseryCapacity / 1024);

  printProfileDurations(profileDurations_);
}

void js::Nursery::printTenuringData(const TenureCountCache& tenureCounts) {
  for (const auto& entry : tenureCounts.entries) {
    if (entry.count >= reportTenurings_) {
      fprintf(stderr, "  %u x ", entry.count);
      AutoSweepObjectGroup sweep(entry.group);
      entry.group->print(sweep);
    }
  }
}

js::Nursery::CollectionResult js::Nursery::doCollection(
    JS::GCReason reason, TenureCountCache& tenureCounts) {
  JSRuntime* rt = runtime();
  AutoGCSession session(gc, JS::HeapState::MinorCollecting);
  AutoSetThreadIsPerformingGC performingGC;
  AutoStopVerifyingBarriers av(rt, false);
  AutoDisableProxyCheck disableStrictProxyChecking;
  mozilla::DebugOnly<AutoEnterOOMUnsafeRegion> oomUnsafeRegion;

  // Move objects pointed to by roots from the nursery to the major heap.
  TenuringTracer mover(rt, this);

  // Mark the store buffer. This must happen first.
  StoreBuffer& sb = gc->storeBuffer();

  // The MIR graph only contains nursery pointers if cancelIonCompilations()
  // is set on the store buffer, in which case we cancel all compilations
  // of such graphs.
  startProfile(ProfileKey::CancelIonCompilations);
  if (sb.cancelIonCompilations()) {
    js::CancelOffThreadIonCompilesUsingNurseryPointers(rt);
  }
  endProfile(ProfileKey::CancelIonCompilations);

  // Strings in the whole cell buffer must be traced first, in order to mark
  // tenured dependent strings' bases as non-deduplicatable. The rest of
  // nursery collection (whole non-string cells, edges, etc.) can happen later.
  startProfile(ProfileKey::TraceWholeCells);
  sb.traceWholeCells(mover);
  endProfile(ProfileKey::TraceWholeCells);

  startProfile(ProfileKey::TraceValues);
  sb.traceValues(mover);
  endProfile(ProfileKey::TraceValues);

  startProfile(ProfileKey::TraceCells);
  sb.traceCells(mover);
  endProfile(ProfileKey::TraceCells);

  startProfile(ProfileKey::TraceSlots);
  sb.traceSlots(mover);
  endProfile(ProfileKey::TraceSlots);

  startProfile(ProfileKey::TraceGenericEntries);
  sb.traceGenericEntries(&mover);
  endProfile(ProfileKey::TraceGenericEntries);

  startProfile(ProfileKey::MarkRuntime);
  gc->traceRuntimeForMinorGC(&mover, session);
  endProfile(ProfileKey::MarkRuntime);

  startProfile(ProfileKey::MarkDebugger);
  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_ROOTS);
    DebugAPI::traceAllForMovingGC(&mover);
  }
  endProfile(ProfileKey::MarkDebugger);

  startProfile(ProfileKey::SweepCaches);
  gc->purgeRuntimeForMinorGC();
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
  gc->callObjectsTenuredCallback();
  endProfile(ProfileKey::ObjectsTenuredCallback);

  // Sweep.
  startProfile(ProfileKey::FreeMallocedBuffers);
  gc->queueBuffersForFreeAfterMinorGC(mallocedBuffers);
  mallocedBufferBytes = 0;
  endProfile(ProfileKey::FreeMallocedBuffers);

  startProfile(ProfileKey::ClearNursery);
  clear();
  endProfile(ProfileKey::ClearNursery);

  startProfile(ProfileKey::ClearStoreBuffer);
  gc->storeBuffer().clear();
  endProfile(ProfileKey::ClearStoreBuffer);

  // Purge the StringToAtomCache. This has to happen at the end because the
  // cache is used when tenuring strings.
  startProfile(ProfileKey::PurgeStringToAtomCache);
  runtime()->caches().stringToAtomCache.purge();
  endProfile(ProfileKey::PurgeStringToAtomCache);

  // Make sure hashtables have been updated after the collection.
  startProfile(ProfileKey::CheckHashTables);
#ifdef JS_GC_ZEAL
  if (gc->hasZealMode(ZealMode::CheckHashTablesOnMinorGC)) {
    gc->checkHashTablesAfterMovingGC();
  }
#endif
  endProfile(ProfileKey::CheckHashTables);

  return {mover.tenuredSize, mover.tenuredCells};
}

size_t js::Nursery::doPretenuring(JSRuntime* rt, JS::GCReason reason,
                                  const TenureCountCache& tenureCounts,
                                  bool highPromotionRate) {
  // If we are promoting the nursery, or exhausted the store buffer with
  // pointers to nursery things, which will force a collection well before
  // the nursery is full, look for object groups that are getting promoted
  // excessively and try to pretenure them.

  bool pretenureObj = false;
  bool pretenureStr = false;
  bool pretenureBigInt = false;
  if (tunables().attemptPretenuring()) {
    // Should we check for pretenuring regardless of GCReason?
    bool pretenureAll =
        highPromotionRate && previousGC.nurseryUsedBytes >= 4 * 1024 * 1024;

    pretenureObj =
        pretenureAll ||
        IsFullStoreBufferReason(reason, JS::GCReason::FULL_CELL_PTR_OBJ_BUFFER);
    pretenureStr =
        pretenureAll ||
        IsFullStoreBufferReason(reason, JS::GCReason::FULL_CELL_PTR_STR_BUFFER);
    pretenureBigInt =
        pretenureAll || IsFullStoreBufferReason(
                            reason, JS::GCReason::FULL_CELL_PTR_BIGINT_BUFFER);
  }

  size_t pretenureCount = 0;

  if (pretenureObj) {
    JSContext* cx = rt->mainContextFromOwnThread();
    uint32_t threshold = tunables().pretenureGroupThreshold();
    for (auto& entry : tenureCounts.entries) {
      if (entry.count < threshold) {
        continue;
      }

      ObjectGroup* group = entry.group;
      AutoRealm ar(cx, group);
      AutoSweepObjectGroup sweep(group);
      if (group->canPreTenure(sweep)) {
        group->setShouldPreTenure(sweep, cx);
        pretenureCount++;
      }
    }
  }
  stats().setStat(gcstats::STAT_OBJECT_GROUPS_PRETENURED, pretenureCount);

  mozilla::Maybe<AutoGCSession> session;
  uint32_t numStringsTenured = 0;
  uint32_t numNurseryStringRealmsDisabled = 0;
  uint32_t numBigIntsTenured = 0;
  uint32_t numNurseryBigIntRealmsDisabled = 0;
  for (ZonesIter zone(gc, SkipAtoms); !zone.done(); zone.next()) {
    bool disableNurseryStrings = pretenureStr && zone->allocNurseryStrings &&
                                 zone->tenuredStrings >= 30 * 1000;
    bool disableNurseryBigInts = pretenureBigInt && zone->allocNurseryBigInts &&
                                 zone->tenuredBigInts >= 30 * 1000;
    if (disableNurseryStrings || disableNurseryBigInts) {
      if (!session.isSome()) {
        session.emplace(gc, JS::HeapState::MinorCollecting);
      }
      CancelOffThreadIonCompile(zone);
      bool preserving = zone->isPreservingCode();
      zone->setPreservingCode(false);
      zone->discardJitCode(rt->defaultFreeOp());
      zone->setPreservingCode(preserving);
      for (RealmsInZoneIter r(zone); !r.done(); r.next()) {
        if (jit::JitRealm* jitRealm = r->jitRealm()) {
          jitRealm->discardStubs();
          if (disableNurseryStrings) {
            jitRealm->setStringsCanBeInNursery(false);
            numNurseryStringRealmsDisabled++;
          }
          if (disableNurseryBigInts) {
            numNurseryBigIntRealmsDisabled++;
          }
        }
      }
      if (disableNurseryStrings) {
        zone->allocNurseryStrings = false;
      }
      if (disableNurseryBigInts) {
        zone->allocNurseryBigInts = false;
      }
    }
    numStringsTenured += zone->tenuredStrings;
    zone->tenuredStrings = 0;
    numBigIntsTenured += zone->tenuredBigInts;
    zone->tenuredBigInts = 0;
  }
  session.reset();  // End the minor GC session, if running one.
  stats().setStat(gcstats::STAT_NURSERY_STRING_REALMS_DISABLED,
                  numNurseryStringRealmsDisabled);
  stats().setStat(gcstats::STAT_STRINGS_TENURED, numStringsTenured);
  stats().setStat(gcstats::STAT_NURSERY_BIGINT_REALMS_DISABLED,
                  numNurseryBigIntRealmsDisabled);
  stats().setStat(gcstats::STAT_BIGINTS_TENURED, numBigIntsTenured);

  return pretenureCount;
}

bool js::Nursery::registerMallocedBuffer(void* buffer, size_t nbytes) {
  MOZ_ASSERT(buffer);
  MOZ_ASSERT(nbytes > 0);
  if (!mallocedBuffers.putNew(buffer)) {
    return false;
  }

  mallocedBufferBytes += nbytes;
  if (MOZ_UNLIKELY(mallocedBufferBytes > capacity() * 8)) {
    requestMinorGC(JS::GCReason::NURSERY_MALLOC_BUFFERS);
  }

  return true;
}

void js::Nursery::sweep(JSTracer* trc) {
  // Sweep unique IDs first before we sweep any tables that may be keyed based
  // on them.
  for (Cell* cell : cellsWithUid_) {
    auto* obj = static_cast<JSObject*>(cell);
    if (!IsForwarded(obj)) {
      obj->nurseryZone()->removeUniqueId(obj);
    } else {
      JSObject* dst = Forwarded(obj);
      obj->nurseryZone()->transferUniqueId(dst, obj);
    }
  }
  cellsWithUid_.clear();

  for (CompartmentsIter c(runtime()); !c.done(); c.next()) {
    c->sweepAfterMinorGC(trc);
  }

  for (ZonesIter zone(trc->runtime(), SkipAtoms); !zone.done(); zone.next()) {
    zone->sweepAfterMinorGC(trc);
  }

  sweepDictionaryModeObjects();
  sweepMapAndSetObjects();
}

void js::Nursery::clear() {
  // Poison the nursery contents so touching a freed object will crash.
  unsigned firstClearChunk;
  if (gc->hasZealMode(ZealMode::GenerationalGC)) {
    // Poison all the chunks used in this cycle. The new start chunk is
    // reposioned in Nursery::collect() but there's no point optimising that in
    // this case.
    firstClearChunk = currentStartChunk_;
  } else {
    // In normal mode we start at the second chunk, the first one will be used
    // in the next cycle and poisoned in Nusery::collect();
    MOZ_ASSERT(currentStartChunk_ == 0);
    firstClearChunk = 1;
  }
  for (unsigned i = firstClearChunk; i < currentChunk_; ++i) {
    chunk(i).poisonAfterEvict();
  }
  // Clear only the used part of the chunk because that's the part we touched,
  // but only if it's not going to be re-used immediately (>= firstClearChunk).
  if (currentChunk_ >= firstClearChunk) {
    chunk(currentChunk_)
        .poisonAfterEvict(position() - chunk(currentChunk_).start());
  }

  // Reset the start chunk & position if we're not in this zeal mode, or we're
  // in it and close to the end of the nursery.
  MOZ_ASSERT(maxChunkCount() > 0);
  if (!gc->hasZealMode(ZealMode::GenerationalGC) ||
      (gc->hasZealMode(ZealMode::GenerationalGC) &&
       currentChunk_ + 1 == maxChunkCount())) {
    setCurrentChunk(0);
  }

  // Set current start position for isEmpty checks.
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

MOZ_ALWAYS_INLINE void js::Nursery::setCurrentChunk(unsigned chunkno) {
  MOZ_ASSERT(chunkno < allocatedChunkCount());

  currentChunk_ = chunkno;
  position_ = chunk(chunkno).start();
  setCurrentEnd();
}

void js::Nursery::poisonAndInitCurrentChunk(size_t extent) {
  if (gc->hasZealMode(ZealMode::GenerationalGC) || !isSubChunkMode()) {
    chunk(currentChunk_).poisonAndInit(runtime());
  } else {
    extent = std::min(capacity_, extent);
    MOZ_ASSERT(extent <= NurseryChunkUsableSize);
    chunk(currentChunk_).poisonAndInit(runtime(), extent);
  }
}

MOZ_ALWAYS_INLINE void js::Nursery::setCurrentEnd() {
  MOZ_ASSERT_IF(isSubChunkMode(),
                currentChunk_ == 0 && currentEnd_ <= chunk(0).end());
  currentEnd_ = chunk(currentChunk_).start() +
                std::min({capacity_, NurseryChunkUsableSize});
  if (canAllocateStrings_) {
    currentStringEnd_ = currentEnd_;
  }
  if (canAllocateBigInts_) {
    currentBigIntEnd_ = currentEnd_;
  }
}

bool js::Nursery::allocateNextChunk(const unsigned chunkno,
                                    AutoLockGCBgAlloc& lock) {
  const unsigned priorCount = allocatedChunkCount();
  const unsigned newCount = priorCount + 1;

  MOZ_ASSERT((chunkno == currentChunk_ + 1) ||
             (chunkno == 0 && allocatedChunkCount() == 0));
  MOZ_ASSERT(chunkno == allocatedChunkCount());
  MOZ_ASSERT(chunkno < HowMany(capacity(), ChunkSize));

  if (!chunks_.resize(newCount)) {
    return false;
  }

  Chunk* newChunk;
  newChunk = gc->getOrAllocChunk(lock);
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

void js::Nursery::maybeResizeNursery(JSGCInvocationKind kind,
                                     JS::GCReason reason) {
#ifdef JS_GC_ZEAL
  // This zeal mode disabled nursery resizing.
  if (gc->hasZealMode(ZealMode::GenerationalGC)) {
    return;
  }
#endif

  size_t newCapacity =
      mozilla::Clamp(targetSize(kind, reason), tunables().gcMinNurseryBytes(),
                     tunables().gcMaxNurseryBytes());

  MOZ_ASSERT(roundSize(newCapacity) == newCapacity);

  if (newCapacity > capacity()) {
    growAllocableSpace(newCapacity);
  } else if (newCapacity < capacity()) {
    shrinkAllocableSpace(newCapacity);
  }
}

static inline double ClampDouble(double value, double min, double max) {
  MOZ_ASSERT(!std::isnan(value) && !std::isnan(min) && !std::isnan(max));
  MOZ_ASSERT(max >= min);

  if (value <= min) {
    return min;
  }

  if (value >= max) {
    return max;
  }

  return value;
}

size_t js::Nursery::targetSize(JSGCInvocationKind kind, JS::GCReason reason) {
  // Shrink the nursery as much as possible if shrinking was requested or in low
  // memory situations.
  if (kind == GC_SHRINK || gc::IsOOMReason(reason) ||
      gc->systemHasLowMemory()) {
    clearRecentGrowthData();
    return 0;
  }

  // Don't resize the nursery during shutdown.
  if (gc::IsShutdownReason(reason)) {
    clearRecentGrowthData();
    return capacity();
  }

  TimeStamp now = ReallyNow();

  // Calculate the fraction of the nursery promoted out of its entire
  // capacity. This gives better results than using the promotion rate (based on
  // the amount of nursery used) in cases where we collect before the nursery is
  // full.
  double fractionPromoted =
      double(previousGC.tenuredBytes) / double(previousGC.nurseryCapacity);

  // Calculate the fraction of time spent collecting the nursery.
  double timeFraction = 0.0;
#ifndef JS_MORE_DETERMINISTIC
  if (lastResizeTime) {
    TimeDuration collectorTime = now - collectionStartTime();
    TimeDuration totalTime = now - lastResizeTime;
    timeFraction = collectorTime.ToSeconds() / totalTime.ToSeconds();
  }
#endif

  // Adjust the nursery size to try to achieve a target promotion rate and
  // collector time goals.
  static const double PromotionGoal = 0.02;
  static const double TimeGoal = 0.01;
  double growthFactor =
      std::max(fractionPromoted / PromotionGoal, timeFraction / TimeGoal);

  // Limit the range of the growth factor to prevent transient high promotion
  // rates from affecting the nursery size too far into the future.
  static const double GrowthRange = 2.0;
  growthFactor = ClampDouble(growthFactor, 1.0 / GrowthRange, GrowthRange);

#ifndef JS_MORE_DETERMINISTIC
  // Use exponential smoothing on the desired growth rate to take into account
  // the promotion rate from recent previous collections.
  if (lastResizeTime &&
      now - lastResizeTime < TimeDuration::FromMilliseconds(200)) {
    growthFactor = 0.75 * smoothedGrowthFactor + 0.25 * growthFactor;
  }

  lastResizeTime = now;
  smoothedGrowthFactor = growthFactor;
#endif

  // Leave size untouched if we are close to the promotion goal.
  static const double GoalWidth = 1.5;
  if (growthFactor > (1.0 / GoalWidth) && growthFactor < GoalWidth) {
    return capacity();
  }

  // The multiplication below cannot overflow because growthFactor is at
  // most two.
  MOZ_ASSERT(growthFactor <= 2.0);
  MOZ_ASSERT(capacity() < SIZE_MAX / 2);

  return roundSize(size_t(double(capacity()) * growthFactor));
}

void js::Nursery::clearRecentGrowthData() {
#ifndef JS_MORE_DETERMINISTIC
  lastResizeTime = TimeStamp();
  smoothedGrowthFactor = 1.0;
#endif
}

/* static */
size_t js::Nursery::roundSize(size_t size) {
  static_assert(SubChunkStep > gc::ChunkTrailerSize,
                "Don't allow the nursery to overwrite the trailer when using "
                "less than a chunk");

  size_t step = size >= ChunkSize ? ChunkSize : SubChunkStep;
  size = Round(size, step);

  MOZ_ASSERT(size >= ArenaSize);

  return size;
}

void js::Nursery::growAllocableSpace(size_t newCapacity) {
  MOZ_ASSERT_IF(!isSubChunkMode(), newCapacity > currentChunk_ * ChunkSize);
  MOZ_ASSERT(newCapacity <= tunables().gcMaxNurseryBytes());
  MOZ_ASSERT(newCapacity > capacity());

  if (isSubChunkMode()) {
    // Avoid growing into an area that's about to be decommitted.
    decommitTask.join();

    MOZ_ASSERT(currentChunk_ == 0);

    // The remainder of the chunk may have been decommitted.
    if (!chunk(0).markPagesInUseHard(
            std::min(newCapacity, ChunkSize - ArenaSize))) {
      // The OS won't give us the memory we need, we can't grow.
      return;
    }

    // The capacity has changed and since we were in sub-chunk mode we need to
    // update the poison values / asan infomation for the now-valid region of
    // this chunk.
    size_t poisonSize =
        std::min({newCapacity, NurseryChunkUsableSize}) - capacity();
    // Don't poison the trailer.
    MOZ_ASSERT(capacity() + poisonSize <= NurseryChunkUsableSize);
    chunk(0).poisonRange(capacity(), poisonSize, JS_FRESH_NURSERY_PATTERN,
                         MemCheckKind::MakeUndefined);
  }

  capacity_ = newCapacity;

  setCurrentEnd();
}

void js::Nursery::freeChunksFrom(const unsigned firstFreeChunk) {
  MOZ_ASSERT(firstFreeChunk < chunks_.length());

  // The loop below may need to skip the first chunk, so we may use this so we
  // can modify it.
  unsigned firstChunkToDecommit = firstFreeChunk;

  if ((firstChunkToDecommit == 0) && isSubChunkMode()) {
    // Part of the first chunk may be hard-decommitted, un-decommit it so that
    // the GC's normal chunk-handling doesn't segfault.
    MOZ_ASSERT(currentChunk_ == 0);
    if (!chunk(0).markPagesInUseHard(ChunkSize - ArenaSize)) {
      // Free the chunk if we can't allocate its pages.
      UnmapPages(static_cast<void*>(&chunk(0)), ChunkSize);
      firstChunkToDecommit = 1;
    }
  }

  {
    AutoLockHelperThreadState lock;
    for (size_t i = firstChunkToDecommit; i < chunks_.length(); i++) {
      decommitTask.queueChunk(chunks_[i], lock);
    }
    decommitTask.startOrRunIfIdle(lock);
  }

  chunks_.shrinkTo(firstFreeChunk);
}

void js::Nursery::shrinkAllocableSpace(size_t newCapacity) {
#ifdef JS_GC_ZEAL
  if (gc->hasZealMode(ZealMode::GenerationalGC)) {
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

  unsigned newCount = HowMany(newCapacity, ChunkSize);
  if (newCount < allocatedChunkCount()) {
    freeChunksFrom(newCount);
  }

  size_t oldCapacity = capacity_;
  capacity_ = newCapacity;

  setCurrentEnd();

  if (isSubChunkMode()) {
    MOZ_ASSERT(currentChunk_ == 0);
    chunk(0).poisonRange(
        newCapacity,
        std::min({oldCapacity, NurseryChunkUsableSize}) - newCapacity,
        JS_SWEPT_NURSERY_PATTERN, MemCheckKind::MakeNoAccess);

    AutoLockHelperThreadState lock;
    decommitTask.queueRange(capacity_, chunk(0), lock);
    decommitTask.startOrRunIfIdle(lock);
  }
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

gcstats::Statistics& js::Nursery::stats() const { return gc->stats(); }

MOZ_ALWAYS_INLINE const js::gc::GCSchedulingTunables& js::Nursery::tunables()
    const {
  return gc->tunables;
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
  auto fop = runtime()->defaultFreeOp();

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
  ReleaseAllJITCode(cx->defaultFreeOp());
  cx->runtime()->gc.nursery().enableStrings();
}

JS_PUBLIC_API void JS::DisableNurseryStrings(JSContext* cx) {
  AutoEmptyNursery empty(cx);
  ReleaseAllJITCode(cx->defaultFreeOp());
  cx->runtime()->gc.nursery().disableStrings();
}

JS_PUBLIC_API void JS::EnableNurseryBigInts(JSContext* cx) {
  AutoEmptyNursery empty(cx);
  ReleaseAllJITCode(cx->defaultFreeOp());
  cx->runtime()->gc.nursery().enableBigInts();
}

JS_PUBLIC_API void JS::DisableNurseryBigInts(JSContext* cx) {
  AutoEmptyNursery empty(cx);
  ReleaseAllJITCode(cx->defaultFreeOp());
  cx->runtime()->gc.nursery().disableBigInts();
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ArrayBufferObject-inl.h"
#include "vm/ArrayBufferObject.h"

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TaggedAnonymousMemory.h"

#include <algorithm>  // std::max, std::min
#include <memory>     // std::uninitialized_copy_n
#include <string.h>
#ifndef XP_WIN
#  include <sys/mman.h>
#endif
#include <tuple>  // std::tuple
#ifdef MOZ_VALGRIND
#  include <valgrind/memcheck.h>
#endif

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsnum.h"
#include "jstypes.h"

#include "builtin/Array.h"
#include "builtin/DataViewObject.h"
#include "gc/Barrier.h"
#include "gc/Memory.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/MemoryMetrics.h"
#include "js/PropertySpec.h"
#include "js/SharedArrayBuffer.h"
#include "js/Wrapper.h"
#include "util/Windows.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/SharedArrayObject.h"
#include "vm/Warnings.h"  // js::WarnNumberASCII
#include "vm/WrapperObject.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmTypes.h"

#include "gc/FreeOp-inl.h"
#include "gc/Marking-inl.h"
#include "gc/Nursery-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Shape-inl.h"

using JS::ToInt32;

using mozilla::Atomic;
using mozilla::CheckedInt;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;
using mozilla::Unused;

using namespace js;

// If there are too many wasm memory buffers (typically 6GB each) live we run up
// against system resource exhaustion (address space or number of memory map
// descriptors), see bug 1068684, bug 1073934, bug 1517412, bug 1502733 for
// details.  The limiting case seems to be Android on ARM64, where the
// per-process address space is limited to 4TB (39 bits) by the organization of
// the page tables.  An earlier problem was Windows Vista Home 64-bit, where the
// per-process address space is limited to 8TB (40 bits).
//
// Thus we track the number of live objects if we are using large mappings, and
// set a limit of the number of live buffer objects per process. We trigger GC
// work when we approach the limit and we throw an OOM error if the per-process
// limit is exceeded. The limit (MaximumLiveMappedBuffers) is specific to
// architecture, OS, and OS configuration.
//
// Since the MaximumLiveMappedBuffers limit is not generally accounted for by
// any existing GC-trigger heuristics, we need an extra heuristic for triggering
// GCs when the caller is allocating memories rapidly without other garbage.
// Thus, once the live buffer count crosses the threshold
// StartTriggeringAtLiveBufferCount, we start triggering GCs every
// AllocatedBuffersPerTrigger allocations.  Once we reach
// StartSyncFullGCAtLiveBufferCount live buffers, we perform expensive
// non-incremental full GCs as a last-ditch effort to avoid unnecessary failure.
// Once we reach MaximumLiveMappedBuffers, we perform further full GCs before
// giving up.

#if defined(JS_CODEGEN_ARM64) && defined(ANDROID)
// With 6GB mappings, the hard limit is 84 buffers.  75 cuts it close.
static const int32_t MaximumLiveMappedBuffers = 75;
#elif defined(MOZ_TSAN) || defined(MOZ_ASAN)
// ASAN and TSAN use a ton of vmem for bookkeeping leaving a lot less for the
// program so use a lower limit.
static const int32_t MaximumLiveMappedBuffers = 500;
#else
static const int32_t MaximumLiveMappedBuffers = 1000;
#endif

// StartTriggeringAtLiveBufferCount + AllocatedBuffersPerTrigger must be well
// below StartSyncFullGCAtLiveBufferCount in order to provide enough time for
// incremental GC to do its job.

#if defined(JS_CODEGEN_ARM64) && defined(ANDROID)
static const int32_t StartTriggeringAtLiveBufferCount = 15;
static const int32_t StartSyncFullGCAtLiveBufferCount =
    MaximumLiveMappedBuffers - 15;
static const int32_t AllocatedBuffersPerTrigger = 15;
#else
static const int32_t StartTriggeringAtLiveBufferCount = 100;
static const int32_t StartSyncFullGCAtLiveBufferCount =
    MaximumLiveMappedBuffers - 100;
static const int32_t AllocatedBuffersPerTrigger = 100;
#endif

static Atomic<int32_t, mozilla::ReleaseAcquire> liveBufferCount(0);
static Atomic<int32_t, mozilla::ReleaseAcquire> allocatedSinceLastTrigger(0);

int32_t js::LiveMappedBufferCount() { return liveBufferCount; }

void* js::MapBufferMemory(size_t mappedSize, size_t initialCommittedSize) {
  MOZ_ASSERT(mappedSize % gc::SystemPageSize() == 0);
  MOZ_ASSERT(initialCommittedSize % gc::SystemPageSize() == 0);
  MOZ_ASSERT(initialCommittedSize <= mappedSize);

  auto decrement = mozilla::MakeScopeExit([&] { liveBufferCount--; });
  if (wasm::IsHugeMemoryEnabled()) {
    liveBufferCount++;
  } else {
    decrement.release();
  }

  // Test >= to guard against the case where multiple extant runtimes
  // race to allocate.
  if (liveBufferCount >= MaximumLiveMappedBuffers) {
    if (OnLargeAllocationFailure) {
      OnLargeAllocationFailure();
    }
    if (liveBufferCount >= MaximumLiveMappedBuffers) {
      return nullptr;
    }
  }

#ifdef XP_WIN
  void* data = VirtualAlloc(nullptr, mappedSize, MEM_RESERVE, PAGE_NOACCESS);
  if (!data) {
    return nullptr;
  }

  if (!VirtualAlloc(data, initialCommittedSize, MEM_COMMIT, PAGE_READWRITE)) {
    VirtualFree(data, 0, MEM_RELEASE);
    return nullptr;
  }
#else   // XP_WIN
  void* data =
      MozTaggedAnonymousMmap(nullptr, mappedSize, PROT_NONE,
                             MAP_PRIVATE | MAP_ANON, -1, 0, "wasm-reserved");
  if (data == MAP_FAILED) {
    return nullptr;
  }

  // Note we will waste a page on zero-sized memories here
  if (mprotect(data, initialCommittedSize, PROT_READ | PROT_WRITE)) {
    munmap(data, mappedSize);
    return nullptr;
  }
#endif  // !XP_WIN

#if defined(MOZ_VALGRIND) && \
    defined(VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE)
  VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(
      (unsigned char*)data + initialCommittedSize,
      mappedSize - initialCommittedSize);
#endif

  decrement.release();
  return data;
}

bool js::CommitBufferMemory(void* dataEnd, uint32_t delta) {
  MOZ_ASSERT(delta);
  MOZ_ASSERT(delta % gc::SystemPageSize() == 0);

#ifdef XP_WIN
  if (!VirtualAlloc(dataEnd, delta, MEM_COMMIT, PAGE_READWRITE)) {
    return false;
  }
#else   // XP_WIN
  if (mprotect(dataEnd, delta, PROT_READ | PROT_WRITE)) {
    return false;
  }
#endif  // !XP_WIN

#if defined(MOZ_VALGRIND) && \
    defined(VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE)
  VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE((unsigned char*)dataEnd, delta);
#endif

  return true;
}

bool js::ExtendBufferMapping(void* dataPointer, size_t mappedSize,
                             size_t newMappedSize) {
  MOZ_ASSERT(mappedSize % gc::SystemPageSize() == 0);
  MOZ_ASSERT(newMappedSize % gc::SystemPageSize() == 0);
  MOZ_ASSERT(newMappedSize >= mappedSize);

#ifdef XP_WIN
  void* mappedEnd = (char*)dataPointer + mappedSize;
  uint32_t delta = newMappedSize - mappedSize;
  if (!VirtualAlloc(mappedEnd, delta, MEM_RESERVE, PAGE_NOACCESS)) {
    return false;
  }
  return true;
#elif defined(XP_LINUX)
  // Note this will not move memory (no MREMAP_MAYMOVE specified)
  if (MAP_FAILED == mremap(dataPointer, mappedSize, newMappedSize, 0)) {
    return false;
  }
  return true;
#else
  // No mechanism for remapping on MacOS and other Unices. Luckily
  // shouldn't need it here as most of these are 64-bit.
  return false;
#endif
}

void js::UnmapBufferMemory(void* base, size_t mappedSize) {
  MOZ_ASSERT(mappedSize % gc::SystemPageSize() == 0);

#ifdef XP_WIN
  VirtualFree(base, 0, MEM_RELEASE);
#else   // XP_WIN
  munmap(base, mappedSize);
#endif  // !XP_WIN

#if defined(MOZ_VALGRIND) && \
    defined(VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE)
  VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE((unsigned char*)base,
                                                mappedSize);
#endif

  if (wasm::IsHugeMemoryEnabled()) {
    // Decrement the buffer counter at the end -- otherwise, a race condition
    // could enable the creation of unlimited buffers.
    --liveBufferCount;
  }
}

/*
 * ArrayBufferObject
 *
 * This class holds the underlying raw buffer that the TypedArrayObject classes
 * access.  It can be created explicitly and passed to a TypedArrayObject, or
 * can be created implicitly by constructing a TypedArrayObject with a size.
 */

/*
 * ArrayBufferObject (base)
 */

static const JSClassOps ArrayBufferObjectClassOps = {
    nullptr,                      // addProperty
    nullptr,                      // delProperty
    nullptr,                      // enumerate
    nullptr,                      // newEnumerate
    nullptr,                      // resolve
    nullptr,                      // mayResolve
    ArrayBufferObject::finalize,  // finalize
    nullptr,                      // call
    nullptr,                      // hasInstance
    nullptr,                      // construct
    nullptr,                      // trace
};

static const JSFunctionSpec arraybuffer_functions[] = {
    JS_FN("isView", ArrayBufferObject::fun_isView, 1, 0), JS_FS_END};

static const JSPropertySpec arraybuffer_properties[] = {
    JS_SELF_HOSTED_SYM_GET(species, "$ArrayBufferSpecies", 0), JS_PS_END};

static const JSFunctionSpec arraybuffer_proto_functions[] = {
    JS_SELF_HOSTED_FN("slice", "ArrayBufferSlice", 2, 0), JS_FS_END};

static const JSPropertySpec arraybuffer_proto_properties[] = {
    JS_PSG("byteLength", ArrayBufferObject::byteLengthGetter, 0),
    JS_STRING_SYM_PS(toStringTag, "ArrayBuffer", JSPROP_READONLY), JS_PS_END};

static const ClassSpec ArrayBufferObjectClassSpec = {
    GenericCreateConstructor<ArrayBufferObject::class_constructor, 1,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<ArrayBufferObject>,
    arraybuffer_functions,
    arraybuffer_properties,
    arraybuffer_proto_functions,
    arraybuffer_proto_properties};

static const ClassExtension ArrayBufferObjectClassExtension = {
    ArrayBufferObject::objectMoved,  // objectMovedOp
};

const JSClass ArrayBufferObject::class_ = {
    "ArrayBuffer",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer) |
        JSCLASS_BACKGROUND_FINALIZE,
    &ArrayBufferObjectClassOps, &ArrayBufferObjectClassSpec,
    &ArrayBufferObjectClassExtension};

const JSClass ArrayBufferObject::protoClass_ = {
    "ArrayBufferPrototype", JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer),
    JS_NULL_CLASS_OPS, &ArrayBufferObjectClassSpec};

bool js::IsArrayBuffer(HandleValue v) {
  return v.isObject() && v.toObject().is<ArrayBufferObject>();
}

bool js::IsArrayBuffer(JSObject* obj) { return obj->is<ArrayBufferObject>(); }

ArrayBufferObject& js::AsArrayBuffer(JSObject* obj) {
  MOZ_ASSERT(IsArrayBuffer(obj));
  return obj->as<ArrayBufferObject>();
}

bool js::IsArrayBufferMaybeShared(HandleValue v) {
  return v.isObject() && v.toObject().is<ArrayBufferObjectMaybeShared>();
}

bool js::IsArrayBufferMaybeShared(JSObject* obj) {
  return obj->is<ArrayBufferObjectMaybeShared>();
}

ArrayBufferObjectMaybeShared& js::AsArrayBufferMaybeShared(JSObject* obj) {
  MOZ_ASSERT(IsArrayBufferMaybeShared(obj));
  return obj->as<ArrayBufferObjectMaybeShared>();
}

MOZ_ALWAYS_INLINE bool ArrayBufferObject::byteLengthGetterImpl(
    JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(IsArrayBuffer(args.thisv()));
  args.rval().setInt32(
      args.thisv().toObject().as<ArrayBufferObject>().byteLength());
  return true;
}

bool ArrayBufferObject::byteLengthGetter(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsArrayBuffer, byteLengthGetterImpl>(cx, args);
}

/*
 * ArrayBuffer.isView(obj); ES6 (Dec 2013 draft) 24.1.3.1
 */
bool ArrayBufferObject::fun_isView(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(args.get(0).isObject() &&
                         JS_IsArrayBufferViewObject(&args.get(0).toObject()));
  return true;
}

// ES2017 draft 24.1.2.1
bool ArrayBufferObject::class_constructor(JSContext* cx, unsigned argc,
                                          Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "ArrayBuffer")) {
    return false;
  }

  // Step 2.
  uint64_t byteLength;
  if (!ToIndex(cx, args.get(0), &byteLength)) {
    return false;
  }

  // Step 3 (Inlined 24.1.1.1 AllocateArrayBuffer).
  // 24.1.1.1, step 1 (Inlined 9.1.14 OrdinaryCreateFromConstructor).
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_ArrayBuffer,
                                          &proto)) {
    return false;
  }

  // 24.1.1.1, step 3 (Inlined 6.2.6.1 CreateByteDataBlock, step 2).
  // Refuse to allocate too large buffers, currently limited to ~2 GiB.
  if (byteLength > INT32_MAX) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_ARRAY_LENGTH);
    return false;
  }

  // 24.1.1.1, steps 1 and 4-6.
  JSObject* bufobj = createZeroed(cx, uint32_t(byteLength), proto);
  if (!bufobj) {
    return false;
  }
  args.rval().setObject(*bufobj);
  return true;
}

using ArrayBufferContents = UniquePtr<uint8_t[], JS::FreePolicy>;

static ArrayBufferContents AllocateUninitializedArrayBufferContents(
    JSContext* cx, uint32_t nbytes) {
  // First attempt a normal allocation.
  uint8_t* p =
      cx->maybe_pod_arena_malloc<uint8_t>(js::ArrayBufferContentsArena, nbytes);
  if (MOZ_UNLIKELY(!p)) {
    // Otherwise attempt a large allocation, calling the
    // large-allocation-failure callback if necessary.
    p = static_cast<uint8_t*>(cx->runtime()->onOutOfMemoryCanGC(
        js::AllocFunction::Malloc, js::ArrayBufferContentsArena, nbytes));
    if (!p) {
      ReportOutOfMemory(cx);
    }
  }

  return ArrayBufferContents(p);
}

static ArrayBufferContents AllocateArrayBufferContents(JSContext* cx,
                                                       uint32_t nbytes) {
  // First attempt a normal allocation.
  uint8_t* p =
      cx->maybe_pod_arena_calloc<uint8_t>(js::ArrayBufferContentsArena, nbytes);
  if (MOZ_UNLIKELY(!p)) {
    // Otherwise attempt a large allocation, calling the
    // large-allocation-failure callback if necessary.
    p = static_cast<uint8_t*>(cx->runtime()->onOutOfMemoryCanGC(
        js::AllocFunction::Calloc, js::ArrayBufferContentsArena, nbytes));
    if (!p) {
      ReportOutOfMemory(cx);
    }
  }

  return ArrayBufferContents(p);
}

static ArrayBufferContents NewCopiedBufferContents(
    JSContext* cx, Handle<ArrayBufferObject*> buffer) {
  ArrayBufferContents dataCopy =
      AllocateUninitializedArrayBufferContents(cx, buffer->byteLength());
  if (dataCopy) {
    if (auto count = buffer->byteLength()) {
      memcpy(dataCopy.get(), buffer->dataPointer(), count);
    }
  }
  return dataCopy;
}

/* static */
void ArrayBufferObject::detach(JSContext* cx,
                               Handle<ArrayBufferObject*> buffer) {
  cx->check(buffer);
  MOZ_ASSERT(!buffer->isPreparedForAsmJS());
  MOZ_ASSERT(!buffer->hasTypedObjectViews());

  auto NoteViewBufferWasDetached = [&cx](ArrayBufferViewObject* view) {
    MOZ_ASSERT(!view->isSharedMemory());

    view->notifyBufferDetached();

    // Notify compiled jit code that the base pointer has moved.
    MarkObjectStateChange(cx, view);
  };

  // Update all views of the buffer to account for the buffer having been
  // detached, and clear the buffer's data and list of views.
  //
  // Typed object buffers are not exposed and cannot be detached.

  auto& innerViews = ObjectRealm::get(buffer).innerViews.get();
  if (InnerViewTable::ViewVector* views =
          innerViews.maybeViewsUnbarriered(buffer)) {
    for (size_t i = 0; i < views->length(); i++) {
      JSObject* view = (*views)[i];
      NoteViewBufferWasDetached(&view->as<ArrayBufferViewObject>());
    }
    innerViews.removeViews(buffer);
  }
  if (JSObject* view = buffer->firstView()) {
    NoteViewBufferWasDetached(&view->as<ArrayBufferViewObject>());
    buffer->setFirstView(nullptr);
  }

  if (buffer->dataPointer()) {
    buffer->releaseData(cx->runtime()->defaultFreeOp());
    buffer->setDataPointer(BufferContents::createNoData());
  }

  buffer->setByteLength(0);
  buffer->setIsDetached();
}

/*
 * [SMDOC] WASM Linear Memory structure
 *
 * Wasm Raw Buf Linear Memory Structure
 *
 * The linear heap in Wasm is an mmaped array buffer. Several
 * constants manage its lifetime:
 *
 *  - length - the wasm-visible current length of the buffer. Accesses in the
 *    range [0, length] succeed. May only increase.
 *
 *  - boundsCheckLimit - the size against which we perform bounds checks. It is
 *    always a constant offset smaller than mappedSize. Currently that constant
 *    offset is 64k (wasm::GuardSize).
 *
 *  - maxSize - the optional declared limit on how much length can grow.
 *
 *  - mappedSize - the actual mmaped size. Access in the range
 *    [0, mappedSize] will either succeed, or be handled by the wasm signal
 *    handlers.
 *
 * The below diagram shows the layout of the wasm heap. The wasm-visible
 * portion of the heap starts at 0. There is one extra page prior to the
 * start of the wasm heap which contains the WasmArrayRawBuffer struct at
 * its end (i.e. right before the start of the WASM heap).
 *
 *  WasmArrayRawBuffer
 *      \    ArrayBufferObject::dataPointer()
 *       \  /
 *        \ |
 *  ______|_|____________________________________________________________
 * |______|_|______________|___________________|____________|____________|
 *          0          length              maxSize  boundsCheckLimit  mappedSize
 *
 * \_______________________/
 *          COMMITED
 *                          \____________________________________________/
 *                                           SLOP
 * \_____________________________________________________________________/
 *                         MAPPED
 *
 * Invariants:
 *  - length only increases
 *  - 0 <= length <= maxSize (if present) <= boundsCheckLimit <= mappedSize
 *  - on ARM boundsCheckLimit must be a valid ARM immediate.
 *  - if maxSize is not specified, boundsCheckLimit/mappedSize may grow. They
 *    are otherwise constant.
 *
 * NOTE: For asm.js on non-x64 we guarantee that
 *
 * length == maxSize == boundsCheckLimit == mappedSize
 *
 * That is, signal handlers will not be invoked, since they cannot emulate
 * asm.js accesses on non-x64 architectures.
 *
 * The region between length and mappedSize is the SLOP - an area where we use
 * signal handlers to catch things that slip by bounds checks. Logically it has
 * two parts:
 *
 *  - from length to boundsCheckLimit - this part of the SLOP serves to catch
 *  accesses to memory we have reserved but not yet grown into. This allows us
 *  to grow memory up to max (when present) without having to patch/update the
 *  bounds checks.
 *
 *  - from boundsCheckLimit to mappedSize - this part of the SLOP allows us to
 *  bounds check against base pointers and fold some constant offsets inside
 *  loads. This enables better Bounds Check Elimination.
 *
 */

MOZ_MUST_USE bool WasmArrayRawBuffer::growToSizeInPlace(uint32_t oldSize,
                                                        uint32_t newSize) {
  MOZ_ASSERT(newSize >= oldSize);
  MOZ_ASSERT_IF(maxSize(), newSize <= maxSize().value());
  MOZ_ASSERT(newSize <= mappedSize());

  uint32_t delta = newSize - oldSize;
  MOZ_ASSERT(delta % wasm::PageSize == 0);

  uint8_t* dataEnd = dataPointer() + oldSize;
  MOZ_ASSERT(uintptr_t(dataEnd) % gc::SystemPageSize() == 0);

  if (delta && !CommitBufferMemory(dataEnd, delta)) {
    return false;
  }

  length_ = newSize;

  return true;
}

bool WasmArrayRawBuffer::extendMappedSize(uint32_t maxSize) {
  size_t newMappedSize = wasm::ComputeMappedSize(maxSize);
  MOZ_ASSERT(mappedSize_ <= newMappedSize);
  if (mappedSize_ == newMappedSize) {
    return true;
  }

  if (!ExtendBufferMapping(dataPointer(), mappedSize_, newMappedSize)) {
    return false;
  }

  mappedSize_ = newMappedSize;
  return true;
}

void WasmArrayRawBuffer::tryGrowMaxSizeInPlace(uint32_t deltaMaxSize) {
  CheckedInt<uint32_t> newMaxSize = maxSize_.value();
  newMaxSize += deltaMaxSize;
  MOZ_ASSERT(newMaxSize.isValid());
  MOZ_ASSERT(newMaxSize.value() % wasm::PageSize == 0);

  if (!extendMappedSize(newMaxSize.value())) {
    return;
  }

  maxSize_ = Some(newMaxSize.value());
}

/* static */
WasmArrayRawBuffer* WasmArrayRawBuffer::Allocate(uint32_t numBytes,
                                                 const Maybe<uint32_t>& maxSize,
                                                 const Maybe<size_t>& mapped) {
  MOZ_RELEASE_ASSERT(numBytes <= ArrayBufferObject::MaxBufferByteLength);

  size_t mappedSize = mapped.isSome()
                          ? *mapped
                          : wasm::ComputeMappedSize(maxSize.valueOr(numBytes));

  MOZ_RELEASE_ASSERT(mappedSize <= SIZE_MAX - gc::SystemPageSize());
  MOZ_RELEASE_ASSERT(numBytes <= maxSize.valueOr(UINT32_MAX));
  MOZ_ASSERT(numBytes % gc::SystemPageSize() == 0);
  MOZ_ASSERT(mappedSize % gc::SystemPageSize() == 0);

  uint64_t mappedSizeWithHeader = mappedSize + gc::SystemPageSize();
  uint64_t numBytesWithHeader = numBytes + gc::SystemPageSize();

  void* data =
      MapBufferMemory((size_t)mappedSizeWithHeader, (size_t)numBytesWithHeader);
  if (!data) {
    return nullptr;
  }

  uint8_t* base = reinterpret_cast<uint8_t*>(data) + gc::SystemPageSize();
  uint8_t* header = base - sizeof(WasmArrayRawBuffer);

  auto rawBuf =
      new (header) WasmArrayRawBuffer(base, maxSize, mappedSize, numBytes);
  return rawBuf;
}

/* static */
void WasmArrayRawBuffer::Release(void* mem) {
  WasmArrayRawBuffer* header =
      (WasmArrayRawBuffer*)((uint8_t*)mem - sizeof(WasmArrayRawBuffer));

  MOZ_RELEASE_ASSERT(header->mappedSize() <= SIZE_MAX - gc::SystemPageSize());
  size_t mappedSizeWithHeader = header->mappedSize() + gc::SystemPageSize();

  UnmapBufferMemory(header->basePointer(), mappedSizeWithHeader);
}

WasmArrayRawBuffer* ArrayBufferObject::BufferContents::wasmBuffer() const {
  MOZ_RELEASE_ASSERT(kind_ == WASM);
  return (WasmArrayRawBuffer*)(data_ - sizeof(WasmArrayRawBuffer));
}

template <typename ObjT, typename RawbufT>
static bool CreateSpecificWasmBuffer(
    JSContext* cx, uint32_t initialSize, const Maybe<uint32_t>& maxSize,
    MutableHandleArrayBufferObjectMaybeShared maybeSharedObject) {
  bool useHugeMemory = wasm::IsHugeMemoryEnabled();

  Maybe<uint32_t> clampedMaxSize = maxSize;
  if (clampedMaxSize) {
#ifdef JS_64BIT
    // On 64-bit platforms when we aren't using huge memory, clamp
    // clampedMaxSize to a smaller value that satisfies the 32-bit invariants
    // clampedMaxSize + wasm::PageSize < UINT32_MAX and clampedMaxSize %
    // wasm::PageSize == 0
    if (!useHugeMemory &&
        clampedMaxSize.value() >= (UINT32_MAX - wasm::PageSize)) {
      uint32_t clamp = (wasm::MaxMemoryMaximumPages - 2) * wasm::PageSize;
      MOZ_ASSERT(clamp < UINT32_MAX);
      MOZ_ASSERT(initialSize <= clamp);
      clampedMaxSize = Some(clamp);
    }
#else
    static_assert(sizeof(uintptr_t) == 4, "assuming not 64 bit implies 32 bit");

    // On 32-bit platforms, prevent applications specifying a large max
    // (like UINT32_MAX) from unintentially OOMing the browser: they just
    // want "a lot of memory". Maintain the invariant that
    // initialSize <= clampedMaxSize.
    static const uint32_t OneGiB = 1 << 30;
    uint32_t clamp = std::max(OneGiB, initialSize);
    clampedMaxSize = Some(std::min(clamp, *clampedMaxSize));
#endif
  }

  Maybe<size_t> mappedSize;

#ifdef WASM_SUPPORTS_HUGE_MEMORY
  if (useHugeMemory) {
    mappedSize = Some(wasm::HugeMappedSize);
  }
#endif

  RawbufT* buffer = RawbufT::Allocate(initialSize, clampedMaxSize, mappedSize);
  if (!buffer) {
    if (useHugeMemory) {
      WarnNumberASCII(cx, JSMSG_WASM_HUGE_MEMORY_FAILED);
      if (cx->isExceptionPending()) {
        cx->clearPendingException();
      }

      ReportOutOfMemory(cx);
      return false;
    }

    // If we fail, and have a clampedMaxSize, try to reserve the biggest chunk
    // in the range [initialSize, clampedMaxSize) using log backoff.
    if (!clampedMaxSize) {
      wasm::Log(cx, "new Memory({initial=%u bytes}) failed", initialSize);
      ReportOutOfMemory(cx);
      return false;
    }

    uint32_t cur = clampedMaxSize.value() / 2;

    for (; cur > initialSize; cur /= 2) {
      uint32_t clampedMaxSize = RoundUp(cur, wasm::PageSize);
      buffer = RawbufT::Allocate(initialSize, Some(clampedMaxSize), mappedSize);
      if (buffer) {
        break;
      }
    }

    if (!buffer) {
      wasm::Log(cx, "new Memory({initial=%u bytes}) failed", initialSize);
      ReportOutOfMemory(cx);
      return false;
    }

    // Try to grow our chunk as much as possible.
    for (size_t d = cur / 2; d >= wasm::PageSize; d /= 2) {
      buffer->tryGrowMaxSizeInPlace(RoundUp(d, wasm::PageSize));
    }
  }

  // ObjT::createFromNewRawBuffer assumes ownership of |buffer| even in case
  // of failure.
  RootedArrayBufferObjectMaybeShared object(
      cx, ObjT::createFromNewRawBuffer(cx, buffer, initialSize));
  if (!object) {
    return false;
  }

  maybeSharedObject.set(object);

  // See MaximumLiveMappedBuffers comment above.
  if (liveBufferCount > StartSyncFullGCAtLiveBufferCount) {
    JS::PrepareForFullGC(cx);
    JS::NonIncrementalGC(cx, GC_NORMAL, JS::GCReason::TOO_MUCH_WASM_MEMORY);
    allocatedSinceLastTrigger = 0;
  } else if (liveBufferCount > StartTriggeringAtLiveBufferCount) {
    allocatedSinceLastTrigger++;
    if (allocatedSinceLastTrigger > AllocatedBuffersPerTrigger) {
      Unused << cx->runtime()->gc.triggerGC(JS::GCReason::TOO_MUCH_WASM_MEMORY);
      allocatedSinceLastTrigger = 0;
    }
  } else {
    allocatedSinceLastTrigger = 0;
  }

  if (clampedMaxSize) {
    if (useHugeMemory) {
      wasm::Log(cx,
                "new Memory({initial:%u bytes, maximum:%u bytes}) succeeded",
                unsigned(initialSize), unsigned(*clampedMaxSize));
    } else {
      wasm::Log(cx,
                "new Memory({initial:%u bytes, maximum:%u bytes}) succeeded "
                "with internal maximum of %u",
                unsigned(initialSize), unsigned(*clampedMaxSize),
                unsigned(object->wasmMaxSize().value()));
    }
  } else {
    wasm::Log(cx, "new Memory({initial:%u bytes}) succeeded",
              unsigned(initialSize));
  }

  return true;
}

bool js::CreateWasmBuffer(JSContext* cx, const wasm::Limits& memory,
                          MutableHandleArrayBufferObjectMaybeShared buffer) {
  MOZ_ASSERT(memory.initial % wasm::PageSize == 0);
  MOZ_RELEASE_ASSERT(cx->wasmHaveSignalHandlers);
  MOZ_RELEASE_ASSERT((memory.initial / wasm::PageSize) <=
                     wasm::MaxMemoryInitialPages);

  if (memory.shared == wasm::Shareable::True) {
    if (!cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_WASM_NO_SHMEM_LINK);
      return false;
    }
    return CreateSpecificWasmBuffer<SharedArrayBufferObject,
                                    SharedArrayRawBuffer>(
        cx, memory.initial, memory.maximum, buffer);
  }
  return CreateSpecificWasmBuffer<ArrayBufferObject, WasmArrayRawBuffer>(
      cx, memory.initial, memory.maximum, buffer);
}

bool ArrayBufferObject::prepareForAsmJS() {
  MOZ_ASSERT(byteLength() % wasm::PageSize == 0,
             "prior size checking should have guaranteed page-size multiple");
  MOZ_ASSERT(byteLength() > 0,
             "prior size checking should have excluded empty buffers");

  switch (bufferKind()) {
    case MALLOCED:
    case MAPPED:
    case EXTERNAL:
      // It's okay if this uselessly sets the flag a second time.
      setIsPreparedForAsmJS();
      return true;

    case INLINE_DATA:
      static_assert(wasm::PageSize > MaxInlineBytes,
                    "inline data must be too small to be a page size multiple");
      MOZ_ASSERT_UNREACHABLE(
          "inline-data buffers should be implicitly excluded by size checks");
      return false;

    case NO_DATA:
      MOZ_ASSERT_UNREACHABLE(
          "size checking should have excluded detached or empty buffers");
      return false;

    // asm.js code and associated buffers are potentially long-lived.  Yet a
    // buffer of user-owned data *must* be detached by the user before the
    // user-owned data is disposed.  No caller wants to use a user-owned
    // ArrayBuffer with asm.js, so just don't support this and avoid a mess of
    // complexity.
    case USER_OWNED:
    // wasm buffers can be detached at any time.
    case WASM:
      MOZ_ASSERT(!isPreparedForAsmJS());
      return false;

    case BAD1:
      MOZ_ASSERT_UNREACHABLE("invalid bufferKind() encountered");
      return false;
  }

  MOZ_ASSERT_UNREACHABLE("non-exhaustive kind-handling switch?");
  return false;
}

ArrayBufferObject::BufferContents ArrayBufferObject::createMappedContents(
    int fd, size_t offset, size_t length) {
  void* data =
      gc::AllocateMappedContent(fd, offset, length, ARRAY_BUFFER_ALIGNMENT);
  return BufferContents::createMapped(data);
}

uint8_t* ArrayBufferObject::inlineDataPointer() const {
  return static_cast<uint8_t*>(fixedData(JSCLASS_RESERVED_SLOTS(&class_)));
}

uint8_t* ArrayBufferObject::dataPointer() const {
  return static_cast<uint8_t*>(getFixedSlot(DATA_SLOT).toPrivate());
}

SharedMem<uint8_t*> ArrayBufferObject::dataPointerShared() const {
  return SharedMem<uint8_t*>::unshared(getFixedSlot(DATA_SLOT).toPrivate());
}

ArrayBufferObject::FreeInfo* ArrayBufferObject::freeInfo() const {
  MOZ_ASSERT(isExternal());
  return reinterpret_cast<FreeInfo*>(inlineDataPointer());
}

void ArrayBufferObject::releaseData(JSFreeOp* fop) {
  switch (bufferKind()) {
    case INLINE_DATA:
      // Inline data doesn't require releasing.
      break;
    case MALLOCED:
      fop->free_(this, dataPointer(), byteLength(),
                 MemoryUse::ArrayBufferContents);
      break;
    case NO_DATA:
      // There's nothing to release if there's no data.
      MOZ_ASSERT(dataPointer() == nullptr);
      break;
    case USER_OWNED:
      // User-owned data is released by, well, the user.
      break;
    case MAPPED:
      gc::DeallocateMappedContent(dataPointer(), byteLength());
      fop->removeCellMemory(this, associatedBytes(),
                            MemoryUse::ArrayBufferContents);
      break;
    case WASM:
      WasmArrayRawBuffer::Release(dataPointer());
      fop->removeCellMemory(this, byteLength(), MemoryUse::ArrayBufferContents);
      break;
    case EXTERNAL:
      if (freeInfo()->freeFunc) {
        // The analyzer can't know for sure whether the embedder-supplied
        // free function will GC. We give the analyzer a hint here.
        // (Doing a GC in the free function is considered a programmer
        // error.)
        JS::AutoSuppressGCAnalysis nogc;
        freeInfo()->freeFunc(dataPointer(), freeInfo()->freeUserData);
      }
      break;
    case BAD1:
      MOZ_CRASH("invalid BufferKind encountered");
      break;
  }
}

void ArrayBufferObject::setDataPointer(BufferContents contents) {
  setFixedSlot(DATA_SLOT, PrivateValue(contents.data()));
  setFlags((flags() & ~KIND_MASK) | contents.kind());

  if (isExternal()) {
    auto info = freeInfo();
    info->freeFunc = contents.freeFunc();
    info->freeUserData = contents.freeUserData();
  }
}

uint32_t ArrayBufferObject::byteLength() const {
  return getFixedSlot(BYTE_LENGTH_SLOT).toInt32();
}

inline size_t ArrayBufferObject::associatedBytes() const {
  if (bufferKind() == MALLOCED) {
    return byteLength();
  } else if (bufferKind() == MAPPED) {
    return RoundUp(byteLength(), js::gc::SystemPageSize());
  } else {
    MOZ_CRASH("Unexpected buffer kind");
  }
}

void ArrayBufferObject::setByteLength(uint32_t length) {
  MOZ_ASSERT(length <= INT32_MAX);
  setFixedSlot(BYTE_LENGTH_SLOT, Int32Value(length));
}

size_t ArrayBufferObject::wasmMappedSize() const {
  if (isWasm()) {
    return contents().wasmBuffer()->mappedSize();
  }
  return byteLength();
}

size_t js::WasmArrayBufferMappedSize(const ArrayBufferObjectMaybeShared* buf) {
  if (buf->is<ArrayBufferObject>()) {
    return buf->as<ArrayBufferObject>().wasmMappedSize();
  }
  return buf->as<SharedArrayBufferObject>().wasmMappedSize();
}

Maybe<uint32_t> ArrayBufferObject::wasmMaxSize() const {
  if (isWasm()) {
    return contents().wasmBuffer()->maxSize();
  } else {
    return Some<uint32_t>(byteLength());
  }
}

Maybe<uint32_t> js::WasmArrayBufferMaxSize(
    const ArrayBufferObjectMaybeShared* buf) {
  if (buf->is<ArrayBufferObject>()) {
    return buf->as<ArrayBufferObject>().wasmMaxSize();
  }
  return buf->as<SharedArrayBufferObject>().wasmMaxSize();
}

static void CheckStealPreconditions(Handle<ArrayBufferObject*> buffer,
                                    JSContext* cx) {
  cx->check(buffer);

  MOZ_ASSERT(!buffer->isDetached(), "can't steal from a detached buffer");
  MOZ_ASSERT(!buffer->isPreparedForAsmJS(),
             "asm.js-prepared buffers don't have detachable/stealable data");
  MOZ_ASSERT(!buffer->hasTypedObjectViews(),
             "buffers for typed objects don't have detachable/stealable data");
}

/* static */
bool ArrayBufferObject::wasmGrowToSizeInPlace(
    uint32_t newSize, HandleArrayBufferObject oldBuf,
    MutableHandleArrayBufferObject newBuf, JSContext* cx) {
  CheckStealPreconditions(oldBuf, cx);

  MOZ_ASSERT(oldBuf->isWasm());

  // On failure, do not throw and ensure that the original buffer is
  // unmodified and valid. After WasmArrayRawBuffer::growToSizeInPlace(), the
  // wasm-visible length of the buffer has been increased so it must be the
  // last fallible operation.

  if (newSize > ArrayBufferObject::MaxBufferByteLength) {
    return false;
  }

  newBuf.set(ArrayBufferObject::createEmpty(cx));
  if (!newBuf) {
    cx->clearPendingException();
    return false;
  }

  MOZ_ASSERT(newBuf->isNoData());

  if (!oldBuf->contents().wasmBuffer()->growToSizeInPlace(oldBuf->byteLength(),
                                                          newSize)) {
    return false;
  }

  // Extract the grown contents from |oldBuf|.
  BufferContents oldContents = oldBuf->contents();

  // Overwrite |oldBuf|'s data pointer *without* releasing old data.
  oldBuf->setDataPointer(BufferContents::createNoData());

  // Detach |oldBuf| now that doing so won't release |oldContents|.
  RemoveCellMemory(oldBuf, oldBuf->byteLength(),
                   MemoryUse::ArrayBufferContents);
  ArrayBufferObject::detach(cx, oldBuf);

  // Set |newBuf|'s contents to |oldBuf|'s original contents.
  newBuf->initialize(newSize, oldContents);
  AddCellMemory(newBuf, newSize, MemoryUse::ArrayBufferContents);

  return true;
}

/* static */
bool ArrayBufferObject::wasmMovingGrowToSize(
    uint32_t newSize, HandleArrayBufferObject oldBuf,
    MutableHandleArrayBufferObject newBuf, JSContext* cx) {
  // On failure, do not throw and ensure that the original buffer is
  // unmodified and valid.

  if (newSize > ArrayBufferObject::MaxBufferByteLength) {
    return false;
  }

  if (wasm::ComputeMappedSize(newSize) <= oldBuf->wasmMappedSize() ||
      oldBuf->contents().wasmBuffer()->extendMappedSize(newSize)) {
    return wasmGrowToSizeInPlace(newSize, oldBuf, newBuf, cx);
  }

  newBuf.set(ArrayBufferObject::createEmpty(cx));
  if (!newBuf) {
    cx->clearPendingException();
    return false;
  }

  WasmArrayRawBuffer* newRawBuf =
      WasmArrayRawBuffer::Allocate(newSize, Nothing(), Nothing());
  if (!newRawBuf) {
    return false;
  }

  AddCellMemory(newBuf, newSize, MemoryUse::ArrayBufferContents);

  BufferContents contents =
      BufferContents::createWasm(newRawBuf->dataPointer());
  newBuf->initialize(newSize, contents);

  memcpy(newBuf->dataPointer(), oldBuf->dataPointer(), oldBuf->byteLength());
  ArrayBufferObject::detach(cx, oldBuf);
  return true;
}

uint32_t ArrayBufferObject::flags() const {
  return uint32_t(getFixedSlot(FLAGS_SLOT).toInt32());
}

void ArrayBufferObject::setFlags(uint32_t flags) {
  setFixedSlot(FLAGS_SLOT, Int32Value(flags));
}

static MOZ_MUST_USE bool CheckArrayBufferTooLarge(JSContext* cx,
                                                  uint32_t nbytes) {
  // Refuse to allocate too large buffers, currently limited to ~2 GiB.
  if (MOZ_UNLIKELY(nbytes > ArrayBufferObject::MaxBufferByteLength)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_ARRAY_LENGTH);
    return false;
  }

  return true;
}

static inline js::gc::AllocKind GetArrayBufferGCObjectKind(size_t numSlots) {
  if (numSlots <= 4) {
    return js::gc::AllocKind::ARRAYBUFFER4;
  }
  if (numSlots <= 8) {
    return js::gc::AllocKind::ARRAYBUFFER8;
  }
  if (numSlots <= 12) {
    return js::gc::AllocKind::ARRAYBUFFER12;
  }
  return js::gc::AllocKind::ARRAYBUFFER16;
}

ArrayBufferObject* ArrayBufferObject::createForContents(
    JSContext* cx, uint32_t nbytes, BufferContents contents) {
  MOZ_ASSERT(contents);
  MOZ_ASSERT(contents.kind() != INLINE_DATA);
  MOZ_ASSERT(contents.kind() != NO_DATA);
  MOZ_ASSERT(contents.kind() != WASM);

  // 24.1.1.1, step 3 (Inlined 6.2.6.1 CreateByteDataBlock, step 2).
  if (!CheckArrayBufferTooLarge(cx, nbytes)) {
    return nullptr;
  }

  // Some |contents| kinds need to store extra data in the ArrayBuffer beyond a
  // data pointer.  If needed for the particular kind, add extra fixed slots to
  // the ArrayBuffer for use as raw storage to store such information.
  size_t reservedSlots = JSCLASS_RESERVED_SLOTS(&class_);

  size_t nAllocated = 0;
  size_t nslots = reservedSlots;
  if (contents.kind() == USER_OWNED) {
    // No accounting to do in this case.
  } else if (contents.kind() == EXTERNAL) {
    // Store the FreeInfo in the inline data slots so that we
    // don't use up slots for it in non-refcounted array buffers.
    size_t freeInfoSlots = HowMany(sizeof(FreeInfo), sizeof(Value));
    MOZ_ASSERT(reservedSlots + freeInfoSlots <= NativeObject::MAX_FIXED_SLOTS,
               "FreeInfo must fit in inline slots");
    nslots += freeInfoSlots;
  } else {
    // The ABO is taking ownership, so account the bytes against the zone.
    nAllocated = nbytes;
    if (contents.kind() == MAPPED) {
      nAllocated = RoundUp(nbytes, js::gc::SystemPageSize());
    } else {
      MOZ_ASSERT(contents.kind() == MALLOCED,
                 "should have handled all possible callers' kinds");
    }
  }

  MOZ_ASSERT(!(class_.flags & JSCLASS_HAS_PRIVATE));
  gc::AllocKind allocKind = GetArrayBufferGCObjectKind(nslots);

  AutoSetNewObjectMetadata metadata(cx);
  Rooted<ArrayBufferObject*> buffer(
      cx, NewObjectWithClassProto<ArrayBufferObject>(cx, nullptr, allocKind,
                                                     TenuredObject));
  if (!buffer) {
    return nullptr;
  }

  MOZ_ASSERT(!gc::IsInsideNursery(buffer),
             "ArrayBufferObject has a finalizer that must be called to not "
             "leak in some cases, so it can't be nursery-allocated");

  buffer->initialize(nbytes, contents);

  if (contents.kind() == MAPPED || contents.kind() == MALLOCED) {
    AddCellMemory(buffer, nAllocated, MemoryUse::ArrayBufferContents);
  }

  return buffer;
}

template <ArrayBufferObject::FillContents FillType>
/* static */ std::tuple<ArrayBufferObject*, uint8_t*>
ArrayBufferObject::createBufferAndData(
    JSContext* cx, uint32_t nbytes, AutoSetNewObjectMetadata&,
    JS::Handle<JSObject*> proto /* = nullptr */) {
  MOZ_ASSERT(nbytes <= ArrayBufferObject::MaxBufferByteLength,
             "caller must validate the byte count it passes");

  // Try fitting the data inline with the object by repurposing fixed-slot
  // storage.  Add extra fixed slots if necessary to accomplish this, but don't
  // exceed the maximum number of fixed slots!
  size_t nslots = JSCLASS_RESERVED_SLOTS(&class_);
  ArrayBufferContents data;
  if (nbytes <= MaxInlineBytes) {
    int newSlots = HowMany(nbytes, sizeof(Value));
    MOZ_ASSERT(int(nbytes) <= newSlots * int(sizeof(Value)));

    nslots += newSlots;
  } else {
    data = (FillType == FillContents::Uninitialized
                ? AllocateUninitializedArrayBufferContents
                : AllocateArrayBufferContents)(cx, nbytes);
    if (!data) {
      return {nullptr, nullptr};
    }
  }

  MOZ_ASSERT(!(class_.flags & JSCLASS_HAS_PRIVATE));
  gc::AllocKind allocKind = GetArrayBufferGCObjectKind(nslots);

  ArrayBufferObject* buffer = NewObjectWithClassProto<ArrayBufferObject>(
      cx, proto, allocKind, GenericObject);
  if (!buffer) {
    return {nullptr, nullptr};
  }

  MOZ_ASSERT(!gc::IsInsideNursery(buffer),
             "ArrayBufferObject has a finalizer that must be called to not "
             "leak in some cases, so it can't be nursery-allocated");

  uint8_t* toFill;
  if (data) {
    toFill = data.release();
    buffer->initialize(nbytes, BufferContents::createMalloced(toFill));
    AddCellMemory(buffer, nbytes, MemoryUse::ArrayBufferContents);
  } else {
    toFill = static_cast<uint8_t*>(buffer->initializeToInlineData(nbytes));
    if constexpr (FillType == FillContents::Zero) {
      memset(toFill, 0, nbytes);
    }
  }

  return {buffer, toFill};
}

/* static */ ArrayBufferObject* ArrayBufferObject::copy(
    JSContext* cx, JS::Handle<ArrayBufferObject*> unwrappedArrayBuffer) {
  if (unwrappedArrayBuffer->isDetached()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return nullptr;
  }

  uint32_t nbytes = unwrappedArrayBuffer->byteLength();

  AutoSetNewObjectMetadata metadata(cx);
  auto [buffer, toFill] = createBufferAndData<FillContents::Uninitialized>(
      cx, nbytes, metadata, nullptr);
  if (!buffer) {
    return nullptr;
  }

  std::uninitialized_copy_n(unwrappedArrayBuffer->dataPointer(), nbytes,
                            toFill);
  return buffer;
}

ArrayBufferObject* ArrayBufferObject::createZeroed(
    JSContext* cx, uint32_t nbytes, HandleObject proto /* = nullptr */) {
  // 24.1.1.1, step 3 (Inlined 6.2.6.1 CreateByteDataBlock, step 2).
  if (!CheckArrayBufferTooLarge(cx, nbytes)) {
    return nullptr;
  }

  AutoSetNewObjectMetadata metadata(cx);
  auto [buffer, toFill] =
      createBufferAndData<FillContents::Zero>(cx, nbytes, metadata, proto);
  Unused << toFill;
  return buffer;
}

ArrayBufferObject* ArrayBufferObject::createForTypedObject(JSContext* cx,
                                                           uint32_t nbytes) {
  ArrayBufferObject* buffer = createZeroed(cx, nbytes);
  if (buffer) {
    buffer->setHasTypedObjectViews();
  }
  return buffer;
}

ArrayBufferObject* ArrayBufferObject::createEmpty(JSContext* cx) {
  AutoSetNewObjectMetadata metadata(cx);
  ArrayBufferObject* obj = NewBuiltinClassInstance<ArrayBufferObject>(cx);
  if (!obj) {
    return nullptr;
  }

  obj->initialize(0, BufferContents::createNoData());
  return obj;
}

ArrayBufferObject* ArrayBufferObject::createFromNewRawBuffer(
    JSContext* cx, WasmArrayRawBuffer* rawBuffer, uint32_t initialSize) {
  AutoSetNewObjectMetadata metadata(cx);
  ArrayBufferObject* buffer = NewBuiltinClassInstance<ArrayBufferObject>(cx);
  if (!buffer) {
    WasmArrayRawBuffer::Release(rawBuffer->dataPointer());
    return nullptr;
  }

  MOZ_ASSERT(initialSize == rawBuffer->byteLength());

  buffer->setByteLength(initialSize);
  buffer->setFlags(0);
  buffer->setFirstView(nullptr);

  auto contents = BufferContents::createWasm(rawBuffer->dataPointer());
  buffer->setDataPointer(contents);

  AddCellMemory(buffer, initialSize, MemoryUse::ArrayBufferContents);

  return buffer;
}

/* static */ uint8_t* ArrayBufferObject::stealMallocedContents(
    JSContext* cx, Handle<ArrayBufferObject*> buffer) {
  CheckStealPreconditions(buffer, cx);

  switch (buffer->bufferKind()) {
    case MALLOCED: {
      uint8_t* stolenData = buffer->dataPointer();
      MOZ_ASSERT(stolenData);

      RemoveCellMemory(buffer, buffer->byteLength(),
                       MemoryUse::ArrayBufferContents);

      // Overwrite the old data pointer *without* releasing the contents
      // being stolen.
      buffer->setDataPointer(BufferContents::createNoData());

      // Detach |buffer| now that doing so won't free |stolenData|.
      ArrayBufferObject::detach(cx, buffer);
      return stolenData;
    }

    case INLINE_DATA:
    case NO_DATA:
    case USER_OWNED:
    case MAPPED:
    case EXTERNAL: {
      // We can't use these data types directly.  Make a copy to return.
      ArrayBufferContents copiedData = NewCopiedBufferContents(cx, buffer);
      if (!copiedData) {
        return nullptr;
      }

      // Detach |buffer|.  This immediately releases the currently owned
      // contents, freeing or unmapping data in the MAPPED and EXTERNAL cases.
      ArrayBufferObject::detach(cx, buffer);
      return copiedData.release();
    }

    case WASM:
      MOZ_ASSERT_UNREACHABLE(
          "wasm buffers aren't stealable except by a "
          "memory.grow operation that shouldn't call this "
          "function");
      return nullptr;

    case BAD1:
      MOZ_ASSERT_UNREACHABLE("bad kind when stealing malloc'd data");
      return nullptr;
  }

  MOZ_ASSERT_UNREACHABLE("garbage kind computed");
  return nullptr;
}

/* static */ ArrayBufferObject::BufferContents
ArrayBufferObject::extractStructuredCloneContents(
    JSContext* cx, Handle<ArrayBufferObject*> buffer) {
  CheckStealPreconditions(buffer, cx);

  BufferContents contents = buffer->contents();

  switch (contents.kind()) {
    case INLINE_DATA:
    case NO_DATA:
    case USER_OWNED: {
      ArrayBufferContents copiedData = NewCopiedBufferContents(cx, buffer);
      if (!copiedData) {
        return BufferContents::createFailed();
      }

      ArrayBufferObject::detach(cx, buffer);
      return BufferContents::createMalloced(copiedData.release());
    }

    case MALLOCED:
    case MAPPED: {
      MOZ_ASSERT(contents);

      RemoveCellMemory(buffer, buffer->associatedBytes(),
                       MemoryUse::ArrayBufferContents);

      // Overwrite the old data pointer *without* releasing old data.
      buffer->setDataPointer(BufferContents::createNoData());

      // Detach |buffer| now that doing so won't release |oldContents|.
      ArrayBufferObject::detach(cx, buffer);
      return contents;
    }

    case WASM:
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_WASM_NO_TRANSFER);
      return BufferContents::createFailed();

    case EXTERNAL:
      MOZ_ASSERT_UNREACHABLE(
          "external ArrayBuffer shouldn't have passed the "
          "structured-clone preflighting");
      break;

    case BAD1:
      MOZ_ASSERT_UNREACHABLE("bad kind when stealing malloc'd data");
      break;
  }

  MOZ_ASSERT_UNREACHABLE("garbage kind computed");
  return BufferContents::createFailed();
}

/* static */
void ArrayBufferObject::addSizeOfExcludingThis(
    JSObject* obj, mozilla::MallocSizeOf mallocSizeOf, JS::ClassInfo* info) {
  ArrayBufferObject& buffer = AsArrayBuffer(obj);
  switch (buffer.bufferKind()) {
    case INLINE_DATA:
      // Inline data's size should be reported by this object's size-class
      // reporting.
      break;
    case MALLOCED:
      if (buffer.isPreparedForAsmJS()) {
        info->objectsMallocHeapElementsAsmJS +=
            mallocSizeOf(buffer.dataPointer());
      } else {
        info->objectsMallocHeapElementsNormal +=
            mallocSizeOf(buffer.dataPointer());
      }
      break;
    case NO_DATA:
      // No data is no memory.
      MOZ_ASSERT(buffer.dataPointer() == nullptr);
      break;
    case USER_OWNED:
      // User-owned data should be accounted for by the user.
      break;
    case MAPPED:
      info->objectsNonHeapElementsNormal += buffer.byteLength();
      break;
    case WASM:
      info->objectsNonHeapElementsWasm += buffer.byteLength();
      MOZ_ASSERT(buffer.wasmMappedSize() >= buffer.byteLength());
      info->wasmGuardPages += buffer.wasmMappedSize() - buffer.byteLength();
      break;
    case EXTERNAL:
      MOZ_CRASH("external buffers not currently supported");
      break;
    case BAD1:
      MOZ_CRASH("bad bufferKind()");
  }
}

/* static */
void ArrayBufferObject::finalize(JSFreeOp* fop, JSObject* obj) {
  obj->as<ArrayBufferObject>().releaseData(fop);
}

/* static */
void ArrayBufferObject::copyData(Handle<ArrayBufferObject*> toBuffer,
                                 uint32_t toIndex,
                                 Handle<ArrayBufferObject*> fromBuffer,
                                 uint32_t fromIndex, uint32_t count) {
  MOZ_ASSERT(toBuffer->byteLength() >= count);
  MOZ_ASSERT(toBuffer->byteLength() >= toIndex + count);
  MOZ_ASSERT(fromBuffer->byteLength() >= fromIndex);
  MOZ_ASSERT(fromBuffer->byteLength() >= fromIndex + count);

  memcpy(toBuffer->dataPointer() + toIndex,
         fromBuffer->dataPointer() + fromIndex, count);
}

/* static */
size_t ArrayBufferObject::objectMoved(JSObject* obj, JSObject* old) {
  ArrayBufferObject& dst = obj->as<ArrayBufferObject>();
  const ArrayBufferObject& src = old->as<ArrayBufferObject>();

  // Fix up possible inline data pointer.
  if (src.hasInlineData()) {
    dst.setFixedSlot(DATA_SLOT, PrivateValue(dst.inlineDataPointer()));
  }

  return 0;
}

JSObject* ArrayBufferObject::firstView() {
  return getFixedSlot(FIRST_VIEW_SLOT).isObject()
             ? &getFixedSlot(FIRST_VIEW_SLOT).toObject()
             : nullptr;
}

void ArrayBufferObject::setFirstView(ArrayBufferViewObject* view) {
  setFixedSlot(FIRST_VIEW_SLOT, ObjectOrNullValue(view));
}

bool ArrayBufferObject::addView(JSContext* cx, ArrayBufferViewObject* view) {
  if (!firstView()) {
    setFirstView(view);
    return true;
  }

  return ObjectRealm::get(this).innerViews.get().addView(cx, this, view);
}

/*
 * InnerViewTable
 */

constexpr size_t VIEW_LIST_MAX_LENGTH = 500;

bool InnerViewTable::addView(JSContext* cx, ArrayBufferObject* buffer,
                             JSObject* view) {
  // ArrayBufferObject entries are only added when there are multiple views.
  MOZ_ASSERT(buffer->firstView());

  Map::AddPtr p = map.lookupForAdd(buffer);

  MOZ_ASSERT(!gc::IsInsideNursery(buffer));
  bool addToNursery = nurseryKeysValid && gc::IsInsideNursery(view);

  if (p) {
    ViewVector& views = p->value();
    MOZ_ASSERT(!views.empty());

    if (addToNursery) {
      // Only add the entry to |nurseryKeys| if it isn't already there.
      if (views.length() >= VIEW_LIST_MAX_LENGTH) {
        // To avoid quadratic blowup, skip the loop below if we end up
        // adding enormous numbers of views for the same object.
        nurseryKeysValid = false;
      } else {
        for (size_t i = 0; i < views.length(); i++) {
          if (gc::IsInsideNursery(views[i])) {
            addToNursery = false;
            break;
          }
        }
      }
    }

    if (!views.append(view)) {
      ReportOutOfMemory(cx);
      return false;
    }
  } else {
    if (!map.add(p, buffer, ViewVector(cx->zone()))) {
      ReportOutOfMemory(cx);
      return false;
    }
    // ViewVector has one inline element, so the first insertion is
    // guaranteed to succeed.
    MOZ_ALWAYS_TRUE(p->value().append(view));
  }

  if (addToNursery && !nurseryKeys.append(buffer)) {
    nurseryKeysValid = false;
  }

  return true;
}

InnerViewTable::ViewVector* InnerViewTable::maybeViewsUnbarriered(
    ArrayBufferObject* buffer) {
  Map::Ptr p = map.lookup(buffer);
  if (p) {
    return &p->value();
  }
  return nullptr;
}

void InnerViewTable::removeViews(ArrayBufferObject* buffer) {
  Map::Ptr p = map.lookup(buffer);
  MOZ_ASSERT(p);

  map.remove(p);
}

/* static */
bool InnerViewTable::sweepEntry(JSObject** pkey, ViewVector& views) {
  if (IsAboutToBeFinalizedUnbarriered(pkey)) {
    return true;
  }

  MOZ_ASSERT(!views.empty());
  size_t i = 0;
  while (i < views.length()) {
    if (IsAboutToBeFinalizedUnbarriered(&views[i])) {
      // If the current element is garbage then remove it from the
      // vector by moving the last one into its place.
      views[i] = views.back();
      views.popBack();
    } else {
      i++;
    }
  }

  return views.empty();
}

void InnerViewTable::sweep() {
  MOZ_ASSERT(nurseryKeys.empty());
  map.sweep();
}

void InnerViewTable::sweepAfterMinorGC() {
  MOZ_ASSERT(needsSweepAfterMinorGC());

  if (nurseryKeysValid) {
    for (size_t i = 0; i < nurseryKeys.length(); i++) {
      JSObject* buffer = MaybeForwarded(nurseryKeys[i]);
      Map::Ptr p = map.lookup(buffer);
      if (!p) {
        continue;
      }

      if (sweepEntry(&p->mutableKey(), p->value())) {
        map.remove(buffer);
      }
    }
    nurseryKeys.clear();
  } else {
    // Do the required sweeping by looking at every map entry.
    nurseryKeys.clear();
    sweep();

    nurseryKeysValid = true;
  }
}

size_t InnerViewTable::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  size_t vectorSize = 0;
  for (Map::Enum e(map); !e.empty(); e.popFront()) {
    vectorSize += e.front().value().sizeOfExcludingThis(mallocSizeOf);
  }

  return vectorSize + map.shallowSizeOfExcludingThis(mallocSizeOf) +
         nurseryKeys.sizeOfExcludingThis(mallocSizeOf);
}

template <>
bool JSObject::is<js::ArrayBufferObjectMaybeShared>() const {
  return is<ArrayBufferObject>() || is<SharedArrayBufferObject>();
}

JS_FRIEND_API uint32_t JS::GetArrayBufferByteLength(JSObject* obj) {
  ArrayBufferObject* aobj = obj->maybeUnwrapAs<ArrayBufferObject>();
  return aobj ? aobj->byteLength() : 0;
}

JS_FRIEND_API uint8_t* JS::GetArrayBufferData(JSObject* obj,
                                              bool* isSharedMemory,
                                              const JS::AutoRequireNoGC&) {
  ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>();
  if (!aobj) {
    return nullptr;
  }
  *isSharedMemory = false;
  return aobj->dataPointer();
}

JS_FRIEND_API bool JS::DetachArrayBuffer(JSContext* cx, HandleObject obj) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(obj);

  if (!obj->is<ArrayBufferObject>()) {
    JS_ReportErrorASCII(cx, "ArrayBuffer object required");
    return false;
  }

  Rooted<ArrayBufferObject*> buffer(cx, &obj->as<ArrayBufferObject>());

  if (buffer->isWasm() || buffer->isPreparedForAsmJS()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_NO_TRANSFER);
    return false;
  }

  ArrayBufferObject::detach(cx, buffer);
  return true;
}

JS_FRIEND_API bool JS::IsDetachedArrayBufferObject(JSObject* obj) {
  ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>();
  if (!aobj) {
    return false;
  }

  return aobj->isDetached();
}

JS_FRIEND_API JSObject* JS::NewArrayBuffer(JSContext* cx, uint32_t nbytes) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  return ArrayBufferObject::createZeroed(cx, nbytes);
}

JS_PUBLIC_API JSObject* JS::NewArrayBufferWithContents(JSContext* cx,
                                                       size_t nbytes,
                                                       void* data) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  MOZ_ASSERT_IF(!data, nbytes == 0);

  if (!data) {
    // Don't pass nulled contents to |createForContents|.
    return ArrayBufferObject::createZeroed(cx, 0);
  }

  using BufferContents = ArrayBufferObject::BufferContents;

  BufferContents contents = BufferContents::createMalloced(data);
  return ArrayBufferObject::createForContents(cx, nbytes, contents);
}

static ArrayBufferObject* UnwrapArrayBuffer(
    JSContext* cx, JS::Handle<JSObject*> maybeArrayBuffer) {
  JSObject* obj = CheckedUnwrapStatic(maybeArrayBuffer);
  if (!obj) {
    ReportAccessDenied(cx);
    return nullptr;
  }

  if (!obj->is<ArrayBufferObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_BAD_ARGS);
    return nullptr;
  }

  return &obj->as<ArrayBufferObject>();
}

JS_PUBLIC_API JSObject* JS::CopyArrayBuffer(JSContext* cx,
                                            Handle<JSObject*> arrayBuffer) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  MOZ_ASSERT(arrayBuffer != nullptr);

  Rooted<ArrayBufferObject*> unwrappedSource(
      cx, UnwrapArrayBuffer(cx, arrayBuffer));
  if (!unwrappedSource) {
    return nullptr;
  }

  return ArrayBufferObject::copy(cx, unwrappedSource);
}

JS_PUBLIC_API JSObject* JS::NewExternalArrayBuffer(
    JSContext* cx, size_t nbytes, void* data,
    JS::BufferContentsFreeFunc freeFunc, void* freeUserData) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  MOZ_ASSERT(data);
  MOZ_ASSERT(nbytes > 0);

  using BufferContents = ArrayBufferObject::BufferContents;

  BufferContents contents =
      BufferContents::createExternal(data, freeFunc, freeUserData);
  return ArrayBufferObject::createForContents(cx, nbytes, contents);
}

JS_PUBLIC_API JSObject* JS::NewArrayBufferWithUserOwnedContents(JSContext* cx,
                                                                size_t nbytes,
                                                                void* data) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  MOZ_ASSERT(data);

  using BufferContents = ArrayBufferObject::BufferContents;

  BufferContents contents = BufferContents::createUserOwned(data);
  return ArrayBufferObject::createForContents(cx, nbytes, contents);
}

JS_FRIEND_API bool JS::IsArrayBufferObject(JSObject* obj) {
  return obj->canUnwrapAs<ArrayBufferObject>();
}

JS_FRIEND_API bool JS::ArrayBufferHasData(JSObject* obj) {
  return !obj->unwrapAs<ArrayBufferObject>().isDetached();
}

JS_FRIEND_API JSObject* JS::UnwrapArrayBuffer(JSObject* obj) {
  return obj->maybeUnwrapIf<ArrayBufferObject>();
}

JS_FRIEND_API JSObject* JS::UnwrapSharedArrayBuffer(JSObject* obj) {
  return obj->maybeUnwrapIf<SharedArrayBufferObject>();
}

JS_PUBLIC_API void* JS::StealArrayBufferContents(JSContext* cx,
                                                 HandleObject objArg) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(objArg);

  JSObject* obj = CheckedUnwrapStatic(objArg);
  if (!obj) {
    ReportAccessDenied(cx);
    return nullptr;
  }

  if (!obj->is<ArrayBufferObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_BAD_ARGS);
    return nullptr;
  }

  Rooted<ArrayBufferObject*> buffer(cx, &obj->as<ArrayBufferObject>());
  if (buffer->isDetached()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return nullptr;
  }

  if (buffer->isWasm() || buffer->isPreparedForAsmJS()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_NO_TRANSFER);
    return nullptr;
  }

  AutoRealm ar(cx, buffer);
  return ArrayBufferObject::stealMallocedContents(cx, buffer);
}

JS_PUBLIC_API JSObject* JS::NewMappedArrayBufferWithContents(JSContext* cx,
                                                             size_t nbytes,
                                                             void* data) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  MOZ_ASSERT(data);

  using BufferContents = ArrayBufferObject::BufferContents;

  BufferContents contents = BufferContents::createMapped(data);
  return ArrayBufferObject::createForContents(cx, nbytes, contents);
}

JS_PUBLIC_API void* JS::CreateMappedArrayBufferContents(int fd, size_t offset,
                                                        size_t length) {
  return ArrayBufferObject::createMappedContents(fd, offset, length).data();
}

JS_PUBLIC_API void JS::ReleaseMappedArrayBufferContents(void* contents,
                                                        size_t length) {
  gc::DeallocateMappedContent(contents, length);
}

JS_FRIEND_API bool JS::IsMappedArrayBufferObject(JSObject* obj) {
  ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>();
  if (!aobj) {
    return false;
  }

  return aobj->isMapped();
}

JS_FRIEND_API JSObject* JS::GetObjectAsArrayBuffer(JSObject* obj,
                                                   uint32_t* length,
                                                   uint8_t** data) {
  ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>();
  if (!aobj) {
    return nullptr;
  }

  *length = aobj->byteLength();
  *data = aobj->dataPointer();

  return aobj;
}

JS_FRIEND_API void JS::GetArrayBufferLengthAndData(JSObject* obj,
                                                   uint32_t* length,
                                                   bool* isSharedMemory,
                                                   uint8_t** data) {
  MOZ_ASSERT(IsArrayBuffer(obj));
  *length = AsArrayBuffer(obj).byteLength();
  *data = AsArrayBuffer(obj).dataPointer();
  *isSharedMemory = false;
}

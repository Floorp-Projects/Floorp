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
#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TaggedAnonymousMemory.h"

#include <algorithm>  // std::max, std::min
#include <memory>     // std::uninitialized_copy_n
#include <string.h>
#if !defined(XP_WIN) && !defined(__wasi__)
#  include <sys/mman.h>
#endif
#include <tuple>  // std::tuple
#ifdef MOZ_VALGRIND
#  include <valgrind/memcheck.h>
#endif

#include "jsnum.h"
#include "jstypes.h"

#include "builtin/Array.h"
#include "builtin/DataViewObject.h"
#include "gc/Barrier.h"
#include "gc/Memory.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/experimental/TypedData.h"  // JS_IsArrayBufferViewObject
#include "js/friend/ErrorMessages.h"    // js::GetErrorMessage, JSMSG_*
#include "js/MemoryMetrics.h"
#include "js/PropertySpec.h"
#include "js/SharedArrayBuffer.h"
#include "js/Wrapper.h"
#include "util/WindowsWrapper.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/SharedArrayObject.h"
#include "vm/Warnings.h"  // js::WarnNumberASCII
#include "vm/WrapperObject.h"
#include "wasm/WasmConstants.h"
#include "wasm/WasmLog.h"
#include "wasm/WasmModuleTypes.h"
#include "wasm/WasmProcess.h"
#include "wasm/WasmSignalHandlers.h"

#include "gc/FreeOp-inl.h"
#include "gc/Marking-inl.h"
#include "gc/Nursery-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Realm-inl.h"  // js::AutoRealm
#include "vm/Shape-inl.h"

using JS::ToInt32;

using js::wasm::IndexType;
using js::wasm::Pages;
using mozilla::Atomic;
using mozilla::CheckedInt;
using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

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

bool js::ArrayBufferObject::supportLargeBuffers = true;

[[nodiscard]] static bool CheckArrayBufferTooLarge(JSContext* cx,
                                                   uint64_t nbytes) {
  // Refuse to allocate too large buffers.
  if (MOZ_UNLIKELY(nbytes > ArrayBufferObject::maxBufferByteLength())) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_ARRAY_LENGTH);
    return false;
  }

  return true;
}

void* js::MapBufferMemory(wasm::IndexType t, size_t mappedSize,
                          size_t initialCommittedSize) {
  MOZ_ASSERT(mappedSize % gc::SystemPageSize() == 0);
  MOZ_ASSERT(initialCommittedSize % gc::SystemPageSize() == 0);
  MOZ_ASSERT(initialCommittedSize <= mappedSize);

  auto decrement = mozilla::MakeScopeExit([&] { liveBufferCount--; });
  // Testing just IsHugeMemoryEnabled() here will overestimate the number of
  // live buffers, as asm.js buffers are not huge.  This is OK in practice.
  if (wasm::IsHugeMemoryEnabled(t)) {
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
#elif defined(__wasi__)
  void* data = nullptr;
  if (int err = posix_memalign(&data, gc::SystemPageSize(), mappedSize)) {
    MOZ_ASSERT(err == ENOMEM);
    return nullptr;
  }
  MOZ_ASSERT(data);
  memset(data, 0, mappedSize);
#else   // !XP_WIN && !__wasi__
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
#endif  // !XP_WIN && !__wasi__

#if defined(MOZ_VALGRIND) && \
    defined(VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE)
  VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(
      (unsigned char*)data + initialCommittedSize,
      mappedSize - initialCommittedSize);
#endif

  decrement.release();
  return data;
}

bool js::CommitBufferMemory(void* dataEnd, size_t delta) {
  MOZ_ASSERT(delta);
  MOZ_ASSERT(delta % gc::SystemPageSize() == 0);

#ifdef XP_WIN
  if (!VirtualAlloc(dataEnd, delta, MEM_COMMIT, PAGE_READWRITE)) {
    return false;
  }
#elif defined(__wasi__)
  // posix_memalign'd memory is already committed
  return true;
#else
  if (mprotect(dataEnd, delta, PROT_READ | PROT_WRITE)) {
    return false;
  }
#endif  // XP_WIN

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
#elif defined(__wasi__)
  return false;
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

void js::UnmapBufferMemory(wasm::IndexType t, void* base, size_t mappedSize) {
  MOZ_ASSERT(mappedSize % gc::SystemPageSize() == 0);

#ifdef XP_WIN
  VirtualFree(base, 0, MEM_RELEASE);
#elif defined(__wasi__)
  free(base);
#else
  munmap(base, mappedSize);
#endif  // XP_WIN

#if defined(MOZ_VALGRIND) && \
    defined(VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE)
  VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE((unsigned char*)base,
                                                mappedSize);
#endif

  if (wasm::IsHugeMemoryEnabled(t)) {
    // Decrement the buffer counter at the end -- otherwise, a race condition
    // could enable the creation of unlimited buffers.
    //
    // Also see comment in MapBufferMemory about overestimation due to the
    // imprecision of the above guard.
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
    "ArrayBuffer.prototype", JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer),
    JS_NULL_CLASS_OPS, &ArrayBufferObjectClassSpec};

static bool IsArrayBuffer(HandleValue v) {
  return v.isObject() && v.toObject().is<ArrayBufferObject>();
}

MOZ_ALWAYS_INLINE bool ArrayBufferObject::byteLengthGetterImpl(
    JSContext* cx, const CallArgs& args) {
  MOZ_ASSERT(IsArrayBuffer(args.thisv()));
  auto* buffer = &args.thisv().toObject().as<ArrayBufferObject>();
  args.rval().setNumber(buffer->byteLength());
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
  if (!CheckArrayBufferTooLarge(cx, byteLength)) {
    return false;
  }

  // 24.1.1.1, steps 1 and 4-6.
  JSObject* bufobj = createZeroed(cx, byteLength, proto);
  if (!bufobj) {
    return false;
  }
  args.rval().setObject(*bufobj);
  return true;
}

using ArrayBufferContents = UniquePtr<uint8_t[], JS::FreePolicy>;

static ArrayBufferContents AllocateUninitializedArrayBufferContents(
    JSContext* cx, size_t nbytes) {
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
                                                       size_t nbytes) {
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

  // Update all views of the buffer to account for the buffer having been
  // detached, and clear the buffer's data and list of views.
  //
  // Typed object buffers are not exposed and cannot be detached.

  auto& innerViews = ObjectRealm::get(buffer).innerViews.get();
  if (InnerViewTable::ViewVector* views =
          innerViews.maybeViewsUnbarriered(buffer)) {
    for (size_t i = 0; i < views->length(); i++) {
      JSObject* view = (*views)[i];
      view->as<ArrayBufferViewObject>().notifyBufferDetached();
    }
    innerViews.removeViews(buffer);
  }
  if (JSObject* view = buffer->firstView()) {
    view->as<ArrayBufferViewObject>().notifyBufferDetached();
    buffer->setFirstView(nullptr);
  }

  if (buffer->dataPointer()) {
    buffer->releaseData(cx->runtime()->defaultFreeOp());
    buffer->setDataPointer(BufferContents::createNoData());
  }

  buffer->setByteLength(0);
  buffer->setIsDetached();
}

/* clang-format off */
/*
 * [SMDOC] WASM Linear Memory structure
 *
 * Wasm Raw Buf Linear Memory Structure
 *
 * The linear heap in Wasm is an mmaped array buffer. Several constants manage
 * its lifetime:
 *
 *  - byteLength - the wasm-visible current length of the buffer in
 *    bytes. Accesses in the range [0, byteLength] succeed. May only increase.
 *
 *  - boundsCheckLimit - the size against which we perform bounds checks.  The
 *    value of this depends on the bounds checking strategy chosen for the array
 *    buffer and the specific bounds checking semantics.  For asm.js code and
 *    for wasm code running with explicit bounds checking, it is the always the
 *    same as the byteLength.  For wasm code using the huge-memory trick, it is
 *    always wasm::GuardSize smaller than mappedSize.
 *
 *    See also "Linear memory addresses and bounds checking" in
 *    wasm/WasmMemory.cpp.
 *
 *    See also WasmMemoryObject::boundsCheckLimit().
 *
 *  - sourceMaxSize - the optional declared limit on how far byteLength can grow
 *    in pages. This is the unmodified maximum size from the source module or
 *    JS-API invocation. This may not be representable in byte lengths, nor
 *    feasible for a module to actually grow to due to implementation limits.
 *    It is used for correct linking checks and js-types reflection.
 *
 *  - clampedMaxSize - the maximum size on how far the byteLength can grow in
 *    pages. This value respects implementation limits and is always
 *    representable as a byte length. Every memory has a clampedMaxSize, even if
 *    no maximum was specified in source. When a memory has no sourceMaxSize,
 *    the clampedMaxSize will be the maximum amount of memory that can be grown
 *    to while still respecting implementation limits.
 *
 *  - mappedSize - the actual mmapped size. Access in the range [0, mappedSize]
 *    will either succeed, or be handled by the wasm signal handlers. If
 *    sourceMaxSize is present at initialization, then we attempt to map the
 *    whole clampedMaxSize. Otherwise we only map the region needed for the
 *    initial size.
 *
 * The below diagram shows the layout of the wasm heap. The wasm-visible portion
 * of the heap starts at 0. There is one extra page prior to the start of the
 * wasm heap which contains the WasmArrayRawBuffer struct at its end (i.e. right
 * before the start of the WASM heap).
 *
 *  WasmArrayRawBuffer
 *      \    ArrayBufferObject::dataPointer()
 *       \  /
 *        \ |
 *  ______|_|______________________________________________________
 * |______|_|______________|___________________|___________________|
 *          0          byteLength          clampedMaxSize     mappedSize
 *
 * \_______________________/
 *          COMMITED
 *                          \_____________________________________/
 *                                           SLOP
 * \______________________________________________________________/
 *                         MAPPED
 *
 * Invariants on byteLength, clampedMaxSize, and mappedSize:
 *  - byteLength only increases
 *  - 0 <= byteLength <= clampedMaxSize <= mappedSize
 *  - if sourceMaxSize is not specified, mappedSize may grow.
 *    It is otherwise constant.
 *  - initialLength <= clampedMaxSize <= sourceMaxSize (if present)
 *  - clampedMaxSize <= wasm::MaxMemoryPages()
 *
 * Invariants on boundsCheckLimit:
 *  - for wasm code with the huge-memory trick,
 *      clampedMaxSize <= boundsCheckLimit <= mappedSize
 *  - for asm.js code or wasm with explicit bounds checking,
 *      byteLength == boundsCheckLimit <= clampedMaxSize
 *  - on ARM, boundsCheckLimit must be a valid ARM immediate.
 *  - if sourceMaxSize is not specified, boundsCheckLimit may grow as
 *    mappedSize grows. They are otherwise constant.

 * NOTE: For asm.js on 32-bit platforms and on all platforms when running with
 * explicit bounds checking, we guarantee that
 *
 *   byteLength == maxSize == boundsCheckLimit == mappedSize
 *
 * That is, signal handlers will not be invoked.
 *
 * The region between byteLength and mappedSize is the SLOP - an area where we use
 * signal handlers to catch things that slip by bounds checks. Logically it has
 * two parts:
 *
 *  - from byteLength to boundsCheckLimit - this part of the SLOP serves to catch
 *    accesses to memory we have reserved but not yet grown into. This allows us
 *    to grow memory up to max (when present) without having to patch/update the
 *    bounds checks.
 *
 *  - from boundsCheckLimit to mappedSize - this part of the SLOP allows us to
 *    bounds check against base pointers and fold some constant offsets inside
 *    loads. This enables better Bounds Check Elimination.  See "Linear memory
 *    addresses and bounds checking" in wasm/WasmMemory.cpp.
 *
 */
/* clang-format on */

[[nodiscard]] bool WasmArrayRawBuffer::growToPagesInPlace(Pages newPages) {
  size_t newSize = newPages.byteLength();
  size_t oldSize = byteLength();

  MOZ_ASSERT(newSize >= oldSize);
  MOZ_ASSERT(newPages <= clampedMaxPages());
  MOZ_ASSERT(newSize <= mappedSize());

  size_t delta = newSize - oldSize;
  MOZ_ASSERT(delta % wasm::PageSize == 0);

  uint8_t* dataEnd = dataPointer() + oldSize;
  MOZ_ASSERT(uintptr_t(dataEnd) % gc::SystemPageSize() == 0);

  if (delta && !CommitBufferMemory(dataEnd, delta)) {
    return false;
  }

  length_ = newSize;

  return true;
}

bool WasmArrayRawBuffer::extendMappedSize(Pages maxPages) {
  size_t newMappedSize = wasm::ComputeMappedSize(maxPages);
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

void WasmArrayRawBuffer::tryGrowMaxPagesInPlace(Pages deltaMaxPages) {
  Pages newMaxPages = clampedMaxPages_;

  DebugOnly<bool> valid = newMaxPages.checkedIncrement(deltaMaxPages);
  // Caller must ensure increment does not overflow or increase over the
  // specified maximum pages.
  MOZ_ASSERT(valid);
  MOZ_ASSERT_IF(sourceMaxPages_.isSome(), newMaxPages <= *sourceMaxPages_);

  if (!extendMappedSize(newMaxPages)) {
    return;
  }
  clampedMaxPages_ = newMaxPages;
}

/* static */
WasmArrayRawBuffer* WasmArrayRawBuffer::AllocateWasm(
    IndexType indexType, Pages initialPages, Pages clampedMaxPages,
    const Maybe<Pages>& sourceMaxPages, const Maybe<size_t>& mapped) {
  // Prior code has asserted that initial pages is within our implementation
  // limits (wasm::MaxMemoryPages) and we can assume it is a valid size_t.
  MOZ_ASSERT(initialPages.hasByteLength());
  size_t numBytes = initialPages.byteLength();

  // If there is a specified maximum, attempt to map the whole range for
  // clampedMaxPages. Or else map only what's required for initialPages.
  Pages initialMappedPages =
      sourceMaxPages.isSome() ? clampedMaxPages : initialPages;

  // Use an override mapped size, or else compute the mapped size from
  // initialMappedPages.
  size_t mappedSize =
      mapped.isSome() ? *mapped : wasm::ComputeMappedSize(initialMappedPages);

  MOZ_RELEASE_ASSERT(mappedSize <= SIZE_MAX - gc::SystemPageSize());
  MOZ_RELEASE_ASSERT(numBytes <= SIZE_MAX - gc::SystemPageSize());
  MOZ_RELEASE_ASSERT(initialPages <= clampedMaxPages);
  MOZ_ASSERT(numBytes % gc::SystemPageSize() == 0);
  MOZ_ASSERT(mappedSize % gc::SystemPageSize() == 0);

  uint64_t mappedSizeWithHeader = mappedSize + gc::SystemPageSize();
  uint64_t numBytesWithHeader = numBytes + gc::SystemPageSize();

  void* data = MapBufferMemory(indexType, (size_t)mappedSizeWithHeader,
                               (size_t)numBytesWithHeader);
  if (!data) {
    return nullptr;
  }

  uint8_t* base = reinterpret_cast<uint8_t*>(data) + gc::SystemPageSize();
  uint8_t* header = base - sizeof(WasmArrayRawBuffer);

  auto rawBuf = new (header) WasmArrayRawBuffer(
      indexType, base, clampedMaxPages, sourceMaxPages, mappedSize, numBytes);
  return rawBuf;
}

/* static */
void WasmArrayRawBuffer::Release(void* mem) {
  WasmArrayRawBuffer* header =
      (WasmArrayRawBuffer*)((uint8_t*)mem - sizeof(WasmArrayRawBuffer));

  MOZ_RELEASE_ASSERT(header->mappedSize() <= SIZE_MAX - gc::SystemPageSize());
  size_t mappedSizeWithHeader = header->mappedSize() + gc::SystemPageSize();

  UnmapBufferMemory(header->indexType(), header->basePointer(),
                    mappedSizeWithHeader);
}

WasmArrayRawBuffer* ArrayBufferObject::BufferContents::wasmBuffer() const {
  MOZ_RELEASE_ASSERT(kind_ == WASM);
  return (WasmArrayRawBuffer*)(data_ - sizeof(WasmArrayRawBuffer));
}

template <typename ObjT, typename RawbufT>
static bool CreateSpecificWasmBuffer(
    JSContext* cx, const wasm::MemoryDesc& memory,
    MutableHandleArrayBufferObjectMaybeShared maybeSharedObject) {
  bool useHugeMemory = wasm::IsHugeMemoryEnabled(memory.indexType());
  Pages initialPages = memory.initialPages();
  Maybe<Pages> sourceMaxPages = memory.maximumPages();
  Pages clampedMaxPages = wasm::ClampedMaxPages(
      memory.indexType(), initialPages, sourceMaxPages, useHugeMemory);

  Maybe<size_t> mappedSize;
#ifdef WASM_SUPPORTS_HUGE_MEMORY
  // Override the mapped size if we are using huge memory. If we are not, then
  // it will be calculated by the raw buffer we are using.
  if (useHugeMemory) {
    mappedSize = Some(wasm::HugeMappedSize);
  }
#endif

  RawbufT* buffer =
      RawbufT::AllocateWasm(memory.limits.indexType, initialPages,
                            clampedMaxPages, sourceMaxPages, mappedSize);
  if (!buffer) {
    if (useHugeMemory) {
      WarnNumberASCII(cx, JSMSG_WASM_HUGE_MEMORY_FAILED);
      if (cx->isExceptionPending()) {
        cx->clearPendingException();
      }

      ReportOutOfMemory(cx);
      return false;
    }

    // If we fail, and have a sourceMaxPages, try to reserve the biggest
    // chunk in the range [initialPages, clampedMaxPages) using log backoff.
    if (!sourceMaxPages) {
      wasm::Log(cx, "new Memory({initial=%" PRIu64 " pages}) failed",
                initialPages.value());
      ReportOutOfMemory(cx);
      return false;
    }

    uint64_t cur = clampedMaxPages.value() / 2;
    for (; Pages(cur) > initialPages; cur /= 2) {
      buffer = RawbufT::AllocateWasm(memory.limits.indexType, initialPages,
                                     Pages(cur), sourceMaxPages, mappedSize);
      if (buffer) {
        break;
      }
    }

    if (!buffer) {
      wasm::Log(cx, "new Memory({initial=%" PRIu64 " pages}) failed",
                initialPages.value());
      ReportOutOfMemory(cx);
      return false;
    }

    // Try to grow our chunk as much as possible.
    for (size_t d = cur / 2; d >= 1; d /= 2) {
      buffer->tryGrowMaxPagesInPlace(Pages(d));
    }
  }

  // ObjT::createFromNewRawBuffer assumes ownership of |buffer| even in case
  // of failure.
  RootedArrayBufferObjectMaybeShared object(
      cx, ObjT::createFromNewRawBuffer(cx, buffer, initialPages.byteLength()));
  if (!object) {
    return false;
  }

  maybeSharedObject.set(object);

  // See MaximumLiveMappedBuffers comment above.
  if (liveBufferCount > StartSyncFullGCAtLiveBufferCount) {
    JS::PrepareForFullGC(cx);
    JS::NonIncrementalGC(cx, JS::GCOptions::Normal,
                         JS::GCReason::TOO_MUCH_WASM_MEMORY);
    allocatedSinceLastTrigger = 0;
  } else if (liveBufferCount > StartTriggeringAtLiveBufferCount) {
    allocatedSinceLastTrigger++;
    if (allocatedSinceLastTrigger > AllocatedBuffersPerTrigger) {
      (void)cx->runtime()->gc.triggerGC(JS::GCReason::TOO_MUCH_WASM_MEMORY);
      allocatedSinceLastTrigger = 0;
    }
  } else {
    allocatedSinceLastTrigger = 0;
  }

  // Log the result with details on the memory allocation
  if (sourceMaxPages) {
    if (useHugeMemory) {
      wasm::Log(cx,
                "new Memory({initial:%" PRIu64 " pages, maximum:%" PRIu64
                " pages}) succeeded",
                initialPages.value(), sourceMaxPages->value());
    } else {
      wasm::Log(cx,
                "new Memory({initial:%" PRIu64 " pages, maximum:%" PRIu64
                " pages}) succeeded "
                "with internal maximum of %" PRIu64 " pages",
                initialPages.value(), sourceMaxPages->value(),
                object->wasmClampedMaxPages().value());
    }
  } else {
    wasm::Log(cx, "new Memory({initial:%" PRIu64 " pages}) succeeded",
              initialPages.value());
  }

  return true;
}

bool js::CreateWasmBuffer(JSContext* cx, const wasm::MemoryDesc& memory,
                          MutableHandleArrayBufferObjectMaybeShared buffer) {
  MOZ_RELEASE_ASSERT(memory.initialPages() <=
                     wasm::MaxMemoryPages(memory.indexType()));
  MOZ_RELEASE_ASSERT(cx->wasm().haveSignalHandlers);

  if (memory.isShared()) {
    if (!cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_WASM_NO_SHMEM_LINK);
      return false;
    }
    return CreateSpecificWasmBuffer<SharedArrayBufferObject,
                                    SharedArrayRawBuffer>(cx, memory, buffer);
  }
  return CreateSpecificWasmBuffer<ArrayBufferObject, WasmArrayRawBuffer>(
      cx, memory, buffer);
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

size_t ArrayBufferObject::byteLength() const {
  return size_t(getFixedSlot(BYTE_LENGTH_SLOT).toPrivate());
}

inline size_t ArrayBufferObject::associatedBytes() const {
  if (bufferKind() == MALLOCED) {
    return byteLength();
  }
  if (bufferKind() == MAPPED) {
    return RoundUp(byteLength(), js::gc::SystemPageSize());
  }
  MOZ_CRASH("Unexpected buffer kind");
}

void ArrayBufferObject::setByteLength(size_t length) {
  MOZ_ASSERT(length <= maxBufferByteLength());
  setFixedSlot(BYTE_LENGTH_SLOT, PrivateValue(length));
}

size_t ArrayBufferObject::wasmMappedSize() const {
  if (isWasm()) {
    return contents().wasmBuffer()->mappedSize();
  }
  return byteLength();
}

IndexType ArrayBufferObject::wasmIndexType() const {
  if (isWasm()) {
    return contents().wasmBuffer()->indexType();
  }
  MOZ_ASSERT(isPreparedForAsmJS());
  return wasm::IndexType::I32;
}

Pages ArrayBufferObject::wasmPages() const {
  if (isWasm()) {
    return contents().wasmBuffer()->pages();
  }
  MOZ_ASSERT(isPreparedForAsmJS());
  return Pages::fromByteLengthExact(byteLength());
}

Pages ArrayBufferObject::wasmClampedMaxPages() const {
  if (isWasm()) {
    return contents().wasmBuffer()->clampedMaxPages();
  }
  MOZ_ASSERT(isPreparedForAsmJS());
  return Pages::fromByteLengthExact(byteLength());
}

Maybe<Pages> ArrayBufferObject::wasmSourceMaxPages() const {
  if (isWasm()) {
    return contents().wasmBuffer()->sourceMaxPages();
  }
  MOZ_ASSERT(isPreparedForAsmJS());
  return Some<Pages>(Pages::fromByteLengthExact(byteLength()));
}

size_t js::WasmArrayBufferMappedSize(const ArrayBufferObjectMaybeShared* buf) {
  if (buf->is<ArrayBufferObject>()) {
    return buf->as<ArrayBufferObject>().wasmMappedSize();
  }
  return buf->as<SharedArrayBufferObject>().wasmMappedSize();
}

IndexType js::WasmArrayBufferIndexType(
    const ArrayBufferObjectMaybeShared* buf) {
  if (buf->is<ArrayBufferObject>()) {
    return buf->as<ArrayBufferObject>().wasmIndexType();
  }
  return buf->as<SharedArrayBufferObject>().wasmIndexType();
}
Pages js::WasmArrayBufferPages(const ArrayBufferObjectMaybeShared* buf) {
  if (buf->is<ArrayBufferObject>()) {
    return buf->as<ArrayBufferObject>().wasmPages();
  }
  return buf->as<SharedArrayBufferObject>().volatileWasmPages();
}
Pages js::WasmArrayBufferClampedMaxPages(
    const ArrayBufferObjectMaybeShared* buf) {
  if (buf->is<ArrayBufferObject>()) {
    return buf->as<ArrayBufferObject>().wasmClampedMaxPages();
  }
  return buf->as<SharedArrayBufferObject>().wasmClampedMaxPages();
}
Maybe<Pages> js::WasmArrayBufferSourceMaxPages(
    const ArrayBufferObjectMaybeShared* buf) {
  if (buf->is<ArrayBufferObject>()) {
    return buf->as<ArrayBufferObject>().wasmSourceMaxPages();
  }
  return Some(buf->as<SharedArrayBufferObject>().wasmSourceMaxPages());
}

static void CheckStealPreconditions(Handle<ArrayBufferObject*> buffer,
                                    JSContext* cx) {
  cx->check(buffer);

  MOZ_ASSERT(!buffer->isDetached(), "can't steal from a detached buffer");
  MOZ_ASSERT(!buffer->isPreparedForAsmJS(),
             "asm.js-prepared buffers don't have detachable/stealable data");
}

/* static */
bool ArrayBufferObject::wasmGrowToPagesInPlace(
    wasm::IndexType t, Pages newPages, HandleArrayBufferObject oldBuf,
    MutableHandleArrayBufferObject newBuf, JSContext* cx) {
  CheckStealPreconditions(oldBuf, cx);

  MOZ_ASSERT(oldBuf->isWasm());

  // Check that the new pages is within our allowable range. This will
  // simultaneously check against the maximum specified in source and our
  // implementation limits.
  if (newPages > oldBuf->wasmClampedMaxPages()) {
    return false;
  }
  MOZ_ASSERT(newPages <= wasm::MaxMemoryPages(t) &&
             newPages.byteLength() <= ArrayBufferObject::maxBufferByteLength());

  // We have checked against the clamped maximum and so we know we can convert
  // to byte lengths now.
  size_t newSize = newPages.byteLength();

  // On failure, do not throw and ensure that the original buffer is
  // unmodified and valid. After WasmArrayRawBuffer::growToPagesInPlace(), the
  // wasm-visible length of the buffer has been increased so it must be the
  // last fallible operation.

  newBuf.set(ArrayBufferObject::createEmpty(cx));
  if (!newBuf) {
    cx->clearPendingException();
    return false;
  }

  MOZ_ASSERT(newBuf->isNoData());

  if (!oldBuf->contents().wasmBuffer()->growToPagesInPlace(newPages)) {
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
bool ArrayBufferObject::wasmMovingGrowToPages(
    IndexType t, Pages newPages, HandleArrayBufferObject oldBuf,
    MutableHandleArrayBufferObject newBuf, JSContext* cx) {
  // On failure, do not throw and ensure that the original buffer is
  // unmodified and valid.

  // Check that the new pages is within our allowable range. This will
  // simultaneously check against the maximum specified in source and our
  // implementation limits.
  if (newPages > oldBuf->wasmClampedMaxPages()) {
    return false;
  }
  MOZ_ASSERT(newPages <= wasm::MaxMemoryPages(t) &&
             newPages.byteLength() < ArrayBufferObject::maxBufferByteLength());

  // We have checked against the clamped maximum and so we know we can convert
  // to byte lengths now.
  size_t newSize = newPages.byteLength();

  if (wasm::ComputeMappedSize(newPages) <= oldBuf->wasmMappedSize() ||
      oldBuf->contents().wasmBuffer()->extendMappedSize(newPages)) {
    return wasmGrowToPagesInPlace(t, newPages, oldBuf, newBuf, cx);
  }

  newBuf.set(ArrayBufferObject::createEmpty(cx));
  if (!newBuf) {
    cx->clearPendingException();
    return false;
  }

  Pages clampedMaxPages =
      wasm::ClampedMaxPages(t, newPages, Nothing(), /* hugeMemory */ false);
  WasmArrayRawBuffer* newRawBuf = WasmArrayRawBuffer::AllocateWasm(
      oldBuf->wasmIndexType(), newPages, clampedMaxPages, Nothing(), Nothing());
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

static ArrayBufferObject* NewArrayBufferObject(JSContext* cx,
                                               HandleObject proto_,
                                               gc::AllocKind allocKind) {
  MOZ_ASSERT(allocKind == gc::AllocKind::ARRAYBUFFER4 ||
             allocKind == gc::AllocKind::ARRAYBUFFER8 ||
             allocKind == gc::AllocKind::ARRAYBUFFER12 ||
             allocKind == gc::AllocKind::ARRAYBUFFER16);

  RootedObject proto(cx, proto_);
  if (!proto) {
    proto = GlobalObject::getOrCreatePrototype(cx, JSProto_ArrayBuffer);
    if (!proto) {
      return nullptr;
    }
  }

  const JSClass* clasp = &ArrayBufferObject::class_;

  // Array buffers can store data inline so we only use fixed slots to cover the
  // reserved slots, ignoring the AllocKind.
  MOZ_ASSERT(ClassCanHaveFixedData(clasp));
  constexpr size_t nfixed = ArrayBufferObject::RESERVED_SLOTS;
  static_assert(nfixed <= NativeObject::MAX_FIXED_SLOTS);

  RootedShape shape(cx, SharedShape::getInitialShape(cx, clasp, cx->realm(),
                                                     AsTaggedProto(proto),
                                                     nfixed, ObjectFlags()));
  if (!shape) {
    return nullptr;
  }

  // Array buffers can't be nursery allocated but can be background-finalized.
  MOZ_ASSERT(IsBackgroundFinalized(allocKind));
  MOZ_ASSERT(!CanNurseryAllocateFinalizedClass(clasp));
  constexpr gc::InitialHeap heap = gc::TenuredHeap;

  NativeObject* obj = NativeObject::create(cx, allocKind, heap, shape);
  if (!obj) {
    return nullptr;
  }

  return &obj->as<ArrayBufferObject>();
}

// Creates a new ArrayBufferObject with %ArrayBuffer.prototype% as proto and no
// space for inline data.
static ArrayBufferObject* NewArrayBufferObject(JSContext* cx) {
  static_assert(ArrayBufferObject::RESERVED_SLOTS == 4);
  return NewArrayBufferObject(cx, nullptr, gc::AllocKind::ARRAYBUFFER4);
}

ArrayBufferObject* ArrayBufferObject::createForContents(
    JSContext* cx, size_t nbytes, BufferContents contents) {
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
  constexpr size_t reservedSlots = ArrayBufferObject::RESERVED_SLOTS;

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

  gc::AllocKind allocKind = GetArrayBufferGCObjectKind(nslots);

  AutoSetNewObjectMetadata metadata(cx);
  Rooted<ArrayBufferObject*> buffer(
      cx, NewArrayBufferObject(cx, nullptr, allocKind));
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
    JSContext* cx, size_t nbytes, AutoSetNewObjectMetadata&,
    JS::Handle<JSObject*> proto /* = nullptr */) {
  MOZ_ASSERT(nbytes <= ArrayBufferObject::maxBufferByteLength(),
             "caller must validate the byte count it passes");

  // Try fitting the data inline with the object by repurposing fixed-slot
  // storage.  Add extra fixed slots if necessary to accomplish this, but don't
  // exceed the maximum number of fixed slots!
  size_t nslots = ArrayBufferObject::RESERVED_SLOTS;
  ArrayBufferContents data;
  if (nbytes <= MaxInlineBytes) {
    int newSlots = HowMany(nbytes, sizeof(Value));
    MOZ_ASSERT(int(nbytes) <= newSlots * int(sizeof(Value)));

    nslots += newSlots;
  } else {
    data = FillType == FillContents::Uninitialized
               ? AllocateUninitializedArrayBufferContents(cx, nbytes)
               : AllocateArrayBufferContents(cx, nbytes);
    if (!data) {
      return {nullptr, nullptr};
    }
  }

  gc::AllocKind allocKind = GetArrayBufferGCObjectKind(nslots);

  ArrayBufferObject* buffer = NewArrayBufferObject(cx, proto, allocKind);
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

  size_t nbytes = unwrappedArrayBuffer->byteLength();

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
    JSContext* cx, size_t nbytes, HandleObject proto /* = nullptr */) {
  // 24.1.1.1, step 3 (Inlined 6.2.6.1 CreateByteDataBlock, step 2).
  if (!CheckArrayBufferTooLarge(cx, nbytes)) {
    return nullptr;
  }

  AutoSetNewObjectMetadata metadata(cx);
  auto [buffer, toFill] =
      createBufferAndData<FillContents::Zero>(cx, nbytes, metadata, proto);
  (void)toFill;
  return buffer;
}

ArrayBufferObject* ArrayBufferObject::createEmpty(JSContext* cx) {
  AutoSetNewObjectMetadata metadata(cx);
  ArrayBufferObject* obj = NewArrayBufferObject(cx);
  if (!obj) {
    return nullptr;
  }

  obj->initialize(0, BufferContents::createNoData());
  return obj;
}

ArrayBufferObject* ArrayBufferObject::createFromNewRawBuffer(
    JSContext* cx, WasmArrayRawBuffer* rawBuffer, size_t initialSize) {
  AutoSetNewObjectMetadata metadata(cx);
  ArrayBufferObject* buffer = NewArrayBufferObject(cx);
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
    JSObject* obj, mozilla::MallocSizeOf mallocSizeOf, JS::ClassInfo* info,
    JS::RuntimeSizes* runtimeSizes) {
  auto& buffer = obj->as<ArrayBufferObject>();
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
    case EXTERNAL:
      // External data will be accounted for by the owner of the buffer,
      // not this view.
      break;
    case MAPPED:
      info->objectsNonHeapElementsNormal += buffer.byteLength();
      break;
    case WASM:
      if (!buffer.isDetached()) {
        info->objectsNonHeapElementsWasm += buffer.byteLength();
        if (runtimeSizes) {
          MOZ_ASSERT(buffer.wasmMappedSize() >= buffer.byteLength());
          runtimeSizes->wasmGuardPages +=
              buffer.wasmMappedSize() - buffer.byteLength();
        }
      }
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
                                 size_t toIndex,
                                 Handle<ArrayBufferObject*> fromBuffer,
                                 size_t fromIndex, size_t count) {
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

bool InnerViewTable::traceWeak(JSTracer* trc) { return map.traceWeak(trc); }

void InnerViewTable::sweepAfterMinorGC(JSTracer* trc) {
  MOZ_ASSERT(needsSweepAfterMinorGC());

  if (nurseryKeysValid) {
    for (size_t i = 0; i < nurseryKeys.length(); i++) {
      JSObject* buffer = MaybeForwarded(nurseryKeys[i]);
      Map::Ptr p = map.lookup(buffer);
      if (p &&
          !Map::EntryGCPolicy::traceWeak(trc, &p->mutableKey(), &p->value())) {
        map.remove(p);
      }
    }
  } else {
    // Do the required sweeping by looking at every map entry.
    map.traceWeak(trc);
  }

  nurseryKeys.clear();
  nurseryKeysValid = true;
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

JS_PUBLIC_API size_t JS::GetArrayBufferByteLength(JSObject* obj) {
  ArrayBufferObject* aobj = obj->maybeUnwrapAs<ArrayBufferObject>();
  return aobj ? aobj->byteLength() : 0;
}

JS_PUBLIC_API uint8_t* JS::GetArrayBufferData(JSObject* obj,
                                              bool* isSharedMemory,
                                              const JS::AutoRequireNoGC&) {
  ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>();
  if (!aobj) {
    return nullptr;
  }
  *isSharedMemory = false;
  return aobj->dataPointer();
}

static ArrayBufferObject* UnwrapOrReportArrayBuffer(
    JSContext* cx, JS::Handle<JSObject*> maybeArrayBuffer) {
  JSObject* obj = CheckedUnwrapStatic(maybeArrayBuffer);
  if (!obj) {
    ReportAccessDenied(cx);
    return nullptr;
  }

  if (!obj->is<ArrayBufferObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_ARRAYBUFFER_REQUIRED);
    return nullptr;
  }

  return &obj->as<ArrayBufferObject>();
}

JS_PUBLIC_API bool JS::DetachArrayBuffer(JSContext* cx, HandleObject obj) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(obj);

  Rooted<ArrayBufferObject*> unwrappedBuffer(
      cx, UnwrapOrReportArrayBuffer(cx, obj));
  if (!unwrappedBuffer) {
    return false;
  }

  if (unwrappedBuffer->isWasm() || unwrappedBuffer->isPreparedForAsmJS()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_NO_TRANSFER);
    return false;
  }

  AutoRealm ar(cx, unwrappedBuffer);
  ArrayBufferObject::detach(cx, unwrappedBuffer);
  return true;
}

JS_PUBLIC_API bool JS::IsDetachedArrayBufferObject(JSObject* obj) {
  ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>();
  if (!aobj) {
    return false;
  }

  return aobj->isDetached();
}

JS_PUBLIC_API JSObject* JS::NewArrayBuffer(JSContext* cx, size_t nbytes) {
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

JS_PUBLIC_API JSObject* JS::CopyArrayBuffer(JSContext* cx,
                                            Handle<JSObject*> arrayBuffer) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  MOZ_ASSERT(arrayBuffer != nullptr);

  Rooted<ArrayBufferObject*> unwrappedSource(
      cx, UnwrapOrReportArrayBuffer(cx, arrayBuffer));
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

JS_PUBLIC_API bool JS::IsArrayBufferObject(JSObject* obj) {
  return obj->canUnwrapAs<ArrayBufferObject>();
}

JS_PUBLIC_API bool JS::ArrayBufferHasData(JSObject* obj) {
  return !obj->unwrapAs<ArrayBufferObject>().isDetached();
}

JS_PUBLIC_API JSObject* JS::UnwrapArrayBuffer(JSObject* obj) {
  return obj->maybeUnwrapIf<ArrayBufferObject>();
}

JS_PUBLIC_API JSObject* JS::UnwrapSharedArrayBuffer(JSObject* obj) {
  return obj->maybeUnwrapIf<SharedArrayBufferObject>();
}

JS_PUBLIC_API void* JS::StealArrayBufferContents(JSContext* cx,
                                                 HandleObject obj) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(obj);

  Rooted<ArrayBufferObject*> unwrappedBuffer(
      cx, UnwrapOrReportArrayBuffer(cx, obj));
  if (!unwrappedBuffer) {
    return nullptr;
  }

  if (unwrappedBuffer->isDetached()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TYPED_ARRAY_DETACHED);
    return nullptr;
  }

  if (unwrappedBuffer->isWasm() || unwrappedBuffer->isPreparedForAsmJS()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_NO_TRANSFER);
    return nullptr;
  }

  AutoRealm ar(cx, unwrappedBuffer);
  return ArrayBufferObject::stealMallocedContents(cx, unwrappedBuffer);
}

JS_PUBLIC_API void JS::SetLargeArrayBuffersEnabled(bool enable) {
  ArrayBufferObject::supportLargeBuffers = enable;
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

JS_PUBLIC_API bool JS::IsMappedArrayBufferObject(JSObject* obj) {
  ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>();
  if (!aobj) {
    return false;
  }

  return aobj->isMapped();
}

JS_PUBLIC_API JSObject* JS::GetObjectAsArrayBuffer(JSObject* obj,
                                                   size_t* length,
                                                   uint8_t** data) {
  ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>();
  if (!aobj) {
    return nullptr;
  }

  *length = aobj->byteLength();
  *data = aobj->dataPointer();

  return aobj;
}

JS_PUBLIC_API void JS::GetArrayBufferLengthAndData(JSObject* obj,
                                                   size_t* length,
                                                   bool* isSharedMemory,
                                                   uint8_t** data) {
  auto& aobj = obj->as<ArrayBufferObject>();
  *length = aobj.byteLength();
  *data = aobj.dataPointer();
  *isSharedMemory = false;
}

const JSClass* const JS::ArrayBuffer::UnsharedClass =
    &ArrayBufferObject::class_;
const JSClass* const JS::ArrayBuffer::SharedClass =
    &SharedArrayBufferObject::class_;

/* static */ JS::ArrayBuffer JS::ArrayBuffer::create(JSContext* cx,
                                                     size_t nbytes) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  return JS::ArrayBuffer(ArrayBufferObject::createZeroed(cx, nbytes));
}

uint8_t* JS::ArrayBuffer::getLengthAndData(size_t* length, bool* isSharedMemory,
                                           const JS::AutoRequireNoGC& nogc) {
  auto* buffer = obj->maybeUnwrapAs<ArrayBufferObjectMaybeShared>();
  if (!buffer) {
    return nullptr;
  }
  *length = buffer->byteLength();
  if (buffer->is<SharedArrayBufferObject>()) {
    *isSharedMemory = true;
    return buffer->dataPointerEither().unwrap();
  }
  *isSharedMemory = false;
  return buffer->as<ArrayBufferObject>().dataPointer();
};

JS::ArrayBuffer JS::ArrayBuffer::unwrap(JSObject* maybeWrapped) {
  if (!maybeWrapped) {
    return JS::ArrayBuffer(nullptr);
  }
  auto* ab = maybeWrapped->maybeUnwrapIf<ArrayBufferObjectMaybeShared>();
  return fromObject(ab);
}

void JS::ArrayBufferCopyData(JSContext* cx, Handle<JSObject*> toBlock,
                             size_t toIndex, Handle<JSObject*> fromBlock,
                             size_t fromIndex, size_t count) {
  MOZ_ASSERT(toBlock->is<ArrayBufferObjectMaybeShared>());
  MOZ_ASSERT(fromBlock->is<ArrayBufferObjectMaybeShared>());

  // If both are array bufferrs, can use ArrayBufferCopyData
  if (toBlock->is<ArrayBufferObject>() && fromBlock->is<ArrayBufferObject>()) {
    Rooted<ArrayBufferObject*> toArray(cx, &toBlock->as<ArrayBufferObject>());
    Rooted<ArrayBufferObject*> fromArray(cx,
                                         &fromBlock->as<ArrayBufferObject>());
    ArrayBufferObject::copyData(toArray, toIndex, fromArray, fromIndex, count);
    return;
  }

  Rooted<ArrayBufferObjectMaybeShared*> toArray(
      cx, &toBlock->as<ArrayBufferObjectMaybeShared>());
  Rooted<ArrayBufferObjectMaybeShared*> fromArray(
      cx, &toBlock->as<ArrayBufferObjectMaybeShared>());
  SharedArrayBufferObject::copyData(toArray, toIndex, fromArray, fromIndex,
                                    count);
}

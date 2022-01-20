/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmInstance.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"

#include <algorithm>
#include <utility>

#include "jsmath.h"

#include "jit/AtomicOperations.h"
#include "jit/Disassemble.h"
#include "jit/JitCommon.h"
#include "jit/JitRuntime.h"
#include "js/ForOfIterator.h"
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "vm/BigIntType.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "wasm/TypedObject.h"
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmDebugFrame.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmTypeDef.h"
#include "wasm/WasmValType.h"
#include "wasm/WasmValue.h"

#include "gc/StoreBuffer-inl.h"
#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::BitwiseCast;
using mozilla::CheckedInt;
using mozilla::DebugOnly;

using CheckedU32 = CheckedInt<uint32_t>;

//////////////////////////////////////////////////////////////////////////////
//
// Functions and invocation.

class FuncTypeIdSet {
  using Map =
      HashMap<const FuncType*, uint32_t, FuncTypeHashPolicy, SystemAllocPolicy>;
  Map map_;

 public:
  ~FuncTypeIdSet() {
    MOZ_ASSERT_IF(!JSRuntime::hasLiveRuntimes(), map_.empty());
  }

  bool allocateFuncTypeId(JSContext* cx, const FuncType& funcType,
                          const void** funcTypeId) {
    Map::AddPtr p = map_.lookupForAdd(funcType);
    if (p) {
      MOZ_ASSERT(p->value() > 0);
      p->value()++;
      *funcTypeId = p->key();
      return true;
    }

    UniquePtr<FuncType> clone = MakeUnique<FuncType>();
    if (!clone || !clone->clone(funcType) || !map_.add(p, clone.get(), 1)) {
      ReportOutOfMemory(cx);
      return false;
    }

    *funcTypeId = clone.release();
    MOZ_ASSERT(!(uintptr_t(*funcTypeId) & TypeIdDesc::ImmediateBit));
    return true;
  }

  void deallocateFuncTypeId(const FuncType& funcType, const void* funcTypeId) {
    Map::Ptr p = map_.lookup(funcType);
    MOZ_RELEASE_ASSERT(p && p->key() == funcTypeId && p->value() > 0);

    p->value()--;
    if (!p->value()) {
      js_delete(p->key());
      map_.remove(p);
    }
  }
};

ExclusiveData<FuncTypeIdSet> funcTypeIdSet(mutexid::WasmFuncTypeIdSet);

const void** Instance::addressOfTypeId(const TypeIdDesc& typeId) const {
  return (const void**)(globalData() + typeId.globalDataOffset());
}

FuncImportTls& Instance::funcImportTls(const FuncImport& fi) {
  return *(FuncImportTls*)(globalData() + fi.tlsDataOffset());
}

TableTls& Instance::tableTls(const TableDesc& td) const {
  return *(TableTls*)(globalData() + td.globalDataOffset);
}

void* Instance::checkedCallEntry(const uint32_t functionIndex,
                                 const Tier tier) const {
  uint8_t* codeBaseTier = codeBase(tier);
  const MetadataTier& metadataTier = metadata(tier);
  const CodeRangeVector& codeRanges = metadataTier.codeRanges;
  const Uint32Vector& funcToCodeRange = metadataTier.funcToCodeRange;
  return codeBaseTier +
         codeRanges[funcToCodeRange[functionIndex]].funcCheckedCallEntry();
}

static bool IsImportedFunction(const uint32_t functionIndex,
                               const MetadataTier& metadataTier) {
  return functionIndex < metadataTier.funcImports.length();
}

static bool IsJSExportedFunction(JSFunction* fun) {
  // There's an assumption here that the function is in fact an imported
  // function.  The caller must ensure this.
  return !IsWasmExportedFunction(fun);
}

// TODO: This could usefully be used in WasmValidate.cpp too.
static bool IsNullFunction(const uint32_t functionIndex) {
  return functionIndex == NullFuncIndex;
}

// TODO(1626251): Consolidate definitions into Iterable.h
static bool IterableToArray(JSContext* cx, HandleValue iterable,
                            MutableHandle<ArrayObject*> array) {
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(iterable, JS::ForOfIterator::ThrowOnNonIterable)) {
    return false;
  }

  array.set(NewDenseEmptyArray(cx));
  if (!array) {
    return false;
  }

  RootedValue nextValue(cx);
  while (true) {
    bool done;
    if (!iterator.next(&nextValue, &done)) {
      return false;
    }
    if (done) {
      break;
    }

    if (!NewbornArrayPush(cx, array, nextValue)) {
      return false;
    }
  }
  return true;
}

static bool UnpackResults(JSContext* cx, const ValTypeVector& resultTypes,
                          const Maybe<char*> stackResultsArea, uint64_t* argv,
                          MutableHandleValue rval) {
  if (!stackResultsArea) {
    MOZ_ASSERT(resultTypes.length() <= 1);
    // Result is either one scalar value to unpack to a wasm value, or
    // an ignored value for a zero-valued function.
    if (resultTypes.length() == 1) {
      return ToWebAssemblyValue(cx, rval, resultTypes[0], argv, true);
    }
    return true;
  }

  MOZ_ASSERT(stackResultsArea.isSome());
  RootedArrayObject array(cx);
  if (!IterableToArray(cx, rval, &array)) {
    return false;
  }

  if (resultTypes.length() != array->length()) {
    UniqueChars expected(JS_smprintf("%zu", resultTypes.length()));
    UniqueChars got(JS_smprintf("%u", array->length()));

    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_WRONG_NUMBER_OF_VALUES, expected.get(),
                             got.get());
    return false;
  }

  DebugOnly<uint64_t> previousOffset = ~(uint64_t)0;

  ABIResultIter iter(ResultType::Vector(resultTypes));
  // The values are converted in the order they are pushed on the
  // abstract WebAssembly stack; switch to iterate in push order.
  while (!iter.done()) {
    iter.next();
  }
  DebugOnly<bool> seenRegisterResult = false;
  for (iter.switchToPrev(); !iter.done(); iter.prev()) {
    const ABIResult& result = iter.cur();
    MOZ_ASSERT(!seenRegisterResult);
    // Use rval as a scratch area to hold the extracted result.
    rval.set(array->getDenseElement(iter.index()));
    if (result.inRegister()) {
      // Currently, if a function type has results, there can be only
      // one register result.  If there is only one result, it is
      // returned as a scalar and not an iterable, so we don't get here.
      // If there are multiple results, we extract the register result
      // and set `argv[0]` set to the extracted result, to be returned by
      // register in the stub.  The register result follows any stack
      // results, so this preserves conversion order.
      if (!ToWebAssemblyValue(cx, rval, result.type(), argv, true)) {
        return false;
      }
      seenRegisterResult = true;
      continue;
    }
    uint32_t result_size = result.size();
    MOZ_ASSERT(result_size == 4 || result_size == 8);
#ifdef DEBUG
    if (previousOffset == ~(uint64_t)0) {
      previousOffset = (uint64_t)result.stackOffset();
    } else {
      MOZ_ASSERT(previousOffset - (uint64_t)result_size ==
                 (uint64_t)result.stackOffset());
      previousOffset -= (uint64_t)result_size;
    }
#endif
    char* loc = stackResultsArea.value() + result.stackOffset();
    if (!ToWebAssemblyValue(cx, rval, result.type(), loc, result_size == 8)) {
      return false;
    }
  }

  return true;
}

bool Instance::callImport(JSContext* cx, uint32_t funcImportIndex,
                          unsigned argc, uint64_t* argv) {
  AssertRealmUnchanged aru(cx);

  Tier tier = code().bestTier();

  const FuncImport& fi = metadata(tier).funcImports[funcImportIndex];

  ArgTypeVector argTypes(fi.funcType());
  InvokeArgs args(cx);
  if (!args.init(cx, argTypes.lengthWithoutStackResults())) {
    return false;
  }

  if (fi.funcType().hasUnexposableArgOrRet()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_VAL_TYPE);
    return false;
  }

  MOZ_ASSERT(argTypes.lengthWithStackResults() == argc);
  Maybe<char*> stackResultPointer;
  for (size_t i = 0; i < argc; i++) {
    const void* rawArgLoc = &argv[i];
    if (argTypes.isSyntheticStackResultPointerArg(i)) {
      stackResultPointer = Some(*(char**)rawArgLoc);
      continue;
    }
    size_t naturalIndex = argTypes.naturalIndex(i);
    ValType type = fi.funcType().args()[naturalIndex];
    MutableHandleValue argValue = args[naturalIndex];
    if (!ToJSValue(cx, rawArgLoc, type, argValue)) {
      return false;
    }
  }

  FuncImportTls& import = funcImportTls(fi);
  RootedFunction importFun(cx, import.fun);
  MOZ_ASSERT(cx->realm() == importFun->realm());

  RootedValue fval(cx, ObjectValue(*importFun));
  RootedValue thisv(cx, UndefinedValue());
  RootedValue rval(cx);
  if (!Call(cx, fval, thisv, args, &rval)) {
    return false;
  }

  if (!UnpackResults(cx, fi.funcType().results(), stackResultPointer, argv,
                     &rval)) {
    return false;
  }

  if (!JitOptions.enableWasmJitExit) {
    return true;
  }

  // The import may already have become optimized.
  for (auto t : code().tiers()) {
    void* jitExitCode = codeBase(t) + fi.jitExitCodeOffset();
    if (import.code == jitExitCode) {
      return true;
    }
  }

  void* jitExitCode = codeBase(tier) + fi.jitExitCodeOffset();

  // Test if the function is JIT compiled.
  if (!importFun->hasBytecode()) {
    return true;
  }

  JSScript* script = importFun->nonLazyScript();
  if (!script->hasJitScript()) {
    return true;
  }

  // Skip if the function does not have a signature that allows for a JIT exit.
  if (!fi.canHaveJitExit()) {
    return true;
  }

  // Let's optimize it!

  import.code = jitExitCode;
  return true;
}

/* static */ int32_t /* 0 to signal trap; 1 to signal OK */
Instance::callImport_general(Instance* instance, int32_t funcImportIndex,
                             int32_t argc, uint64_t* argv) {
  JSContext* cx = instance->tlsData()->cx;
  return instance->callImport(cx, funcImportIndex, argc, argv);
}

//////////////////////////////////////////////////////////////////////////////
//
// Atomic operations and shared memory.

template <typename ValT, typename PtrT>
static int32_t PerformWait(Instance* instance, PtrT byteOffset, ValT value,
                           int64_t timeout_ns) {
  JSContext* cx = instance->tlsData()->cx;

  if (!instance->memory()->isShared()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_NONSHARED_WAIT);
    return -1;
  }

  if (byteOffset & (sizeof(ValT) - 1)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_UNALIGNED_ACCESS);
    return -1;
  }

  if (byteOffset + sizeof(ValT) > instance->memory()->volatileMemoryLength()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  mozilla::Maybe<mozilla::TimeDuration> timeout;
  if (timeout_ns >= 0) {
    timeout = mozilla::Some(
        mozilla::TimeDuration::FromMicroseconds(timeout_ns / 1000));
  }

  MOZ_ASSERT(byteOffset <= SIZE_MAX, "Bounds check is broken");
  switch (atomics_wait_impl(cx, instance->sharedMemoryBuffer(),
                            size_t(byteOffset), value, timeout)) {
    case FutexThread::WaitResult::OK:
      return 0;
    case FutexThread::WaitResult::NotEqual:
      return 1;
    case FutexThread::WaitResult::TimedOut:
      return 2;
    case FutexThread::WaitResult::Error:
      return -1;
    default:
      MOZ_CRASH();
  }
}

/* static */ int32_t Instance::wait_i32_m32(Instance* instance,
                                            uint32_t byteOffset, int32_t value,
                                            int64_t timeout_ns) {
  MOZ_ASSERT(SASigWaitI32M32.failureMode == FailureMode::FailOnNegI32);
  return PerformWait(instance, byteOffset, value, timeout_ns);
}

/* static */ int32_t Instance::wait_i32_m64(Instance* instance,
                                            uint64_t byteOffset, int32_t value,
                                            int64_t timeout_ns) {
  MOZ_ASSERT(SASigWaitI32M64.failureMode == FailureMode::FailOnNegI32);
  return PerformWait(instance, byteOffset, value, timeout_ns);
}

/* static */ int32_t Instance::wait_i64_m32(Instance* instance,
                                            uint32_t byteOffset, int64_t value,
                                            int64_t timeout_ns) {
  MOZ_ASSERT(SASigWaitI64M32.failureMode == FailureMode::FailOnNegI32);
  return PerformWait(instance, byteOffset, value, timeout_ns);
}

/* static */ int32_t Instance::wait_i64_m64(Instance* instance,
                                            uint64_t byteOffset, int64_t value,
                                            int64_t timeout_ns) {
  MOZ_ASSERT(SASigWaitI64M64.failureMode == FailureMode::FailOnNegI32);
  return PerformWait(instance, byteOffset, value, timeout_ns);
}

template <typename PtrT>
static int32_t PerformWake(Instance* instance, PtrT byteOffset, int32_t count) {
  JSContext* cx = instance->tlsData()->cx;

  // The alignment guard is not in the wasm spec as of 2017-11-02, but is
  // considered likely to appear, as 4-byte alignment is required for WAKE by
  // the spec's validation algorithm.

  if (byteOffset & 3) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_UNALIGNED_ACCESS);
    return -1;
  }

  if (byteOffset >= instance->memory()->volatileMemoryLength()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  if (!instance->memory()->isShared()) {
    return 0;
  }

  MOZ_ASSERT(byteOffset <= SIZE_MAX, "Bounds check is broken");
  int64_t woken = atomics_notify_impl(instance->sharedMemoryBuffer(),
                                      size_t(byteOffset), int64_t(count));

  if (woken > INT32_MAX) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_WAKE_OVERFLOW);
    return -1;
  }

  return int32_t(woken);
}

/* static */ int32_t Instance::wake_m32(Instance* instance, uint32_t byteOffset,
                                        int32_t count) {
  MOZ_ASSERT(SASigWakeM32.failureMode == FailureMode::FailOnNegI32);
  return PerformWake(instance, byteOffset, count);
}

/* static */ int32_t Instance::wake_m64(Instance* instance, uint64_t byteOffset,
                                        int32_t count) {
  MOZ_ASSERT(SASigWakeM32.failureMode == FailureMode::FailOnNegI32);
  return PerformWake(instance, byteOffset, count);
}

//////////////////////////////////////////////////////////////////////////////
//
// Bulk memory operations.

/* static */ uint32_t Instance::memoryGrow_m32(Instance* instance,
                                               uint32_t delta) {
  MOZ_ASSERT(SASigMemoryGrowM32.failureMode == FailureMode::Infallible);
  MOZ_ASSERT(!instance->isAsmJS());

  JSContext* cx = instance->tlsData()->cx;
  RootedWasmMemoryObject memory(cx, instance->memory_);

  // It is safe to cast to uint32_t, as all limits have been checked inside
  // grow() and will not have been exceeded for a 32-bit memory.
  uint32_t ret = uint32_t(WasmMemoryObject::grow(memory, uint64_t(delta), cx));

  // If there has been a moving grow, this Instance should have been notified.
  MOZ_RELEASE_ASSERT(instance->tlsData()->memoryBase ==
                     instance->memory_->buffer().dataPointerEither());

  return ret;
}

/* static */ uint64_t Instance::memoryGrow_m64(Instance* instance,
                                               uint64_t delta) {
  MOZ_ASSERT(SASigMemoryGrowM64.failureMode == FailureMode::Infallible);
  MOZ_ASSERT(!instance->isAsmJS());

  JSContext* cx = instance->tlsData()->cx;
  RootedWasmMemoryObject memory(cx, instance->memory_);

  uint64_t ret = WasmMemoryObject::grow(memory, delta, cx);

  // If there has been a moving grow, this Instance should have been notified.
  MOZ_RELEASE_ASSERT(instance->tlsData()->memoryBase ==
                     instance->memory_->buffer().dataPointerEither());

  return ret;
}

/* static */ uint32_t Instance::memorySize_m32(Instance* instance) {
  MOZ_ASSERT(SASigMemorySizeM32.failureMode == FailureMode::Infallible);

  // This invariant must hold when running Wasm code. Assert it here so we can
  // write tests for cross-realm calls.
  DebugOnly<JSContext*> cx = instance->tlsData()->cx;
  MOZ_ASSERT(cx->realm() == instance->realm());

  Pages pages = instance->memory()->volatilePages();
#ifdef JS_64BIT
  // Ensure that the memory size is no more than 4GiB.
  MOZ_ASSERT(pages <= Pages(MaxMemory32LimitField));
#endif
  return uint32_t(pages.value());
}

/* static */ uint64_t Instance::memorySize_m64(Instance* instance) {
  MOZ_ASSERT(SASigMemorySizeM64.failureMode == FailureMode::Infallible);

  // This invariant must hold when running Wasm code. Assert it here so we can
  // write tests for cross-realm calls.
  DebugOnly<JSContext*> cx = instance->tlsData()->cx;
  MOZ_ASSERT(cx->realm() == instance->realm());

  Pages pages = instance->memory()->volatilePages();
#ifdef JS_64BIT
  MOZ_ASSERT(pages <= Pages(MaxMemory64LimitField));
#endif
  return pages.value();
}

static inline bool BoundsCheckCopy(uint32_t dstByteOffset,
                                   uint32_t srcByteOffset, uint32_t len,
                                   size_t memLen) {
  uint64_t dstOffsetLimit = uint64_t(dstByteOffset) + uint64_t(len);
  uint64_t srcOffsetLimit = uint64_t(srcByteOffset) + uint64_t(len);

  return dstOffsetLimit > memLen || srcOffsetLimit > memLen;
}

static inline bool BoundsCheckCopy(uint64_t dstByteOffset,
                                   uint64_t srcByteOffset, uint64_t len,
                                   size_t memLen) {
  uint64_t dstOffsetLimit = dstByteOffset + len;
  uint64_t srcOffsetLimit = srcByteOffset + len;

  return dstOffsetLimit < dstByteOffset || dstOffsetLimit > memLen ||
         srcOffsetLimit < srcByteOffset || srcOffsetLimit > memLen;
}

template <typename T, typename F, typename I>
inline int32_t WasmMemoryCopy(JSContext* cx, T memBase, size_t memLen,
                              I dstByteOffset, I srcByteOffset, I len,
                              F memMove) {
  if (BoundsCheckCopy(dstByteOffset, srcByteOffset, len, memLen)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  memMove(memBase + uintptr_t(dstByteOffset),
          memBase + uintptr_t(srcByteOffset), size_t(len));
  return 0;
}

template <typename I>
inline int32_t MemoryCopy(JSContext* cx, I dstByteOffset, I srcByteOffset,
                          I len, uint8_t* memBase) {
  const WasmArrayRawBuffer* rawBuf = WasmArrayRawBuffer::fromDataPtr(memBase);
  size_t memLen = rawBuf->byteLength();
  return WasmMemoryCopy(cx, memBase, memLen, dstByteOffset, srcByteOffset, len,
                        memmove);
}

template <typename I>
inline int32_t MemoryCopyShared(JSContext* cx, I dstByteOffset, I srcByteOffset,
                                I len, uint8_t* memBase) {
  using RacyMemMove =
      void (*)(SharedMem<uint8_t*>, SharedMem<uint8_t*>, size_t);

  const SharedArrayRawBuffer* rawBuf =
      SharedArrayRawBuffer::fromDataPtr(memBase);
  size_t memLen = rawBuf->volatileByteLength();

  return WasmMemoryCopy<SharedMem<uint8_t*>, RacyMemMove>(
      cx, SharedMem<uint8_t*>::shared(memBase), memLen, dstByteOffset,
      srcByteOffset, len, AtomicOperations::memmoveSafeWhenRacy);
}

/* static */ int32_t Instance::memCopy_m32(Instance* instance,
                                           uint32_t dstByteOffset,
                                           uint32_t srcByteOffset, uint32_t len,
                                           uint8_t* memBase) {
  MOZ_ASSERT(SASigMemCopyM32.failureMode == FailureMode::FailOnNegI32);
  JSContext* cx = instance->tlsData()->cx;
  return MemoryCopy(cx, dstByteOffset, srcByteOffset, len, memBase);
}

/* static */ int32_t Instance::memCopyShared_m32(Instance* instance,
                                                 uint32_t dstByteOffset,
                                                 uint32_t srcByteOffset,
                                                 uint32_t len,
                                                 uint8_t* memBase) {
  MOZ_ASSERT(SASigMemCopySharedM32.failureMode == FailureMode::FailOnNegI32);
  JSContext* cx = instance->tlsData()->cx;
  return MemoryCopyShared(cx, dstByteOffset, srcByteOffset, len, memBase);
}

/* static */ int32_t Instance::memCopy_m64(Instance* instance,
                                           uint64_t dstByteOffset,
                                           uint64_t srcByteOffset, uint64_t len,
                                           uint8_t* memBase) {
  MOZ_ASSERT(SASigMemCopyM64.failureMode == FailureMode::FailOnNegI32);
  JSContext* cx = instance->tlsData()->cx;
  return MemoryCopy(cx, dstByteOffset, srcByteOffset, len, memBase);
}

/* static */ int32_t Instance::memCopyShared_m64(Instance* instance,
                                                 uint64_t dstByteOffset,
                                                 uint64_t srcByteOffset,
                                                 uint64_t len,
                                                 uint8_t* memBase) {
  MOZ_ASSERT(SASigMemCopySharedM64.failureMode == FailureMode::FailOnNegI32);
  JSContext* cx = instance->tlsData()->cx;
  return MemoryCopyShared(cx, dstByteOffset, srcByteOffset, len, memBase);
}

static inline bool BoundsCheckFill(uint32_t byteOffset, uint32_t len,
                                   size_t memLen) {
  uint64_t offsetLimit = uint64_t(byteOffset) + uint64_t(len);
  return offsetLimit > memLen;
}

static inline bool BoundsCheckFill(uint64_t byteOffset, uint64_t len,
                                   size_t memLen) {
  uint64_t offsetLimit = byteOffset + len;
  return offsetLimit < byteOffset || offsetLimit > memLen;
}

template <typename T, typename F, typename I>
inline int32_t WasmMemoryFill(JSContext* cx, T memBase, size_t memLen,
                              I byteOffset, uint32_t value, I len, F memSet) {
  if (BoundsCheckFill(byteOffset, len, memLen)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  // The required write direction is upward, but that is not currently
  // observable as there are no fences nor any read/write protect operation.
  memSet(memBase + uintptr_t(byteOffset), int(value), size_t(len));
  return 0;
}

template <typename I>
inline int32_t MemoryFill(JSContext* cx, I byteOffset, uint32_t value, I len,
                          uint8_t* memBase) {
  const WasmArrayRawBuffer* rawBuf = WasmArrayRawBuffer::fromDataPtr(memBase);
  size_t memLen = rawBuf->byteLength();
  return WasmMemoryFill(cx, memBase, memLen, byteOffset, value, len, memset);
}

template <typename I>
inline int32_t MemoryFillShared(JSContext* cx, I byteOffset, uint32_t value,
                                I len, uint8_t* memBase) {
  const SharedArrayRawBuffer* rawBuf =
      SharedArrayRawBuffer::fromDataPtr(memBase);
  size_t memLen = rawBuf->volatileByteLength();
  return WasmMemoryFill(cx, SharedMem<uint8_t*>::shared(memBase), memLen,
                        byteOffset, value, len,
                        AtomicOperations::memsetSafeWhenRacy);
}

/* static */ int32_t Instance::memFill_m32(Instance* instance,
                                           uint32_t byteOffset, uint32_t value,
                                           uint32_t len, uint8_t* memBase) {
  MOZ_ASSERT(SASigMemFillM32.failureMode == FailureMode::FailOnNegI32);
  JSContext* cx = instance->tlsData()->cx;
  return MemoryFill(cx, byteOffset, value, len, memBase);
}

/* static */ int32_t Instance::memFillShared_m32(Instance* instance,
                                                 uint32_t byteOffset,
                                                 uint32_t value, uint32_t len,
                                                 uint8_t* memBase) {
  MOZ_ASSERT(SASigMemFillSharedM32.failureMode == FailureMode::FailOnNegI32);
  JSContext* cx = instance->tlsData()->cx;
  return MemoryFillShared(cx, byteOffset, value, len, memBase);
}

/* static */ int32_t Instance::memFill_m64(Instance* instance,
                                           uint64_t byteOffset, uint32_t value,
                                           uint64_t len, uint8_t* memBase) {
  MOZ_ASSERT(SASigMemFillM64.failureMode == FailureMode::FailOnNegI32);
  JSContext* cx = instance->tlsData()->cx;
  return MemoryFill(cx, byteOffset, value, len, memBase);
}

/* static */ int32_t Instance::memFillShared_m64(Instance* instance,
                                                 uint64_t byteOffset,
                                                 uint32_t value, uint64_t len,
                                                 uint8_t* memBase) {
  MOZ_ASSERT(SASigMemFillSharedM64.failureMode == FailureMode::FailOnNegI32);
  JSContext* cx = instance->tlsData()->cx;
  return MemoryFillShared(cx, byteOffset, value, len, memBase);
}

static bool BoundsCheckInit(uint32_t dstOffset, uint32_t srcOffset,
                            uint32_t len, size_t memLen, uint32_t segLen) {
  uint64_t dstOffsetLimit = uint64_t(dstOffset) + uint64_t(len);
  uint64_t srcOffsetLimit = uint64_t(srcOffset) + uint64_t(len);

  return dstOffsetLimit > memLen || srcOffsetLimit > segLen;
}

static bool BoundsCheckInit(uint64_t dstOffset, uint32_t srcOffset,
                            uint32_t len, size_t memLen, uint32_t segLen) {
  uint64_t dstOffsetLimit = dstOffset + uint64_t(len);
  uint64_t srcOffsetLimit = uint64_t(srcOffset) + uint64_t(len);

  return dstOffsetLimit < dstOffset || dstOffsetLimit > memLen ||
         srcOffsetLimit > segLen;
}

template <typename I>
static int32_t MemoryInit(JSContext* cx, Instance* instance, I dstOffset,
                          uint32_t srcOffset, uint32_t len,
                          const DataSegment* maybeSeg) {
  if (!maybeSeg) {
    if (len == 0 && srcOffset == 0) {
      return 0;
    }

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  const DataSegment& seg = *maybeSeg;
  MOZ_RELEASE_ASSERT(!seg.active());

  const uint32_t segLen = seg.bytes.length();

  WasmMemoryObject* mem = instance->memory();
  const size_t memLen = mem->volatileMemoryLength();

  // We are proposing to copy
  //
  //   seg.bytes.begin()[ srcOffset .. srcOffset + len - 1 ]
  // to
  //   memoryBase[ dstOffset .. dstOffset + len - 1 ]

  if (BoundsCheckInit(dstOffset, srcOffset, len, memLen, segLen)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  // The required read/write direction is upward, but that is not currently
  // observable as there are no fences nor any read/write protect operation.
  SharedMem<uint8_t*> dataPtr = mem->buffer().dataPointerEither();
  if (mem->isShared()) {
    AtomicOperations::memcpySafeWhenRacy(
        dataPtr + uintptr_t(dstOffset), (uint8_t*)seg.bytes.begin() + srcOffset,
        len);
  } else {
    uint8_t* rawBuf = dataPtr.unwrap(/*Unshared*/);
    memcpy(rawBuf + uintptr_t(dstOffset),
           (const char*)seg.bytes.begin() + srcOffset, len);
  }
  return 0;
}

/* static */ int32_t Instance::memInit_m32(Instance* instance,
                                           uint32_t dstOffset,
                                           uint32_t srcOffset, uint32_t len,
                                           uint32_t segIndex) {
  MOZ_ASSERT(SASigMemInitM32.failureMode == FailureMode::FailOnNegI32);
  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveDataSegments_.length(),
                     "ensured by validation");

  JSContext* cx = instance->tlsData()->cx;
  return MemoryInit(cx, instance, dstOffset, srcOffset, len,
                    instance->passiveDataSegments_[segIndex]);
}

/* static */ int32_t Instance::memInit_m64(Instance* instance,
                                           uint64_t dstOffset,
                                           uint32_t srcOffset, uint32_t len,
                                           uint32_t segIndex) {
  MOZ_ASSERT(SASigMemInitM64.failureMode == FailureMode::FailOnNegI32);
  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveDataSegments_.length(),
                     "ensured by validation");

  JSContext* cx = instance->tlsData()->cx;
  return MemoryInit(cx, instance, dstOffset, srcOffset, len,
                    instance->passiveDataSegments_[segIndex]);
}

//////////////////////////////////////////////////////////////////////////////
//
// Bulk table operations.

/* static */ int32_t Instance::tableCopy(Instance* instance, uint32_t dstOffset,
                                         uint32_t srcOffset, uint32_t len,
                                         uint32_t dstTableIndex,
                                         uint32_t srcTableIndex) {
  MOZ_ASSERT(SASigTableCopy.failureMode == FailureMode::FailOnNegI32);

  JSContext* cx = instance->tlsData()->cx;
  const SharedTable& srcTable = instance->tables()[srcTableIndex];
  uint32_t srcTableLen = srcTable->length();

  const SharedTable& dstTable = instance->tables()[dstTableIndex];
  uint32_t dstTableLen = dstTable->length();

  // Bounds check and deal with arithmetic overflow.
  uint64_t dstOffsetLimit = uint64_t(dstOffset) + len;
  uint64_t srcOffsetLimit = uint64_t(srcOffset) + len;

  if (dstOffsetLimit > dstTableLen || srcOffsetLimit > srcTableLen) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  bool isOOM = false;

  if (&srcTable == &dstTable && dstOffset > srcOffset) {
    for (uint32_t i = len; i > 0; i--) {
      if (!dstTable->copy(cx, *srcTable, dstOffset + (i - 1),
                          srcOffset + (i - 1))) {
        isOOM = true;
        break;
      }
    }
  } else if (&srcTable == &dstTable && dstOffset == srcOffset) {
    // No-op
  } else {
    for (uint32_t i = 0; i < len; i++) {
      if (!dstTable->copy(cx, *srcTable, dstOffset + i, srcOffset + i)) {
        isOOM = true;
        break;
      }
    }
  }

  if (isOOM) {
    return -1;
  }
  return 0;
}

void* Instance::ensureAndGetIndirectStub(Tier tier, uint32_t funcIndex) {
  const CodeTier& codeTier = code(tier);
  auto stubs = codeTier.lazyStubs().lock();

  void* stub_entry = stubs->lookupIndirectStub(funcIndex, tlsData());
  if (stub_entry) {
    return stub_entry;
  }

  VectorOfIndirectStubTarget targets;
  void* entry = checkedCallEntry(funcIndex, tier);
  if (!targets.append(IndirectStubTarget{funcIndex, entry, tlsData()}) ||
      !stubs->createManyIndirectStubs(targets, codeTier)) {
    return nullptr;
  }

  stub_entry = stubs->lookupIndirectStub(funcIndex, tlsData());
  MOZ_ASSERT(stub_entry);
  return stub_entry;
}

bool Instance::createManyIndirectStubs(
    const VectorOfIndirectStubTarget& targets, const Tier tier) {
  if (targets.empty()) {
    return true;
  }

  const CodeTier& codeTier = code(tier);
  auto stubs = codeTier.lazyStubs().lock();
  return stubs->createManyIndirectStubs(targets, codeTier);
}

void* Instance::getIndirectStub(uint32_t funcIndex, TlsData* targetTlsData,
                                const Tier tier) const {
  MOZ_ASSERT(funcIndex != NullFuncIndex);

  auto stubs = code(tier).lazyStubs().lock();
  return stubs->lookupIndirectStub(funcIndex, targetTlsData);
}

static void RemoveDuplicates(VectorOfIndirectStubTarget* vector) {
  auto comparator = [](const IndirectStubTarget& lhs,
                       const IndirectStubTarget& rhs) {
    if (lhs.functionIdx == rhs.functionIdx) {
      const auto lshTls = reinterpret_cast<uintptr_t>(lhs.tls);
      const auto rshTls = reinterpret_cast<uintptr_t>(rhs.tls);
      return lshTls < rshTls;
    }
    return lhs.functionIdx < rhs.functionIdx;
  };
  std::sort(vector->begin(), vector->end(), comparator);
  auto* newEnd = std::unique(vector->begin(), vector->end());
  vector->erase(newEnd, vector->end());
}

bool Instance::ensureIndirectStubs(JSContext* cx,
                                   const Uint32Vector& elemFuncIndices,
                                   uint32_t srcOffset, uint32_t len,
                                   const Tier tier,
                                   const bool tableIsImportedOrExported) {
  const MetadataTier& metadataTier = metadata(tier);
  VectorOfIndirectStubTarget targets;

  for (uint32_t i = 0; i < len; i++) {
    const uint32_t funcIndex = elemFuncIndices[srcOffset + i];
    if (IsNullFunction(funcIndex)) {
      continue;
    }

    if (IsImportedFunction(funcIndex, metadataTier)) {
      FuncImportTls& import =
          funcImportTls(metadataTier.funcImports[funcIndex]);
      JSFunction* fun = import.fun;
      if (IsJSExportedFunction(fun)) {
        continue;
      }

      Rooted<WasmInstanceObject*> calleeInstanceObj(
          cx, ExportedFunctionToInstanceObject(fun));
      Instance& calleeInstance = calleeInstanceObj->instance();
      TlsData* calleeTls = calleeInstance.tlsData();
      if (getIndirectStub(funcIndex, calleeTls, tier)) {
        continue;
      }

      Tier calleeTier = calleeInstance.code().bestTier();
      const CodeRange& calleeCodeRange =
          calleeInstanceObj->getExportedFunctionCodeRange(fun, calleeTier);
      void* calleeEntry = calleeInstance.codeBase(calleeTier) +
                          calleeCodeRange.funcCheckedCallEntry();
      if (!targets.append(
              IndirectStubTarget{funcIndex, calleeEntry, calleeTls})) {
        return false;
      }
      continue;
    }

    if (!tableIsImportedOrExported ||
        getIndirectStub(funcIndex, tlsData(), tier)) {
      continue;
    }

    if (!targets.append(IndirectStubTarget{
            funcIndex, checkedCallEntry(funcIndex, tier), tlsData()})) {
      return false;
    }
  }

  // This removes duplicates from the list.  In addition, we have already
  // filtered these so that those that have stubs at the tier are not in this
  // list.  So these are unique (funcIndex,tls) pairs that do not yet appear in
  // the list for the tier.

  RemoveDuplicates(&targets);
  return createManyIndirectStubs(targets, tier);
}

bool Instance::ensureIndirectStub(JSContext* cx, FuncRef* ref, const Tier tier,
                                  const bool tableIsImportedOrExported) {
  if (ref->isNull()) {
    return true;
  }

  uint32_t functionIndex = ExportedFunctionToFuncIndex(ref->asJSFunction());
  Uint32Vector functionIndices;
  if (!functionIndices.append(functionIndex)) {
    return false;
  }

  return ensureIndirectStubs(cx, functionIndices, 0, 1u, tier,
                             tableIsImportedOrExported);
}

bool Instance::initElems(JSContext* cx, uint32_t tableIndex,
                         const ElemSegment& seg, uint32_t dstOffset,
                         uint32_t srcOffset, uint32_t len) {
  Table& table = *tables_[tableIndex];
  MOZ_ASSERT(dstOffset <= table.length());
  MOZ_ASSERT(len <= table.length() - dstOffset);

  const Tier tier = code().bestTier();
  const MetadataTier& metadataTier = metadata(tier);
  const FuncImportVector& funcImports = metadataTier.funcImports;
  const Uint32Vector& elemFuncIndices = seg.elemFuncIndices;
  MOZ_ASSERT(srcOffset <= elemFuncIndices.length());
  MOZ_ASSERT(len <= elemFuncIndices.length() - srcOffset);

  if (table.isFunction()) {
    // The purpose of this call is to avoid OOM handling below when we store
    // code pointers in the table.  This ensures that either the table is
    // updated with all pointers, or with none.
    if (!ensureIndirectStubs(cx, elemFuncIndices, srcOffset, len, tier,
                             table.isImportedOrExported())) {
      return false;
    }
  }

  for (uint32_t i = 0; i < len; i++) {
    uint32_t funcIndex = elemFuncIndices[srcOffset + i];
    if (IsNullFunction(funcIndex)) {
      table.setNull(dstOffset + i);
      continue;
    }

    if (!table.isFunction()) {
      // Note, fnref must be rooted if we do anything more than just store it.
      void* fnref = Instance::refFunc(this, funcIndex);
      if (fnref == AnyRef::invalid().forCompiledCode()) {
        return false;  // OOM, which has already been reported.
      }
      table.fillAnyRef(dstOffset + i, 1, AnyRef::fromCompiledCode(fnref));
      continue;
    }

    // Table-of-function and function is not null.  We need to compute the code
    // pointer to install in the table.

    void* code = nullptr;
    if (IsImportedFunction(funcIndex, metadataTier)) {
      FuncImportTls& import = funcImportTls(funcImports[funcIndex]);
      JSFunction* fun = import.fun;
      if (IsJSExportedFunction(fun)) {
        code = checkedCallEntry(funcIndex, tier);
      } else {
        code = getIndirectStub(funcIndex, import.tls, tier);
        MOZ_ASSERT(code);
      }
    } else {
      // The function is an internal wasm function that belongs to the current
      // instance. If table is isImportedOrExported then some other module can
      // import this table and call its functions so we have to use indirect
      // stub, otherwise we can use checked call entry because we don't cross
      // instance's borders.
      if (table.isImportedOrExported()) {
        code = getIndirectStub(funcIndex, tlsData(), tier);
        MOZ_ASSERT(code);
      } else {
        code = checkedCallEntry(funcIndex, tier);
      }
    }

    // Install the code pointer along with the current instance.
    //
    // Using the current instance is correct:
    //
    // - if it's an imported JS function, the current instance is correct,
    //   because that's how we've always done it
    //
    // - if it's an indirect stub for an imported function, the current instance
    //   is correct because it keeps the stub code alive, and the indirect stub
    //   holds the correct remote instance for that call.
    //
    // - otherwise it's a local function, and the current instance is correct in
    //   either case.

    // Utter paranoia to use _RELEASE_ here but we must never store null, all
    // null stores should be handled above.
    MOZ_RELEASE_ASSERT(code);
    table.setFuncRef(dstOffset + i, code, this);
  }

  return true;
}

/* static */ int32_t Instance::tableInit(Instance* instance, uint32_t dstOffset,
                                         uint32_t srcOffset, uint32_t len,
                                         uint32_t segIndex,
                                         uint32_t tableIndex) {
  MOZ_ASSERT(SASigTableInit.failureMode == FailureMode::FailOnNegI32);

  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveElemSegments_.length(),
                     "ensured by validation");

  JSContext* cx = instance->tlsData()->cx;
  if (!instance->passiveElemSegments_[segIndex]) {
    if (len == 0 && srcOffset == 0) {
      return 0;
    }

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  const ElemSegment& seg = *instance->passiveElemSegments_[segIndex];
  MOZ_RELEASE_ASSERT(!seg.active());
  const uint32_t segLen = seg.length();

  const Table& table = *instance->tables()[tableIndex];
  const uint32_t tableLen = table.length();

  // We are proposing to copy
  //
  //   seg[ srcOffset .. srcOffset + len - 1 ]
  // to
  //   tableBase[ dstOffset .. dstOffset + len - 1 ]

  // Bounds check and deal with arithmetic overflow.
  uint64_t dstOffsetLimit = uint64_t(dstOffset) + uint64_t(len);
  uint64_t srcOffsetLimit = uint64_t(srcOffset) + uint64_t(len);

  if (dstOffsetLimit > tableLen || srcOffsetLimit > segLen) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  if (!instance->initElems(cx, tableIndex, seg, dstOffset, srcOffset, len)) {
    return -1;  // OOM, which has already been reported.
  }

  return 0;
}

/* static */ int32_t Instance::tableFill(Instance* instance, uint32_t start,
                                         void* value, uint32_t len,
                                         uint32_t tableIndex) {
  MOZ_ASSERT(SASigTableFill.failureMode == FailureMode::FailOnNegI32);

  JSContext* cx = instance->tlsData()->cx;
  Table& table = *instance->tables()[tableIndex];

  // Bounds check and deal with arithmetic overflow.
  uint64_t offsetLimit = uint64_t(start) + uint64_t(len);

  if (offsetLimit > table.length()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  switch (table.repr()) {
    case TableRepr::Ref:
      table.fillAnyRef(start, len, AnyRef::fromCompiledCode(value));
      break;
    case TableRepr::Func:
      MOZ_RELEASE_ASSERT(!table.isAsmJS());
      if (!table.fillFuncRef(Nothing(), start, len,
                             FuncRef::fromCompiledCode(value), cx)) {
        ReportOutOfMemory(cx);
        return -1;
      }
      break;
  }

  return 0;
}

/* static */ void* Instance::tableGet(Instance* instance, uint32_t index,
                                      uint32_t tableIndex) {
  MOZ_ASSERT(SASigTableGet.failureMode == FailureMode::FailOnInvalidRef);

  JSContext* cx = instance->tlsData()->cx;
  const Table& table = *instance->tables()[tableIndex];

  if (index >= table.length()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_TABLE_OUT_OF_BOUNDS);
    return AnyRef::invalid().forCompiledCode();
  }

  switch (table.repr()) {
    case TableRepr::Ref:
      return table.getAnyRef(index).forCompiledCode();
    case TableRepr::Func: {
      MOZ_RELEASE_ASSERT(!table.isAsmJS());
      RootedFunction fun(cx);
      if (!table.getFuncRef(cx, index, &fun)) {
        return AnyRef::invalid().forCompiledCode();
      }
      return FuncRef::fromJSFunction(fun).forCompiledCode();
    }
  }

  MOZ_CRASH("Should not happen");
}

/* static */ uint32_t Instance::tableGrow(Instance* instance, void* initValue,
                                          uint32_t delta, uint32_t tableIndex) {
  MOZ_ASSERT(SASigTableGrow.failureMode == FailureMode::Infallible);

  JSContext* cx = instance->tlsData()->cx;
  Table& table = *instance->tables()[tableIndex];

  switch (table.repr()) {
    case TableRepr::Ref: {
      uint32_t oldSize = table.grow(delta);
      if (oldSize != uint32_t(-1) && initValue != nullptr) {
        table.fillAnyRef(oldSize, delta, AnyRef::fromCompiledCode(initValue));
      }
      return oldSize;
    }
    case TableRepr::Func: {
      RootedFuncRef functionForFill(cx, FuncRef::fromCompiledCode(initValue));
      const Tier tier = instance->code().bestTier();

      // Call ensureIndirectStub first so as to be able to signal OOM before the
      // table is grown; we don't want a grown table that then can't be
      // initialized because a stub can't be created for the function.
      //
      // Be sure to use the same tier for creating the stub and filling the
      // table, or the later call to fillFuncRef may want to create a new stub
      // for the better tier and may OOM anyway, and it must not.
      if (!instance->ensureIndirectStub(cx, functionForFill.address(), tier,
                                        table.isImportedOrExported())) {
        return -1;
      }

      uint32_t oldSize = table.grow(delta);
      if (oldSize != uint32_t(-1) && initValue != nullptr) {
        MOZ_RELEASE_ASSERT(!table.isAsmJS());
        MOZ_ALWAYS_TRUE(
            table.fillFuncRef(Some(tier), oldSize, delta, functionForFill, cx));
      }
      return oldSize;
    }
  }

  MOZ_CRASH("Should not happen");
}

/* static */ int32_t Instance::tableSet(Instance* instance, uint32_t index,
                                        void* value, uint32_t tableIndex) {
  MOZ_ASSERT(SASigTableSet.failureMode == FailureMode::FailOnNegI32);

  JSContext* cx = instance->tlsData()->cx;
  Table& table = *instance->tables()[tableIndex];

  if (index >= table.length()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_TABLE_OUT_OF_BOUNDS);
    return -1;
  }

  switch (table.repr()) {
    case TableRepr::Ref:
      table.fillAnyRef(index, 1, AnyRef::fromCompiledCode(value));
      break;
    case TableRepr::Func:
      MOZ_RELEASE_ASSERT(!table.isAsmJS());
      if (!table.fillFuncRef(Nothing(), index, 1,
                             FuncRef::fromCompiledCode(value), cx)) {
        ReportOutOfMemory(cx);
        return -1;
      }
      break;
  }

  return 0;
}

/* static */ uint32_t Instance::tableSize(Instance* instance,
                                          uint32_t tableIndex) {
  MOZ_ASSERT(SASigTableSize.failureMode == FailureMode::Infallible);
  Table& table = *instance->tables()[tableIndex];
  return table.length();
}

/* static */ void* Instance::refFunc(Instance* instance, uint32_t funcIndex) {
  MOZ_ASSERT(SASigRefFunc.failureMode == FailureMode::FailOnInvalidRef);
  JSContext* cx = instance->tlsData()->cx;

  Tier tier = instance->code().bestTier();
  const MetadataTier& metadataTier = instance->metadata(tier);
  const FuncImportVector& funcImports = metadataTier.funcImports;

  // If this is an import, we need to recover the original function to maintain
  // reference equality between a re-exported function and 'ref.func'. The
  // identity of the imported function object is stable across tiers, which is
  // what we want.
  //
  // Use the imported function only if it is an exported function, otherwise
  // fall through to get a (possibly new) exported function.
  if (funcIndex < funcImports.length()) {
    FuncImportTls& import = instance->funcImportTls(funcImports[funcIndex]);
    if (IsWasmExportedFunction(import.fun)) {
      return FuncRef::fromJSFunction(import.fun).forCompiledCode();
    }
  }

  RootedFunction fun(cx);
  RootedWasmInstanceObject instanceObj(cx, instance->object());
  if (!WasmInstanceObject::getExportedFunction(cx, instanceObj, funcIndex,
                                               &fun)) {
    // Validation ensures that we always have a valid funcIndex, so we must
    // have OOM'ed
    ReportOutOfMemory(cx);
    return AnyRef::invalid().forCompiledCode();
  }

  return FuncRef::fromJSFunction(fun).forCompiledCode();
}

//////////////////////////////////////////////////////////////////////////////
//
// Segment management.

/* static */ int32_t Instance::elemDrop(Instance* instance, uint32_t segIndex) {
  MOZ_ASSERT(SASigElemDrop.failureMode == FailureMode::FailOnNegI32);

  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveElemSegments_.length(),
                     "ensured by validation");

  if (!instance->passiveElemSegments_[segIndex]) {
    return 0;
  }

  SharedElemSegment& segRefPtr = instance->passiveElemSegments_[segIndex];
  MOZ_RELEASE_ASSERT(!segRefPtr->active());

  // Drop this instance's reference to the ElemSegment so it can be released.
  segRefPtr = nullptr;
  return 0;
}

/* static */ int32_t Instance::dataDrop(Instance* instance, uint32_t segIndex) {
  MOZ_ASSERT(SASigDataDrop.failureMode == FailureMode::FailOnNegI32);

  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveDataSegments_.length(),
                     "ensured by validation");

  if (!instance->passiveDataSegments_[segIndex]) {
    return 0;
  }

  SharedDataSegment& segRefPtr = instance->passiveDataSegments_[segIndex];
  MOZ_RELEASE_ASSERT(!segRefPtr->active());

  // Drop this instance's reference to the DataSegment so it can be released.
  segRefPtr = nullptr;
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// Object support.

/* static */ void Instance::preBarrierFiltering(Instance* instance,
                                                gc::Cell** location) {
  MOZ_ASSERT(SASigPreBarrierFiltering.failureMode == FailureMode::Infallible);
  MOZ_ASSERT(location);
  gc::PreWriteBarrier(*reinterpret_cast<JSObject**>(location));
}

/* static */ void Instance::postBarrier(Instance* instance,
                                        gc::Cell** location) {
  MOZ_ASSERT(SASigPostBarrier.failureMode == FailureMode::Infallible);
  MOZ_ASSERT(location);
  JSContext* cx = instance->tlsData()->cx;
  cx->runtime()->gc.storeBuffer().putCell(
      reinterpret_cast<JSObject**>(location));
}

/* static */ void Instance::postBarrierFiltering(Instance* instance,
                                                 gc::Cell** location) {
  MOZ_ASSERT(SASigPostBarrierFiltering.failureMode == FailureMode::Infallible);
  MOZ_ASSERT(location);
  if (*location == nullptr || !gc::IsInsideNursery(*location)) {
    return;
  }
  JSContext* cx = instance->tlsData()->cx;
  cx->runtime()->gc.storeBuffer().putCell(
      reinterpret_cast<JSObject**>(location));
}

//////////////////////////////////////////////////////////////////////////////
//
// GC and exception handling support.

// The typeIndex is an index into the rttValues_ table in the instance.
// That table holds RttValue objects.
//
// When we fail to allocate we return a nullptr; the wasm side must check this
// and propagate it as an error.

/* static */ void* Instance::structNew(Instance* instance, void* structDescr) {
  MOZ_ASSERT(SASigStructNew.failureMode == FailureMode::FailOnNullPtr);
  JSContext* cx = instance->tlsData()->cx;
  Rooted<RttValue*> rttValue(cx, (RttValue*)structDescr);
  MOZ_ASSERT(rttValue);
  return TypedObject::createStruct(cx, rttValue);
}

/* static */ void* Instance::arrayNew(Instance* instance, uint32_t length,
                                      void* arrayDescr) {
  MOZ_ASSERT(SASigArrayNew.failureMode == FailureMode::FailOnNullPtr);
  JSContext* cx = instance->tlsData()->cx;
  Rooted<RttValue*> rttValue(cx, (RttValue*)arrayDescr);
  MOZ_ASSERT(rttValue);
  return TypedObject::createArray(cx, rttValue, length);
}

#ifdef ENABLE_WASM_EXCEPTIONS
/* static */ void* Instance::exceptionNew(Instance* instance, uint32_t exnIndex,
                                          uint32_t nbytes) {
  MOZ_ASSERT(SASigExceptionNew.failureMode == FailureMode::FailOnNullPtr);

  JSContext* cx = instance->tlsData()->cx;

  SharedExceptionTag tag = instance->exceptionTags()[exnIndex];
  RootedArrayBufferObject buf(cx, ArrayBufferObject::createZeroed(cx, nbytes));

  if (!buf) {
    return nullptr;
  }

  RootedArrayObject refs(cx, NewDenseEmptyArray(cx));

  if (!refs) {
    return nullptr;
  }

  return AnyRef::fromJSObject(WasmExceptionObject::create(cx, tag, buf, refs))
      .forCompiledCode();
}

/* static */ void* Instance::throwException(Instance* instance, JSObject* exn) {
  MOZ_ASSERT(SASigThrowException.failureMode == FailureMode::FailOnNullPtr);

  JSContext* cx = instance->tlsData()->cx;
  RootedValue exnVal(cx, UnboxAnyRef(AnyRef::fromJSObject(exn)));
  cx->setPendingException(exnVal, nullptr);

  // By always returning a nullptr, we trigger a wasmTrap(Trap::ThrowReported),
  // and use that to trigger the stack walking for this exception.
  return nullptr;
}

/* static */ uint32_t Instance::consumePendingException(Instance* instance) {
  MOZ_ASSERT(SASigConsumePendingException.failureMode ==
             FailureMode::Infallible);

  JSContext* cx = instance->tlsData()->cx;
  RootedObject exn(cx, instance->tlsData()->pendingException);
  instance->tlsData()->pendingException = nullptr;

  if (exn->is<WasmExceptionObject>()) {
    ExceptionTag& exnTag = exn->as<WasmExceptionObject>().tag();
    for (size_t i = 0; i < instance->exceptionTags().length(); i++) {
      ExceptionTag& tag = *instance->exceptionTags()[i];
      if (&tag == &exnTag) {
        return i;
      }
    }
  }

  // Signal an unknown exception tag, e.g., for a non-imported exception or
  // JS value.
  return UINT32_MAX;
}

/* static */ int32_t Instance::pushRefIntoExn(Instance* instance, JSObject* exn,
                                              JSObject* ref) {
  MOZ_ASSERT(SASigPushRefIntoExn.failureMode == FailureMode::FailOnNegI32);

  JSContext* cx = instance->tlsData()->cx;

  MOZ_ASSERT(exn->is<WasmExceptionObject>());
  RootedWasmExceptionObject exnObj(cx, &exn->as<WasmExceptionObject>());

  // TODO/AnyRef-boxing: With boxed immediates and strings, this may need to
  // handle other kinds of values.
  ASSERT_ANYREF_IS_JSOBJECT;

  RootedValue refVal(cx, ObjectOrNullValue(ref));
  RootedArrayObject arr(cx, &exnObj->refs());

  if (!NewbornArrayPush(cx, arr, refVal)) {
    return -1;
  }

  return 0;
}
#endif

/* static */ int32_t Instance::refTest(Instance* instance, void* refPtr,
                                       void* rttPtr) {
  MOZ_ASSERT(SASigRefTest.failureMode == FailureMode::Infallible);

  if (!refPtr) {
    return 0;
  }

  JSContext* cx = instance->tlsData()->cx;

  ASSERT_ANYREF_IS_JSOBJECT;
  RootedTypedObject ref(
      cx, (TypedObject*)AnyRef::fromCompiledCode(refPtr).asJSObject());
  RootedRttValue rtt(
      cx, &AnyRef::fromCompiledCode(rttPtr).asJSObject()->as<RttValue>());
  return int32_t(ref->isRuntimeSubtype(rtt));
}

/* static */ void* Instance::rttSub(Instance* instance, void* rttParentPtr,
                                    void* rttSubCanonPtr) {
  MOZ_ASSERT(SASigRttSub.failureMode == FailureMode::FailOnNullPtr);
  JSContext* cx = instance->tlsData()->cx;

  ASSERT_ANYREF_IS_JSOBJECT;
  RootedRttValue parentRtt(
      cx, &AnyRef::fromCompiledCode(rttParentPtr).asJSObject()->as<RttValue>());
  RootedRttValue subCanonRtt(
      cx,
      &AnyRef::fromCompiledCode(rttSubCanonPtr).asJSObject()->as<RttValue>());

  RootedRttValue subRtt(cx, RttValue::rttSub(cx, parentRtt, subCanonRtt));
  return AnyRef::fromJSObject(subRtt.get()).forCompiledCode();
}

/* static */ int32_t Instance::intrI8VecMul(Instance* instance, uint32_t dest,
                                            uint32_t src1, uint32_t src2,
                                            uint32_t len, uint8_t* memBase) {
  MOZ_ASSERT(SASigIntrI8VecMul.failureMode == FailureMode::FailOnNegI32);

  JSContext* cx = instance->tlsData()->cx;
  const WasmArrayRawBuffer* rawBuf = WasmArrayRawBuffer::fromDataPtr(memBase);
  size_t memLen = rawBuf->byteLength();

  // Bounds check and deal with arithmetic overflow.
  uint64_t destLimit = uint64_t(dest) + uint64_t(len);
  uint64_t src1Limit = uint64_t(src1) + uint64_t(len);
  uint64_t src2Limit = uint64_t(src2) + uint64_t(len);
  if (destLimit > memLen || src1Limit > memLen || src2Limit > memLen) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  // Basic dot product
  uint8_t* destPtr = &memBase[dest];
  uint8_t* src1Ptr = &memBase[src1];
  uint8_t* src2Ptr = &memBase[src2];
  while (len > 0) {
    *destPtr = (*src1Ptr) * (*src2Ptr);

    destPtr++;
    src1Ptr++;
    src2Ptr++;
    len--;
  }

  return 0;
}

// Note, dst must point into nonmoveable storage that is not in the nursery,
// this matters for the write barriers.  Furthermore, for pointer types the
// current value of *dst must be null so that only a post-barrier is required.
//
// Regarding the destination not being in the nursery, we have these cases.
// Either the written location is in the global data section in the
// WasmInstanceObject, or the Cell of a WasmGlobalObject:
//
// - WasmInstanceObjects are always tenured and u.ref_ may point to a
//   nursery object, so we need a post-barrier since the global data of an
//   instance is effectively a field of the WasmInstanceObject.
//
// - WasmGlobalObjects are always tenured, and they have a Cell field, so a
//   post-barrier may be needed for the same reason as above.

void CopyValPostBarriered(uint8_t* dst, const Val& src) {
  switch (src.type().kind()) {
    case ValType::I32: {
      int32_t x = src.i32();
      memcpy(dst, &x, sizeof(x));
      break;
    }
    case ValType::I64: {
      int64_t x = src.i64();
      memcpy(dst, &x, sizeof(x));
      break;
    }
    case ValType::F32: {
      float x = src.f32();
      memcpy(dst, &x, sizeof(x));
      break;
    }
    case ValType::F64: {
      double x = src.f64();
      memcpy(dst, &x, sizeof(x));
      break;
    }
    case ValType::V128: {
      V128 x = src.v128();
      memcpy(dst, &x, sizeof(x));
      break;
    }
    case ValType::Rtt:
    case ValType::Ref: {
      // TODO/AnyRef-boxing: With boxed immediates and strings, the write
      // barrier is going to have to be more complicated.
      ASSERT_ANYREF_IS_JSOBJECT;
      MOZ_ASSERT(*(void**)dst == nullptr,
                 "should be null so no need for a pre-barrier");
      AnyRef x = src.ref();
      memcpy(dst, x.asJSObjectAddress(), sizeof(*x.asJSObjectAddress()));
      if (!x.isNull()) {
        JSObject::postWriteBarrier((JSObject**)dst, nullptr, x.asJSObject());
      }
      break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Instance creation and related.

Instance::Instance(JSContext* cx, Handle<WasmInstanceObject*> object,
                   SharedCode code, UniqueTlsData tlsDataIn,
                   HandleWasmMemoryObject memory,
                   SharedExceptionTagVector&& exceptionTags,
                   SharedTableVector&& tables, UniqueDebugState maybeDebug)
    : realm_(cx->realm()),
      object_(object),
      jsJitArgsRectifier_(
          cx->runtime()->jitRuntime()->getArgumentsRectifier().value),
      jsJitExceptionHandler_(
          cx->runtime()->jitRuntime()->getExceptionTail().value),
      preBarrierCode_(
          cx->runtime()->jitRuntime()->preBarrier(MIRType::Object).value),
      code_(std::move(code)),
      tlsData_(std::move(tlsDataIn)),
      memory_(memory),
      exceptionTags_(std::move(exceptionTags)),
      tables_(std::move(tables)),
      maybeDebug_(std::move(maybeDebug))
#ifdef ENABLE_WASM_GC
      ,
      hasGcTypes_(false)
#endif
{
}

bool Instance::init(JSContext* cx, const JSFunctionVector& funcImports,
                    const ValVector& globalImportValues,
                    const WasmGlobalObjectVector& globalObjs,
                    const DataSegmentVector& dataSegments,
                    const ElemSegmentVector& elemSegments) {
  MOZ_ASSERT(!!maybeDebug_ == metadata().debugEnabled);
#ifdef ENABLE_WASM_EXCEPTIONS
  // Currently the only tags are exception tags.
  MOZ_ASSERT(exceptionTags_.length() == metadata().tags.length());
#else
  MOZ_ASSERT(exceptionTags_.length() == 0);
#endif

#ifdef DEBUG
  for (auto t : code_->tiers()) {
    MOZ_ASSERT(funcImports.length() == metadata(t).funcImports.length());
  }
#endif
  MOZ_ASSERT(tables_.length() == metadata().tables.length());

  tlsData()->memoryBase =
      memory_ ? memory_->buffer().dataPointerEither().unwrap() : nullptr;
  size_t limit = memory_ ? memory_->boundsCheckLimit() : 0;
#if !defined(JS_64BIT) || defined(ENABLE_WASM_CRANELIFT)
  // We assume that the limit is a 32-bit quantity
  MOZ_ASSERT(limit <= UINT32_MAX);
#endif
  tlsData()->boundsCheckLimit = limit;
  tlsData()->instance = this;
  tlsData()->realm = realm_;
  tlsData()->cx = cx;
  tlsData()->valueBoxClass = &WasmValueBox::class_;
  tlsData()->resetInterrupt(cx);
  tlsData()->jumpTable = code_->tieringJumpTable();
  tlsData()->addressOfNeedsIncrementalBarrier =
      cx->compartment()->zone()->addressOfNeedsIncrementalBarrier();

  // Initialize function imports in the tls data
  Tier callerTier = code_->bestTier();
  for (size_t i = 0; i < metadata(callerTier).funcImports.length(); i++) {
    JSFunction* f = funcImports[i];
    const FuncImport& fi = metadata(callerTier).funcImports[i];
    FuncImportTls& import = funcImportTls(fi);
    import.fun = f;
    if (!isAsmJS() && IsWasmExportedFunction(f)) {
      WasmInstanceObject* calleeInstanceObj =
          ExportedFunctionToInstanceObject(f);
      Instance& calleeInstance = calleeInstanceObj->instance();
      Tier calleeTier = calleeInstance.code().bestTier();
      const CodeRange& codeRange =
          calleeInstanceObj->getExportedFunctionCodeRange(f, calleeTier);
      import.tls = calleeInstance.tlsData();
      import.realm = f->realm();
      import.code = calleeInstance.codeBase(calleeTier) +
                    codeRange.funcUncheckedCallEntry();
    } else if (void* thunk = MaybeGetBuiltinThunk(f, fi.funcType())) {
      import.tls = tlsData();
      import.realm = f->realm();
      import.code = thunk;
    } else {
      import.tls = tlsData();
      import.realm = f->realm();
      import.code = codeBase(callerTier) + fi.interpExitCodeOffset();
    }
  }

  // Initialize tables in the tls data
  for (size_t i = 0; i < tables_.length(); i++) {
    const TableDesc& td = metadata().tables[i];
    TableTls& table = tableTls(td);
    table.length = tables_[i]->length();
    table.functionBase = tables_[i]->functionBase();
  }

  // Add observer if our memory base may grow
  if (memory_ && memory_->movingGrowable() &&
      !memory_->addMovingGrowObserver(cx, object_)) {
    return false;
  }

  // Add observers if our tables may grow
  for (const SharedTable& table : tables_) {
    if (table->movingGrowable() && !table->addMovingGrowObserver(cx, object_)) {
      return false;
    }
  }

  // Allocate in the global type sets for structural type checks
  if (!metadata().types.empty()) {
#ifdef ENABLE_WASM_GC
    if (GcAvailable(cx)) {
      // Transfer and allocate type objects for the struct types in the module
      MutableTypeContext tycx = js_new<TypeContext>();
      if (!tycx || !tycx->cloneDerived(metadata().types)) {
        return false;
      }

      for (uint32_t typeIndex = 0; typeIndex < metadata().types.length();
           typeIndex++) {
        const TypeDefWithId& typeDef = metadata().types[typeIndex];
        if (!typeDef.isStructType() && !typeDef.isArrayType()) {
          continue;
        }

        Rooted<RttValue*> rttValue(
            cx, RttValue::rttCanon(cx, TypeHandle(tycx, typeIndex)));
        if (!rttValue) {
          return false;
        }
        // We do not need to use a barrier here because RttValue is always
        // tenured
        MOZ_ASSERT(rttValue.get()->isTenured());
        *((GCPtrObject*)addressOfTypeId(typeDef.id)) = rttValue;
        hasGcTypes_ = true;
      }
    }
#endif

    // Handle functions specially (for now) as they're guaranteed to be
    // acyclical and can use simpler hash-consing logic.
    ExclusiveData<FuncTypeIdSet>::Guard lockedFuncTypeIdSet =
        funcTypeIdSet.lock();

    for (const TypeDefWithId& typeDef : metadata().types) {
      switch (typeDef.kind()) {
        case TypeDefKind::Func: {
          const FuncType& funcType = typeDef.funcType();
          const void* funcTypeId;
          if (!lockedFuncTypeIdSet->allocateFuncTypeId(cx, funcType,
                                                       &funcTypeId)) {
            return false;
          }
          *addressOfTypeId(typeDef.id) = funcTypeId;
          break;
        }
        case TypeDefKind::Struct:
        case TypeDefKind::Array:
          continue;
        default:
          MOZ_CRASH();
      }
    }
  }

  // Initialize globals in the tls data.
  //
  // This must be performed after we have initialized runtime types as a global
  // initializer may reference them.
  for (size_t i = 0; i < metadata().globals.length(); i++) {
    const GlobalDesc& global = metadata().globals[i];

    // Constants are baked into the code, never stored in the global area.
    if (global.isConstant()) {
      continue;
    }

    uint8_t* globalAddr = globalData() + global.offset();
    switch (global.kind()) {
      case GlobalKind::Import: {
        size_t imported = global.importIndex();
        if (global.isIndirect()) {
          *(void**)globalAddr =
              (void*)&globalObjs[imported]->val().get().cell();
        } else {
          CopyValPostBarriered(globalAddr, globalImportValues[imported]);
        }
        break;
      }
      case GlobalKind::Variable: {
        RootedVal val(cx);
        const InitExpr& init = global.initExpr();
        RootedWasmInstanceObject instanceObj(cx, object());
        if (!init.evaluate(cx, globalImportValues, instanceObj, &val)) {
          return false;
        }

        if (global.isIndirect()) {
          void* address = (void*)&globalObjs[i]->val().get().cell();
          *(void**)globalAddr = address;
          CopyValPostBarriered((uint8_t*)address, val.get());
        } else {
          CopyValPostBarriered(globalAddr, val.get());
        }
        break;
      }
      case GlobalKind::Constant: {
        MOZ_CRASH("skipped at the top");
      }
    }
  }

  // Take references to the passive data segments
  if (!passiveDataSegments_.resize(dataSegments.length())) {
    return false;
  }
  for (size_t i = 0; i < dataSegments.length(); i++) {
    if (!dataSegments[i]->active()) {
      passiveDataSegments_[i] = dataSegments[i];
    }
  }

  // Take references to the passive element segments
  if (!passiveElemSegments_.resize(elemSegments.length())) {
    return false;
  }
  for (size_t i = 0; i < elemSegments.length(); i++) {
    if (elemSegments[i]->kind != ElemSegment::Kind::Active) {
      passiveElemSegments_[i] = elemSegments[i];
    }
  }

  return true;
}

Instance::~Instance() {
  realm_->wasm.unregisterInstance(*this);

  if (!metadata().types.empty()) {
    ExclusiveData<FuncTypeIdSet>::Guard lockedFuncTypeIdSet =
        funcTypeIdSet.lock();

    for (const TypeDefWithId& typeDef : metadata().types) {
      if (!typeDef.isFuncType()) {
        continue;
      }
      const FuncType& funcType = typeDef.funcType();
      if (const void* funcTypeId = *addressOfTypeId(typeDef.id)) {
        lockedFuncTypeIdSet->deallocateFuncTypeId(funcType, funcTypeId);
      }
    }
  }

  // Any pending exceptions should have been consumed.
#ifdef ENABLE_WASM_EXCEPTIONS
  MOZ_ASSERT(!tlsData()->pendingException);
#endif
}

size_t Instance::memoryMappedSize() const {
  return memory_->buffer().wasmMappedSize();
}

bool Instance::memoryAccessInGuardRegion(const uint8_t* addr,
                                         unsigned numBytes) const {
  MOZ_ASSERT(numBytes > 0);

  if (!metadata().usesMemory()) {
    return false;
  }

  uint8_t* base = memoryBase().unwrap(/* comparison */);
  if (addr < base) {
    return false;
  }

  size_t lastByteOffset = addr - base + (numBytes - 1);
  return lastByteOffset >= memory()->volatileMemoryLength() &&
         lastByteOffset < memoryMappedSize();
}

void Instance::tracePrivate(JSTracer* trc) {
  // This method is only called from WasmInstanceObject so the only reason why
  // TraceEdge is called is so that the pointer can be updated during a moving
  // GC.
  MOZ_ASSERT_IF(trc->isMarkingTracer(), gc::IsMarked(trc->runtime(), object_));
  TraceEdge(trc, &object_, "wasm instance object");

  // OK to just do one tier here; though the tiers have different funcImports
  // tables, they share the tls object.
  for (const FuncImport& fi : metadata(code().stableTier()).funcImports) {
    TraceNullableEdge(trc, &funcImportTls(fi).fun, "wasm import");
  }

  for (const SharedTable& table : tables_) {
    table->trace(trc);
  }

  for (const GlobalDesc& global : code().metadata().globals) {
    // Indirect reference globals get traced by the owning WebAssembly.Global.
    if (!global.type().isRefRepr() || global.isConstant() ||
        global.isIndirect()) {
      continue;
    }
    GCPtrObject* obj = (GCPtrObject*)(globalData() + global.offset());
    TraceNullableEdge(trc, obj, "wasm reference-typed global");
  }

  TraceNullableEdge(trc, &memory_, "wasm buffer");
#ifdef ENABLE_WASM_GC
  if (hasGcTypes_) {
    for (const TypeDefWithId& typeDef : metadata().types) {
      if (!typeDef.isStructType() && !typeDef.isArrayType()) {
        continue;
      }
      TraceNullableEdge(trc, ((GCPtrObject*)addressOfTypeId(typeDef.id)),
                        "wasm rtt value");
    }
  }
#endif

#ifdef ENABLE_WASM_EXCEPTIONS
  TraceNullableEdge(trc, &tlsData()->pendingException,
                    "wasm pending exception value");
#endif

  if (maybeDebug_) {
    maybeDebug_->trace(trc);
  }
}

void Instance::trace(JSTracer* trc) {
  // Technically, instead of having this method, the caller could use
  // Instance::object() to get the owning WasmInstanceObject to mark,
  // but this method is simpler and more efficient. The trace hook of
  // WasmInstanceObject will call Instance::tracePrivate at which point we
  // can mark the rest of the children.
  TraceEdge(trc, &object_, "wasm instance object");
}

uintptr_t Instance::traceFrame(JSTracer* trc, const wasm::WasmFrameIter& wfi,
                               uint8_t* nextPC,
                               uintptr_t highestByteVisitedInPrevFrame) {
  const StackMap* map = code().lookupStackMap(nextPC);
  if (!map) {
    return 0;
  }

  Frame* frame = wfi.frame();

  // |frame| points somewhere in the middle of the area described by |map|.
  // We have to calculate |scanStart|, the lowest address that is described by
  // |map|, by consulting |map->frameOffsetFromTop|.

  const size_t numMappedBytes =
      (map->numMappedWords + frame->numberOfTrampolineSlots()) * sizeof(void*);
  const uintptr_t scanStart = uintptr_t(frame) +
                              (map->frameOffsetFromTop * sizeof(void*)) -
                              numMappedBytes;
  MOZ_ASSERT(0 == scanStart % sizeof(void*));

  // Do what we can to assert that, for consecutive wasm frames, their stack
  // maps also abut exactly.  This is a useful sanity check on the sizing of
  // stackmaps.
  //
  // In debug builds, the stackmap construction machinery goes to considerable
  // efforts to ensure that the stackmaps for consecutive frames abut exactly.
  // This is so as to ensure there are no areas of stack inadvertently ignored
  // by a stackmap, nor covered by two stackmaps.  Hence any failure of this
  // assertion is serious and should be investigated.

  // This condition isn't kept for Cranelift
  // (https://github.com/bytecodealliance/wasmtime/issues/2281), but this is ok
  // to disable this assertion because when CL compiles a function, in the
  // prologue, it (generates code) copies all of the in-memory arguments into
  // registers. So, because of that, none of the in-memory argument words are
  // actually live.
#ifndef JS_CODEGEN_ARM64
  MOZ_ASSERT_IF(highestByteVisitedInPrevFrame != 0,
                highestByteVisitedInPrevFrame + 1 == scanStart);
#endif

  uintptr_t* stackWords = (uintptr_t*)scanStart;
  const uint32_t trampolineBeginIdx =
      map->numMappedWords - static_cast<uint32_t>(map->frameOffsetFromTop) +
      frame->numberOfTrampolineSlots();

  // If we have some exit stub words, this means the map also covers an area
  // created by a exit stub, and so the highest word of that should be a
  // constant created by (code created by) GenerateTrapExit.
  MOZ_ASSERT_IF(
      map->numExitStubWords > 0,
      stackWords[map->numExitStubWords - 1 - TrapExitDummyValueOffsetFromTop] ==
          TrapExitDummyValue);

  // And actually hand them off to the GC.
  uint32_t stackIdx = 0;
  for (uint32_t mapIdx = 0; mapIdx < map->numMappedWords;
       ++mapIdx, ++stackIdx) {
    if (frame->callerIsTrampolineFP() && mapIdx == trampolineBeginIdx) {
      // Skip trampoline frame.
      stackIdx += frame->numberOfTrampolineSlots();
    }

    if (map->getBit(mapIdx) == 0) {
      continue;
    }

    // TODO/AnyRef-boxing: With boxed immediates and strings, the value may
    // not be a traceable JSObject*.
    ASSERT_ANYREF_IS_JSOBJECT;

    // This assertion seems at least moderately effective in detecting
    // discrepancies or misalignments between the map and reality.
    MOZ_ASSERT(
        js::gc::IsCellPointerValidOrNull((const void*)stackWords[stackIdx]));

    if (stackWords[stackIdx]) {
      TraceRoot(trc, (JSObject**)&stackWords[stackIdx],
                "Instance::traceWasmFrame: normal word");
    }
  }

  // Finally, deal with any GC-managed fields in the DebugFrame, if it is
  // present and those fields may be live.
  if (map->hasDebugFrameWithLiveRefs) {
    DebugFrame* debugFrame = DebugFrame::from(frame);
    char* debugFrameP = (char*)debugFrame;

    // TODO/AnyRef-boxing: With boxed immediates and strings, the value may
    // not be a traceable JSObject*.
    ASSERT_ANYREF_IS_JSOBJECT;

    for (size_t i = 0; i < MaxRegisterResults; i++) {
      if (debugFrame->hasSpilledRegisterRefResult(i)) {
        char* resultRefP = debugFrameP + DebugFrame::offsetOfRegisterResult(i);
        TraceNullableRoot(
            trc, (JSObject**)resultRefP,
            "Instance::traceWasmFrame: DebugFrame::resultResults_");
      }
    }

    if (debugFrame->hasCachedReturnJSValue()) {
      char* cachedReturnJSValueP =
          debugFrameP + DebugFrame::offsetOfCachedReturnJSValue();
      TraceRoot(trc, (js::Value*)cachedReturnJSValueP,
                "Instance::traceWasmFrame: DebugFrame::cachedReturnJSValue_");
    }
  }

  return scanStart + numMappedBytes - 1;
}

Instance* Instance::getOriginalInstanceAndFunction(Tier tier, uint32_t funcIdx,
                                                   JSFunction** fun) {
  const MetadataTier& metadataTier = metadata(tier);
  const FuncImportVector& funcImports = metadataTier.funcImports;

  if (IsImportedFunction(funcIdx, metadataTier)) {
    FuncImportTls& import = funcImportTls(funcImports[funcIdx]);
    *fun = import.fun;
    if (IsJSExportedFunction(*fun)) {
      return this;
    }
    WasmInstanceObject* calleeInstanceObj =
        ExportedFunctionToInstanceObject(*fun);
    return &calleeInstanceObj->instance();
  }

  return this;
}

WasmMemoryObject* Instance::memory() const { return memory_; }

SharedMem<uint8_t*> Instance::memoryBase() const {
  MOZ_ASSERT(metadata().usesMemory());
  MOZ_ASSERT(tlsData()->memoryBase == memory_->buffer().dataPointerEither());
  return memory_->buffer().dataPointerEither();
}

SharedArrayRawBuffer* Instance::sharedMemoryBuffer() const {
  MOZ_ASSERT(memory_->isShared());
  return memory_->sharedArrayRawBuffer();
}

WasmInstanceObject* Instance::objectUnbarriered() const {
  return object_.unbarrieredGet();
}

WasmInstanceObject* Instance::object() const { return object_; }

static bool EnsureEntryStubs(const Instance& instance, uint32_t funcIndex,
                             const FuncExport** funcExport,
                             void** interpEntry) {
  Tier tier = instance.code().bestTier();

  size_t funcExportIndex;
  *funcExport =
      &instance.metadata(tier).lookupFuncExport(funcIndex, &funcExportIndex);

  const FuncExport& fe = **funcExport;
  if (fe.hasEagerStubs()) {
    *interpEntry = instance.codeBase(tier) + fe.eagerInterpEntryOffset();
    return true;
  }

  MOZ_ASSERT(!instance.isAsmJS(), "only wasm can lazily export functions");

  // If the best tier is Ion, life is simple: background compilation has
  // already completed and has been committed, so there's no risk of race
  // conditions here.
  //
  // If the best tier is Baseline, there could be a background compilation
  // happening at the same time. The background compilation will lock the
  // first tier lazy stubs first to stop new baseline stubs from being
  // generated, then the second tier stubs to generate them.
  //
  // - either we take the tier1 lazy stub lock before the background
  // compilation gets it, then we generate the lazy stub for tier1. When the
  // background thread gets the tier1 lazy stub lock, it will see it has a
  // lazy stub and will recompile it for tier2.
  // - or we don't take the lock here first. Background compilation won't
  // find a lazy stub for this function, thus won't generate it. So we'll do
  // it ourselves after taking the tier2 lock.
  //
  // Also see doc block for stubs in WasmJS.cpp.

  auto stubs = instance.code(tier).lazyStubs().lock();
  *interpEntry = stubs->lookupInterpEntry(fe.funcIndex());
  if (*interpEntry) {
    return true;
  }

  // The best tier might have changed after we've taken the lock.
  Tier prevTier = tier;
  tier = instance.code().bestTier();
  const CodeTier& codeTier = instance.code(tier);
  if (tier == prevTier) {
    if (!stubs->createOneEntryStub(funcExportIndex, codeTier)) {
      return false;
    }

    *interpEntry = stubs->lookupInterpEntry(fe.funcIndex());
    MOZ_ASSERT(*interpEntry);
    return true;
  }

  MOZ_RELEASE_ASSERT(prevTier == Tier::Baseline && tier == Tier::Optimized);
  auto stubs2 = instance.code(tier).lazyStubs().lock();

  // If it didn't have a stub in the first tier, background compilation
  // shouldn't have made one in the second tier.
  MOZ_ASSERT(!stubs2->hasEntryStub(fe.funcIndex()));

  if (!stubs2->createOneEntryStub(funcExportIndex, codeTier)) {
    return false;
  }

  *interpEntry = stubs2->lookupInterpEntry(fe.funcIndex());
  MOZ_ASSERT(*interpEntry);
  return true;
}

static bool GetInterpEntryAndEnsureStubs(JSContext* cx, Instance& instance,
                                         uint32_t funcIndex, CallArgs args,
                                         void** interpEntry,
                                         const FuncType** funcType) {
  const FuncExport* funcExport;
  if (!EnsureEntryStubs(instance, funcIndex, &funcExport, interpEntry)) {
    return false;
  }

#ifdef DEBUG
  // EnsureEntryStubs() has ensured proper jit-entry stubs have been created and
  // installed in funcIndex's JumpTable entry, so check against the presence of
  // the provisional lazy stub.  See also
  // WasmInstanceObject::getExportedFunction().
  if (!funcExport->hasEagerStubs() && funcExport->canHaveJitEntry()) {
    if (!EnsureBuiltinThunksInitialized()) {
      return false;
    }
    JSFunction& callee = args.callee().as<JSFunction>();
    void* provisionalLazyJitEntryStub = ProvisionalLazyJitEntryStub();
    MOZ_ASSERT(provisionalLazyJitEntryStub);
    MOZ_ASSERT(callee.isWasmWithJitEntry());
    MOZ_ASSERT(*callee.wasmJitEntry() != provisionalLazyJitEntryStub);
  }
#endif

  *funcType = &funcExport->funcType();
  return true;
}

bool wasm::ResultsToJSValue(JSContext* cx, ResultType type,
                            void* registerResultLoc,
                            Maybe<char*> stackResultsLoc,
                            MutableHandleValue rval, CoercionLevel level) {
  if (type.empty()) {
    // No results: set to undefined, and we're done.
    rval.setUndefined();
    return true;
  }

  // If we added support for multiple register results, we'd need to establish a
  // convention for how to store them to memory in registerResultLoc.  For now
  // we can punt.
  static_assert(MaxRegisterResults == 1);

  // Stack results written to stackResultsLoc; register result written
  // to registerResultLoc.

  // First, convert the register return value, and prepare to iterate in
  // push order.  Note that if the register result is a reference type,
  // it may be unrooted, so ToJSValue_anyref must not GC in that case.
  ABIResultIter iter(type);
  DebugOnly<bool> usedRegisterResult = false;
  for (; !iter.done(); iter.next()) {
    if (iter.cur().inRegister()) {
      MOZ_ASSERT(!usedRegisterResult);
      if (!ToJSValue<DebugCodegenVal>(cx, registerResultLoc, iter.cur().type(),
                                      rval, level)) {
        return false;
      }
      usedRegisterResult = true;
    }
  }
  MOZ_ASSERT(usedRegisterResult);

  MOZ_ASSERT((stackResultsLoc.isSome()) == (iter.count() > 1));
  if (!stackResultsLoc) {
    // A single result: we're done.
    return true;
  }

  // Otherwise, collect results in an array, in push order.
  Rooted<ArrayObject*> array(cx, NewDenseEmptyArray(cx));
  if (!array) {
    return false;
  }
  RootedValue tmp(cx);
  for (iter.switchToPrev(); !iter.done(); iter.prev()) {
    const ABIResult& result = iter.cur();
    if (result.onStack()) {
      char* loc = stackResultsLoc.value() + result.stackOffset();
      if (!ToJSValue<DebugCodegenVal>(cx, loc, result.type(), &tmp, level)) {
        return false;
      }
      if (!NewbornArrayPush(cx, array, tmp)) {
        return false;
      }
    } else {
      if (!NewbornArrayPush(cx, array, rval)) {
        return false;
      }
    }
  }
  rval.set(ObjectValue(*array));
  return true;
}

class MOZ_RAII ReturnToJSResultCollector {
  class MOZ_RAII StackResultsRooter : public JS::CustomAutoRooter {
    ReturnToJSResultCollector& collector_;

   public:
    StackResultsRooter(JSContext* cx, ReturnToJSResultCollector& collector)
        : JS::CustomAutoRooter(cx), collector_(collector) {}

    void trace(JSTracer* trc) final {
      for (ABIResultIter iter(collector_.type_); !iter.done(); iter.next()) {
        const ABIResult& result = iter.cur();
        if (result.onStack() && result.type().isRefRepr()) {
          char* loc = collector_.stackResultsArea_.get() + result.stackOffset();
          JSObject** refLoc = reinterpret_cast<JSObject**>(loc);
          TraceNullableRoot(trc, refLoc, "StackResultsRooter::trace");
        }
      }
    }
  };
  friend class StackResultsRooter;

  ResultType type_;
  UniquePtr<char[], JS::FreePolicy> stackResultsArea_;
  Maybe<StackResultsRooter> rooter_;

 public:
  explicit ReturnToJSResultCollector(const ResultType& type) : type_(type){};
  bool init(JSContext* cx) {
    bool needRooter = false;
    ABIResultIter iter(type_);
    for (; !iter.done(); iter.next()) {
      const ABIResult& result = iter.cur();
      if (result.onStack() && result.type().isRefRepr()) {
        needRooter = true;
      }
    }
    uint32_t areaBytes = iter.stackBytesConsumedSoFar();
    MOZ_ASSERT_IF(needRooter, areaBytes > 0);
    if (areaBytes > 0) {
      // It is necessary to zero storage for ref results, and it doesn't
      // hurt to do so for other POD results.
      stackResultsArea_ = cx->make_zeroed_pod_array<char>(areaBytes);
      if (!stackResultsArea_) {
        return false;
      }
      if (needRooter) {
        rooter_.emplace(cx, *this);
      }
    }
    return true;
  }

  void* stackResultsArea() {
    MOZ_ASSERT(stackResultsArea_);
    return stackResultsArea_.get();
  }

  bool collect(JSContext* cx, void* registerResultLoc, MutableHandleValue rval,
               CoercionLevel level) {
    Maybe<char*> stackResultsLoc =
        stackResultsArea_ ? Some(stackResultsArea_.get()) : Nothing();
    return ResultsToJSValue(cx, type_, registerResultLoc, stackResultsLoc, rval,
                            level);
  }
};

bool Instance::callExport(JSContext* cx, uint32_t funcIndex, CallArgs args,
                          CoercionLevel level) {
  if (memory_) {
    // If there has been a moving grow, this Instance should have been notified.
    MOZ_RELEASE_ASSERT(memory_->buffer().dataPointerEither() == memoryBase());
  }

  void* interpEntry;
  const FuncType* funcType;
  if (!GetInterpEntryAndEnsureStubs(cx, *this, funcIndex, args, &interpEntry,
                                    &funcType)) {
    return false;
  }

  // Lossless coercions can handle unexposable arguments or returns. This is
  // only available in testing code.
  if (level != CoercionLevel::Lossless && funcType->hasUnexposableArgOrRet()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_VAL_TYPE);
    return false;
  }

  ArgTypeVector argTypes(*funcType);
  ResultType resultType(ResultType::Vector(funcType->results()));
  ReturnToJSResultCollector results(resultType);
  if (!results.init(cx)) {
    return false;
  }

  // The calling convention for an external call into wasm is to pass an
  // array of 16-byte values where each value contains either a coerced int32
  // (in the low word), or a double value (in the low dword) value, with the
  // coercions specified by the wasm signature. The external entry point
  // unpacks this array into the system-ABI-specified registers and stack
  // memory and then calls into the internal entry point. The return value is
  // stored in the first element of the array (which, therefore, must have
  // length >= 1).
  Vector<ExportArg, 8> exportArgs(cx);
  if (!exportArgs.resize(
          std::max<size_t>(1, argTypes.lengthWithStackResults()))) {
    return false;
  }

  ASSERT_ANYREF_IS_JSOBJECT;
  Rooted<GCVector<JSObject*, 8, SystemAllocPolicy>> refs(cx);

  DebugCodegen(DebugChannel::Function, "wasm-function[%d] arguments [",
               funcIndex);
  RootedValue v(cx);
  for (size_t i = 0; i < argTypes.lengthWithStackResults(); ++i) {
    void* rawArgLoc = &exportArgs[i];
    if (argTypes.isSyntheticStackResultPointerArg(i)) {
      *reinterpret_cast<void**>(rawArgLoc) = results.stackResultsArea();
      continue;
    }
    size_t naturalIdx = argTypes.naturalIndex(i);
    v = naturalIdx < args.length() ? args[naturalIdx] : UndefinedValue();
    ValType type = funcType->arg(naturalIdx);
    if (!ToWebAssemblyValue<DebugCodegenVal>(cx, v, type, rawArgLoc, true,
                                             level)) {
      return false;
    }
    if (type.isRefRepr()) {
      // Ensure we don't have a temporarily unsupported Ref type in callExport
      MOZ_RELEASE_ASSERT(!type.isTypeIndex());
      void* ptr = *reinterpret_cast<void**>(rawArgLoc);
      // Store in rooted array until no more GC is possible.
      RootedAnyRef ref(cx, AnyRef::fromCompiledCode(ptr));
      ASSERT_ANYREF_IS_JSOBJECT;
      if (!refs.emplaceBack(ref.get().asJSObject())) {
        return false;
      }
      DebugCodegen(DebugChannel::Function, "/(#%d)", int(refs.length() - 1));
    }
  }

  // Copy over reference values from the rooted array, if any.
  if (refs.length() > 0) {
    DebugCodegen(DebugChannel::Function, "; ");
    size_t nextRef = 0;
    for (size_t i = 0; i < argTypes.lengthWithStackResults(); ++i) {
      if (argTypes.isSyntheticStackResultPointerArg(i)) {
        continue;
      }
      size_t naturalIdx = argTypes.naturalIndex(i);
      ValType type = funcType->arg(naturalIdx);
      if (type.isRefRepr()) {
        void** rawArgLoc = (void**)&exportArgs[i];
        *rawArgLoc = refs[nextRef++];
        DebugCodegen(DebugChannel::Function, " ref(#%d) := %p ",
                     int(nextRef - 1), *rawArgLoc);
      }
    }
    refs.clear();
  }

  DebugCodegen(DebugChannel::Function, "]\n");

  // Ensure pending exception is cleared before and after (below) call.
#ifdef ENABLE_WASM_EXCEPTIONS
  MOZ_ASSERT(!tlsData()->pendingException);
#endif

  {
    JitActivation activation(cx);

    // Call the per-exported-function trampoline created by GenerateEntry.
    auto funcPtr = JS_DATA_TO_FUNC_PTR(ExportFuncPtr, interpEntry);
    if (!CALL_GENERATED_2(funcPtr, exportArgs.begin(), tlsData())) {
      return false;
    }
  }

#ifdef ENABLE_WASM_EXCEPTIONS
  MOZ_ASSERT(!tlsData()->pendingException);
#endif

  if (isAsmJS() && args.isConstructing()) {
    // By spec, when a JS function is called as a constructor and this
    // function returns a primary type, which is the case for all asm.js
    // exported functions, the returned value is discarded and an empty
    // object is returned instead.
    PlainObject* obj = NewPlainObject(cx);
    if (!obj) {
      return false;
    }
    args.rval().set(ObjectValue(*obj));
    return true;
  }

  // Note that we're not rooting the register result, if any; we depend
  // on ResultsCollector::collect to root the value on our behalf,
  // before causing any GC.
  void* registerResultLoc = &exportArgs[0];
  DebugCodegen(DebugChannel::Function, "wasm-function[%d]; results [",
               funcIndex);
  if (!results.collect(cx, registerResultLoc, args.rval(), level)) {
    return false;
  }
  DebugCodegen(DebugChannel::Function, "]\n");

  return true;
}

bool Instance::constantRefFunc(uint32_t funcIndex,
                               MutableHandleFuncRef result) {
  void* fnref = Instance::refFunc(this, funcIndex);
  if (fnref == AnyRef::invalid().forCompiledCode()) {
    return false;  // OOM, which has already been reported.
  }
  result.set(FuncRef::fromCompiledCode(fnref));
  return true;
}

bool Instance::constantRttCanon(JSContext* cx, uint32_t sourceTypeIndex,
                                MutableHandleRttValue result) {
  // Get the renumbered type index from the source type index
  uint32_t renumberedTypeIndex = metadata().typesRenumbering[sourceTypeIndex];
  // The original type definition cannot have been a function type, so it
  // could not have been an immediate type
  MOZ_ASSERT(renumberedTypeIndex != UINT32_MAX);
  // Get the TypeIdDesc to find the canonical RttValue
  const TypeDefWithId& typeDef = metadata().types[renumberedTypeIndex];
  MOZ_ASSERT(typeDef.isStructType() || typeDef.isArrayType());
  // Cast from untyped storage to RttValue
  result.set(*(RttValue**)addressOfTypeId(typeDef.id));
  return true;
}

bool Instance::constantRttSub(JSContext* cx, HandleRttValue parentRtt,
                              uint32_t sourceChildTypeIndex,
                              MutableHandleRttValue result) {
  RootedRttValue subCanonRtt(cx, nullptr);
  // Get the canonical rtt value from the child type index, this is used to
  // memoize results of rtt.sub
  if (!constantRttCanon(cx, sourceChildTypeIndex, &subCanonRtt)) {
    return false;
  }
  result.set(RttValue::rttSub(cx, parentRtt, subCanonRtt));
  if (!result) {
    ReportOutOfMemory(cx);
    return false;
  }
  return true;
}

JSAtom* Instance::getFuncDisplayAtom(JSContext* cx, uint32_t funcIndex) const {
  // The "display name" of a function is primarily shown in Error.stack which
  // also includes location, so use getFuncNameBeforeLocation.
  UTF8Bytes name;
  if (!metadata().getFuncNameBeforeLocation(funcIndex, &name)) {
    return nullptr;
  }

  return AtomizeUTF8Chars(cx, name.begin(), name.length());
}

void Instance::ensureProfilingLabels(bool profilingEnabled) const {
  return code_->ensureProfilingLabels(profilingEnabled);
}

void Instance::onMovingGrowMemory() {
  MOZ_ASSERT(!isAsmJS());
  MOZ_ASSERT(!memory_->isShared());

  ArrayBufferObject& buffer = memory_->buffer().as<ArrayBufferObject>();
  tlsData()->memoryBase = buffer.dataPointer();
  size_t limit = memory_->boundsCheckLimit();
#if !defined(JS_64BIT) || defined(ENABLE_WASM_CRANELIFT)
  // We assume that the limit is a 32-bit quantity
  MOZ_ASSERT(limit <= UINT32_MAX);
#endif
  tlsData()->boundsCheckLimit = limit;
}

void Instance::onMovingGrowTable(const Table* theTable) {
  MOZ_ASSERT(!isAsmJS());

  // `theTable` has grown and we must update cached data for it.  Importantly,
  // we can have cached those data in more than one location: we'll have
  // cached them once for each time the table was imported into this instance.
  //
  // When an instance is registered as an observer of a table it is only
  // registered once, regardless of how many times the table was imported.
  // Thus when a table is grown, onMovingGrowTable() is only invoked once for
  // the table.
  //
  // Ergo we must go through the entire list of tables in the instance here
  // and check for the table in all the cached-data slots; we can't exit after
  // the first hit.

  for (uint32_t i = 0; i < tables_.length(); i++) {
    if (tables_[i] == theTable) {
      TableTls& table = tableTls(metadata().tables[i]);
      table.length = tables_[i]->length();
      table.functionBase = tables_[i]->functionBase();
    }
  }
}

JSString* Instance::createDisplayURL(JSContext* cx) {
  // In the best case, we simply have a URL, from a streaming compilation of a
  // fetched Response.

  if (metadata().filenameIsURL) {
    return NewStringCopyZ<CanGC>(cx, metadata().filename.get());
  }

  // Otherwise, build wasm module URL from following parts:
  // - "wasm:" as protocol;
  // - URI encoded filename from metadata (if can be encoded), plus ":";
  // - 64-bit hash of the module bytes (as hex dump).

  JSStringBuilder result(cx);
  if (!result.append("wasm:")) {
    return nullptr;
  }

  if (const char* filename = metadata().filename.get()) {
    // EncodeURI returns false due to invalid chars or OOM -- fail only
    // during OOM.
    JSString* filenamePrefix = EncodeURI(cx, filename, strlen(filename));
    if (!filenamePrefix) {
      if (cx->isThrowingOutOfMemory()) {
        return nullptr;
      }

      MOZ_ASSERT(!cx->isThrowingOverRecursed());
      cx->clearPendingException();
      return nullptr;
    }

    if (!result.append(filenamePrefix)) {
      return nullptr;
    }
  }

  if (metadata().debugEnabled) {
    if (!result.append(":")) {
      return nullptr;
    }

    const ModuleHash& hash = metadata().debugHash;
    for (unsigned char byte : hash) {
      char digit1 = byte / 16, digit2 = byte % 16;
      if (!result.append(
              (char)(digit1 < 10 ? digit1 + '0' : digit1 + 'a' - 10))) {
        return nullptr;
      }
      if (!result.append(
              (char)(digit2 < 10 ? digit2 + '0' : digit2 + 'a' - 10))) {
        return nullptr;
      }
    }
  }

  return result.finishString();
}

WasmBreakpointSite* Instance::getOrCreateBreakpointSite(JSContext* cx,
                                                        uint32_t offset) {
  MOZ_ASSERT(debugEnabled());
  return debug().getOrCreateBreakpointSite(cx, this, offset);
}

void Instance::destroyBreakpointSite(JSFreeOp* fop, uint32_t offset) {
  MOZ_ASSERT(debugEnabled());
  return debug().destroyBreakpointSite(fop, this, offset);
}

void Instance::disassembleExport(JSContext* cx, uint32_t funcIndex, Tier tier,
                                 PrintCallback printString) const {
  const MetadataTier& metadataTier = metadata(tier);
  const FuncExport& funcExport = metadataTier.lookupFuncExport(funcIndex);
  const CodeRange& range = metadataTier.codeRange(funcExport);
  const CodeTier& codeTier = code(tier);
  const ModuleSegment& segment = codeTier.segment();

  MOZ_ASSERT(range.begin() < segment.length());
  MOZ_ASSERT(range.end() < segment.length());

  uint8_t* functionCode = segment.base() + range.begin();
  jit::Disassemble(functionCode, range.end() - range.begin(), printString);
}

void Instance::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                             Metadata::SeenSet* seenMetadata,
                             Code::SeenSet* seenCode,
                             Table::SeenSet* seenTables, size_t* code,
                             size_t* data) const {
  *data += mallocSizeOf(this);
  *data += mallocSizeOf(tlsData_.get());
  for (const SharedTable& table : tables_) {
    *data += table->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenTables);
  }

  if (maybeDebug_) {
    maybeDebug_->addSizeOfMisc(mallocSizeOf, seenMetadata, seenCode, code,
                               data);
  }

  code_->addSizeOfMiscIfNotSeen(mallocSizeOf, seenMetadata, seenCode, code,
                                data);
}

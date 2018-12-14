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

#include "jit/AtomicOperations.h"
#include "jit/BaselineJIT.h"
#include "jit/InlinableNatives.h"
#include "jit/JitCommon.h"
#include "jit/JitRealm.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmStubs.h"

#include "gc/StoreBuffer-inl.h"
#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::BitwiseCast;

typedef CheckedInt<uint32_t> CheckedU32;

class FuncTypeIdSet {
  typedef HashMap<const FuncType*, uint32_t, FuncTypeHashPolicy,
                  SystemAllocPolicy>
      Map;
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
    MOZ_ASSERT(!(uintptr_t(*funcTypeId) & FuncTypeIdDesc::ImmediateBit));
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

const void** Instance::addressOfFuncTypeId(
    const FuncTypeIdDesc& funcTypeId) const {
  return (const void**)(globalData() + funcTypeId.globalDataOffset());
}

FuncImportTls& Instance::funcImportTls(const FuncImport& fi) {
  return *(FuncImportTls*)(globalData() + fi.tlsDataOffset());
}

TableTls& Instance::tableTls(const TableDesc& td) const {
  return *(TableTls*)(globalData() + td.globalDataOffset);
}

bool Instance::callImport(JSContext* cx, uint32_t funcImportIndex,
                          unsigned argc, const uint64_t* argv,
                          MutableHandleValue rval) {
  AssertRealmUnchanged aru(cx);

  Tier tier = code().bestTier();

  const FuncImport& fi = metadata(tier).funcImports[funcImportIndex];

  InvokeArgs args(cx);
  if (!args.init(cx, argc)) {
    return false;
  }

  if (fi.funcType().hasI64ArgOrRet()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_I64_TYPE);
    return false;
  }

  MOZ_ASSERT(fi.funcType().args().length() == argc);
  for (size_t i = 0; i < argc; i++) {
    switch (fi.funcType().args()[i].code()) {
      case ValType::I32:
        args[i].set(Int32Value(*(int32_t*)&argv[i]));
        break;
      case ValType::F32:
        args[i].set(JS::CanonicalizedDoubleValue(*(float*)&argv[i]));
        break;
      case ValType::F64:
        args[i].set(JS::CanonicalizedDoubleValue(*(double*)&argv[i]));
        break;
      case ValType::Ref:
      case ValType::AnyRef: {
        args[i].set(ObjectOrNullValue(*(JSObject**)&argv[i]));
        break;
      }
      case ValType::I64:
        MOZ_CRASH("unhandled type in callImport");
      case ValType::NullRef:
        MOZ_CRASH("NullRef not expressible");
    }
  }

  FuncImportTls& import = funcImportTls(fi);
  RootedFunction importFun(cx, import.fun);
  MOZ_ASSERT(cx->realm() == importFun->realm());

  RootedValue fval(cx, ObjectValue(*importFun));
  RootedValue thisv(cx, UndefinedValue());
  if (!Call(cx, fval, thisv, args, rval)) {
    return false;
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
  if (!importFun->hasScript()) {
    return true;
  }

  JSScript* script = importFun->nonLazyScript();
  if (!script->hasBaselineScript()) {
    MOZ_ASSERT(!script->hasIonScript());
    return true;
  }

  // Don't enable jit entry when we have a pending ion builder.
  // Take the interpreter path which will link it and enable
  // the fast path on the next call.
  if (script->baselineScript()->hasPendingIonBuilder()) {
    return true;
  }

  // Ensure the argument types are included in the argument TypeSets stored in
  // the TypeScript. This is necessary for Ion, because the import will use
  // the skip-arg-checks entry point.
  //
  // Note that the TypeScript is never discarded while the script has a
  // BaselineScript, so if those checks hold now they must hold at least until
  // the BaselineScript is discarded and when that happens the import is
  // patched back.
  if (!TypeScript::ThisTypes(script)->hasType(TypeSet::UndefinedType())) {
    return true;
  }

  // Functions with anyref in signature don't have a jit exit at the moment.
  if (fi.funcType().temporarilyUnsupportedAnyRef()) {
    return true;
  }

  const ValTypeVector& importArgs = fi.funcType().args();

  size_t numKnownArgs = Min(importArgs.length(), importFun->nargs());
  for (uint32_t i = 0; i < numKnownArgs; i++) {
    TypeSet::Type type = TypeSet::UnknownType();
    switch (importArgs[i].code()) {
      case ValType::I32:
        type = TypeSet::Int32Type();
        break;
      case ValType::F32:
        type = TypeSet::DoubleType();
        break;
      case ValType::F64:
        type = TypeSet::DoubleType();
        break;
      case ValType::Ref:
        MOZ_CRASH("case guarded above");
      case ValType::AnyRef:
        MOZ_CRASH("case guarded above");
      case ValType::I64:
        MOZ_CRASH("NYI");
      case ValType::NullRef:
        MOZ_CRASH("NullRef not expressible");
    }
    if (!TypeScript::ArgTypes(script, i)->hasType(type)) {
      return true;
    }
  }

  // These arguments will be filled with undefined at runtime by the
  // arguments rectifier: check that the imported function can handle
  // undefined there.
  for (uint32_t i = importArgs.length(); i < importFun->nargs(); i++) {
    if (!TypeScript::ArgTypes(script, i)->hasType(TypeSet::UndefinedType())) {
      return true;
    }
  }

  // Let's optimize it!
  if (!script->baselineScript()->addDependentWasmImport(cx, *this,
                                                        funcImportIndex)) {
    return false;
  }

  import.code = jitExitCode;
  import.baselineScript = script->baselineScript();
  return true;
}

/* static */ int32_t /* 0 to signal trap; 1 to signal OK */
Instance::callImport_void(Instance* instance, int32_t funcImportIndex,
                          int32_t argc, uint64_t* argv) {
  JSContext* cx = TlsContext.get();
  RootedValue rval(cx);
  return instance->callImport(cx, funcImportIndex, argc, argv, &rval);
}

/* static */ int32_t /* 0 to signal trap; 1 to signal OK */
Instance::callImport_i32(Instance* instance, int32_t funcImportIndex,
                         int32_t argc, uint64_t* argv) {
  JSContext* cx = TlsContext.get();
  RootedValue rval(cx);
  if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval)) {
    return false;
  }

  return ToInt32(cx, rval, (int32_t*)argv);
}

/* static */ int32_t /* 0 to signal trap; 1 to signal OK */
Instance::callImport_i64(Instance* instance, int32_t funcImportIndex,
                         int32_t argc, uint64_t* argv) {
  JSContext* cx = TlsContext.get();
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_I64_TYPE);
  return false;
}

/* static */ int32_t /* 0 to signal trap; 1 to signal OK */
Instance::callImport_f64(Instance* instance, int32_t funcImportIndex,
                         int32_t argc, uint64_t* argv) {
  JSContext* cx = TlsContext.get();
  RootedValue rval(cx);
  if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval)) {
    return false;
  }

  return ToNumber(cx, rval, (double*)argv);
}

static bool ToRef(JSContext* cx, HandleValue val, void* addr) {
  if (val.isNull()) {
    *(JSObject**)addr = nullptr;
    return true;
  }

  JSObject* obj = ToObject(cx, val);
  if (!obj) {
    return false;
  }
  *(JSObject**)addr = obj;
  return true;
}

/* static */ int32_t /* 0 to signal trap; 1 to signal OK */
Instance::callImport_ref(Instance* instance, int32_t funcImportIndex,
                         int32_t argc, uint64_t* argv) {
  JSContext* cx = TlsContext.get();
  RootedValue rval(cx);
  if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval)) {
    return false;
  }
  return ToRef(cx, rval, argv);
}

/* static */ uint32_t /* infallible */
Instance::growMemory_i32(Instance* instance, uint32_t delta) {
  MOZ_ASSERT(!instance->isAsmJS());

  JSContext* cx = TlsContext.get();
  RootedWasmMemoryObject memory(cx, instance->memory_);

  uint32_t ret = WasmMemoryObject::grow(memory, delta, cx);

  // If there has been a moving grow, this Instance should have been notified.
  MOZ_RELEASE_ASSERT(instance->tlsData()->memoryBase ==
                     instance->memory_->buffer().dataPointerEither());

  return ret;
}

/* static */ uint32_t /* infallible */
Instance::currentMemory_i32(Instance* instance) {
  // This invariant must hold when running Wasm code. Assert it here so we can
  // write tests for cross-realm calls.
  MOZ_ASSERT(TlsContext.get()->realm() == instance->realm());

  uint32_t byteLength = instance->memory()->volatileMemoryLength();
  MOZ_ASSERT(byteLength % wasm::PageSize == 0);
  return byteLength / wasm::PageSize;
}

template <typename T>
static int32_t PerformWait(Instance* instance, uint32_t byteOffset, T value,
                           int64_t timeout_ns) {
  JSContext* cx = TlsContext.get();

  if (byteOffset & (sizeof(T) - 1)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_UNALIGNED_ACCESS);
    return -1;
  }

  if (byteOffset + sizeof(T) > instance->memory()->volatileMemoryLength()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
  }

  mozilla::Maybe<mozilla::TimeDuration> timeout;
  if (timeout_ns >= 0) {
    timeout = mozilla::Some(
        mozilla::TimeDuration::FromMicroseconds(timeout_ns / 1000));
  }

  switch (atomics_wait_impl(cx, instance->sharedMemoryBuffer(), byteOffset,
                            value, timeout)) {
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

/* static */ int32_t /* -1 to signal trap; nonnegative result for ok */
Instance::wait_i32(Instance* instance, uint32_t byteOffset, int32_t value,
                   int64_t timeout_ns) {
  return PerformWait<int32_t>(instance, byteOffset, value, timeout_ns);
}

/* static */ int32_t /* -1 to signal trap; nonnegative result for ok */
Instance::wait_i64(Instance* instance, uint32_t byteOffset, int64_t value,
                   int64_t timeout_ns) {
  return PerformWait<int64_t>(instance, byteOffset, value, timeout_ns);
}

/* static */ int32_t /* -1 to signal trap; nonnegative for ok */
Instance::wake(Instance* instance, uint32_t byteOffset, int32_t count) {
  JSContext* cx = TlsContext.get();

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

  int64_t woken = atomics_notify_impl(instance->sharedMemoryBuffer(),
                                      byteOffset, int64_t(count));

  if (woken > INT32_MAX) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_WAKE_OVERFLOW);
    return -1;
  }

  return int32_t(woken);
}

/* static */ int32_t /* -1 to signal trap; 0 for ok */
Instance::memCopy(Instance* instance, uint32_t dstByteOffset,
                  uint32_t srcByteOffset, uint32_t len) {
  WasmMemoryObject* mem = instance->memory();
  uint32_t memLen = mem->volatileMemoryLength();

  if (len == 0) {
    // Even though the length is zero, we must check for a valid offset.
    if (dstByteOffset < memLen && srcByteOffset < memLen) {
      return 0;
    }
  } else {
    // Here, we know that |len - 1| cannot underflow.
    CheckedU32 lenMinus1 = CheckedU32(len - 1);
    CheckedU32 highestDstOffset = CheckedU32(dstByteOffset) + lenMinus1;
    CheckedU32 highestSrcOffset = CheckedU32(srcByteOffset) + lenMinus1;
    if (highestDstOffset.isValid() && highestSrcOffset.isValid() &&
        highestDstOffset.value() < memLen &&
        highestSrcOffset.value() < memLen) {
      ArrayBufferObjectMaybeShared& arrBuf = mem->buffer();
      uint8_t* rawBuf = arrBuf.dataPointerEither().unwrap();
      memmove(rawBuf + dstByteOffset, rawBuf + srcByteOffset, size_t(len));
      return 0;
    }
  }

  JSContext* cx = TlsContext.get();
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_WASM_OUT_OF_BOUNDS);
  return -1;
}

/* static */ int32_t /* -1 to signal trap; 0 for ok */
Instance::memDrop(Instance* instance, uint32_t segIndex) {
  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveDataSegments_.length(),
                     "ensured by validation");

  if (!instance->passiveDataSegments_[segIndex]) {
    JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                              JSMSG_WASM_INVALID_PASSIVE_DATA_SEG);
    return -1;
  }

  SharedDataSegment& segRefPtr = instance->passiveDataSegments_[segIndex];
  MOZ_RELEASE_ASSERT(!segRefPtr->active());

  // Drop this instance's reference to the DataSegment so it can be released.
  segRefPtr = nullptr;
  return 0;
}

/* static */ int32_t /* -1 to signal trap; 0 for ok */
Instance::memFill(Instance* instance, uint32_t byteOffset, uint32_t value,
                  uint32_t len) {
  WasmMemoryObject* mem = instance->memory();
  uint32_t memLen = mem->volatileMemoryLength();

  if (len == 0) {
    // Even though the length is zero, we must check for a valid offset.
    if (byteOffset < memLen) {
      return 0;
    }
  } else {
    // Here, we know that |len - 1| cannot underflow.
    CheckedU32 highestOffset = CheckedU32(byteOffset) + CheckedU32(len - 1);
    if (highestOffset.isValid() && highestOffset.value() < memLen) {
      ArrayBufferObjectMaybeShared& arrBuf = mem->buffer();
      uint8_t* rawBuf = arrBuf.dataPointerEither().unwrap();
      memset(rawBuf + byteOffset, int(value), size_t(len));
      return 0;
    }
  }

  JSContext* cx = TlsContext.get();
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_WASM_OUT_OF_BOUNDS);
  return -1;
}

/* static */ int32_t /* -1 to signal trap; 0 for ok */
Instance::memInit(Instance* instance, uint32_t dstOffset, uint32_t srcOffset,
                  uint32_t len, uint32_t segIndex) {
  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveDataSegments_.length(),
                     "ensured by validation");

  if (!instance->passiveDataSegments_[segIndex]) {
    JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                              JSMSG_WASM_INVALID_PASSIVE_DATA_SEG);
    return -1;
  }

  const DataSegment& seg = *instance->passiveDataSegments_[segIndex];
  MOZ_RELEASE_ASSERT(!seg.active());

  const uint32_t segLen = seg.bytes.length();

  WasmMemoryObject* mem = instance->memory();
  const uint32_t memLen = mem->volatileMemoryLength();

  // We are proposing to copy
  //
  //   seg.bytes.begin()[ srcOffset .. srcOffset + len - 1 ]
  // to
  //   memoryBase[ dstOffset .. dstOffset + len - 1 ]

  if (len == 0) {
    // Even though the length is zero, we must check for valid offsets.
    if (dstOffset < memLen && srcOffset < segLen) {
      return 0;
    }
  } else {
    // Here, we know that |len - 1| cannot underflow.
    CheckedU32 lenMinus1 = CheckedU32(len - 1);
    CheckedU32 highestDstOffset = CheckedU32(dstOffset) + lenMinus1;
    CheckedU32 highestSrcOffset = CheckedU32(srcOffset) + lenMinus1;
    if (highestDstOffset.isValid() && highestSrcOffset.isValid() &&
        highestDstOffset.value() < memLen &&
        highestSrcOffset.value() < segLen) {
      ArrayBufferObjectMaybeShared& arrBuf = mem->buffer();
      uint8_t* memoryBase = arrBuf.dataPointerEither().unwrap();
      memcpy(memoryBase + dstOffset, (const char*)seg.bytes.begin() + srcOffset,
             len);
      return 0;
    }
  }

  JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                            JSMSG_WASM_OUT_OF_BOUNDS);
  return -1;
}

/* static */ int32_t /* -1 to signal trap; 0 for ok */
Instance::tableCopy(Instance* instance, uint32_t dstOffset, uint32_t srcOffset,
                    uint32_t len, uint32_t dstTableIndex,
                    uint32_t srcTableIndex) {
  const SharedTable& srcTable = instance->tables()[srcTableIndex];
  uint32_t srcTableLen = srcTable->length();

  const SharedTable& dstTable = instance->tables()[dstTableIndex];
  uint32_t dstTableLen = dstTable->length();

  if (len == 0) {
    // Even though the number of items to copy is zero, we must check
    // for valid offsets.
    if (dstOffset < dstTableLen && srcOffset < srcTableLen) {
      return 0;
    }
  } else {
    // Here, we know that |len - 1| cannot underflow.
    CheckedU32 lenMinus1 = CheckedU32(len - 1);
    CheckedU32 highestDstOffset = CheckedU32(dstOffset) + lenMinus1;
    CheckedU32 highestSrcOffset = CheckedU32(srcOffset) + lenMinus1;
    if (highestDstOffset.isValid() && highestSrcOffset.isValid() &&
        highestDstOffset.value() < dstTableLen &&
        highestSrcOffset.value() < srcTableLen) {
      // Actually do the copy, taking care to handle overlapping cases
      // correctly.
      if (&srcTable == &dstTable && dstOffset > srcOffset) {
        for (uint32_t i = len; i > 0; i--) {
          dstTable->copy(*srcTable, dstOffset + (i - 1), srcOffset + (i - 1));
        }
      } else if (&srcTable == &dstTable && dstOffset == srcOffset) {
        // No-op
      } else {
        for (uint32_t i = 0; i < len; i++) {
          dstTable->copy(*srcTable, dstOffset + i, srcOffset + i);
        }
      }

      return 0;
    }
  }

  JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                            JSMSG_WASM_OUT_OF_BOUNDS);
  return -1;
}

/* static */ int32_t /* -1 to signal trap; 0 for ok */
Instance::tableDrop(Instance* instance, uint32_t segIndex) {
  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveElemSegments_.length(),
                     "ensured by validation");

  if (!instance->passiveElemSegments_[segIndex]) {
    JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                              JSMSG_WASM_INVALID_PASSIVE_ELEM_SEG);
    return -1;
  }

  SharedElemSegment& segRefPtr = instance->passiveElemSegments_[segIndex];
  MOZ_RELEASE_ASSERT(!segRefPtr->active());

  // Drop this instance's reference to the ElemSegment so it can be released.
  segRefPtr = nullptr;
  return 0;
}

void Instance::initElems(uint32_t tableIndex, const ElemSegment& seg,
                         uint32_t dstOffset, uint32_t srcOffset, uint32_t len) {
  Table& table = *tables_[tableIndex];
  MOZ_ASSERT(dstOffset <= table.length());
  MOZ_ASSERT(len <= table.length() - dstOffset);

  Tier tier = code().bestTier();
  const MetadataTier& metadataTier = metadata(tier);
  const FuncImportVector& funcImports = metadataTier.funcImports;
  const CodeRangeVector& codeRanges = metadataTier.codeRanges;
  const Uint32Vector& funcToCodeRange = metadataTier.funcToCodeRange;
  const Uint32Vector& elemFuncIndices = seg.elemFuncIndices;
  MOZ_ASSERT(srcOffset <= elemFuncIndices.length());
  MOZ_ASSERT(len <= elemFuncIndices.length() - srcOffset);

  uint8_t* codeBaseTier = codeBase(tier);
  for (uint32_t i = 0; i < len; i++) {
    uint32_t funcIndex = elemFuncIndices[srcOffset + i];
    if (funcIndex < funcImports.length()) {
      FuncImportTls& import = funcImportTls(funcImports[funcIndex]);
      JSFunction* fun = import.fun;
      if (IsExportedWasmFunction(fun)) {
        // This element is a wasm function imported from another
        // instance. To preserve the === function identity required by
        // the JS embedding spec, we must set the element to the
        // imported function's underlying CodeRange.funcTableEntry and
        // Instance so that future Table.get()s produce the same
        // function object as was imported.
        WasmInstanceObject* calleeInstanceObj =
            ExportedFunctionToInstanceObject(fun);
        Instance& calleeInstance = calleeInstanceObj->instance();
        Tier calleeTier = calleeInstance.code().bestTier();
        const CodeRange& calleeCodeRange =
            calleeInstanceObj->getExportedFunctionCodeRange(fun, calleeTier);
        void* code = calleeInstance.codeBase(calleeTier) +
                     calleeCodeRange.funcTableEntry();
        table.setAnyFunc(dstOffset + i, code, &calleeInstance);
        continue;
      }
    }
    void* code =
        codeBaseTier + codeRanges[funcToCodeRange[funcIndex]].funcTableEntry();
    table.setAnyFunc(dstOffset + i, code, this);
  }
}

/* static */ int32_t /* -1 to signal trap; 0 for ok */
Instance::tableInit(Instance* instance, uint32_t dstOffset, uint32_t srcOffset,
                    uint32_t len, uint32_t segIndex, uint32_t tableIndex) {
  MOZ_RELEASE_ASSERT(size_t(segIndex) < instance->passiveElemSegments_.length(),
                     "ensured by validation");

  if (!instance->passiveElemSegments_[segIndex]) {
    JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                              JSMSG_WASM_INVALID_PASSIVE_ELEM_SEG);
    return -1;
  }

  const ElemSegment& seg = *instance->passiveElemSegments_[segIndex];
  MOZ_RELEASE_ASSERT(!seg.active());
  const Table& table = *instance->tables()[tableIndex];

  // Element segments cannot currently contain arbitrary values, and anyref
  // tables cannot be initialized from segments.
  MOZ_ASSERT(table.kind() == TableKind::AnyFunction);

  // We are proposing to copy
  //
  //   seg[ srcOffset .. srcOffset + len - 1 ]
  // to
  //   tableBase[ dstOffset .. dstOffset + len - 1 ]

  if (len == 0) {
    // Even though the length is zero, we must check for valid offsets.
    if (dstOffset < table.length() && srcOffset < seg.length()) {
      return 0;
    }
  } else {
    // Here, we know that |len - 1| cannot underflow.
    CheckedU32 lenMinus1 = CheckedU32(len - 1);
    CheckedU32 highestDstOffset = CheckedU32(dstOffset) + lenMinus1;
    CheckedU32 highestSrcOffset = CheckedU32(srcOffset) + lenMinus1;
    if (highestDstOffset.isValid() && highestSrcOffset.isValid() &&
        highestDstOffset.value() < table.length() &&
        highestSrcOffset.value() < seg.length()) {
      instance->initElems(tableIndex, seg, dstOffset, srcOffset, len);
      return 0;
    }
  }

  JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                            JSMSG_WASM_OUT_OF_BOUNDS);
  return -1;
}

/* static */ void* /* (void*)-1 to signal trap; other pointer value for ok */
Instance::tableGet(Instance* instance, uint32_t index, uint32_t tableIndex) {
  const Table& table = *instance->tables()[tableIndex];
  MOZ_RELEASE_ASSERT(table.kind() == TableKind::AnyRef);
  if (index >= table.length()) {
    JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                              JSMSG_WASM_TABLE_OUT_OF_BOUNDS);
    return (void*)-1;
  }
  return table.getAnyRef(index);
}

/* static */ uint32_t /* infallible */
Instance::tableGrow(Instance* instance, uint32_t delta, void* initValue,
                    uint32_t tableIndex) {
  RootedObject obj(TlsContext.get(), (JSObject*)initValue);
  Table& table = *instance->tables()[tableIndex];
  MOZ_RELEASE_ASSERT(table.kind() == TableKind::AnyRef);

  uint32_t oldSize = table.grow(delta, TlsContext.get());
  if (oldSize != uint32_t(-1) && initValue != nullptr) {
    for (uint32_t i = 0; i < delta; i++) {
      table.setAnyRef(oldSize + i, obj.get());
    }
  }
  return oldSize;
}

/* static */ int32_t /* -1 to signal trap; 0 for ok */
Instance::tableSet(Instance* instance, uint32_t index, void* value,
                   uint32_t tableIndex) {
  Table& table = *instance->tables()[tableIndex];
  MOZ_RELEASE_ASSERT(table.kind() == TableKind::AnyRef);
  if (index >= table.length()) {
    JS_ReportErrorNumberASCII(TlsContext.get(), GetErrorMessage, nullptr,
                              JSMSG_WASM_TABLE_OUT_OF_BOUNDS);
    return -1;
  }
  table.setAnyRef(index, (JSObject*)value);
  return 0;
}

/* static */ uint32_t /* infallible */
Instance::tableSize(Instance* instance, uint32_t tableIndex) {
  Table& table = *instance->tables()[tableIndex];
  return table.length();
}

/* static */ void /* infallible */
Instance::postBarrier(Instance* instance, gc::Cell** location) {
  MOZ_ASSERT(location);
  TlsContext.get()->runtime()->gc.storeBuffer().putCell(location);
}

// The typeIndex is an index into the structTypeDescrs_ table in the instance.
// That table holds TypeDescr objects.
//
// When we fail to allocate we return a nullptr; the wasm side must check this
// and propagate it as an error.

/* static */ void* /* null on OOM, otherwise a pointer */
Instance::structNew(Instance* instance, uint32_t typeIndex) {
  JSContext* cx = TlsContext.get();
  Rooted<TypeDescr*> typeDescr(cx, instance->structTypeDescrs_[typeIndex]);
  return TypedObject::createZeroed(cx, typeDescr);
}

/* static */ void* /* infallible */
Instance::structNarrow(Instance* instance, uint32_t mustUnboxAnyref,
                       uint32_t outputTypeIndex, void* maybeNullPtr) {
  JSContext* cx = TlsContext.get();

  Rooted<TypedObject*> obj(cx);
  Rooted<StructTypeDescr*> typeDescr(cx);

  if (maybeNullPtr == nullptr) {
    return maybeNullPtr;
  }

  void* nonnullPtr = maybeNullPtr;
  if (mustUnboxAnyref) {
    Rooted<NativeObject*> no(cx, static_cast<NativeObject*>(nonnullPtr));
    if (!no->is<TypedObject>()) {
      return nullptr;
    }
    obj = &no->as<TypedObject>();
    Rooted<TypeDescr*> td(cx, &obj->typeDescr());
    if (td->kind() != type::Struct) {
      return nullptr;
    }
    typeDescr = &td->as<StructTypeDescr>();
  } else {
    obj = static_cast<TypedObject*>(nonnullPtr);
    typeDescr = &obj->typeDescr().as<StructTypeDescr>();
  }

  // Optimization opportunity: instead of this loop we could perhaps load an
  // index from `typeDescr` and use that to index into the structTypes table
  // of the instance.  If the index is in bounds and the desc at that index is
  // the desc we have then we know the index is good, and we can use that for
  // the prefix check.

  uint32_t found = UINT32_MAX;
  for (uint32_t i = 0; i < instance->structTypeDescrs_.length(); i++) {
    if (instance->structTypeDescrs_[i] == typeDescr) {
      found = i;
      break;
    }
  }

  if (found == UINT32_MAX) {
    return nullptr;
  }

  // Also asserted in constructor; let's just be double sure.

  MOZ_ASSERT(instance->structTypeDescrs_.length() ==
             instance->structTypes().length());

  // Now we know that the object was created by the instance, and we know its
  // concrete type.  We need to check that its type is an extension of the
  // type of outputTypeIndex.

  if (!instance->structTypes()[found].hasPrefix(
          instance->structTypes()[outputTypeIndex])) {
    return nullptr;
  }

  return nonnullPtr;
}

Instance::Instance(JSContext* cx, Handle<WasmInstanceObject*> object,
                   SharedCode code, UniqueTlsData tlsDataIn,
                   HandleWasmMemoryObject memory, SharedTableVector&& tables,
                   StructTypeDescrVector&& structTypeDescrs,
                   Handle<FunctionVector> funcImports,
                   HandleValVector globalImportValues,
                   const WasmGlobalObjectVector& globalObjs,
                   UniqueDebugState maybeDebug)
    : realm_(cx->realm()),
      object_(object),
      code_(code),
      tlsData_(std::move(tlsDataIn)),
      memory_(memory),
      tables_(std::move(tables)),
      maybeDebug_(std::move(maybeDebug)),
      structTypeDescrs_(std::move(structTypeDescrs)) {
  MOZ_ASSERT(!!maybeDebug_ == metadata().debugEnabled);
  MOZ_ASSERT(structTypeDescrs_.length() == structTypes().length());

#ifdef DEBUG
  for (auto t : code_->tiers()) {
    MOZ_ASSERT(funcImports.length() == metadata(t).funcImports.length());
  }
#endif
  MOZ_ASSERT(tables_.length() == metadata().tables.length());

  tlsData()->memoryBase =
      memory ? memory->buffer().dataPointerEither().unwrap() : nullptr;
  tlsData()->boundsCheckLimit =
      memory ? memory->buffer().wasmBoundsCheckLimit() : 0;
  tlsData()->instance = this;
  tlsData()->realm = realm_;
  tlsData()->cx = cx;
  tlsData()->resetInterrupt(cx);
  tlsData()->jumpTable = code_->tieringJumpTable();
  tlsData()->addressOfNeedsIncrementalBarrier =
      (uint8_t*)cx->compartment()->zone()->addressOfNeedsIncrementalBarrier();

  Tier callerTier = code_->bestTier();
  for (size_t i = 0; i < metadata(callerTier).funcImports.length(); i++) {
    HandleFunction f = funcImports[i];
    const FuncImport& fi = metadata(callerTier).funcImports[i];
    FuncImportTls& import = funcImportTls(fi);
    import.fun = f;
    if (!isAsmJS() && IsExportedWasmFunction(f)) {
      WasmInstanceObject* calleeInstanceObj =
          ExportedFunctionToInstanceObject(f);
      Instance& calleeInstance = calleeInstanceObj->instance();
      Tier calleeTier = calleeInstance.code().bestTier();
      const CodeRange& codeRange =
          calleeInstanceObj->getExportedFunctionCodeRange(f, calleeTier);
      import.tls = calleeInstance.tlsData();
      import.realm = f->realm();
      import.code =
          calleeInstance.codeBase(calleeTier) + codeRange.funcNormalEntry();
      import.baselineScript = nullptr;
    } else if (void* thunk = MaybeGetBuiltinThunk(f, fi.funcType())) {
      import.tls = tlsData();
      import.realm = f->realm();
      import.code = thunk;
      import.baselineScript = nullptr;
    } else {
      import.tls = tlsData();
      import.realm = f->realm();
      import.code = codeBase(callerTier) + fi.interpExitCodeOffset();
      import.baselineScript = nullptr;
    }
  }

  for (size_t i = 0; i < tables_.length(); i++) {
    const TableDesc& td = metadata().tables[i];
    TableTls& table = tableTls(td);
    table.length = tables_[i]->length();
    table.functionBase = tables_[i]->functionBase();
  }

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
          *(void**)globalAddr = globalObjs[imported]->cell();
        } else {
          globalImportValues[imported].get().writePayload(globalAddr);
        }
        break;
      }
      case GlobalKind::Variable: {
        const InitExpr& init = global.initExpr();
        switch (init.kind()) {
          case InitExpr::Kind::Constant: {
            if (global.isIndirect()) {
              *(void**)globalAddr = globalObjs[i]->cell();
            } else {
              Val(init.val()).writePayload(globalAddr);
            }
            break;
          }
          case InitExpr::Kind::GetGlobal: {
            const GlobalDesc& imported = metadata().globals[init.globalIndex()];

            // Global-ref initializers cannot reference mutable globals, so
            // the source global should never be indirect.
            MOZ_ASSERT(!imported.isIndirect());

            RootedVal dest(cx,
                           globalImportValues[imported.importIndex()].get());
            if (global.isIndirect()) {
              void* address = globalObjs[i]->cell();
              *(void**)globalAddr = address;
              dest.get().writePayload((uint8_t*)address);
            } else {
              dest.get().writePayload(globalAddr);
            }
            break;
          }
        }
        break;
      }
      case GlobalKind::Constant: {
        MOZ_CRASH("skipped at the top");
      }
    }
  }
}

bool Instance::init(JSContext* cx, const DataSegmentVector& dataSegments,
                    const ElemSegmentVector& elemSegments) {
  if (memory_ && memory_->movingGrowable() &&
      !memory_->addMovingGrowObserver(cx, object_)) {
    return false;
  }

  for (const SharedTable& table : tables_) {
    if (table->movingGrowable() && !table->addMovingGrowObserver(cx, object_)) {
      return false;
    }
  }

  if (!metadata().funcTypeIds.empty()) {
    ExclusiveData<FuncTypeIdSet>::Guard lockedFuncTypeIdSet =
        funcTypeIdSet.lock();

    for (const FuncTypeWithId& funcType : metadata().funcTypeIds) {
      const void* funcTypeId;
      if (!lockedFuncTypeIdSet->allocateFuncTypeId(cx, funcType, &funcTypeId)) {
        return false;
      }

      *addressOfFuncTypeId(funcType.id) = funcTypeId;
    }
  }

  JitRuntime* jitRuntime = cx->runtime()->getJitRuntime(cx);
  if (!jitRuntime) {
    return false;
  }
  jsJitArgsRectifier_ = jitRuntime->getArgumentsRectifier();
  jsJitExceptionHandler_ = jitRuntime->getExceptionTail();
  preBarrierCode_ = jitRuntime->preBarrier(MIRType::Object);

  if (!passiveDataSegments_.resize(dataSegments.length())) {
    return false;
  }
  for (size_t i = 0; i < dataSegments.length(); i++) {
    if (!dataSegments[i]->active()) {
      passiveDataSegments_[i] = dataSegments[i];
    }
  }

  if (!passiveElemSegments_.resize(elemSegments.length())) {
    return false;
  }
  for (size_t i = 0; i < elemSegments.length(); i++) {
    if (!elemSegments[i]->active()) {
      passiveElemSegments_[i] = elemSegments[i];
    }
  }

  return true;
}

Instance::~Instance() {
  realm_->wasm.unregisterInstance(*this);

  const FuncImportVector& funcImports =
      metadata(code().stableTier()).funcImports;

  for (unsigned i = 0; i < funcImports.length(); i++) {
    FuncImportTls& import = funcImportTls(funcImports[i]);
    if (import.baselineScript) {
      import.baselineScript->removeDependentWasmImport(*this, i);
    }
  }

  if (!metadata().funcTypeIds.empty()) {
    ExclusiveData<FuncTypeIdSet>::Guard lockedFuncTypeIdSet =
        funcTypeIdSet.lock();

    for (const FuncTypeWithId& funcType : metadata().funcTypeIds) {
      if (const void* funcTypeId = *addressOfFuncTypeId(funcType.id)) {
        lockedFuncTypeIdSet->deallocateFuncTypeId(funcType, funcTypeId);
      }
    }
  }
}

size_t Instance::memoryMappedSize() const {
  return memory_->buffer().wasmMappedSize();
}

bool Instance::memoryAccessInGuardRegion(uint8_t* addr,
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
  // GC. TraceWeakEdge may sound better, but it is less efficient given that
  // we know object_ is already marked.
  MOZ_ASSERT(!gc::IsAboutToBeFinalized(&object_));
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
    // Indirect anyref global get traced by the owning WebAssembly.Global.
    if (!global.type().isReference() || global.isConstant() ||
        global.isIndirect()) {
      continue;
    }
    GCPtrObject* obj = (GCPtrObject*)(globalData() + global.offset());
    TraceNullableEdge(trc, obj, "wasm ref/anyref global");
  }

  TraceNullableEdge(trc, &memory_, "wasm buffer");
  structTypeDescrs_.trace(trc);
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

  const size_t numMappedBytes = map->numMappedWords * sizeof(void*);
  const uintptr_t scanStart = uintptr_t(frame) +
                              (map->frameOffsetFromTop * sizeof(void*)) -
                              numMappedBytes;
  MOZ_ASSERT(0 == scanStart % sizeof(void*));

  // Do what we can to assert that, for consecutive wasm frames, their stack
  // maps also abut exactly.  This is a useful sanity check on the sizing of
  // stack maps.
  MOZ_ASSERT_IF(highestByteVisitedInPrevFrame != 0,
                highestByteVisitedInPrevFrame + 1 == scanStart);

  uintptr_t* stackWords = (uintptr_t*)scanStart;

  // If we have some exit stub words, this means the map also covers an area
  // created by a exit stub, and so the highest word of that should be a
  // constant created by (code created by) GenerateTrapExit.
  MOZ_ASSERT_IF(
      map->numExitStubWords > 0,
      stackWords[map->numExitStubWords - 1 - TrapExitDummyValueOffsetFromTop] ==
          TrapExitDummyValue);

  // And actually hand them off to the GC.
  for (uint32_t i = 0; i < map->numMappedWords; i++) {
    if (map->getBit(i) == 0) {
      continue;
    }

    // This assertion seems at least moderately effective in detecting
    // discrepancies or misalignments between the map and reality.
    MOZ_ASSERT(js::gc::IsCellPointerValidOrNull((const void*)stackWords[i]));

    if (stackWords[i]) {
      TraceRoot(trc, (JSObject**)&stackWords[i],
                "Instance::traceWasmFrame: normal word");
    }
  }

  // Finally, deal with a ref-typed DebugFrame if it is present.
  if (map->hasRefTypedDebugFrame) {
    DebugFrame* debugFrame = DebugFrame::from(frame);
    char* debugFrameP = (char*)debugFrame;

    char* resultRefP = debugFrameP + DebugFrame::offsetOfResults();
    if (*(intptr_t*)resultRefP) {
      TraceRoot(trc, (JSObject**)resultRefP,
                "Instance::traceWasmFrame: DebugFrame::resultRef_");
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

bool Instance::callExport(JSContext* cx, uint32_t funcIndex, CallArgs args) {
  // If there has been a moving grow, this Instance should have been notified.
  MOZ_RELEASE_ASSERT(!memory_ || tlsData()->memoryBase ==
                                     memory_->buffer().dataPointerEither());

  Tier tier = code().bestTier();

  const FuncExport& func = metadata(tier).lookupFuncExport(funcIndex);

  if (func.funcType().hasI64ArgOrRet()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_I64_TYPE);
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
  if (!exportArgs.resize(Max<size_t>(1, func.funcType().args().length()))) {
    return false;
  }

  RootedValue v(cx);
  for (unsigned i = 0; i < func.funcType().args().length(); ++i) {
    v = i < args.length() ? args[i] : UndefinedValue();
    switch (func.funcType().arg(i).code()) {
      case ValType::I32:
        if (!ToInt32(cx, v, (int32_t*)&exportArgs[i])) {
          return false;
        }
        break;
      case ValType::I64:
        MOZ_CRASH("unexpected i64 flowing into callExport");
      case ValType::F32:
        if (!RoundFloat32(cx, v, (float*)&exportArgs[i])) {
          return false;
        }
        break;
      case ValType::F64:
        if (!ToNumber(cx, v, (double*)&exportArgs[i])) {
          return false;
        }
        break;
      case ValType::Ref:
      case ValType::AnyRef: {
        if (!ToRef(cx, v, &exportArgs[i])) {
          return false;
        }
        break;
      }
      case ValType::NullRef: {
        MOZ_CRASH("NullRef not expressible");
      }
    }
  }

  {
    JitActivation activation(cx);

    void* callee;
    if (func.hasEagerStubs()) {
      callee = codeBase(tier) + func.eagerInterpEntryOffset();
    } else {
      callee = code(tier).lazyStubs().lock()->lookupInterpEntry(funcIndex);
    }

    // Call the per-exported-function trampoline created by GenerateEntry.
    auto funcPtr = JS_DATA_TO_FUNC_PTR(ExportFuncPtr, callee);
    if (!CALL_GENERATED_2(funcPtr, exportArgs.begin(), tlsData())) {
      return false;
    }
  }

  if (isAsmJS() && args.isConstructing()) {
    // By spec, when a JS function is called as a constructor and this
    // function returns a primary type, which is the case for all asm.js
    // exported functions, the returned value is discarded and an empty
    // object is returned instead.
    PlainObject* obj = NewBuiltinClassInstance<PlainObject>(cx);
    if (!obj) {
      return false;
    }
    args.rval().set(ObjectValue(*obj));
    return true;
  }

  void* retAddr = &exportArgs[0];

  bool expectsObject = false;
  JSObject* retObj = nullptr;
  switch (func.funcType().ret().code()) {
    case ExprType::Void:
      args.rval().set(UndefinedValue());
      break;
    case ExprType::I32:
      args.rval().set(Int32Value(*(int32_t*)retAddr));
      break;
    case ExprType::I64:
      MOZ_CRASH("unexpected i64 flowing from callExport");
    case ExprType::F32:
      args.rval().set(NumberValue(*(float*)retAddr));
      break;
    case ExprType::F64:
      args.rval().set(NumberValue(*(double*)retAddr));
      break;
    case ExprType::Ref:
    case ExprType::AnyRef:
      retObj = *(JSObject**)retAddr;
      expectsObject = true;
      break;
    case ExprType::NullRef:
      MOZ_CRASH("NullRef not expressible");
    case ExprType::Limit:
      MOZ_CRASH("Limit");
  }

  if (expectsObject) {
    args.rval().set(ObjectOrNullValue(retObj));
  } else if (retObj) {
    args.rval().set(ObjectValue(*retObj));
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

void Instance::onMovingGrowMemory(uint8_t* prevMemoryBase) {
  MOZ_ASSERT(!isAsmJS());
  MOZ_ASSERT(!memory_->isShared());

  ArrayBufferObject& buffer = memory_->buffer().as<ArrayBufferObject>();
  tlsData()->memoryBase = buffer.dataPointer();
  tlsData()->boundsCheckLimit = buffer.wasmBoundsCheckLimit();
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

void Instance::deoptimizeImportExit(uint32_t funcImportIndex) {
  Tier t = code().bestTier();
  const FuncImport& fi = metadata(t).funcImports[funcImportIndex];
  FuncImportTls& import = funcImportTls(fi);
  import.code = codeBase(t) + fi.interpExitCodeOffset();
  import.baselineScript = nullptr;
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

  StringBuffer result(cx);
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
    for (size_t i = 0; i < sizeof(ModuleHash); i++) {
      char digit1 = hash[i] / 16, digit2 = hash[i] % 16;
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

void Instance::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                             Metadata::SeenSet* seenMetadata,
                             ShareableBytes::SeenSet* seenBytes,
                             Code::SeenSet* seenCode,
                             Table::SeenSet* seenTables, size_t* code,
                             size_t* data) const {
  *data += mallocSizeOf(this);
  *data += mallocSizeOf(tlsData_.get());
  for (const SharedTable& table : tables_) {
    *data += table->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenTables);
  }

  if (maybeDebug_) {
    maybeDebug_->addSizeOfMisc(mallocSizeOf, seenMetadata, seenBytes, seenCode,
                               code, data);
  }

  code_->addSizeOfMiscIfNotSeen(mallocSizeOf, seenMetadata, seenCode, code,
                                data);
}

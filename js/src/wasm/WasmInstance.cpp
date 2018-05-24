/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmModule.h"

#include "gc/StoreBuffer-inl.h"
#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::BitwiseCast;

class FuncTypeIdSet
{
    typedef HashMap<const FuncType*, uint32_t, FuncTypeHashPolicy, SystemAllocPolicy> Map;
    Map map_;

  public:
    ~FuncTypeIdSet() {
        MOZ_ASSERT_IF(!JSRuntime::hasLiveRuntimes(), !map_.initialized() || map_.empty());
    }

    bool ensureInitialized(JSContext* cx) {
        if (!map_.initialized() && !map_.init()) {
            ReportOutOfMemory(cx);
            return false;
        }

        return true;
    }

    bool allocateFuncTypeId(JSContext* cx, const FuncType& funcType, const void** funcTypeId) {
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

const void**
Instance::addressOfFuncTypeId(const FuncTypeIdDesc& funcTypeId) const
{
    return (const void**)(globalData() + funcTypeId.globalDataOffset());
}

FuncImportTls&
Instance::funcImportTls(const FuncImport& fi)
{
    return *(FuncImportTls*)(globalData() + fi.tlsDataOffset());
}

TableTls&
Instance::tableTls(const TableDesc& td) const
{
    return *(TableTls*)(globalData() + td.globalDataOffset);
}

bool
Instance::callImport(JSContext* cx, uint32_t funcImportIndex, unsigned argc, const uint64_t* argv,
                     MutableHandleValue rval)
{
    AssertRealmUnchanged aru(cx);

    Tier tier = code().bestTier();

    const FuncImport& fi = metadata(tier).funcImports[funcImportIndex];

    InvokeArgs args(cx);
    if (!args.init(cx, argc))
        return false;

    if (fi.funcType().hasI64ArgOrRet()) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_I64_TYPE);
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
          case ValType::I8x16:
          case ValType::I16x8:
          case ValType::I32x4:
          case ValType::F32x4:
          case ValType::B8x16:
          case ValType::B16x8:
          case ValType::B32x4:
            MOZ_CRASH("unhandled type in callImport");
        }
    }

    FuncImportTls& import = funcImportTls(fi);
    RootedFunction importFun(cx, &import.obj->as<JSFunction>());
    MOZ_ASSERT(cx->realm() == importFun->realm());

    RootedValue fval(cx, ObjectValue(*import.obj));
    RootedValue thisv(cx, UndefinedValue());
    if (!Call(cx, fval, thisv, args, rval))
        return false;

    // The import may already have become optimized.
    for (auto t : code().tiers()) {
        void* jitExitCode = codeBase(t) + fi.jitExitCodeOffset();
        if (import.code == jitExitCode)
            return true;
    }

    void* jitExitCode = codeBase(tier) + fi.jitExitCodeOffset();

    // Test if the function is JIT compiled.
    if (!importFun->hasScript())
        return true;

    JSScript* script = importFun->nonLazyScript();
    if (!script->hasBaselineScript()) {
        MOZ_ASSERT(!script->hasIonScript());
        return true;
    }

    // Don't enable jit entry when we have a pending ion builder.
    // Take the interpreter path which will link it and enable
    // the fast path on the next call.
    if (script->baselineScript()->hasPendingIonBuilder())
        return true;

    // Ensure the argument types are included in the argument TypeSets stored in
    // the TypeScript. This is necessary for Ion, because the import will use
    // the skip-arg-checks entry point.
    //
    // Note that the TypeScript is never discarded while the script has a
    // BaselineScript, so if those checks hold now they must hold at least until
    // the BaselineScript is discarded and when that happens the import is
    // patched back.
    if (!TypeScript::ThisTypes(script)->hasType(TypeSet::UndefinedType()))
        return true;

    // Functions with anyref in signature don't have a jit exit at the moment.
    if (fi.funcType().temporarilyUnsupportedAnyRef())
        return true;

    const ValTypeVector& importArgs = fi.funcType().args();

    size_t numKnownArgs = Min(importArgs.length(), importFun->nargs());
    for (uint32_t i = 0; i < numKnownArgs; i++) {
        TypeSet::Type type = TypeSet::UnknownType();
        switch (importArgs[i].code()) {
          case ValType::I32:    type = TypeSet::Int32Type(); break;
          case ValType::F32:    type = TypeSet::DoubleType(); break;
          case ValType::F64:    type = TypeSet::DoubleType(); break;
          case ValType::Ref:    MOZ_CRASH("case guarded above");
          case ValType::AnyRef: MOZ_CRASH("case guarded above");
          case ValType::I64:    MOZ_CRASH("NYI");
          case ValType::I8x16:  MOZ_CRASH("NYI");
          case ValType::I16x8:  MOZ_CRASH("NYI");
          case ValType::I32x4:  MOZ_CRASH("NYI");
          case ValType::F32x4:  MOZ_CRASH("NYI");
          case ValType::B8x16:  MOZ_CRASH("NYI");
          case ValType::B16x8:  MOZ_CRASH("NYI");
          case ValType::B32x4:  MOZ_CRASH("NYI");
        }
        if (!TypeScript::ArgTypes(script, i)->hasType(type))
            return true;
    }

    // These arguments will be filled with undefined at runtime by the
    // arguments rectifier: check that the imported function can handle
    // undefined there.
    for (uint32_t i = importArgs.length(); i < importFun->nargs(); i++) {
        if (!TypeScript::ArgTypes(script, i)->hasType(TypeSet::UndefinedType()))
            return true;
    }

    // Let's optimize it!
    if (!script->baselineScript()->addDependentWasmImport(cx, *this, funcImportIndex))
        return false;

    import.code = jitExitCode;
    import.baselineScript = script->baselineScript();
    return true;
}

/* static */ int32_t
Instance::callImport_void(Instance* instance, int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    JSContext* cx = TlsContext.get();
    RootedValue rval(cx);
    return instance->callImport(cx, funcImportIndex, argc, argv, &rval);
}

/* static */ int32_t
Instance::callImport_i32(Instance* instance, int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    JSContext* cx = TlsContext.get();
    RootedValue rval(cx);
    if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;

    return ToInt32(cx, rval, (int32_t*)argv);
}

/* static */ int32_t
Instance::callImport_i64(Instance* instance, int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    JSContext* cx = TlsContext.get();
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_I64_TYPE);
    return false;
}

/* static */ int32_t
Instance::callImport_f64(Instance* instance, int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    JSContext* cx = TlsContext.get();
    RootedValue rval(cx);
    if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;

    return ToNumber(cx, rval, (double*)argv);
}

static bool
ToRef(JSContext* cx, HandleValue val, void* addr)
{
    if (val.isNull()) {
        *(JSObject**)addr = nullptr;
        return true;
    }

    JSObject* obj = ToObject(cx, val);
    if (!obj)
        return false;
    *(JSObject**)addr = obj;
    return true;
}

/* static */ int32_t
Instance::callImport_ref(Instance* instance, int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    JSContext* cx = TlsContext.get();
    RootedValue rval(cx);
    if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;
    return ToRef(cx, rval, argv);
}

/* static */ uint32_t
Instance::growMemory_i32(Instance* instance, uint32_t delta)
{
    MOZ_ASSERT(!instance->isAsmJS());

    JSContext* cx = TlsContext.get();
    RootedWasmMemoryObject memory(cx, instance->memory_);

    uint32_t ret = WasmMemoryObject::grow(memory, delta, cx);

    // If there has been a moving grow, this Instance should have been notified.
    MOZ_RELEASE_ASSERT(instance->tlsData()->memoryBase ==
                       instance->memory_->buffer().dataPointerEither());

    return ret;
}

/* static */ uint32_t
Instance::currentMemory_i32(Instance* instance)
{
    // This invariant must hold when running Wasm code. Assert it here so we can
    // write tests for cross-realm calls.
    MOZ_ASSERT(TlsContext.get()->realm() == instance->realm());

    uint32_t byteLength = instance->memory()->volatileMemoryLength();
    MOZ_ASSERT(byteLength % wasm::PageSize == 0);
    return byteLength / wasm::PageSize;
}

template<typename T>
static int32_t
PerformWait(Instance* instance, uint32_t byteOffset, T value, int64_t timeout_ns)
{
    JSContext* cx = TlsContext.get();

    if (byteOffset & (sizeof(T) - 1)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_UNALIGNED_ACCESS);
        return -1;
    }

    if (byteOffset + sizeof(T) > instance->memory()->volatileMemoryLength()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_OUT_OF_BOUNDS);
        return -1;
    }

    mozilla::Maybe<mozilla::TimeDuration> timeout;
    if (timeout_ns >= 0)
        timeout = mozilla::Some(mozilla::TimeDuration::FromMicroseconds(timeout_ns / 1000));

    switch (atomics_wait_impl(cx, instance->sharedMemoryBuffer(), byteOffset, value, timeout)) {
      case FutexThread::WaitResult::OK:       return 0;
      case FutexThread::WaitResult::NotEqual: return 1;
      case FutexThread::WaitResult::TimedOut: return 2;
      case FutexThread::WaitResult::Error:    return -1;
      default: MOZ_CRASH();
    }
}

/* static */ int32_t
Instance::wait_i32(Instance* instance, uint32_t byteOffset, int32_t value, int64_t timeout_ns)
{
    return PerformWait<int32_t>(instance, byteOffset, value, timeout_ns);
}

/* static */ int32_t
Instance::wait_i64(Instance* instance, uint32_t byteOffset, int64_t value, int64_t timeout_ns)
{
    return PerformWait<int64_t>(instance, byteOffset, value, timeout_ns);
}

/* static */ int32_t
Instance::wake(Instance* instance, uint32_t byteOffset, int32_t count)
{
    JSContext* cx = TlsContext.get();

    // The alignment guard is not in the wasm spec as of 2017-11-02, but is
    // considered likely to appear, as 4-byte alignment is required for WAKE by
    // the spec's validation algorithm.

    if (byteOffset & 3) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_UNALIGNED_ACCESS);
        return -1;
    }

    if (byteOffset >= instance->memory()->volatileMemoryLength()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_OUT_OF_BOUNDS);
        return -1;
    }

    int64_t woken = atomics_wake_impl(instance->sharedMemoryBuffer(), byteOffset, int64_t(count));

    if (woken > INT32_MAX) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_WAKE_OVERFLOW);
        return -1;
    }

    return int32_t(woken);
}

/* static */ int32_t
Instance::memCopy(Instance* instance, uint32_t destByteOffset, uint32_t srcByteOffset, uint32_t len)
{
    WasmMemoryObject* mem = instance->memory();
    uint32_t memLen = mem->volatileMemoryLength();

    // Knowing that len > 0 below simplifies the wraparound checks.
    if (len == 0) {
        // Even though the length is zero, we must check for a valid offset.
        if (destByteOffset < memLen && srcByteOffset < memLen)
            return 0;

        // else fall through to failure case
    } else {
        ArrayBufferObjectMaybeShared& arrBuf = mem->buffer();
        uint8_t* rawBuf = arrBuf.dataPointerEither().unwrap();

        // Here, we know that |len - 1| cannot underflow.
        typedef CheckedInt<uint32_t> CheckedU32;
        CheckedU32 highest_destOffset = CheckedU32(destByteOffset) + CheckedU32(len - 1);
        CheckedU32 highest_srcOffset = CheckedU32(srcByteOffset) + CheckedU32(len - 1);

        if (highest_destOffset.isValid()   &&   // wraparound check
            highest_srcOffset.isValid()    &&   // wraparound check
            highest_destOffset.value() < memLen &&   // range check
            highest_srcOffset.value() < memLen)      // range check
        {
            memmove(rawBuf + destByteOffset, rawBuf + srcByteOffset, size_t(len));
            return 0;
        }
        // else fall through to failure case
    }

    JSContext* cx = TlsContext.get();
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
}

/* static */ int32_t
Instance::memFill(Instance* instance, uint32_t byteOffset, uint32_t value, uint32_t len)
{
    WasmMemoryObject* mem = instance->memory();
    uint32_t memLen = mem->volatileMemoryLength();

    // Knowing that len > 0 below simplifies the wraparound check.
    if (len == 0) {
        // Even though the length is zero, we must check for a valid offset.
        if (byteOffset < memLen)
            return 0;

        // else fall through to failure case
    } else {
        ArrayBufferObjectMaybeShared& arrBuf = mem->buffer();
        uint8_t* rawBuf = arrBuf.dataPointerEither().unwrap();

        // Here, we know that |len - 1| cannot underflow.
        typedef CheckedInt<uint32_t> CheckedU32;
        CheckedU32 highest_offset = CheckedU32(byteOffset) + CheckedU32(len - 1);

        if (highest_offset.isValid() &&     // wraparound check
            highest_offset.value() < memLen)     // range check
        {
            memset(rawBuf + byteOffset, int(value), size_t(len));
            return 0;
        }
        // else fall through to failure case
    }

    JSContext* cx = TlsContext.get();
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_OUT_OF_BOUNDS);
    return -1;
}

/* static */ void
Instance::postBarrier(Instance* instance, PostBarrierArg arg)
{
    gc::Cell** cell = nullptr;
    switch (arg.type()) {
      case PostBarrierArg::Type::Global: {
        const GlobalDesc& global = instance->metadata().globals[arg.globalIndex()];
        MOZ_ASSERT(!global.isConstant());
        MOZ_ASSERT(global.type().isRefOrAnyRef());
        uint8_t* globalAddr = instance->globalData() + global.offset();
        if (global.isIndirect())
            globalAddr = *(uint8_t**)globalAddr;
        MOZ_ASSERT(*(JSObject**)globalAddr, "shouldn't call postbarrier if null");
        cell = (gc::Cell**) globalAddr;
        break;
      }
    }

    MOZ_ASSERT(cell);
    TlsContext.get()->runtime()->gc.storeBuffer().putCell(cell);
}

Instance::Instance(JSContext* cx,
                   Handle<WasmInstanceObject*> object,
                   SharedCode code,
                   UniqueDebugState debug,
                   UniqueTlsData tlsDataIn,
                   HandleWasmMemoryObject memory,
                   SharedTableVector&& tables,
                   Handle<FunctionVector> funcImports,
                   HandleValVector globalImportValues,
                   const WasmGlobalObjectVector& globalObjs)
  : realm_(cx->realm()),
    object_(object),
    code_(code),
    debug_(std::move(debug)),
    tlsData_(std::move(tlsDataIn)),
    memory_(memory),
    tables_(std::move(tables)),
    enterFrameTrapsEnabled_(false)
{
#ifdef DEBUG
    for (auto t : code_->tiers())
        MOZ_ASSERT(funcImports.length() == metadata(t).funcImports.length());
#endif
    MOZ_ASSERT(tables_.length() == metadata().tables.length());

    tlsData()->memoryBase = memory ? memory->buffer().dataPointerEither().unwrap() : nullptr;
#ifndef WASM_HUGE_MEMORY
    tlsData()->boundsCheckLimit = memory ? memory->buffer().wasmBoundsCheckLimit() : 0;
#endif
    tlsData()->instance = this;
    tlsData()->realm = realm_;
    tlsData()->cx = cx;
    tlsData()->resetInterrupt(cx);
    tlsData()->jumpTable = code_->tieringJumpTable();
#ifdef ENABLE_WASM_GC
    tlsData()->addressOfNeedsIncrementalBarrier =
        (uint8_t*)cx->compartment()->zone()->addressOfNeedsIncrementalBarrier();
#endif

    Tier callerTier = code_->bestTier();

    for (size_t i = 0; i < metadata(callerTier).funcImports.length(); i++) {
        HandleFunction f = funcImports[i];
        const FuncImport& fi = metadata(callerTier).funcImports[i];
        FuncImportTls& import = funcImportTls(fi);
        if (!isAsmJS() && IsExportedWasmFunction(f)) {
            WasmInstanceObject* calleeInstanceObj = ExportedFunctionToInstanceObject(f);
            Instance& calleeInstance = calleeInstanceObj->instance();
            Tier calleeTier = calleeInstance.code().bestTier();
            const CodeRange& codeRange = calleeInstanceObj->getExportedFunctionCodeRange(f, calleeTier);
            import.tls = calleeInstance.tlsData();
            import.realm = f->realm();
            import.code = calleeInstance.codeBase(calleeTier) + codeRange.funcNormalEntry();
            import.baselineScript = nullptr;
            import.obj = calleeInstanceObj;
        } else if (void* thunk = MaybeGetBuiltinThunk(f, fi.funcType())) {
            import.tls = tlsData();
            import.realm = f->realm();
            import.code = thunk;
            import.baselineScript = nullptr;
            import.obj = f;
        } else {
            import.tls = tlsData();
            import.realm = f->realm();
            import.code = codeBase(callerTier) + fi.interpExitCodeOffset();
            import.baselineScript = nullptr;
            import.obj = f;
        }
    }

    for (size_t i = 0; i < tables_.length(); i++) {
        const TableDesc& td = metadata().tables[i];
        TableTls& table = tableTls(td);
        table.length = tables_[i]->length();
        table.base = tables_[i]->base();
    }

    for (size_t i = 0; i < metadata().globals.length(); i++) {
        const GlobalDesc& global = metadata().globals[i];

        // Constants are baked into the code, never stored in the global area.
        if (global.isConstant())
            continue;

        uint8_t* globalAddr = globalData() + global.offset();
        switch (global.kind()) {
          case GlobalKind::Import: {
            size_t imported = global.importIndex();
            if (global.isIndirect())
                *(void**)globalAddr = globalObjs[imported]->cell();
            else
                globalImportValues[imported].get().writePayload(globalAddr);
            break;
          }
          case GlobalKind::Variable: {
            const InitExpr& init = global.initExpr();
            switch (init.kind()) {
              case InitExpr::Kind::Constant: {
                if (global.isIndirect())
                    *(void**)globalAddr = globalObjs[i]->cell();
                else
                    Val(init.val()).writePayload(globalAddr);
                break;
              }
              case InitExpr::Kind::GetGlobal: {
                const GlobalDesc& imported = metadata().globals[init.globalIndex()];

                // Global-ref initializers cannot reference mutable globals, so
                // the source global should never be indirect.
                MOZ_ASSERT(!imported.isIndirect());

                RootedVal dest(cx, globalImportValues[imported.importIndex()].get());
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

bool
Instance::init(JSContext* cx)
{
    if (memory_ && memory_->movingGrowable() && !memory_->addMovingGrowObserver(cx, object_))
        return false;

    for (const SharedTable& table : tables_) {
        if (table->movingGrowable() && !table->addMovingGrowObserver(cx, object_))
            return false;
    }

    if (!metadata().funcTypeIds.empty()) {
        ExclusiveData<FuncTypeIdSet>::Guard lockedFuncTypeIdSet = funcTypeIdSet.lock();

        if (!lockedFuncTypeIdSet->ensureInitialized(cx))
            return false;

        for (const FuncTypeWithId& funcType : metadata().funcTypeIds) {
            const void* funcTypeId;
            if (!lockedFuncTypeIdSet->allocateFuncTypeId(cx, funcType, &funcTypeId))
                return false;

            *addressOfFuncTypeId(funcType.id) = funcTypeId;
        }
    }

    JitRuntime* jitRuntime = cx->runtime()->getJitRuntime(cx);
    if (!jitRuntime)
        return false;
    jsJitArgsRectifier_ = jitRuntime->getArgumentsRectifier();
    jsJitExceptionHandler_ = jitRuntime->getExceptionTail();
    preBarrierCode_ = jitRuntime->preBarrier(MIRType::Object);
    return true;
}

Instance::~Instance()
{
    realm_->wasm.unregisterInstance(*this);

    const FuncImportVector& funcImports = metadata(code().stableTier()).funcImports;

    for (unsigned i = 0; i < funcImports.length(); i++) {
        FuncImportTls& import = funcImportTls(funcImports[i]);
        if (import.baselineScript)
            import.baselineScript->removeDependentWasmImport(*this, i);
    }

    if (!metadata().funcTypeIds.empty()) {
        ExclusiveData<FuncTypeIdSet>::Guard lockedFuncTypeIdSet = funcTypeIdSet.lock();

        for (const FuncTypeWithId& funcType : metadata().funcTypeIds) {
            if (const void* funcTypeId = *addressOfFuncTypeId(funcType.id))
                lockedFuncTypeIdSet->deallocateFuncTypeId(funcType, funcTypeId);
        }
    }
}

size_t
Instance::memoryMappedSize() const
{
    return memory_->buffer().wasmMappedSize();
}

#ifdef JS_SIMULATOR
bool
Instance::memoryAccessInGuardRegion(uint8_t* addr, unsigned numBytes) const
{
    MOZ_ASSERT(numBytes > 0);

    if (!metadata().usesMemory())
        return false;

    uint8_t* base = memoryBase().unwrap(/* comparison */);
    if (addr < base)
        return false;

    size_t lastByteOffset = addr - base + (numBytes - 1);
    return lastByteOffset >= memory()->volatileMemoryLength() && lastByteOffset < memoryMappedSize();
}
#endif

void
Instance::tracePrivate(JSTracer* trc)
{
    // This method is only called from WasmInstanceObject so the only reason why
    // TraceEdge is called is so that the pointer can be updated during a moving
    // GC. TraceWeakEdge may sound better, but it is less efficient given that
    // we know object_ is already marked.
    MOZ_ASSERT(!gc::IsAboutToBeFinalized(&object_));
    TraceEdge(trc, &object_, "wasm instance object");

    // OK to just do one tier here; though the tiers have different funcImports
    // tables, they share the tls object.
    for (const FuncImport& fi : metadata(code().stableTier()).funcImports)
        TraceNullableEdge(trc, &funcImportTls(fi).obj, "wasm import");

    for (const SharedTable& table : tables_)
        table->trace(trc);

#ifdef ENABLE_WASM_GC
    for (const GlobalDesc& global : code().metadata().globals) {
        // Indirect anyref global get traced by the owning WebAssembly.Global.
        if (global.type() != ValType::AnyRef || global.isConstant() || global.isIndirect())
            continue;
        GCPtrObject* obj = (GCPtrObject*)(globalData() + global.offset());
        TraceNullableEdge(trc, obj, "wasm anyref global");
    }
#endif

    TraceNullableEdge(trc, &memory_, "wasm buffer");
}

void
Instance::trace(JSTracer* trc)
{
    // Technically, instead of having this method, the caller could use
    // Instance::object() to get the owning WasmInstanceObject to mark,
    // but this method is simpler and more efficient. The trace hook of
    // WasmInstanceObject will call Instance::tracePrivate at which point we
    // can mark the rest of the children.
    TraceEdge(trc, &object_, "wasm instance object");
}

WasmMemoryObject*
Instance::memory() const
{
    return memory_;
}

SharedMem<uint8_t*>
Instance::memoryBase() const
{
    MOZ_ASSERT(metadata().usesMemory());
    MOZ_ASSERT(tlsData()->memoryBase == memory_->buffer().dataPointerEither());
    return memory_->buffer().dataPointerEither();
}

SharedArrayRawBuffer*
Instance::sharedMemoryBuffer() const
{
    MOZ_ASSERT(memory_->isShared());
    return memory_->sharedArrayRawBuffer();
}

WasmInstanceObject*
Instance::objectUnbarriered() const
{
    return object_.unbarrieredGet();
}

WasmInstanceObject*
Instance::object() const
{
    return object_;
}

bool
Instance::callExport(JSContext* cx, uint32_t funcIndex, CallArgs args)
{
    // If there has been a moving grow, this Instance should have been notified.
    MOZ_RELEASE_ASSERT(!memory_ || tlsData()->memoryBase == memory_->buffer().dataPointerEither());

    Tier tier = code().bestTier();

    const FuncExport& func = metadata(tier).lookupFuncExport(funcIndex);

    if (func.funcType().hasI64ArgOrRet()) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_I64_TYPE);
        return false;
    }

    // The calling convention for an external call into wasm is to pass an
    // array of 16-byte values where each value contains either a coerced int32
    // (in the low word), a double value (in the low dword) or a SIMD vector
    // value, with the coercions specified by the wasm signature. The external
    // entry point unpacks this array into the system-ABI-specified registers
    // and stack memory and then calls into the internal entry point. The return
    // value is stored in the first element of the array (which, therefore, must
    // have length >= 1).
    Vector<ExportArg, 8> exportArgs(cx);
    if (!exportArgs.resize(Max<size_t>(1, func.funcType().args().length())))
        return false;

    RootedValue v(cx);
    for (unsigned i = 0; i < func.funcType().args().length(); ++i) {
        v = i < args.length() ? args[i] : UndefinedValue();
        switch (func.funcType().arg(i).code()) {
          case ValType::I32:
            if (!ToInt32(cx, v, (int32_t*)&exportArgs[i]))
                return false;
            break;
          case ValType::I64:
            MOZ_CRASH("unexpected i64 flowing into callExport");
          case ValType::F32:
            if (!RoundFloat32(cx, v, (float*)&exportArgs[i]))
                return false;
            break;
          case ValType::F64:
            if (!ToNumber(cx, v, (double*)&exportArgs[i]))
                return false;
            break;
          case ValType::Ref:
          case ValType::AnyRef: {
            if (!ToRef(cx, v, &exportArgs[i]))
                return false;
            break;
          }
          case ValType::I8x16: {
            SimdConstant simd;
            if (!ToSimdConstant<Int8x16>(cx, v, &simd))
                return false;
            memcpy(&exportArgs[i], simd.asInt8x16(), Simd128DataSize);
            break;
          }
          case ValType::I16x8: {
            SimdConstant simd;
            if (!ToSimdConstant<Int16x8>(cx, v, &simd))
                return false;
            memcpy(&exportArgs[i], simd.asInt16x8(), Simd128DataSize);
            break;
          }
          case ValType::I32x4: {
            SimdConstant simd;
            if (!ToSimdConstant<Int32x4>(cx, v, &simd))
                return false;
            memcpy(&exportArgs[i], simd.asInt32x4(), Simd128DataSize);
            break;
          }
          case ValType::F32x4: {
            SimdConstant simd;
            if (!ToSimdConstant<Float32x4>(cx, v, &simd))
                return false;
            memcpy(&exportArgs[i], simd.asFloat32x4(), Simd128DataSize);
            break;
          }
          case ValType::B8x16: {
            SimdConstant simd;
            if (!ToSimdConstant<Bool8x16>(cx, v, &simd))
                return false;
            // Bool8x16 uses the same representation as Int8x16.
            memcpy(&exportArgs[i], simd.asInt8x16(), Simd128DataSize);
            break;
          }
          case ValType::B16x8: {
            SimdConstant simd;
            if (!ToSimdConstant<Bool16x8>(cx, v, &simd))
                return false;
            // Bool16x8 uses the same representation as Int16x8.
            memcpy(&exportArgs[i], simd.asInt16x8(), Simd128DataSize);
            break;
          }
          case ValType::B32x4: {
            SimdConstant simd;
            if (!ToSimdConstant<Bool32x4>(cx, v, &simd))
                return false;
            // Bool32x4 uses the same representation as Int32x4.
            memcpy(&exportArgs[i], simd.asInt32x4(), Simd128DataSize);
            break;
          }
        }
    }

    {
        JitActivation activation(cx);

        void* callee;
        if (func.hasEagerStubs())
            callee = codeBase(tier) + func.eagerInterpEntryOffset();
        else
            callee = code(tier).lazyStubs().lock()->lookupInterpEntry(funcIndex);

        // Call the per-exported-function trampoline created by GenerateEntry.
        auto funcPtr = JS_DATA_TO_FUNC_PTR(ExportFuncPtr, callee);
        if (!CALL_GENERATED_2(funcPtr, exportArgs.begin(), tlsData()))
            return false;
    }

    if (isAsmJS() && args.isConstructing()) {
        // By spec, when a JS function is called as a constructor and this
        // function returns a primary type, which is the case for all asm.js
        // exported functions, the returned value is discarded and an empty
        // object is returned instead.
        PlainObject* obj = NewBuiltinClassInstance<PlainObject>(cx);
        if (!obj)
            return false;
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
      case ExprType::I8x16:
        retObj = CreateSimd<Int8x16>(cx, (int8_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::I16x8:
        retObj = CreateSimd<Int16x8>(cx, (int16_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::I32x4:
        retObj = CreateSimd<Int32x4>(cx, (int32_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::F32x4:
        retObj = CreateSimd<Float32x4>(cx, (float*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::B8x16:
        retObj = CreateSimd<Bool8x16>(cx, (int8_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::B16x8:
        retObj = CreateSimd<Bool16x8>(cx, (int16_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::B32x4:
        retObj = CreateSimd<Bool32x4>(cx, (int32_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::Limit:
        MOZ_CRASH("Limit");
    }

    if (expectsObject)
        args.rval().set(ObjectOrNullValue(retObj));
    else if (retObj)
        args.rval().set(ObjectValue(*retObj));

    return true;
}

JSAtom*
Instance::getFuncDisplayAtom(JSContext* cx, uint32_t funcIndex) const
{
    // The "display name" of a function is primarily shown in Error.stack which
    // also includes location, so use getFuncNameBeforeLocation.
    UTF8Bytes name;
    if (!metadata().getFuncNameBeforeLocation(debug_->maybeBytecode(), funcIndex, &name))
        return nullptr;

    return AtomizeUTF8Chars(cx, name.begin(), name.length());
}

void
Instance::ensureProfilingLabels(bool profilingEnabled) const
{
    return code_->ensureProfilingLabels(debug_->maybeBytecode(), profilingEnabled);
}

void
Instance::onMovingGrowMemory(uint8_t* prevMemoryBase)
{
    MOZ_ASSERT(!isAsmJS());
    MOZ_ASSERT(!memory_->isShared());

    ArrayBufferObject& buffer = memory_->buffer().as<ArrayBufferObject>();
    tlsData()->memoryBase = buffer.dataPointer();
#ifndef WASM_HUGE_MEMORY
    tlsData()->boundsCheckLimit = buffer.wasmBoundsCheckLimit();
#endif
}

void
Instance::onMovingGrowTable()
{
    MOZ_ASSERT(!isAsmJS());
    MOZ_ASSERT(tables_.length() == 1);
    TableTls& table = tableTls(metadata().tables[0]);
    table.length = tables_[0]->length();
    table.base = tables_[0]->base();
}

void
Instance::deoptimizeImportExit(uint32_t funcImportIndex)
{
    Tier t = code().bestTier();
    const FuncImport& fi = metadata(t).funcImports[funcImportIndex];
    FuncImportTls& import = funcImportTls(fi);
    import.code = codeBase(t) + fi.interpExitCodeOffset();
    import.baselineScript = nullptr;
}

void
Instance::ensureEnterFrameTrapsState(JSContext* cx, bool enabled)
{
    if (enterFrameTrapsEnabled_ == enabled)
        return;

    debug_->adjustEnterAndLeaveFrameTrapsState(cx, enabled);
    enterFrameTrapsEnabled_ = enabled;
}

void
Instance::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                        Metadata::SeenSet* seenMetadata,
                        ShareableBytes::SeenSet* seenBytes,
                        Code::SeenSet* seenCode,
                        Table::SeenSet* seenTables,
                        size_t* code,
                        size_t* data) const
{
    *data += mallocSizeOf(this);
    *data += mallocSizeOf(tlsData_.get());
    for (const SharedTable& table : tables_)
         *data += table->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenTables);

    debug_->addSizeOfMisc(mallocSizeOf, seenMetadata, seenBytes, seenCode, code, data);
    code_->addSizeOfMiscIfNotSeen(mallocSizeOf, seenMetadata, seenCode, code, data);
}

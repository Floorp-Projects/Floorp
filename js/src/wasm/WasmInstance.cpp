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

#include "jit/BaselineJIT.h"
#include "jit/JitCommon.h"
#include "wasm/WasmModule.h"

#include "jsobjinlines.h"

#include "vm/ArrayBufferObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::BinarySearch;
using mozilla::BitwiseCast;
using mozilla::IsNaN;
using mozilla::Swap;

class SigIdSet
{
    typedef HashMap<const Sig*, uint32_t, SigHashPolicy, SystemAllocPolicy> Map;
    Map map_;

  public:
    ~SigIdSet() {
        MOZ_ASSERT_IF(!JSRuntime::hasLiveRuntimes(), !map_.initialized() || map_.empty());
    }

    bool ensureInitialized(JSContext* cx) {
        if (!map_.initialized() && !map_.init()) {
            ReportOutOfMemory(cx);
            return false;
        }

        return true;
    }

    bool allocateSigId(JSContext* cx, const Sig& sig, const void** sigId) {
        Map::AddPtr p = map_.lookupForAdd(sig);
        if (p) {
            MOZ_ASSERT(p->value() > 0);
            p->value()++;
            *sigId = p->key();
            return true;
        }

        UniquePtr<Sig> clone = MakeUnique<Sig>();
        if (!clone || !clone->clone(sig) || !map_.add(p, clone.get(), 1)) {
            ReportOutOfMemory(cx);
            return false;
        }

        *sigId = clone.release();
        MOZ_ASSERT(!(uintptr_t(*sigId) & SigIdDesc::ImmediateBit));
        return true;
    }

    void deallocateSigId(const Sig& sig, const void* sigId) {
        Map::Ptr p = map_.lookup(sig);
        MOZ_RELEASE_ASSERT(p && p->key() == sigId && p->value() > 0);

        p->value()--;
        if (!p->value()) {
            js_delete(p->key());
            map_.remove(p);
        }
    }
};

ExclusiveData<SigIdSet>* sigIdSet = nullptr;

bool
js::wasm::InitInstanceStaticData()
{
    MOZ_ASSERT(!sigIdSet);
    sigIdSet = js_new<ExclusiveData<SigIdSet>>(mutexid::WasmSigIdSet);
    return sigIdSet != nullptr;
}

void
js::wasm::ShutDownInstanceStaticData()
{
    MOZ_ASSERT(sigIdSet);
    js_delete(sigIdSet);
    sigIdSet = nullptr;
}

const void**
Instance::addressOfSigId(const SigIdDesc& sigId) const
{
    MOZ_ASSERT(sigId.globalDataOffset() >= InitialGlobalDataBytes);
    return (const void**)(codeSegment().globalData() + sigId.globalDataOffset());
}

FuncImportTls&
Instance::funcImportTls(const FuncImport& fi)
{
    MOZ_ASSERT(fi.tlsDataOffset() >= InitialGlobalDataBytes);
    return *(FuncImportTls*)(codeSegment().globalData() + fi.tlsDataOffset());
}

TableTls&
Instance::tableTls(const TableDesc& td) const
{
    MOZ_ASSERT(td.globalDataOffset >= InitialGlobalDataBytes);
    return *(TableTls*)(codeSegment().globalData() + td.globalDataOffset);
}

bool
Instance::callImport(JSContext* cx, uint32_t funcImportIndex, unsigned argc, const uint64_t* argv,
                     MutableHandleValue rval)
{
    const FuncImport& fi = metadata().funcImports[funcImportIndex];

    InvokeArgs args(cx);
    if (!args.init(cx, argc))
        return false;

    bool hasI64Arg = false;
    MOZ_ASSERT(fi.sig().args().length() == argc);
    for (size_t i = 0; i < argc; i++) {
        switch (fi.sig().args()[i]) {
          case ValType::I32:
            args[i].set(Int32Value(*(int32_t*)&argv[i]));
            break;
          case ValType::F32:
            args[i].set(JS::CanonicalizedDoubleValue(*(float*)&argv[i]));
            break;
          case ValType::F64:
            args[i].set(JS::CanonicalizedDoubleValue(*(double*)&argv[i]));
            break;
          case ValType::I64: {
            if (!JitOptions.wasmTestMode) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_I64);
                return false;
            }
            RootedObject obj(cx, CreateI64Object(cx, *(int64_t*)&argv[i]));
            if (!obj)
                return false;
            args[i].set(ObjectValue(*obj));
            hasI64Arg = true;
            break;
          }
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
    RootedValue fval(cx, ObjectValue(*import.obj));
    RootedValue thisv(cx, UndefinedValue());
    if (!Call(cx, fval, thisv, args, rval))
        return false;

    // Throw an error if returning i64 and not in test mode.
    if (!JitOptions.wasmTestMode && fi.sig().ret() == ExprType::I64) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_I64);
        return false;
    }

    // Don't try to optimize if the function has at least one i64 arg or if
    // it returns an int64. GenerateJitExit relies on this, as does the
    // type inference code below in this function.
    if (hasI64Arg || fi.sig().ret() == ExprType::I64)
        return true;

    // The import may already have become optimized.
    void* jitExitCode = codeBase() + fi.jitExitCodeOffset();
    if (import.code == jitExitCode)
        return true;

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

    // Currently we can't rectify arguments. Therefore disable if argc is too low.
    if (importFun->nargs() > fi.sig().args().length())
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
    for (uint32_t i = 0; i < importFun->nargs(); i++) {
        TypeSet::Type type = TypeSet::UnknownType();
        switch (fi.sig().args()[i]) {
          case ValType::I32:   type = TypeSet::Int32Type(); break;
          case ValType::I64:   MOZ_CRASH("can't happen because of above guard");
          case ValType::F32:   type = TypeSet::DoubleType(); break;
          case ValType::F64:   type = TypeSet::DoubleType(); break;
          case ValType::I8x16: MOZ_CRASH("NYI");
          case ValType::I16x8: MOZ_CRASH("NYI");
          case ValType::I32x4: MOZ_CRASH("NYI");
          case ValType::F32x4: MOZ_CRASH("NYI");
          case ValType::B8x16: MOZ_CRASH("NYI");
          case ValType::B16x8: MOZ_CRASH("NYI");
          case ValType::B32x4: MOZ_CRASH("NYI");
        }
        if (!TypeScript::ArgTypes(script, i)->hasType(type))
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
    JSContext* cx = instance->cx();
    RootedValue rval(cx);
    return instance->callImport(cx, funcImportIndex, argc, argv, &rval);
}

/* static */ int32_t
Instance::callImport_i32(Instance* instance, int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    JSContext* cx = instance->cx();
    RootedValue rval(cx);
    if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;

    return ToInt32(cx, rval, (int32_t*)argv);
}

/* static */ int32_t
Instance::callImport_i64(Instance* instance, int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    JSContext* cx = instance->cx();
    RootedValue rval(cx);
    if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;

    return ReadI64Object(cx, rval, (int64_t*)argv);
}

/* static */ int32_t
Instance::callImport_f64(Instance* instance, int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    JSContext* cx = instance->cx();
    RootedValue rval(cx);
    if (!instance->callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;

    return ToNumber(cx, rval, (double*)argv);
}

/* static */ uint32_t
Instance::growMemory_i32(Instance* instance, uint32_t delta)
{
    MOZ_ASSERT(!instance->isAsmJS());

    JSContext* cx = instance->cx();
    RootedWasmMemoryObject memory(cx, instance->memory_);

    uint32_t ret = WasmMemoryObject::grow(memory, delta, cx);

    // If there has been a moving grow, this Instance should have been notified.
    MOZ_RELEASE_ASSERT(instance->tlsData_.memoryBase ==
                       instance->memory_->buffer().dataPointerEither());

    return ret;
}

/* static */ uint32_t
Instance::currentMemory_i32(Instance* instance)
{
    uint32_t byteLength = instance->memoryLength();
    MOZ_ASSERT(byteLength % wasm::PageSize == 0);
    return byteLength / wasm::PageSize;
}

Instance::Instance(JSContext* cx,
                   Handle<WasmInstanceObject*> object,
                   UniqueCode code,
                   HandleWasmMemoryObject memory,
                   SharedTableVector&& tables,
                   Handle<FunctionVector> funcImports,
                   const ValVector& globalImports)
  : compartment_(cx->compartment()),
    object_(object),
    code_(Move(code)),
    memory_(memory),
    tables_(Move(tables))
{
    MOZ_ASSERT(funcImports.length() == metadata().funcImports.length());
    MOZ_ASSERT(tables_.length() == metadata().tables.length());

    tlsData_.cx = cx;
    tlsData_.instance = this;
    tlsData_.globalData = code_->segment().globalData();
    tlsData_.memoryBase = memory ? memory->buffer().dataPointerEither().unwrap() : nullptr;
    tlsData_.stackLimit = *(void**)cx->stackLimitAddressForJitCode(StackForUntrustedScript);

    for (size_t i = 0; i < metadata().funcImports.length(); i++) {
        HandleFunction f = funcImports[i];
        const FuncImport& fi = metadata().funcImports[i];
        FuncImportTls& import = funcImportTls(fi);
        if (!isAsmJS() && IsExportedWasmFunction(f)) {
            WasmInstanceObject* calleeInstanceObj = ExportedFunctionToInstanceObject(f);
            const CodeRange& codeRange = calleeInstanceObj->getExportedFunctionCodeRange(f);
            Instance& calleeInstance = calleeInstanceObj->instance();
            import.tls = &calleeInstance.tlsData_;
            import.code = calleeInstance.codeSegment().base() + codeRange.funcNonProfilingEntry();
            import.baselineScript = nullptr;
            import.obj = calleeInstanceObj;
        } else {
            import.tls = &tlsData_;
            import.code = codeBase() + fi.interpExitCodeOffset();
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

    uint8_t* globalData = code_->segment().globalData();

    for (size_t i = 0; i < metadata().globals.length(); i++) {
        const GlobalDesc& global = metadata().globals[i];
        if (global.isConstant())
            continue;

        uint8_t* globalAddr = globalData + global.offset();
        switch (global.kind()) {
          case GlobalKind::Import: {
            globalImports[global.importIndex()].writePayload(globalAddr);
            break;
          }
          case GlobalKind::Variable: {
            const InitExpr& init = global.initExpr();
            switch (init.kind()) {
              case InitExpr::Kind::Constant: {
                init.val().writePayload(globalAddr);
                break;
              }
              case InitExpr::Kind::GetGlobal: {
                const GlobalDesc& imported = metadata().globals[init.globalIndex()];
                globalImports[imported.importIndex()].writePayload(globalAddr);
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

    if (!metadata().sigIds.empty()) {
        ExclusiveData<SigIdSet>::Guard lockedSigIdSet = sigIdSet->lock();

        if (!lockedSigIdSet->ensureInitialized(cx))
            return false;

        for (const SigWithId& sig : metadata().sigIds) {
            const void* sigId;
            if (!lockedSigIdSet->allocateSigId(cx, sig, &sigId))
                return false;

            *addressOfSigId(sig.id) = sigId;
        }
    }

    return true;
}

Instance::~Instance()
{
    compartment_->wasm.unregisterInstance(*this);

    for (unsigned i = 0; i < metadata().funcImports.length(); i++) {
        FuncImportTls& import = funcImportTls(metadata().funcImports[i]);
        if (import.baselineScript)
            import.baselineScript->removeDependentWasmImport(*this, i);
    }

    if (!metadata().sigIds.empty()) {
        ExclusiveData<SigIdSet>::Guard lockedSigIdSet = sigIdSet->lock();

        for (const SigWithId& sig : metadata().sigIds) {
            if (const void* sigId = *addressOfSigId(sig.id))
                lockedSigIdSet->deallocateSigId(sig, sigId);
        }
    }
}

size_t
Instance::memoryMappedSize() const
{
    return memory_->buffer().wasmMappedSize();
}

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
    return lastByteOffset >= memoryLength() && lastByteOffset < memoryMappedSize();
}

void
Instance::tracePrivate(JSTracer* trc)
{
    // This method is only called from WasmInstanceObject so the only reason why
    // TraceEdge is called is so that the pointer can be updated during a moving
    // GC. TraceWeakEdge may sound better, but it is less efficient given that
    // we know object_ is already marked.
    MOZ_ASSERT(!gc::IsAboutToBeFinalized(&object_));
    TraceEdge(trc, &object_, "wasm instance object");

    for (const FuncImport& fi : metadata().funcImports)
        TraceNullableEdge(trc, &funcImportTls(fi).obj, "wasm import");

    for (const SharedTable& table : tables_)
        table->trace(trc);

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

SharedMem<uint8_t*>
Instance::memoryBase() const
{
    MOZ_ASSERT(metadata().usesMemory());
    MOZ_ASSERT(tlsData_.memoryBase == memory_->buffer().dataPointerEither());
    return memory_->buffer().dataPointerEither();
}

size_t
Instance::memoryLength() const
{
    return memory_->buffer().byteLength();
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
    MOZ_RELEASE_ASSERT(!memory_ || tlsData_.memoryBase == memory_->buffer().dataPointerEither());

    if (!cx->compartment()->wasm.ensureProfilingState(cx))
        return false;

    const FuncExport& func = metadata().lookupFuncExport(funcIndex);

    // The calling convention for an external call into wasm is to pass an
    // array of 16-byte values where each value contains either a coerced int32
    // (in the low word), a double value (in the low dword) or a SIMD vector
    // value, with the coercions specified by the wasm signature. The external
    // entry point unpacks this array into the system-ABI-specified registers
    // and stack memory and then calls into the internal entry point. The return
    // value is stored in the first element of the array (which, therefore, must
    // have length >= 1).
    Vector<ExportArg, 8> exportArgs(cx);
    if (!exportArgs.resize(Max<size_t>(1, func.sig().args().length())))
        return false;

    RootedValue v(cx);
    for (unsigned i = 0; i < func.sig().args().length(); ++i) {
        v = i < args.length() ? args[i] : UndefinedValue();
        switch (func.sig().arg(i)) {
          case ValType::I32:
            if (!ToInt32(cx, v, (int32_t*)&exportArgs[i]))
                return false;
            break;
          case ValType::I64:
            if (!JitOptions.wasmTestMode) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_I64);
                return false;
            }
            if (!ReadI64Object(cx, v, (int64_t*)&exportArgs[i]))
                return false;
            break;
          case ValType::F32:
            if (JitOptions.wasmTestMode && v.isObject()) {
                if (!ReadCustomFloat32NaNObject(cx, v, (uint32_t*)&exportArgs[i]))
                    return false;
                break;
            }
            if (!RoundFloat32(cx, v, (float*)&exportArgs[i]))
                return false;
            break;
          case ValType::F64:
            if (JitOptions.wasmTestMode && v.isObject()) {
                if (!ReadCustomDoubleNaNObject(cx, v, (uint64_t*)&exportArgs[i]))
                    return false;
                break;
            }
            if (!ToNumber(cx, v, (double*)&exportArgs[i]))
                return false;
            break;
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
        // Push a WasmActivation to describe the wasm frames we're about to push
        // when running this module. Additionally, push a JitActivation so that
        // the optimized wasm-to-Ion FFI call path (which we want to be very
        // fast) can avoid doing so. The JitActivation is marked as inactive so
        // stack iteration will skip over it.
        WasmActivation activation(cx);
        JitActivation jitActivation(cx, /* active */ false);

        // Call the per-exported-function trampoline created by GenerateEntry.
        auto funcPtr = JS_DATA_TO_FUNC_PTR(ExportFuncPtr, codeBase() + func.entryOffset());
        if (!CALL_GENERATED_2(funcPtr, exportArgs.begin(), &tlsData_))
            return false;
    }

    if (args.isConstructing()) {
        // By spec, when a function is called as a constructor and this function
        // returns a primary type, which is the case for all wasm exported
        // functions, the returned value is discarded and an empty object is
        // returned instead.
        PlainObject* obj = NewBuiltinClassInstance<PlainObject>(cx);
        if (!obj)
            return false;
        args.rval().set(ObjectValue(*obj));
        return true;
    }

    void* retAddr = &exportArgs[0];
    JSObject* retObj = nullptr;
    switch (func.sig().ret()) {
      case ExprType::Void:
        args.rval().set(UndefinedValue());
        break;
      case ExprType::I32:
        args.rval().set(Int32Value(*(int32_t*)retAddr));
        break;
      case ExprType::I64:
        if (!JitOptions.wasmTestMode) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_I64);
            return false;
        }
        retObj = CreateI64Object(cx, *(int64_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::F32:
        if (JitOptions.wasmTestMode && IsNaN(*(float*)retAddr)) {
            retObj = CreateCustomNaNObject(cx, (float*)retAddr);
            if (!retObj)
                return false;
            break;
        }
        args.rval().set(NumberValue(*(float*)retAddr));
        break;
      case ExprType::F64:
        if (JitOptions.wasmTestMode && IsNaN(*(double*)retAddr)) {
            retObj = CreateCustomNaNObject(cx, (double*)retAddr);
            if (!retObj)
                return false;
            break;
        }
        args.rval().set(NumberValue(*(double*)retAddr));
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

    if (retObj)
        args.rval().set(ObjectValue(*retObj));

    return true;
}

void
Instance::onMovingGrowMemory(uint8_t* prevMemoryBase)
{
    MOZ_ASSERT(!isAsmJS());
    ArrayBufferObject& buffer = memory_->buffer().as<ArrayBufferObject>();
    tlsData_.memoryBase = buffer.dataPointer();
    code_->segment().onMovingGrow(prevMemoryBase, metadata(), buffer);
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
    const FuncImport& fi = metadata().funcImports[funcImportIndex];
    FuncImportTls& import = funcImportTls(fi);
    import.code = codeBase() + fi.interpExitCodeOffset();
    import.baselineScript = nullptr;
}

static void
UpdateEntry(const Code& code, bool profilingEnabled, void** entry)
{
    const CodeRange& codeRange = *code.lookupRange(*entry);
    void* from = code.segment().base() + codeRange.funcNonProfilingEntry();
    void* to = code.segment().base() + codeRange.funcProfilingEntry();

    if (!profilingEnabled)
        Swap(from, to);

    MOZ_ASSERT(*entry == from);
    *entry = to;
}

bool
Instance::ensureProfilingState(JSContext* cx, bool newProfilingEnabled)
{
    if (code_->profilingEnabled() == newProfilingEnabled)
        return true;

    if (!code_->ensureProfilingState(cx, newProfilingEnabled))
        return false;

    // Imported wasm functions and typed function tables point directly to
    // either the profiling or non-profiling prologue and must therefore be
    // updated when the profiling mode is toggled.

    for (const FuncImport& fi : metadata().funcImports) {
        FuncImportTls& import = funcImportTls(fi);
        if (import.obj && import.obj->is<WasmInstanceObject>()) {
            Code& code = import.obj->as<WasmInstanceObject>().instance().code();
            UpdateEntry(code, newProfilingEnabled, &import.code);
        }
    }

    for (const SharedTable& table : tables_) {
        if (!table->isTypedFunction())
            continue;

        // This logic will have to be generalized to match the import logic
        // above if wasm can create typed function tables since a single table
        // can contain elements from multiple instances.
        MOZ_ASSERT(metadata().kind == ModuleKind::AsmJS);

        void** array = table->internalArray();
        uint32_t length = table->length();
        for (size_t i = 0; i < length; i++) {
            if (array[i])
                UpdateEntry(*code_, newProfilingEnabled, &array[i]);
        }
    }

    return true;
}

void
Instance::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                        Metadata::SeenSet* seenMetadata,
                        ShareableBytes::SeenSet* seenBytes,
                        Table::SeenSet* seenTables,
                        size_t* code,
                        size_t* data) const
{
    *data += mallocSizeOf(this);

    code_->addSizeOfMisc(mallocSizeOf, seenMetadata, seenBytes, code, data);

    for (const SharedTable& table : tables_)
         *data += table->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenTables);
}

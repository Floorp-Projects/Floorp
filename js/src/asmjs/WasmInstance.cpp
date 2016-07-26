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

#include "asmjs/WasmInstance.h"

#include "mozilla/BinarySearch.h"

#include "jsprf.h"

#include "asmjs/WasmBinaryToExperimentalText.h"
#include "asmjs/WasmJS.h"
#include "asmjs/WasmModule.h"
#include "builtin/SIMD.h"
#include "jit/BaselineJIT.h"
#include "jit/JitCommon.h"
#include "jit/JitCompartment.h"
#include "vm/StringBuffer.h"

#include "jsobjinlines.h"

#include "vm/ArrayBufferObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::BinarySearch;
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

ExclusiveData<SigIdSet> sigIdSet;

uint8_t**
Instance::addressOfMemoryBase() const
{
    return (uint8_t**)(codeSegment_->globalData() + HeapGlobalDataOffset);
}

void**
Instance::addressOfTableBase(size_t tableIndex) const
{
    MOZ_ASSERT(metadata_->tables[tableIndex].globalDataOffset >= InitialGlobalDataBytes);
    return (void**)(codeSegment_->globalData() + metadata_->tables[tableIndex].globalDataOffset);
}

const void**
Instance::addressOfSigId(const SigIdDesc& sigId) const
{
    MOZ_ASSERT(sigId.globalDataOffset() >= InitialGlobalDataBytes);
    return (const void**)(codeSegment_->globalData() + sigId.globalDataOffset());
}

FuncImportExit&
Instance::funcImportToExit(const FuncImport& fi)
{
    MOZ_ASSERT(fi.exitGlobalDataOffset() >= InitialGlobalDataBytes);
    return *(FuncImportExit*)(codeSegment_->globalData() + fi.exitGlobalDataOffset());
}

WasmActivation*&
Instance::activation()
{
    return *(WasmActivation**)(codeSegment_->globalData() + ActivationGlobalDataOffset);
}

bool
Instance::toggleProfiling(JSContext* cx)
{
    profilingEnabled_ = !profilingEnabled_;

    {
        AutoWritableJitCode awjc(cx->runtime(), codeSegment_->code(), codeSegment_->codeLength());
        AutoFlushICache afc("Instance::toggleProfiling");
        AutoFlushICache::setRange(uintptr_t(codeSegment_->code()), codeSegment_->codeLength());

        for (const CallSite& callSite : metadata_->callSites)
            ToggleProfiling(*this, callSite, profilingEnabled_);
        for (const CallThunk& callThunk : metadata_->callThunks)
            ToggleProfiling(*this, callThunk, profilingEnabled_);
        for (const CodeRange& codeRange : metadata_->codeRanges)
            ToggleProfiling(*this, codeRange, profilingEnabled_);
    }

    // When enabled, generate profiling labels for every name in funcNames_
    // that is the name of some Function CodeRange. This involves malloc() so
    // do it now since, once we start sampling, we'll be in a signal-handing
    // context where we cannot malloc.
    if (profilingEnabled_) {
        for (const CodeRange& codeRange : metadata_->codeRanges) {
            if (!codeRange.isFunction())
                continue;

            TwoByteName name(cx);
            if (!getFuncName(cx, codeRange.funcIndex(), &name))
                return false;
            if (!name.append('\0'))
                return false;

            UniqueChars label(JS_smprintf("%hs (%s:%u)",
                                          name.begin(),
                                          metadata_->filename.get(),
                                          codeRange.funcLineOrBytecode()));
            if (!label) {
                ReportOutOfMemory(cx);
                return false;
            }

            if (codeRange.funcIndex() >= funcLabels_.length()) {
                if (!funcLabels_.resize(codeRange.funcIndex() + 1))
                    return false;
            }
            funcLabels_[codeRange.funcIndex()] = Move(label);
        }
    } else {
        funcLabels_.clear();
    }

    // Typed-function tables' elements point directly to either the profiling or
    // non-profiling prologue and must therefore be updated when the profiling
    // mode is toggled.

    for (const SharedTable& table : tables_) {
        if (!table->isTypedFunction())
            continue;

        void** array = table->array();
        uint32_t length = table->length();
        for (size_t i = 0; i < length; i++) {
            const CodeRange* codeRange = lookupCodeRange(array[i]);
            void* from = codeSegment_->code() + codeRange->funcNonProfilingEntry();
            void* to = codeSegment_->code() + codeRange->funcProfilingEntry();
            if (!profilingEnabled_)
                Swap(from, to);
            MOZ_ASSERT(array[i] == from);
            array[i] = to;
        }
    }

    return true;
}

bool
Instance::callImport(JSContext* cx, uint32_t funcImportIndex, unsigned argc, const uint64_t* argv,
                     MutableHandleValue rval)
{
    const FuncImport& fi = metadata_->funcImports[funcImportIndex];

    InvokeArgs args(cx);
    if (!args.init(argc))
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
            MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
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
          case ValType::Limit:
            MOZ_CRASH("unhandled type in callImport");
        }
    }

    FuncImportExit& exit = funcImportToExit(fi);
    RootedValue fval(cx, ObjectValue(*exit.fun));
    RootedValue thisv(cx, UndefinedValue());
    if (!Call(cx, fval, thisv, args, rval))
        return false;

    // Don't try to optimize if the function has at least one i64 arg or if
    // it returns an int64. GenerateJitExit relies on this, as does the
    // type inference code below in this function.
    if (hasI64Arg || fi.sig().ret() == ExprType::I64)
        return true;

    // The exit may already have become optimized.
    void* jitExitCode = codeSegment_->code() + fi.jitExitCodeOffset();
    if (exit.code == jitExitCode)
        return true;

    // Test if the function is JIT compiled.
    if (!exit.fun->hasScript())
        return true;

    JSScript* script = exit.fun->nonLazyScript();
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
    if (exit.fun->nargs() > fi.sig().args().length())
        return true;

    // Ensure the argument types are included in the argument TypeSets stored in
    // the TypeScript. This is necessary for Ion, because the import exit will
    // use the skip-arg-checks entry point.
    //
    // Note that the TypeScript is never discarded while the script has a
    // BaselineScript, so if those checks hold now they must hold at least until
    // the BaselineScript is discarded and when that happens the import exit is
    // patched back.
    if (!TypeScript::ThisTypes(script)->hasType(TypeSet::UndefinedType()))
        return true;
    for (uint32_t i = 0; i < exit.fun->nargs(); i++) {
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
          case ValType::Limit: MOZ_CRASH("Limit");
        }
        if (!TypeScript::ArgTypes(script, i)->hasType(type))
            return true;
    }

    // Let's optimize it!
    if (!script->baselineScript()->addDependentWasmImport(cx, *this, funcImportIndex))
        return false;

    exit.code = jitExitCode;
    exit.baselineScript = script->baselineScript();
    return true;
}

/* static */ int32_t
Instance::callImport_void(int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    return activation->instance().callImport(cx, funcImportIndex, argc, argv, &rval);
}

/* static */ int32_t
Instance::callImport_i32(int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->instance().callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;

    return ToInt32(cx, rval, (int32_t*)argv);
}

/* static */ int32_t
Instance::callImport_i64(int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->instance().callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;

    return ReadI64Object(cx, rval, (int64_t*)argv);
}

/* static */ int32_t
Instance::callImport_f64(int32_t funcImportIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->instance().callImport(cx, funcImportIndex, argc, argv, &rval))
        return false;

    return ToNumber(cx, rval, (double*)argv);
}

Instance::Instance(JSContext* cx,
                   UniqueCodeSegment codeSegment,
                   const Metadata& metadata,
                   const ShareableBytes* maybeBytecode,
                   HandleWasmMemoryObject memory,
                   SharedTableVector&& tables,
                   Handle<FunctionVector> funcImports,
                   const ValVector& globalImports)
  : codeSegment_(Move(codeSegment)),
    metadata_(&metadata),
    maybeBytecode_(maybeBytecode),
    memory_(memory),
    tables_(Move(tables)),
    profilingEnabled_(false)
{
    MOZ_ASSERT(funcImports.length() == metadata.funcImports.length());
    MOZ_ASSERT(tables_.length() == metadata.tables.length());

    for (size_t i = 0; i < metadata.funcImports.length(); i++) {
        const FuncImport& fi = metadata.funcImports[i];
        FuncImportExit& exit = funcImportToExit(fi);
        exit.code = codeSegment_->code() + fi.interpExitCodeOffset();
        exit.fun = funcImports[i];
        exit.baselineScript = nullptr;
    }

    uint8_t* globalData = codeSegment_->globalData();

    for (size_t i = 0; i < metadata.globals.length(); i++) {
        const GlobalDesc& global = metadata.globals[i];
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
                const GlobalDesc& imported = metadata.globals[init.globalIndex()];
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

    if (memory)
        *addressOfMemoryBase() = memory->buffer().dataPointerEither().unwrap();

    for (size_t i = 0; i < tables_.length(); i++)
        *addressOfTableBase(i) = tables_[i]->array();

   updateStackLimit(cx);
}

bool
Instance::init(JSContext* cx)
{
    if (!metadata_->sigIds.empty()) {
        ExclusiveData<SigIdSet>::Guard lockedSigIdSet = sigIdSet.lock();

        if (!lockedSigIdSet->ensureInitialized(cx))
            return false;

        for (const SigWithId& sig : metadata_->sigIds) {
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
    for (unsigned i = 0; i < metadata_->funcImports.length(); i++) {
        FuncImportExit& exit = funcImportToExit(metadata_->funcImports[i]);
        if (exit.baselineScript)
            exit.baselineScript->removeDependentWasmImport(*this, i);
    }

    if (!metadata_->sigIds.empty()) {
        ExclusiveData<SigIdSet>::Guard lockedSigIdSet = sigIdSet.lock();

        for (const SigWithId& sig : metadata_->sigIds) {
            if (const void* sigId = *addressOfSigId(sig.id))
                lockedSigIdSet->deallocateSigId(sig, sigId);
        }
    }
}

void
Instance::trace(JSTracer* trc)
{
    for (const FuncImport& fi : metadata_->funcImports)
        TraceNullableEdge(trc, &funcImportToExit(fi).fun, "wasm function import");
    TraceNullableEdge(trc, &memory_, "wasm buffer");
}

SharedMem<uint8_t*>
Instance::memoryBase() const
{
    MOZ_ASSERT(metadata_->usesMemory());
    MOZ_ASSERT(*addressOfMemoryBase() == memory_->buffer().dataPointerEither());
    return memory_->buffer().dataPointerEither();
}

size_t
Instance::memoryLength() const
{
    return memory_->buffer().byteLength();
}

void
Instance::updateStackLimit(JSContext* cx)
{
    // Capture the stack limit for cx's thread.
    tlsData_.stackLimit = *(void**)cx->stackLimitAddressForJitCode(StackForUntrustedScript);
}

bool
Instance::callExport(JSContext* cx, uint32_t funcIndex, CallArgs args)
{
    const FuncExport& func = metadata_->lookupFuncExport(funcIndex);

    // Enable/disable profiling in the Module to match the current global
    // profiling state. Don't do this if the Module is already active on the
    // stack since this would leave the Module in a state where profiling is
    // enabled but the stack isn't unwindable.
    if (profilingEnabled() != cx->runtime()->spsProfiler.enabled() && !activation()) {
        if (!toggleProfiling(cx))
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
            MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
            if (!ReadI64Object(cx, v, (int64_t*)&exportArgs[i]))
                return false;
            break;
          case ValType::F32:
            if (!RoundFloat32(cx, v, (float*)&exportArgs[i]))
                return false;
            break;
          case ValType::F64:
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
          case ValType::Limit:
            MOZ_CRASH("Limit");
        }
    }

    {
        // Push a WasmActivation to describe the wasm frames we're about to push
        // when running this module. Additionally, push a JitActivation so that
        // the optimized wasm-to-Ion FFI call path (which we want to be very
        // fast) can avoid doing so. The JitActivation is marked as inactive so
        // stack iteration will skip over it.
        WasmActivation activation(cx, *this);
        JitActivation jitActivation(cx, /* active */ false);

        // Call the per-exported-function trampoline created by GenerateEntry.
        auto funcPtr = JS_DATA_TO_FUNC_PTR(ExportFuncPtr, codeSegment_->code() + func.entryOffset());
        if (!CALL_GENERATED_3(funcPtr, exportArgs.begin(), codeSegment_->globalData(), tlsData()))
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
        MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
        retObj = CreateI64Object(cx, *(int64_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::F32:
        // The entry stub has converted the F32 into a double for us.
      case ExprType::F64:
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

const char experimentalWarning[] =
    "Temporary\n"
    ".--.      .--.   ____       .-'''-. ,---.    ,---.\n"
    "|  |_     |  | .'  __ `.   / _     \\|    \\  /    |\n"
    "| _( )_   |  |/   '  \\  \\ (`' )/`--'|  ,  \\/  ,  |\n"
    "|(_ o _)  |  ||___|  /  |(_ o _).   |  |\\_   /|  |\n"
    "| (_,_) \\ |  |   _.-`   | (_,_). '. |  _( )_/ |  |\n"
    "|  |/    \\|  |.'   _    |.---.  \\  :| (_ o _) |  |\n"
    "|  '  /\\  `  ||  _( )_  |\\    `-'  ||  (_,_)  |  |\n"
    "|    /  \\    |\\ (_ o _) / \\       / |  |      |  |\n"
    "`---'    `---` '.(_,_).'   `-...-'  '--'      '--'\n"
    "text support (Work In Progress):\n\n";

const size_t experimentalWarningLinesCount = 12;

const char enabledMessage[] =
    "Restart with developer tools open to view WebAssembly source";

JSString*
Instance::createText(JSContext* cx)
{
    StringBuffer buffer(cx);
    if (maybeBytecode_) {
        const Bytes& bytes = maybeBytecode_->bytes;
        if (!buffer.append(experimentalWarning))
            return nullptr;
        maybeSourceMap_.reset(cx->runtime()->new_<GeneratedSourceMap>(cx));
        if (!maybeSourceMap_)
            return nullptr;
        if (!BinaryToExperimentalText(cx, bytes.begin(), bytes.length(), buffer,
                                      ExperimentalTextFormatting(), maybeSourceMap_.get()))
            return nullptr;
#if DEBUG
        // Checking source map invariant: expression and function locations must be sorted
        // by line number.
        uint32_t lastLineno = 0;
        for (size_t i = 0; i < maybeSourceMap_->exprlocs().length(); i++) {
            MOZ_ASSERT(lastLineno <= maybeSourceMap_->exprlocs()[i].lineno);
            lastLineno = maybeSourceMap_->exprlocs()[i].lineno;
        }
        lastLineno = 0;
        for (size_t i = 0; i < maybeSourceMap_->functionlocs().length(); i++) {
            MOZ_ASSERT(lastLineno <= maybeSourceMap_->functionlocs()[i].startLineno &&
                maybeSourceMap_->functionlocs()[i].startLineno <=
                  maybeSourceMap_->functionlocs()[i].endLineno);
            lastLineno = maybeSourceMap_->functionlocs()[i].endLineno + 1;
        }
#endif
    } else {
        if (!buffer.append(enabledMessage))
            return nullptr;
    }
    return buffer.finishString();
}

struct GeneratedSourceMapLinenoComparator
{
    int operator()(const ExprLoc& loc) const {
        return lineno == loc.lineno ? 0 : lineno < loc.lineno ? -1 : 1;
    }
    explicit GeneratedSourceMapLinenoComparator(uint32_t lineno_) : lineno(lineno_) {}
    const uint32_t lineno;
};

bool
Instance::getLineOffsets(size_t lineno, Vector<uint32_t>& offsets)
{
    // TODO Ensure text was generated?
    if (!maybeSourceMap_)
        return false;

    if (lineno < experimentalWarningLinesCount)
        return true;
    lineno -= experimentalWarningLinesCount;

    ExprLocVector& exprlocs = maybeSourceMap_->exprlocs();

    // Binary search for the expression with the specified line number and
    // rewind to the first expression, if more than one expression on the same line.
    size_t match;
    if (!BinarySearchIf(exprlocs, 0, exprlocs.length(),
                        GeneratedSourceMapLinenoComparator(lineno), &match))
        return true;
    while (match > 0 && exprlocs[match - 1].lineno == lineno)
        match--;
    // Returning all expression offsets that were printed on the specified line.
    for (size_t i = match; i < exprlocs.length() && exprlocs[i].lineno == lineno; i++) {
        if (!offsets.append(exprlocs[i].offset))
            return false;
    }
    return true;
}

bool
Instance::getFuncName(JSContext* cx, uint32_t funcIndex, TwoByteName* name) const
{
    const Bytes* maybeBytecode = maybeBytecode_ ? &maybeBytecode_.get()->bytes : nullptr;
    return metadata_->getFuncName(cx, maybeBytecode, funcIndex, name);
}

JSAtom*
Instance::getFuncAtom(JSContext* cx, uint32_t funcIndex) const
{
    TwoByteName name(cx);
    if (!getFuncName(cx, funcIndex, &name))
        return nullptr;

    return AtomizeChars(cx, name.begin(), name.length());
}

void
Instance::deoptimizeImportExit(uint32_t funcImportIndex)
{
    const FuncImport& fi = metadata_->funcImports[funcImportIndex];
    FuncImportExit& exit = funcImportToExit(fi);
    exit.code = codeSegment_->code() + fi.interpExitCodeOffset();
    exit.baselineScript = nullptr;
}

struct CallSiteRetAddrOffset
{
    const CallSiteVector& callSites;
    explicit CallSiteRetAddrOffset(const CallSiteVector& callSites) : callSites(callSites) {}
    uint32_t operator[](size_t index) const {
        return callSites[index].returnAddressOffset();
    }
};

const CallSite*
Instance::lookupCallSite(void* returnAddress) const
{
    uint32_t target = ((uint8_t*)returnAddress) - codeSegment_->code();
    size_t lowerBound = 0;
    size_t upperBound = metadata_->callSites.length();

    size_t match;
    if (!BinarySearch(CallSiteRetAddrOffset(metadata_->callSites), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->callSites[match];
}

const CodeRange*
Instance::lookupCodeRange(void* pc) const
{
    CodeRange::PC target((uint8_t*)pc - codeSegment_->code());
    size_t lowerBound = 0;
    size_t upperBound = metadata_->codeRanges.length();

    size_t match;
    if (!BinarySearch(metadata_->codeRanges, lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->codeRanges[match];
}

#ifdef ASMJS_MAY_USE_SIGNAL_HANDLERS
struct MemoryAccessOffset
{
    const MemoryAccessVector& accesses;
    explicit MemoryAccessOffset(const MemoryAccessVector& accesses) : accesses(accesses) {}
    uintptr_t operator[](size_t index) const {
        return accesses[index].insnOffset();
    }
};

const MemoryAccess*
Instance::lookupMemoryAccess(void* pc) const
{
    MOZ_ASSERT(codeSegment_->containsFunctionPC(pc));

    uint32_t target = ((uint8_t*)pc) - codeSegment_->code();
    size_t lowerBound = 0;
    size_t upperBound = metadata_->memoryAccesses.length();

    size_t match;
    if (!BinarySearch(MemoryAccessOffset(metadata_->memoryAccesses), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->memoryAccesses[match];
}
#endif // ASMJS_MAY_USE_SIGNAL_HANDLERS_FOR_OOB

void
Instance::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                        Metadata::SeenSet* seenMetadata,
                        ShareableBytes::SeenSet* seenBytes,
                        Table::SeenSet* seenTables,
                        size_t* code,
                        size_t* data) const
{
    *code += codeSegment_->codeLength();
    *data += mallocSizeOf(this) +
             codeSegment_->globalDataLength() +
             metadata_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenMetadata);

    for (const SharedTable& table : tables_)
         *data += table->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenTables);

    if (maybeBytecode_)
        *data += maybeBytecode_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenBytes);
}

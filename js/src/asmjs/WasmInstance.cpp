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
#include "jit/JitCommon.h"
#include "jit/JitOptions.h"
#include "vm/StringBuffer.h"

#include "jsatominlines.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/Debugger-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::BinarySearch;
using mozilla::Swap;

uint8_t**
Instance::addressOfHeapPtr() const
{
    return (uint8_t**)(codeSegment_->globalData() + HeapGlobalDataOffset);
}

ImportExit&
Instance::importToExit(const Import& import)
{
    return *(ImportExit*)(codeSegment_->globalData() + import.exitGlobalDataOffset());
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

            UniqueChars owner;
            const char* funcName = metadata_->getFuncName(cx, codeRange.funcIndex(), &owner);
            if (!funcName)
                return false;

            UniqueChars label(JS_smprintf("%s (%s:%u)",
                                          funcName,
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

    // Homogeneous table elements point directly to the prologue and must be
    // updated to reflect the profiling mode. In wasm, table elements point to
    // the (one) table entry which checks signature before jumping to the
    // appropriate prologue (which is patched by ToggleProfiling).
    for (const TypedFuncTable& table : typedFuncTables_) {
        auto array = reinterpret_cast<void**>(codeSegment_->globalData() + table.globalDataOffset);
        for (size_t i = 0; i < table.numElems; i++) {
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

static bool
ReadI64Object(JSContext* cx, HandleValue v, int64_t* i64)
{
    if (!v.isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_FAIL,
                             "i64 JS value must be an object");
        return false;
    }

    RootedObject obj(cx, &v.toObject());

    int32_t* i32 = (int32_t*)i64;

    RootedValue val(cx);
    if (!JS_GetProperty(cx, obj, "low", &val))
        return false;
    if (!ToInt32(cx, val, &i32[0]))
        return false;

    if (!JS_GetProperty(cx, obj, "high", &val))
        return false;
    if (!ToInt32(cx, val, &i32[1]))
        return false;

    return true;
}

static JSObject*
CreateI64Object(JSContext* cx, int64_t i64)
{
    RootedObject result(cx, JS_NewPlainObject(cx));
    if (!result)
        return nullptr;

    RootedValue val(cx, Int32Value(uint32_t(i64)));
    if (!JS_DefineProperty(cx, result, "low", val, JSPROP_ENUMERATE))
        return nullptr;

    val = Int32Value(uint32_t(i64 >> 32));
    if (!JS_DefineProperty(cx, result, "high", val, JSPROP_ENUMERATE))
        return nullptr;

    return result;
}

bool
Instance::callImport(JSContext* cx, uint32_t importIndex, unsigned argc, const uint64_t* argv,
                     MutableHandleValue rval)
{
    const Import& import = metadata_->imports[importIndex];

    InvokeArgs args(cx);
    if (!args.init(argc))
        return false;

    bool hasI64Arg = false;
    MOZ_ASSERT(import.sig().args().length() == argc);
    for (size_t i = 0; i < argc; i++) {
        switch (import.sig().args()[i]) {
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

    RootedValue fval(cx, ObjectValue(*importToExit(import).fun));
    RootedValue thisv(cx, UndefinedValue());
    if (!Call(cx, fval, thisv, args, rval))
        return false;

    // Don't try to optimize if the function has at least one i64 arg or if
    // it returns an int64. GenerateJitExit relies on this, as does the
    // type inference code below in this function.
    if (hasI64Arg || import.sig().ret() == ExprType::I64)
        return true;

    ImportExit& exit = importToExit(import);

    // The exit may already have become optimized.
    void* jitExitCode = codeSegment_->code() + import.jitExitCodeOffset();
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
    if (exit.fun->nargs() > import.sig().args().length())
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
        switch (import.sig().args()[i]) {
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
    if (!script->baselineScript()->addDependentWasmImport(cx, *this, importIndex))
        return false;

    exit.code = jitExitCode;
    exit.baselineScript = script->baselineScript();
    return true;
}

/* static */ int32_t
Instance::callImport_void(int32_t importIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    return activation->instance().callImport(cx, importIndex, argc, argv, &rval);
}

/* static */ int32_t
Instance::callImport_i32(int32_t importIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->instance().callImport(cx, importIndex, argc, argv, &rval))
        return false;

    return ToInt32(cx, rval, (int32_t*)argv);
}

/* static */ int32_t
Instance::callImport_i64(int32_t importIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->instance().callImport(cx, importIndex, argc, argv, &rval))
        return false;

    return ReadI64Object(cx, rval, (int64_t*)argv);
}

/* static */ int32_t
Instance::callImport_f64(int32_t importIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->instance().callImport(cx, importIndex, argc, argv, &rval))
        return false;

    return ToNumber(cx, rval, (double*)argv);
}

static bool
WasmCall(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedFunction callee(cx, &args.callee().as<JSFunction>());

    Instance& instance = ExportedFunctionToInstance(callee);
    uint32_t exportIndex = ExportedFunctionToExportIndex(callee);

    return instance.callExport(cx, exportIndex, args);
}

static JSFunction*
NewExportedFunction(JSContext* cx, Handle<WasmInstanceObject*> instanceObj, uint32_t exportIndex)
{
    Instance& instance = instanceObj->instance();
    const Metadata& metadata = instance.metadata();
    const Export& exp = metadata.exports[exportIndex];
    unsigned numArgs = exp.sig().args().length();

    RootedAtom name(cx, metadata.getFuncAtom(cx, exp.funcIndex()));
    if (!name)
        return nullptr;

    JSFunction* fun = NewNativeConstructor(cx, WasmCall, numArgs, name,
                                           gc::AllocKind::FUNCTION_EXTENDED, GenericObject,
                                           JSFunction::ASMJS_CTOR);
    if (!fun)
        return nullptr;

    fun->setExtendedSlot(FunctionExtended::WASM_MODULE_SLOT, ObjectValue(*instanceObj));
    fun->setExtendedSlot(FunctionExtended::WASM_EXPORT_INDEX_SLOT, Int32Value(exportIndex));
    return fun;
}

static bool
CreateExportObject(JSContext* cx,
                   Handle<WasmInstanceObject*> instanceObj,
                   Handle<ArrayBufferObjectMaybeShared*> heap,
                   const ExportMap& exportMap,
                   const ExportVector& exports,
                   MutableHandleObject exportObj)
{
    MOZ_ASSERT(exportMap.fieldNames.length() == exportMap.fieldsToExports.length());

    for (size_t fieldIndex = 0; fieldIndex < exportMap.fieldNames.length(); fieldIndex++) {
        const char* fieldName = exportMap.fieldNames[fieldIndex].get();
        if (!*fieldName) {
            MOZ_ASSERT(!exportObj);
            uint32_t exportIndex = exportMap.fieldsToExports[fieldIndex];
            if (exportIndex == MemoryExport) {
                MOZ_ASSERT(heap);
                exportObj.set(heap);
            } else {
                exportObj.set(NewExportedFunction(cx, instanceObj, exportIndex));
                if (!exportObj)
                    return false;
            }
            break;
        }
    }

    Rooted<ValueVector> vals(cx, ValueVector(cx));
    for (size_t exportIndex = 0; exportIndex < exports.length(); exportIndex++) {
        JSFunction* fun = NewExportedFunction(cx, instanceObj, exportIndex);
        if (!fun || !vals.append(ObjectValue(*fun)))
            return false;
    }

    if (!exportObj) {
        exportObj.set(JS_NewPlainObject(cx));
        if (!exportObj)
            return false;
    }

    for (size_t fieldIndex = 0; fieldIndex < exportMap.fieldNames.length(); fieldIndex++) {
        const char* fieldName = exportMap.fieldNames[fieldIndex].get();
        if (!*fieldName)
            continue;

        JSAtom* atom = AtomizeUTF8Chars(cx, fieldName, strlen(fieldName));
        if (!atom)
            return false;

        RootedId id(cx, AtomToId(atom));
        RootedValue val(cx);
        uint32_t exportIndex = exportMap.fieldsToExports[fieldIndex];
        if (exportIndex == MemoryExport)
            val = ObjectValue(*heap);
        else
            val = vals[exportIndex];

        if (!JS_DefinePropertyById(cx, exportObj, id, val, JSPROP_ENUMERATE))
            return false;
    }

    return true;
}

Instance::Instance(UniqueCodeSegment codeSegment,
                   const Metadata& metadata,
                   const ShareableBytes* maybeBytecode,
                   TypedFuncTableVector&& typedFuncTables,
                   HandleArrayBufferObjectMaybeShared heap)
  : codeSegment_(Move(codeSegment)),
    metadata_(&metadata),
    maybeBytecode_(maybeBytecode),
    typedFuncTables_(Move(typedFuncTables)),
    heap_(heap),
    profilingEnabled_(false)
{}

static const char ExportField[] = "exports";

/* static */ bool
Instance::create(JSContext* cx,
                 UniqueCodeSegment codeSegment,
                 const Metadata& metadata,
                 const ShareableBytes* maybeBytecode,
                 TypedFuncTableVector&& typedFuncTables,
                 HandleArrayBufferObjectMaybeShared heap,
                 Handle<FunctionVector> funcImports,
                 const ExportMap& exportMap,
                 MutableHandleWasmInstanceObject instanceObj)
{
    // Ensure that the Instance is traceable via WasmInstanceObject before any
    // GC can occur.

    instanceObj.set(WasmInstanceObject::create(cx));
    if (!instanceObj)
        return false;

    {
        auto instance = cx->make_unique<Instance>(Move(codeSegment), metadata, maybeBytecode,
                                                  Move(typedFuncTables), heap);
        if (!instance)
            return false;

        instanceObj->init(Move(instance));
    }

    // Initialize the instance

    Instance& instance = instanceObj->instance();

    for (size_t i = 0; i < metadata.imports.length(); i++) {
        const Import& import = metadata.imports[i];
        ImportExit& exit = instance.importToExit(import);
        exit.code = instance.codeSegment().code() + import.interpExitCodeOffset();
        exit.fun = funcImports[i];
        exit.baselineScript = nullptr;
    }

    if (heap)
        *instance.addressOfHeapPtr() = heap->dataPointerEither().unwrap(/* wasm heap pointer */);

    // Create the export object

    RootedObject exportObj(cx);
    if (!CreateExportObject(cx, instanceObj, heap, exportMap, metadata.exports, &exportObj))
        return false;

    // Attach the export object to the instance object

    instanceObj->initExportsObject(exportObj);

    JSAtom* atom = Atomize(cx, ExportField, strlen(ExportField));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));

    RootedValue val(cx, ObjectValue(*exportObj));
    if (!JS_DefinePropertyById(cx, instanceObj, id, val, JSPROP_ENUMERATE))
        return false;

    // Notify the Debugger of the new Instance

    Debugger::onNewWasmInstance(cx, instanceObj);

    return true;
}

Instance::~Instance()
{
    for (unsigned i = 0; i < metadata_->imports.length(); i++) {
        ImportExit& exit = importToExit(metadata_->imports[i]);
        if (exit.baselineScript)
            exit.baselineScript->removeDependentWasmImport(*this, i);
    }
}

void
Instance::trace(JSTracer* trc)
{
    for (const Import& import : metadata_->imports)
        TraceNullableEdge(trc, &importToExit(import).fun, "wasm function import");
    TraceNullableEdge(trc, &heap_, "wasm buffer");
}

SharedMem<uint8_t*>
Instance::heap() const
{
    MOZ_ASSERT(metadata_->usesHeap());
    MOZ_ASSERT(*addressOfHeapPtr() == heap_->dataPointerEither());
    return heap_->dataPointerEither();
}

size_t
Instance::heapLength() const
{
    return heap_->byteLength();
}

bool
Instance::callExport(JSContext* cx, uint32_t exportIndex, CallArgs args)
{
    const Export& exp = metadata_->exports[exportIndex];

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
    if (!exportArgs.resize(Max<size_t>(1, exp.sig().args().length())))
        return false;

    RootedValue v(cx);
    for (unsigned i = 0; i < exp.sig().args().length(); ++i) {
        v = i < args.length() ? args[i] : UndefinedValue();
        switch (exp.sig().arg(i)) {
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
        auto funcPtr = JS_DATA_TO_FUNC_PTR(ExportFuncPtr, codeSegment_->code() + exp.stubOffset());
        if (!CALL_GENERATED_2(funcPtr, exportArgs.begin(), codeSegment_->globalData()))
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
    switch (exp.sig().ret()) {
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
        if (!BinaryToExperimentalText(cx, bytes.begin(), bytes.length(), buffer))
            return nullptr;
    } else {
        if (!buffer.append(enabledMessage))
            return nullptr;
    }
    return buffer.finishString();
}

void
Instance::deoptimizeImportExit(uint32_t importIndex)
{
    const Import& import = metadata_->imports[importIndex];
    ImportExit& exit = importToExit(import);
    exit.code = codeSegment_->code() + import.interpExitCodeOffset();
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

struct HeapAccessOffset
{
    const HeapAccessVector& accesses;
    explicit HeapAccessOffset(const HeapAccessVector& accesses) : accesses(accesses) {}
    uintptr_t operator[](size_t index) const {
        return accesses[index].insnOffset();
    }
};

const HeapAccess*
Instance::lookupHeapAccess(void* pc) const
{
    MOZ_ASSERT(codeSegment_->containsFunctionPC(pc));

    uint32_t target = ((uint8_t*)pc) - codeSegment_->code();
    size_t lowerBound = 0;
    size_t upperBound = metadata_->heapAccesses.length();

    size_t match;
    if (!BinarySearch(HeapAccessOffset(metadata_->heapAccesses), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->heapAccesses[match];
}

void
Instance::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                        Metadata::SeenSet* seenMetadata,
                        ShareableBytes::SeenSet* seenBytes,
                        size_t* code, size_t* data) const
{
    *code += codeSegment_->codeLength();
    *data += mallocSizeOf(this) +
             codeSegment_->globalDataLength() +
             metadata_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenMetadata) +
             typedFuncTables_.sizeOfExcludingThis(mallocSizeOf);

    if (maybeBytecode_)
        *data += maybeBytecode_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenBytes);
}

bool
wasm::IsExportedFunction(JSFunction* fun)
{
    return fun->maybeNative() == WasmCall;
}

Instance&
wasm::ExportedFunctionToInstance(JSFunction* fun)
{
    MOZ_ASSERT(IsExportedFunction(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_MODULE_SLOT);
    return v.toObject().as<WasmInstanceObject>().instance();
}

uint32_t
wasm::ExportedFunctionToExportIndex(JSFunction* fun)
{
    MOZ_ASSERT(IsExportedFunction(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_EXPORT_INDEX_SLOT);
    return v.toInt32();
}

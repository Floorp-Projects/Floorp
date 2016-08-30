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

#include "asmjs/WasmJS.h"

#include "mozilla/Maybe.h"

#include "asmjs/WasmCompile.h"
#include "asmjs/WasmInstance.h"
#include "asmjs/WasmModule.h"
#include "asmjs/WasmSignalHandlers.h"
#include "builtin/Promise.h"
#include "jit/JitOptions.h"
#include "vm/Interpreter.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::Nothing;

bool
wasm::HasCompilerSupport(ExclusiveContext* cx)
{
    if (!cx->jitSupportsFloatingPoint())
        return false;

    if (!cx->jitSupportsUnalignedAccesses())
        return false;

    if (!wasm::HaveSignalHandlers())
        return false;

#if defined(JS_CODEGEN_NONE) || defined(JS_CODEGEN_ARM64)
    return false;
#else
    return true;
#endif
}

static bool
CheckCompilerSupport(JSContext* cx)
{
    if (!HasCompilerSupport(cx)) {
#ifdef JS_MORE_DETERMINISTIC
        fprintf(stderr, "WebAssembly is not supported on the current device.\n");
#endif
        JS_ReportError(cx, "WebAssembly is not supported on the current device.");
        return false;
    }

    return true;
}

// ============================================================================
// (Temporary) Wasm class and static methods

static bool
Throw(JSContext* cx, const char* str)
{
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_FAIL, str);
    return false;
}

static bool
Throw(JSContext* cx, unsigned errorNumber, const char* str)
{
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, errorNumber, str);
    return false;
}

static bool
GetProperty(JSContext* cx, HandleObject obj, const char* chars, MutableHandleValue v)
{
    JSAtom* atom = AtomizeUTF8Chars(cx, chars, strlen(chars));
    if (!atom)
        return false;

    RootedId id(cx, AtomToId(atom));
    return GetProperty(cx, obj, obj, id, v);
}

static bool
GetImports(JSContext* cx,
           const Module& module,
           HandleObject importObj,
           MutableHandle<FunctionVector> funcImports,
           MutableHandleWasmTableObject tableImport,
           MutableHandleWasmMemoryObject memoryImport,
           ValVector* globalImports)
{
    const ImportVector& imports = module.imports();
    if (!imports.empty() && !importObj)
        return Throw(cx, "no import object given");

    const Metadata& metadata = module.metadata();

    uint32_t globalIndex = 0;
    const GlobalDescVector& globals = metadata.globals;
    for (const Import& import : imports) {
        RootedValue v(cx);
        if (!GetProperty(cx, importObj, import.module.get(), &v))
            return false;

        if (strlen(import.func.get()) > 0) {
            if (!v.isObject())
                return Throw(cx, JSMSG_WASM_BAD_IMPORT_FIELD, "an Object");

            RootedObject obj(cx, &v.toObject());
            if (!GetProperty(cx, obj, import.func.get(), &v))
                return false;
        }

        switch (import.kind) {
          case DefinitionKind::Function:
            if (!IsFunctionObject(v))
                return Throw(cx, JSMSG_WASM_BAD_IMPORT_FIELD, "a Function");

            if (!funcImports.append(&v.toObject().as<JSFunction>()))
                return false;

            break;
          case DefinitionKind::Table:
            if (!v.isObject() || !v.toObject().is<WasmTableObject>())
                return Throw(cx, JSMSG_WASM_BAD_IMPORT_FIELD, "a Table");

            MOZ_ASSERT(!tableImport);
            tableImport.set(&v.toObject().as<WasmTableObject>());
            break;
          case DefinitionKind::Memory:
            if (!v.isObject() || !v.toObject().is<WasmMemoryObject>())
                return Throw(cx, JSMSG_WASM_BAD_IMPORT_FIELD, "a Memory");

            MOZ_ASSERT(!memoryImport);
            memoryImport.set(&v.toObject().as<WasmMemoryObject>());
            break;

          case DefinitionKind::Global:
            Val val;
            const GlobalDesc& global = globals[globalIndex++];
            MOZ_ASSERT(global.importIndex() == globalIndex - 1);
            MOZ_ASSERT(!global.isMutable());
            switch (global.type()) {
              case ValType::I32: {
                int32_t i32;
                if (!ToInt32(cx, v, &i32))
                    return false;
                val = Val(uint32_t(i32));
                break;
              }
              case ValType::I64: {
                MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in JS");
                int64_t i64;
                if (!ReadI64Object(cx, v, &i64))
                    return false;
                val = Val(uint64_t(i64));
                break;
              }
              case ValType::F32: {
                double d;
                if (!ToNumber(cx, v, &d))
                    return false;
                val = Val(float(d));
                break;
              }
              case ValType::F64: {
                double d;
                if (!ToNumber(cx, v, &d))
                    return false;
                val = Val(d);
                break;
              }
              default: {
                MOZ_CRASH("unexpected import value type");
              }
            }
            if (!globalImports->append(val))
                return false;
        }
    }

    MOZ_ASSERT(globalIndex == globals.length() || !globals[globalIndex].isImport());

    return true;
}

static bool
DescribeScriptedCaller(JSContext* cx, ScriptedCaller* scriptedCaller)
{
    // Note: JS::DescribeScriptedCaller returns whether a scripted caller was
    // found, not whether an error was thrown. This wrapper function converts
    // back to the more ordinary false-if-error form.

    JS::AutoFilename af;
    if (JS::DescribeScriptedCaller(cx, &af, &scriptedCaller->line, &scriptedCaller->column)) {
        scriptedCaller->filename = DuplicateString(cx, af.get());
        if (!scriptedCaller->filename)
            return false;
    }

    return true;
}

bool
wasm::Eval(JSContext* cx, Handle<TypedArrayObject*> code, HandleObject importObj,
           MutableHandleWasmInstanceObject instanceObj)
{
    if (!CheckCompilerSupport(cx))
        return false;

    MutableBytes bytecode = cx->new_<ShareableBytes>();
    if (!bytecode)
        return false;

    if (!bytecode->append((uint8_t*)code->viewDataEither().unwrap(), code->byteLength())) {
        ReportOutOfMemory(cx);
        return false;
    }

    ScriptedCaller scriptedCaller;
    if (!DescribeScriptedCaller(cx, &scriptedCaller))
        return false;

    CompileArgs compileArgs;
    if (!compileArgs.initFromContext(cx, Move(scriptedCaller)))
        return false;

    UniqueChars error;
    SharedModule module = Compile(*bytecode, compileArgs, &error);
    if (!module) {
        if (error)
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_FAIL, error.get());
        else
            ReportOutOfMemory(cx);
        return false;
    }

    Rooted<FunctionVector> funcs(cx, FunctionVector(cx));
    RootedWasmTableObject table(cx);
    RootedWasmMemoryObject memory(cx);
    ValVector globals;
    if (!GetImports(cx, *module, importObj, &funcs, &table, &memory, &globals))
        return false;

    return module->instantiate(cx, funcs, table, memory, globals, nullptr, instanceObj);
}

static bool
InstantiateModule(JSContext* cx, unsigned argc, Value* vp)
{
    MOZ_ASSERT(cx->options().wasm());
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.get(0).isObject() || !args.get(0).toObject().is<TypedArrayObject>()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_BUF_ARG);
        return false;
    }

    RootedObject importObj(cx);
    if (!args.get(1).isUndefined()) {
        if (!args.get(1).isObject()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMPORT_ARG);
            return false;
        }
        importObj = &args[1].toObject();
    }

    Rooted<TypedArrayObject*> code(cx, &args[0].toObject().as<TypedArrayObject>());
    RootedWasmInstanceObject instanceObj(cx);
    if (!Eval(cx, code, importObj, &instanceObj))
        return false;

    args.rval().setObject(*instanceObj);
    return true;
}

#if JS_HAS_TOSOURCE
static bool
wasm_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().Wasm);
    return true;
}
#endif

static const JSFunctionSpec wasm_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,     wasm_toSource,     0, 0),
#endif
    JS_FN("instantiateModule", InstantiateModule, 1, 0),
    JS_FS_END
};

const Class js::WasmClass = {
    js_Wasm_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Wasm)
};

JSObject*
js::InitWasmClass(JSContext* cx, HandleObject global)
{
    MOZ_ASSERT(cx->options().wasm());

    RootedObject proto(cx, global->as<GlobalObject>().getOrCreateObjectPrototype(cx));
    if (!proto)
        return nullptr;

    RootedObject Wasm(cx, NewObjectWithGivenProto(cx, &WasmClass, proto, SingletonObject));
    if (!Wasm)
        return nullptr;

    if (!JS_DefineProperty(cx, global, js_Wasm_str, Wasm, JSPROP_RESOLVING))
        return nullptr;

    RootedValue version(cx, Int32Value(EncodingVersion));
    if (!JS_DefineProperty(cx, Wasm, "experimentalVersion", version, JSPROP_RESOLVING))
        return nullptr;

    if (!JS_DefineFunctions(cx, Wasm, wasm_static_methods))
        return nullptr;

    global->as<GlobalObject>().setConstructor(JSProto_Wasm, ObjectValue(*Wasm));
    return Wasm;
}

// ============================================================================
// WebAssembly.Module class and methods

const ClassOps WasmModuleObject::classOps_ =
{
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    WasmModuleObject::finalize
};

const Class WasmModuleObject::class_ =
{
    "WebAssembly.Module",
    JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(WasmModuleObject::RESERVED_SLOTS) |
    JSCLASS_FOREGROUND_FINALIZE,
    &WasmModuleObject::classOps_,
};

const JSPropertySpec WasmModuleObject::properties[] =
{ JS_PS_END };

const JSFunctionSpec WasmModuleObject::methods[] =
{ JS_FS_END };

/* static */ void
WasmModuleObject::finalize(FreeOp* fop, JSObject* obj)
{
    obj->as<WasmModuleObject>().module().Release();
}

/* static */ WasmModuleObject*
WasmModuleObject::create(ExclusiveContext* cx, Module& module, HandleObject proto)
{
    AutoSetNewObjectMetadata metadata(cx);
    auto* obj = NewObjectWithGivenProto<WasmModuleObject>(cx, proto);
    if (!obj)
        return nullptr;

    obj->initReservedSlot(MODULE_SLOT, PrivateValue(&module));
    module.AddRef();
    return obj;
}

static bool
GetCompileArgs(JSContext* cx, CallArgs callArgs, const char* name, MutableBytes* bytecode,
               CompileArgs* compileArgs)
{
    if (!callArgs.requireAtLeast(cx, name, 1))
        return false;

    if (!callArgs[0].isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_BUF_ARG);
        return false;
    }

    *bytecode = cx->new_<ShareableBytes>();
    if (!*bytecode)
        return false;

    JSObject* unwrapped = CheckedUnwrap(&callArgs[0].toObject());
    if (unwrapped && unwrapped->is<TypedArrayObject>()) {
        TypedArrayObject& view = unwrapped->as<TypedArrayObject>();
        if (!(*bytecode)->append((uint8_t*)view.viewDataEither().unwrap(), view.byteLength()))
            return false;
    } else if (unwrapped && unwrapped->is<ArrayBufferObject>()) {
        ArrayBufferObject& buffer = unwrapped->as<ArrayBufferObject>();
        if (!(*bytecode)->append(buffer.dataPointer(), buffer.byteLength()))
            return false;
    } else {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_BUF_ARG);
        return false;
    }

    ScriptedCaller scriptedCaller;
    if (!DescribeScriptedCaller(cx, &scriptedCaller))
        return false;

    if (!compileArgs->initFromContext(cx, Move(scriptedCaller)))
        return false;

    compileArgs->assumptions.newFormat = true;

    return CheckCompilerSupport(cx);
}

/* static */ bool
WasmModuleObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs callArgs = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, callArgs, "Module"))
        return false;

    MutableBytes bytecode;
    CompileArgs compileArgs;
    if (!GetCompileArgs(cx, callArgs, "WebAssembly.Module", &bytecode, &compileArgs))
        return false;

    UniqueChars error;
    SharedModule module = Compile(*bytecode, compileArgs, &error);
    if (!module) {
        if (error)
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_FAIL, error.get());
        else
            ReportOutOfMemory(cx);
        return false;
    }

    RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
    RootedObject moduleObj(cx, WasmModuleObject::create(cx, *module, proto));
    if (!moduleObj)
        return false;

    callArgs.rval().setObject(*moduleObj);
    return true;
}

Module&
WasmModuleObject::module() const
{
    MOZ_ASSERT(is<WasmModuleObject>());
    return *(Module*)getReservedSlot(MODULE_SLOT).toPrivate();
}

// ============================================================================
// WebAssembly.Instance class and methods

const ClassOps WasmInstanceObject::classOps_ =
{
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    WasmInstanceObject::finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    WasmInstanceObject::trace
};

const Class WasmInstanceObject::class_ =
{
    "WebAssembly.Instance",
    JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(WasmInstanceObject::RESERVED_SLOTS) |
    JSCLASS_FOREGROUND_FINALIZE,
    &WasmInstanceObject::classOps_,
};

const JSPropertySpec WasmInstanceObject::properties[] =
{ JS_PS_END };

const JSFunctionSpec WasmInstanceObject::methods[] =
{ JS_FS_END };

bool
WasmInstanceObject::isNewborn() const
{
    MOZ_ASSERT(is<WasmInstanceObject>());
    return getReservedSlot(INSTANCE_SLOT).isUndefined();
}

/* static */ void
WasmInstanceObject::finalize(FreeOp* fop, JSObject* obj)
{
    fop->delete_(&obj->as<WasmInstanceObject>().exports());
    if (!obj->as<WasmInstanceObject>().isNewborn())
        fop->delete_(&obj->as<WasmInstanceObject>().instance());
}

/* static */ void
WasmInstanceObject::trace(JSTracer* trc, JSObject* obj)
{
    if (!obj->as<WasmInstanceObject>().isNewborn())
        obj->as<WasmInstanceObject>().instance().tracePrivate(trc);
}

/* static */ WasmInstanceObject*
WasmInstanceObject::create(JSContext* cx,
                           UniqueCode code,
                           HandleWasmMemoryObject memory,
                           SharedTableVector&& tables,
                           Handle<FunctionVector> funcImports,
                           const ValVector& globalImports,
                           HandleObject proto)
{
    UniquePtr<WeakExportMap> exports = js::MakeUnique<WeakExportMap>(cx->zone(), ExportMap());
    if (!exports || !exports->init()) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    AutoSetNewObjectMetadata metadata(cx);
    RootedWasmInstanceObject obj(cx, NewObjectWithGivenProto<WasmInstanceObject>(cx, proto));
    if (!obj)
        return nullptr;

    obj->setReservedSlot(EXPORTS_SLOT, PrivateValue(exports.release()));
    MOZ_ASSERT(obj->isNewborn());

    // Root the Instance via WasmInstanceObject before any possible GC.
    auto* instance = cx->new_<Instance>(cx,
                                        obj,
                                        Move(code),
                                        memory,
                                        Move(tables),
                                        funcImports,
                                        globalImports);
    if (!instance)
        return nullptr;

    obj->initReservedSlot(INSTANCE_SLOT, PrivateValue(instance));
    MOZ_ASSERT(!obj->isNewborn());

    if (!instance->init(cx))
        return nullptr;

    return obj;
}

/* static */ bool
WasmInstanceObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "Instance"))
        return false;

    if (!args.requireAtLeast(cx, "WebAssembly.Instance", 1))
        return false;

    if (!args.get(0).isObject() || !args[0].toObject().is<WasmModuleObject>()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_MOD_ARG);
        return false;
    }

    const Module& module = args[0].toObject().as<WasmModuleObject>().module();

    RootedObject importObj(cx);
    if (!args.get(1).isUndefined()) {
        if (!args[1].isObject()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMPORT_ARG);
            return false;
        }
        importObj = &args[1].toObject();
    }

    Rooted<FunctionVector> funcs(cx, FunctionVector(cx));
    RootedWasmTableObject table(cx);
    RootedWasmMemoryObject memory(cx);
    ValVector globals;
    if (!GetImports(cx, module, importObj, &funcs, &table, &memory, &globals))
        return false;

    RootedObject instanceProto(cx, &cx->global()->getPrototype(JSProto_WasmInstance).toObject());
    RootedWasmInstanceObject instanceObj(cx);
    if (!module.instantiate(cx, funcs, table, memory, globals, instanceProto, &instanceObj))
        return false;

    args.rval().setObject(*instanceObj);
    return true;
}

Instance&
WasmInstanceObject::instance() const
{
    MOZ_ASSERT(!isNewborn());
    return *(Instance*)getReservedSlot(INSTANCE_SLOT).toPrivate();
}

WasmInstanceObject::WeakExportMap&
WasmInstanceObject::exports() const
{
    return *(WeakExportMap*)getReservedSlot(EXPORTS_SLOT).toPrivate();
}

static bool
WasmCall(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedFunction callee(cx, &args.callee().as<JSFunction>());

    Instance& instance = ExportedFunctionToInstance(callee);
    uint32_t funcIndex = ExportedFunctionToIndex(callee);
    return instance.callExport(cx, funcIndex, args);
}

/* static */ bool
WasmInstanceObject::getExportedFunction(JSContext* cx, HandleWasmInstanceObject instanceObj,
                                        uint32_t funcIndex, MutableHandleFunction fun)
{
    if (ExportMap::Ptr p = instanceObj->exports().lookup(funcIndex)) {
        fun.set(p->value());
        return true;
    }

    const Instance& instance = instanceObj->instance();
    RootedAtom name(cx, instance.code().getFuncAtom(cx, funcIndex));
    if (!name)
        return false;

    unsigned numArgs = instance.metadata().lookupFuncExport(funcIndex).sig().args().length();
    fun.set(NewNativeConstructor(cx, WasmCall, numArgs, name, gc::AllocKind::FUNCTION_EXTENDED,
                                 GenericObject, JSFunction::ASMJS_CTOR));
    if (!fun)
        return false;

    fun->setExtendedSlot(FunctionExtended::WASM_INSTANCE_SLOT, ObjectValue(*instanceObj));
    fun->setExtendedSlot(FunctionExtended::WASM_FUNC_INDEX_SLOT, Int32Value(funcIndex));

    if (!instanceObj->exports().putNew(funcIndex, fun)) {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

bool
wasm::IsExportedFunction(JSFunction* fun)
{
    return fun->maybeNative() == WasmCall;
}

bool
wasm::IsExportedFunction(const Value& v, MutableHandleFunction f)
{
    if (!v.isObject())
        return false;

    JSObject& obj = v.toObject();
    if (!obj.is<JSFunction>() || !IsExportedFunction(&obj.as<JSFunction>()))
        return false;

    f.set(&obj.as<JSFunction>());
    return true;
}

Instance&
wasm::ExportedFunctionToInstance(JSFunction* fun)
{
    return ExportedFunctionToInstanceObject(fun)->instance();
}

WasmInstanceObject*
wasm::ExportedFunctionToInstanceObject(JSFunction* fun)
{
    MOZ_ASSERT(IsExportedFunction(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_INSTANCE_SLOT);
    return &v.toObject().as<WasmInstanceObject>();
}

uint32_t
wasm::ExportedFunctionToIndex(JSFunction* fun)
{
    MOZ_ASSERT(IsExportedFunction(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_FUNC_INDEX_SLOT);
    return v.toInt32();
}

// ============================================================================
// WebAssembly.Memory class and methods

const Class WasmMemoryObject::class_ =
{
    "WebAssembly.Memory",
    JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(WasmMemoryObject::RESERVED_SLOTS)
};

/* static */ WasmMemoryObject*
WasmMemoryObject::create(ExclusiveContext* cx, HandleArrayBufferObjectMaybeShared buffer,
                         HandleObject proto)
{
    AutoSetNewObjectMetadata metadata(cx);
    auto* obj = NewObjectWithGivenProto<WasmMemoryObject>(cx, proto);
    if (!obj)
        return nullptr;

    obj->initReservedSlot(BUFFER_SLOT, ObjectValue(*buffer));
    return obj;
}

/* static */ bool
WasmMemoryObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "Memory"))
        return false;

    if (!args.requireAtLeast(cx, "WebAssembly.Memory", 1))
        return false;

    if (!args.get(0).isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_DESC_ARG, "memory");
        return false;
    }

    JSAtom* initialAtom = Atomize(cx, "initial", strlen("initial"));
    if (!initialAtom)
        return false;
    RootedId initialId(cx, AtomToId(initialAtom));

    JSAtom* maximumAtom = Atomize(cx, "maximum", strlen("maximum"));
    if (!maximumAtom)
        return false;
    RootedId maximumId(cx, AtomToId(maximumAtom));

    RootedObject obj(cx, &args[0].toObject());
    RootedValue initialVal(cx);
    if (!GetProperty(cx, obj, obj, initialId, &initialVal))
        return false;

    double initialDbl;
    if (!ToInteger(cx, initialVal, &initialDbl))
        return false;

    if (initialDbl < 0 || initialDbl > INT32_MAX / PageSize) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_SIZE, "Memory", "initial");
        return false;
    }

    Maybe<uint32_t> maxSize;

    bool found;
    if (HasProperty(cx, obj, maximumId, &found) && found) {
        RootedValue maxVal(cx);
        if (!GetProperty(cx, obj, obj, maximumId, &maxVal))
            return false;

        double maxDbl;
        if (!ToInteger(cx, maxVal, &maxDbl))
            return false;

        if (maxDbl < initialDbl || maxDbl > UINT32_MAX / PageSize) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_SIZE, "Memory",
                                 "maximum");
            return false;
        }

        maxSize = Some<uint32_t>(uint32_t(maxDbl) * PageSize);
    }

    uint32_t initialSize = uint32_t(initialDbl) * PageSize;
    RootedArrayBufferObject buffer(cx, ArrayBufferObject::createForWasm(cx, initialSize, maxSize));
    if (!buffer)
        return false;

    RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmMemory).toObject());
    RootedWasmMemoryObject memoryObj(cx, WasmMemoryObject::create(cx, buffer, proto));
    if (!memoryObj)
        return false;

    args.rval().setObject(*memoryObj);
    return true;
}

static bool
IsMemory(HandleValue v)
{
    return v.isObject() && v.toObject().is<WasmMemoryObject>();
}

static bool
MemoryBufferGetterImpl(JSContext* cx, const CallArgs& args)
{
    args.rval().setObject(args.thisv().toObject().as<WasmMemoryObject>().buffer());
    return true;
}

static bool
MemoryBufferGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsMemory, MemoryBufferGetterImpl>(cx, args);
}

const JSPropertySpec WasmMemoryObject::properties[] =
{
    JS_PSG("buffer", MemoryBufferGetter, 0),
    JS_PS_END
};

const JSFunctionSpec WasmMemoryObject::methods[] =
{ JS_FS_END };

ArrayBufferObjectMaybeShared&
WasmMemoryObject::buffer() const
{
    return getReservedSlot(BUFFER_SLOT).toObject().as<ArrayBufferObjectMaybeShared>();
}

// ============================================================================
// WebAssembly.Table class and methods

const ClassOps WasmTableObject::classOps_ =
{
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    WasmTableObject::finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    WasmTableObject::trace
};

const Class WasmTableObject::class_ =
{
    "WebAssembly.Table",
    JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(WasmTableObject::RESERVED_SLOTS) |
    JSCLASS_FOREGROUND_FINALIZE,
    &WasmTableObject::classOps_
};

bool
WasmTableObject::isNewborn() const
{
    MOZ_ASSERT(is<WasmTableObject>());
    return getReservedSlot(TABLE_SLOT).isUndefined();
}

/* static */ void
WasmTableObject::finalize(FreeOp* fop, JSObject* obj)
{
    WasmTableObject& tableObj = obj->as<WasmTableObject>();
    if (!tableObj.isNewborn())
        tableObj.table().Release();
}

/* static */ void
WasmTableObject::trace(JSTracer* trc, JSObject* obj)
{
    WasmTableObject& tableObj = obj->as<WasmTableObject>();
    if (!tableObj.isNewborn())
        tableObj.table().tracePrivate(trc);
}

/* static */ WasmTableObject*
WasmTableObject::create(JSContext* cx, uint32_t length)
{
    RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmTable).toObject());

    AutoSetNewObjectMetadata metadata(cx);
    RootedWasmTableObject obj(cx, NewObjectWithGivenProto<WasmTableObject>(cx, proto));
    if (!obj)
        return nullptr;

    MOZ_ASSERT(obj->isNewborn());

    TableDesc desc;
    desc.kind = TableKind::AnyFunction;
    desc.external = true;
    desc.initial = length;
    desc.maximum = length;

    SharedTable table = Table::create(cx, desc, obj);
    if (!table)
        return nullptr;

    obj->initReservedSlot(TABLE_SLOT, PrivateValue(table.forget().take()));

    MOZ_ASSERT(!obj->isNewborn());
    return obj;
}

/* static */ bool
WasmTableObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "Table"))
        return false;

    if (!args.requireAtLeast(cx, "WebAssembly.Table", 1))
        return false;

    if (!args.get(0).isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_DESC_ARG, "table");
        return false;
    }

    RootedObject obj(cx, &args[0].toObject());
    RootedId id(cx);
    RootedValue val(cx);

    JSAtom* elementAtom = Atomize(cx, "element", strlen("element"));
    if (!elementAtom)
        return false;
    id = AtomToId(elementAtom);
    if (!GetProperty(cx, obj, obj, id, &val))
        return false;

    if (!val.isString()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_ELEMENT);
        return false;
    }

    JSLinearString* str = val.toString()->ensureLinear(cx);
    if (!str)
        return false;

    if (!StringEqualsAscii(str, "anyfunc")) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_ELEMENT);
        return false;
    }

    JSAtom* initialAtom = Atomize(cx, "initial", strlen("initial"));
    if (!initialAtom)
        return false;
    id = AtomToId(initialAtom);
    if (!GetProperty(cx, obj, obj, id, &val))
        return false;

    double initialDbl;
    if (!ToInteger(cx, val, &initialDbl))
        return false;

    if (initialDbl < 0 || initialDbl > INT32_MAX) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_SIZE, "Table", "initial");
        return false;
    }

    uint32_t initial = uint32_t(initialDbl);
    MOZ_ASSERT(double(initial) == initialDbl);

    if (initial > MaxTableElems) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_SIZE, "Table", "initial");
        return false;
    }

    RootedWasmTableObject table(cx, WasmTableObject::create(cx, initial));
    if (!table)
        return false;

    args.rval().setObject(*table);
    return true;
}

static bool
IsTable(HandleValue v)
{
    return v.isObject() && v.toObject().is<WasmTableObject>();
}

/* static */ bool
WasmTableObject::lengthGetterImpl(JSContext* cx, const CallArgs& args)
{
    args.rval().setNumber(args.thisv().toObject().as<WasmTableObject>().table().length());
    return true;
}

/* static */ bool
WasmTableObject::lengthGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsTable, lengthGetterImpl>(cx, args);
}

const JSPropertySpec WasmTableObject::properties[] =
{
    JS_PSG("length", WasmTableObject::lengthGetter, 0),
    JS_PS_END
};

/* static */ bool
WasmTableObject::getImpl(JSContext* cx, const CallArgs& args)
{
    RootedWasmTableObject tableObj(cx, &args.thisv().toObject().as<WasmTableObject>());
    const Table& table = tableObj->table();

    double indexDbl;
    if (!ToInteger(cx, args.get(0), &indexDbl))
        return false;

    if (indexDbl < 0 || indexDbl >= table.length()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_BAD_INDEX);
        return false;
    }

    uint32_t index = uint32_t(indexDbl);
    MOZ_ASSERT(double(index) == indexDbl);

    if (!table.initialized()) {
        args.rval().setNull();
        return true;
    }

    ExternalTableElem& elem = table.externalArray()[index];
    Instance& instance = *elem.tls->instance;
    const CodeRange* codeRange = instance.code().lookupRange(elem.code);

    // A non-function code range means the bad-indirect-call stub, so a null element.
    if (!codeRange || !codeRange->isFunction()) {
        args.rval().setNull();
        return true;
    }

    RootedWasmInstanceObject instanceObj(cx, instance.object());
    RootedFunction fun(cx);
    if (!instanceObj->getExportedFunction(cx, instanceObj, codeRange->funcIndex(), &fun))
        return false;

    args.rval().setObject(*fun);
    return true;
}

/* static */ bool
WasmTableObject::get(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsTable, getImpl>(cx, args);
}

/* static */ bool
WasmTableObject::setImpl(JSContext* cx, const CallArgs& args)
{
    RootedWasmTableObject tableObj(cx, &args.thisv().toObject().as<WasmTableObject>());
    Table& table = tableObj->table();

    if (!args.requireAtLeast(cx, "set", 2))
        return false;

    double indexDbl;
    if (!ToInteger(cx, args[0], &indexDbl))
        return false;

    if (indexDbl < 0 || indexDbl >= table.length()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_BAD_INDEX);
        return false;
    }

    uint32_t index = uint32_t(indexDbl);
    MOZ_ASSERT(double(index) == indexDbl);

    RootedFunction value(cx);
    if (!IsExportedFunction(args[1], &value) && !args[1].isNull()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_SET_VALUE);
        return false;
    }

    if (!table.initialized()) {
        if (!value) {
            args.rval().setUndefined();
            return true;
        }

        table.init(ExportedFunctionToInstance(value));
    }

    if (value) {
        RootedWasmInstanceObject instanceObj(cx, ExportedFunctionToInstanceObject(value));
        uint32_t funcIndex = ExportedFunctionToIndex(value);

#ifdef DEBUG
        RootedFunction f(cx);
        MOZ_ASSERT(instanceObj->getExportedFunction(cx, instanceObj, funcIndex, &f));
        MOZ_ASSERT(value == f);
#endif

        Instance& instance = instanceObj->instance();
        const FuncExport& funcExport = instance.metadata().lookupFuncExport(funcIndex);
        const CodeRange& codeRange = instance.metadata().codeRanges[funcExport.codeRangeIndex()];
        void* code = instance.codeSegment().base() + codeRange.funcTableEntry();
        table.set(index, code, instance);
    } else {
        table.setNull(index);
    }

    args.rval().setUndefined();
    return true;
}

/* static */ bool
WasmTableObject::set(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsTable, setImpl>(cx, args);
}

const JSFunctionSpec WasmTableObject::methods[] =
{
    JS_FN("get", WasmTableObject::get, 1, 0),
    JS_FN("set", WasmTableObject::set, 2, 0),
    JS_FS_END
};

Table&
WasmTableObject::table() const
{
    return *(Table*)getReservedSlot(TABLE_SLOT).toPrivate();
}

// ============================================================================
// WebAssembly class and static methods

#if JS_HAS_TOSOURCE
static bool
WebAssembly_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().WebAssembly);
    return true;
}
#endif

#ifdef SPIDERMONKEY_PROMISE
static bool
Nop(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();
    return true;
}

static bool
Reject(JSContext* cx, const CompileArgs& args, UniqueChars error, Handle<PromiseObject*> promise)
{
    if (!error) {
        ReportOutOfMemory(cx);

        RootedValue rejectionValue(cx);
        if (!cx->getPendingException(&rejectionValue))
            return false;

        return promise->reject(cx, rejectionValue);
    }

    RootedObject stack(cx, promise->allocationSite());
    RootedString filename(cx, JS_NewStringCopyZ(cx, args.scriptedCaller.filename.get()));
    if (!filename)
        return false;

    unsigned line = args.scriptedCaller.line;
    unsigned column = args.scriptedCaller.column;

    RootedString message(cx, NewLatin1StringZ(cx, Move(error)));
    if (!message)
        return false;

    RootedObject errorObj(cx,
        ErrorObject::create(cx, JSEXN_TYPEERR, stack, filename, line, column, nullptr, message));
    if (!errorObj)
        return false;

    RootedValue rejectionValue(cx, ObjectValue(*errorObj));
    return promise->reject(cx, rejectionValue);
}

static bool
Resolve(JSContext* cx, Module& module, Handle<PromiseObject*> promise)
{
    RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
    RootedObject moduleObj(cx, WasmModuleObject::create(cx, module, proto));
    if (!moduleObj)
        return false;

    RootedValue resolutionValue(cx, ObjectValue(*moduleObj));
    return promise->resolve(cx, resolutionValue);
}

struct CompileTask : PromiseTask
{
    MutableBytes bytecode;
    CompileArgs  compileArgs;
    UniqueChars  error;
    SharedModule module;

    CompileTask(JSContext* cx, Handle<PromiseObject*> promise)
      : PromiseTask(cx, promise)
    {}

    void execute() override {
        module = Compile(*bytecode, compileArgs, &error);
    }

    bool finishPromise(JSContext* cx, Handle<PromiseObject*> promise) override {
        return module
               ? Resolve(cx, *module, promise)
               : Reject(cx, compileArgs, Move(error), promise);
    }
};

static bool
WebAssembly_compile(JSContext* cx, unsigned argc, Value* vp)
{
    if (!cx->startAsyncTaskCallback || !cx->finishAsyncTaskCallback) {
        JS_ReportError(cx, "WebAssembly.compile not supported in this runtime.");
        return false;
    }

    CallArgs callArgs = CallArgsFromVp(argc, vp);

    RootedFunction nopFun(cx, NewNativeFunction(cx, Nop, 0, nullptr));
    if (!nopFun)
        return false;

    Rooted<PromiseObject*> promise(cx, PromiseObject::create(cx, nopFun));
    if (!promise)
        return false;

    auto task = cx->make_unique<CompileTask>(cx, promise);
    if (!task)
        return false;

    if (!GetCompileArgs(cx, callArgs, "WebAssembly.compile", &task->bytecode, &task->compileArgs)) {
        if (!cx->isExceptionPending())
            return false;

        RootedValue rejectionValue(cx);
        if (!GetAndClearException(cx, &rejectionValue))
            return false;

        if (!promise->reject(cx, rejectionValue))
            return false;

        callArgs.rval().setObject(*promise);
        return true;
    }

    if (CanUseExtraThreads()) {
        if (!StartPromiseTask(cx, Move(task)))
            return false;
    } else {
        task->execute();
        task->finishPromise(cx, promise);
    }

    callArgs.rval().setObject(*promise);
    return true;
}
#endif

static const JSFunctionSpec WebAssembly_static_methods[] =
{
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, WebAssembly_toSource, 0, 0),
#endif
#ifdef SPIDERMONKEY_PROMISE
    JS_FN("compile", WebAssembly_compile, 1, 0),
#endif
    JS_FS_END
};

const Class js::WebAssemblyClass =
{
    js_WebAssembly_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_WebAssembly)
};

template <class Class>
static bool
InitConstructor(JSContext* cx, HandleObject wasm, const char* name, MutableHandleObject proto)
{
    proto.set(NewBuiltinClassInstance<PlainObject>(cx, SingletonObject));
    if (!proto)
        return false;

    if (!DefinePropertiesAndFunctions(cx, proto, Class::properties, Class::methods))
        return false;

    RootedAtom className(cx, Atomize(cx, name, strlen(name)));
    if (!className)
        return false;

    RootedFunction ctor(cx, NewNativeConstructor(cx, Class::construct, 1, className));
    if (!ctor)
        return false;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return false;

    RootedId id(cx, AtomToId(className));
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    return DefineProperty(cx, wasm, id, ctorValue, nullptr, nullptr, 0);
}

JSObject*
js::InitWebAssemblyClass(JSContext* cx, HandleObject obj)
{
    MOZ_ASSERT(cx->options().wasm());

    Handle<GlobalObject*> global = obj.as<GlobalObject>();
    MOZ_ASSERT(!global->isStandardClassResolved(JSProto_WebAssembly));

    RootedObject proto(cx, global->getOrCreateObjectPrototype(cx));
    if (!proto)
        return nullptr;

    RootedObject wasm(cx, NewObjectWithGivenProto(cx, &WebAssemblyClass, proto, SingletonObject));
    if (!wasm)
        return nullptr;

    // This property will be removed before the initial WebAssembly release.
    if (!JS_DefineProperty(cx, wasm, "experimentalVersion", EncodingVersion, JSPROP_RESOLVING))
        return nullptr;

    if (!JS_DefineFunctions(cx, wasm, WebAssembly_static_methods))
        return nullptr;

    RootedObject moduleProto(cx), instanceProto(cx), memoryProto(cx), tableProto(cx);
    if (!InitConstructor<WasmModuleObject>(cx, wasm, "Module", &moduleProto))
        return nullptr;
    if (!InitConstructor<WasmInstanceObject>(cx, wasm, "Instance", &instanceProto))
        return nullptr;
    if (!InitConstructor<WasmMemoryObject>(cx, wasm, "Memory", &memoryProto))
        return nullptr;
    if (!InitConstructor<WasmTableObject>(cx, wasm, "Table", &tableProto))
        return nullptr;

    // Perform the final fallible write of the WebAssembly object to a global
    // object property at the end. Only after that succeeds write all the
    // constructor and prototypes to the JSProto slots. This ensures that
    // initialization is atomic since a failed initialization can be retried.

    if (!JS_DefineProperty(cx, global, js_WebAssembly_str, wasm, JSPROP_RESOLVING))
        return nullptr;

    global->setPrototype(JSProto_WasmModule, ObjectValue(*moduleProto));
    global->setPrototype(JSProto_WasmInstance, ObjectValue(*instanceProto));
    global->setPrototype(JSProto_WasmMemory, ObjectValue(*memoryProto));
    global->setPrototype(JSProto_WasmTable, ObjectValue(*tableProto));
    global->setConstructor(JSProto_WebAssembly, ObjectValue(*wasm));

    MOZ_ASSERT(global->isStandardClassResolved(JSProto_WebAssembly));
    return wasm;
}


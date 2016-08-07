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

#include "asmjs/WasmCompile.h"
#include "asmjs/WasmInstance.h"
#include "asmjs/WasmModule.h"

#include "jit/JitOptions.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

bool
wasm::HasCompilerSupport(ExclusiveContext* cx)
{
    if (!cx->jitSupportsFloatingPoint())
        return false;

    if (!cx->jitSupportsUnalignedAccesses())
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

bool
wasm::IsI64Implemented()
{
#if defined(JS_CPU_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    return true;
#else
    return false;
#endif
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

    UniqueChars filename;
    {
        JS::AutoFilename af;
        if (DescribeScriptedCaller(cx, &af)) {
            filename = DuplicateString(cx, af.get());
            if (!filename)
                return false;
        }
    }

    CompileArgs compileArgs;
    if (!compileArgs.initFromContext(cx, Move(filename)))
        return false;

    UniqueChars error;
    SharedModule module = Compile(*bytecode, Move(compileArgs), &error);
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
    JSCLASS_HAS_RESERVED_SLOTS(WasmModuleObject::RESERVED_SLOTS),
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

/* static */ bool
WasmModuleObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs callArgs = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, callArgs, "Module"))
        return false;

    if (!callArgs.requireAtLeast(cx, "WebAssembly.Module", 1))
        return false;

    if (!callArgs.get(0).isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_BUF_ARG);
        return false;
    }

    MutableBytes bytecode = cx->new_<ShareableBytes>();
    if (!bytecode)
        return false;

    if (callArgs[0].toObject().is<TypedArrayObject>()) {
        TypedArrayObject& view = callArgs[0].toObject().as<TypedArrayObject>();
        if (!bytecode->append((uint8_t*)view.viewDataEither().unwrap(), view.byteLength())) {
            ReportOutOfMemory(cx);
            return false;
        }
    } else if (callArgs[0].toObject().is<ArrayBufferObject>()) {
        ArrayBufferObject& buffer = callArgs[0].toObject().as<ArrayBufferObject>();
        if (!bytecode->append(buffer.dataPointer(), buffer.byteLength())) {
            ReportOutOfMemory(cx);
            return false;
        }
    } else {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_BUF_ARG);
        return false;
    }

    UniqueChars filename;
    {
        JS::AutoFilename af;
        if (DescribeScriptedCaller(cx, &af)) {
            filename = DuplicateString(cx, af.get());
            if (!filename)
                return false;
        }
    }

    CompileArgs compileArgs;
    if (!compileArgs.initFromContext(cx, Move(filename)))
        return false;

    compileArgs.assumptions.newFormat = true;

    if (!CheckCompilerSupport(cx))
        return false;

    UniqueChars error;
    SharedModule module = Compile(*bytecode, Move(compileArgs), &error);
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
    JSCLASS_HAS_RESERVED_SLOTS(WasmInstanceObject::RESERVED_SLOTS),
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

    RootedObject obj(cx, &args[0].toObject());
    RootedId id(cx, AtomToId(initialAtom));
    RootedValue initialVal(cx);
    if (!GetProperty(cx, obj, obj, id, &initialVal))
        return false;

    double initialDbl;
    if (!ToInteger(cx, initialVal, &initialDbl))
        return false;

    if (initialDbl < 0 || initialDbl > INT32_MAX / PageSize) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_SIZE, "Memory", "initial");
        return false;
    }

    uint32_t bytes = uint32_t(initialDbl) * PageSize;
    bool signalsForOOB = SignalUsage().forOOB;
    RootedArrayBufferObject buffer(cx, ArrayBufferObject::createForWasm(cx, bytes, signalsForOOB));
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
    JSCLASS_HAS_RESERVED_SLOTS(WasmTableObject::RESERVED_SLOTS),
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
        tableObj.table().trace(trc);
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

    SharedTable table = Table::create(cx, desc);
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

static const JSFunctionSpec WebAssembly_static_methods[] =
{
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, WebAssembly_toSource, 0, 0),
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


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

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;
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

// ============================================================================
// (Temporary) Wasm class and static methods

static bool
Throw(JSContext* cx, const char* str)
{
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_FAIL, str);
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
GetImports(JSContext* cx, HandleObject importObj, const ImportVector& imports,
           MutableHandle<FunctionVector> funcImports, MutableHandleWasmMemoryObject memoryImport)
{
    if (!imports.empty() && !importObj)
        return Throw(cx, "no import object given");

    for (const Import& import : imports) {
        RootedValue v(cx);
        if (!GetProperty(cx, importObj, import.module.get(), &v))
            return false;

        if (strlen(import.func.get()) > 0) {
            if (!v.isObject())
                return Throw(cx, "import object field is not an Object");

            RootedObject obj(cx, &v.toObject());
            if (!GetProperty(cx, obj, import.func.get(), &v))
                return false;
        }

        switch (import.kind) {
          case DefinitionKind::Function:
            if (!IsFunctionObject(v))
                return Throw(cx, "import object field is not a Function");

            if (!funcImports.append(&v.toObject().as<JSFunction>()))
                return false;

            break;
          case DefinitionKind::Table:
            MOZ_CRASH("NYI");
          case DefinitionKind::Memory:
            if (!v.isObject() || !v.toObject().is<WasmMemoryObject>())
                return Throw(cx, "import object field is not a Memory");

            MOZ_ASSERT(!memoryImport);
            memoryImport.set(&v.toObject().as<WasmMemoryObject>());
            break;
        }
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

    Rooted<FunctionVector> funcImports(cx, FunctionVector(cx));
    RootedWasmMemoryObject memoryImport(cx);
    if (!GetImports(cx, importObj, module->imports(), &funcImports, &memoryImport))
        return false;

    instanceObj.set(WasmInstanceObject::create(cx));
    if (!instanceObj)
        return false;

    return module->instantiate(cx, funcImports, memoryImport, instanceObj);
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

    obj->initReservedSlot(MODULE_SLOT, PrivateValue((void*)&module));
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
        if (!bytecode->append((uint8_t*)view.viewDataEither().unwrap(), view.byteLength()))
            return false;
    } else if (callArgs[0].toObject().is<ArrayBufferObject>()) {
        ArrayBufferObject& buffer = callArgs[0].toObject().as<ArrayBufferObject>();
        if (!bytecode->append(buffer.dataPointer(), buffer.byteLength()))
            return false;
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

bool
WasmInstanceObject::isNewborn() const
{
    MOZ_ASSERT(is<WasmInstanceObject>());
    return getReservedSlot(INSTANCE_SLOT).isUndefined();
}

/* static */ void
WasmInstanceObject::finalize(FreeOp* fop, JSObject* obj)
{
    if (!obj->as<WasmInstanceObject>().isNewborn())
        fop->delete_(&obj->as<WasmInstanceObject>().instance());
}

/* static */ void
WasmInstanceObject::trace(JSTracer* trc, JSObject* obj)
{
    if (!obj->as<WasmInstanceObject>().isNewborn())
        obj->as<WasmInstanceObject>().instance().trace(trc);
}

/* static */ WasmInstanceObject*
WasmInstanceObject::create(ExclusiveContext* cx, HandleObject proto)
{
    AutoSetNewObjectMetadata metadata(cx);
    auto* obj = NewObjectWithGivenProto<WasmInstanceObject>(cx, proto);
    if (!obj)
        return nullptr;

    MOZ_ASSERT(obj->isNewborn());
    return obj;
}

void
WasmInstanceObject::init(UniqueInstance instance)
{
    MOZ_ASSERT(isNewborn());
    initReservedSlot(INSTANCE_SLOT, PrivateValue((void*)instance.release()));
    MOZ_ASSERT(!isNewborn());
}

void
WasmInstanceObject::initExportsObject(HandleObject exportObj)
{
    initReservedSlot(EXPORTS_SLOT, ObjectValue(*exportObj));
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

    Rooted<FunctionVector> funcImports(cx, FunctionVector(cx));
    RootedWasmMemoryObject memoryImport(cx);
    if (!GetImports(cx, importObj, module.imports(), &funcImports, &memoryImport))
        return false;

    RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmInstance).toObject());
    RootedWasmInstanceObject instanceObj(cx, WasmInstanceObject::create(cx, proto));
    if (!instanceObj)
        return false;

    if (!module.instantiate(cx, funcImports, memoryImport, instanceObj))
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

JSObject&
WasmInstanceObject::exportsObject() const
{
    MOZ_ASSERT(!isNewborn());
    return getReservedSlot(EXPORTS_SLOT).toObject();
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

ArrayBufferObjectMaybeShared&
WasmMemoryObject::buffer() const
{
    return getReservedSlot(BUFFER_SLOT).toObject().as<ArrayBufferObjectMaybeShared>();
}

// ============================================================================
// WebAssembly.Table class and methods

static void
WasmTableObject_finalize(FreeOp* fop, JSObject* obj)
{
    obj->as<WasmTableObject>().table().Release();
}

static const ClassOps WasmTableObject_classOps =
{
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    WasmTableObject_finalize
};

const Class WasmTableObject::class_ =
{
    "WebAssembly.Table",
    JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(WasmTableObject::RESERVED_SLOTS),
    &WasmTableObject_classOps
};

/* static */ WasmTableObject*
WasmTableObject::create(JSContext* cx, Table& table)
{
    RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmTable).toObject());

    AutoSetNewObjectMetadata metadata(cx);
    auto* obj = NewObjectWithGivenProto<WasmTableObject>(cx, proto);
    if (!obj)
        return nullptr;

    obj->initReservedSlot(TABLE_SLOT, PrivateValue((void*)&table));
    table.AddRef();
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

    if (initialDbl < 0 || initialDbl > INT32_MAX) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_SIZE, "Table", "initial");
        return false;
    }

    SharedTable table = Table::create(cx, TableKind::AnyFunction, uint32_t(initialDbl));
    if (!table)
        return false;

    RootedWasmTableObject tableObj(cx, WasmTableObject::create(cx, *table));
    if (!tableObj)
        return false;

    args.rval().setObject(*tableObj);
    return true;
}

static bool
IsTable(HandleValue v)
{
    return v.isObject() && v.toObject().is<WasmTableObject>();
}

static bool
TableLengthGetterImpl(JSContext* cx, const CallArgs& args)
{
    args.rval().setNumber(args.thisv().toObject().as<WasmTableObject>().table().length());
    return true;
}

static bool
TableLengthGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsTable, TableLengthGetterImpl>(cx, args);
}

const JSPropertySpec WasmTableObject::properties[] =
{
    JS_PSG("length", TableLengthGetter, 0),
    JS_PS_END
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

    if (!JS_DefineProperties(cx, proto, Class::properties))
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


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

#include "asmjs/Wasm.h"
#include "asmjs/WasmInstance.h"
#include "asmjs/WasmModule.h"

#include "jsobjinlines.h"

using namespace js;
using namespace js::wasm;

static bool
InstantiateModule(JSContext* cx, unsigned argc, Value* vp)
{
    MOZ_ASSERT(cx->runtime()->options().wasm());
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
    MOZ_ASSERT(cx->runtime()->options().wasm());

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
    "WasmModuleObject",
    JSCLASS_IS_ANONYMOUS | JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(WasmModuleObject::RESERVED_SLOTS),
    &WasmModuleObject::classOps_,
};

/* static */ void
WasmModuleObject::finalize(FreeOp* fop, JSObject* obj)
{
    fop->delete_(&obj->as<WasmModuleObject>().module());
}

/* static */ WasmModuleObject*
WasmModuleObject::create(ExclusiveContext* cx, UniqueModule module)
{
    AutoSetNewObjectMetadata metadata(cx);
    auto obj = NewObjectWithGivenProto<WasmModuleObject>(cx, nullptr);
    if (!obj)
        return nullptr;

    obj->setReservedSlot(MODULE_SLOT, PrivateValue((void*)module.release()));
    return obj;
}

Module&
WasmModuleObject::module() const
{
    MOZ_ASSERT(is<WasmModuleObject>());
    return *(Module*)getReservedSlot(MODULE_SLOT).toPrivate();
}

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
    "WasmInstanceObject",
    JSCLASS_IS_ANONYMOUS | JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(WasmInstanceObject::RESERVED_SLOTS),
    &WasmInstanceObject::classOps_,
};

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
WasmInstanceObject::create(ExclusiveContext* cx)
{
    AutoSetNewObjectMetadata metadata(cx);
    auto obj = NewObjectWithGivenProto<WasmInstanceObject>(cx, nullptr);
    if (!obj)
        return nullptr;

    MOZ_ASSERT(obj->isNewborn());
    return obj;
}

void
WasmInstanceObject::init(UniqueInstance instance)
{
    MOZ_ASSERT(isNewborn());
    setReservedSlot(INSTANCE_SLOT, PrivateValue((void*)instance.release()));
    MOZ_ASSERT(!isNewborn());
}

void
WasmInstanceObject::initExportsObject(HandleObject exportObj)
{
    initReservedSlot(EXPORTS_SLOT, ObjectValue(*exportObj));
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

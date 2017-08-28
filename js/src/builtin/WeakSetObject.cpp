/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/WeakSetObject.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jsiter.h"

#include "builtin/MapObject.h"
#include "builtin/SelfHostingDefines.h"
#include "builtin/WeakMapObject.h"
#include "vm/GlobalObject.h"
#include "vm/SelfHosting.h"

#include "jsobjinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

const Class WeakSetObject::class_ = {
    "WeakSet",
    JSCLASS_HAS_CACHED_PROTO(JSProto_WeakSet) |
    JSCLASS_HAS_RESERVED_SLOTS(WeakSetObject::RESERVED_SLOTS)
};

const JSPropertySpec WeakSetObject::properties[] = {
    JS_PS_END
};

const JSFunctionSpec WeakSetObject::methods[] = {
    JS_SELF_HOSTED_FN("add",    "WeakSet_add",    1, 0),
    JS_SELF_HOSTED_FN("delete", "WeakSet_delete", 1, 0),
    JS_SELF_HOSTED_FN("has",    "WeakSet_has",    1, 0),
    JS_FS_END
};

JSObject*
WeakSetObject::initClass(JSContext* cx, HandleObject obj)
{
    Handle<GlobalObject*> global = obj.as<GlobalObject>();
    RootedPlainObject proto(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!proto)
        return nullptr;

    Rooted<JSFunction*> ctor(cx, GlobalObject::createConstructor(cx, construct,
                                                                 ClassName(JSProto_WeakSet, cx), 0));
    if (!ctor ||
        !LinkConstructorAndPrototype(cx, ctor, proto) ||
        !DefinePropertiesAndFunctions(cx, proto, properties, methods) ||
        !DefineToStringTag(cx, proto, cx->names().WeakSet) ||
        !GlobalObject::initBuiltinConstructor(cx, global, JSProto_WeakSet, ctor, proto))
    {
        return nullptr;
    }
    return proto;
}

WeakSetObject*
WeakSetObject::create(JSContext* cx, HandleObject proto /* = nullptr */)
{
    RootedObject map(cx, NewBuiltinClassInstance<WeakMapObject>(cx));
    if (!map)
        return nullptr;

    WeakSetObject* obj = NewObjectWithClassProto<WeakSetObject>(cx, proto);
    if (!obj)
        return nullptr;

    obj->setReservedSlot(WEAKSET_MAP_SLOT, ObjectValue(*map));
    return obj;
}

bool
WeakSetObject::isBuiltinAdd(HandleValue add, JSContext* cx)
{
    JSFunction* addFn;
    return IsFunctionObject(add, &addFn) && IsSelfHostedFunctionWithName(addFn, cx->names().WeakSet_add);
}

bool
WeakSetObject::construct(JSContext* cx, unsigned argc, Value* vp)
{
    // Based on our "Set" implementation instead of the more general ES6 steps.
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "WeakSet"))
        return false;

    RootedObject proto(cx);
    if (!GetPrototypeFromBuiltinConstructor(cx, args, &proto))
        return false;

    Rooted<WeakSetObject*> obj(cx, WeakSetObject::create(cx, proto));
    if (!obj)
        return false;

    if (!args.get(0).isNullOrUndefined()) {
        RootedValue iterable(cx, args[0]);
        bool optimized = false;
        if (!IsOptimizableInitForSet<GlobalObject::getOrCreateWeakSetPrototype, isBuiltinAdd>(cx, obj, iterable, &optimized))
            return false;

        if (optimized) {
            RootedValue keyVal(cx);
            RootedObject keyObject(cx);
            RootedValue placeholder(cx, BooleanValue(true));
            RootedObject map(cx, &obj->getReservedSlot(WEAKSET_MAP_SLOT).toObject());
            RootedArrayObject array(cx, &iterable.toObject().as<ArrayObject>());
            for (uint32_t index = 0; index < array->getDenseInitializedLength(); ++index) {
                keyVal.set(array->getDenseElement(index));
                MOZ_ASSERT(!keyVal.isMagic(JS_ELEMENTS_HOLE));

                if (keyVal.isPrimitive()) {
                    ReportNotObjectWithName(cx, "WeakSet value", keyVal);
                    return false;
                }

                keyObject = &keyVal.toObject();
                if (!SetWeakMapEntry(cx, map, keyObject, placeholder))
                    return false;
            }
        } else {
            FixedInvokeArgs<1> args2(cx);
            args2[0].set(args[0]);

            RootedValue thisv(cx, ObjectValue(*obj));
            if (!CallSelfHostedFunction(cx, cx->names().WeakSetConstructorInit, thisv, args2, args2.rval()))
                return false;
        }
    }

    args.rval().setObject(*obj);
    return true;
}


JSObject*
js::InitWeakSetClass(JSContext* cx, HandleObject obj)
{
    return WeakSetObject::initClass(cx, obj);
}

JS_FRIEND_API(bool)
JS_NondeterministicGetWeakSetKeys(JSContext* cx, HandleObject objArg, MutableHandleObject ret)
{
    RootedObject obj(cx, objArg);
    obj = UncheckedUnwrap(obj);
    if (!obj || !obj->is<WeakSetObject>()) {
        ret.set(nullptr);
        return true;
    }

    Rooted<WeakSetObject*> weakset(cx, &obj->as<WeakSetObject>());
    if (!weakset)
        return false;

    RootedObject map(cx, &weakset->getReservedSlot(WEAKSET_MAP_SLOT).toObject());
    return JS_NondeterministicGetWeakMapKeys(cx, map, ret);
}

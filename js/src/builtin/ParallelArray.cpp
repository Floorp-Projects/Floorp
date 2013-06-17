/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsobj.h"
#include "jsarray.h"

#include "builtin/ParallelArray.h"

#include "vm/ForkJoin.h"
#include "vm/GlobalObject.h"
#include "vm/String.h"
#include "vm/ThreadPool.h"

#include "vm/Interpreter-inl.h"

using namespace js;

//
// ParallelArrayObject
//

FixedHeapPtr<PropertyName> ParallelArrayObject::ctorNames[NumCtors];

const JSFunctionSpec ParallelArrayObject::methods[] = {
    { "map",       JSOP_NULLWRAPPER, 2, 0, "ParallelArrayMap"       },
    { "reduce",    JSOP_NULLWRAPPER, 2, 0, "ParallelArrayReduce"    },
    { "scan",      JSOP_NULLWRAPPER, 2, 0, "ParallelArrayScan"      },
    { "scatter",   JSOP_NULLWRAPPER, 5, 0, "ParallelArrayScatter"   },
    { "filter",    JSOP_NULLWRAPPER, 2, 0, "ParallelArrayFilter"    },
    { "partition", JSOP_NULLWRAPPER, 1, 0, "ParallelArrayPartition" },
    { "flatten",   JSOP_NULLWRAPPER, 0, 0, "ParallelArrayFlatten" },

    // FIXME #838906. Note that `get()` is not currently defined on this table but
    // rather is assigned to each instance of ParallelArray as an own
    // property.  This is a bit of a hack designed to supply a
    // specialized version of get() based on the dimensionality of the
    // receiver.  In the future we can improve this by (1) extending
    // TI to track the dimensionality of the receiver and (2) using a
    // hint to aggressively inline calls to get().
    // { "get",      JSOP_NULLWRAPPER, 1, 0, "ParallelArrayGet" },

    { "toString", JSOP_NULLWRAPPER, 0, 0, "ParallelArrayToString" },
    JS_FS_END
};

Class ParallelArrayObject::protoClass = {
    "ParallelArray",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ParallelArray),
    JS_PropertyStub,         // addProperty
    JS_DeletePropertyStub,   // delProperty
    JS_PropertyStub,         // getProperty
    JS_StrictPropertyStub,   // setProperty
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

Class ParallelArrayObject::class_ = {
    "ParallelArray",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ParallelArray),
    JS_PropertyStub,         // addProperty
    JS_DeletePropertyStub,   // delProperty
    JS_PropertyStub,         // getProperty
    JS_StrictPropertyStub,   // setProperty
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

/*static*/ bool
ParallelArrayObject::initProps(JSContext *cx, HandleObject obj)
{
    RootedValue undef(cx, UndefinedValue());
    RootedValue zero(cx, Int32Value(0));

    if (!JSObject::defineProperty(cx, obj, cx->names().buffer, undef))
        return false;
    if (!JSObject::defineProperty(cx, obj, cx->names().offset, zero))
        return false;
    if (!JSObject::defineProperty(cx, obj, cx->names().shape, undef))
        return false;
    if (!JSObject::defineProperty(cx, obj, cx->names().get, undef))
        return false;

    return true;
}

/*static*/ JSBool
ParallelArrayObject::construct(JSContext *cx, unsigned argc, Value *vp)
{
    RootedFunction ctor(cx, getConstructor(cx, argc));
    if (!ctor)
        return false;
    CallArgs args = CallArgsFromVp(argc, vp);
    return constructHelper(cx, &ctor, args);
}


/* static */ JSFunction *
ParallelArrayObject::getConstructor(JSContext *cx, unsigned argc)
{
    RootedPropertyName ctorName(cx, ctorNames[js::Min(argc, NumCtors - 1)]);
    RootedValue ctorValue(cx);
    if (!cx->global()->getIntrinsicValue(cx, ctorName, &ctorValue))
        return NULL;
    JS_ASSERT(ctorValue.isObject() && ctorValue.toObject().isFunction());
    return ctorValue.toObject().toFunction();
}

/*static*/ JSObject *
ParallelArrayObject::newInstance(JSContext *cx, NewObjectKind newKind /* = GenericObject */)
{
    gc::AllocKind kind = gc::GetGCObjectKind(NumFixedSlots);
    RootedObject result(cx, NewBuiltinClassInstance(cx, &class_, kind, newKind));
    if (!result)
        return NULL;

    // Add in the basic PA properties now with default values:
    if (!initProps(cx, result))
        return NULL;

    return result;
}

/*static*/ JSBool
ParallelArrayObject::constructHelper(JSContext *cx, MutableHandleFunction ctor, CallArgs &args0)
{
    RootedObject result(cx, newInstance(cx, TenuredObject));
    if (!result)
        return false;

    if (cx->typeInferenceEnabled()) {
        jsbytecode *pc;
        RootedScript script(cx, cx->stack.currentScript(&pc));
        if (script) {
            if (ctor->nonLazyScript()->shouldCloneAtCallsite) {
                ctor.set(CloneFunctionAtCallsite(cx, ctor, script, pc));
                if (!ctor)
                    return false;
            }

            // Create the type object for the PA.  Add in the current
            // properties as definite properties if this type object is newly
            // created.  To tell if it is newly created, we check whether it
            // has any properties yet or not, since any returned type object
            // must have been created by this same C++ code and hence would
            // already have properties if it had been returned before.
            types::TypeObject *paTypeObject =
                types::TypeScript::InitObject(cx, script, pc, JSProto_ParallelArray);
            if (!paTypeObject)
                return false;
            if (paTypeObject->getPropertyCount() == 0 && !paTypeObject->unknownProperties()) {
                if (!paTypeObject->addDefiniteProperties(cx, result))
                    return false;

                // addDefiniteProperties() above should have added one
                // property for each of the fixed slots:
                JS_ASSERT(paTypeObject->getPropertyCount() == NumFixedSlots);
            }
            result->setType(paTypeObject);
        }
    }

    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, args0.length(), &args))
        return false;

    args.setCallee(ObjectValue(*ctor));
    args.setThis(ObjectValue(*result));

    for (uint32_t i = 0; i < args0.length(); i++)
        args[i] = args0[i];

    if (!Invoke(cx, args))
        return false;

    args0.rval().setObject(*result);
    return true;
}

JSObject *
ParallelArrayObject::initClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());

    // Cache constructor names.
    {
        const char *ctorStrs[NumCtors] = { "ParallelArrayConstructEmpty",
                                           "ParallelArrayConstructFromArray",
                                           "ParallelArrayConstructFromFunction",
                                           "ParallelArrayConstructFromFunctionMode" };
        for (uint32_t i = 0; i < NumCtors; i++) {
            JSAtom *atom = Atomize(cx, ctorStrs[i], strlen(ctorStrs[i]), InternAtom);
            if (!atom)
                return NULL;
            ctorNames[i].init(atom->asPropertyName());
        }
    }

    Rooted<GlobalObject *> global(cx, &obj->asGlobal());

    RootedObject proto(cx, global->createBlankPrototype(cx, &protoClass));
    if (!proto)
        return NULL;

    JSProtoKey key = JSProto_ParallelArray;
    RootedFunction ctor(cx, global->createConstructor(cx, construct,
                                                      cx->names().ParallelArray, 0));
    if (!ctor ||
        !LinkConstructorAndPrototype(cx, ctor, proto) ||
        !DefinePropertiesAndBrand(cx, proto, NULL, methods) ||
        !DefineConstructorAndPrototype(cx, global, key, ctor, proto))
    {
        return NULL;
    }

    // Define the length getter.
    {
        const char lengthStr[] = "ParallelArrayLength";
        JSAtom *atom = Atomize(cx, lengthStr, strlen(lengthStr));
        if (!atom)
            return NULL;
        Rooted<PropertyName *> lengthProp(cx, atom->asPropertyName());
        RootedValue lengthValue(cx);
        if (!cx->global()->getIntrinsicValue(cx, lengthProp, &lengthValue))
            return NULL;
        RootedObject lengthGetter(cx, &lengthValue.toObject());
        if (!lengthGetter)
            return NULL;

        RootedId lengthId(cx, AtomToId(cx->names().length));
        unsigned flags = JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_GETTER;
        RootedValue value(cx, UndefinedValue());
        if (!DefineNativeProperty(cx, proto, lengthId, value,
                                  JS_DATA_TO_FUNC_PTR(PropertyOp, lengthGetter.get()), NULL,
                                  flags, 0, 0))
        {
            return NULL;
        }
    }

    return proto;
}

bool
ParallelArrayObject::is(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&class_);
}

JSObject *
js_InitParallelArrayClass(JSContext *cx, js::HandleObject obj)
{
    return ParallelArrayObject::initClass(cx, obj);
}

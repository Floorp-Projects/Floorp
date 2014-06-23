/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/SymbolObject.h"

#include "jsobjinlines.h"

#include "vm/Symbol-inl.h"

using namespace js;

const Class SymbolObject::class_ = {
    "Symbol",
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) | JSCLASS_HAS_CACHED_PROTO(JSProto_Symbol),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

SymbolObject *
SymbolObject::create(JSContext *cx, JS::Symbol *symbol)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &class_);
    if (!obj)
        return nullptr;
    SymbolObject &symobj = obj->as<SymbolObject>();
    symobj.setPrimitiveValue(symbol);
    return &symobj;
}

const JSPropertySpec SymbolObject::properties[] = {
    JS_PS_END
};

const JSFunctionSpec SymbolObject::methods[] = {
    JS_FS_END
};

const JSFunctionSpec SymbolObject::staticMethods[] = {
    JS_FS_END
};

JSObject *
SymbolObject::initClass(JSContext *cx, HandleObject obj)
{
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    // This uses &JSObject::class_ because: "The Symbol prototype object is an
    // ordinary object. It is not a Symbol instance and does not have a
    // [[SymbolData]] internal slot." (ES6 rev 24, 19.4.3)
    RootedObject proto(cx, global->createBlankPrototype(cx, &JSObject::class_));
    if (!proto)
        return nullptr;

    RootedFunction ctor(cx, global->createConstructor(cx, construct,
                                                      ClassName(JSProto_Symbol, cx), 1));
    if (!ctor ||
        !LinkConstructorAndPrototype(cx, ctor, proto) ||
        !DefinePropertiesAndFunctions(cx, proto, properties, methods) ||
        !DefinePropertiesAndFunctions(cx, ctor, nullptr, staticMethods) ||
        !GlobalObject::initBuiltinConstructor(cx, global, JSProto_Symbol, ctor, proto))
    {
        return nullptr;
    }
    return proto;
}

// ES6 rev 24 (2014 Apr 27) 19.4.1.1 and 19.4.1.2
bool
SymbolObject::construct(JSContext *cx, unsigned argc, Value *vp)
{
    // According to a note in the draft standard, "Symbol has ordinary
    // [[Construct]] behaviour but the definition of its @@create method causes
    // `new Symbol` to throw a TypeError exception." We do not support @@create
    // yet, so just throw a TypeError.
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.isConstructing()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_CONSTRUCTOR, "Symbol");
        return false;
    }

    // steps 1-3
    RootedString desc(cx);
    if (!args.get(0).isUndefined()) {
        desc = ToString(cx, args.get(0));
        if (!desc)
            return false;
    }

    // step 4
    RootedSymbol symbol(cx, JS::Symbol::new_(cx, desc));
    if (!symbol)
        return false;
    args.rval().setSymbol(symbol);
    return true;
}

JSObject *
js_InitSymbolClass(JSContext *cx, HandleObject obj)
{
    return SymbolObject::initClass(cx, obj);
}

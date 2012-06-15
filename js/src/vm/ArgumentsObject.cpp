/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsgc.h"
#include "jsinfer.h"
#include "jsinterp.h"

#include "vm/GlobalObject.h"
#include "vm/MethodGuard.h"
#include "vm/Stack.h"
#include "vm/Xdr.h"

#include "jsobjinlines.h"

#include "gc/Barrier-inl.h"
#include "vm/ArgumentsObject-inl.h"

using namespace js;
using namespace js::gc;

ArgumentsObject *
ArgumentsObject::create(JSContext *cx, StackFrame *fp)
{
    JSFunction &callee = fp->callee();
    RootedObject proto(cx, callee.global().getOrCreateObjectPrototype(cx));
    if (!proto)
        return NULL;

    RootedTypeObject type(cx);
    type = proto->getNewType(cx);
    if (!type)
        return NULL;

    bool strict = callee.inStrictMode();
    Class *clasp = strict ? &StrictArgumentsObjectClass : &NormalArgumentsObjectClass;

    RootedShape emptyArgumentsShape(cx);
    emptyArgumentsShape =
        EmptyShape::getInitialShape(cx, clasp, proto,
                                    proto->getParent(), FINALIZE_KIND,
                                    BaseShape::INDEXED);
    if (!emptyArgumentsShape)
        return NULL;

    unsigned numActuals = fp->numActualArgs();
    unsigned numFormals = fp->numFormalArgs();
    unsigned numDeletedWords = NumWordsForBitArrayOfLength(numActuals);
    unsigned numArgs = Max(numActuals, numFormals);
    unsigned numBytes = offsetof(ArgumentsData, args) +
                        numDeletedWords * sizeof(size_t) +
                        numArgs * sizeof(Value);

    ArgumentsData *data = (ArgumentsData *)cx->malloc_(numBytes);
    if (!data)
        return NULL;

    data->numArgs = numArgs;
    data->callee.init(ObjectValue(callee));
    data->script = fp->script();

    /* Copy [0, numArgs) into data->slots. */
    HeapValue *dst = data->args, *dstEnd = data->args + numArgs;
    for (Value *src = fp->formals(), *end = src + numFormals; src != end; ++src, ++dst)
        dst->init(*src);
    if (numActuals > numFormals) {
        for (Value *src = fp->actuals() + numFormals; dst != dstEnd; ++src, ++dst)
            dst->init(*src);
    } else if (numActuals < numFormals) {
        for (; dst != dstEnd; ++dst)
            dst->init(UndefinedValue());
    }

    data->deletedBits = reinterpret_cast<size_t *>(dstEnd);
    ClearAllBitArrayElements(data->deletedBits, numDeletedWords);

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyArgumentsShape, type, NULL);
    if (!obj)
        return NULL;

    obj->initFixedSlot(INITIAL_LENGTH_SLOT, Int32Value(numActuals << PACKED_BITS_COUNT));
    obj->initFixedSlot(DATA_SLOT, PrivateValue(data));

    /*
     * If it exists and the arguments object aliases formals, the call object
     * is the canonical location for formals.
     */
    JSScript *script = fp->script();
    if (fp->fun()->isHeavyweight() && script->argsObjAliasesFormals()) {
        obj->initFixedSlot(MAYBE_CALL_SLOT, ObjectValue(fp->callObj()));

        /* Flag each slot that canonically lives in the callObj. */
        if (script->bindingsAccessedDynamically) {
            for (unsigned i = 0; i < numFormals; ++i)
                data->args[i] = MagicValue(JS_FORWARD_TO_CALL_OBJECT);
        } else {
            for (unsigned i = 0; i < script->numClosedArgs(); ++i)
                data->args[script->getClosedArg(i)] = MagicValue(JS_FORWARD_TO_CALL_OBJECT);
        }
    }

    ArgumentsObject &argsobj = obj->asArguments();
    JS_ASSERT(argsobj.initialLength() == numActuals);
    JS_ASSERT(!argsobj.hasOverriddenLength());
    return &argsobj;
}

ArgumentsObject *
ArgumentsObject::createExpected(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(fp->script()->needsArgsObj());
    ArgumentsObject *argsobj = create(cx, fp);
    if (!argsobj)
        return NULL;

    fp->initArgsObj(*argsobj);
    return argsobj;
}

ArgumentsObject *
ArgumentsObject::createUnexpected(JSContext *cx, StackFrame *fp)
{
    return create(cx, fp);
}

static JSBool
args_delProperty(JSContext *cx, HandleObject obj, HandleId id, Value *vp)
{
    ArgumentsObject &argsobj = obj->asArguments();
    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            argsobj.markElementDeleted(arg);
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        argsobj.markLengthOverridden();
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom)) {
        argsobj.asNormalArguments().clearCallee();
    }
    return true;
}

static JSBool
ArgGetter(JSContext *cx, HandleObject obj, HandleId id, Value *vp)
{
    if (!obj->isNormalArguments())
        return true;

    NormalArgumentsObject &argsobj = obj->asNormalArguments();
    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            *vp = argsobj.element(arg);
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (!argsobj.hasOverriddenLength())
            *vp = Int32Value(argsobj.initialLength());
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom));
        if (!argsobj.callee().isMagic(JS_OVERWRITTEN_CALLEE))
            *vp = argsobj.callee();
    }
    return true;
}

static JSBool
ArgSetter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp)
{
    if (!obj->isNormalArguments())
        return true;

    NormalArgumentsObject &argsobj = obj->asNormalArguments();
    JSScript *script = argsobj.containingScript();

    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg)) {
            argsobj.setElement(arg, *vp);
            if (arg < script->function()->nargs) {
                if (!script->ensureHasTypes(cx))
                    return false;
                types::TypeScript::SetArgument(cx, script, arg, *vp);
            }
            return true;
        }
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom) ||
                  JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom));
    }

    /*
     * For simplicity we use delete/define to replace the property with one
     * backed by the default Object getter and setter. Note that we rely on
     * args_delProperty to clear the corresponding reserved slot so the GC can
     * collect its value. Note also that we must define the property instead
     * of setting it in case the user has changed the prototype to an object
     * that has a setter for this id.
     */
    RootedValue value(cx);
    return baseops::DeleteGeneric(cx, obj, id, value.address(), false) &&
           baseops::DefineGeneric(cx, obj, id, vp, NULL, NULL, JSPROP_ENUMERATE);
}

static JSBool
args_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
             JSObject **objp)
{
    *objp = NULL;

    Rooted<NormalArgumentsObject*> argsobj(cx, &obj->asNormalArguments());

    unsigned attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    if (JSID_IS_INT(id)) {
        uint32_t arg = uint32_t(JSID_TO_INT(id));
        if (arg >= argsobj->initialLength() || argsobj->isElementDeleted(arg))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (argsobj->hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom))
            return true;

        if (argsobj->callee().isMagic(JS_OVERWRITTEN_CALLEE))
            return true;
    }

    Value undef = UndefinedValue();
    if (!baseops::DefineGeneric(cx, argsobj, id, &undef, ArgGetter, ArgSetter, attrs))
        return JS_FALSE;

    *objp = argsobj;
    return true;
}

bool
NormalArgumentsObject::optimizedGetElem(JSContext *cx, StackFrame *fp, const Value &elem, Value *vp)
{
    JS_ASSERT(!fp->script()->needsArgsObj());

    /* Fast path: no need to convert to id when elem is already an int in range. */
    if (elem.isInt32()) {
        int32_t i = elem.toInt32();
        if (i >= 0 && uint32_t(i) < fp->numActualArgs()) {
            *vp = fp->unaliasedActual(i);
            return true;
        }
    }

    /* Slow path: create and canonicalize an id, then emulate args_resolve. */

    jsid id;
    if (!ValueToId(cx, elem, &id))
        return false;

    if (JSID_IS_INT(id)) {
        int32_t i = JSID_TO_INT(id);
        if (i >= 0 && uint32_t(i) < fp->numActualArgs()) {
            *vp = fp->unaliasedActual(i);
            return true;
        }
    }

    if (id == NameToId(cx->runtime->atomState.lengthAtom)) {
        *vp = Int32Value(fp->numActualArgs());
        return true;
    }

    if (id == NameToId(cx->runtime->atomState.calleeAtom)) {
        *vp = ObjectValue(fp->callee());
        return true;
    }

    JSObject *proto = fp->global().getOrCreateObjectPrototype(cx);
    if (!proto)
        return false;

    Rooted<jsid> root(cx, id);
    return proto->getGeneric(cx, root, vp);
}

static JSBool
args_enumerate(JSContext *cx, HandleObject obj)
{
    Rooted<NormalArgumentsObject*> argsobj(cx, &obj->asNormalArguments());
    RootedId id(cx);

    /*
     * Trigger reflection in args_resolve using a series of js_LookupProperty
     * calls.
     */
    int argc = int(argsobj->initialLength());
    for (int i = -2; i != argc; i++) {
        id = (i == -2)
             ? NameToId(cx->runtime->atomState.lengthAtom)
             : (i == -1)
             ? NameToId(cx->runtime->atomState.calleeAtom)
             : INT_TO_JSID(i);

        JSObject *pobj;
        JSProperty *prop;
        if (!baseops::LookupProperty(cx, argsobj, id, &pobj, &prop))
            return false;
    }
    return true;
}

static JSBool
StrictArgGetter(JSContext *cx, HandleObject obj, HandleId id, Value *vp)
{
    if (!obj->isStrictArguments())
        return true;

    StrictArgumentsObject &argsobj = obj->asStrictArguments();

    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            *vp = argsobj.element(arg);
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom));
        if (!argsobj.hasOverriddenLength())
            vp->setInt32(argsobj.initialLength());
    }
    return true;
}

static JSBool
StrictArgSetter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp)
{
    if (!obj->isStrictArguments())
        return true;

    Rooted<StrictArgumentsObject*> argsobj(cx, &obj->asStrictArguments());

    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj->initialLength()) {
            argsobj->setElement(arg, *vp);
            return true;
        }
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom));
    }

    /*
     * For simplicity we use delete/set to replace the property with one
     * backed by the default Object getter and setter. Note that we rely on
     * args_delProperty to clear the corresponding reserved slot so the GC can
     * collect its value.
     */
    RootedValue value(cx);
    return baseops::DeleteGeneric(cx, argsobj, id, value.address(), strict) &&
           baseops::SetPropertyHelper(cx, argsobj, id, 0, vp, strict);
}

static JSBool
strictargs_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags, JSObject **objp)
{
    *objp = NULL;

    Rooted<StrictArgumentsObject*> argsobj(cx, &obj->asStrictArguments());

    unsigned attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    PropertyOp getter = StrictArgGetter;
    StrictPropertyOp setter = StrictArgSetter;

    if (JSID_IS_INT(id)) {
        uint32_t arg = uint32_t(JSID_TO_INT(id));
        if (arg >= argsobj->initialLength() || argsobj->isElementDeleted(arg))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (argsobj->hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom) &&
            !JSID_IS_ATOM(id, cx->runtime->atomState.callerAtom)) {
            return true;
        }

        attrs = JSPROP_PERMANENT | JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED;
        getter = CastAsPropertyOp(argsobj->global().getThrowTypeError());
        setter = CastAsStrictPropertyOp(argsobj->global().getThrowTypeError());
    }

    Value undef = UndefinedValue();
    if (!baseops::DefineGeneric(cx, argsobj, id, &undef, getter, setter, attrs))
        return false;

    *objp = argsobj;
    return true;
}

static JSBool
strictargs_enumerate(JSContext *cx, HandleObject obj)
{
    Rooted<StrictArgumentsObject*> argsobj(cx, &obj->asStrictArguments());

    /*
     * Trigger reflection in strictargs_resolve using a series of
     * js_LookupProperty calls.
     */
    JSObject *pobj;
    JSProperty *prop;
    RootedId id(cx);

    // length
    id = NameToId(cx->runtime->atomState.lengthAtom);
    if (!baseops::LookupProperty(cx, argsobj, id, &pobj, &prop))
        return false;

    // callee
    id = NameToId(cx->runtime->atomState.calleeAtom);
    if (!baseops::LookupProperty(cx, argsobj, id, &pobj, &prop))
        return false;

    // caller
    id = NameToId(cx->runtime->atomState.callerAtom);
    if (!baseops::LookupProperty(cx, argsobj, id, &pobj, &prop))
        return false;

    for (uint32_t i = 0, argc = argsobj->initialLength(); i < argc; i++) {
        id = INT_TO_JSID(i);
        if (!baseops::LookupProperty(cx, argsobj, id, &pobj, &prop))
            return false;
    }

    return true;
}

void
ArgumentsObject::finalize(FreeOp *fop, JSObject *obj)
{
    fop->free_(reinterpret_cast<void *>(obj->asArguments().data()));
}

void
ArgumentsObject::trace(JSTracer *trc, JSObject *obj)
{
    ArgumentsObject &argsobj = obj->asArguments();
    ArgumentsData *data = argsobj.data();
    MarkValue(trc, &data->callee, js_callee_str);
    MarkValueRange(trc, data->numArgs, data->args, js_arguments_str);
    MarkScriptUnbarriered(trc, &data->script, "script");
}

/*
 * The classes below collaborate to lazily reflect and synchronize actual
 * argument values, argument count, and callee function object stored in a
 * StackFrame with their corresponding property values in the frame's
 * arguments object.
 */
Class js::NormalArgumentsObjectClass = {
    "Arguments",
    JSCLASS_NEW_RESOLVE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(NormalArgumentsObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object) |
    JSCLASS_FOR_OF_ITERATION,
    JS_PropertyStub,         /* addProperty */
    args_delProperty,
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    args_enumerate,
    reinterpret_cast<JSResolveOp>(args_resolve),
    JS_ConvertStub,
    ArgumentsObject::finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    ArgumentsObject::trace,
    {
        NULL,       /* equality    */
        NULL,       /* outerObject */
        NULL,       /* innerObject */
        JS_ElementIteratorStub,
        NULL,       /* unused      */
        false,      /* isWrappedNative */
    }
};

/*
 * Strict mode arguments is significantly less magical than non-strict mode
 * arguments, so it is represented by a different class while sharing some
 * functionality.
 */
Class js::StrictArgumentsObjectClass = {
    "Arguments",
    JSCLASS_NEW_RESOLVE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(StrictArgumentsObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object) |
    JSCLASS_FOR_OF_ITERATION,
    JS_PropertyStub,         /* addProperty */
    args_delProperty,
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    strictargs_enumerate,
    reinterpret_cast<JSResolveOp>(strictargs_resolve),
    JS_ConvertStub,
    ArgumentsObject::finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    ArgumentsObject::trace,
    {
        NULL,       /* equality    */
        NULL,       /* outerObject */
        NULL,       /* innerObject */
        JS_ElementIteratorStub,
        NULL,       /* unused      */
        false,      /* isWrappedNative */
    }
};

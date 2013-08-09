/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ArgumentsObject-inl.h"

#include "jsinfer.h"

#ifdef JS_ION
#include "ion/IonFrames.h"
#endif
#include "vm/GlobalObject.h"
#include "vm/Stack.h"

#include "jsobjinlines.h"

#include "gc/Barrier-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;

static void
CopyStackFrameArguments(const AbstractFramePtr frame, HeapValue *dst, unsigned totalArgs)
{
    JS_ASSERT_IF(frame.isStackFrame(), !frame.asStackFrame()->runningInJit());

    JS_ASSERT(Max(frame.numActualArgs(), frame.numFormalArgs()) == totalArgs);

    /* Copy arguments. */
    Value *src = frame.argv();
    Value *end = src + totalArgs;
    while (src != end)
        (dst++)->init(*src++);
}

/* static */ void
ArgumentsObject::MaybeForwardToCallObject(AbstractFramePtr frame, JSObject *obj,
                                          ArgumentsData *data)
{
    JSScript *script = frame.script();
    if (frame.fun()->isHeavyweight() && script->argsObjAliasesFormals()) {
        obj->initFixedSlot(MAYBE_CALL_SLOT, ObjectValue(frame.callObj()));
        for (AliasedFormalIter fi(script); fi; fi++)
            data->args[fi.frameIndex()] = MagicValue(JS_FORWARD_TO_CALL_OBJECT);
    }
}

#if defined(JS_ION)
/* static */ void
ArgumentsObject::MaybeForwardToCallObject(ion::IonJSFrameLayout *frame, HandleObject callObj,
                                          JSObject *obj, ArgumentsData *data)
{
    JSFunction *callee = ion::CalleeTokenToFunction(frame->calleeToken());
    JSScript *script = callee->nonLazyScript();
    if (callee->isHeavyweight() && script->argsObjAliasesFormals()) {
        JS_ASSERT(callObj && callObj->is<CallObject>());
        obj->initFixedSlot(MAYBE_CALL_SLOT, ObjectValue(*callObj.get()));
        for (AliasedFormalIter fi(script); fi; fi++)
            data->args[fi.frameIndex()] = MagicValue(JS_FORWARD_TO_CALL_OBJECT);
    }
}
#endif

struct CopyFrameArgs
{
    AbstractFramePtr frame_;

    CopyFrameArgs(AbstractFramePtr frame)
      : frame_(frame)
    { }

    void copyArgs(JSContext *, HeapValue *dst, unsigned totalArgs) const {
        CopyStackFrameArguments(frame_, dst, totalArgs);
    }

    /*
     * If a call object exists and the arguments object aliases formals, the
     * call object is the canonical location for formals.
     */
    void maybeForwardToCallObject(JSObject *obj, ArgumentsData *data) {
        ArgumentsObject::MaybeForwardToCallObject(frame_, obj, data);
    }
};

#if defined(JS_ION)
struct CopyIonJSFrameArgs
{
    ion::IonJSFrameLayout *frame_;
    HandleObject callObj_;

    CopyIonJSFrameArgs(ion::IonJSFrameLayout *frame, HandleObject callObj)
      : frame_(frame), callObj_(callObj)
    { }

    void copyArgs(JSContext *, HeapValue *dstBase, unsigned totalArgs) const {
        unsigned numActuals = frame_->numActualArgs();
        unsigned numFormals = ion::CalleeTokenToFunction(frame_->calleeToken())->nargs;
        JS_ASSERT(numActuals <= totalArgs);
        JS_ASSERT(numFormals <= totalArgs);
        JS_ASSERT(Max(numActuals, numFormals) == totalArgs);

        /* Copy all arguments. */
        Value *src = frame_->argv() + 1;  /* +1 to skip this. */
        Value *end = src + numActuals;
        HeapValue *dst = dstBase;
        while (src != end)
            (dst++)->init(*src++);

        if (numActuals < numFormals) {
            HeapValue *dstEnd = dstBase + totalArgs;
            while (dst != dstEnd)
                (dst++)->init(UndefinedValue());
        }
    }

    /*
     * If a call object exists and the arguments object aliases formals, the
     * call object is the canonical location for formals.
     */
    void maybeForwardToCallObject(JSObject *obj, ArgumentsData *data) {
        ArgumentsObject::MaybeForwardToCallObject(frame_, callObj_, obj, data);
    }
};
#endif

struct CopyScriptFrameIterArgs
{
    ScriptFrameIter &iter_;

    CopyScriptFrameIterArgs(ScriptFrameIter &iter)
      : iter_(iter)
    { }

    void copyArgs(JSContext *cx, HeapValue *dstBase, unsigned totalArgs) const {
        if (!iter_.isJit()) {
            CopyStackFrameArguments(iter_.abstractFramePtr(), dstBase, totalArgs);
            return;
        }

        /* Copy actual arguments. */
        iter_.ionForEachCanonicalActualArg(cx, CopyToHeap(dstBase));

        /* Define formals which are not part of the actuals. */
        unsigned numActuals = iter_.numActualArgs();
        unsigned numFormals = iter_.callee()->nargs;
        JS_ASSERT(numActuals <= totalArgs);
        JS_ASSERT(numFormals <= totalArgs);
        JS_ASSERT(Max(numActuals, numFormals) == totalArgs);

        if (numActuals < numFormals) {
            HeapValue *dst = dstBase + numActuals, *dstEnd = dstBase + totalArgs;
            while (dst != dstEnd)
                (dst++)->init(UndefinedValue());
        }
    }

    /*
     * Ion frames are copying every argument onto the stack, other locations are
     * invalid.
     */
    void maybeForwardToCallObject(JSObject *obj, ArgumentsData *data) {
        if (!iter_.isJit())
            ArgumentsObject::MaybeForwardToCallObject(iter_.abstractFramePtr(), obj, data);
    }
};

template <typename CopyArgs>
/* static */ ArgumentsObject *
ArgumentsObject::create(JSContext *cx, HandleScript script, HandleFunction callee, unsigned numActuals,
                        CopyArgs &copy)
{
    RootedObject proto(cx, callee->global().getOrCreateObjectPrototype(cx));
    if (!proto)
        return NULL;

    bool strict = callee->strict();
    Class *clasp = strict ? &StrictArgumentsObject::class_ : &NormalArgumentsObject::class_;

    RootedTypeObject type(cx, cx->getNewType(clasp, proto.get()));
    if (!type)
        return NULL;

    JSObject *metadata = NULL;
    if (!NewObjectMetadata(cx, &metadata))
        return NULL;

    RootedShape shape(cx, EmptyShape::getInitialShape(cx, clasp, TaggedProto(proto),
                                                      proto->getParent(), metadata, FINALIZE_KIND,
                                                      BaseShape::INDEXED));
    if (!shape)
        return NULL;

    unsigned numFormals = callee->nargs;
    unsigned numDeletedWords = NumWordsForBitArrayOfLength(numActuals);
    unsigned numArgs = Max(numActuals, numFormals);
    unsigned numBytes = offsetof(ArgumentsData, args) +
                        numDeletedWords * sizeof(size_t) +
                        numArgs * sizeof(Value);

    ArgumentsData *data = (ArgumentsData *)cx->malloc_(numBytes);
    if (!data)
        return NULL;

    data->numArgs = numArgs;
    data->callee.init(ObjectValue(*callee.get()));
    data->script = script;

    /* Copy [0, numArgs) into data->slots. */
    HeapValue *dst = data->args, *dstEnd = data->args + numArgs;
    copy.copyArgs(cx, dst, numArgs);

    data->deletedBits = reinterpret_cast<size_t *>(dstEnd);
    ClearAllBitArrayElements(data->deletedBits, numDeletedWords);

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, GetInitialHeap(GenericObject, clasp),
                                     shape, type);
    if (!obj) {
        js_free(data);
        return NULL;
    }

    obj->initFixedSlot(INITIAL_LENGTH_SLOT, Int32Value(numActuals << PACKED_BITS_COUNT));
    obj->initFixedSlot(DATA_SLOT, PrivateValue(data));

    copy.maybeForwardToCallObject(obj, data);

    ArgumentsObject &argsobj = obj->as<ArgumentsObject>();
    JS_ASSERT(argsobj.initialLength() == numActuals);
    JS_ASSERT(!argsobj.hasOverriddenLength());
    return &argsobj;
}

ArgumentsObject *
ArgumentsObject::createExpected(JSContext *cx, AbstractFramePtr frame)
{
    JS_ASSERT(frame.script()->needsArgsObj());
    RootedScript script(cx, frame.script());
    RootedFunction callee(cx, frame.callee());
    CopyFrameArgs copy(frame);
    ArgumentsObject *argsobj = create(cx, script, callee, frame.numActualArgs(), copy);
    if (!argsobj)
        return NULL;

    frame.initArgsObj(*argsobj);
    return argsobj;
}

ArgumentsObject *
ArgumentsObject::createUnexpected(JSContext *cx, ScriptFrameIter &iter)
{
    RootedScript script(cx, iter.script());
    RootedFunction callee(cx, iter.callee());
    CopyScriptFrameIterArgs copy(iter);
    return create(cx, script, callee, iter.numActualArgs(), copy);
}

ArgumentsObject *
ArgumentsObject::createUnexpected(JSContext *cx, AbstractFramePtr frame)
{
    RootedScript script(cx, frame.script());
    RootedFunction callee(cx, frame.callee());
    CopyFrameArgs copy(frame);
    return create(cx, script, callee, frame.numActualArgs(), copy);
}

#if defined(JS_ION)
ArgumentsObject *
ArgumentsObject::createForIon(JSContext *cx, ion::IonJSFrameLayout *frame, HandleObject scopeChain)
{
    ion::CalleeToken token = frame->calleeToken();
    JS_ASSERT(ion::CalleeTokenIsFunction(token));
    RootedScript script(cx, ion::ScriptFromCalleeToken(token));
    RootedFunction callee(cx, ion::CalleeTokenToFunction(token));
    RootedObject callObj(cx, scopeChain->is<CallObject>() ? scopeChain.get() : NULL);
    CopyIonJSFrameArgs copy(frame, callObj);
    return create(cx, script, callee, frame->numActualArgs(), copy);
}
#endif

static JSBool
args_delProperty(JSContext *cx, HandleObject obj, HandleId id, JSBool *succeeded)
{
    ArgumentsObject &argsobj = obj->as<ArgumentsObject>();
    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            argsobj.markElementDeleted(arg);
    } else if (JSID_IS_ATOM(id, cx->names().length)) {
        argsobj.markLengthOverridden();
    } else if (JSID_IS_ATOM(id, cx->names().callee)) {
        argsobj.as<NormalArgumentsObject>().clearCallee();
    }
    *succeeded = true;
    return true;
}

static JSBool
ArgGetter(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    if (!obj->is<NormalArgumentsObject>())
        return true;

    NormalArgumentsObject &argsobj = obj->as<NormalArgumentsObject>();
    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            vp.set(argsobj.element(arg));
    } else if (JSID_IS_ATOM(id, cx->names().length)) {
        if (!argsobj.hasOverriddenLength())
            vp.setInt32(argsobj.initialLength());
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->names().callee));
        if (!argsobj.callee().isMagic(JS_OVERWRITTEN_CALLEE))
            vp.set(argsobj.callee());
    }
    return true;
}

static JSBool
ArgSetter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, MutableHandleValue vp)
{
    if (!obj->is<NormalArgumentsObject>())
        return true;

    unsigned attrs;
    if (!baseops::GetAttributes(cx, obj, id, &attrs))
        return false;
    JS_ASSERT(!(attrs & JSPROP_READONLY));
    attrs &= (JSPROP_ENUMERATE | JSPROP_PERMANENT); /* only valid attributes */

    NormalArgumentsObject &argsobj = obj->as<NormalArgumentsObject>();
    RootedScript script(cx, argsobj.containingScript());

    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg)) {
            argsobj.setElement(cx, arg, vp);
            if (arg < script->function()->nargs)
                types::TypeScript::SetArgument(cx, script, arg, vp);
            return true;
        }
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->names().length) || JSID_IS_ATOM(id, cx->names().callee));
    }

    /*
     * For simplicity we use delete/define to replace the property with one
     * backed by the default Object getter and setter. Note that we rely on
     * args_delProperty to clear the corresponding reserved slot so the GC can
     * collect its value. Note also that we must define the property instead
     * of setting it in case the user has changed the prototype to an object
     * that has a setter for this id.
     */
    JSBool succeeded;
    return baseops::DeleteGeneric(cx, obj, id, &succeeded) &&
           baseops::DefineGeneric(cx, obj, id, vp, NULL, NULL, attrs);
}

static JSBool
args_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
             MutableHandleObject objp)
{
    objp.set(NULL);

    Rooted<NormalArgumentsObject*> argsobj(cx, &obj->as<NormalArgumentsObject>());

    unsigned attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    if (JSID_IS_INT(id)) {
        uint32_t arg = uint32_t(JSID_TO_INT(id));
        if (arg >= argsobj->initialLength() || argsobj->isElementDeleted(arg))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->names().length)) {
        if (argsobj->hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->names().callee))
            return true;

        if (argsobj->callee().isMagic(JS_OVERWRITTEN_CALLEE))
            return true;
    }

    RootedValue undef(cx, UndefinedValue());
    if (!baseops::DefineGeneric(cx, argsobj, id, undef, ArgGetter, ArgSetter, attrs))
        return false;

    objp.set(argsobj);
    return true;
}

static JSBool
args_enumerate(JSContext *cx, HandleObject obj)
{
    Rooted<NormalArgumentsObject*> argsobj(cx, &obj->as<NormalArgumentsObject>());
    RootedId id(cx);

    /*
     * Trigger reflection in args_resolve using a series of js_LookupProperty
     * calls.
     */
    int argc = int(argsobj->initialLength());
    for (int i = -2; i != argc; i++) {
        id = (i == -2)
             ? NameToId(cx->names().length)
             : (i == -1)
             ? NameToId(cx->names().callee)
             : INT_TO_JSID(i);

        RootedObject pobj(cx);
        RootedShape prop(cx);
        if (!baseops::LookupProperty<CanGC>(cx, argsobj, id, &pobj, &prop))
            return false;
    }
    return true;
}

static JSBool
StrictArgGetter(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    if (!obj->is<StrictArgumentsObject>())
        return true;

    StrictArgumentsObject &argsobj = obj->as<StrictArgumentsObject>();

    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            vp.set(argsobj.element(arg));
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->names().length));
        if (!argsobj.hasOverriddenLength())
            vp.setInt32(argsobj.initialLength());
    }
    return true;
}

static JSBool
StrictArgSetter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, MutableHandleValue vp)
{
    if (!obj->is<StrictArgumentsObject>())
        return true;

    unsigned attrs;
    if (!baseops::GetAttributes(cx, obj, id, &attrs))
        return false;
    JS_ASSERT(!(attrs & JSPROP_READONLY));
    attrs &= (JSPROP_ENUMERATE | JSPROP_PERMANENT); /* only valid attributes */

    Rooted<StrictArgumentsObject*> argsobj(cx, &obj->as<StrictArgumentsObject>());

    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj->initialLength()) {
            argsobj->setElement(cx, arg, vp);
            return true;
        }
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->names().length));
    }

    /*
     * For simplicity we use delete/define to replace the property with one
     * backed by the default Object getter and setter. Note that we rely on
     * args_delProperty to clear the corresponding reserved slot so the GC can
     * collect its value.
     */
    JSBool succeeded;
    return baseops::DeleteGeneric(cx, argsobj, id, &succeeded) &&
           baseops::DefineGeneric(cx, argsobj, id, vp, NULL, NULL, attrs);
}

static JSBool
strictargs_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                   MutableHandleObject objp)
{
    objp.set(NULL);

    Rooted<StrictArgumentsObject*> argsobj(cx, &obj->as<StrictArgumentsObject>());

    unsigned attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    PropertyOp getter = StrictArgGetter;
    StrictPropertyOp setter = StrictArgSetter;

    if (JSID_IS_INT(id)) {
        uint32_t arg = uint32_t(JSID_TO_INT(id));
        if (arg >= argsobj->initialLength() || argsobj->isElementDeleted(arg))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->names().length)) {
        if (argsobj->hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->names().callee) && !JSID_IS_ATOM(id, cx->names().caller))
            return true;

        attrs = JSPROP_PERMANENT | JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED;
        getter = CastAsPropertyOp(argsobj->global().getThrowTypeError());
        setter = CastAsStrictPropertyOp(argsobj->global().getThrowTypeError());
    }

    RootedValue undef(cx, UndefinedValue());
    if (!baseops::DefineGeneric(cx, argsobj, id, undef, getter, setter, attrs))
        return false;

    objp.set(argsobj);
    return true;
}

static JSBool
strictargs_enumerate(JSContext *cx, HandleObject obj)
{
    Rooted<StrictArgumentsObject*> argsobj(cx, &obj->as<StrictArgumentsObject>());

    /*
     * Trigger reflection in strictargs_resolve using a series of
     * js_LookupProperty calls.
     */
    RootedObject pobj(cx);
    RootedShape prop(cx);
    RootedId id(cx);

    // length
    id = NameToId(cx->names().length);
    if (!baseops::LookupProperty<CanGC>(cx, argsobj, id, &pobj, &prop))
        return false;

    // callee
    id = NameToId(cx->names().callee);
    if (!baseops::LookupProperty<CanGC>(cx, argsobj, id, &pobj, &prop))
        return false;

    // caller
    id = NameToId(cx->names().caller);
    if (!baseops::LookupProperty<CanGC>(cx, argsobj, id, &pobj, &prop))
        return false;

    for (uint32_t i = 0, argc = argsobj->initialLength(); i < argc; i++) {
        id = INT_TO_JSID(i);
        if (!baseops::LookupProperty<CanGC>(cx, argsobj, id, &pobj, &prop))
            return false;
    }

    return true;
}

void
ArgumentsObject::finalize(FreeOp *fop, JSObject *obj)
{
    fop->free_(reinterpret_cast<void *>(obj->as<ArgumentsObject>().data()));
}

void
ArgumentsObject::trace(JSTracer *trc, JSObject *obj)
{
    ArgumentsObject &argsobj = obj->as<ArgumentsObject>();
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
Class NormalArgumentsObject::class_ = {
    "Arguments",
    JSCLASS_NEW_RESOLVE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(NormalArgumentsObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object) | JSCLASS_BACKGROUND_FINALIZE,
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
    NULL,                    /* hasInstance */
    NULL,                    /* construct   */
    ArgumentsObject::trace,
    {
        NULL,       /* outerObject */
        NULL,       /* innerObject */
        NULL,       /* iteratorObject  */
        false,      /* isWrappedNative */
    }
};

/*
 * Strict mode arguments is significantly less magical than non-strict mode
 * arguments, so it is represented by a different class while sharing some
 * functionality.
 */
Class StrictArgumentsObject::class_ = {
    "Arguments",
    JSCLASS_NEW_RESOLVE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(StrictArgumentsObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object) | JSCLASS_BACKGROUND_FINALIZE,
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
    NULL,                    /* hasInstance */
    NULL,                    /* construct   */
    ArgumentsObject::trace,
    {
        NULL,       /* outerObject */
        NULL,       /* innerObject */
        NULL,       /* iteratorObject  */
        false,      /* isWrappedNative */
    }
};

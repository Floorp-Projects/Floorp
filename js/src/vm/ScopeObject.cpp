/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"
#include "jsiter.h"
#include "jsscope.h"

#include "GlobalObject.h"
#include "ScopeObject.h"
#include "Xdr.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "ScopeObject-inl.h"

using namespace js;
using namespace js::types;

/*****************************************************************************/

void
js_PutCallObject(StackFrame *fp, CallObject &callobj)
{
    JS_ASSERT(callobj.maybeStackFrame() == fp);
    JS_ASSERT_IF(fp->isEvalFrame(), fp->isStrictEvalFrame());
    JS_ASSERT(fp->isEvalFrame() == callobj.isForEval());

    JSScript *script = fp->script();
    Bindings &bindings = script->bindings;

    if (callobj.isForEval()) {
        JS_ASSERT(script->strictModeCode);
        JS_ASSERT(bindings.numArgs() == 0);

        /* This could be optimized as below, but keep it simple for now. */
        callobj.copyValues(0, NULL, bindings.numVars(), fp->slots());
    } else {
        JSFunction *fun = fp->fun();
        JS_ASSERT(script == callobj.getCalleeFunction()->script());
        JS_ASSERT(script == fun->script());

        unsigned n = bindings.count();
        if (n > 0) {
            uint32_t nvars = bindings.numVars();
            uint32_t nargs = bindings.numArgs();
            JS_ASSERT(fun->nargs == nargs);
            JS_ASSERT(nvars + nargs == n);

            JSScript *script = fun->script();
            if (script->bindingsAccessedDynamically
#ifdef JS_METHODJIT
                || script->debugMode
#endif
                ) {
                callobj.copyValues(nargs, fp->formalArgs(), nvars, fp->slots());
            } else {
                /*
                 * For each arg & var that is closed over, copy it from the stack
                 * into the call object. We use initArg/VarUnchecked because,
                 * when you call a getter on a call object, js_NativeGetInline
                 * caches the return value in the slot, so we can't assert that
                 * it's undefined.
                 */
                uint32_t nclosed = script->numClosedArgs();
                for (uint32_t i = 0; i < nclosed; i++) {
                    uint32_t e = script->getClosedArg(i);
#ifdef JS_GC_ZEAL
                    callobj.setArg(e, fp->formalArg(e));
#else
                    callobj.initArgUnchecked(e, fp->formalArg(e));
#endif
                }

                nclosed = script->numClosedVars();
                for (uint32_t i = 0; i < nclosed; i++) {
                    uint32_t e = script->getClosedVar(i);
#ifdef JS_GC_ZEAL
                    callobj.setVar(e, fp->slots()[e]);
#else
                    callobj.initVarUnchecked(e, fp->slots()[e]);
#endif
                }
            }

            /*
             * Update the args and vars for the active call if this is an outer
             * function in a script nesting.
             */
            types::TypeScriptNesting *nesting = script->nesting();
            if (nesting && script->isOuterFunction) {
                nesting->argArray = callobj.argArray();
                nesting->varArray = callobj.varArray();
            }
        }

        /* Clear private pointers to fp, which is about to go away. */
        if (js_IsNamedLambda(fun)) {
            JSObject &env = callobj.enclosingScope();
            JS_ASSERT(env.asDeclEnv().maybeStackFrame() == fp);
            env.setPrivate(NULL);
        }
    }

    callobj.setStackFrame(NULL);
}

/*
 * Construct a call object for the given bindings.  If this is a call object
 * for a function invocation, callee should be the function being called.
 * Otherwise it must be a call object for eval of strict mode code, and callee
 * must be null.
 */
CallObject *
CallObject::create(JSContext *cx, JSScript *script, HandleObject enclosing, HandleObject callee)
{
    RootedVarShape shape(cx);
    shape = script->bindings.callObjectShape(cx);
    if (shape == NULL)
        return NULL;

    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots() + 1);

    RootedVarTypeObject type(cx);
    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    HeapSlot *slots;
    if (!PreallocateObjectDynamicSlots(cx, shape, &slots))
        return NULL;

    RootedVarObject obj(cx, JSObject::create(cx, kind, shape, type, slots));
    if (!obj)
        return NULL;

    /*
     * Update the parent for bindings associated with non-compileAndGo scripts,
     * whose call objects do not have a consistent global variable and need
     * to be updated dynamically.
     */
    if (&enclosing->global() != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        if (!JSObject::setParent(cx, obj, RootedVarObject(cx, &enclosing->global())))
            return NULL;
    }

#ifdef DEBUG
    JS_ASSERT(!obj->inDictionaryMode());
    for (Shape::Range r = obj->lastProperty(); !r.empty(); r.popFront()) {
        const Shape &s = r.front();
        if (s.hasSlot()) {
            JS_ASSERT(s.slot() + 1 == obj->slotSpan());
            break;
        }
    }
#endif

    if (!obj->asScope().setEnclosingScope(cx, enclosing))
        return NULL;

    JS_ASSERT_IF(callee, callee->isFunction());
    obj->initFixedSlot(CALLEE_SLOT, ObjectOrNullValue(callee));

    /*
     * If |bindings| is for a function that has extensible parents, that means
     * its Call should have its own shape; see BaseShape::extensibleParents.
     */
    if (obj->lastProperty()->extensibleParents()) {
        if (!obj->generateOwnShape(cx))
            return NULL;
    }

    return &obj->asCall();
}

CallObject *
CallObject::createForFunction(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(fp->isNonEvalFunctionFrame());
    JS_ASSERT(!fp->hasCallObj());

    RootedVarObject scopeChain(cx, fp->scopeChain());

    /*
     * For a named function expression Call's parent points to an environment
     * object holding function's name.
     */
    if (js_IsNamedLambda(fp->fun())) {
        scopeChain = DeclEnvObject::create(cx, fp);
        if (!scopeChain)
            return NULL;
    }

    CallObject *callobj = create(cx, fp->script(), scopeChain, RootedVarObject(cx, &fp->callee()));
    if (!callobj)
        return NULL;

    callobj->setStackFrame(fp);
    return callobj;
}

CallObject *
CallObject::createForStrictEval(JSContext *cx, StackFrame *fp)
{
    CallObject *callobj = create(cx, fp->script(), fp->scopeChain(), RootedVarObject(cx));
    if (!callobj)
        return NULL;

    callobj->setStackFrame(fp);
    fp->initScopeChain(*callobj);
    return callobj;
}

JSBool
CallObject::getArgOp(JSContext *cx, HandleObject obj, HandleId id, Value *vp)
{
    CallObject &callobj = obj->asCall();

    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    DebugOnly<JSScript *> script = callobj.getCalleeFunction()->script();
    JS_ASSERT_IF(!cx->okToAccessUnaliasedBindings, script->formalLivesInCallObject(i));

    if (StackFrame *fp = callobj.maybeStackFrame())
        *vp = fp->formalArg(i);
    else
        *vp = callobj.arg(i);
    return true;
}

JSBool
CallObject::setArgOp(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp)
{
    CallObject &callobj = obj->asCall();

    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    JSScript *script = callobj.getCalleeFunction()->script();
    JS_ASSERT_IF(!cx->okToAccessUnaliasedBindings, script->formalLivesInCallObject(i));

    if (StackFrame *fp = callobj.maybeStackFrame())
        fp->formalArg(i) = *vp;
    else
        callobj.setArg(i, *vp);

    if (!script->ensureHasTypes(cx))
        return false;

    TypeScript::SetArgument(cx, script, i, *vp);

    return true;
}

JSBool
CallObject::getVarOp(JSContext *cx, HandleObject obj, HandleId id, Value *vp)
{
    CallObject &callobj = obj->asCall();

    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    DebugOnly<JSScript *> script = callobj.getCalleeFunction()->script();
    JS_ASSERT_IF(!cx->okToAccessUnaliasedBindings, script->varIsAliased(i));

    if (StackFrame *fp = callobj.maybeStackFrame())
        *vp = fp->varSlot(i);
    else
        *vp = callobj.var(i);

    JS_ASSERT(!vp->isMagic(JS_OPTIMIZED_ARGUMENTS));
    return true;
}

JSBool
CallObject::setVarOp(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp)
{
    CallObject &callobj = obj->asCall();

    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    JSScript *script = callobj.getCalleeFunction()->script();
    JS_ASSERT_IF(!cx->okToAccessUnaliasedBindings, script->varIsAliased(i));

    if (StackFrame *fp = callobj.maybeStackFrame())
        fp->varSlot(i) = *vp;
    else
        callobj.setVar(i, *vp);

    if (!script->ensureHasTypes(cx))
        return false;

    TypeScript::SetLocal(cx, script, i, *vp);
    return true;
}

bool
CallObject::containsVarOrArg(PropertyName *name, Value *vp, JSContext *cx)
{
    jsid id = NameToId(name);
    const Shape *shape = nativeLookup(cx, id);
    if (!shape)
        return false;

    PropertyOp op = shape->getterOp();
    if (op != getVarOp && op != getArgOp)
        return false;

    JS_ALWAYS_TRUE(op(cx, RootedVarObject(cx, this), RootedVarId(cx, INT_TO_JSID(shape->shortid())), vp));
    return true;
}

static void
call_trace(JSTracer *trc, JSObject *obj)
{
    JS_ASSERT(obj->isCall());

    /* Mark any generator frame, as for arguments objects. */
#if JS_HAS_GENERATORS
    StackFrame *fp = (StackFrame *) obj->getPrivate();
    if (fp && fp->isFloatingGenerator())
        MarkObject(trc, &js_FloatingFrameToGenerator(fp)->obj, "generator object");
#endif
}

JS_PUBLIC_DATA(Class) js::CallClass = {
    "Call",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_IS_ANONYMOUS |
    JSCLASS_HAS_RESERVED_SLOTS(CallObject::RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    NULL,                    /* convert: Leave it NULL so we notice if calls ever escape */
    NULL,                    /* finalize */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    call_trace
};

Class js::DeclEnvClass = {
    js_Object_str,
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(DeclEnvObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

DeclEnvObject *
DeclEnvObject::create(JSContext *cx, StackFrame *fp)
{
    RootedVarTypeObject type(cx);
    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    RootedVarShape emptyDeclEnvShape(cx);
    emptyDeclEnvShape = EmptyShape::getInitialShape(cx, &DeclEnvClass, NULL,
                                                    &fp->global(), FINALIZE_KIND);
    if (!emptyDeclEnvShape)
        return NULL;

    RootedVarObject obj(cx, JSObject::create(cx, FINALIZE_KIND, emptyDeclEnvShape, type, NULL));
    if (!obj)
        return NULL;

    obj->setPrivate(fp);
    if (!obj->asScope().setEnclosingScope(cx, fp->scopeChain()))
        return NULL;


    if (!DefineNativeProperty(cx, obj, RootedVarId(cx, AtomToId(fp->fun()->atom)),
                              ObjectValue(fp->callee()), NULL, NULL,
                              JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY,
                              0, 0)) {
        return NULL;
    }

    return &obj->asDeclEnv();
}

WithObject *
WithObject::create(JSContext *cx, HandleObject proto, HandleObject enclosing, uint32_t depth)
{
    RootedVarTypeObject type(cx);
    type = proto->getNewType(cx);
    if (!type)
        return NULL;

    RootedVarShape emptyWithShape(cx);
    emptyWithShape = EmptyShape::getInitialShape(cx, &WithClass, proto,
                                                 &enclosing->global(), FINALIZE_KIND);
    if (!emptyWithShape)
        return NULL;

    RootedVarObject obj(cx, JSObject::create(cx, FINALIZE_KIND, emptyWithShape, type, NULL));
    if (!obj)
        return NULL;

    if (!obj->asScope().setEnclosingScope(cx, enclosing))
        return NULL;

    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(depth));

    JSObject *thisp = proto->thisObject(cx);
    if (!thisp)
        return NULL;

    obj->setFixedSlot(THIS_SLOT, ObjectValue(*thisp));

    return &obj->asWith();
}

static JSBool
with_LookupGeneric(JSContext *cx, HandleObject obj, HandleId id, JSObject **objp, JSProperty **propp)
{
    /* Fixes bug 463997 */
    unsigned flags = cx->resolveFlags;
    if (flags == RESOLVE_INFER)
        flags = js_InferFlags(cx, flags);
    flags |= JSRESOLVE_WITH;
    JSAutoResolveFlags rf(cx, flags);
    return obj->asWith().object().lookupGeneric(cx, id, objp, propp);
}

static JSBool
with_LookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, JSObject **objp, JSProperty **propp)
{
    return with_LookupGeneric(cx, obj, RootedVarId(cx, NameToId(name)), objp, propp);
}

static JSBool
with_LookupElement(JSContext *cx, HandleObject obj, uint32_t index, JSObject **objp,
                   JSProperty **propp)
{
    RootedVarId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
with_LookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, JSObject **objp, JSProperty **propp)
{
    return with_LookupGeneric(cx, obj, RootedVarId(cx, SPECIALID_TO_JSID(sid)), objp, propp);
}

static JSBool
with_GetGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id, Value *vp)
{
    return obj->asWith().object().getGeneric(cx, id, vp);
}

static JSBool
with_GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name, Value *vp)
{
    return with_GetGeneric(cx, obj, receiver, RootedVarId(cx, NameToId(name)), vp);
}

static JSBool
with_GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index, Value *vp)
{
    RootedVarId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
with_GetSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid, Value *vp)
{
    return with_GetGeneric(cx, obj, receiver, RootedVarId(cx, SPECIALID_TO_JSID(sid)), vp);
}

static JSBool
with_SetGeneric(JSContext *cx, HandleObject obj, HandleId id, Value *vp, JSBool strict)
{
    return obj->asWith().object().setGeneric(cx, id, vp, strict);
}

static JSBool
with_SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *vp, JSBool strict)
{
    return obj->asWith().object().setProperty(cx, name, vp, strict);
}

static JSBool
with_SetElement(JSContext *cx, HandleObject obj, uint32_t index, Value *vp, JSBool strict)
{
    return obj->asWith().object().setElement(cx, index, vp, strict);
}

static JSBool
with_SetSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *vp, JSBool strict)
{
    return obj->asWith().object().setSpecial(cx, sid, vp, strict);
}

static JSBool
with_GetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    return obj->asWith().object().getGenericAttributes(cx, id, attrsp);
}

static JSBool
with_GetPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    return obj->asWith().object().getPropertyAttributes(cx, name, attrsp);
}

static JSBool
with_GetElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    return obj->asWith().object().getElementAttributes(cx, index, attrsp);
}

static JSBool
with_GetSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    return obj->asWith().object().getSpecialAttributes(cx, sid, attrsp);
}

static JSBool
with_SetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    return obj->asWith().object().setGenericAttributes(cx, id, attrsp);
}

static JSBool
with_SetPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    return obj->asWith().object().setPropertyAttributes(cx, name, attrsp);
}

static JSBool
with_SetElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    return obj->asWith().object().setElementAttributes(cx, index, attrsp);
}

static JSBool
with_SetSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    return obj->asWith().object().setSpecialAttributes(cx, sid, attrsp);
}

static JSBool
with_DeleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteProperty(cx, name, rval, strict);
}

static JSBool
with_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteElement(cx, index, rval, strict);
}

static JSBool
with_DeleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteSpecial(cx, sid, rval, strict);
}

static JSBool
with_Enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
               Value *statep, jsid *idp)
{
    return obj->asWith().object().enumerate(cx, enum_op, statep, idp);
}

static JSType
with_TypeOf(JSContext *cx, HandleObject obj)
{
    return JSTYPE_OBJECT;
}

static JSObject *
with_ThisObject(JSContext *cx, HandleObject obj)
{
    return &obj->asWith().withThis();
}

Class js::WithClass = {
    "With",
    JSCLASS_HAS_RESERVED_SLOTS(WithObject::RESERVED_SLOTS) |
    JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* finalize */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    NULL,                    /* trace       */
    JS_NULL_CLASS_EXT,
    {
        with_LookupGeneric,
        with_LookupProperty,
        with_LookupElement,
        with_LookupSpecial,
        NULL,             /* defineGeneric */
        NULL,             /* defineProperty */
        NULL,             /* defineElement */
        NULL,             /* defineSpecial */
        with_GetGeneric,
        with_GetProperty,
        with_GetElement,
        NULL,             /* getElementIfPresent */
        with_GetSpecial,
        with_SetGeneric,
        with_SetProperty,
        with_SetElement,
        with_SetSpecial,
        with_GetGenericAttributes,
        with_GetPropertyAttributes,
        with_GetElementAttributes,
        with_GetSpecialAttributes,
        with_SetGenericAttributes,
        with_SetPropertyAttributes,
        with_SetElementAttributes,
        with_SetSpecialAttributes,
        with_DeleteProperty,
        with_DeleteElement,
        with_DeleteSpecial,
        with_Enumerate,
        with_TypeOf,
        with_ThisObject,
        NULL,             /* clear */
    }
};

/*****************************************************************************/

ClonedBlockObject *
ClonedBlockObject::create(JSContext *cx, Handle<StaticBlockObject *> block, StackFrame *fp)
{
    RootedVarTypeObject type(cx);
    type = block->getNewType(cx);
    if (!type)
        return NULL;

    HeapSlot *slots;
    if (!PreallocateObjectDynamicSlots(cx, block->lastProperty(), &slots))
        return NULL;

    RootedVarShape shape(cx);
    shape = block->lastProperty();

    RootedVarObject obj(cx, JSObject::create(cx, FINALIZE_KIND, shape, type, slots));
    if (!obj)
        return NULL;

    /* Set the parent if necessary, as for call objects. */
    if (&fp->global() != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        if (!JSObject::setParent(cx, obj, RootedVarObject(cx, &fp->global())))
            return NULL;
    }

    JS_ASSERT(!obj->inDictionaryMode());
    JS_ASSERT(obj->slotSpan() >= block->slotCount() + RESERVED_SLOTS);

    obj->setReservedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*fp->scopeChain()));
    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(block->stackDepth()));
    obj->setPrivate(js_FloatingFrameIfGenerator(cx, fp));

    if (obj->lastProperty()->extensibleParents() && !obj->generateOwnShape(cx))
        return NULL;

    return &obj->asClonedBlock();
}

void
ClonedBlockObject::put(StackFrame *fp)
{
    uint32_t count = slotCount();
    uint32_t depth = stackDepth();

    /* See comments in CheckDestructuring in frontend/Parser.cpp. */
    JS_ASSERT(count >= 1);

    copySlotRange(RESERVED_SLOTS, fp->base() + depth, count);

    /* We must clear the private slot even with errors. */
    setPrivate(NULL);
}

static JSBool
block_getProperty(JSContext *cx, HandleObject obj, HandleId id, Value *vp)
{
    /*
     * Block objects are never exposed to script, and the engine handles them
     * with care. So unlike other getters, this one can assert (rather than
     * check) certain invariants about obj.
     */
    ClonedBlockObject &block = obj->asClonedBlock();
    unsigned index = (unsigned) JSID_TO_INT(id);

    JS_ASSERT_IF(!block.compartment()->debugMode(), block.staticBlock().isAliased(index));

    if (StackFrame *fp = block.maybeStackFrame()) {
        fp = js_LiveFrameIfGenerator(fp);
        index += fp->numFixed() + block.stackDepth();
        JS_ASSERT(index < fp->numSlots());
        *vp = fp->slots()[index];
        return true;
    }

    /* Values are in slots immediately following the class-reserved ones. */
    JS_ASSERT(block.closedSlot(index) == *vp);
    return true;
}

static JSBool
block_setProperty(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp)
{
    ClonedBlockObject &block = obj->asClonedBlock();
    unsigned index = (unsigned) JSID_TO_INT(id);

    JS_ASSERT_IF(!block.compartment()->debugMode(), block.staticBlock().isAliased(index));

    if (StackFrame *fp = block.maybeStackFrame()) {
        fp = js_LiveFrameIfGenerator(fp);
        index += fp->numFixed() + block.stackDepth();
        JS_ASSERT(index < fp->numSlots());
        fp->slots()[index] = *vp;
        return true;
    }

    /*
     * The value in *vp will be written back to the slot in obj that was
     * allocated when this let binding was defined.
     */
    return true;
}

bool
ClonedBlockObject::containsVar(PropertyName *name, Value *vp, JSContext *cx)
{
    RootedVarObject self(cx, this);

    const Shape *shape = nativeLookup(cx, NameToId(name));
    if (!shape)
        return false;

    JS_ASSERT(shape->getterOp() == block_getProperty);
    JS_ALWAYS_TRUE(block_getProperty(cx, self, RootedVarId(cx, INT_TO_JSID(shape->shortid())), vp));
    return true;
}

StaticBlockObject *
StaticBlockObject::create(JSContext *cx)
{
    RootedVarTypeObject type(cx);
    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    RootedVarShape emptyBlockShape(cx);
    emptyBlockShape = EmptyShape::getInitialShape(cx, &BlockClass, NULL, NULL, FINALIZE_KIND);
    if (!emptyBlockShape)
        return NULL;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyBlockShape, type, NULL);
    if (!obj)
        return NULL;

    obj->setPrivate(NULL);
    return &obj->asStaticBlock();
}

const Shape *
StaticBlockObject::addVar(JSContext *cx, jsid id, int index, bool *redeclared)
{
    JS_ASSERT(JSID_IS_ATOM(id) || (JSID_IS_INT(id) && JSID_TO_INT(id) == index));

    *redeclared = false;

    /* Inline JSObject::addProperty in order to trap the redefinition case. */
    Shape **spp;
    if (Shape::search(cx, lastProperty(), id, &spp, true)) {
        *redeclared = true;
        return NULL;
    }

    /*
     * Don't convert this object to dictionary mode so that we can clone the
     * block's shape later.
     */
    uint32_t slot = JSSLOT_FREE(&BlockClass) + index;
    return addPropertyInternal(cx, id, block_getProperty, block_setProperty,
                               slot, JSPROP_ENUMERATE | JSPROP_PERMANENT,
                               Shape::HAS_SHORTID, index, spp,
                               /* allowDictionary = */ false);
}

static void
block_trace(JSTracer *trc, JSObject *obj)
{
    if (obj->isStaticBlock())
        return;

    /* XXX: this will be removed again with bug 659577. */
#if JS_HAS_GENERATORS
    StackFrame *fp = obj->asClonedBlock().maybeStackFrame();
    if (fp && fp->isFloatingGenerator())
        MarkObject(trc, &js_FloatingFrameToGenerator(fp)->obj, "generator object");
#endif
}

Class js::BlockClass = {
    "Block",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(BlockObject::RESERVED_SLOTS) |
    JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* finalize */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    block_trace
};

#define NO_PARENT_INDEX UINT32_MAX

/*
 * If there's a parent id, then get the parent out of our script's object
 * array. We know that we clone block objects in outer-to-inner order, which
 * means that getting the parent now will work.
 */
static uint32_t
FindObjectIndex(JSScript *script, StaticBlockObject *maybeBlock)
{
    if (!maybeBlock || !script->hasObjects())
        return NO_PARENT_INDEX;

    ObjectArray *objects = script->objects();
    HeapPtrObject *vector = objects->vector;
    unsigned length = objects->length;
    for (unsigned i = 0; i < length; ++i) {
        if (vector[i] == maybeBlock)
            return i;
    }

    return NO_PARENT_INDEX;
}

template<XDRMode mode>
bool
js::XDRStaticBlockObject(XDRState<mode> *xdr, JSScript *script, StaticBlockObject **objp)
{
    /* NB: Keep this in sync with CloneStaticBlockObject. */

    JSContext *cx = xdr->cx();

    StaticBlockObject *obj = NULL;
    uint32_t parentId = 0;
    uint32_t count = 0;
    uint32_t depthAndCount = 0;
    if (mode == XDR_ENCODE) {
        obj = *objp;
        parentId = FindObjectIndex(script, obj->enclosingBlock());
        uint32_t depth = obj->stackDepth();
        JS_ASSERT(depth <= UINT16_MAX);
        count = obj->slotCount();
        JS_ASSERT(count <= UINT16_MAX);
        depthAndCount = (depth << 16) | uint16_t(count);
    }

    /* First, XDR the parent atomid. */
    if (!xdr->codeUint32(&parentId))
        return false;

    if (mode == XDR_DECODE) {
        obj = StaticBlockObject::create(cx);
        if (!obj)
            return false;
        *objp = obj;

        obj->setEnclosingBlock(parentId == NO_PARENT_INDEX
                               ? NULL
                               : &script->getObject(parentId)->asStaticBlock());
    }

    AutoObjectRooter tvr(cx, obj);

    if (!xdr->codeUint32(&depthAndCount))
        return false;

    if (mode == XDR_DECODE) {
        uint32_t depth = uint16_t(depthAndCount >> 16);
        count = uint16_t(depthAndCount);
        obj->setStackDepth(depth);

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (unsigned i = 0; i < count; i++) {
            JSAtom *atom;
            if (!XDRAtom(xdr, &atom))
                return false;

            /* The empty string indicates an int id. */
            jsid id = atom != cx->runtime->emptyString
                      ? AtomToId(atom)
                      : INT_TO_JSID(i);

            bool redeclared;
            if (!obj->addVar(cx, id, i, &redeclared)) {
                JS_ASSERT(!redeclared);
                return false;
            }

            uint32_t aliased;
            if (!xdr->codeUint32(&aliased))
                return false;

            JS_ASSERT(aliased == 0 || aliased == 1);
            obj->setAliased(i, !!aliased);
        }
    } else {
        AutoShapeVector shapes(cx);
        if (!shapes.growBy(count))
            return false;

        for (Shape::Range r(obj->lastProperty()); !r.empty(); r.popFront()) {
            const Shape *shape = &r.front();
            shapes[shape->shortid()] = shape;
        }

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (unsigned i = 0; i < count; i++) {
            const Shape *shape = shapes[i];
            JS_ASSERT(shape->getter() == block_getProperty);
            JS_ASSERT(unsigned(shape->shortid()) == i);

            jsid propid = shape->propid();
            JS_ASSERT(JSID_IS_ATOM(propid) || JSID_IS_INT(propid));

            /* The empty string indicates an int id. */
            JSAtom *atom = JSID_IS_ATOM(propid)
                           ? JSID_TO_ATOM(propid)
                           : cx->runtime->emptyString;

            if (!XDRAtom(xdr, &atom))
                return false;

            uint32_t aliased = obj->isAliased(i);
            if (!xdr->codeUint32(&aliased))
                return false;
        }
    }
    return true;
}

template bool
js::XDRStaticBlockObject(XDRState<XDR_ENCODE> *xdr, JSScript *script, StaticBlockObject **objp);

template bool
js::XDRStaticBlockObject(XDRState<XDR_DECODE> *xdr, JSScript *script, StaticBlockObject **objp);

JSObject *
js::CloneStaticBlockObject(JSContext *cx, StaticBlockObject &srcBlock,
                           const AutoObjectVector &objects, JSScript *src)
{
    /* NB: Keep this in sync with XDRStaticBlockObject. */

    StaticBlockObject *clone = StaticBlockObject::create(cx);
    if (!clone)
        return NULL;

    uint32_t parentId = FindObjectIndex(src, srcBlock.enclosingBlock());
    clone->setEnclosingBlock(parentId == NO_PARENT_INDEX
                             ? NULL
                             : &objects[parentId]->asStaticBlock());

    clone->setStackDepth(srcBlock.stackDepth());

    /* Shape::Range is reverse order, so build a list in forward order. */
    AutoShapeVector shapes(cx);
    if (!shapes.growBy(srcBlock.slotCount()))
        return NULL;
    for (Shape::Range r = srcBlock.lastProperty()->all(); !r.empty(); r.popFront())
        shapes[r.front().shortid()] = &r.front();

    for (const Shape **p = shapes.begin(); p != shapes.end(); ++p) {
        jsid id = (*p)->propid();
        unsigned i = (*p)->shortid();

        bool redeclared;
        if (!clone->addVar(cx, id, i, &redeclared)) {
            JS_ASSERT(!redeclared);
            return NULL;
        }

        clone->setAliased(i, srcBlock.isAliased(i));
    }

    return clone;
}

/*****************************************************************************/

ScopeIter::ScopeIter()
 : fp_(NULL),
   cur_(reinterpret_cast<JSObject *>(-1)),
   block_(reinterpret_cast<StaticBlockObject *>(-1)),
   type_(Type(-1))
{}

ScopeIter::ScopeIter(JSObject &enclosingScope)
  : fp_(NULL),
    cur_(&enclosingScope),
    block_(reinterpret_cast<StaticBlockObject *>(-1)),
    type_(Type(-1))
{}

ScopeIter::ScopeIter(StackFrame *fp)
  : fp_(fp),
    cur_(fp->scopeChain()),
    block_(fp->maybeBlockChain())
{
    settle();
}

ScopeIter::ScopeIter(ScopeIter si, StackFrame *fp)
  : fp_(fp),
    cur_(si.cur_),
    block_(si.block_),
    type_(si.type_),
    hasScopeObject_(si.hasScopeObject_)
{}

ScopeIter::ScopeIter(StackFrame *fp, ScopeObject &scope)
  : fp_(fp),
    cur_(&scope)
{
    /*
     * Find the appropriate static block for this iterator, given 'scope'. We
     * know that 'scope' is a (non-optimized) scope on fp's scope chain. We do
     * not, however, know whether fp->maybeScopeChain() encloses 'scope'. E.g.:
     *
     *   let (x = 1) {
     *     g = function() { eval('debugger') };
     *     let (y = 1) g();
     *   }
     *
     * g will have x's block in its enclosing scope but not y's. However, at
     * the debugger statement, both the x's and y's blocks will be on
     * fp->blockChain. Fortunately, we can compare scope object stack depths to
     * determine the block (if any) that encloses 'scope'.
     */
    if (cur_->isNestedScope()) {
        block_ = fp->maybeBlockChain();
        while (block_) {
            if (block_->stackDepth() <= cur_->asNestedScope().stackDepth())
                break;
            block_ = block_->enclosingBlock();
        }
        JS_ASSERT_IF(cur_->isClonedBlock(), cur_->asClonedBlock().staticBlock() == *block_);
    } else {
        block_ = NULL;
    }
    settle();
}

ScopeObject &
ScopeIter::scope() const
{
    JS_ASSERT(hasScopeObject());
    return cur_->asScope();
}

ScopeIter
ScopeIter::enclosing() const
{
    JS_ASSERT(!done());
    ScopeIter si = *this;
    switch (type_) {
      case Call:
        if (hasScopeObject_) {
            si.cur_ = &cur_->asCall().enclosingScope();
            if (CallObjectLambdaName(si.fp_->fun()))
                si.cur_ = &si.cur_->asDeclEnv().enclosingScope();
        }
        si.fp_ = NULL;
        break;
      case Block:
        si.block_ = block_->enclosingBlock();
        if (hasScopeObject_)
            si.cur_ = &cur_->asClonedBlock().enclosingScope();
        si.settle();
        break;
      case With:
        JS_ASSERT(hasScopeObject_);
        si.cur_ = &cur_->asWith().enclosingScope();
        si.settle();
        break;
      case StrictEvalScope:
        if (hasScopeObject_)
            si.cur_ = &cur_->asCall().enclosingScope();
        si.fp_ = NULL;
        break;
    }
    return si;
}

void
ScopeIter::settle()
{
    /*
     * Given an iterator state (cur_, block_), figure out which (potentially
     * optimized) scope the iterator should report. Thus, the result is a pair
     * (type_, hasScopeObject_) where hasScopeObject_ indicates whether the
     * scope object has been optimized away and does not exist on the scope
     * chain. Beware: while ScopeIter iterates over the scopes of a single
     * frame, the scope chain (pointed to by cur_) continues into the scopes of
     * enclosing frames. Thus, it is important not to look at cur_ until it is
     * certain that cur_ points to a scope object in the current frame. In
     * particular, there are two tricky corner cases:
     *  - nested non-heavyweight functions;
     *  - non-strict direct eval.
     * In both cases, cur_ can already be pointing into an enclosing frame's
     * scope chain. As a final twist: even if cur_ points into an enclosing
     * frame's scope chain, the current frame may still have uncloned blocks.
     *
     * Note: DebugScopeObject falls nicely into this plan: since they are only
     * ever introduced as the *enclosing* scope of a frame, they should never
     * show up in scope iteration and fall into the final non-scope case.
     */
    if (fp_->isNonEvalFunctionFrame() && !fp_->fun()->isHeavyweight()) {
        if (block_) {
            type_ = Block;
            hasScopeObject_ = block_->needsClone();
        } else {
            type_ = Call;
            hasScopeObject_ = false;
        }
    } else if (fp_->isNonStrictDirectEvalFrame() && cur_ == fp_->prev()->scopeChain()) {
        if (block_) {
            JS_ASSERT(!block_->needsClone());
            type_ = Block;
            hasScopeObject_ = false;
        } else {
            fp_ = NULL;
        }
    } else if (cur_->isWith()) {
        JS_ASSERT_IF(fp_->isFunctionFrame(), fp_->fun()->isHeavyweight());
        JS_ASSERT_IF(block_, block_->needsClone());
        JS_ASSERT_IF(block_, block_->stackDepth() < cur_->asWith().stackDepth());
        type_ = With;
        hasScopeObject_ = true;
    } else if (block_) {
        type_ = Block;
        hasScopeObject_ = block_->needsClone();
        JS_ASSERT_IF(hasScopeObject_, cur_->asClonedBlock().staticBlock() == *block_);
    } else if (cur_->isCall()) {
        CallObject &callobj = cur_->asCall();
        type_ = callobj.isForEval() ? StrictEvalScope : Call;
        hasScopeObject_ = true;
        JS_ASSERT_IF(type_ == Call, callobj.getCalleeFunction()->script() == fp_->script());
    } else {
        JS_ASSERT(!cur_->isScope());
        JS_ASSERT(fp_->isGlobalFrame() || fp_->isDebuggerFrame());
        fp_ = NULL;
    }
}

/* static */ HashNumber
ScopeIter::hash(ScopeIter si)
{
    /* hasScopeObject_ is determined by the other fields. */
    return size_t(si.fp_) ^ size_t(si.cur_) ^ size_t(si.block_) ^ si.type_;
}

/* static */ bool
ScopeIter::match(ScopeIter si1, ScopeIter si2)
{
    /* hasScopeObject_ is determined by the other fields. */
    return si1.fp_ == si2.fp_ &&
           (!si1.fp_ ||
            (si1.cur_   == si2.cur_   &&
             si1.block_ == si2.block_ &&
             si1.type_  == si2.type_));
}

/*****************************************************************************/

namespace js {

/*
 * DebugScopeProxy is the handler for DebugScopeObject proxy objects and mostly
 * just wraps ScopeObjects. Having a custom handler (rather than trying to
 * reuse js::Wrapper) gives us several important abilities:
 *  - We want to pass the ScopeObject as the receiver to forwarded scope
 *    property ops so that Call/Block/With ops do not all require a
 *    'normalization' step.
 *  - The engine has made certain assumptions about the possible reads/writes
 *    in a scope. DebugScopeProxy allows us to prevent the debugger from
 *    breaking those assumptions. Examples include adding shadowing variables
 *    or changing the property attributes of bindings.
 *  - The engine makes optimizations that are observable to the debugger. The
 *    proxy can either hide these optimizations or make the situation more
 *    clear to the debugger. An example is 'arguments'.
 */
class DebugScopeProxy : public BaseProxyHandler
{
    static bool isArguments(JSContext *cx, jsid id)
    {
        return id == NameToId(cx->runtime->atomState.argumentsAtom);
    }

    static bool isFunctionScope(ScopeObject &scope)
    {
        return scope.isCall() && !scope.asCall().isForEval();
    }

    /*
     * In theory, every function scope contains an 'arguments' bindings.
     * However, the engine only adds a binding if 'arguments' is used in the
     * function body. Thus, from the debugger's perspective, 'arguments' may be
     * missing from the list of bindings.
     */
    static bool isMissingArgumentsBinding(ScopeObject &scope)
    {
        return isFunctionScope(scope) &&
               !scope.asCall().getCalleeFunction()->script()->argumentsHasLocalBinding();
    }

    /*
     * This function creates an arguments object when the debugger requests
     * 'arguments' for a function scope where the arguments object has been
     * optimized away (either because the binding is missing altogether or
     * because !ScriptAnalysis::needsArgsObj).
     */
    static bool checkForMissingArguments(JSContext *cx, jsid id, ScopeObject &scope,
                                         ArgumentsObject **maybeArgsObj)
    {
        *maybeArgsObj = NULL;

        if (!isArguments(cx, id) || !isFunctionScope(scope))
            return true;

        JSScript *script = scope.asCall().getCalleeFunction()->script();
        if (script->needsArgsObj())
            return true;

        StackFrame *fp = scope.maybeStackFrame();
        if (!fp) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_LIVE,
                                 "Debugger scope");
            return false;
        }

        *maybeArgsObj = ArgumentsObject::createUnexpected(cx, fp);
        return true;
    }

  public:
    static int family;
    static DebugScopeProxy singleton;

    DebugScopeProxy() : BaseProxyHandler(&family) {}

    bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                               PropertyDescriptor *desc) MOZ_OVERRIDE
    {
        return getOwnPropertyDescriptor(cx, proxy, id, set, desc);
    }

    bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                  PropertyDescriptor *desc) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        ArgumentsObject *maybeArgsObj;
        if (!checkForMissingArguments(cx, id, scope, &maybeArgsObj))
            return false;

        if (maybeArgsObj) {
            PodZero(desc);
            desc->obj = proxy;
            desc->attrs = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
            desc->value = ObjectValue(*maybeArgsObj);
            return true;
        }

        AutoAllowUnaliasedVarAccess a(cx);
        return JS_GetPropertyDescriptorById(cx, &scope, id, JSRESOLVE_QUALIFIED, desc);
    }

    bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        ArgumentsObject *maybeArgsObj;
        if (!checkForMissingArguments(cx, id, scope, &maybeArgsObj))
            return false;

        if (maybeArgsObj) {
            *vp = ObjectValue(*maybeArgsObj);
            return true;
        }

        AutoAllowUnaliasedVarAccess a(cx);
        return scope.getGeneric(cx, RootedVarObject(cx, &scope), RootedVarId(cx, id), vp);
    }

    bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                     Value *vp) MOZ_OVERRIDE
    {
        AutoAllowUnaliasedVarAccess a(cx);
        ScopeObject &scope = proxy->asDebugScope().scope();
        return scope.setGeneric(cx, RootedVarId(cx, id), vp, strict);
    }

    bool defineProperty(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc) MOZ_OVERRIDE
    {
        return Throw(cx, id, JSMSG_CANT_REDEFINE_PROP);
    }

    bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        if (isMissingArgumentsBinding(scope) &&
            !props.append(NameToId(cx->runtime->atomState.argumentsAtom)))
        {
            return false;
        }

        return GetPropertyNames(cx, &scope, JSITER_OWNONLY, &props);
    }

    bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE
    {
        return js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_CANT_DELETE,
                                        JSDVG_IGNORE_STACK, IdToValue(id), NULL,
                                        NULL, NULL);
    }

    bool enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        if (isMissingArgumentsBinding(scope) &&
            !props.append(NameToId(cx->runtime->atomState.argumentsAtom)))
        {
            return false;
        }

        return GetPropertyNames(cx, &scope, 0, &props);
    }

    bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        if (isArguments(cx, id) && isFunctionScope(scope)) {
            *bp = true;
            return true;
        }

        JSBool found;
        if (!JS_HasPropertyById(cx, &scope, id, &found))
            return false;

        *bp = found;
        return true;
    }
};

}  /* namespace js */

int DebugScopeProxy::family = 0;
DebugScopeProxy DebugScopeProxy::singleton;

/* static */ DebugScopeObject *
DebugScopeObject::create(JSContext *cx, ScopeObject &scope, JSObject &enclosing)
{
    JSObject *obj = NewProxyObject(cx, &DebugScopeProxy::singleton, ObjectValue(scope),
                                   NULL /* proto */, &scope.global(),
                                   NULL /* call */, NULL /* construct */);
    if (!obj)
        return NULL;

    JS_ASSERT(!enclosing.isScope());
    SetProxyExtra(obj, ENCLOSING_EXTRA, ObjectValue(enclosing));

    return &obj->asDebugScope();
}

ScopeObject &
DebugScopeObject::scope() const
{
    return Wrapper::wrappedObject(this)->asScope();
}

JSObject &
DebugScopeObject::enclosingScope() const
{
    return GetProxyExtra(this, ENCLOSING_EXTRA).toObject();
}

bool
DebugScopeObject::isForDeclarative() const
{
    ScopeObject &s = scope();
    return s.isCall() || s.isBlock() || s.isDeclEnv();
}

bool
js_IsDebugScopeSlow(const JSObject *obj)
{
    return obj->getClass() == &ObjectProxyClass &&
           GetProxyHandler(obj) == &DebugScopeProxy::singleton;
}

/*****************************************************************************/

DebugScopes::DebugScopes(JSRuntime *rt)
 : proxiedScopes(rt),
   missingScopes(rt)
{}

DebugScopes::~DebugScopes()
{
    JS_ASSERT(missingScopes.empty());
}

bool
DebugScopes::init()
{
    if (!proxiedScopes.init() ||
        !missingScopes.init())
    {
        return false;
    }
    return true;
}

void
DebugScopes::mark(JSTracer *trc)
{
    proxiedScopes.trace(trc);
}

void
DebugScopes::sweep()
{
    /*
     * Note: missingScopes points to debug scopes weakly not just so that debug
     * scopes can be released more eagerly, but, more importantly, to avoid
     * creating an uncollectable cycle with suspended generator frames.
     */
    for (MissingScopeMap::Enum e(missingScopes); !e.empty(); e.popFront()) {
        if (IsAboutToBeFinalized(e.front().value))
            e.removeFront();
    }
}

/*
 * Unfortunately, GetDebugScopeForFrame needs to work even outside debug mode
 * (in particular, JS_GetFrameScopeChain does not require debug mode). Since
 * DebugScopes::onPop* are only called in debug mode, this means we cannot
 * use any of the maps in DebugScopes. This will produce debug scope chains
 * that do not obey the debugger invariants but that is just fine.
 */
static bool
CanUseDebugScopeMaps(JSContext *cx)
{
    return cx->compartment->debugMode();
}

DebugScopeObject *
DebugScopes::hasDebugScope(JSContext *cx, ScopeObject &scope) const
{
    if (ObjectWeakMap::Ptr p = proxiedScopes.lookup(&scope)) {
        JS_ASSERT(CanUseDebugScopeMaps(cx));
        return &p->value->asDebugScope();
    }
    return NULL;
}

bool
DebugScopes::addDebugScope(JSContext *cx, ScopeObject &scope, DebugScopeObject &debugScope)
{
    if (!CanUseDebugScopeMaps(cx))
        return true;
    JS_ASSERT(!proxiedScopes.has(&scope));
    if (!proxiedScopes.put(&scope, &debugScope)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

DebugScopeObject *
DebugScopes::hasDebugScope(JSContext *cx, ScopeIter si) const
{
    JS_ASSERT(!si.hasScopeObject());
    if (MissingScopeMap::Ptr p = missingScopes.lookup(si)) {
        JS_ASSERT(CanUseDebugScopeMaps(cx));
        return p->value;
    }
    return NULL;
}

bool
DebugScopes::addDebugScope(JSContext *cx, ScopeIter si, DebugScopeObject &debugScope)
{
    JS_ASSERT(!si.hasScopeObject());
    if (!CanUseDebugScopeMaps(cx))
        return true;
    JS_ASSERT(!missingScopes.has(si));
    if (!missingScopes.put(si, &debugScope)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

void
DebugScopes::onPopCall(StackFrame *fp)
{
    if (fp->isYielding())
        return;

    if (!fp->fun()->isHeavyweight()) {
        JS_ASSERT(!fp->hasCallObj());
        if (MissingScopeMap::Ptr p = missingScopes.lookup(ScopeIter(fp))) {
            js_PutCallObject(fp, p->value->scope().asCall());
            missingScopes.remove(p);
        }
    }
}

void
DebugScopes::onPopBlock(JSContext *cx, StackFrame *fp)
{
    StaticBlockObject &block = *fp->maybeBlockChain();
    if (!block.needsClone()) {
        JS_ASSERT(!fp->scopeChain()->isBlock() ||
                  fp->scopeChain()->asClonedBlock().staticBlock() != block);
        if (MissingScopeMap::Ptr p = missingScopes.lookup(ScopeIter(fp))) {
            p->value->scope().asClonedBlock().put(fp);
            missingScopes.remove(p);
        }
    }
}

void
DebugScopes::onGeneratorFrameChange(StackFrame *from, StackFrame *to)
{
    for (ScopeIter toIter(to); !toIter.done(); toIter = toIter.enclosing()) {
        if (!toIter.hasScopeObject()) {
            if (MissingScopeMap::Ptr p = missingScopes.lookup(ScopeIter(toIter, from))) {
                DebugScopeObject &debugScope = *p->value;
                ScopeObject &scope = debugScope.scope();
                if (scope.isCall()) {
                    JS_ASSERT(scope.maybeStackFrame() == from);
                    scope.setStackFrame(to);
                    if (scope.enclosingScope().isDeclEnv()) {
                        JS_ASSERT(scope.enclosingScope().asDeclEnv().maybeStackFrame() == from);
                        scope.enclosingScope().asDeclEnv().setStackFrame(to);
                    }
                }
                missingScopes.remove(p);
                missingScopes.put(toIter, &debugScope);
            }
        }
    }
}

void
DebugScopes::onCompartmentLeaveDebugMode(JSCompartment *c)
{
    for (MissingScopeMap::Enum e(missingScopes); !e.empty(); e.popFront()) {
        if (e.front().key.fp()->compartment() == c)
            e.removeFront();
    }
}

/*****************************************************************************/

static JSObject *
GetDebugScope(JSContext *cx, ScopeIter si);

static DebugScopeObject *
GetDebugScopeForScope(JSContext *cx, ScopeObject &scope, ScopeIter enclosing)
{
    DebugScopes &debugScopes = *cx->runtime->debugScopes;
    if (DebugScopeObject *debugScope = debugScopes.hasDebugScope(cx, scope))
        return debugScope;

    JSObject *enclosingDebug = GetDebugScope(cx, enclosing);
    if (!enclosingDebug)
        return NULL;

    JSObject &maybeDecl = scope.enclosingScope();
    if (maybeDecl.isDeclEnv()) {
        JS_ASSERT(CallObjectLambdaName(scope.asCall().getCalleeFunction()));
        enclosingDebug = DebugScopeObject::create(cx, maybeDecl.asDeclEnv(), *enclosingDebug);
        if (!enclosingDebug)
            return NULL;
    }

    DebugScopeObject *debugScope = DebugScopeObject::create(cx, scope, *enclosingDebug);
    if (!debugScope)
        return NULL;

    if (!debugScopes.addDebugScope(cx, scope, *debugScope))
        return NULL;

    return debugScope;
}

static DebugScopeObject *
GetDebugScopeForMissing(JSContext *cx, ScopeIter si)
{
    DebugScopes &debugScopes = *cx->runtime->debugScopes;
    if (DebugScopeObject *debugScope = debugScopes.hasDebugScope(cx, si))
        return debugScope;

    JSObject *enclosingDebug = GetDebugScope(cx, si.enclosing());
    if (!enclosingDebug)
        return NULL;

    /*
     * Create the missing scope object. This takes care of storing variable
     * values after the StackFrame has been popped. To preserve scopeChain
     * depth invariants, these lazily-reified scopes must not be put on the
     * frame's scope chain; instead, they are maintained via DebugScopes hooks.
     */
    DebugScopeObject *debugScope = NULL;
    switch (si.type()) {
      case ScopeIter::Call: {
        CallObject *callobj = CallObject::createForFunction(cx, si.fp());
        if (!callobj)
            return NULL;

        JSObject &maybeDecl = callobj->enclosingScope();
        if (maybeDecl.isDeclEnv()) {
            JS_ASSERT(CallObjectLambdaName(callobj->getCalleeFunction()));
            enclosingDebug = DebugScopeObject::create(cx, maybeDecl.asDeclEnv(), *enclosingDebug);
            if (!enclosingDebug)
                return NULL;
        }

        debugScope = DebugScopeObject::create(cx, *callobj, *enclosingDebug);
        if (!debugScope)
            return NULL;

        if (!CanUseDebugScopeMaps(cx))
            js_PutCallObject(si.fp(), *callobj);
        break;
      }
      case ScopeIter::Block: {
        RootedVar<StaticBlockObject *> staticBlock(cx, &si.staticBlock());
        ClonedBlockObject *block = ClonedBlockObject::create(cx, staticBlock, si.fp());
        if (!block)
            return NULL;

        debugScope = DebugScopeObject::create(cx, *block, *enclosingDebug);
        if (!debugScope)
            return NULL;

        if (!CanUseDebugScopeMaps(cx))
            block->put(si.fp());
        break;
      }
      case ScopeIter::With:
      case ScopeIter::StrictEvalScope:
        JS_NOT_REACHED("should already have a scope");
    }

    if (!debugScopes.addDebugScope(cx, si, *debugScope))
        return NULL;

    return debugScope;
}

static JSObject *
GetDebugScope(JSContext *cx, JSObject &obj)
{
    /*
     * As an engine invariant (maintained internally and asserted by Execute),
     * ScopeObjects and non-ScopeObjects cannot be interleaved on the scope
     * chain; every scope chain must start with zero or more ScopeObjects and
     * terminate with one or more non-ScopeObjects (viz., GlobalObject).
     */
    if (!obj.isScope()) {
#ifdef DEBUG
        JSObject *o = &obj;
        while ((o = o->enclosingScope()))
            JS_ASSERT(!o->isScope());
#endif
        return &obj;
    }

    /*
     * If 'scope' is a 'with' block, then the chain is fully reified from that
     * point outwards, and there's no point in bothering with a ScopeIter. If
     * |scope| has an associated stack frame, we can get more detailed scope
     * chain information from that.
     * Note: all this frame hackery will be removed by bug 659577.
     */
    ScopeObject &scope = obj.asScope();
    if (!scope.isWith() && scope.maybeStackFrame()) {
        StackFrame *fp = scope.maybeStackFrame();
        if (scope.isClonedBlock())
            fp = js_LiveFrameIfGenerator(fp);
        return GetDebugScope(cx, ScopeIter(fp, scope));
    }
    return GetDebugScopeForScope(cx, scope, ScopeIter(scope.enclosingScope()));
}

static JSObject *
GetDebugScope(JSContext *cx, ScopeIter si)
{
    JS_CHECK_RECURSION(cx, return NULL);

    if (si.done())
        return GetDebugScope(cx, si.enclosingScope());

    if (!si.hasScopeObject())
        return GetDebugScopeForMissing(cx, si);

    return GetDebugScopeForScope(cx, si.scope(), si.enclosing());
}

JSObject *
js::GetDebugScopeForFunction(JSContext *cx, JSFunction *fun)
{
    assertSameCompartment(cx, fun);
    JS_ASSERT(cx->compartment->debugMode());
    return GetDebugScope(cx, *fun->environment());
}

JSObject *
js::GetDebugScopeForFrame(JSContext *cx, StackFrame *fp)
{
    assertSameCompartment(cx, fp);
    /* Unfortunately, we cannot JS_ASSERT(debugMode); see CanUseDebugScopeMaps. */
    return GetDebugScope(cx, ScopeIter(fp));
}

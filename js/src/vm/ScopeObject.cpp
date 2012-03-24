/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey call object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Biggar <pbiggar@mozilla.com> (original author)
 *   Luke Wagner <luke@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jscompartment.h"
#include "jsiter.h"
#include "jsscope.h"
#if JS_HAS_XDR
#include "jsxdrapi.h"
#endif

#include "GlobalObject.h"
#include "ScopeObject.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "ScopeObject-inl.h"

using namespace js;
using namespace js::types;

void
js_PutCallObject(StackFrame *fp)
{
    CallObject &callobj = fp->callObj().asCall();
    JS_ASSERT(callobj.maybeStackFrame() == fp);
    JS_ASSERT_IF(fp->isEvalFrame(), fp->isStrictEvalFrame());
    JS_ASSERT(fp->isEvalFrame() == callobj.isForEval());

    JSScript *script = fp->script();
    Bindings &bindings = script->bindings;

    if (callobj.isForEval()) {
        JS_ASSERT(script->strictModeCode);
        JS_ASSERT(bindings.countArgs() == 0);

        /* This could be optimized as below, but keep it simple for now. */
        callobj.copyValues(0, NULL, bindings.countVars(), fp->slots());
    } else {
        JSFunction *fun = fp->fun();
        JS_ASSERT(script == callobj.getCalleeFunction()->script());
        JS_ASSERT(script == fun->script());

        unsigned n = bindings.countLocalNames();
        if (n > 0) {
            uint32_t nvars = bindings.countVars();
            uint32_t nargs = bindings.countArgs();
            JS_ASSERT(fun->nargs == nargs);
            JS_ASSERT(nvars + nargs == n);

            JSScript *script = fun->script();
            if (script->usesEval
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
                uint32_t nclosed = script->nClosedArgs;
                for (uint32_t i = 0; i < nclosed; i++) {
                    uint32_t e = script->getClosedArg(i);
#ifdef JS_GC_ZEAL
                    callobj.setArg(e, fp->formalArg(e));
#else
                    callobj.initArgUnchecked(e, fp->formalArg(e));
#endif
                }

                nclosed = script->nClosedVars;
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
CallObject::create(JSContext *cx, JSScript *script, JSObject &enclosing, JSObject *callee)
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

    JSObject *obj = JSObject::create(cx, kind, shape, type, slots);
    if (!obj)
        return NULL;

    /*
     * Update the parent for bindings associated with non-compileAndGo scripts,
     * whose call objects do not have a consistent global variable and need
     * to be updated dynamically.
     */
    JSObject &global = enclosing.global();
    if (&global != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        if (!obj->setParent(cx, &global))
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
    obj->initFixedSlot(ARGUMENTS_SLOT, MagicValue(JS_UNASSIGNED_ARGUMENTS));

    /*
     * If |bindings| is for a function that has extensible parents, that means
     * its Call should have its own shape; see BaseShape::extensibleParents.
     */
    if (obj->lastProperty()->extensibleParents() && !obj->generateOwnShape(cx))
        return NULL;

    return &obj->asCall();
}

CallObject *
CallObject::createForFunction(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(fp->isNonEvalFunctionFrame());
    JS_ASSERT(!fp->hasCallObj());

    JSObject *scopeChain = &fp->scopeChain();
    JS_ASSERT_IF(scopeChain->isWith() || scopeChain->isBlock() || scopeChain->isCall(),
                 scopeChain->getPrivate() != fp);

    /*
     * For a named function expression Call's parent points to an environment
     * object holding function's name.
     */
    if (JSAtom *lambdaName = CallObjectLambdaName(fp->fun())) {
        scopeChain = DeclEnvObject::create(cx, fp);
        if (!scopeChain)
            return NULL;

        if (!DefineNativeProperty(cx, scopeChain, ATOM_TO_JSID(lambdaName),
                                  ObjectValue(fp->callee()), NULL, NULL,
                                  JSPROP_PERMANENT | JSPROP_READONLY, 0, 0)) {
            return NULL;
        }
    }

    CallObject *callobj = create(cx, fp->script(), *scopeChain, &fp->callee());
    if (!callobj)
        return NULL;

    callobj->setStackFrame(fp);
    fp->setScopeChainWithOwnCallObj(*callobj);
    if (fp->hasArgsObj())
        callobj->setArguments(ObjectValue(fp->argsObj()));
    return callobj;
}

CallObject *
CallObject::createForStrictEval(JSContext *cx, StackFrame *fp)
{
    CallObject *callobj = create(cx, fp->script(), fp->scopeChain(), NULL);
    if (!callobj)
        return NULL;

    callobj->setStackFrame(fp);
    fp->setScopeChainWithOwnCallObj(*callobj);
    return callobj;
}

JSBool
CallObject::getArgumentsOp(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    *vp = obj->asCall().arguments();

    /*
     * This can only happen through eval-in-frame. Eventually, this logic can
     * be hoisted into debugger scope wrappers. That will allow 'arguments' to
     * be a pure data property and allow call_resolve to be removed.
     */
    if (vp->isMagic(JS_UNASSIGNED_ARGUMENTS)) {
        StackFrame *fp = obj->asCall().maybeStackFrame();
        ArgumentsObject *argsObj = ArgumentsObject::createUnexpected(cx, fp);
        if (!argsObj)
            return false;

        *vp = ObjectValue(*argsObj);
        obj->asCall().setArguments(*vp);
    }

    return true;
}

JSBool
CallObject::setArgumentsOp(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    JS_ASSERT(obj->asCall().maybeStackFrame());
    obj->asCall().setArguments(*vp);
    return true;
}

JSBool
CallObject::getArgOp(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    CallObject &callobj = obj->asCall();
    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    if (StackFrame *fp = callobj.maybeStackFrame())
        *vp = fp->formalArg(i);
    else
        *vp = callobj.arg(i);
    return true;
}

JSBool
CallObject::setArgOp(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    CallObject &callobj = obj->asCall();
    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    if (StackFrame *fp = callobj.maybeStackFrame())
        fp->formalArg(i) = *vp;
    else
        callobj.setArg(i, *vp);

    JSFunction *fun = callobj.getCalleeFunction();
    JSScript *script = fun->script();
    if (!script->ensureHasTypes(cx))
        return false;

    TypeScript::SetArgument(cx, script, i, *vp);

    return true;
}

JSBool
CallObject::getVarOp(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    CallObject &callobj = obj->asCall();
    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    if (StackFrame *fp = callobj.maybeStackFrame())
        *vp = fp->varSlot(i);
    else
        *vp = callobj.var(i);
    return true;
}

JSBool
CallObject::setVarOp(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    CallObject &callobj = obj->asCall();

    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    if (StackFrame *fp = callobj.maybeStackFrame())
        fp->varSlot(i) = *vp;
    else
        callobj.setVar(i, *vp);

    JSFunction *fun = callobj.getCalleeFunction();
    JSScript *script = fun->script();
    if (!script->ensureHasTypes(cx))
        return false;

    TypeScript::SetLocal(cx, script, i, *vp);

    return true;
}

bool
CallObject::containsVarOrArg(PropertyName *name, Value *vp, JSContext *cx)
{
    jsid id = ATOM_TO_JSID(name);
    const Shape *shape = nativeLookup(cx, id);
    if (!shape)
        return false;

    PropertyOp op = shape->getterOp();
    if (op != getVarOp && op != getArgOp)
        return false;

    JS_ALWAYS_TRUE(op(cx, this, INT_TO_JSID(shape->shortid()), vp));
    return true;
}

static JSBool
call_resolve(JSContext *cx, JSObject *obj, jsid id, unsigned flags, JSObject **objp)
{
    JS_ASSERT(!obj->getProto());

    if (!JSID_IS_ATOM(id))
        return true;

    JSObject *callee = obj->asCall().getCallee();
#ifdef DEBUG
    if (callee) {
        JSScript *script = callee->toFunction()->script();
        JS_ASSERT(!script->bindings.hasBinding(cx, JSID_TO_ATOM(id)));
    }
#endif

    /*
     * Resolve arguments so that we never store a particular Call object's
     * arguments object reference in a Call prototype's |arguments| slot.
     *
     * Include JSPROP_ENUMERATE for consistency with all other Call object
     * properties; see js::Bindings::add and js::Interpret's JSOP_DEFFUN
     * rebinding-Call-property logic.
     */
    if (callee && id == ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom)) {
        if (!DefineNativeProperty(cx, obj, id, UndefinedValue(),
                                  CallObject::getArgumentsOp, CallObject::setArgumentsOp,
                                  JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_ENUMERATE,
                                  0, 0, DNP_DONT_PURGE)) {
            return false;
        }
        *objp = obj;
        return true;
    }

    /* Control flow reaches here only if id was not resolved. */
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
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(CallObject::RESERVED_SLOTS) |
    JSCLASS_NEW_RESOLVE | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    (JSResolveOp)call_resolve,
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
                                                    &fp->scopeChain().global(),
                                                    FINALIZE_KIND);
    if (!emptyDeclEnvShape)
        return NULL;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyDeclEnvShape, type, NULL);
    if (!obj)
        return NULL;

    obj->setPrivate(fp);
    if (!obj->asScope().setEnclosingScope(cx, fp->scopeChain()))
        return NULL;

    return &obj->asDeclEnv();
}

WithObject *
WithObject::create(JSContext *cx, StackFrame *fp, JSObject &proto, JSObject &enclosing,
                   uint32_t depth)
{
    RootedVarTypeObject type(cx);
    type = proto.getNewType(cx);
    if (!type)
        return NULL;

    RootedVarShape emptyWithShape(cx);
    emptyWithShape = EmptyShape::getInitialShape(cx, &WithClass, &proto,
                                                 &enclosing.global(), FINALIZE_KIND);
    if (!emptyWithShape)
        return NULL;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyWithShape, type, NULL);
    if (!obj)
        return NULL;

    if (!obj->asScope().setEnclosingScope(cx, enclosing))
        return NULL;

    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(depth));
    obj->setPrivate(js_FloatingFrameIfGenerator(cx, fp));

    JSObject *thisp = proto.thisObject(cx);
    if (!thisp)
        return NULL;

    obj->setFixedSlot(THIS_SLOT, ObjectValue(*thisp));

    return &obj->asWith();
}

static JSBool
with_LookupGeneric(JSContext *cx, JSObject *obj, jsid id, JSObject **objp, JSProperty **propp)
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
with_LookupProperty(JSContext *cx, JSObject *obj, PropertyName *name, JSObject **objp, JSProperty **propp)
{
    return with_LookupGeneric(cx, obj, ATOM_TO_JSID(name), objp, propp);
}

static JSBool
with_LookupElement(JSContext *cx, JSObject *obj, uint32_t index, JSObject **objp,
                   JSProperty **propp)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
with_LookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid, JSObject **objp, JSProperty **propp)
{
    return with_LookupGeneric(cx, obj, SPECIALID_TO_JSID(sid), objp, propp);
}

static JSBool
with_GetGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{
    return obj->asWith().object().getGeneric(cx, id, vp);
}

static JSBool
with_GetProperty(JSContext *cx, JSObject *obj, JSObject *receiver, PropertyName *name, Value *vp)
{
    return with_GetGeneric(cx, obj, receiver, ATOM_TO_JSID(name), vp);
}

static JSBool
with_GetElement(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index, Value *vp)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
with_GetSpecial(JSContext *cx, JSObject *obj, JSObject *receiver, SpecialId sid, Value *vp)
{
    return with_GetGeneric(cx, obj, receiver, SPECIALID_TO_JSID(sid), vp);
}

static JSBool
with_SetGeneric(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    return obj->asWith().object().setGeneric(cx, id, vp, strict);
}

static JSBool
with_SetProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *vp, JSBool strict)
{
    return obj->asWith().object().setProperty(cx, name, vp, strict);
}

static JSBool
with_SetElement(JSContext *cx, JSObject *obj, uint32_t index, Value *vp, JSBool strict)
{
    return obj->asWith().object().setElement(cx, index, vp, strict);
}

static JSBool
with_SetSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *vp, JSBool strict)
{
    return obj->asWith().object().setSpecial(cx, sid, vp, strict);
}

static JSBool
with_GetGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp)
{
    return obj->asWith().object().getGenericAttributes(cx, id, attrsp);
}

static JSBool
with_GetPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp)
{
    return obj->asWith().object().getPropertyAttributes(cx, name, attrsp);
}

static JSBool
with_GetElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp)
{
    return obj->asWith().object().getElementAttributes(cx, index, attrsp);
}

static JSBool
with_GetSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp)
{
    return obj->asWith().object().getSpecialAttributes(cx, sid, attrsp);
}

static JSBool
with_SetGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp)
{
    return obj->asWith().object().setGenericAttributes(cx, id, attrsp);
}

static JSBool
with_SetPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp)
{
    return obj->asWith().object().setPropertyAttributes(cx, name, attrsp);
}

static JSBool
with_SetElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp)
{
    return obj->asWith().object().setElementAttributes(cx, index, attrsp);
}

static JSBool
with_SetSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp)
{
    return obj->asWith().object().setSpecialAttributes(cx, sid, attrsp);
}

static JSBool
with_DeleteProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteProperty(cx, name, rval, strict);
}

static JSBool
with_DeleteElement(JSContext *cx, JSObject *obj, uint32_t index, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteElement(cx, index, rval, strict);
}

static JSBool
with_DeleteSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteSpecial(cx, sid, rval, strict);
}

static JSBool
with_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
               Value *statep, jsid *idp)
{
    return obj->asWith().object().enumerate(cx, enum_op, statep, idp);
}

static JSType
with_TypeOf(JSContext *cx, JSObject *obj)
{
    return JSTYPE_OBJECT;
}

static JSObject *
with_ThisObject(JSContext *cx, JSObject *obj)
{
    return &obj->asWith().withThis();
}

Class js::WithClass = {
    "With",
    JSCLASS_HAS_PRIVATE |
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
        NULL,             /* fix   */
        with_ThisObject,
        NULL,             /* clear */
    }
};

ClonedBlockObject *
ClonedBlockObject::create(JSContext *cx, StaticBlockObject &block, StackFrame *fp)
{
    RootedVarTypeObject type(cx);
    type = block.getNewType(cx);
    if (!type)
        return NULL;

    HeapSlot *slots;
    if (!PreallocateObjectDynamicSlots(cx, block.lastProperty(), &slots))
        return NULL;

    RootedVarShape shape(cx);
    shape = block.lastProperty();

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, shape, type, slots);
    if (!obj)
        return NULL;

    /* Set the parent if necessary, as for call objects. */
    JSObject &global = fp->scopeChain().global();
    if (&global != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        if (!obj->setParent(cx, &global))
            return NULL;
    }

    JS_ASSERT(!obj->inDictionaryMode());
    JS_ASSERT(obj->slotSpan() >= block.slotCount() + RESERVED_SLOTS);

    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(block.stackDepth()));
    obj->setPrivate(js_FloatingFrameIfGenerator(cx, fp));

    if (obj->lastProperty()->extensibleParents() && !obj->generateOwnShape(cx))
        return NULL;

    return &obj->asClonedBlock();
}

void
ClonedBlockObject::put(JSContext *cx)
{
    StackFrame *fp = cx->fp();
    JS_ASSERT(maybeStackFrame() == js_FloatingFrameIfGenerator(cx, fp));

    uint32_t count = slotCount();
    uint32_t depth = stackDepth();

    /* The block and its locals must be on the current stack for GC safety. */
    JS_ASSERT(depth <= uint32_t(cx->regs().sp - fp->base()));
    JS_ASSERT(count <= uint32_t(cx->regs().sp - fp->base() - depth));

    /* See comments in CheckDestructuring in frontend/Parser.cpp. */
    JS_ASSERT(count >= 1);

    copySlotRange(RESERVED_SLOTS, fp->base() + depth, count);

    /* We must clear the private slot even with errors. */
    setPrivate(NULL);
    fp->setScopeChainNoCallObj(enclosingScope());
}

static JSBool
block_getProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    /*
     * Block objects are never exposed to script, and the engine handles them
     * with care. So unlike other getters, this one can assert (rather than
     * check) certain invariants about obj.
     */
    ClonedBlockObject &block = obj->asClonedBlock();
    unsigned index = (unsigned) JSID_TO_INT(id);
    JS_ASSERT(index < block.slotCount());

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
block_setProperty(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    ClonedBlockObject &block = obj->asClonedBlock();
    unsigned index = (unsigned) JSID_TO_INT(id);
    JS_ASSERT(index < block.slotCount());

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
    jsid id = ATOM_TO_JSID(name);
    const Shape *shape = nativeLookup(cx, id);
    if (!shape)
        return false;

    JS_ASSERT(shape->getterOp() == block_getProperty);
    JS_ALWAYS_TRUE(block_getProperty(cx, this, INT_TO_JSID(shape->shortid()), vp));
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
    JS_ConvertStub
};

#if JS_HAS_XDR

#define NO_PARENT_INDEX UINT32_MAX

static uint32_t
FindObjectIndex(JSObjectArray *array, JSObject *obj)
{
    size_t i;

    if (array) {
        i = array->length;
        do {

            if (array->vector[--i] == obj)
                return i;
        } while (i != 0);
    }

    return NO_PARENT_INDEX;
}

bool
js::XDRStaticBlockObject(JSXDRState *xdr, JSScript *script, StaticBlockObject **objp)
{
    JSContext *cx = xdr->cx;

    StaticBlockObject *obj = NULL;
    uint32_t parentId = 0;
    uint32_t count = 0;
    uint32_t depthAndCount = 0;
    if (xdr->mode == JSXDR_ENCODE) {
        obj = *objp;
        parentId = JSScript::isValidOffset(script->objectsOffset)
                   ? FindObjectIndex(script->objects(), obj->enclosingBlock())
                   : NO_PARENT_INDEX;
        uint32_t depth = obj->stackDepth();
        JS_ASSERT(depth <= UINT16_MAX);
        count = obj->slotCount();
        JS_ASSERT(count <= UINT16_MAX);
        depthAndCount = (depth << 16) | uint16_t(count);
    }

    /* First, XDR the parent atomid. */
    if (!JS_XDRUint32(xdr, &parentId))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        obj = StaticBlockObject::create(cx);
        if (!obj)
            return false;
        *objp = obj;

        /*
         * If there's a parent id, then get the parent out of our script's
         * object array. We know that we XDR block object in outer-to-inner
         * order, which means that getting the parent now will work.
         */
        obj->setEnclosingBlock(parentId == NO_PARENT_INDEX
                               ? NULL
                               : &script->getObject(parentId)->asStaticBlock());
    }

    AutoObjectRooter tvr(cx, obj);

    if (!JS_XDRUint32(xdr, &depthAndCount))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        uint32_t depth = uint16_t(depthAndCount >> 16);
        count = uint16_t(depthAndCount);
        obj->setStackDepth(depth);

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (unsigned i = 0; i < count; i++) {
            JSAtom *atom;
            if (!js_XDRAtom(xdr, &atom))
                return false;

            /* The empty string indicates an int id. */
            jsid id = atom != cx->runtime->emptyString
                      ? ATOM_TO_JSID(atom)
                      : INT_TO_JSID(i);

            bool redeclared;
            if (!obj->addVar(cx, id, i, &redeclared)) {
                JS_ASSERT(!redeclared);
                return false;
            }
        }
    } else {
        AutoShapeVector shapes(cx);
        shapes.growBy(count);

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

            if (!js_XDRAtom(xdr, &atom))
                return false;
        }
    }
    return true;
}

#endif  /* JS_HAS_XDR */

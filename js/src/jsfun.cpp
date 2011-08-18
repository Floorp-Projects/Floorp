/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * JS function support.
 */
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsbit.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jspropertytree.h"
#include "jsproxy.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsexn.h"
#include "jsstaticcheck.h"
#include "jstracer.h"
#include "vm/Debugger.h"

#if JS_HAS_GENERATORS
# include "jsiter.h"
#endif

#if JS_HAS_XDR
# include "jsxdrapi.h"
#endif

#ifdef JS_METHODJIT
#include "methodjit/MethodJIT.h"
#endif

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/ArgumentsObject-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;

inline JSObject *
JSObject::getThrowTypeError() const
{
    return getGlobal()->getThrowTypeError();
}

JSBool
js_GetArgsValue(JSContext *cx, StackFrame *fp, Value *vp)
{
    JSObject *argsobj;

    if (fp->hasOverriddenArgs()) {
        JS_ASSERT(fp->hasCallObj());
        jsid id = ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
        return fp->callObj().getProperty(cx, id, vp);
    }
    argsobj = js_GetArgsObject(cx, fp);
    if (!argsobj)
        return JS_FALSE;
    vp->setObject(*argsobj);
    return JS_TRUE;
}

JSBool
js_GetArgsProperty(JSContext *cx, StackFrame *fp, jsid id, Value *vp)
{
    JS_ASSERT(fp->isFunctionFrame());

    if (fp->hasOverriddenArgs()) {
        JS_ASSERT(fp->hasCallObj());

        jsid argumentsid = ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
        Value v;
        if (!fp->callObj().getProperty(cx, argumentsid, &v))
            return false;

        JSObject *obj;
        if (v.isPrimitive()) {
            obj = js_ValueToNonNullObject(cx, v);
            if (!obj)
                return false;
        } else {
            obj = &v.toObject();
        }
        return obj->getProperty(cx, id, vp);
    }

    vp->setUndefined();
    if (JSID_IS_INT(id)) {
        uint32 arg = uint32(JSID_TO_INT(id));
        ArgumentsObject *argsobj = fp->maybeArgsObj();
        if (arg < fp->numActualArgs()) {
            if (argsobj) {
                const Value &v = argsobj->element(arg);
                if (v.isMagic(JS_ARGS_HOLE))
                    return argsobj->getProperty(cx, id, vp);
                if (fp->functionScript()->strictModeCode) {
                    *vp = v;
                    return true;
                }
            }
            *vp = fp->canonicalActualArg(arg);
        } else {
            /*
             * Per ECMA-262 Ed. 3, 10.1.8, last bulleted item, do not share
             * storage between the formal parameter and arguments[k] for all
             * fp->argc <= k && k < fp->fun->nargs.  For example, in
             *
             *   function f(x) { x = 42; return arguments[0]; }
             *   f();
             *
             * the call to f should return undefined, not 42.  If fp->argsobj
             * is null at this point, as it would be in the example, return
             * undefined in *vp.
             */
            if (argsobj)
                return argsobj->getProperty(cx, id, vp);
        }
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        ArgumentsObject *argsobj = fp->maybeArgsObj();
        if (argsobj && argsobj->hasOverriddenLength())
            return argsobj->getProperty(cx, id, vp);
        vp->setInt32(fp->numActualArgs());
    }
    return true;
}

js::ArgumentsObject *
ArgumentsObject::create(JSContext *cx, uint32 argc, JSObject &callee)
{
    JS_ASSERT(argc <= StackSpace::ARGS_LENGTH_MAX);

    JSObject *proto;
    if (!js_GetClassPrototype(cx, callee.getGlobal(), JSProto_Object, &proto))
        return NULL;

    JS_STATIC_ASSERT(NormalArgumentsObject::RESERVED_SLOTS == 2);
    JS_STATIC_ASSERT(StrictArgumentsObject::RESERVED_SLOTS == 2);
    JSObject *obj = js_NewGCObject(cx, FINALIZE_OBJECT2);
    if (!obj)
        return NULL;

    EmptyShape *emptyArgumentsShape = EmptyShape::getEmptyArgumentsShape(cx);
    if (!emptyArgumentsShape)
        return NULL;
    AutoShapeRooter shapeRoot(cx, emptyArgumentsShape);

    ArgumentsData *data = (ArgumentsData *)
        cx->malloc_(offsetof(ArgumentsData, slots) + argc * sizeof(Value));
    if (!data)
        return NULL;
    SetValueRangeToUndefined(data->slots, argc);

    /* Can't fail from here on, so initialize everything in argsobj. */
    obj->init(cx, callee.getFunctionPrivate()->inStrictMode()
              ? &StrictArgumentsObject::jsClass
              : &NormalArgumentsObject::jsClass,
              proto, proto->getParent(), NULL, false);
    obj->setMap(emptyArgumentsShape);

    ArgumentsObject *argsobj = obj->asArguments();

    JS_ASSERT(UINT32_MAX > (uint64(argc) << PACKED_BITS_COUNT));
    argsobj->setInitialLength(argc);

    argsobj->setCalleeAndData(callee, data);

    return argsobj;
}

struct STATIC_SKIP_INFERENCE PutArg
{
    PutArg(Value *dst) : dst(dst) {}
    Value *dst;
    bool operator()(uintN, Value *src) {
        if (!dst->isMagic(JS_ARGS_HOLE))
            *dst = *src;
        ++dst;
        return true;
    }
};

JSObject *
js_GetArgsObject(JSContext *cx, StackFrame *fp)
{
    /*
     * Arguments and Call objects are owned by the enclosing non-eval function
     * frame, thus any eval frames must be skipped before testing hasArgsObj.
     */
    JS_ASSERT(fp->isFunctionFrame());
    while (fp->isEvalInFunction())
        fp = fp->prev();

    /* Create an arguments object for fp only if it lacks one. */
    JS_ASSERT_IF(fp->fun()->isHeavyweight(), fp->hasCallObj());
    if (fp->hasArgsObj())
        return &fp->argsObj();

    ArgumentsObject *argsobj =
        ArgumentsObject::create(cx, fp->numActualArgs(), fp->callee());
    if (!argsobj)
        return argsobj;

    /*
     * Strict mode functions have arguments objects that copy the initial
     * actual parameter values.  It is the caller's responsibility to get the
     * arguments object before any parameters are modified!  (The emitter
     * ensures this by synthesizing an arguments access at the start of any
     * strict mode function that contains an assignment to a parameter, or
     * that calls eval.)  Non-strict mode arguments use the frame pointer to
     * retrieve up-to-date parameter values.
     */
    if (argsobj->isStrictArguments())
        fp->forEachCanonicalActualArg(PutArg(argsobj->data()->slots));
    else
        argsobj->setPrivate(fp);

    fp->setArgsObj(*argsobj);
    return argsobj;
}

void
js_PutArgsObject(StackFrame *fp)
{
    ArgumentsObject &argsobj = fp->argsObj();
    if (argsobj.isNormalArguments()) {
        JS_ASSERT(argsobj.getPrivate() == fp);
        fp->forEachCanonicalActualArg(PutArg(argsobj.data()->slots));
        argsobj.setPrivate(NULL);
    } else {
        JS_ASSERT(!argsobj.getPrivate());
    }
}

#ifdef JS_TRACER

/*
 * Traced versions of js_GetArgsObject and js_PutArgsObject.
 */
JSObject * JS_FASTCALL
js_NewArgumentsOnTrace(JSContext *cx, uint32 argc, JSObject *callee)
{
    ArgumentsObject *argsobj = ArgumentsObject::create(cx, argc, *callee);
    if (!argsobj)
        return NULL;

    if (argsobj->isStrictArguments()) {
        /*
         * Strict mode callers must copy arguments into the created arguments
         * object. The trace-JITting code is in TraceRecorder::newArguments.
         */
        JS_ASSERT(!argsobj->getPrivate());
    } else {
        argsobj->setPrivate(JS_ARGUMENTS_OBJECT_ON_TRACE);
    }

    return argsobj;
}
JS_DEFINE_CALLINFO_3(extern, OBJECT, js_NewArgumentsOnTrace, CONTEXT, UINT32, OBJECT,
                     0, nanojit::ACCSET_STORE_ANY)

/* FIXME change the return type to void. */
JSBool JS_FASTCALL
js_PutArgumentsOnTrace(JSContext *cx, JSObject *obj, Value *argv)
{
    NormalArgumentsObject *argsobj = obj->asNormalArguments();

    JS_ASSERT(argsobj->getPrivate() == JS_ARGUMENTS_OBJECT_ON_TRACE);

    /*
     * TraceRecorder::putActivationObjects builds a single, contiguous array of
     * the arguments, regardless of whether #actuals > #formals so there is no
     * need to worry about actual vs. formal arguments.
     */
    Value *srcend = argv + argsobj->initialLength();
    Value *dst = argsobj->data()->slots;
    for (Value *src = argv; src < srcend; ++src, ++dst) {
        if (!dst->isMagic(JS_ARGS_HOLE))
            *dst = *src;
    }

    argsobj->setPrivate(NULL);
    return true;
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_PutArgumentsOnTrace, CONTEXT, OBJECT, VALUEPTR, 0,
                     nanojit::ACCSET_STORE_ANY)

#endif /* JS_TRACER */

static JSBool
args_delProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    ArgumentsObject *argsobj = obj->asArguments();
    if (JSID_IS_INT(id)) {
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < argsobj->initialLength())
            argsobj->setElement(arg, MagicValue(JS_ARGS_HOLE));
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        argsobj->markLengthOverridden();
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom)) {
        argsobj->asNormalArguments()->clearCallee();
    }
    return true;
}

static JSBool
ArgGetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    LeaveTrace(cx);

    if (!obj->isNormalArguments())
        return true;

    NormalArgumentsObject *argsobj = obj->asNormalArguments();
    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < argsobj->initialLength()) {
            JS_ASSERT(!argsobj->element(arg).isMagic(JS_ARGS_HOLE));
            if (StackFrame *fp = reinterpret_cast<StackFrame *>(argsobj->getPrivate()))
                *vp = fp->canonicalActualArg(arg);
            else
                *vp = argsobj->element(arg);
        }
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (!argsobj->hasOverriddenLength())
            vp->setInt32(argsobj->initialLength());
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom));
        const Value &v = argsobj->callee();
        if (!v.isMagic(JS_ARGS_HOLE))
            *vp = v;
    }
    return true;
}

static JSBool
ArgSetter(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
#ifdef JS_TRACER
    // To be able to set a property here on trace, we would have to make
    // sure any updates also get written back to the trace native stack.
    // For simplicity, we just leave trace, since this is presumably not
    // a common operation.
    LeaveTrace(cx);
#endif


    if (!obj->isNormalArguments())
        return true;

    NormalArgumentsObject *argsobj = obj->asNormalArguments();

    if (JSID_IS_INT(id)) {
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < argsobj->initialLength()) {
            if (StackFrame *fp = reinterpret_cast<StackFrame *>(argsobj->getPrivate())) {
                JSScript *script = fp->functionScript();
                if (script->usesArguments)
                    fp->canonicalActualArg(arg) = *vp;
                return true;
            }
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
    AutoValueRooter tvr(cx);
    return js_DeleteProperty(cx, argsobj, id, tvr.addr(), false) &&
           js_DefineProperty(cx, argsobj, id, vp, NULL, NULL, JSPROP_ENUMERATE);
}

static JSBool
args_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
             JSObject **objp)
{
    *objp = NULL;

    NormalArgumentsObject *argsobj = obj->asNormalArguments();

    uintN attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    if (JSID_IS_INT(id)) {
        uint32 arg = uint32(JSID_TO_INT(id));
        if (arg >= argsobj->initialLength() || argsobj->element(arg).isMagic(JS_ARGS_HOLE))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (argsobj->hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom))
            return true;

        if (argsobj->callee().isMagic(JS_ARGS_HOLE))
            return true;
    }

    Value undef = UndefinedValue();
    if (!js_DefineProperty(cx, argsobj, id, &undef, ArgGetter, ArgSetter, attrs))
        return JS_FALSE;

    *objp = argsobj;
    return true;
}

static JSBool
args_enumerate(JSContext *cx, JSObject *obj)
{
    NormalArgumentsObject *argsobj = obj->asNormalArguments();

    /*
     * Trigger reflection in args_resolve using a series of js_LookupProperty
     * calls.
     */
    int argc = int(argsobj->initialLength());
    for (int i = -2; i != argc; i++) {
        jsid id = (i == -2)
                  ? ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)
                  : (i == -1)
                  ? ATOM_TO_JSID(cx->runtime->atomState.calleeAtom)
                  : INT_TO_JSID(i);

        JSObject *pobj;
        JSProperty *prop;
        if (!js_LookupProperty(cx, argsobj, id, &pobj, &prop))
            return false;
    }
    return true;
}

static JSBool
StrictArgGetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    LeaveTrace(cx);

    if (!obj->isStrictArguments())
        return true;

    StrictArgumentsObject *argsobj = obj->asStrictArguments();

    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < argsobj->initialLength()) {
            const Value &v = argsobj->element(arg);
            if (!v.isMagic(JS_ARGS_HOLE))
                *vp = v;
        }
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom));
        if (!argsobj->hasOverriddenLength())
            vp->setInt32(argsobj->initialLength());
    }
    return true;
}

static JSBool
StrictArgSetter(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    if (!obj->isStrictArguments())
        return true;

    StrictArgumentsObject *argsobj = obj->asStrictArguments();

    if (JSID_IS_INT(id)) {
        uintN arg = uintN(JSID_TO_INT(id));
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
    AutoValueRooter tvr(cx);
    return js_DeleteProperty(cx, argsobj, id, tvr.addr(), strict) &&
           js_SetPropertyHelper(cx, argsobj, id, 0, vp, strict);
}

static JSBool
strictargs_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags, JSObject **objp)
{
    *objp = NULL;

    StrictArgumentsObject *argsobj = obj->asStrictArguments();

    uintN attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    PropertyOp getter = StrictArgGetter;
    StrictPropertyOp setter = StrictArgSetter;

    if (JSID_IS_INT(id)) {
        uint32 arg = uint32(JSID_TO_INT(id));
        if (arg >= argsobj->initialLength() || argsobj->element(arg).isMagic(JS_ARGS_HOLE))
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
        getter = CastAsPropertyOp(argsobj->getThrowTypeError());
        setter = CastAsStrictPropertyOp(argsobj->getThrowTypeError());
    }

    Value undef = UndefinedValue();
    if (!js_DefineProperty(cx, argsobj, id, &undef, getter, setter, attrs))
        return false;

    *objp = argsobj;
    return true;
}

static JSBool
strictargs_enumerate(JSContext *cx, JSObject *obj)
{
    StrictArgumentsObject *argsobj = obj->asStrictArguments();

    /*
     * Trigger reflection in strictargs_resolve using a series of
     * js_LookupProperty calls.
     */
    JSObject *pobj;
    JSProperty *prop;

    // length
    if (!js_LookupProperty(cx, argsobj, ATOM_TO_JSID(cx->runtime->atomState.lengthAtom), &pobj, &prop))
        return false;

    // callee
    if (!js_LookupProperty(cx, argsobj, ATOM_TO_JSID(cx->runtime->atomState.calleeAtom), &pobj, &prop))
        return false;

    // caller
    if (!js_LookupProperty(cx, argsobj, ATOM_TO_JSID(cx->runtime->atomState.callerAtom), &pobj, &prop))
        return false;

    for (uint32 i = 0, argc = argsobj->initialLength(); i < argc; i++) {
        if (!js_LookupProperty(cx, argsobj, INT_TO_JSID(i), &pobj, &prop))
            return false;
    }

    return true;
}

static void
args_finalize(JSContext *cx, JSObject *obj)
{
    cx->free_(reinterpret_cast<void *>(obj->asArguments()->data()));
}

/*
 * If a generator's arguments or call object escapes, and the generator frame
 * is not executing, the generator object needs to be marked because it is not
 * otherwise reachable. An executing generator is rooted by its invocation.  To
 * distinguish the two cases (which imply different access paths to the
 * generator object), we use the JSFRAME_FLOATING_GENERATOR flag, which is only
 * set on the StackFrame kept in the generator object's JSGenerator.
 */
static inline void
MaybeMarkGenerator(JSTracer *trc, JSObject *obj)
{
#if JS_HAS_GENERATORS
    StackFrame *fp = (StackFrame *) obj->getPrivate();
    if (fp && fp->isFloatingGenerator()) {
        JSObject *genobj = js_FloatingFrameToGenerator(fp)->obj;
        MarkObject(trc, *genobj, "generator object");
    }
#endif
}

static void
args_trace(JSTracer *trc, JSObject *obj)
{
    ArgumentsObject *argsobj = obj->asArguments();
    if (argsobj->getPrivate() == JS_ARGUMENTS_OBJECT_ON_TRACE) {
        JS_ASSERT(!argsobj->isStrictArguments());
        return;
    }

    ArgumentsData *data = argsobj->data();
    if (data->callee.isObject())
        MarkObject(trc, data->callee.toObject(), js_callee_str);
    MarkValueRange(trc, argsobj->initialLength(), data->slots, js_arguments_str);

    MaybeMarkGenerator(trc, argsobj);
}

namespace js {

/*
 * The classes below collaborate to lazily reflect and synchronize actual
 * argument values, argument count, and callee function object stored in a
 * StackFrame with their corresponding property values in the frame's
 * arguments object.
 */
Class NormalArgumentsObject::jsClass = {
    "Arguments",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    PropertyStub,         /* addProperty */
    args_delProperty,
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    args_enumerate,
    reinterpret_cast<JSResolveOp>(args_resolve),
    ConvertStub,
    args_finalize,        /* finalize   */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    args_trace
};

/*
 * Strict mode arguments is significantly less magical than non-strict mode
 * arguments, so it is represented by a different class while sharing some
 * functionality.
 */
Class StrictArgumentsObject::jsClass = {
    "Arguments",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    PropertyStub,         /* addProperty */
    args_delProperty,
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    strictargs_enumerate,
    reinterpret_cast<JSResolveOp>(strictargs_resolve),
    ConvertStub,
    args_finalize,        /* finalize   */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    args_trace
};

}

/*
 * A Declarative Environment object stores its active StackFrame pointer in
 * its private slot, just as Call and Arguments objects do.
 */
Class js_DeclEnvClass = {
    js_Object_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

/*
 * Construct a call object for the given bindings.  If this is a call object
 * for a function invocation, callee should be the function being called.
 * Otherwise it must be a call object for eval of strict mode code, and callee
 * must be null.
 */
static JSObject *
NewCallObject(JSContext *cx, JSScript *script, JSObject &scopeChain, JSObject *callee)
{
    Bindings &bindings = script->bindings;
    size_t argsVars = bindings.countArgsAndVars();
    size_t slots = JSObject::CALL_RESERVED_SLOTS + argsVars;
    gc::FinalizeKind kind = gc::GetGCObjectKind(slots);

    JSObject *callobj = js_NewGCObject(cx, kind);
    if (!callobj)
        return NULL;

    /* Init immediately to avoid GC seeing a half-init'ed object. */
    callobj->initCall(cx, bindings, &scopeChain);
    callobj->makeVarObj();

    /* This must come after callobj->lastProp has been set. */
    if (!callobj->ensureInstanceReservedSlots(cx, argsVars))
        return NULL;

#ifdef DEBUG
    for (Shape::Range r = callobj->lastProp; !r.empty(); r.popFront()) {
        const Shape &s = r.front();
        if (s.slot != SHAPE_INVALID_SLOT) {
            JS_ASSERT(s.slot + 1 == callobj->slotSpan());
            break;
        }
    }
#endif

    callobj->setCallObjCallee(callee);
    return callobj;
}

static inline JSObject *
NewDeclEnvObject(JSContext *cx, StackFrame *fp)
{
    JSObject *envobj = js_NewGCObject(cx, FINALIZE_OBJECT2);
    if (!envobj)
        return NULL;

    EmptyShape *emptyDeclEnvShape = EmptyShape::getEmptyDeclEnvShape(cx);
    if (!emptyDeclEnvShape)
        return NULL;

    envobj->init(cx, &js_DeclEnvClass, NULL, &fp->scopeChain(), fp, false);
    envobj->setMap(emptyDeclEnvShape);
    return envobj;
}

namespace js {

JSObject *
CreateFunCallObject(JSContext *cx, StackFrame *fp)
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
    if (JSAtom *lambdaName = (fp->fun()->flags & JSFUN_LAMBDA) ? fp->fun()->atom : NULL) {
        scopeChain = NewDeclEnvObject(cx, fp);
        if (!scopeChain)
            return NULL;

        if (!DefineNativeProperty(cx, scopeChain, ATOM_TO_JSID(lambdaName),
                                  ObjectValue(fp->callee()), NULL, NULL,
                                  JSPROP_PERMANENT | JSPROP_READONLY, 0, 0)) {
            return NULL;
        }
    }

    JSObject *callobj = NewCallObject(cx, fp->script(), *scopeChain, &fp->callee());
    if (!callobj)
        return NULL;

    callobj->setPrivate(fp);
    fp->setScopeChainWithOwnCallObj(*callobj);
    return callobj;
}

JSObject *
CreateEvalCallObject(JSContext *cx, StackFrame *fp)
{
    JSObject *callobj = NewCallObject(cx, fp->script(), fp->scopeChain(), NULL);
    if (!callobj)
        return NULL;

    callobj->setPrivate(fp);
    fp->setScopeChainWithOwnCallObj(*callobj);
    return callobj;
}

} // namespace js

JSObject * JS_FASTCALL
js_CreateCallObjectOnTrace(JSContext *cx, JSFunction *fun, JSObject *callee, JSObject *scopeChain)
{
    JS_ASSERT(!js_IsNamedLambda(fun));
    JS_ASSERT(scopeChain);
    JS_ASSERT(callee);
    return NewCallObject(cx, fun->script(), *scopeChain, callee);
}

JS_DEFINE_CALLINFO_4(extern, OBJECT, js_CreateCallObjectOnTrace, CONTEXT, FUNCTION, OBJECT, OBJECT,
                     0, nanojit::ACCSET_STORE_ANY)

inline static void
CopyValuesToCallObject(JSObject &callobj, uintN nargs, Value *argv, uintN nvars, Value *slots)
{
    JS_ASSERT(callobj.numSlots() >= JSObject::CALL_RESERVED_SLOTS + nargs + nvars);
    callobj.copySlots(JSObject::CALL_RESERVED_SLOTS, argv, nargs);
    callobj.copySlots(JSObject::CALL_RESERVED_SLOTS + nargs, slots, nvars);
}

void
js_PutCallObject(StackFrame *fp)
{
    JSObject &callobj = fp->callObj();
    JS_ASSERT(callobj.getPrivate() == fp);
    JS_ASSERT_IF(fp->isEvalFrame(), fp->isStrictEvalFrame());
    JS_ASSERT(fp->isEvalFrame() == callobj.callIsForEval());

    /* Get the arguments object to snapshot fp's actual argument values. */
    if (fp->hasArgsObj()) {
        if (!fp->hasOverriddenArgs())
            callobj.setCallObjArguments(ObjectValue(fp->argsObj()));
        js_PutArgsObject(fp);
    }

    JSScript *script = fp->script();
    Bindings &bindings = script->bindings;

    if (callobj.callIsForEval()) {
        JS_ASSERT(script->strictModeCode);
        JS_ASSERT(bindings.countArgs() == 0);

        /* This could be optimized as below, but keep it simple for now. */
        CopyValuesToCallObject(callobj, 0, NULL, bindings.countVars(), fp->slots());
    } else {
        JSFunction *fun = fp->fun();
        JS_ASSERT(fun == callobj.getCallObjCalleeFunction());
        JS_ASSERT(script == fun->script());

        uintN n = bindings.countArgsAndVars();
        if (n > 0) {
            JS_ASSERT(JSObject::CALL_RESERVED_SLOTS + n <= callobj.numSlots());

            uint32 nvars = bindings.countVars();
            uint32 nargs = bindings.countArgs();
            JS_ASSERT(fun->nargs == nargs);
            JS_ASSERT(nvars + nargs == n);

            JSScript *script = fun->script();
            if (script->usesEval
#ifdef JS_METHODJIT
                || script->debugMode
#endif
                ) {
                CopyValuesToCallObject(callobj, nargs, fp->formalArgs(), nvars, fp->slots());
            } else {
                /*
                 * For each arg & var that is closed over, copy it from the stack
                 * into the call object.
                 */
                uint32 nclosed = script->nClosedArgs;
                for (uint32 i = 0; i < nclosed; i++) {
                    uint32 e = script->getClosedArg(i);
                    callobj.setSlot(JSObject::CALL_RESERVED_SLOTS + e, fp->formalArg(e));
                }

                nclosed = script->nClosedVars;
                for (uint32 i = 0; i < nclosed; i++) {
                    uint32 e = script->getClosedVar(i);
                    callobj.setSlot(JSObject::CALL_RESERVED_SLOTS + nargs + e, fp->slots()[e]);
                }
            }
        }

        /* Clear private pointers to fp, which is about to go away (js_Invoke). */
        if (js_IsNamedLambda(fun)) {
            JSObject *env = callobj.getParent();

            JS_ASSERT(env->getClass() == &js_DeclEnvClass);
            JS_ASSERT(env->getPrivate() == fp);
            env->setPrivate(NULL);
        }
    }

    callobj.setPrivate(NULL);
}

JSBool JS_FASTCALL
js_PutCallObjectOnTrace(JSObject *callobj, uint32 nargs, Value *argv,
                        uint32 nvars, Value *slots)
{
    JS_ASSERT(callobj->isCall());
    JS_ASSERT(!callobj->getPrivate());

    uintN n = nargs + nvars;
    if (n != 0)
        CopyValuesToCallObject(*callobj, nargs, argv, nvars, slots);

    return true;
}

JS_DEFINE_CALLINFO_5(extern, BOOL, js_PutCallObjectOnTrace, OBJECT, UINT32, VALUEPTR,
                     UINT32, VALUEPTR, 0, nanojit::ACCSET_STORE_ANY)

namespace js {

static JSBool
GetCallArguments(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    StackFrame *fp = obj->maybeCallObjStackFrame();
    if (fp && !fp->hasOverriddenArgs()) {
        JSObject *argsobj = js_GetArgsObject(cx, fp);
        if (!argsobj)
            return false;
        vp->setObject(*argsobj);
    } else {
        *vp = obj->getCallObjArguments();
    }
    return true;
}

static JSBool
SetCallArguments(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    if (StackFrame *fp = obj->maybeCallObjStackFrame())
        fp->setOverriddenArgs();
    obj->setCallObjArguments(*vp);
    return true;
}

JSBool
GetCallArg(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT((int16) JSID_TO_INT(id) == JSID_TO_INT(id));
    uintN i = (uint16) JSID_TO_INT(id);

    if (StackFrame *fp = obj->maybeCallObjStackFrame())
        *vp = fp->formalArg(i);
    else
        *vp = obj->callObjArg(i);
    return true;
}

JSBool
SetCallArg(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    JS_ASSERT((int16) JSID_TO_INT(id) == JSID_TO_INT(id));
    uintN i = (uint16) JSID_TO_INT(id);

    if (StackFrame *fp = obj->maybeCallObjStackFrame())
        fp->formalArg(i) = *vp;
    else
        obj->setCallObjArg(i, *vp);

    return true;
}

JSBool
GetCallUpvar(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT((int16) JSID_TO_INT(id) == JSID_TO_INT(id));
    uintN i = (uint16) JSID_TO_INT(id);

    *vp = obj->getCallObjCallee()->getFlatClosureUpvar(i);
    return true;
}

JSBool
SetCallUpvar(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    JS_ASSERT((int16) JSID_TO_INT(id) == JSID_TO_INT(id));
    uintN i = (uint16) JSID_TO_INT(id);

    obj->getCallObjCallee()->setFlatClosureUpvar(i, *vp);
    return true;
}

JSBool
GetCallVar(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT((int16) JSID_TO_INT(id) == JSID_TO_INT(id));
    uintN i = (uint16) JSID_TO_INT(id);

    if (StackFrame *fp = obj->maybeCallObjStackFrame())
        *vp = fp->varSlot(i);
    else
        *vp = obj->callObjVar(i);
    return true;
}

JSBool
SetCallVar(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    JS_ASSERT(obj->isCall());

    JS_ASSERT((int16) JSID_TO_INT(id) == JSID_TO_INT(id));
    uintN i = (uint16) JSID_TO_INT(id);

    /*
     * As documented in TraceRecorder::attemptTreeCall(), when recording an
     * inner tree call, the recorder assumes the inner tree does not mutate
     * any tracked upvars. The abort here is a pessimistic precaution against
     * bug 620662, where an inner tree setting a closed stack variable in an
     * outer tree is illegal, and runtime would fall off trace.
     */
#ifdef JS_TRACER
    if (JS_ON_TRACE(cx)) {
        TraceMonitor *tm = JS_TRACE_MONITOR_ON_TRACE(cx);
        if (tm->recorder && tm->tracecx)
            AbortRecording(cx, "upvar write in nested tree");
    }
#endif

    if (StackFrame *fp = obj->maybeCallObjStackFrame())
        fp->varSlot(i) = *vp;
    else
        obj->setCallObjVar(i, *vp);

    return true;
}

} // namespace js

#if JS_TRACER
JSBool JS_FASTCALL
js_SetCallArg(JSContext *cx, JSObject *obj, jsid slotid, ValueArgType arg)
{
    Value argcopy = ValueArgToConstRef(arg);
    return SetCallArg(cx, obj, slotid, false /* STRICT DUMMY */, &argcopy);
}
JS_DEFINE_CALLINFO_4(extern, BOOL, js_SetCallArg, CONTEXT, OBJECT, JSID, VALUE, 0,
                     nanojit::ACCSET_STORE_ANY)

JSBool JS_FASTCALL
js_SetCallVar(JSContext *cx, JSObject *obj, jsid slotid, ValueArgType arg)
{
    Value argcopy = ValueArgToConstRef(arg);
    return SetCallVar(cx, obj, slotid, false /* STRICT DUMMY */, &argcopy);
}
JS_DEFINE_CALLINFO_4(extern, BOOL, js_SetCallVar, CONTEXT, OBJECT, JSID, VALUE, 0,
                     nanojit::ACCSET_STORE_ANY)
#endif

static JSBool
call_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
             JSObject **objp)
{
    JS_ASSERT(obj->isCall());
    JS_ASSERT(!obj->getProto());

    if (!JSID_IS_ATOM(id))
        return true;

    JSObject *callee = obj->getCallObjCallee();
#ifdef DEBUG
    if (callee) {
        JSScript *script = callee->getFunctionPrivate()->script();
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
                                  GetCallArguments, SetCallArguments,
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
    if (StackFrame *fp = obj->maybeCallObjStackFrame()) {
        /*
         * FIXME: Hide copies of stack values rooted by fp from the Cycle
         * Collector, which currently lacks a non-stub Unlink implementation
         * for JS objects (including Call objects), so is unable to collect
         * cycles involving Call objects whose frames are active without this
         * hiding hack.
         */
        uintN first = JSObject::CALL_RESERVED_SLOTS;
        uintN count = fp->script()->bindings.countArgsAndVars();

        JS_ASSERT(obj->numSlots() >= first + count);
        obj->setSlotsUndefined(first, count);
    }

    MaybeMarkGenerator(trc, obj);
}

JS_PUBLIC_DATA(Class) js_CallClass = {
    "Call",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::CALL_RESERVED_SLOTS) |
    JSCLASS_NEW_RESOLVE | JSCLASS_IS_ANONYMOUS,
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    (JSResolveOp)call_resolve,
    NULL,                 /* convert: Leave it NULL so we notice if calls ever escape */
    NULL,                 /* finalize */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    call_trace
};

bool
StackFrame::getValidCalleeObject(JSContext *cx, Value *vp)
{
    if (!isFunctionFrame()) {
        vp->setNull();
        return true;
    }

    JSFunction *fun = this->fun();
    JSObject &funobj = callee();
    vp->setObject(funobj);

    /*
     * Check for an escape attempt by a joined function object, which must go
     * through the frame's |this| object's method read barrier for the method
     * atom by which it was uniquely associated with a property.
     */
    const Value &thisv = functionThis();
    if (thisv.isObject()) {
        JS_ASSERT(funobj.getFunctionPrivate() == fun);

        if (fun->compiledFunObj() == funobj && fun->methodAtom()) {
            JSObject *thisp = &thisv.toObject();
            JSObject *first_barriered_thisp = NULL;

            do {
                /*
                 * While a non-native object is responsible for handling its
                 * entire prototype chain, notable non-natives including dense
                 * and typed arrays have native prototypes, so keep going.
                 */
                if (!thisp->isNative())
                    continue;

                if (thisp->hasMethodBarrier()) {
                    const Shape *shape = thisp->nativeLookup(ATOM_TO_JSID(fun->methodAtom()));
                    if (shape) {
                        /*
                         * Two cases follow: the method barrier was not crossed
                         * yet, so we cross it here; the method barrier *was*
                         * crossed but after the call, in which case we fetch
                         * and validate the cloned (unjoined) funobj from the
                         * method property's slot.
                         *
                         * In either case we must allow for the method property
                         * to have been replaced, or its value overwritten.
                         */
                        if (shape->isMethod() && shape->methodObject() == funobj) {
                            if (!thisp->methodReadBarrier(cx, *shape, vp))
                                return false;
                            overwriteCallee(vp->toObject());
                            return true;
                        }

                        if (shape->hasSlot()) {
                            Value v = thisp->getSlot(shape->slot);
                            JSObject *clone;

                            if (IsFunctionObject(v, &clone) &&
                                clone->getFunctionPrivate() == fun &&
                                clone->hasMethodObj(*thisp))
                            {
                                JS_ASSERT(clone != &funobj);
                                *vp = v;
                                overwriteCallee(*clone);
                                return true;
                            }
                        }
                    }

                    if (!first_barriered_thisp)
                        first_barriered_thisp = thisp;
                }
            } while ((thisp = thisp->getProto()) != NULL);

            if (!first_barriered_thisp)
                return true;

            /*
             * At this point, we couldn't find an already-existing clone (or
             * force to exist a fresh clone) created via thisp's method read
             * barrier, so we must clone fun and store it in fp's callee to
             * avoid re-cloning upon repeated foo.caller access.
             *
             * This must mean the code in js_DeleteProperty could not find this
             * stack frame on the stack when the method was deleted. We've lost
             * track of the method, so we associate it with the first barriered
             * object found starting from thisp on the prototype chain.
             */
            JSObject *newfunobj = CloneFunctionObject(cx, fun, fun->getParent());
            if (!newfunobj)
                return false;
            newfunobj->setMethodObj(*first_barriered_thisp);
            overwriteCallee(*newfunobj);
            vp->setObject(*newfunobj);
            return true;
        }
    }

    return true;
}

static JSBool
fun_getProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    while (!obj->isFunction()) {
        obj = obj->getProto();
        if (!obj)
            return true;
    }
    JSFunction *fun = obj->getFunctionPrivate();

    /* Set to early to null in case of error */
    vp->setNull();

    /* Find fun's top-most activation record. */
    StackFrame *fp = js_GetTopStackFrame(cx);
    if (!fp)
        return true;

    while (!fp->isFunctionFrame() || fp->fun() != fun) {
        fp = fp->prev();
        if (!fp)
            return true;
    }

    if (JSID_IS_ATOM(id, cx->runtime->atomState.argumentsAtom)) {
        /* Warn if strict about f.arguments or equivalent unqualified uses. */
        if (!JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING | JSREPORT_STRICT, js_GetErrorMessage,
                                          NULL, JSMSG_DEPRECATED_USAGE, js_arguments_str)) {
            return false;
        }

        return js_GetArgsValue(cx, fp, vp);
    }

    if (JSID_IS_ATOM(id, cx->runtime->atomState.callerAtom)) {
        if (!fp->prev())
            return true;

        StackFrame *frame = js_GetScriptedCaller(cx, fp->prev());
        if (frame && !frame->getValidCalleeObject(cx, vp))
            return false;

        if (!vp->isObject()) {
            JS_ASSERT(vp->isNull());
            return true;
        }

        /* Censor the caller if it is from another compartment. */
        JSObject &caller = vp->toObject();
        if (caller.compartment() != cx->compartment) {
            vp->setNull();
        } else if (caller.isFunction()) {
            JSFunction *callerFun = caller.getFunctionPrivate();
            if (callerFun->isInterpreted() && callerFun->inStrictMode()) {
                JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                             JSMSG_CALLER_IS_STRICT);
                return false;
            }
        }

        return true;
    }

    JS_NOT_REACHED("fun_getProperty");
    return false;
}



/* NB: no sentinels at ends -- use JS_ARRAY_LENGTH to bound loops.
 * Properties censored into [[ThrowTypeError]] in strict mode. */
static const uint16 poisonPillProps[] = {
    ATOM_OFFSET(arguments),
    ATOM_OFFSET(caller),
};

static JSBool
fun_enumerate(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isFunction());

    jsid id;
    bool found;

    if (!obj->isBoundFunction()) {
        id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
        if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
            return false;
    }

    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
        return false;
        
    id = ATOM_TO_JSID(cx->runtime->atomState.nameAtom);
    if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
        return false;

    for (uintN i = 0; i < JS_ARRAY_LENGTH(poisonPillProps); i++) {
        const uint16 offset = poisonPillProps[i];
        id = ATOM_TO_JSID(OFFSET_TO_ATOM(cx->runtime, offset));
        if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
            return false;
    }

    return true;
}

static JSObject *
ResolveInterpretedFunctionPrototype(JSContext *cx, JSObject *obj)
{
#ifdef DEBUG
    JSFunction *fun = obj->getFunctionPrivate();
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!fun->isFunctionPrototype());
#endif

    /*
     * Assert that fun is not a compiler-created function object, which
     * must never leak to script or embedding code and then be mutated.
     * Also assert that obj is not bound, per the ES5 15.3.4.5 ref above.
     */
    JS_ASSERT(!IsInternalFunctionObject(obj));
    JS_ASSERT(!obj->isBoundFunction());

    /*
     * Make the prototype object an instance of Object with the same parent
     * as the function object itself.
     */
    JSObject *parent = obj->getParent();
    JSObject *proto;
    if (!js_GetClassPrototype(cx, parent, JSProto_Object, &proto))
        return NULL;
    proto = NewNativeClassInstance(cx, &js_ObjectClass, proto, parent);
    if (!proto)
        return NULL;

    /*
     * Per ES5 15.3.5.2 a user-defined function's .prototype property is
     * initially non-configurable, non-enumerable, and writable.  Per ES5 13.2
     * the prototype's .constructor property is configurable, non-enumerable,
     * and writable.
     */
    if (!obj->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                             ObjectValue(*proto), PropertyStub, StrictPropertyStub,
                             JSPROP_PERMANENT) ||
        !proto->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.constructorAtom),
                               ObjectValue(*obj), PropertyStub, StrictPropertyStub, 0))
    {
       return NULL;
    }

    return proto;
}

static JSBool
fun_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
            JSObject **objp)
{
    if (!JSID_IS_ATOM(id))
        return true;

    JSFunction *fun = obj->getFunctionPrivate();

    if (JSID_IS_ATOM(id, cx->runtime->atomState.classPrototypeAtom)) {
        /*
         * Native or "built-in" functions do not have a .prototype property per
         * ECMA-262, or (Object.prototype, Function.prototype, etc.) have that
         * property created eagerly.
         *
         * ES5 15.3.4: the non-native function object named Function.prototype
         * does not have a .prototype property.
         *
         * ES5 15.3.4.5: bound functions don't have a prototype property. The
         * isNative() test covers this case because bound functions are native
         * functions by definition/construction.
         */
        if (fun->isNative() || fun->isFunctionPrototype())
            return true;

        if (!ResolveInterpretedFunctionPrototype(cx, obj))
            return false;
        *objp = obj;
        return true;
    }

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom) ||
        JSID_IS_ATOM(id, cx->runtime->atomState.nameAtom)) {
        JS_ASSERT(!IsInternalFunctionObject(obj));

        Value v;
        if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
            v.setInt32(fun->nargs);
        else
            v.setString(fun->atom ? fun->atom : cx->runtime->emptyString);
        
        if (!DefineNativeProperty(cx, obj, id, v, PropertyStub, StrictPropertyStub,
                                  JSPROP_PERMANENT | JSPROP_READONLY, 0, 0)) {
            return false;
        }
        *objp = obj;
        return true;
    }

    for (uintN i = 0; i < JS_ARRAY_LENGTH(poisonPillProps); i++) {
        const uint16 offset = poisonPillProps[i];

        if (JSID_IS_ATOM(id, OFFSET_TO_ATOM(cx->runtime, offset))) {
            JS_ASSERT(!IsInternalFunctionObject(obj));

            PropertyOp getter;
            StrictPropertyOp setter;
            uintN attrs = JSPROP_PERMANENT;
            if (fun->isInterpreted() ? fun->inStrictMode() : obj->isBoundFunction()) {
                JSObject *throwTypeError = obj->getThrowTypeError();

                getter = CastAsPropertyOp(throwTypeError);
                setter = CastAsStrictPropertyOp(throwTypeError);
                attrs |= JSPROP_GETTER | JSPROP_SETTER;
            } else {
                getter = fun_getProperty;
                setter = StrictPropertyStub;
            }

            if (!DefineNativeProperty(cx, obj, id, UndefinedValue(), getter, setter,
                                      attrs, 0, 0)) {
                return false;
            }
            *objp = obj;
            return true;
        }
    }

    return true;
}

#if JS_HAS_XDR

/* XXX store parent and proto, if defined */
JSBool
js_XDRFunctionObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    JSFunction *fun;
    uint32 firstword;           /* flag telling whether fun->atom is non-null,
                                   plus for fun->u.i.skipmin, fun->u.i.wrapper,
                                   and 14 bits reserved for future use */
    uint32 flagsword;           /* word for argument count and fun->flags */

    cx = xdr->cx;
    if (xdr->mode == JSXDR_ENCODE) {
        fun = (*objp)->getFunctionPrivate();
        if (!fun->isInterpreted()) {
            JSAutoByteString funNameBytes;
            if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_SCRIPTED_FUNCTION,
                                     name);
            }
            return false;
        }
        if (fun->u.i.wrapper) {
            JSAutoByteString funNameBytes;
            if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes))
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_XDR_CLOSURE_WRAPPER, name);
            return false;
        }
        JS_ASSERT((fun->u.i.wrapper & ~1U) == 0);
        firstword = (fun->u.i.skipmin << 2) | (fun->u.i.wrapper << 1) | !!fun->atom;
        flagsword = (fun->nargs << 16) | fun->flags;
    } else {
        fun = js_NewFunction(cx, NULL, NULL, 0, JSFUN_INTERPRETED, NULL, NULL);
        if (!fun)
            return false;
        fun->clearParent();
        fun->clearProto();
    }

    AutoObjectRooter tvr(cx, fun);

    if (!JS_XDRUint32(xdr, &firstword))
        return false;
    if ((firstword & 1U) && !js_XDRAtom(xdr, &fun->atom))
        return false;
    if (!JS_XDRUint32(xdr, &flagsword))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        fun->nargs = flagsword >> 16;
        JS_ASSERT((flagsword & JSFUN_KINDMASK) >= JSFUN_INTERPRETED);
        fun->flags = uint16(flagsword);
        fun->u.i.skipmin = uint16(firstword >> 2);
        fun->u.i.wrapper = JSPackedBool((firstword >> 1) & 1);
    }

    /*
     * Don't directly store into fun->u.i.script because we want this to happen
     * at the same time as we set the script's owner.
     */
    JSScript *script = fun->u.i.script;
    if (!js_XDRScript(xdr, &script))
        return false;
    fun->u.i.script = script;

    if (xdr->mode == JSXDR_DECODE) {
        *objp = fun;
        fun->u.i.script->setOwnerObject(fun);
#ifdef CHECK_SCRIPT_OWNER
        fun->script()->owner = NULL;
#endif
        JS_ASSERT(fun->nargs == fun->script()->bindings.countArgs());
        js_CallNewScriptHook(cx, fun->script(), fun);
    }

    return true;
}

#else  /* !JS_HAS_XDR */

#define js_XDRFunctionObject NULL

#endif /* !JS_HAS_XDR */

/*
 * [[HasInstance]] internal method for Function objects: fetch the .prototype
 * property of its 'this' parameter, and walks the prototype chain of v (only
 * if v is an object) returning true if .prototype is found.
 */
static JSBool
fun_hasInstance(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp)
{
    while (obj->isFunction()) {
        if (!obj->isBoundFunction())
            break;
        obj = obj->getBoundFunctionTarget();
    }

    jsid id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    Value pval;
    if (!obj->getProperty(cx, id, &pval))
        return JS_FALSE;

    if (pval.isPrimitive()) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        js_ReportValueError(cx, JSMSG_BAD_PROTOTYPE, -1, ObjectValue(*obj), NULL);
        return JS_FALSE;
    }

    *bp = js_IsDelegate(cx, &pval.toObject(), *v);
    return JS_TRUE;
}

static void
fun_trace(JSTracer *trc, JSObject *obj)
{
    /* A newborn function object may have a not yet initialized private slot. */
    JSFunction *fun = (JSFunction *) obj->getPrivate();
    if (!fun)
        return;

    if (fun != obj) {
        /* obj is a cloned function object, trace the clone-parent, fun. */
        MarkObject(trc, *fun, "private");

        /* The function could be a flat closure with upvar copies in the clone. */
        if (fun->isFlatClosure() && fun->script()->bindings.hasUpvars()) {
            MarkValueRange(trc, fun->script()->bindings.countUpvars(),
                           obj->getFlatClosureUpvars(), "upvars");
        }
        return;
    }

    if (fun->atom)
        MarkString(trc, fun->atom, "atom");

    if (fun->isInterpreted() && fun->script())
        js_TraceScript(trc, fun->script(), obj);
}

static void
fun_finalize(JSContext *cx, JSObject *obj)
{
    /* Ignore newborn function objects. */
    JSFunction *fun = obj->getFunctionPrivate();
    if (!fun)
        return;

    /* Cloned function objects may be flat closures with upvars to free. */
    if (fun != obj) {
        if (fun->isFlatClosure() && fun->script()->bindings.hasUpvars())
            cx->free_((void *) obj->getFlatClosureUpvars());
        return;
    }

    /*
     * Null-check fun->script() because the parser sets interpreted very early.
     */
    if (fun->isInterpreted() && fun->script())
        js_DestroyScriptFromGC(cx, fun->script(), obj);
}

/*
 * Reserve two slots in all function objects for XPConnect.  Note that this
 * does not bloat every instance, only those on which reserved slots are set,
 * and those on which ad-hoc properties are defined.
 */
JS_PUBLIC_DATA(Class) js_FunctionClass = {
    js_Function_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(JSFunction::CLASS_RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Function),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    fun_enumerate,
    (JSResolveOp)fun_resolve,
    ConvertStub,
    fun_finalize,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,
    fun_hasInstance,
    fun_trace
};

JSString *
fun_toStringHelper(JSContext *cx, JSObject *obj, uintN indent)
{
    if (!obj->isFunction()) {
        if (obj->isFunctionProxy())
            return JSProxy::fun_toString(cx, obj, indent);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, js_toString_str,
                             "object");
        return NULL;
    }

    JSFunction *fun = obj->getFunctionPrivate();
    if (!fun)
        return NULL;

    if (!indent && !cx->compartment->toSourceCache.empty()) {
        ToSourceCache::Ptr p = cx->compartment->toSourceCache.ref().lookup(fun);
        if (p)
            return p->value;
    }

    JSString *str = JS_DecompileFunction(cx, fun, indent);
    if (!str)
        return NULL;

    if (!indent) {
        Maybe<ToSourceCache> &lazy = cx->compartment->toSourceCache;

        if (lazy.empty()) {
            lazy.construct();
            if (!lazy.ref().init())
                return NULL;
        }

        if (!lazy.ref().put(fun, str))
            return NULL;
    }

    return str;
}

static JSBool
fun_toString(JSContext *cx, uintN argc, Value *vp)
{
    JS_ASSERT(IsFunctionObject(vp[0]));
    uint32_t indent = 0;

    if (argc != 0 && !ValueToECMAUint32(cx, vp[2], &indent))
        return false;

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    JSString *str = fun_toStringHelper(cx, obj, indent);
    if (!str)
        return false;

    vp->setString(str);
    return true;
}

#if JS_HAS_TOSOURCE
static JSBool
fun_toSource(JSContext *cx, uintN argc, Value *vp)
{
    JS_ASSERT(IsFunctionObject(vp[0]));

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    JSString *str = fun_toStringHelper(cx, obj, JS_DONT_PRETTY_PRINT);
    if (!str)
        return false;

    vp->setString(str);
    return true;
}
#endif

JSBool
js_fun_call(JSContext *cx, uintN argc, Value *vp)
{
    LeaveTrace(cx);
    Value fval = vp[1];

    if (!js_IsCallable(fval)) {
        ReportIncompatibleMethod(cx, vp, &js_FunctionClass);
        return false;
    }

    Value *argv = vp + 2;
    Value thisv;
    if (argc == 0) {
        thisv.setUndefined();
    } else {
        thisv = argv[0];

        argc--;
        argv++;
    }

    /* Allocate stack space for fval, obj, and the args. */
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args))
        return JS_FALSE;

    /* Push fval, thisv, and the args. */
    args.calleev() = fval;
    args.thisv() = thisv;
    memcpy(args.argv(), argv, argc * sizeof *argv);

    bool ok = Invoke(cx, args);
    *vp = args.rval();
    return ok;
}

/* ES5 15.3.4.3 */
JSBool
js_fun_apply(JSContext *cx, uintN argc, Value *vp)
{
    /* Step 1. */
    Value fval = vp[1];
    if (!js_IsCallable(fval)) {
        ReportIncompatibleMethod(cx, vp, &js_FunctionClass);
        return false;
    }

    /* Step 2. */
    if (argc < 2 || vp[3].isNullOrUndefined())
        return js_fun_call(cx, (argc > 0) ? 1 : 0, vp);

    /* N.B. Changes need to be propagated to stubs::SplatApplyArgs. */

    /* Step 3. */
    if (!vp[3].isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS, js_apply_str);
        return false;
    }

    /*
     * Steps 4-5 (note erratum removing steps originally numbered 5 and 7 in
     * original version of ES5).
     */
    JSObject *aobj = &vp[3].toObject();
    jsuint length;
    if (!js_GetLengthProperty(cx, aobj, &length))
        return false;

    LeaveTrace(cx);

    /* Step 6. */
    if (length > StackSpace::ARGS_LENGTH_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TOO_MANY_FUN_APPLY_ARGS);
        return false;
    }

    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, length, &args))
        return false;

    /* Push fval, obj, and aobj's elements as args. */
    args.calleev() = fval;
    args.thisv() = vp[2];

    /* Steps 7-8. */
    if (!GetElements(cx, aobj, length, args.argv()))
        return false;

    /* Step 9. */
    if (!Invoke(cx, args))
        return false;
    *vp = args.rval();
    return true;
}

namespace js {

JSBool
CallOrConstructBoundFunction(JSContext *cx, uintN argc, Value *vp);

}

inline bool
JSObject::initBoundFunction(JSContext *cx, const Value &thisArg,
                            const Value *args, uintN argslen)
{
    JS_ASSERT(isFunction());

    flags |= JSObject::BOUND_FUNCTION;
    setSlot(JSSLOT_BOUND_FUNCTION_THIS, thisArg);
    setSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT, PrivateUint32Value(argslen));
    if (argslen != 0) {
        /* FIXME? Burn memory on an empty scope whose shape covers the args slots. */
        EmptyShape *empty = EmptyShape::create(cx, getClass());
        if (!empty)
            return false;

        empty->slotSpan += argslen;
        setMap(empty);

        if (!ensureInstanceReservedSlots(cx, argslen))
            return false;

        JS_ASSERT(numSlots() >= argslen + FUN_CLASS_RESERVED_SLOTS);
        copySlots(FUN_CLASS_RESERVED_SLOTS, args, argslen);
    }
    return true;
}

inline JSObject *
JSObject::getBoundFunctionTarget() const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    /* Bound functions abuse |parent| to store their target function. */
    return getParent();
}

inline const js::Value &
JSObject::getBoundFunctionThis() const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    return getSlot(JSSLOT_BOUND_FUNCTION_THIS);
}

inline const js::Value *
JSObject::getBoundFunctionArguments(uintN &argslen) const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    argslen = getSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT).toPrivateUint32();
    JS_ASSERT_IF(argslen > 0, numSlots() >= argslen);

    return getSlots() + FUN_CLASS_RESERVED_SLOTS;
}

namespace js {

/* ES5 15.3.4.5.1 and 15.3.4.5.2. */
JSBool
CallOrConstructBoundFunction(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = &vp[0].toObject();
    JS_ASSERT(obj->isFunction());
    JS_ASSERT(obj->isBoundFunction());

    LeaveTrace(cx);

    bool constructing = IsConstructing(vp);

    /* 15.3.4.5.1 step 1, 15.3.4.5.2 step 3. */
    uintN argslen;
    const Value *boundArgs = obj->getBoundFunctionArguments(argslen);

    if (argc + argslen > StackSpace::ARGS_LENGTH_MAX) {
        js_ReportAllocationOverflow(cx);
        return false;
    }

    /* 15.3.4.5.1 step 3, 15.3.4.5.2 step 1. */
    JSObject *target = obj->getBoundFunctionTarget();

    /* 15.3.4.5.1 step 2. */
    const Value &boundThis = obj->getBoundFunctionThis();

    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc + argslen, &args))
        return false;

    /* 15.3.4.5.1, 15.3.4.5.2 step 4. */
    memcpy(args.argv(), boundArgs, argslen * sizeof(Value));
    memcpy(args.argv() + argslen, vp + 2, argc * sizeof(Value));

    /* 15.3.4.5.1, 15.3.4.5.2 step 5. */
    args.calleev().setObject(*target);

    if (!constructing)
        args.thisv() = boundThis;

    if (constructing ? !InvokeConstructor(cx, args) : !Invoke(cx, args))
        return false;

    *vp = args.rval();
    return true;
}

}

#if JS_HAS_GENERATORS
static JSBool
fun_isGenerator(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *funobj;
    if (!IsFunctionObject(vp[1], &funobj)) {
        JS_SET_RVAL(cx, vp, BooleanValue(false));
        return true;
    }

    JSFunction *fun = funobj->getFunctionPrivate();

    bool result = false;
    if (fun->isInterpreted()) {
        JSScript *script = fun->script();
        JS_ASSERT(script->length != 0);
        result = script->code[0] == JSOP_GENERATOR;
    }

    JS_SET_RVAL(cx, vp, BooleanValue(result));
    return true;
}
#endif

/* ES5 15.3.4.5. */
static JSBool
fun_bind(JSContext *cx, uintN argc, Value *vp)
{
    /* Step 1. */
    Value &thisv = vp[1];

    /* Step 2. */
    if (!js_IsCallable(thisv)) {
        ReportIncompatibleMethod(cx, vp, &js_FunctionClass);
        return false;
    }

    JSObject *target = &thisv.toObject();

    /* Step 3. */
    Value *args = NULL;
    uintN argslen = 0;
    if (argc > 1) {
        args = vp + 3;
        argslen = argc - 1;
    }

    /* Steps 15-16. */
    uintN length = 0;
    if (target->isFunction()) {
        uintN nargs = target->getFunctionPrivate()->nargs;
        if (nargs > argslen)
            length = nargs - argslen;
    }

    /* Step 4-6, 10-11. */
    JSAtom *name = target->isFunction() ? target->getFunctionPrivate()->atom : NULL;

    /* NB: Bound functions abuse |parent| to store their target. */
    JSObject *funobj =
        js_NewFunction(cx, NULL, CallOrConstructBoundFunction, length,
                       JSFUN_CONSTRUCTOR, target, name);
    if (!funobj)
        return false;

    /* Steps 7-9. */
    Value thisArg = argc >= 1 ? vp[2] : UndefinedValue();
    if (!funobj->initBoundFunction(cx, thisArg, args, argslen))
        return false;

    /* Steps 17, 19-21 are handled by fun_resolve. */
    /* Step 18 is the default for new functions. */

    /* Step 22. */
    vp->setObject(*funobj);
    return true;
}

static JSFunctionSpec function_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   fun_toSource,   0,0),
#endif
    JS_FN(js_toString_str,   fun_toString,   0,0),
    JS_FN(js_apply_str,      js_fun_apply,   2,0),
    JS_FN(js_call_str,       js_fun_call,    1,0),
    JS_FN("bind",            fun_bind,       1,0),
#if JS_HAS_GENERATORS
    JS_FN("isGenerator",     fun_isGenerator,0,0),
#endif
    JS_FS_END
};

/*
 * Report "malformed formal parameter" iff no illegal char or similar scanner
 * error was already reported.
 */
static bool
OnBadFormal(JSContext *cx, TokenKind tt)
{
    if (tt != TOK_ERROR)
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_FORMAL);
    else
        JS_ASSERT(cx->isExceptionPending());
    return false;
}

static JSBool
Function(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs call = CallArgsFromVp(argc, vp);

    /* Block this call if security callbacks forbid it. */
    GlobalObject *global = call.callee().getGlobal();
    if (!global->isRuntimeCodeGenEnabled(cx)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CSP_BLOCKED_FUNCTION);
        return false;
    }

    JS::Anchor<JSObject *> obj(NewFunction(cx, *global));
    if (!obj.get())
        return false;

    /*
     * NB: (new Function) is not lexically closed by its caller, it's just an
     * anonymous function in the top-level scope that its constructor inhabits.
     * Thus 'var x = 42; f = new Function("return x"); print(f())' prints 42,
     * and so would a call to f from another top-level's script or function.
     */
    JSFunction *fun = js_NewFunction(cx, obj.get(), NULL, 0, JSFUN_LAMBDA | JSFUN_INTERPRETED,
                                     global, cx->runtime->atomState.anonymousAtom);
    if (!fun)
        return false;

    EmptyShape *emptyCallShape = EmptyShape::getEmptyCallShape(cx);
    if (!emptyCallShape)
        return false;
    AutoShapeRooter shapeRoot(cx, emptyCallShape);

    Bindings bindings(cx, emptyCallShape);
    AutoBindingsRooter root(cx, bindings);

    uintN lineno;
    const char *filename = CurrentScriptFileAndLine(cx, &lineno);

    Value *argv = call.argv();
    uintN n = argc ? argc - 1 : 0;
    if (n > 0) {
        /*
         * Collect the function-argument arguments into one string, separated
         * by commas, then make a tokenstream from that string, and scan it to
         * get the arguments.  We need to throw the full scanner at the
         * problem, because the argument string can legitimately contain
         * comments and linefeeds.  XXX It might be better to concatenate
         * everything up into a function definition and pass it to the
         * compiler, but doing it this way is less of a delta from the old
         * code.  See ECMA 15.3.2.1.
         */
        size_t args_length = 0;
        for (uintN i = 0; i < n; i++) {
            /* Collect the lengths for all the function-argument arguments. */
            JSString *arg = js_ValueToString(cx, argv[i]);
            if (!arg)
                return false;
            argv[i].setString(arg);

            /*
             * Check for overflow.  The < test works because the maximum
             * JSString length fits in 2 fewer bits than size_t has.
             */
            size_t old_args_length = args_length;
            args_length = old_args_length + arg->length();
            if (args_length < old_args_length) {
                js_ReportAllocationOverflow(cx);
                return false;
            }
        }

        /* Add 1 for each joining comma and check for overflow (two ways). */
        size_t old_args_length = args_length;
        args_length = old_args_length + n - 1;
        if (args_length < old_args_length ||
            args_length >= ~(size_t)0 / sizeof(jschar)) {
            js_ReportAllocationOverflow(cx);
            return false;
        }

        /*
         * Allocate a string to hold the concatenated arguments, including room
         * for a terminating 0.  Mark cx->tempPool for later release, to free
         * collected_args and its tokenstream in one swoop.
         */
        AutoArenaAllocator aaa(&cx->tempPool);
        jschar *cp = aaa.alloc<jschar>(args_length + 1);
        if (!cp) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        jschar *collected_args = cp;

        /*
         * Concatenate the arguments into the new string, separated by commas.
         */
        for (uintN i = 0; i < n; i++) {
            JSString *arg = argv[i].toString();
            size_t arg_length = arg->length();
            const jschar *arg_chars = arg->getChars(cx);
            if (!arg_chars)
                return false;
            (void) js_strncpy(cp, arg_chars, arg_length);
            cp += arg_length;

            /* Add separating comma or terminating 0. */
            *cp++ = (i + 1 < n) ? ',' : 0;
        }

        /* Initialize a tokenstream that reads from the given string. */
        TokenStream ts(cx);
        if (!ts.init(collected_args, args_length, filename, lineno, cx->findVersion()))
            return false;

        /* The argument string may be empty or contain no tokens. */
        TokenKind tt = ts.getToken();
        if (tt != TOK_EOF) {
            for (;;) {
                /*
                 * Check that it's a name.  This also implicitly guards against
                 * TOK_ERROR, which was already reported.
                 */
                if (tt != TOK_NAME)
                    return OnBadFormal(cx, tt);

                /*
                 * Get the atom corresponding to the name from the token
                 * stream; we're assured at this point that it's a valid
                 * identifier.
                 */
                JSAtom *atom = ts.currentToken().t_atom;

                /* Check for a duplicate parameter name. */
                if (bindings.hasBinding(cx, atom)) {
                    JSAutoByteString name;
                    if (!js_AtomToPrintableString(cx, atom, &name))
                        return false;
                    if (!ReportCompileErrorNumber(cx, &ts, NULL,
                                                  JSREPORT_WARNING | JSREPORT_STRICT,
                                                  JSMSG_DUPLICATE_FORMAL, name.ptr())) {
                        return false;
                    }
                }

                uint16 dummy;
                if (!bindings.addArgument(cx, atom, &dummy))
                    return false;

                /*
                 * Get the next token.  Stop on end of stream.  Otherwise
                 * insist on a comma, get another name, and iterate.
                 */
                tt = ts.getToken();
                if (tt == TOK_EOF)
                    break;
                if (tt != TOK_COMMA)
                    return OnBadFormal(cx, tt);
                tt = ts.getToken();
            }
        }
    }

    JS::Anchor<JSString *> strAnchor(NULL);
    const jschar *chars;
    size_t length;

    if (argc) {
        JSString *str = js_ValueToString(cx, argv[argc - 1]);
        if (!str)
            return false;
        strAnchor.set(str);
        chars = str->getChars(cx);
        length = str->length();
    } else {
        chars = cx->runtime->emptyString->chars();
        length = 0;
    }

    JSPrincipals *principals = PrincipalsForCompiledCode(call, cx);
    bool ok = Compiler::compileFunctionBody(cx, fun, principals, &bindings,
                                            chars, length, filename, lineno,
                                            cx->findVersion());
    call.rval().setObject(obj);
    return ok;
}

namespace js {

bool
IsBuiltinFunctionConstructor(JSFunction *fun)
{
    return fun->maybeNative() == Function;
}

const Shape *
LookupInterpretedFunctionPrototype(JSContext *cx, JSObject *funobj)
{
#ifdef DEBUG
    JSFunction *fun = funobj->getFunctionPrivate();
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!fun->isFunctionPrototype());
    JS_ASSERT(!funobj->isBoundFunction());
#endif

    jsid id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    const Shape *shape = funobj->nativeLookup(id);
    if (!shape) {
        if (!ResolveInterpretedFunctionPrototype(cx, funobj))
            return NULL;
        shape = funobj->nativeLookup(id);
    }
    JS_ASSERT(!shape->configurable());
    JS_ASSERT(shape->isDataDescriptor());
    JS_ASSERT(shape->hasSlot());
    JS_ASSERT(!shape->isMethod());
    return shape;
}

} /* namespace js */

static JSBool
ThrowTypeError(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                 JSMSG_THROW_TYPE_ERROR);
    return false;
}

JSObject *
js_InitFunctionClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto = js_InitClass(cx, obj, NULL, &js_FunctionClass, Function, 1,
                                   NULL, function_methods, NULL, NULL);
    if (!proto)
        return NULL;

    JSFunction *fun = js_NewFunction(cx, proto, NULL, 0, JSFUN_INTERPRETED, obj, NULL);
    if (!fun)
        return NULL;
    fun->flags |= JSFUN_PROTOTYPE;

    JSScript *script = JSScript::NewScript(cx, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, JSVERSION_DEFAULT);
    if (!script)
        return NULL;
    script->noScriptRval = true;
    script->code[0] = JSOP_STOP;
    script->code[1] = SRC_NULL;
#ifdef CHECK_SCRIPT_OWNER
    script->owner = NULL;
#endif
    fun->u.i.script = script;
    script->setOwnerObject(fun);
    js_CallNewScriptHook(cx, script, fun);

    if (obj->isGlobal()) {
        /* ES5 13.2.3: Construct the unique [[ThrowTypeError]] function object. */
        JSFunction *throwTypeError =
            js_NewFunction(cx, NULL, reinterpret_cast<Native>(ThrowTypeError), 0,
                           0, obj, NULL);
        if (!throwTypeError)
            return NULL;

        obj->asGlobal()->setThrowTypeError(throwTypeError);
    }

    return proto;
}

JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, Native native, uintN nargs,
               uintN flags, JSObject *parent, JSAtom *atom)
{
    JSFunction *fun;

    if (funobj) {
        JS_ASSERT(funobj->isFunction());
        funobj->setParent(parent);
    } else {
        funobj = NewFunction(cx, parent);
        if (!funobj)
            return NULL;
    }
    JS_ASSERT(!funobj->getPrivate());
    fun = (JSFunction *) funobj;

    /* Initialize all function members. */
    fun->nargs = uint16(nargs);
    fun->flags = flags & (JSFUN_FLAGS_MASK | JSFUN_KINDMASK | JSFUN_TRCINFO);
    if ((flags & JSFUN_KINDMASK) >= JSFUN_INTERPRETED) {
        JS_ASSERT(!native);
        JS_ASSERT(nargs == 0);
        fun->u.i.skipmin = 0;
        fun->u.i.wrapper = false;
        fun->u.i.script = NULL;
    } else {
        fun->u.n.clasp = NULL;
        if (flags & JSFUN_TRCINFO) {
#ifdef JS_TRACER
            JSNativeTraceInfo *trcinfo =
                JS_FUNC_TO_DATA_PTR(JSNativeTraceInfo *, native);
            fun->u.n.native = (js::Native) trcinfo->native;
            fun->u.n.trcinfo = trcinfo;
#else
            fun->u.n.trcinfo = NULL;
#endif
        } else {
            fun->u.n.native = native;
            fun->u.n.trcinfo = NULL;
        }
        JS_ASSERT(fun->u.n.native);
    }
    fun->atom = atom;

    /* Set private to self to indicate non-cloned fully initialized function. */
    fun->setPrivate(fun);
    return fun;
}

JSObject * JS_FASTCALL
js_CloneFunctionObject(JSContext *cx, JSFunction *fun, JSObject *parent,
                       JSObject *proto)
{
    JS_ASSERT(parent);
    JS_ASSERT(proto);

    JSObject *clone;
    if (cx->compartment == fun->compartment()) {
        /*
         * The cloned function object does not need the extra JSFunction members
         * beyond JSObject as it points to fun via the private slot.
         */
        clone = NewNativeClassInstance(cx, &js_FunctionClass, proto, parent);
        if (!clone)
            return NULL;
        clone->setPrivate(fun);
    } else {
        /*
         * Across compartments we have to deep copy JSFunction and clone the
         * script (for interpreted functions).
         */
        clone = NewFunction(cx, parent);
        if (!clone)
            return NULL;
        JSFunction *cfun = (JSFunction *) clone;
        cfun->nargs = fun->nargs;
        cfun->flags = fun->flags;
        cfun->u = fun->getFunctionPrivate()->u;
        cfun->atom = fun->atom;
        clone->setPrivate(cfun);
        if (cfun->isInterpreted()) {
            JSScript *script = cfun->script();
            JS_ASSERT(script);
            JS_ASSERT(script->compartment == fun->compartment());
            JS_ASSERT(script->compartment != cx->compartment);
            JS_OPT_ASSERT(script->ownerObject == fun);

            JSScript *cscript = js_CloneScript(cx, script);
            if (!cscript)
                return NULL;
            cfun->u.i.script = cscript;
            cfun->script()->setOwnerObject(cfun);
#ifdef CHECK_SCRIPT_OWNER
            cfun->script()->owner = NULL;
#endif
            js_CallNewScriptHook(cx, cfun->script(), cfun);
            Debugger::onNewScript(cx, cfun->script(), cfun, Debugger::NewHeldScript);
        }
    }
    return clone;
}

#ifdef JS_TRACER
JS_DEFINE_CALLINFO_4(extern, OBJECT, js_CloneFunctionObject, CONTEXT, FUNCTION, OBJECT, OBJECT, 0,
                     nanojit::ACCSET_STORE_ANY)
#endif

/*
 * Create a new flat closure, but don't initialize the imported upvar
 * values. The tracer calls this function and then initializes the upvar
 * slots on trace.
 */
JSObject * JS_FASTCALL
js_AllocFlatClosure(JSContext *cx, JSFunction *fun, JSObject *scopeChain)
{
    JS_ASSERT(fun->isFlatClosure());
    JS_ASSERT(JSScript::isValidOffset(fun->script()->upvarsOffset) ==
              fun->script()->bindings.hasUpvars());
    JS_ASSERT_IF(JSScript::isValidOffset(fun->script()->upvarsOffset),
                 fun->script()->upvars()->length == fun->script()->bindings.countUpvars());

    JSObject *closure = CloneFunctionObject(cx, fun, scopeChain);
    if (!closure)
        return closure;

    uint32 nslots = fun->script()->bindings.countUpvars();
    if (nslots == 0)
        return closure;

    Value *upvars = (Value *) cx->malloc_(nslots * sizeof(Value));
    if (!upvars)
        return NULL;

    closure->setFlatClosureUpvars(upvars);
    return closure;
}

JS_DEFINE_CALLINFO_3(extern, OBJECT, js_AllocFlatClosure,
                     CONTEXT, FUNCTION, OBJECT, 0, nanojit::ACCSET_STORE_ANY)

JSObject *
js_NewFlatClosure(JSContext *cx, JSFunction *fun, JSOp op, size_t oplen)
{
    /*
     * Flat closures cannot yet be partial, that is, all upvars must be copied,
     * or the closure won't be flattened. Therefore they do not need to search
     * enclosing scope objects via JSOP_NAME, etc.
     *
     * FIXME: bug 545759 proposes to enable partial flat closures. Fixing this
     * bug requires a GetScopeChainFast call here, along with JS_REQUIRES_STACK
     * annotations on this function's prototype and definition.
     */
    VOUCH_DOES_NOT_REQUIRE_STACK();
    JSObject *scopeChain = &cx->fp()->scopeChain();

    JSObject *closure = js_AllocFlatClosure(cx, fun, scopeChain);
    if (!closure || !fun->script()->bindings.hasUpvars())
        return closure;

    Value *upvars = closure->getFlatClosureUpvars();
    uintN level = fun->script()->staticLevel;
    JSUpvarArray *uva = fun->script()->upvars();

    for (uint32 i = 0, n = uva->length; i < n; i++)
        upvars[i] = GetUpvar(cx, level, uva->vector[i]);

    return closure;
}

JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, jsid id, Native native,
                  uintN nargs, uintN attrs)
{
    PropertyOp gop;
    StrictPropertyOp sop;
    JSFunction *fun;

    if (attrs & JSFUN_STUB_GSOPS) {
        /*
         * JSFUN_STUB_GSOPS is a request flag only, not stored in fun->flags or
         * the defined property's attributes. This allows us to encode another,
         * internal flag using the same bit, JSFUN_EXPR_CLOSURE -- see jsfun.h
         * for more on this.
         */
        attrs &= ~JSFUN_STUB_GSOPS;
        gop = PropertyStub;
        sop = StrictPropertyStub;
    } else {
        gop = NULL;
        sop = NULL;
    }

    /*
     * Historically, all objects have had a parent member as intrinsic scope
     * chain link. We want to move away from this universal parent, but JS
     * requires that function objects have something like parent (ES3 and ES5
     * call it the [[Scope]] internal property), to bake a particular static
     * scope environment into each function object.
     *
     * All function objects thus have parent, including all native functions.
     * All native functions defined by the JS_DefineFunction* APIs are created
     * via the call below to js_NewFunction, which passes obj as the parent
     * parameter, and so binds fun's parent to obj using JSObject::setParent,
     * under js_NewFunction (in JSObject::init, called from NewObject -- see
     * jsobjinlines.h).
     *
     * But JSObject::setParent sets the DELEGATE object flag on its receiver,
     * to mark the object as a proto or parent of another object. Such objects
     * may intervene in property lookups and scope chain searches, so require
     * special handling when caching lookup and search results (since such
     * intervening objects can in general grow shadowing properties later).
     *
     * Thus using setParent prematurely flags certain objects, notably class
     * prototypes, so that defining native methods on them, where the method's
     * name (e.g., toString) is already bound on Object.prototype, triggers
     * shadowingShapeChange events and gratuitous shape regeneration.
     *
     * To fix this longstanding bug, we set check whether obj is already a
     * delegate, and if not, then if js_NewFunction flagged obj as a delegate,
     * we clear the flag.
     *
     * We thus rely on the fact that native functions (including indirect eval)
     * do not use the property cache or equivalent JIT techniques that require
     * this bit to be set on their parent-linked scope chain objects.
     *
     * Note: we keep API compatibility by setting parent to obj for all native
     * function objects, even if obj->getGlobal() would suffice. This should be
     * revisited when parent is narrowed to exist only for function objects and
     * possibly a few prehistoric scope objects (e.g. event targets).
     *
     * FIXME: bug 611190.
     */
    bool wasDelegate = obj->isDelegate();

    fun = js_NewFunction(cx, NULL, native, nargs,
                         attrs & (JSFUN_FLAGS_MASK | JSFUN_TRCINFO),
                         obj,
                         JSID_IS_ATOM(id) ? JSID_TO_ATOM(id) : NULL);
    if (!fun)
        return NULL;

    if (!wasDelegate && obj->isDelegate())
        obj->clearDelegate();

    if (!obj->defineProperty(cx, id, ObjectValue(*fun), gop, sop, attrs & ~JSFUN_FLAGS_MASK))
        return NULL;
    return fun;
}

JS_STATIC_ASSERT((JSV2F_CONSTRUCT & JSV2F_SEARCH_STACK) == 0);

JSFunction *
js_ValueToFunction(JSContext *cx, const Value *vp, uintN flags)
{
    JSObject *funobj;
    if (!IsFunctionObject(*vp, &funobj)) {
        js_ReportIsNotFunction(cx, vp, flags);
        return NULL;
    }
    return funobj->getFunctionPrivate();
}

JSObject *
js_ValueToFunctionObject(JSContext *cx, Value *vp, uintN flags)
{
    JSObject *funobj;
    if (!IsFunctionObject(*vp, &funobj)) {
        js_ReportIsNotFunction(cx, vp, flags);
        return NULL;
    }

    return funobj;
}

JSObject *
js_ValueToCallableObject(JSContext *cx, Value *vp, uintN flags)
{
    if (vp->isObject()) {
        JSObject *callable = &vp->toObject();
        if (callable->isCallable())
            return callable;
    }

    js_ReportIsNotFunction(cx, vp, flags);
    return NULL;
}

void
js_ReportIsNotFunction(JSContext *cx, const Value *vp, uintN flags)
{
    const char *name = NULL, *source = NULL;
    AutoValueRooter tvr(cx);
    uintN error = (flags & JSV2F_CONSTRUCT) ? JSMSG_NOT_CONSTRUCTOR : JSMSG_NOT_FUNCTION;
    LeaveTrace(cx);

    /*
     * We try to the print the code that produced vp if vp is a value in the
     * most recent interpreted stack frame. Note that additional values, not
     * directly produced by the script, may have been pushed onto the frame's
     * expression stack (e.g. by pushInvokeArgs) thereby incrementing sp past
     * the depth simulated by ReconstructPCStack.
     *
     * Conversely, values may have been popped from the stack in preparation
     * for a call (e.g., by SplatApplyArgs). Since we must pass an offset from
     * the top of the simulated stack to js_ReportValueError3, we do bounds
     * checking using the minimum of both the simulated and actual stack depth.
     */
    ptrdiff_t spindex = 0;

    FrameRegsIter i(cx);
    if (!i.done()) {
        uintN depth = js_ReconstructStackDepth(cx, i.fp()->script(), i.pc());
        Value *simsp = i.fp()->base() + depth;
        if (i.fp()->base() <= vp && vp < Min(simsp, i.sp()))
            spindex = vp - simsp;
    }

    if (!spindex)
        spindex = ((flags & JSV2F_SEARCH_STACK) ? JSDVG_SEARCH_STACK : JSDVG_IGNORE_STACK);

    js_ReportValueError3(cx, error, spindex, *vp, NULL, name, source);
}

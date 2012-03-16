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
 * The Original Code is SpiderMonkey arguments object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

struct PutArg
{
    PutArg(JSCompartment *comp, ArgumentsObject &argsobj)
      : compartment(comp), argsobj(argsobj), dst(argsobj.data()->slots) {}
    JSCompartment *compartment;
    ArgumentsObject &argsobj;
    HeapValue *dst;
    bool operator()(unsigned i, Value *src) {
        JS_ASSERT(dst->isUndefined());
        if (!argsobj.isElementDeleted(i))
            dst->set(compartment, *src);
        ++dst;
        return true;
    }
};

void
js_PutArgsObject(StackFrame *fp)
{
    ArgumentsObject &argsobj = fp->argsObj();
    if (argsobj.isNormalArguments()) {
        JS_ASSERT(argsobj.maybeStackFrame() == fp);
        JSCompartment *comp = fp->scopeChain().compartment();
        fp->forEachCanonicalActualArg(PutArg(comp, argsobj));
        argsobj.setStackFrame(NULL);
    } else {
        JS_ASSERT(!argsobj.maybeStackFrame());
    }
}

ArgumentsObject *
ArgumentsObject::create(JSContext *cx, uint32_t argc, JSObject &callee)
{
    JS_ASSERT(argc <= StackSpace::ARGS_LENGTH_MAX);

    JSObject *proto = callee.global().getOrCreateObjectPrototype(cx);
    if (!proto)
        return NULL;

    RootedVarTypeObject type(cx);

    type = proto->getNewType(cx);
    if (!type)
        return NULL;

    bool strict = callee.toFunction()->inStrictMode();
    Class *clasp = strict ? &StrictArgumentsObjectClass : &NormalArgumentsObjectClass;

    RootedVarShape emptyArgumentsShape(cx);
    emptyArgumentsShape =
        EmptyShape::getInitialShape(cx, clasp, proto,
                                    proto->getParent(), FINALIZE_KIND,
                                    BaseShape::INDEXED);
    if (!emptyArgumentsShape)
        return NULL;

    unsigned numDeletedWords = NumWordsForBitArrayOfLength(argc);
    unsigned numBytes = offsetof(ArgumentsData, slots) +
                        numDeletedWords * sizeof(size_t) +
                        argc * sizeof(Value);

    ArgumentsData *data = (ArgumentsData *)cx->malloc_(numBytes);
    if (!data)
        return NULL;

    data->callee.init(ObjectValue(callee));
    for (HeapValue *vp = data->slots; vp != data->slots + argc; vp++)
        vp->init(UndefinedValue());
    data->deletedBits = (size_t *)(data->slots + argc);
    ClearAllBitArrayElements(data->deletedBits, numDeletedWords);

    /* We have everything needed to fill in the object, so make the object. */
    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyArgumentsShape, type, NULL);
    if (!obj)
        return NULL;

    ArgumentsObject &argsobj = obj->asArguments();

    JS_ASSERT(UINT32_MAX > (uint64_t(argc) << PACKED_BITS_COUNT));
    argsobj.initInitialLength(argc);
    argsobj.initData(data);
    argsobj.setStackFrame(NULL);

    JS_ASSERT(argsobj.numFixedSlots() >= NormalArgumentsObject::RESERVED_SLOTS);
    JS_ASSERT(argsobj.numFixedSlots() >= StrictArgumentsObject::RESERVED_SLOTS);

    return &argsobj;
}

bool
ArgumentsObject::create(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(fp->script()->needsArgsObj());
    JS_ASSERT(!fp->hasCallObj());

    ArgumentsObject *argsobj = ArgumentsObject::create(cx, fp->numActualArgs(), fp->callee());
    if (!argsobj)
        return NULL;

    /*
     * Strict mode functions have arguments objects that copy the initial
     * actual parameter values. Non-strict mode arguments use the frame pointer
     * to retrieve up-to-date parameter values.
     */
    if (argsobj->isStrictArguments())
        fp->forEachCanonicalActualArg(PutArg(cx->compartment, *argsobj));
    else
        argsobj->setStackFrame(fp);

    fp->initArgsObj(*argsobj);
    return argsobj;
}

ArgumentsObject *
ArgumentsObject::createUnexpected(JSContext *cx, StackFrame *fp)
{
    ArgumentsObject *argsobj = create(cx, fp->numActualArgs(), fp->callee());
    if (!argsobj)
        return NULL;

    fp->forEachCanonicalActualArg(PutArg(cx->compartment, *argsobj));
    return argsobj;
}

static JSBool
args_delProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    ArgumentsObject &argsobj = obj->asArguments();
    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg)) {
            argsobj.setElement(arg, UndefinedValue());
            argsobj.markElementDeleted(arg);
        }
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        argsobj.markLengthOverridden();
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom)) {
        argsobj.asNormalArguments().clearCallee();
    }
    return true;
}

static JSBool
ArgGetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
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
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg)) {
            if (StackFrame *fp = argsobj.maybeStackFrame()) {
                JS_ASSERT_IF(arg < fp->numFormalArgs(), fp->script()->argIsAliased(arg));
                *vp = fp->canonicalActualArg(arg);
            } else {
                *vp = argsobj.element(arg);
            }
        }
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (!argsobj.hasOverriddenLength())
            vp->setInt32(argsobj.initialLength());
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom));
        const Value &v = argsobj.callee();
        if (!v.isMagic(JS_OVERWRITTEN_CALLEE))
            *vp = v;
    }
    return true;
}

static JSBool
ArgSetter(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    if (!obj->isNormalArguments())
        return true;

    NormalArgumentsObject &argsobj = obj->asNormalArguments();

    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength()) {
            if (StackFrame *fp = argsobj.maybeStackFrame()) {
                JSScript *script = fp->functionScript();
                JS_ASSERT(script->needsArgsObj());
                if (arg < fp->numFormalArgs()) {
                    JS_ASSERT(fp->script()->argIsAliased(arg));
                    types::TypeScript::SetArgument(cx, script, arg, *vp);
                }
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
    return js_DeleteGeneric(cx, &argsobj, id, tvr.addr(), false) &&
           js_DefineProperty(cx, &argsobj, id, vp, NULL, NULL, JSPROP_ENUMERATE);
}

static JSBool
args_resolve(JSContext *cx, JSObject *obj, jsid id, unsigned flags,
             JSObject **objp)
{
    *objp = NULL;

    NormalArgumentsObject &argsobj = obj->asNormalArguments();

    unsigned attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    if (JSID_IS_INT(id)) {
        uint32_t arg = uint32_t(JSID_TO_INT(id));
        if (arg >= argsobj.initialLength() || argsobj.isElementDeleted(arg))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (argsobj.hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom))
            return true;

        if (argsobj.callee().isMagic(JS_OVERWRITTEN_CALLEE))
            return true;
    }

    Value undef = UndefinedValue();
    if (!js_DefineProperty(cx, &argsobj, id, &undef, ArgGetter, ArgSetter, attrs))
        return JS_FALSE;

    *objp = &argsobj;
    return true;
}

bool
NormalArgumentsObject::optimizedGetElem(JSContext *cx, StackFrame *fp, const Value &elem, Value *vp)
{
    JS_ASSERT(!fp->hasArgsObj());

    /* Fast path: no need to convert to id when elem is already an int in range. */
    if (elem.isInt32()) {
        int32_t i = elem.toInt32();
        if (i >= 0 && uint32_t(i) < fp->numActualArgs()) {
            *vp = fp->canonicalActualArg(i);
            return true;
        }
    }

    /* Slow path: create and canonicalize an id, then emulate args_resolve. */

    jsid id;
    if (!ValueToId(cx, elem, &id))
        return false;
    id = js_CheckForStringIndex(id);

    if (JSID_IS_INT(id)) {
        int32_t i = JSID_TO_INT(id);
        if (i >= 0 && uint32_t(i) < fp->numActualArgs()) {
            *vp = fp->canonicalActualArg(i);
            return true;
        }
    }

    if (id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)) {
        *vp = Int32Value(fp->numActualArgs());
        return true;
    }

    if (id == ATOM_TO_JSID(cx->runtime->atomState.calleeAtom)) {
        *vp = ObjectValue(fp->callee());
        return true;
    }

    JSObject *proto = fp->scopeChain().global().getOrCreateObjectPrototype(cx);
    if (!proto)
        return false;

    return proto->getGeneric(cx, id, vp);
}

static JSBool
args_enumerate(JSContext *cx, JSObject *obj)
{
    NormalArgumentsObject &argsobj = obj->asNormalArguments();

    /*
     * Trigger reflection in args_resolve using a series of js_LookupProperty
     * calls.
     */
    int argc = int(argsobj.initialLength());
    for (int i = -2; i != argc; i++) {
        jsid id = (i == -2)
                  ? ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)
                  : (i == -1)
                  ? ATOM_TO_JSID(cx->runtime->atomState.calleeAtom)
                  : INT_TO_JSID(i);

        JSObject *pobj;
        JSProperty *prop;
        if (!js_LookupProperty(cx, &argsobj, id, &pobj, &prop))
            return false;
    }
    return true;
}

static JSBool
StrictArgGetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
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
StrictArgSetter(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    if (!obj->isStrictArguments())
        return true;

    StrictArgumentsObject &argsobj = obj->asStrictArguments();

    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength()) {
            argsobj.setElement(arg, *vp);
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
    return js_DeleteGeneric(cx, &argsobj, id, tvr.addr(), strict) &&
           js_SetPropertyHelper(cx, &argsobj, id, 0, vp, strict);
}

static JSBool
strictargs_resolve(JSContext *cx, JSObject *obj, jsid id, unsigned flags, JSObject **objp)
{
    *objp = NULL;

    StrictArgumentsObject &argsobj = obj->asStrictArguments();

    unsigned attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    PropertyOp getter = StrictArgGetter;
    StrictPropertyOp setter = StrictArgSetter;

    if (JSID_IS_INT(id)) {
        uint32_t arg = uint32_t(JSID_TO_INT(id));
        if (arg >= argsobj.initialLength() || argsobj.isElementDeleted(arg))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (argsobj.hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom) &&
            !JSID_IS_ATOM(id, cx->runtime->atomState.callerAtom)) {
            return true;
        }

        attrs = JSPROP_PERMANENT | JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED;
        getter = CastAsPropertyOp(argsobj.global().getThrowTypeError());
        setter = CastAsStrictPropertyOp(argsobj.global().getThrowTypeError());
    }

    Value undef = UndefinedValue();
    if (!js_DefineProperty(cx, &argsobj, id, &undef, getter, setter, attrs))
        return false;

    *objp = &argsobj;
    return true;
}

static JSBool
strictargs_enumerate(JSContext *cx, JSObject *obj)
{
    StrictArgumentsObject *argsobj = &obj->asStrictArguments();

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

    for (uint32_t i = 0, argc = argsobj->initialLength(); i < argc; i++) {
        if (!js_LookupProperty(cx, argsobj, INT_TO_JSID(i), &pobj, &prop))
            return false;
    }

    return true;
}

static void
args_finalize(FreeOp *fop, JSObject *obj)
{
    fop->free_(reinterpret_cast<void *>(obj->asArguments().data()));
}

static void
args_trace(JSTracer *trc, JSObject *obj)
{
    ArgumentsObject &argsobj = obj->asArguments();
    ArgumentsData *data = argsobj.data();
    MarkValue(trc, &data->callee, js_callee_str);
    MarkValueRange(trc, argsobj.initialLength(), data->slots, js_arguments_str);

    /*
     * If a generator's arguments or call object escapes, and the generator
     * frame is not executing, the generator object needs to be marked because
     * it is not otherwise reachable. An executing generator is rooted by its
     * invocation.  To distinguish the two cases (which imply different access
     * paths to the generator object), we use the JSFRAME_FLOATING_GENERATOR
     * flag, which is only set on the StackFrame kept in the generator object's
     * JSGenerator.
     */
#if JS_HAS_GENERATORS
    StackFrame *fp = argsobj.maybeStackFrame();
    if (fp && fp->isFloatingGenerator())
        MarkObject(trc, &js_FloatingFrameToGenerator(fp)->obj, "generator object");
#endif
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
    args_finalize,           /* finalize   */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    args_trace,
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
    args_finalize,           /* finalize   */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    args_trace,
    {
        NULL,       /* equality    */
        NULL,       /* outerObject */
        NULL,       /* innerObject */
        JS_ElementIteratorStub,
        NULL,       /* unused      */
        false,      /* isWrappedNative */
    }
};

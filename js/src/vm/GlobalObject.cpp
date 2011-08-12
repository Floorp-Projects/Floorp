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
 * The Original Code is SpiderMonkey global object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

#include "GlobalObject.h"

#include "jscntxt.h"
#include "jsexn.h"
#include "jsmath.h"
#include "json.h"

#include "jsobjinlines.h"
#include "jsregexpinlines.h"

using namespace js;

JSObject *
js_InitFunctionAndObjectClasses(JSContext *cx, JSObject *obj)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);

    /* If cx has no global object, use obj so prototypes can be found. */
    if (!cx->globalObject)
        JS_SetGlobalObject(cx, obj);

    /* Record Function and Object in cx->resolvingList. */
    JSAtom **classAtoms = cx->runtime->atomState.classAtoms;
    AutoResolving resolving1(cx, obj, ATOM_TO_JSID(classAtoms[JSProto_Function]));
    AutoResolving resolving2(cx, obj, ATOM_TO_JSID(classAtoms[JSProto_Object]));

    /* Initialize the function class first so constructors can be made. */
    JSObject *fun_proto;
    if (!js_GetClassPrototype(cx, obj, JSProto_Function, &fun_proto))
        return NULL;
    if (!fun_proto) {
        fun_proto = js_InitFunctionClass(cx, obj);
        if (!fun_proto)
            return NULL;
    } else {
        JSObject *ctor = JS_GetConstructor(cx, fun_proto);
        if (!ctor)
            return NULL;
        if (!obj->defineProperty(cx, ATOM_TO_JSID(CLASS_ATOM(cx, Function)),
                                 ObjectValue(*ctor), 0, 0, 0)) {
            return NULL;
        }
    }

    /* Initialize the object class next so Object.prototype works. */
    JSObject *obj_proto;
    if (!js_GetClassPrototype(cx, obj, JSProto_Object, &obj_proto))
        return NULL;
    if (!obj_proto)
        obj_proto = js_InitObjectClass(cx, obj);
    if (!obj_proto)
        return NULL;

    /*
     * Function.prototype and the global object delegate to Object.prototype.
     * Don't update the prototype if the __proto__ of either object was cleared
     * after the objects started getting used.
     */
    if (fun_proto->shouldSplicePrototype(cx) && !fun_proto->splicePrototype(cx, obj_proto))
        return NULL;
    if (obj->shouldSplicePrototype(cx) && !obj->splicePrototype(cx, obj_proto))
        return NULL;

    return fun_proto;
}

namespace js {

GlobalObject *
GlobalObject::create(JSContext *cx, Class *clasp)
{
    JS_ASSERT(clasp->flags & JSCLASS_IS_GLOBAL);

    JSObject *obj = NewNonFunction<WithProto::Given>(cx, clasp, NULL, NULL);
    if (!obj || !obj->setSingletonType(cx))
        return NULL;

    GlobalObject *globalObj = obj->asGlobal();
    globalObj->makeVarObj();
    globalObj->syncSpecialEquality();

    /* Construct a regexp statics object for this global object. */
    JSObject *res = regexp_statics_construct(cx, globalObj);
    if (!res)
        return NULL;
    globalObj->setSlot(REGEXP_STATICS, ObjectValue(*res));
    globalObj->setFlags(0);

    return globalObj;
}

bool
GlobalObject::initStandardClasses(JSContext *cx)
{
    /* Native objects get their reserved slots from birth. */
    JS_ASSERT(numSlots() >= JSSLOT_FREE(getClass()));

    JSAtomState &state = cx->runtime->atomState;

    /* Define a top-level property 'undefined' with the undefined value. */
    if (!defineProperty(cx, ATOM_TO_JSID(state.typeAtoms[JSTYPE_VOID]), UndefinedValue(),
                        PropertyStub, StrictPropertyStub, JSPROP_PERMANENT | JSPROP_READONLY))
    {
        return false;
    }

    if (!js_InitFunctionAndObjectClasses(cx, this))
        return false;

    /* Initialize the rest of the standard objects and functions. */
    return js_InitArrayClass(cx, this) &&
           js_InitBooleanClass(cx, this) &&
           js_InitExceptionClasses(cx, this) &&
           js_InitMathClass(cx, this) &&
           js_InitNumberClass(cx, this) &&
           js_InitJSONClass(cx, this) &&
           js_InitRegExpClass(cx, this) &&
           js_InitStringClass(cx, this) &&
           js_InitTypedArrayClasses(cx, this) &&
#if JS_HAS_XML_SUPPORT
           js_InitXMLClasses(cx, this) &&
#endif
#if JS_HAS_GENERATORS
           js_InitIteratorClasses(cx, this) &&
#endif
           js_InitDateClass(cx, this) &&
           js_InitProxyClass(cx, this);
}

void
GlobalObject::clear(JSContext *cx)
{
    /* This can return false but that doesn't mean it failed. */
    unbrand(cx);

    for (int key = JSProto_Null; key < JSProto_LIMIT * 3; key++)
        setSlot(key, UndefinedValue());

    /* Clear regexp statics. */
    RegExpStatics::extractFrom(this)->clear();

    /* Clear the runtime-codegen-enabled cache. */
    setSlot(RUNTIME_CODEGEN_ENABLED, UndefinedValue());

    /*
     * Mark global as cleared. If we try to execute any compile-and-go
     * scripts from here on, we will throw.
     */
    int32 flags = getSlot(FLAGS).toInt32();
    flags |= FLAGS_CLEARED;
    setSlot(FLAGS, Int32Value(flags));
}

bool
GlobalObject::isRuntimeCodeGenEnabled(JSContext *cx)
{
    Value &v = getSlotRef(RUNTIME_CODEGEN_ENABLED);
    if (v.isUndefined()) {
        JSSecurityCallbacks *callbacks = JS_GetSecurityCallbacks(cx);

        /*
         * If there are callbacks, make sure that the CSP callback is installed
         * and that it permits runtime code generation, then cache the result.
         */
        v = BooleanValue((!callbacks || !callbacks->contentSecurityPolicyAllows) ||
                         callbacks->contentSecurityPolicyAllows(cx));
    }
    return !v.isFalse();
}

JSFunction *
GlobalObject::createConstructor(JSContext *cx, Native ctor, Class *clasp, JSAtom *name,
                                uintN length)
{
    JSFunction *fun = js_NewFunction(cx, NULL, ctor, length, JSFUN_CONSTRUCTOR, this, name);
    if (!fun)
        return NULL;

    /*
     * Remember the class this function is a constructor for so that we know to
     * create an object of this class when we call the constructor.
     */
    FUN_CLASP(fun) = clasp;
    return fun;
}

JSObject *
GlobalObject::createBlankPrototype(JSContext *cx, Class *clasp)
{
    JS_ASSERT(clasp != &js_ObjectClass);
    JS_ASSERT(clasp != &js_FunctionClass);

    JSObject *objectProto;
    if (!js_GetClassPrototype(cx, this, JSProto_Object, &objectProto))
        return NULL;

    JSObject *proto = NewNonFunction<WithProto::Given>(cx, clasp, objectProto, this);
    if (!proto || !proto->setSingletonType(cx))
        return NULL;

    /*
     * Supply the created prototype object with an empty shape for the benefit
     * of callers of JSObject::initSharingEmptyShape.
     */
    types::TypeObject *type = proto->getNewType(cx);
    if (!type || !type->getEmptyShape(cx, clasp, gc::FINALIZE_OBJECT0))
        return NULL;

    return proto;
}

bool
LinkConstructorAndPrototype(JSContext *cx, JSObject *ctor, JSObject *proto)
{
    return ctor->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                                ObjectValue(*proto), PropertyStub, StrictPropertyStub,
                                JSPROP_PERMANENT | JSPROP_READONLY) &&
           proto->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.constructorAtom),
                                 ObjectValue(*ctor), PropertyStub, StrictPropertyStub, 0);
}

bool
DefinePropertiesAndBrand(JSContext *cx, JSObject *obj, JSPropertySpec *ps, JSFunctionSpec *fs)
{
    if ((ps && !JS_DefineProperties(cx, obj, ps)) || (fs && !JS_DefineFunctions(cx, obj, fs)))
        return false;
    if (!cx->typeInferenceEnabled())
        obj->brand(cx);
    return true;
}

} // namespace js

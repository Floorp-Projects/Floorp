/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Andreas Gal <gal@mozilla.com>
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org>
 *   Mike Shaver <shaver@mozilla.org>
 *   David Anderson <danderson@mozilla.com>
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

#include <math.h>

#include "jsapi.h"
#include "jsstdint.h"
#include "jsarray.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jslibmath.h"
#include "jsmath.h"
#include "jsnum.h"
#include "prmjtime.h"
#include "jsdate.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jsbuiltins.h"
#include "jstracer.h"
#include "jsvector.h"

#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsnuminlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"

using namespace avmplus;
using namespace nanojit;
using namespace js;

JS_FRIEND_API(void)
js_SetTraceableNativeFailed(JSContext *cx)
{
    /*
     * We might not be on trace (we might have deep bailed) so we hope
     * cx->compartment is correct.
     */
    SetBuiltinError(JS_TRACE_MONITOR_FROM_CONTEXT(cx));
}

/*
 * NB: bool FASTCALL is not compatible with Nanojit's calling convention usage.
 * Do not use bool FASTCALL, use JSBool only!
 */

jsdouble FASTCALL
js_dmod(jsdouble a, jsdouble b)
{
    if (b == 0.0) {
        jsdpun u;
        u.s.hi = JSDOUBLE_HI32_NAN;
        u.s.lo = JSDOUBLE_LO32_NAN;
        return u.d;
    }
    return js_fmod(a, b);
}
JS_DEFINE_CALLINFO_2(extern, DOUBLE, js_dmod, DOUBLE, DOUBLE, 1, ACCSET_NONE)

int32 FASTCALL
js_imod(int32 a, int32 b)
{
    if (a < 0 || b <= 0)
        return -1;
    int r = a % b;
    return r;
}
JS_DEFINE_CALLINFO_2(extern, INT32, js_imod, INT32, INT32, 1, ACCSET_NONE)

#if JS_BITS_PER_WORD == 32

jsdouble FASTCALL
js_UnboxNumberAsDouble(uint32 tag, uint32 payload)
{
    if (tag == JSVAL_TAG_INT32)
        return (double)(int32)payload;

    JS_ASSERT(tag <= JSVAL_TAG_CLEAR);
    jsval_layout l;
    l.s.tag = (JSValueTag)tag;
    l.s.payload.u32 = payload;
    return l.asDouble;
}
JS_DEFINE_CALLINFO_2(extern, DOUBLE, js_UnboxNumberAsDouble, UINT32, UINT32, 1, ACCSET_NONE)

int32 FASTCALL
js_UnboxNumberAsInt32(uint32 tag, uint32 payload)
{
    if (tag == JSVAL_TAG_INT32)
        return (int32)payload;

    JS_ASSERT(tag <= JSVAL_TAG_CLEAR);
    jsval_layout l;
    l.s.tag = (JSValueTag)tag;
    l.s.payload.u32 = payload;
    return js_DoubleToECMAInt32(l.asDouble);
}
JS_DEFINE_CALLINFO_2(extern, INT32, js_UnboxNumberAsInt32, UINT32, UINT32, 1, ACCSET_NONE)

#elif JS_BITS_PER_WORD == 64

jsdouble FASTCALL
js_UnboxNumberAsDouble(Value v)
{
    if (v.isInt32())
        return (jsdouble)v.toInt32();
    JS_ASSERT(v.isDouble());
    return v.toDouble();
}
JS_DEFINE_CALLINFO_1(extern, DOUBLE, js_UnboxNumberAsDouble, JSVAL, 1, ACCSET_NONE)

int32 FASTCALL
js_UnboxNumberAsInt32(Value v)
{
    if (v.isInt32())
        return v.toInt32();
    JS_ASSERT(v.isDouble());
    return js_DoubleToECMAInt32(v.toDouble());
}
JS_DEFINE_CALLINFO_1(extern, INT32, js_UnboxNumberAsInt32, VALUE, 1, ACCSET_NONE)

#endif

int32 FASTCALL
js_DoubleToInt32(jsdouble d)
{
    return js_DoubleToECMAInt32(d);
}
JS_DEFINE_CALLINFO_1(extern, INT32, js_DoubleToInt32, DOUBLE, 1, ACCSET_NONE)

uint32 FASTCALL
js_DoubleToUint32(jsdouble d)
{
    return js_DoubleToECMAUint32(d);
}
JS_DEFINE_CALLINFO_1(extern, UINT32, js_DoubleToUint32, DOUBLE, 1, ACCSET_NONE)

/*
 * js_StringToNumber and js_StringToInt32 store into their second argument, so
 * they need to be annotated accordingly.  To be future-proof, we use
 * ACCSET_STORE_ANY so that new callers don't have to remember to update the
 * annotation.
 */
jsdouble FASTCALL
js_StringToNumber(JSContext* cx, JSString* str, JSBool *ok)
{
    double out = 0;  /* silence warnings. */
    *ok = StringToNumberType<jsdouble>(cx, str, &out);
    return out;
}
JS_DEFINE_CALLINFO_3(extern, DOUBLE, js_StringToNumber, CONTEXT, STRING, BOOLPTR,
                     0, ACCSET_STORE_ANY)

int32 FASTCALL
js_StringToInt32(JSContext* cx, JSString* str, JSBool *ok)
{
    int32 out = 0;  /* silence warnings. */
    *ok = StringToNumberType<int32>(cx, str, &out);
    return out;
}
JS_DEFINE_CALLINFO_3(extern, INT32, js_StringToInt32, CONTEXT, STRING, BOOLPTR,
                     0, ACCSET_STORE_ANY)

/* Nb: it's always safe to set isDefinitelyAtom to false if you're unsure or don't know. */
static inline JSBool
AddPropertyHelper(JSContext* cx, JSObject* obj, Shape* shape, bool isDefinitelyAtom)
{
    JS_ASSERT(shape->previous() == obj->lastProperty());

    if (obj->nativeEmpty()) {
        if (!obj->ensureClassReservedSlotsForEmptyObject(cx))
            return false;
    }

    uint32 slot;
    slot = shape->slot;
    JS_ASSERT(slot == obj->slotSpan());

    if (slot < obj->numSlots()) {
        JS_ASSERT(obj->getSlot(slot).isUndefined());
    } else {
        if (!obj->allocSlot(cx, &slot))
            return false;
        JS_ASSERT(slot == shape->slot);
    }

    obj->extend(cx, shape, isDefinitelyAtom);
    return !js_IsPropertyCacheDisabled(cx);
}

JSBool FASTCALL
js_AddProperty(JSContext* cx, JSObject* obj, Shape* shape)
{
    return AddPropertyHelper(cx, obj, shape, /* isDefinitelyAtom = */false);
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_AddProperty, CONTEXT, OBJECT, SHAPE, 0, ACCSET_STORE_ANY)

JSBool FASTCALL
js_AddAtomProperty(JSContext* cx, JSObject* obj, Shape* shape)
{
    return AddPropertyHelper(cx, obj, shape, /* isDefinitelyAtom = */true);
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_AddAtomProperty, CONTEXT, OBJECT, SHAPE, 0, ACCSET_STORE_ANY)

static JSBool
HasProperty(JSContext* cx, JSObject* obj, jsid id)
{
    // Check that we know how the lookup op will behave.
    for (JSObject* pobj = obj; pobj; pobj = pobj->getProto()) {
        if (pobj->getOps()->lookupProperty)
            return JS_NEITHER;
        Class* clasp = pobj->getClass();
        if (clasp->resolve != JS_ResolveStub && clasp != &StringClass)
            return JS_NEITHER;
    }

    JSObject* obj2;
    JSProperty* prop;
    if (!LookupPropertyWithFlags(cx, obj, id, JSRESOLVE_QUALIFIED, &obj2, &prop))
        return JS_NEITHER;
    return prop != NULL;
}

JSBool FASTCALL
js_HasNamedProperty(JSContext* cx, JSObject* obj, JSString* idstr)
{
    JSAtom *atom = js_AtomizeString(cx, idstr);
    if (!atom)
        return JS_NEITHER;

    return HasProperty(cx, obj, ATOM_TO_JSID(atom));
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_HasNamedProperty, CONTEXT, OBJECT, STRING,
                     0, ACCSET_STORE_ANY)

JSBool FASTCALL
js_HasNamedPropertyInt32(JSContext* cx, JSObject* obj, int32 index)
{
    jsid id;
    if (!js_Int32ToId(cx, index, &id))
        return JS_NEITHER;

    return HasProperty(cx, obj, id);
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_HasNamedPropertyInt32, CONTEXT, OBJECT, INT32,
                     0, ACCSET_STORE_ANY)

JSString* FASTCALL
js_TypeOfObject(JSContext* cx, JSObject* obj)
{
    JS_ASSERT(obj);
    return cx->runtime->atomState.typeAtoms[obj->typeOf(cx)];
}
JS_DEFINE_CALLINFO_2(extern, STRING, js_TypeOfObject, CONTEXT, OBJECT, 1, ACCSET_NONE)

JSString* FASTCALL
js_BooleanIntToString(JSContext *cx, int32 unboxed)
{
    JS_ASSERT(uint32(unboxed) <= 1);
    return cx->runtime->atomState.booleanAtoms[unboxed];
}
JS_DEFINE_CALLINFO_2(extern, STRING, js_BooleanIntToString, CONTEXT, INT32, 1, ACCSET_NONE)

JSObject* FASTCALL
js_NewNullClosure(JSContext* cx, JSObject* funobj, JSObject* proto, JSObject* parent)
{
    JS_ASSERT(funobj->isFunction());
    JS_ASSERT(proto->isFunction());
    JS_ASSERT(JS_ON_TRACE(cx));

    JSFunction *fun = (JSFunction*) funobj;
    JS_ASSERT(funobj->getFunctionPrivate() == fun);

    types::TypeObject *type = proto->getNewType(cx);
    if (!type)
        return NULL;

    JSObject* closure = js_NewGCObject(cx, gc::FINALIZE_OBJECT2);
    if (!closure)
        return NULL;

    if (!closure->initSharingEmptyShape(cx, &FunctionClass, type, parent,
                                        fun, gc::FINALIZE_OBJECT2)) {
        return NULL;
    }
    return closure;
}
JS_DEFINE_CALLINFO_4(extern, OBJECT, js_NewNullClosure, CONTEXT, OBJECT, OBJECT, OBJECT,
                     0, ACCSET_STORE_ANY)


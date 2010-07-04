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
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jscntxtinlines.h"

using namespace avmplus;
using namespace nanojit;
using namespace js;

JS_FRIEND_API(void)
js_SetTraceableNativeFailed(JSContext *cx)
{
    SetBuiltinError(cx);
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
        u.s.hi = JSDOUBLE_HI32_EXPMASK | JSDOUBLE_HI32_MANTMASK;
        u.s.lo = 0xffffffff;
        return u.d;
    }
    return js_fmod(a, b);
}
JS_DEFINE_CALLINFO_2(extern, DOUBLE, js_dmod, DOUBLE, DOUBLE, 1, ACC_NONE)

int32 FASTCALL
js_imod(int32 a, int32 b)
{
    if (a < 0 || b <= 0)
        return -1;
    int r = a % b;
    return r;
}
JS_DEFINE_CALLINFO_2(extern, INT32, js_imod, INT32, INT32, 1, ACC_NONE)

#if JS_BITS_PER_WORD == 32

jsdouble FASTCALL
js_UnboxDouble(uint32 tag, uint32 payload)
{
    if (tag == JSVAL_TAG_INT32)
        return (double)(int32)payload;

    jsval_layout l;
    l.s.tag = (JSValueTag)tag;
    l.s.payload.u32 = payload;
    return l.asDouble;
}
JS_DEFINE_CALLINFO_2(extern, DOUBLE, js_UnboxDouble, UINT32, UINT32, 1, ACC_NONE)

int32 FASTCALL
js_UnboxInt32(uint32 tag, uint32 payload)
{
    if (tag == JSVAL_TAG_INT32)
        return (int32)payload;

    jsval_layout l;
    l.s.tag = (JSValueTag)tag;
    l.s.payload.u32 = payload;
    return js_DoubleToECMAInt32(l.asDouble);
}
JS_DEFINE_CALLINFO_2(extern, INT32, js_UnboxInt32, UINT32, UINT32, 1, ACC_NONE)

#elif JS_BITS_PER_WORD == 64

jsdouble FASTCALL
js_UnboxDouble(Value v)
{
    if (v.isInt32())
        return (jsdouble)v.asInt32();
    return v.asDouble();
}
JS_DEFINE_CALLINFO_1(extern, DOUBLE, js_UnboxDouble, JSVAL, 1, ACC_NONE)

int32 FASTCALL
js_UnboxInt32(Value v)
{
    if (v.isInt32())
        return v.asInt32();
    return js_DoubleToECMAInt32(v.asDouble());
}
JS_DEFINE_CALLINFO_1(extern, INT32, js_UnboxInt32, VALUE, 1, ACC_NONE)

#endif

int32 FASTCALL
js_DoubleToInt32(jsdouble d)
{
    return js_DoubleToECMAInt32(d);
}
JS_DEFINE_CALLINFO_1(extern, INT32, js_DoubleToInt32, DOUBLE, 1, ACC_NONE)

uint32 FASTCALL
js_DoubleToUint32(jsdouble d)
{
    return js_DoubleToECMAUint32(d);
}
JS_DEFINE_CALLINFO_1(extern, UINT32, js_DoubleToUint32, DOUBLE, 1, ACC_NONE)

jsdouble FASTCALL
js_StringToNumber(JSContext* cx, JSString* str)
{
    return StringToNumberType<jsdouble>(cx, str);
}
JS_DEFINE_CALLINFO_2(extern, DOUBLE, js_StringToNumber, CONTEXT, STRING, 1, ACC_NONE)

int32 FASTCALL
js_StringToInt32(JSContext* cx, JSString* str)
{
    return StringToNumberType<int32>(cx, str);
}
JS_DEFINE_CALLINFO_2(extern, INT32, js_StringToInt32, CONTEXT, STRING, 1, ACC_NONE)

/* Nb: it's always safe to set isDefinitelyAtom to false if you're unsure or don't know. */
static inline JSBool
AddPropertyHelper(JSContext* cx, JSObject* obj, JSScopeProperty* sprop, bool isDefinitelyAtom)
{
    JS_LOCK_OBJ(cx, obj);

    uint32 slot = sprop->slot;
    JSScope* scope = obj->scope();
    if (slot != scope->freeslot)
        return false;
    JS_ASSERT(sprop->parent == scope->lastProperty());

    if (scope->isSharedEmpty()) {
        scope = js_GetMutableScope(cx, obj);
        if (!scope)
            return false;
    } else {
        JS_ASSERT(!scope->hasProperty(sprop));
    }

    if (!scope->table) {
        if (slot < obj->numSlots()) {
            JS_ASSERT(obj->getSlot(scope->freeslot).isUndefined());
            ++scope->freeslot;
        } else {
            if (!js_AllocSlot(cx, obj, &slot))
                goto exit_trace;

            if (slot != sprop->slot) {
                js_FreeSlot(cx, obj, slot);
                goto exit_trace;
            }
        }

        scope->extend(cx, sprop, isDefinitelyAtom);
    } else {
        JSScopeProperty *sprop2 =
            scope->addProperty(cx, sprop->id, sprop->getter(), sprop->setter(),
                               SPROP_INVALID_SLOT, sprop->attributes(), sprop->getFlags(),
                               sprop->shortid);
        if (sprop2 != sprop)
            goto exit_trace;
    }

    if (js_IsPropertyCacheDisabled(cx))
        goto exit_trace;

    JS_UNLOCK_SCOPE(cx, scope);
    return true;

  exit_trace:
    JS_UNLOCK_SCOPE(cx, scope);
    return false;
}

JSBool FASTCALL
js_AddProperty(JSContext* cx, JSObject* obj, JSScopeProperty* sprop)
{
    return AddPropertyHelper(cx, obj, sprop, /* isDefinitelyAtom = */false);
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_AddProperty, CONTEXT, OBJECT, SCOPEPROP, 0, ACC_STORE_ANY)

JSBool FASTCALL
js_AddAtomProperty(JSContext* cx, JSObject* obj, JSScopeProperty* sprop)
{
    return AddPropertyHelper(cx, obj, sprop, /* isDefinitelyAtom = */true);
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_AddAtomProperty, CONTEXT, OBJECT, SCOPEPROP, 0, ACC_STORE_ANY)

static JSBool
HasProperty(JSContext* cx, JSObject* obj, jsid id)
{
    // Check that we know how the lookup op will behave.
    for (JSObject* pobj = obj; pobj; pobj = pobj->getProto()) {
        if (pobj->map->ops->lookupProperty != js_LookupProperty)
            return JS_NEITHER;
        Class* clasp = pobj->getClass();
        if (clasp->resolve != JS_ResolveStub && clasp != &js_StringClass)
            return JS_NEITHER;
    }

    JSObject* obj2;
    JSProperty* prop;
    if (js_LookupPropertyWithFlags(cx, obj, id, JSRESOLVE_QUALIFIED, &obj2, &prop) < 0)
        return JS_NEITHER;
    if (prop)
        obj2->dropProperty(cx, prop);
    return prop != NULL;
}

JSBool FASTCALL
js_HasNamedProperty(JSContext* cx, JSObject* obj, JSString* idstr)
{
    JSAtom *atom = js_AtomizeString(cx, idstr, 0);
    if (!atom)
        return JS_NEITHER;

    return HasProperty(cx, obj, ATOM_TO_JSID(atom));
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_HasNamedProperty, CONTEXT, OBJECT, STRING, 0, ACC_STORE_ANY)

JSBool FASTCALL
js_HasNamedPropertyInt32(JSContext* cx, JSObject* obj, int32 index)
{
    jsid id;
    if (!js_Int32ToId(cx, index, &id))
        return JS_NEITHER;

    return HasProperty(cx, obj, id);
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_HasNamedPropertyInt32, CONTEXT, OBJECT, INT32, 0,
                     ACC_STORE_ANY)

JSString* FASTCALL
js_TypeOfObject(JSContext* cx, JSObject* obj)
{
    JS_ASSERT(obj);
    return ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[obj->typeOf(cx)]);
}
JS_DEFINE_CALLINFO_2(extern, STRING, js_TypeOfObject, CONTEXT, OBJECT, 1, ACC_NONE)

JSString* FASTCALL
js_BooleanIntToString(JSContext *cx, int32 unboxed)
{
    JS_ASSERT(uint32(unboxed) <= 1);
    return ATOM_TO_STRING(cx->runtime->atomState.booleanAtoms[unboxed]);
}
JS_DEFINE_CALLINFO_2(extern, STRING, js_BooleanIntToString, CONTEXT, INT32, 1, ACC_NONE)

JSObject* FASTCALL
js_NewNullClosure(JSContext* cx, JSObject* funobj, JSObject* proto, JSObject* parent)
{
    JS_ASSERT(funobj->isFunction());
    JS_ASSERT(proto->isFunction());
    JS_ASSERT(JS_ON_TRACE(cx));

    JSFunction *fun = (JSFunction*) funobj;
    JS_ASSERT(GET_FUNCTION_PRIVATE(cx, funobj) == fun);

    JSObject* closure = js_NewGCObject(cx);
    if (!closure)
        return NULL;

    closure->initSharingEmptyScope(&js_FunctionClass, proto, parent, PrivateTag(fun));
    return closure;
}
JS_DEFINE_CALLINFO_4(extern, OBJECT, js_NewNullClosure, CONTEXT, OBJECT, OBJECT, OBJECT, 0,
                     ACC_STORE_ANY)

JS_REQUIRES_STACK JSBool FASTCALL
js_PopInterpFrame(JSContext* cx, TracerState* state)
{
    JS_ASSERT(cx->fp && cx->fp->down);
    JSStackFrame* const fp = cx->fp;

    /*
     * Mirror frame popping code from inline_return in js_Interpret. There are
     * some things we just don't want to handle. In those cases, the trace will
     * MISMATCH_EXIT.
     */
    if (fp->hookData)
        return JS_FALSE;
    if (cx->version != fp->callerVersion)
        return JS_FALSE;
    if (fp->flags & JSFRAME_CONSTRUCTING)
        return JS_FALSE;
    if (fp->imacpc)
        return JS_FALSE;
    if (fp->blockChain)
        return JS_FALSE;

    fp->putActivationObjects(cx);
    
    /* Update display table. */
    if (fp->script->staticLevel < JS_DISPLAY_SIZE)
        cx->display[fp->script->staticLevel] = fp->displaySave;

    /* Pop the frame and its memory. */
    cx->stack().popInlineFrame(cx, fp, fp->down);

    /* Update the inline call count. */
    *state->inlineCallCountp = *state->inlineCallCountp - 1;
    return JS_TRUE;
}
JS_DEFINE_CALLINFO_2(extern, BOOL, js_PopInterpFrame, CONTEXT, TRACERSTATE, 0, ACC_STORE_ANY)

JSString* FASTCALL
js_ConcatN(JSContext *cx, JSString **strArray, uint32 size)
{
    /* Calculate total size. */
    size_t numChar = 1;
    for (uint32 i = 0; i < size; ++i) {
        size_t before = numChar;
        numChar += strArray[i]->length();
        if (numChar < before)
            return NULL;
    }


    /* Allocate buffer. */
    if (numChar & js::tl::MulOverflowMask<sizeof(jschar)>::result)
        return NULL;
    jschar *buf = (jschar *)cx->malloc(numChar * sizeof(jschar));
    if (!buf)
        return NULL;

    /* Fill buffer. */
    jschar *ptr = buf;
    for (uint32 i = 0; i < size; ++i) {
        const jschar *chars;
        size_t length;
        strArray[i]->getCharsAndLength(chars, length);
        js_strncpy(ptr, chars, length);
        ptr += length;
    }
    *ptr = '\0';

    /* Create string. */
    JSString *str = js_NewString(cx, buf, numChar - 1);
    if (!str)
        cx->free(buf);
    return str;
}
JS_DEFINE_CALLINFO_3(extern, STRING, js_ConcatN, CONTEXT, STRINGPTR, UINT32, 0, ACC_STORE_ANY)

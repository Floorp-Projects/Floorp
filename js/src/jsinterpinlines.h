/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Andreas Gal <gal@uci.edu>
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org
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

#ifndef jsinterpinlines_h___
#define jsinterpinlines_h___

#ifndef PRIMITIVE
#define PRIMITIVE(x) x
#endif

static inline void
PRIMITIVE(prim_push_stack)(JSFrameRegs& regs, jsval& v)
{
    *regs.sp++ = v;
}

static inline void
PRIMITIVE(prim_pop_stack)(JSFrameRegs& regs, jsval& v)
{
    v = *--regs.sp;
}

static inline void
PRIMITIVE(prim_store_stack)(JSFrameRegs& regs, int n, jsval& v)
{
    regs.sp[n] = v;
}

static inline void
PRIMITIVE(prim_fetch_stack)(JSFrameRegs& regs, int n, jsval& v)
{
    v = regs.sp[n];
}

static inline void
PRIMITIVE(prim_adjust_stack)(JSFrameRegs& regs, int n)
{
    regs.sp += n;
}

static inline void
PRIMITIVE(prim_generate_constant)(jsval c, jsval& v)
{
    v = c;
}

static inline void
PRIMITIVE(prim_boolean_to_jsval)(JSBool& b, jsval& v)
{
    v = BOOLEAN_TO_JSVAL(b);
}

static inline void
PRIMITIVE(prim_string_to_jsval)(JSString*& str, jsval& v)
{
    v = STRING_TO_JSVAL(str);
}

static inline void
PRIMITIVE(prim_object_to_jsval)(JSObject*& obj, jsval& v)
{
    v = OBJECT_TO_JSVAL(obj);
}

static inline void
PRIMITIVE(prim_id_to_jsval)(jsid& id, jsval& v)
{
    v = ID_TO_VALUE(id);
}

static inline void
PRIMITIVE(push_stack_constant)(JSFrameRegs& regs, jsval c)
{
    jsval v;
    prim_generate_constant(c, v);
    prim_push_stack(regs, v);
}

static inline void
PRIMITIVE(push_stack_boolean)(JSFrameRegs& regs, JSBool& b)
{
    jsval v;
    prim_boolean_to_jsval(b, v);
    prim_push_stack(regs, v);
}

static inline void
PRIMITIVE(push_stack_object)(JSFrameRegs& regs, JSObject*& obj)
{
    jsval v;
    prim_object_to_jsval(obj, v);
    prim_push_stack(regs, v);
}

static inline void
PRIMITIVE(push_stack_id)(JSFrameRegs& regs, jsid& id)
{
    jsval v;
    prim_id_to_jsval(id, v);
    prim_push_stack(regs, v);
}

static inline void
PRIMITIVE(store_stack_constant)(JSFrameRegs& regs, int n, jsval c)
{
    jsval v;
    prim_generate_constant(c, v);
    prim_store_stack(regs, n, v);
}

static inline void
PRIMITIVE(store_stack_boolean)(JSFrameRegs& regs, int n, JSBool& b)
{
    jsval v;
    prim_boolean_to_jsval(b, v);
    prim_store_stack(regs, n, v);
}

static inline void
PRIMITIVE(store_stack_string)(JSFrameRegs& regs, int n, JSString*& str)
{
    jsval v;
    prim_string_to_jsval(str, v);
    prim_store_stack(regs, n, v);
}

static inline void
PRIMITIVE(store_stack_object)(JSFrameRegs& regs, int n, JSObject*& obj)
{
    jsval v;
    prim_object_to_jsval(obj, v);
    prim_store_stack(regs, n, v);
}

static inline bool
PRIMITIVE(guard_jsdouble_is_int_and_int_fits_in_jsval)(jsdouble& d, jsint& i)
{
    return JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i);
}

static inline void
PRIMITIVE(prim_int_to_jsval)(jsint& i, jsval& v)
{
    v = INT_TO_JSVAL(i);
}

static inline bool
PRIMITIVE(call_NewDoubleInRootedValue)(JSContext* cx, jsdouble& d, jsval* vp)
{
    return js_NewDoubleInRootedValue(cx, d, vp);
}

static inline bool
PRIMITIVE(store_number)(JSContext* cx, JSFrameRegs& regs, int n, jsdouble& d)
{
    jsint i;
    if (guard_jsdouble_is_int_and_int_fits_in_jsval(d, i))
        prim_int_to_jsval(i, regs.sp[n]);
    else if (!call_NewDoubleInRootedValue(cx, d, &regs.sp[n]))
        return JS_FALSE;
    return JS_TRUE;
}

static inline bool
PRIMITIVE(guard_int_fits_in_jsval)(jsint& i)
{
    return INT_FITS_IN_JSVAL(i);
}

static inline void
PRIMITIVE(prim_int_to_double)(jsint& i, jsdouble& d)
{
    d = (jsdouble) i;
}

static inline bool
PRIMITIVE(store_int)(JSContext* cx, JSFrameRegs& regs, int n, jsint& i)
{
    if (guard_int_fits_in_jsval(i)) {
        prim_int_to_jsval(i, regs.sp[n]);
    } else {
        jsdouble d;
        prim_int_to_double(i, d);
        if (!call_NewDoubleInRootedValue(cx, d, &regs.sp[n]))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static inline bool
PRIMITIVE(guard_uint_fits_in_jsval)(uint32& u)
{
    return u <= JSVAL_INT_MAX;
}

static inline void
PRIMITIVE(prim_uint_to_jsval)(uint32& u, jsval& v)
{
    v = INT_TO_JSVAL(u);
}

static inline void
PRIMITIVE(prim_uint_to_double)(uint32& u, jsdouble& d)
{
    d = (jsdouble) u;
}

static bool
PRIMITIVE(store_uint)(JSContext* cx, JSFrameRegs& regs, int n, uint32& u)
{
    if (guard_uint_fits_in_jsval(u)) {
        prim_uint_to_jsval(u, regs.sp[n]);
    } else {
        jsdouble d;
        prim_uint_to_double(u, d);
        if (!call_NewDoubleInRootedValue(cx, d, &regs.sp[n]))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static inline bool
PRIMITIVE(guard_jsval_is_int)(jsval& v)
{
    return JSVAL_IS_INT(v);
}

static inline void
PRIMITIVE(prim_jsval_to_int)(jsval& v, jsint& i)
{
    i = JSVAL_TO_INT(v);
}

static inline bool
PRIMITIVE(guard_jsval_is_double)(jsval& v)
{
    return JSVAL_IS_DOUBLE(v);
}

static inline void
PRIMITIVE(prim_jsval_to_double)(jsval& v, jsdouble& d)
{
    d = *JSVAL_TO_DOUBLE(v);
}

static inline void
PRIMITIVE(call_ValueToNumber)(JSContext* cx, jsval* vp, jsdouble& d)
{
    d = js_ValueToNumber(cx, vp);
}

static inline bool
PRIMITIVE(guard_jsval_is_null)(jsval& v)
{
    return JSVAL_IS_NULL(v);
}

/*
 * Optimized conversion function that test for the desired type in v before
 * homing sp and calling a conversion function.
 */
static inline bool
PRIMITIVE(value_to_number)(JSContext* cx, JSFrameRegs& regs, int n, jsval& v, jsdouble& d)
{
    JS_ASSERT(v == regs.sp[n]);
    if (guard_jsval_is_int(v)) {
        int i;
        prim_jsval_to_int(v, i);
        prim_int_to_double(i, d);
    } else if (guard_jsval_is_double(v)) {
        prim_jsval_to_double(v, d);
    } else {
        call_ValueToNumber(cx, &regs.sp[n], d);
        if (guard_jsval_is_null(regs.sp[n]))
            return JS_FALSE;
        JS_ASSERT(JSVAL_IS_NUMBER(regs.sp[n]) || (regs.sp[n] == JSVAL_TRUE));
    }
    return JS_TRUE;
}

static inline bool
PRIMITIVE(fetch_number)(JSContext* cx, JSFrameRegs& regs, int n, jsdouble& d)
{
    jsval v;

    prim_fetch_stack(regs, n, v);
    return value_to_number(cx, regs, n, v, d);
}

static inline void
PRIMITIVE(call_ValueToECMAInt32)(JSContext* cx, jsval* vp, jsint& i)
{
    i = js_ValueToECMAInt32(cx, vp);
}

static inline bool
PRIMITIVE(fetch_int)(JSContext* cx, JSFrameRegs& regs, int n, jsint& i)
{
    jsval v;
    
    prim_fetch_stack(regs, n, v);
    if (guard_jsval_is_int(v)) {
        prim_jsval_to_int(v, i);
    } else {
        call_ValueToECMAInt32(cx, &regs.sp[n], i);
        if (!guard_jsval_is_null(regs.sp[n]))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static inline void
PRIMITIVE(prim_int_to_uint)(jsint& i, uint32& u)
{
    u = (uint32) i;
}

static inline void
PRIMITIVE(call_ValueToECMAUint32)(JSContext* cx, jsval* vp, uint32& u)
{
    u = js_ValueToECMAUint32(cx, vp);
}

static inline bool
PRIMITIVE(fetch_uint)(JSContext* cx, JSFrameRegs& regs, int n, uint32& u)
{
    jsval v;
    
    prim_fetch_stack(regs, n, v);
    if (guard_jsval_is_int(v)) {
        int i;
        prim_jsval_to_int(v, i);
        prim_int_to_uint(i, u);
    } else {
        call_ValueToECMAUint32(cx, &regs.sp[n], u);
        if (guard_jsval_is_null(regs.sp[n]))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static inline void
PRIMITIVE(prim_generate_boolean_constant)(JSBool c, JSBool& b)
{
    b = c;
}

static inline bool
PRIMITIVE(guard_jsval_is_boolean)(jsval& v)
{
    return JSVAL_IS_BOOLEAN(v);
}

static inline void
PRIMITIVE(prim_jsval_to_boolean)(jsval& v, JSBool& b)
{
    b = JSVAL_TO_BOOLEAN(v);
}

static inline void
PRIMITIVE(call_ValueToBoolean)(jsval& v, JSBool& b)
{
    b = js_ValueToBoolean(v);
}

static inline void
PRIMITIVE(pop_boolean)(JSContext* cx, JSFrameRegs& regs, jsval& v, JSBool& b)
{
    prim_fetch_stack(regs, -1, v);
    if (guard_jsval_is_null(v)) {
        prim_generate_boolean_constant(JS_FALSE, b);
    } else if (guard_jsval_is_boolean(v)) {
        prim_jsval_to_boolean(v, b);
    } else {
        call_ValueToBoolean(v, b);
    }
    prim_adjust_stack(regs, -1);
}

static inline bool
PRIMITIVE(guard_jsval_is_primitive)(jsval& v)
{
    return JSVAL_IS_PRIMITIVE(v);
}

static inline void
PRIMITIVE(prim_jsval_to_object)(jsval& v, JSObject*& obj)
{
    obj = JSVAL_TO_OBJECT(v);
}

static inline bool
PRIMITIVE(guard_obj_is_null)(JSObject*& obj)
{
    return !obj;
}

static inline void
PRIMITIVE(call_ValueToNonNullObject)(JSContext* cx, jsval& v, JSObject*& obj)
{
    obj = js_ValueToNonNullObject(cx, v);
}

static inline bool
PRIMITIVE(value_to_object)(JSContext* cx, JSFrameRegs& regs, int n, jsval& v,
                JSObject*& obj)
{
    if (!guard_jsval_is_primitive(v)) {
        prim_jsval_to_object(v, obj);
    } else {
        call_ValueToNonNullObject(cx, v, obj);
        if (guard_obj_is_null(obj))
            return JS_FALSE;
        jsval x;
        prim_object_to_jsval(obj, x);
        prim_store_stack(regs, n, x);
    }
    return JS_TRUE;
}

static inline bool
PRIMITIVE(fetch_object)(JSContext* cx, JSFrameRegs& regs, int n, jsval& v, JSObject*& obj)
{
    prim_fetch_stack(regs, n, v);
    return value_to_object(cx, regs, n, v, obj);
}

static inline bool
PRIMITIVE(call_obj_default_value)(JSContext* cx, JSObject*& obj, JSType hint, jsval* vp)
{
    return OBJ_DEFAULT_VALUE(cx, obj, hint, vp);
}

static inline bool
PRIMITIVE(default_value)(JSContext* cx, JSFrameRegs& regs, int n, JSType hint, jsval& v)
{
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
    JS_ASSERT(v == regs.sp[n]);
    JSObject* obj;
    prim_jsval_to_object(v, obj);
    if (!call_obj_default_value(cx, obj, hint, &regs.sp[n])) 
        return JS_FALSE;
    prim_fetch_stack(regs, n, v);
    return JS_TRUE;
}

static inline void
PRIMITIVE(prim_dadd)(jsdouble& a, jsdouble& b, jsdouble& r)
{
    r = a + b;
}

static inline void
PRIMITIVE(prim_dsub)(jsdouble& a, jsdouble& b, jsdouble& r)
{
    r = a - b;
}

static inline void
PRIMITIVE(prim_dmul)(jsdouble& a, jsdouble& b, jsdouble& r)
{
    r = a * b;
}

static inline void
PRIMITIVE(prim_ddiv)(JSContext* cx, JSRuntime* rt, JSFrameRegs& regs, int n, jsdouble& a,
          jsdouble& b)
{
    if (b == 0) {
        jsval* vp = &regs.sp[n];
#ifdef XP_WIN
        /* XXX MSVC miscompiles such that (NaN == 0) */
        if (JSDOUBLE_IS_NaN(b))
            *vp = DOUBLE_TO_JSVAL(rt->jsNaN);
        else
#endif
        if (a == 0 || JSDOUBLE_IS_NaN(a))
            *vp = DOUBLE_TO_JSVAL(rt->jsNaN);
        else if ((JSDOUBLE_HI32(a) ^ JSDOUBLE_HI32(b)) >> 31)
            *vp = DOUBLE_TO_JSVAL(rt->jsNegativeInfinity);
        else
            *vp = DOUBLE_TO_JSVAL(rt->jsPositiveInfinity);
    } else {
        jsdouble r = a / b;
        store_number(cx, regs, n, r);
    }
}

static inline void
PRIMITIVE(prim_dmod)(JSContext* cx, JSRuntime* rt, JSFrameRegs& regs, int n, jsdouble& a,
          jsdouble& b)
{
    if (b == 0) {
        store_stack_constant(regs, -1, DOUBLE_TO_JSVAL(rt->jsNaN));
    } else {
        jsdouble r;
#ifdef XP_WIN
        /* Workaround MS fmod bug where 42 % (1/0) => NaN, not 42. */
        if (!(JSDOUBLE_IS_FINITE(a) && JSDOUBLE_IS_INFINITE(b)))
            r = a;
        else 
#endif
            r = fmod(a, b);
        store_number(cx, regs, n, r);
    }
}

static inline void
PRIMITIVE(prim_ior)(jsint& a, jsint& b, jsint& r)
{
    r = a | b;
}

static inline void
PRIMITIVE(prim_ixor)(jsint& a, jsint& b, jsint& r)
{
    r = a ^ b;
}

static inline void
PRIMITIVE(prim_iand)(jsint& a, jsint& b, jsint& r)
{
    r = a & b;
}

static inline void
PRIMITIVE(prim_ilsh)(jsint& a, jsint& b, jsint& r)
{
    r = a << (b & 31);
}

static inline void
PRIMITIVE(prim_irsh)(jsint& a, jsint& b, jsint& r)
{
    r = a >> (b & 31);
}

static inline void
PRIMITIVE(prim_ursh)(uint32& a, jsint& b, uint32& r)
{
    r = a >> (b & 31);
}

static inline bool
PRIMITIVE(guard_boolean_is_true)(JSBool& cond)
{
    return cond;
}

static inline void
PRIMITIVE(prim_icmp_lt)(jsint& a, jsint& b, JSBool& r)
{
    r = a < b;
}

static inline void
PRIMITIVE(prim_icmp_le)(jsint& a, jsint& b, JSBool& r)
{
    r = a <= b;
}

static inline void
PRIMITIVE(prim_icmp_gt)(jsint& a, jsint& b, JSBool& r)
{
    r = a > b;
}

static inline void
PRIMITIVE(prim_icmp_ge)(jsint& a, jsint& b, JSBool& r)
{
    r = a >= b;
}

static inline void
PRIMITIVE(prim_dcmp_lt)(bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, <, b, ifnan);
}

static inline void
PRIMITIVE(prim_dcmp_le)(bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, <=, b, ifnan);
}

static inline void
PRIMITIVE(prim_dcmp_gt)(bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, >, b, ifnan);
}

static inline void
PRIMITIVE(prim_dcmp_ge)(bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, >=, b, ifnan);
}

static inline void
PRIMITIVE(prim_generate_int_constant)(jsint c, jsint& v)
{
    v = c;
}

static inline void
PRIMITIVE(prim_jsval_to_string)(jsval& v, JSString*& s)
{
    s = JSVAL_TO_STRING(v);
}

static inline void
PRIMITIVE(call_CompareStrings)(JSString*& a, JSString*& b, jsint& r)
{
    r = js_CompareStrings(a, b);
}

static inline bool
PRIMITIVE(guard_both_jsvals_are_int)(jsval& a, jsval& b)
{
    return (a & b) & JSVAL_INT;
}

static inline bool
PRIMITIVE(guard_both_jsvals_are_string)(jsval& a, jsval& b)
{
    return JSVAL_IS_STRING(a) && JSVAL_IS_STRING(b);
}

/*
 * Unsupported opcodes trigger a trace stop condition and cause the trace
 * recorder to abandon the current trace.
 */
static inline void
PRIMITIVE(trace_stop)(const char* op)
{
    /* If we are not tracing, this is a no-op. */
}

#endif /* jsinterpinlines_h___ */

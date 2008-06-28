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
 *   Andreas Gal <gal@mozilla.com>
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org>
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
PRIMITIVE(prim_copy)(JSContext* cx, jsval& from, jsval& to)
{
    to = from;
}

static inline void
PRIMITIVE(prim_push_stack)(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    *regs.sp++ = v;
}

static inline void
PRIMITIVE(prim_pop_stack)(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    v = *--regs.sp;
}

static inline void
PRIMITIVE(prim_store_stack)(JSContext* cx, JSFrameRegs& regs, int n, jsval& v)
{
    regs.sp[n] = v;
}

static inline void
PRIMITIVE(prim_fetch_stack)(JSContext* cx, JSFrameRegs& regs, int n, jsval& v)
{
    v = regs.sp[n];
}

static inline void
PRIMITIVE(prim_adjust_stack)(JSContext* cx, JSFrameRegs& regs, int n)
{
    regs.sp += n;
}

static inline void
PRIMITIVE(prim_generate_constant)(JSContext* cx, jsval c, jsval& v)
{
    v = c;
}

static inline void
PRIMITIVE(prim_boolean_to_jsval)(JSContext* cx, JSBool& b, jsval& v)
{
    v = BOOLEAN_TO_JSVAL(b);
}

static inline void
PRIMITIVE(prim_string_to_jsval)(JSContext* cx, JSString*& str, jsval& v)
{
    v = STRING_TO_JSVAL(str);
}

static inline void
PRIMITIVE(prim_object_to_jsval)(JSContext* cx, JSObject*& obj, jsval& v)
{
    v = OBJECT_TO_JSVAL(obj);
}

static inline void
PRIMITIVE(prim_id_to_jsval)(JSContext* cx, jsid& id, jsval& v)
{
    v = ID_TO_VALUE(id);
}

static inline bool
PRIMITIVE(guard_jsdouble_is_int_and_int_fits_in_jsval)(JSContext* cx, JSFrameRegs& regs, jsdouble& d, jsint& i)
{
    return JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i);
}

static inline void
PRIMITIVE(prim_int_to_jsval)(JSContext* cx, jsint& i, jsval& v)
{
    v = INT_TO_JSVAL(i);
}

static inline bool
PRIMITIVE(call_NewDoubleInRootedValue)(JSContext* cx, jsdouble& d, jsval& v)
{
    return js_NewDoubleInRootedValue(cx, d, &v);
}

static inline void
PRIMITIVE(prim_int_to_double)(JSContext* cx, jsint& i, jsdouble& d)
{
    d = (jsdouble) i;
}

static inline void
PRIMITIVE(prim_uint_to_jsval)(JSContext* cx, uint32& u, jsval& v)
{
    v = INT_TO_JSVAL(u);
}

static inline void
PRIMITIVE(prim_uint_to_double)(JSContext* cx, uint32& u, jsdouble& d)
{
    d = (jsdouble) u;
}

static inline void
PRIMITIVE(prim_jsval_to_int)(JSContext* cx, jsval& v, jsint& i)
{
    i = JSVAL_TO_INT(v);
}

static inline void
PRIMITIVE(prim_jsval_to_double)(JSContext* cx, jsval& v, jsdouble& d)
{
    d = *JSVAL_TO_DOUBLE(v);
}

static inline void
PRIMITIVE(call_ValueToNumber)(JSContext* cx, jsval& v, jsdouble& d)
{
    d = js_ValueToNumber(cx, &v);
}

static inline bool
PRIMITIVE(guard_jsval_is_null)(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    return JSVAL_IS_NULL(v);
}

static inline void
PRIMITIVE(call_ValueToECMAInt32)(JSContext* cx, jsval& v, jsint& i)
{
    i = js_ValueToECMAInt32(cx, &v);
}

static inline void
PRIMITIVE(prim_int_to_uint)(JSContext* cx, jsint& i, uint32& u)
{
    u = (uint32) i;
}

static inline void
PRIMITIVE(call_ValueToECMAUint32)(JSContext* cx, jsval& v, uint32& u)
{
    u = js_ValueToECMAUint32(cx, &v);
}

static inline void
PRIMITIVE(prim_jsval_to_boolean)(JSContext* cx, jsval& v, JSBool& b)
{
    b = JSVAL_TO_BOOLEAN(v);
}

static inline void
PRIMITIVE(call_ValueToBoolean)(JSContext* cx, jsval& v, JSBool& b)
{
    b = js_ValueToBoolean(v);
}

static inline void
PRIMITIVE(prim_jsval_to_object)(JSContext* cx, jsval& v, JSObject*& obj)
{
    obj = JSVAL_TO_OBJECT(v);
}

static inline bool
PRIMITIVE(guard_obj_is_null)(JSContext* cx, JSFrameRegs& regs, JSObject*& obj)
{
    return !obj;
}

static inline void
PRIMITIVE(prim_load_map_from_obj)(JSContext *cx, JSFrameRegs& regs,
                                  JSObject*& obj, JSObjectMap*& map)
{
    map = obj->map;
}

static inline void
PRIMITIVE(prim_load_ops_from_map)(JSContext *cx, JSFrameRegs& regs,
                                  JSObjectMap*& map, JSObjectOps*& ops)
{
    ops = map->ops;
}

static inline bool
PRIMITIVE(guard_ops_are_xml)(JSContext *cx, JSFrameRegs& regs,
                            JSObjectOps*& ops)
{
    return ops == &js_XMLObjectOps.base;
}

static inline void
PRIMITIVE(call_ValueToNonNullObject)(JSContext* cx, jsval& v, JSObject*& obj)
{
    obj = js_ValueToNonNullObject(cx, v);
}

static inline bool
PRIMITIVE(call_obj_default_value)(JSContext* cx, JSObject*& obj, JSType hint,
                                  jsval& v)
{
    return OBJ_DEFAULT_VALUE(cx, obj, hint, &v);
}

static inline void
PRIMITIVE(prim_dadd)(JSContext* cx, jsdouble& a, jsdouble& b, jsdouble& r)
{
    r = a + b;
}

static inline void
PRIMITIVE(prim_dsub)(JSContext* cx, jsdouble& a, jsdouble& b, jsdouble& r)
{
    r = a - b;
}

static inline void
PRIMITIVE(prim_dmul)(JSContext* cx, jsdouble& a, jsdouble& b, jsdouble& r)
{
    r = a * b;
}

static inline bool
PRIMITIVE(prim_ddiv)(JSContext* cx, JSRuntime* rt, JSFrameRegs& regs, int n,
                     jsdouble& a, jsdouble& b)
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
        return true;
    }

    jsdouble r = a / b;
    jsint i;
    if (JSDOUBLE_IS_INT(r, i) && INT_FITS_IN_JSVAL(i)) {
        regs.sp[n] = INT_TO_JSVAL(i);
        return JS_TRUE;
    }
    return js_NewDoubleInRootedValue(cx, r, &regs.sp[n]);
}

static inline bool
PRIMITIVE(prim_dmod)(JSContext* cx, JSRuntime* rt, JSFrameRegs& regs, int n,
                     jsdouble& a, jsdouble& b)
{
    if (b == 0) {
        regs.sp[n] = DOUBLE_TO_JSVAL(rt->jsNaN);
        return true;
    }

    jsdouble r;
#ifdef XP_WIN
    /* Workaround MS fmod bug where 42 % (1/0) => NaN, not 42. */
    if (!(JSDOUBLE_IS_FINITE(a) && JSDOUBLE_IS_INFINITE(b)))
        r = a;
    else
#endif
        r = fmod(a, b);
    jsint i;
    if (JSDOUBLE_IS_INT(r, i) && INT_FITS_IN_JSVAL(i)) {
        regs.sp[n] = INT_TO_JSVAL(i);
        return JS_TRUE;
    }
    return js_NewDoubleInRootedValue(cx, r, &regs.sp[n]);
}

static inline void
PRIMITIVE(prim_iadd)(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    r = a + b;
}

static inline void
PRIMITIVE(prim_isub)(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    r = a - b;
}

static inline void
PRIMITIVE(prim_int_is_nonzero)(JSContext* cx, jsint& a, JSBool& r)
{
    r = (a != 0);
}

static inline void
PRIMITIVE(prim_ior)(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    r = a | b;
}

static inline void
PRIMITIVE(prim_ixor)(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    r = a ^ b;
}

static inline void
PRIMITIVE(prim_iand)(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    r = a & b;
}

static inline void
PRIMITIVE(prim_ilsh)(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    r = a << (b & 31);
}

static inline void
PRIMITIVE(prim_irsh)(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    r = a >> (b & 31);
}

static inline void
PRIMITIVE(prim_ursh)(JSContext* cx, uint32& a, jsint& b, uint32& r)
{
    r = a >> (b & 31);
}

static inline bool
PRIMITIVE(guard_boolean_is_true)(JSContext* cx, JSFrameRegs& regs, JSBool& cond)
{
    return cond;
}

static inline void
PRIMITIVE(prim_icmp_eq)(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    r = a == b;
}

static inline void
PRIMITIVE(prim_icmp_lt)(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    r = a < b;
}

static inline void
PRIMITIVE(prim_icmp_le)(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    r = a <= b;
}

static inline void
PRIMITIVE(prim_icmp_gt)(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    r = a > b;
}

static inline void
PRIMITIVE(prim_icmp_ge)(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    r = a >= b;
}

static inline void
PRIMITIVE(prim_dcmp_eq)(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b,
                        JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, ==, b, ifnan);
}

static inline void
PRIMITIVE(prim_dcmp_ne)(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b,
                        JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, !=, b, ifnan);
}

static inline void
PRIMITIVE(prim_dcmp_lt)(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, <, b, ifnan);
}

static inline void
PRIMITIVE(prim_dcmp_le)(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, <=, b, ifnan);
}

static inline void
PRIMITIVE(prim_dcmp_gt)(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, >, b, ifnan);
}

static inline void
PRIMITIVE(prim_dcmp_ge)(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    r = JSDOUBLE_COMPARE(a, >=, b, ifnan);
}

static inline void
PRIMITIVE(prim_generate_int_constant)(JSContext* cx, jsint c, jsint& i)
{
    i = c;
}

static inline void
PRIMITIVE(prim_jsval_to_string)(JSContext* cx, jsval& v, JSString*& s)
{
    s = JSVAL_TO_STRING(v);
}

static inline void
PRIMITIVE(call_CompareStrings)(JSContext* cx, JSString*& a, JSString*& b, jsint& r)
{
    r = js_CompareStrings(a, b);
}

static inline void
PRIMITIVE(prim_generate_double_constant)(JSContext* cx, jsdouble c, jsdouble& d)
{
    d = c;
}

static inline void
PRIMITIVE(prim_generate_boolean_constant)(JSContext* cx, JSBool c, JSBool& b)
{
    b = c;
}

static inline void
PRIMITIVE(prim_do_fast_inc_dec)(JSContext* cx, JSFrameRegs& regs, jsval& a, jsval incr, jsval& r)
{
    r = a + incr;
}

static inline void
PRIMITIVE(prim_object_as_boolean)(JSContext* cx, JSObject*& obj, JSBool& r)
{
    r = obj ? false : true;
}

#endif /* jsinterpinlines_h___ */

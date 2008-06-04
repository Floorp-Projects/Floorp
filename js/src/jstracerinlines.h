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
 *   Brendan Eich <brendan@mozilla.org
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

#ifndef jstracerinlines_h___
#define jstracerinlines_h___

#define PRIMITIVE(x) interp_##x

#include "jsinterpinlines.h"	

#undef PRIMITIVE

static inline void
prim_push_stack(JSFrameRegs& regs, jsval& v)
{
    interp_prim_push_stack(regs, v);
}

static inline void
prim_pop_stack(JSFrameRegs& regs, jsval& v)
{
    interp_prim_pop_stack(regs, v);
}

static inline void
prim_store_stack(JSFrameRegs& regs, int n, jsval& v)
{
    interp_prim_store_stack(regs, n, v);
}

static inline void
prim_fetch_stack(JSFrameRegs& regs, int n, jsval& v)
{
    interp_prim_fetch_stack(regs, n, v);
}

static inline void
prim_adjust_stack(JSFrameRegs& regs, int n)
{
    interp_prim_adjust_stack(regs, n);
}

static inline void
prim_generate_constant(jsval c, jsval& v)
{
    interp_prim_generate_constant(c, v);
}

static inline void
prim_boolean_to_jsval(JSBool& b, jsval& v)
{
    interp_prim_boolean_to_jsval(b, v);
}

static inline void
prim_string_to_jsval(JSString*& str, jsval& v)
{
    interp_prim_string_to_jsval(str, v);
}

static inline void
prim_object_to_jsval(JSObject*& obj, jsval& v)
{
    interp_prim_object_to_jsval(obj, v);
}

static inline void
prim_id_to_jsval(jsid& id, jsval& v)
{
    interp_prim_id_to_jsval(id, v);
}

static inline bool
guard_jsdouble_is_int_and_int_fits_in_jsval(jsdouble& d, jsint& i)
{
    return interp_guard_jsdouble_is_int_and_int_fits_in_jsval(d, i);
}

static inline void
prim_int_to_jsval(jsint& i, jsval& v)
{
    interp_prim_int_to_jsval(i, v);
}

static inline bool
call_NewDoubleInRootedValue(JSContext* cx, jsdouble& d, jsval* vp)
{
    return interp_call_NewDoubleInRootedValue(cx, d, vp);
}

static inline bool
guard_int_fits_in_jsval(jsint& i)
{
    return interp_guard_int_fits_in_jsval(i);
}

static inline void
prim_int_to_double(jsint& i, jsdouble& d)
{
    interp_prim_int_to_double(i, d);
}

static inline bool
guard_uint_fits_in_jsval(uint32& u)
{
    return interp_guard_uint_fits_in_jsval(u);
}

static inline void
prim_uint_to_jsval(uint32& u, jsval& v)
{
    interp_prim_uint_to_jsval(u, v);
}

static inline void
prim_uint_to_double(uint32& u, jsdouble& d)
{
    interp_prim_uint_to_double(u, d);
}

static inline bool
guard_jsval_is_int(jsval& v)
{
    return interp_guard_jsval_is_int(v);
}

static inline void
prim_jsval_to_int(jsval& v, jsint& i)
{
    interp_prim_jsval_to_int(v, i);
}

static inline bool
guard_jsval_is_double(jsval& v)
{
    return interp_guard_jsval_is_double(v);
}

static inline void
prim_jsval_to_double(jsval& v, jsdouble& d)
{
    interp_prim_jsval_to_double(v, d);
}

static inline void
call_ValueToNumber(JSContext* cx, jsval* vp, jsdouble& d)
{
    interp_call_ValueToNumber(cx, vp, d);
}

static inline bool
guard_jsval_is_null(jsval& v)
{
    return interp_guard_jsval_is_null(v);
}

static inline void
call_ValueToECMAInt32(JSContext* cx, jsval* vp, jsint& i)
{
    interp_call_ValueToECMAInt32(cx, vp, i);
}

static inline void
prim_int_to_uint(jsint& i, uint32& u)
{
    interp_prim_int_to_uint(i, u);
}

static inline void
call_ValueToECMAUint32(JSContext* cx, jsval* vp, uint32& u)
{
    interp_call_ValueToECMAUint32(cx, vp, u);
}

static inline void
prim_generate_boolean_constant(JSBool c, JSBool& b)
{
    interp_prim_generate_boolean_constant(c, b);
}

static inline bool
guard_jsval_is_boolean(jsval& v)
{
    return interp_guard_jsval_is_boolean(v);
}

static inline void
prim_jsval_to_boolean(jsval& v, JSBool& b)
{
    interp_prim_jsval_to_boolean(v, b);
}

static inline void
call_ValueToBoolean(jsval& v, JSBool& b)
{
    interp_call_ValueToBoolean(v, b);
}

static inline bool
guard_jsval_is_primitive(jsval& v)
{
    return interp_guard_jsval_is_primitive(v);
}

static inline void
prim_jsval_to_object(jsval& v, JSObject*& obj)
{
    interp_prim_jsval_to_object(v, obj);
}

static inline bool
guard_obj_is_null(JSObject*& obj)
{
    return interp_guard_obj_is_null(obj);
}

static inline void
call_ValueToNonNullObject(JSContext* cx, jsval& v, JSObject*& obj)
{
    interp_call_ValueToNonNullObject(cx, v, obj);
}

static inline bool
call_obj_default_value(JSContext* cx, JSObject*& obj, JSType hint,
                                  jsval* vp)
{
    return interp_call_obj_default_value(cx, obj, hint, vp);
}

static inline void
prim_dadd(jsdouble& a, jsdouble& b, jsdouble& r)
{
    interp_prim_dadd(a, b, r);
}

static inline void
prim_dsub(jsdouble& a, jsdouble& b, jsdouble& r)
{
    interp_prim_dsub(a, b, r);
}

static inline void
prim_dmul(jsdouble& a, jsdouble& b, jsdouble& r)
{
    interp_prim_dmul(a, b, r);
}

static inline bool
prim_ddiv(JSContext* cx, JSRuntime* rt, JSFrameRegs& regs, int n,
                     jsdouble& a, jsdouble& b)
{
    return interp_prim_ddiv(cx, rt, regs, n, a, b);
}

static inline bool
prim_dmod(JSContext* cx, JSRuntime* rt, JSFrameRegs& regs, int n,
                     jsdouble& a, jsdouble& b)
{
    return interp_prim_dmod(cx, rt, regs, n, a, b);
}

static inline void
prim_ior(jsint& a, jsint& b, jsint& r)
{
    interp_prim_ior(a, b, r);
}

static inline void
prim_ixor(jsint& a, jsint& b, jsint& r)
{
    interp_prim_ixor(a, b, r);
}

static inline void
prim_iand(jsint& a, jsint& b, jsint& r)
{
    interp_prim_iand(a, b, r);
}

static inline void
prim_ilsh(jsint& a, jsint& b, jsint& r)
{
    interp_prim_ilsh(a, b, r);
}

static inline void
prim_irsh(jsint& a, jsint& b, jsint& r)
{
    interp_prim_irsh(a, b, r);
}

static inline void
prim_ursh(uint32& a, jsint& b, uint32& r)
{
    interp_prim_ursh(a, b, r);
}

static inline bool
guard_boolean_is_true(JSBool& cond)
{
    return interp_guard_boolean_is_true(cond);
}

static inline void
prim_icmp_lt(jsint& a, jsint& b, JSBool& r)
{
    interp_prim_icmp_lt(a, b, r);
}

static inline void
prim_icmp_le(jsint& a, jsint& b, JSBool& r)
{
    interp_prim_icmp_le(a, b, r);
}

static inline void
prim_icmp_gt(jsint& a, jsint& b, JSBool& r)
{
    interp_prim_icmp_gt(a, b, r);
}

static inline void
prim_icmp_ge(jsint& a, jsint& b, JSBool& r)
{
    interp_prim_icmp_ge(a, b, r);
}

static inline void
prim_dcmp_lt(bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    interp_prim_dcmp_lt(ifnan, a, b, r);
}

static inline void
prim_dcmp_le(bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    interp_prim_dcmp_le(ifnan, a, b, r);
}

static inline void
prim_dcmp_gt(bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    interp_prim_dcmp_gt(ifnan, a, b, r);
}

static inline void
prim_dcmp_ge(bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    interp_prim_dcmp_ge(ifnan, a, b, r);
}

static inline void
prim_generate_int_constant(jsint c, jsint& v)
{
    interp_prim_generate_int_constant(c, v);
}

static inline void
prim_jsval_to_string(jsval& v, JSString*& s)
{
    interp_prim_jsval_to_string(v, s);
}

static inline void
call_CompareStrings(JSString*& a, JSString*& b, jsint& r)
{
    interp_call_CompareStrings(a, b, r);
}

static inline bool
guard_both_jsvals_are_int(jsval& a, jsval& b)
{
    return interp_guard_both_jsvals_are_int(a, b);
}

static inline bool
guard_both_jsvals_are_string(jsval& a, jsval& b)
{
    return interp_guard_both_jsvals_are_string(a, b);
}

static inline void 
trace_start(JSContext* cx, jsbytecode* pc) 
{
}

static inline void
trace_stop(JSContext* cx, const char* op)
{
}

#endif /* jstracerinlines_h___ */

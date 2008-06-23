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

#define set(v, l)       JS_TRACE_MONITOR(cx).tracker.set(v, l)
#define get(v)          JS_TRACE_MONITOR(cx).tracker.get(v)
#define copy(a, v)      set(v, get(a))
#define L               (*JS_TRACE_MONITOR(cx).lir)
#define F(name)         F_##name
#define G(ok)           (ok ? LIR_xf : LIR_xt)
#define unary(op, a, v) set(v, L.ins1(op, get(a)))
#define binary(op, a, b, v)                                                   \
                        set(v, L.ins2(op, get(a), get(b)))
#define call1(n, a, v)                                                        \
    do {                                                                      \
    LInsp args[] = { get(a) };                                                \
    set(v, L.insCall(F(n), args));                                            \
    } while(0)                                                            
#define call2(n, a, b, v)                                                     \
    do {                                                                      \
    LInsp args[] = { get(a), get(b) };                                        \
    set(v, L.insCall(F(n), args));                                            \
    } while(0)                                                            
#define call3(n, a, b, c, v)                                                  \
    do {                                                                      \
    LInsp args[] = { get(a), get(b), get(c) };                                \
    set(v, L.insCall(F(n), args));                                            \
    } while(0)                                                            

#define STACK_OFFSET(p) (((char*)(p)) - ((char*)JS_TRACE_MONITOR(cx).entryState.sp))

static inline SideExit*
snapshot(JSContext* cx, JSFrameRegs& regs, SideExit& exit)
{
    memset(&exit, 0, sizeof(exit));
    exit.from = JS_TRACE_MONITOR(cx).fragment;
    return &exit;
}

static inline void
prim_copy(JSContext* cx, jsval& from, jsval& to)
{
    interp_prim_copy(cx, from, to);
    copy(&from, &to);
}

static inline void
prim_push_stack(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    set(regs.sp, get(&v));
    L.insStorei(get(&v), JS_TRACE_MONITOR(cx).fragment->param1, STACK_OFFSET(regs.sp));
    interp_prim_push_stack(cx, regs, v);
}

static inline void
prim_pop_stack(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    interp_prim_pop_stack(cx, regs, v);
    set(&v, get(regs.sp));
}

static inline void
prim_store_stack(JSContext* cx, JSFrameRegs& regs, int n, jsval& v)
{
    interp_prim_store_stack(cx, regs, n, v);
    set(&regs.sp[n], get(&v));
    L.insStorei(get(&v), JS_TRACE_MONITOR(cx).fragment->param1, STACK_OFFSET(&regs.sp[n]));
}

static inline void
prim_fetch_stack(JSContext* cx, JSFrameRegs& regs, int n, jsval& v)
{
    interp_prim_fetch_stack(cx, regs, n, v);
    set(&v, get(&regs.sp[n]));
}
    
static inline void
prim_adjust_stack(JSContext* cx, JSFrameRegs& regs, int n)
{
    interp_prim_adjust_stack(cx, regs, n);
}

static inline void
prim_generate_constant(JSContext* cx, jsval c, jsval& v)
{
    interp_prim_generate_constant(cx, c, v);
    if (JSVAL_IS_DOUBLE(c)) {
        jsdouble d = *JSVAL_TO_DOUBLE(c);
        set(&v, L.insImmq(*(uint64_t*)&d));
    } else {
        int32_t x;
        if (JSVAL_IS_BOOLEAN(c)) {
            x = JSVAL_TO_BOOLEAN(c);
        } else if (JSVAL_IS_INT(c)) {
            x = JSVAL_TO_INT(c);
        } else if (JSVAL_IS_STRING(c)) {
            x = (int32_t)JSVAL_TO_STRING(c);
        } else {
            JS_ASSERT(JSVAL_IS_OBJECT(c));
            x = (int32_t)JSVAL_TO_OBJECT(c);
        }
        set(&v, L.insImm(x));
    }
}

static inline void
prim_boolean_to_jsval(JSContext* cx, JSBool& b, jsval& v)
{
    interp_prim_boolean_to_jsval(cx, b, v);
    copy(&b, &v);
}

static inline void
prim_string_to_jsval(JSContext* cx, JSString*& str, jsval& v)
{
    interp_prim_string_to_jsval(cx, str, v);
    copy(&str, &v);
}

static inline void
prim_object_to_jsval(JSContext* cx, JSObject*& obj, jsval& v)
{
    interp_prim_object_to_jsval(cx, obj, v);
    copy(&obj, &v);
}

static inline void
prim_id_to_jsval(JSContext* cx, jsid& id, jsval& v)
{
    interp_prim_id_to_jsval(cx, id, v);
    copy(&id, &v);
}

static inline bool
guard_jsdouble_is_int_and_int_fits_in_jsval(JSContext* cx, JSFrameRegs& regs, jsdouble& d, jsint& i)
{
    bool ok = interp_guard_jsdouble_is_int_and_int_fits_in_jsval(cx, regs, d, i);
    /* We do not box in trace code, so ints always fit and we only check
       that it is actually an int. */
    call1(DOUBLE_IS_INT, &d, &ok); 
    SideExit exit;
    L.insGuard(G(ok), 
            get(&ok),
            snapshot(cx, regs, exit)); 
    unary(LIR_callh, &ok, &i);
    return ok;
}

static inline void
prim_int_to_jsval(JSContext* cx, jsint& i, jsval& v)
{
    interp_prim_int_to_jsval(cx, i, v);
    copy(&i, &v);
}

static inline bool
call_NewDoubleInRootedValue(JSContext* cx, jsdouble& d, jsval& v)
{
    bool ok = interp_call_NewDoubleInRootedValue(cx, d, v);
    copy(&d, &v);
    return ok;
}

static inline bool
guard_int_fits_in_jsval(JSContext* cx, JSFrameRegs& regs, jsint& i)
{
    bool ok = interp_guard_int_fits_in_jsval(cx, regs, i);
    return ok;
}

static inline void
prim_int_to_double(JSContext* cx, jsint& i, jsdouble& d)
{
    interp_prim_int_to_double(cx, i, d);
    unary(LIR_i2f, &i, &d);
}

static inline bool
guard_uint_fits_in_jsval(JSContext* cx, JSFrameRegs& regs, uint32& u)
{
    bool ok = interp_guard_uint_fits_in_jsval(cx, regs, u);
    return ok;
}

static inline void
prim_uint_to_jsval(JSContext* cx, uint32& u, jsval& v)
{
    interp_prim_uint_to_jsval(cx, u, v);
    copy(&u, &v);
}

static inline void
prim_uint_to_double(JSContext* cx, uint32& u, jsdouble& d)
{
    interp_prim_uint_to_double(cx, u, d);
    unary(LIR_u2f, &u, &d);
}

static inline bool
guard_jsval_is_int(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    bool ok = interp_guard_jsval_is_int(cx, regs, v);
    return ok;
}

static inline void
prim_jsval_to_int(JSContext* cx, jsval& v, jsint& i)
{
    interp_prim_jsval_to_int(cx, v, i);
    copy(&v, &i);
}

static inline bool
guard_jsval_is_double(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    bool ok = interp_guard_jsval_is_double(cx, regs, v);
    return ok;
}

static inline void
prim_jsval_to_double(JSContext* cx, jsval& v, jsdouble& d)
{
    interp_prim_jsval_to_double(cx, v, d);
    copy(&v, &d);
}

static inline void
call_ValueToNumber(JSContext* cx, jsval& v, jsdouble& d)
{
    interp_call_ValueToNumber(cx, v, d);
    call2(ValueToNumber, cx, &v, &d);
}

static inline bool
guard_jsval_is_null(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    bool ok = interp_guard_jsval_is_null(cx, regs, v);
    if (JSVAL_IS_OBJECT(v)) {
        SideExit exit;
        L.insGuard(G(ok),
                L.ins2(LIR_eq, get(&v), L.insImm(0)),
                snapshot(cx, regs, exit));
    }
    return ok;
}

static inline void
call_ValueToECMAInt32(JSContext* cx, jsval& v, jsint& i)
{
    interp_call_ValueToECMAInt32(cx, v, i);
    call2(ValueToECMAInt32, cx, &v, &i);
}

static inline void
prim_int_to_uint(JSContext* cx, jsint& i, uint32& u)
{
    interp_prim_int_to_uint(cx, i, u);
    copy(&i, &u);
}

static inline void
call_ValueToECMAUint32(JSContext* cx, jsval& v, uint32& u)
{
    interp_call_ValueToECMAUint32(cx, v, u);
    call2(ValueToECMAUint32, cx, &v, &u);
}

static inline void
prim_generate_boolean_constant(JSContext* cx, JSBool c, JSBool& b)
{
    interp_prim_generate_boolean_constant(cx, c, b);
    set(&b, L.insImm(c));
}

static inline bool
guard_jsval_is_boolean(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    bool ok = interp_guard_jsval_is_boolean(cx, regs, v);
    return ok;
}

static inline void
prim_jsval_to_boolean(JSContext* cx, jsval& v, JSBool& b)
{
    interp_prim_jsval_to_boolean(cx, v, b);
    copy(&v, &b);
}

static inline void
call_ValueToBoolean(JSContext* cx, jsval& v, JSBool& b)
{
    interp_call_ValueToBoolean(cx, v, b);
    call2(ValueToBoolean, cx, &v, &b);
}

static inline bool
guard_jsval_is_primitive(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    bool ok = interp_guard_jsval_is_primitive(cx, regs, v);
    return ok;
}

static inline void
prim_jsval_to_object(JSContext* cx, jsval& v, JSObject*& obj)
{
    interp_prim_jsval_to_object(cx, v, obj);
    copy(&v, &obj);
}

static inline bool
guard_obj_is_null(JSContext* cx, JSFrameRegs& regs, JSObject*& obj)
{
    bool ok = interp_guard_obj_is_null(cx, regs, obj);
    SideExit exit;
    L.insGuard(G(ok),
            L.ins2(LIR_eq, get(&obj), L.insImm(0)),
            snapshot(cx, regs, exit));
    return ok;
}

static inline void
call_ValueToNonNullObject(JSContext* cx, jsval& v, JSObject*& obj)
{
    interp_call_ValueToNonNullObject(cx, v, obj);
    call2(ValueToNonNullObject, cx, &v, &obj);
}

static inline bool
call_obj_default_value(JSContext* cx, JSObject*& obj, JSType hint,
                       jsval& v)
{
    bool ok = interp_call_obj_default_value(cx, obj, hint, v);
    call3(obj_default_value, cx, &obj, L.insImm(hint), &v);
    return ok;
}

static inline void
prim_dadd(JSContext* cx, jsdouble& a, jsdouble& b, jsdouble& r)
{
    interp_prim_dadd(cx, a, b, r);
    binary(LIR_fadd, &a, &b, &r);
}

static inline void
prim_dsub(JSContext* cx, jsdouble& a, jsdouble& b, jsdouble& r)
{
    interp_prim_dsub(cx, a, b, r);
    binary(LIR_fsub, &a, &b, &r);
}

static inline void
prim_dmul(JSContext* cx, jsdouble& a, jsdouble& b, jsdouble& r)
{
    interp_prim_dmul(cx, a, b, r);
    binary(LIR_fmul, &a, &b, &r);
}

static inline bool
prim_ddiv(JSContext* cx, JSRuntime* rt, JSFrameRegs& regs, int n,
                     jsdouble& a, jsdouble& b)
{
    bool ok = interp_prim_ddiv(cx, rt, regs, n, a, b);
    binary(LIR_fdiv, &a, &b, &regs.sp[n]);
    return ok;
}

static inline bool
prim_dmod(JSContext* cx, JSRuntime* rt, JSFrameRegs& regs, int n,
                     jsdouble& a, jsdouble& b)
{
    bool ok = interp_prim_dmod(cx, rt, regs, n, a, b);
    call2(dmod, &a, &b, &regs.sp[n]);
    return ok;
}

static inline void
prim_ior(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    interp_prim_ior(cx, a, b, r);
    binary(LIR_or, &a, &b, &r);
}

static inline void
prim_ixor(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    interp_prim_ixor(cx, a, b, r);
    binary(LIR_xor, &a, &b, &r);
}

static inline void
prim_iand(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    interp_prim_iand(cx, a, b, r);
    binary(LIR_and, &a, &b, &r);
}

static inline void
prim_ilsh(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    interp_prim_ilsh(cx, a, b, r);
    binary(LIR_lsh, &a, &b, &r);
}

static inline void
prim_irsh(JSContext* cx, jsint& a, jsint& b, jsint& r)
{
    interp_prim_irsh(cx, a, b, r);
    binary(LIR_rsh, &a, &b, &r);
}

static inline void
prim_ursh(JSContext* cx, uint32& a, jsint& b, uint32& r)
{
    interp_prim_ursh(cx, a, b, r);
    binary(LIR_ush, &a, &b, &r);
}

static inline bool
guard_boolean_is_true(JSContext* cx, JSFrameRegs& regs, JSBool& cond)
{
    bool ok = interp_guard_boolean_is_true(cx, regs, cond);
    SideExit exit;
    L.insGuard(G(ok),
            L.ins2(LIR_eq, get(&cond), L.insImm(0)),
            snapshot(cx, regs, exit));
    return ok;
}

static inline void
prim_icmp_lt(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    interp_prim_icmp_lt(cx, a, b, r);
    binary(LIR_lt, &a, &b, &r);
}

static inline void
prim_icmp_le(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    interp_prim_icmp_le(cx, a, b, r);
    binary(LIR_le, &a, &b, &r);
}

static inline void
prim_icmp_gt(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    interp_prim_icmp_gt(cx, a, b, r);
    binary(LIR_gt, &a, &b, &r);
}

static inline void
prim_icmp_ge(JSContext* cx, jsint& a, jsint& b, JSBool& r)
{
    interp_prim_icmp_ge(cx, a, b, r);
    binary(LIR_ge, &a, &b, &r);
}

static inline void
prim_dcmp_lt(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    interp_prim_dcmp_lt(cx, ifnan, a, b, r);
    binary(LIR_lt, &a, &b, &r); // TODO: check ifnan handling
}

static inline void
prim_dcmp_le(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    interp_prim_dcmp_le(cx, ifnan, a, b, r);
    binary(LIR_le, &a, &b, &r); // TODO: check ifnan handling
}

static inline void
prim_dcmp_gt(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    interp_prim_dcmp_gt(cx, ifnan, a, b, r);
    binary(LIR_gt, &a, &b, &r); // TODO: check ifnan handling
}

static inline void
prim_dcmp_ge(JSContext* cx, bool ifnan, jsdouble& a, jsdouble& b, JSBool& r)
{
    interp_prim_dcmp_ge(cx, ifnan, a, b, r);
    binary(LIR_ge, &a, &b, &r); // TODO: check ifnan handling
}

static inline void
prim_generate_int_constant(JSContext* cx, jsint c, jsint& i)
{
    interp_prim_generate_int_constant(cx, c, i);
    set(&i, L.insImm(c));
}

static inline void
prim_jsval_to_string(JSContext* cx, jsval& v, JSString*& str)
{
    interp_prim_jsval_to_string(cx, v, str);
    copy(&v, &str);
}

static inline void
call_CompareStrings(JSContext* cx, JSString*& a, JSString*& b, jsint& r)
{
    interp_call_CompareStrings(cx, a, b, r);
    call2(CompareStrings, &a, &b, &r);
}

static inline bool
guard_both_jsvals_are_int(JSContext* cx, JSFrameRegs& regs, jsval& a, jsval& b)
{
    bool ok = interp_guard_both_jsvals_are_int(cx, regs, a, b);
    return ok;
}

static inline bool
guard_both_jsvals_are_string(JSContext* cx, JSFrameRegs& regs, jsval& a, jsval& b)
{
    bool ok = interp_guard_both_jsvals_are_string(cx, regs, a, b);
    return ok;
}

static inline bool
guard_can_do_fast_inc_dec(JSContext* cx, JSFrameRegs& regs, jsval& v)
{
    bool ok = interp_guard_can_do_fast_inc_dec(cx, regs, v);
    /* to do a fast inc/dec the topmost two bits must be both 0 or 1 */
    SideExit exit;
    L.insGuard(G(ok),
            L.ins2(LIR_eq, 
                    L.ins2(LIR_and,
                            L.ins2(LIR_xor,
                                    L.ins2(LIR_lsh, get(&v), L.insImm(1)),
                                    get(&v)),
                            L.insImm(0x80000000)),
                    L.insImm(0)),
            snapshot(cx, regs, exit));
    return ok;
}

static inline void
prim_generate_double_constant(JSContext* cx, jsdouble c, jsdouble& d)
{
    interp_prim_generate_double_constant(cx, c, d);
    set(&d, L.insImmq(*(uint64_t*)&c));
}

static inline void
prim_do_fast_inc_dec(JSContext* cx, jsval& a, jsval incr, jsval& r)
{
    interp_prim_do_fast_inc_dec(cx, a, incr, r);
    set(&r, L.ins2(LIR_add, get(&a), L.insImm(incr/2)));
}

#undef set
#undef get
#undef copy
#undef L
#undef F
#undef G
#undef call1
#undef call2
#undef call3

#endif /* jstracerinlines_h___ */

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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
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

#ifndef jsbuiltins_h___
#define jsbuiltins_h___

#ifdef JS_TRACER

#include "nanojit.h"

enum JSTNErrType { INFALLIBLE, FAIL_NULL, FAIL_NEG, FAIL_VOID, FAIL_JSVAL };
enum { JSTN_ERRTYPE_MASK = 7, JSTN_MORE = 8 };

#define JSTN_ERRTYPE(jstn)  ((jstn)->flags & JSTN_ERRTYPE_MASK)

/*
 * |prefix| and |argtypes| declare what arguments should be passed to the
 * native function.  |prefix| can contain the following characters:
 *
 * 'C': a JSContext* argument
 * 'T': |this| as a JSObject* argument (bails if |this| is not an object)
 * 'S': |this| as a JSString* argument (bails if |this| is not a string)
 * 'R': a JSRuntime* argument
 * 'P': the pc as a jsbytecode*
 * 'D': |this| as a number (jsdouble)
 * 'f': the function being called, as a JSObject*
 * 'p': the .prototype of the function, as a JSObject*
 *
 * The corresponding things will get passed as arguments to the builtin in
 * reverse order (so TC means JSContext* as the first arg, and the
 * JSObject* for |this| as the second arg).
 *
 * |argtypes| can contain the following characters:
 * 'd': a number (double) argument
 * 'i': an integer argument
 * 's': a JSString* argument
 * 'o': a JSObject* argument
 * 'r': a JSObject* argument that is of class js_RegExpClass
 * 'f': a JSObject* argument that is of class js_FunctionClass
 * 'v': a jsval argument (boxing whatever value is actually being passed in)
 */
struct JSTraceableNative {
    JSFastNative            native;
    const nanojit::CallInfo *builtin;
    const char              *prefix;
    const char              *argtypes;
    uintN                   flags;  /* JSTN_MORE | JSTNErrType */
};

/*
 * We use a magic boxed pointer value to represent error conditions that
 * trigger a side exit. The address is so low that it should never be actually
 * in use. If it is, a performance regression occurs, not an actual runtime
 * error.
 */
#define JSVAL_ERROR_COOKIE OBJECT_TO_JSVAL((void*)0x10)

/*
 * We also need a magic unboxed 32-bit integer that signals an error.  Again if
 * this number is hit we experience a performance regression, not a runtime
 * error.
 */
#define INT32_ERROR_COOKIE 0xffffabcd

/* Macros used by JS_DEFINE_CALLINFOn. */
#ifdef DEBUG
#define _JS_CI_NAME(op) ,#op
#else
#define _JS_CI_NAME(op)
#endif

#define  _JS_I32_ARGSIZE    nanojit::ARGSIZE_LO
#define  _JS_I32_RETSIZE    nanojit::ARGSIZE_LO
#define  _JS_F64_ARGSIZE    nanojit::ARGSIZE_F
#define  _JS_F64_RETSIZE    nanojit::ARGSIZE_F
#define  _JS_PTR_ARGSIZE    nanojit::ARGSIZE_LO
#if defined AVMPLUS_64BIT
# define _JS_PTR_RETSIZE    nanojit::ARGSIZE_Q
#else
# define _JS_PTR_RETSIZE    nanojit::ARGSIZE_LO
#endif

/* Supported types for builtin functions. */
#define _JS_TYPEINFO(ctype, size)  (ctype, size)
#define _JS_TYPEINFO_CONTEXT       _JS_TYPEINFO(JSContext *,             _JS_PTR)
#define _JS_TYPEINFO_RUNTIME       _JS_TYPEINFO(JSRuntime *,             _JS_PTR)
#define _JS_TYPEINFO_JSVAL         _JS_TYPEINFO(jsval,                   _JS_I32)
#define _JS_TYPEINFO_BOOL          _JS_TYPEINFO(JSBool,                  _JS_I32)
#define _JS_TYPEINFO_INT32         _JS_TYPEINFO(int32,                   _JS_I32)
#define _JS_TYPEINFO_UINT32        _JS_TYPEINFO(uint32,                  _JS_I32)
#define _JS_TYPEINFO_DOUBLE        _JS_TYPEINFO(jsdouble,                _JS_F64)
#define _JS_TYPEINFO_STRING        _JS_TYPEINFO(JSString *,              _JS_PTR)
#define _JS_TYPEINFO_OBJECT        _JS_TYPEINFO(JSObject *,              _JS_PTR)
#define _JS_TYPEINFO_SCOPEPROP     _JS_TYPEINFO(JSScopeProperty *,       _JS_PTR)
#define _JS_TYPEINFO_PC            _JS_TYPEINFO(jsbytecode *,            _JS_PTR)
#define _JS_TYPEINFO_GUARDRECORD   _JS_TYPEINFO(nanojit::GuardRecord *,  _JS_PTR)
#define _JS_TYPEINFO_INTERPSTATE   _JS_TYPEINFO(avmplus::InterpState *,  _JS_PTR)
#define _JS_TYPEINFO_FRAGMENT      _JS_TYPEINFO(nanojit::Fragment *,     _JS_PTR)

#define _JS_EXPAND(tokens)  tokens

#define _JS_CTYPE2(ctype, size)    ctype
#define _JS_CTYPE(tyname)          _JS_EXPAND(_JS_CTYPE2 _JS_TYPEINFO_##tyname)
#define _JS_RETSIZE2(ctype, size)  size##_ARGSIZE
#define _JS_RETSIZE(tyname)        _JS_EXPAND(_JS_RETSIZE2 _JS_TYPEINFO_##tyname)
#define _JS_ARGSIZE2(ctype, size)  size##_RETSIZE
#define _JS_ARGSIZE(tyname)        _JS_EXPAND(_JS_ARGSIZE2 _JS_TYPEINFO_##tyname)

#define _JS_static_TN(t)  static t
#define _JS_static_CI     static
#define _JS_extern_TN(t)  extern t
#define _JS_extern_CI
#define _JS_FRIEND_TN(t)  extern JS_FRIEND_API(t)
#define _JS_FRIEND_CI
#define _JS_EXPAND_TN_LINKAGE(linkage, t)  _JS_##linkage##_TN(t)
#define _JS_EXPAND_CI_LINKAGE(linkage)     _JS_##linkage##_CI

#define _JS_CALLINFO(name) name##_ci

#define _JS_DEFINE_CALLINFO(linkage, name, crtype, cargtypes, argtypes, cse, fold)                \
    _JS_EXPAND_TN_LINKAGE(linkage, crtype) FASTCALL name cargtypes;                               \
    _JS_EXPAND_CI_LINKAGE(linkage) const nanojit::CallInfo _JS_CALLINFO(name) =                   \
        { (intptr_t) &name, argtypes, cse, fold, nanojit::ABI_FASTCALL _JS_CI_NAME(name) };

/*
 * Declare a C function named <op> and a CallInfo struct named <op>_callinfo so the
 * tracer can call it. |linkage| controls the visibility of both the function
 * and the CallInfo global. It can be extern, static, or FRIEND, which
 * specifies JS_FRIEND_API linkage for the function.
 */
#define JS_DEFINE_CALLINFO_1(linkage, rt, op, at0, cse, fold)                                     \
    _JS_DEFINE_CALLINFO(linkage, op, _JS_CTYPE(rt), (_JS_CTYPE(at0)),                             \
                        (_JS_ARGSIZE(at0) << 2) | _JS_RETSIZE(rt), cse, fold)
#define JS_DEFINE_CALLINFO_2(linkage, rt, op, at0, at1, cse, fold)                                \
    _JS_DEFINE_CALLINFO(linkage, op, _JS_CTYPE(rt), (_JS_CTYPE(at0), _JS_CTYPE(at1)),             \
                        (_JS_ARGSIZE(at0) << 4) | (_JS_ARGSIZE(at1) << 2) | _JS_RETSIZE(rt),      \
                        cse, fold)
#define JS_DEFINE_CALLINFO_3(linkage, rt, op, at0, at1, at2, cse, fold)                           \
    _JS_DEFINE_CALLINFO(linkage, op, _JS_CTYPE(rt),                                               \
                        (_JS_CTYPE(at0), _JS_CTYPE(at1), _JS_CTYPE(at2)),                         \
                        (_JS_ARGSIZE(at0) << 6) | (_JS_ARGSIZE(at1) << 4) |                       \
                        (_JS_ARGSIZE(at2) << 2) | _JS_RETSIZE(rt),                                \
                        cse, fold)
#define JS_DEFINE_CALLINFO_4(linkage, rt, op, at0, at1, at2, at3, cse, fold)                      \
    _JS_DEFINE_CALLINFO(linkage, op, _JS_CTYPE(rt),                                               \
                        (_JS_CTYPE(at0), _JS_CTYPE(at1), _JS_CTYPE(at2), _JS_CTYPE(at3)),         \
                        (_JS_ARGSIZE(at0) << 8) | (_JS_ARGSIZE(at1) << 6) |                       \
                        (_JS_ARGSIZE(at2) << 4) | (_JS_ARGSIZE(at3) << 2) | _JS_RETSIZE(rt),      \
                        cse, fold)
#define JS_DEFINE_CALLINFO_5(linkage, rt, op, at0, at1, at2, at3, at4, cse, fold)                 \
    _JS_DEFINE_CALLINFO(linkage, op, _JS_CTYPE(rt),                                               \
                        (_JS_CTYPE(at0), _JS_CTYPE(at1), _JS_CTYPE(at2), _JS_CTYPE(at3),          \
                         _JS_CTYPE(at4)),                                                         \
                        (_JS_ARGSIZE(at0) << 10) | (_JS_ARGSIZE(at1) << 8) |                      \
                        (_JS_ARGSIZE(at2) << 6) | (_JS_ARGSIZE(at3) << 4) |                       \
                        (_JS_ARGSIZE(at4) << 2) | _JS_RETSIZE(rt),                                \
                        cse, fold)

#define JS_DECLARE_CALLINFO(name)  extern const nanojit::CallInfo _JS_CALLINFO(name);

#else

#define JS_DEFINE_CALLINFO_1(linkage, rt, op, at0, cse, fold)
#define JS_DEFINE_CALLINFO_2(linkage, rt, op, at0, at1, cse, fold)
#define JS_DEFINE_CALLINFO_3(linkage, rt, op, at0, at1, at2, cse, fold)
#define JS_DEFINE_CALLINFO_4(linkage, rt, op, at0, at1, at2, at3, cse, fold)
#define JS_DEFINE_CALLINFO_5(linkage, rt, op, at0, at1, at2, at3, at4, cse, fold)
#define JS_DECLARE_CALLINFO(name)

#endif /* !JS_TRACER */

/* Defined in jsarray.cpp */
JS_DECLARE_CALLINFO(js_Array_dense_setelem)
JS_DECLARE_CALLINFO(js_FastNewArray)
JS_DECLARE_CALLINFO(js_Array_1int)
JS_DECLARE_CALLINFO(js_Array_1str)
JS_DECLARE_CALLINFO(js_Array_2obj)
JS_DECLARE_CALLINFO(js_Array_3num)

/* Defined in jsdate.cpp */
JS_DECLARE_CALLINFO(js_FastNewDate)

/* Defined in jsnum.cpp */
JS_DECLARE_CALLINFO(js_NumberToString)

/* Defined in jsstr.cpp */
JS_DECLARE_CALLINFO(js_ConcatStrings)
JS_DECLARE_CALLINFO(js_String_getelem)
JS_DECLARE_CALLINFO(js_String_p_charCodeAt)
JS_DECLARE_CALLINFO(js_EqualStrings)
JS_DECLARE_CALLINFO(js_CompareStrings)

/* Defined in jsbuiltins.cpp */
#define BUILTIN1(linkage, rt, op, at0,                     cse, fold)  JS_DECLARE_CALLINFO(op)
#define BUILTIN2(linkage, rt, op, at0, at1,                cse, fold)  JS_DECLARE_CALLINFO(op)
#define BUILTIN3(linkage, rt, op, at0, at1, at2,           cse, fold)  JS_DECLARE_CALLINFO(op)
#define BUILTIN4(linkage, rt, op, at0, at1, at2, at3,      cse, fold)  JS_DECLARE_CALLINFO(op)
#define BUILTIN5(linkage, rt, op, at0, at1, at2, at3, at4, cse, fold)  JS_DECLARE_CALLINFO(op)
#include "builtins.tbl"
#undef BUILTIN
#undef BUILTIN1
#undef BUILTIN2
#undef BUILTIN3
#undef BUILTIN4
#undef BUILTIN5

#endif /* jsbuiltins_h___ */

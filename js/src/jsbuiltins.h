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

// nanojit.h includes windows.h, so undo the obnoxious #defines, if needed
#include "nanojit/nanojit.h"
#include "jswin.h"

#include "jsvalue.h"

enum JSTNErrType { INFALLIBLE, FAIL_STATUS, FAIL_NULL, FAIL_NEG, FAIL_NEITHER };
enum { 
    JSTN_ERRTYPE_MASK        = 0x07,
    JSTN_UNBOX_AFTER         = 0x08,
    JSTN_MORE                = 0x10,
    JSTN_CONSTRUCTOR         = 0x20,
    JSTN_RETURN_NULLABLE_STR = 0x40,
    JSTN_RETURN_NULLABLE_OBJ = 0x80
};

#define JSTN_ERRTYPE(jstn)  ((jstn)->flags & JSTN_ERRTYPE_MASK)

/*
 * Type describing a type specialization of a js::Native.
 *
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
 * 'v': a value argument: on 32-bit, a Value*, on 64-bit, a jsval
 */
struct JSSpecializedNative {
    const nanojit::CallInfo *builtin;
    const char              *prefix;
    const char              *argtypes;
    uintN                   flags;  /* JSTNErrType | JSTN_UNBOX_AFTER | JSTN_MORE |
                                       JSTN_CONSTRUCTOR */
};

/*
 * Type holding extra trace-specific information about a fast native.
 *
 * 'specializations' points to a static array of available specializations
 * terminated by the lack of having the JSTN_MORE flag set.
 */
struct JSNativeTraceInfo {
    js::Native              native;
    JSSpecializedNative     *specializations;
};

/* Macros used by JS_DEFINE_CALLINFOn. */
#ifdef DEBUG
#define _JS_CI_NAME(op) ,#op
#else
#define _JS_CI_NAME(op)
#endif

#define _JS_I32_ARGTYPE    nanojit::ARGTYPE_I
#define _JS_I32_RETTYPE    nanojit::ARGTYPE_I
#define _JS_U64_ARGTYPE    nanojit::ARGTYPE_Q
#define _JS_U64_RETTYPE    nanojit::ARGTYPE_Q
#define _JS_F64_ARGTYPE    nanojit::ARGTYPE_D
#define _JS_F64_RETTYPE    nanojit::ARGTYPE_D
#define _JS_PTR_ARGTYPE    nanojit::ARGTYPE_P
#define _JS_PTR_RETTYPE    nanojit::ARGTYPE_P

struct ClosureVarInfo;

/*
 * Supported types for builtin functions.
 *
 * Types with -- for the two string fields are not permitted as argument types
 * in JS_DEFINE_TRCINFO.
 *
 * There are three kinds of traceable-native error handling.
 *
 *   - If a traceable native's return type ends with _FAIL, it always runs to
 *     completion.  It can either succeed or fail with an error or exception;
 *     on success, it may or may not stay on trace.  There may be side effects
 *     in any case.  If the call succeeds but bails off trace, we resume in the
 *     interpreter at the next opcode.
 *
 *     _FAIL builtins indicate failure or bailing off trace by setting bits in
 *     cx->interpState->builtinStatus.
 *
 *   - If a traceable native's return type contains _RETRY, it can either
 *     succeed, fail with a JS exception, or tell the caller to bail off trace
 *     and retry the call from the interpreter.  The last case happens if the
 *     builtin discovers that it can't do its job without examining the JS
 *     stack, reentering the interpreter, accessing properties of the global
 *     object, etc.
 *
 *     The builtin must detect the need to retry before committing any side
 *     effects.  If a builtin can't do this, it must use a _FAIL return type
 *     instead of _RETRY.
 *
 *     _RETRY builtins indicate failure with a special return value that
 *     depends on the return type:
 *
 *         BOOL_RETRY: JS_NEITHER
 *         INT32_RETRY: any negative value
 *         STRING_RETRY: NULL
 *         OBJECT_RETRY_NULL: NULL
 *
 *     _RETRY function calls are faster than _FAIL calls.  Each _RETRY call
 *     saves two writes to tm->bailExit and a read from state->builtinStatus.
 *
 *   - All other traceable natives are infallible (e.g. Date.now, Math.log).
 *
 * Special builtins known to the tracer can have their own idiosyncratic
 * error codes.
 *
 * When a traceable native returns a value indicating failure, we fall off
 * trace.  If an exception is pending, it is thrown; otherwise, we assume the
 * builtin had no side effects and retry the current bytecode in the
 * interpreter.
 *
 * So a builtin must not return a value indicating failure after causing side
 * effects (such as reporting an error), without setting an exception pending.
 * The operation would be retried, despite the first attempt's observable
 * effects.
 */
#define _JS_CTYPE(ctype, size, pch, ach, flags)     (ctype, size, pch, ach, flags)

#define _JS_CTYPE_CONTEXT           _JS_CTYPE(JSContext *,            _JS_PTR,"C", "", INFALLIBLE)
#define _JS_CTYPE_RUNTIME           _JS_CTYPE(JSRuntime *,            _JS_PTR,"R", "", INFALLIBLE)
#define _JS_CTYPE_MATHCACHE         _JS_CTYPE(js::MathCache *,        _JS_PTR,"M", "", INFALLIBLE)
#define _JS_CTYPE_THIS              _JS_CTYPE(JSObject *,             _JS_PTR,"T", "", INFALLIBLE)
#define _JS_CTYPE_THIS_DOUBLE       _JS_CTYPE(jsdouble,               _JS_F64,"D", "", INFALLIBLE)
#define _JS_CTYPE_THIS_STRING       _JS_CTYPE(JSString *,             _JS_PTR,"S", "", INFALLIBLE)
#define _JS_CTYPE_CALLEE            _JS_CTYPE(JSObject *,             _JS_PTR,"f", "", INFALLIBLE)
#define _JS_CTYPE_CALLEE_PROTOTYPE  _JS_CTYPE(JSObject *,             _JS_PTR,"p", "", INFALLIBLE)
#define _JS_CTYPE_FUNCTION          _JS_CTYPE(JSFunction *,           _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_PC                _JS_CTYPE(jsbytecode *,           _JS_PTR,"P", "", INFALLIBLE)
#define _JS_CTYPE_VALUEPTR          _JS_CTYPE(js::Value *,            _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_CVALUEPTR         _JS_CTYPE(const js::Value *,      _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_JSID              _JS_CTYPE(jsid,                   _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_JSVAL             _JS_CTYPE(js::Value,              _JS_U64, --, --, INFALLIBLE)
#define _JS_CTYPE_BOOL              _JS_CTYPE(JSBool,                 _JS_I32, "","i", INFALLIBLE)
#define _JS_CTYPE_BOOL_RETRY        _JS_CTYPE(JSBool,                 _JS_I32, --, --, FAIL_NEITHER)
#define _JS_CTYPE_BOOL_FAIL         _JS_CTYPE(JSBool,                 _JS_I32, --, --, FAIL_STATUS)
#define _JS_CTYPE_BOOLPTR           _JS_CTYPE(JSBool *,               _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_INT32             _JS_CTYPE(int32,                  _JS_I32, "","i", INFALLIBLE)
#define _JS_CTYPE_INT32_RETRY       _JS_CTYPE(int32,                  _JS_I32, --, --, FAIL_NEG)
#define _JS_CTYPE_INT32_FAIL        _JS_CTYPE(int32,                  _JS_I32, --, --, FAIL_STATUS)
#define _JS_CTYPE_INT32PTR          _JS_CTYPE(int32 *,                _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_UINT32            _JS_CTYPE(uint32,                 _JS_I32, "","i", INFALLIBLE)
#define _JS_CTYPE_UINT32_RETRY      _JS_CTYPE(uint32,                 _JS_I32, --, --, FAIL_NEG)
#define _JS_CTYPE_UINT32_FAIL       _JS_CTYPE(uint32,                 _JS_I32, --, --, FAIL_STATUS)
#define _JS_CTYPE_DOUBLE            _JS_CTYPE(jsdouble,               _JS_F64, "","d", INFALLIBLE)
#define _JS_CTYPE_DOUBLE_FAIL       _JS_CTYPE(jsdouble,               _JS_F64, --, --, FAIL_STATUS)
#define _JS_CTYPE_STRING            _JS_CTYPE(JSString *,             _JS_PTR, "","s", INFALLIBLE)
#define _JS_CTYPE_STRING_RETRY      _JS_CTYPE(JSString *,             _JS_PTR, --, --, FAIL_NULL)
#define _JS_CTYPE_STRING_FAIL       _JS_CTYPE(JSString *,             _JS_PTR, --, --, FAIL_STATUS)
#define _JS_CTYPE_STRING_OR_NULL_FAIL _JS_CTYPE(JSString *,           _JS_PTR, --, --, FAIL_STATUS | \
                                                                              JSTN_RETURN_NULLABLE_STR)
#define _JS_CTYPE_STRINGPTR         _JS_CTYPE(JSString **,            _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_OBJECT            _JS_CTYPE(JSObject *,             _JS_PTR, "","o", INFALLIBLE)
#define _JS_CTYPE_OBJECT_RETRY      _JS_CTYPE(JSObject *,             _JS_PTR, --, --, FAIL_NULL)
#define _JS_CTYPE_OBJECT_FAIL       _JS_CTYPE(JSObject *,             _JS_PTR, --, --, FAIL_STATUS)
#define _JS_CTYPE_OBJECT_OR_NULL_FAIL _JS_CTYPE(JSObject *,           _JS_PTR, --, --, FAIL_STATUS | \
                                                                              JSTN_RETURN_NULLABLE_OBJ)
#define _JS_CTYPE_OBJECTPTR         _JS_CTYPE(JSObject **,            _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_CONSTRUCTOR_RETRY _JS_CTYPE(JSObject *,             _JS_PTR, --, --, FAIL_NULL | \
                                                                                  JSTN_CONSTRUCTOR)
#define _JS_CTYPE_REGEXP            _JS_CTYPE(JSObject *,             _JS_PTR, "","r", INFALLIBLE)
#define _JS_CTYPE_SHAPE             _JS_CTYPE(js::Shape *,            _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_TRACERSTATE       _JS_CTYPE(TracerState *,          _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_FRAGMENT          _JS_CTYPE(nanojit::Fragment *,    _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_CLASS             _JS_CTYPE(js::Class *,            _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_DOUBLEPTR         _JS_CTYPE(double *,               _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_CHARPTR           _JS_CTYPE(char *,                 _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_CVIPTR            _JS_CTYPE(const ClosureVarInfo *, _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_FRAMEINFO         _JS_CTYPE(FrameInfo *,            _JS_PTR, --, --, INFALLIBLE)
#define _JS_CTYPE_UINTN             _JS_CTYPE(uintN,                  _JS_PTR, --, --, INFALLIBLE)
 
/*
 * The "VALUE" type is used to indicate that a native takes a js::Value
 * parameter by value. Unfortunately, for technical reasons, we can't simply
 * have the parameter type be js::Value. Furthermore, the right thing to pass
 * differs based on word size. Thus, a native that declares a parameter of type
 * VALUE should have the corresponding argument type be:
 *  - on 32-bit: const Value*
 *  - on 64-bit: jsval (which is a uint64)
 *
 * To write code that just does the right thing, use the pattern:
 *  void foo(js::ValueArgType arg) {
 *    const js::Value &v = js::ValueArgToConstRef(arg);
 */
#if JS_BITS_PER_WORD == 32
# define _JS_CTYPE_VALUE            _JS_CTYPE(js::ValueArgType,       _JS_PTR, "","v", INFALLIBLE)
#elif JS_BITS_PER_WORD == 64
# define _JS_CTYPE_VALUE            _JS_CTYPE(js::ValueArgType,       _JS_U64, "","v", INFALLIBLE)
#endif

#define _JS_EXPAND(tokens)  tokens

#define _JS_CTYPE_TYPE2(t,s,p,a,f)      t
#define _JS_CTYPE_TYPE(tyname)          _JS_EXPAND(_JS_CTYPE_TYPE2    _JS_CTYPE_##tyname)
#define _JS_CTYPE_RETTYPE2(t,s,p,a,f)   s##_RETTYPE
#define _JS_CTYPE_RETTYPE(tyname)       _JS_EXPAND(_JS_CTYPE_RETTYPE2 _JS_CTYPE_##tyname)
#define _JS_CTYPE_ARGTYPE2(t,s,p,a,f)   s##_ARGTYPE
#define _JS_CTYPE_ARGTYPE(tyname)       _JS_EXPAND(_JS_CTYPE_ARGTYPE2 _JS_CTYPE_##tyname)
#define _JS_CTYPE_PCH2(t,s,p,a,f)       p
#define _JS_CTYPE_PCH(tyname)           _JS_EXPAND(_JS_CTYPE_PCH2     _JS_CTYPE_##tyname)
#define _JS_CTYPE_ACH2(t,s,p,a,f)       a
#define _JS_CTYPE_ACH(tyname)           _JS_EXPAND(_JS_CTYPE_ACH2     _JS_CTYPE_##tyname)
#define _JS_CTYPE_FLAGS2(t,s,p,a,f)     f
#define _JS_CTYPE_FLAGS(tyname)         _JS_EXPAND(_JS_CTYPE_FLAGS2   _JS_CTYPE_##tyname)

#define _JS_static_TN(t)  static t
#define _JS_static_CI     static
#define _JS_extern_TN(t)  extern t
#define _JS_extern_CI
#define _JS_FRIEND_TN(t)  extern JS_FRIEND_API(t)
#define _JS_FRIEND_CI
#define _JS_TN_LINKAGE(linkage, t)  _JS_##linkage##_TN(t)
#define _JS_CI_LINKAGE(linkage)     _JS_##linkage##_CI

#define _JS_CALLINFO(name) name##_ci

#if defined(JS_NO_FASTCALL) && defined(NANOJIT_IA32)
#define _JS_DEFINE_CALLINFO(linkage, name, crtype, cargtypes, argtypes, isPure, storeAccSet)      \
    _JS_TN_LINKAGE(linkage, crtype) name cargtypes;                                               \
    _JS_CI_LINKAGE(linkage) const nanojit::CallInfo _JS_CALLINFO(name) =                          \
        { (intptr_t) &name, argtypes, nanojit::ABI_CDECL, isPure, storeAccSet _JS_CI_NAME(name) };\
    JS_STATIC_ASSERT_IF(isPure, (storeAccSet) == nanojit::ACCSET_NONE);

#else
#define _JS_DEFINE_CALLINFO(linkage, name, crtype, cargtypes, argtypes, isPure, storeAccSet)      \
    _JS_TN_LINKAGE(linkage, crtype) FASTCALL name cargtypes;                                      \
    _JS_CI_LINKAGE(linkage) const nanojit::CallInfo _JS_CALLINFO(name) =                          \
        { (uintptr_t) &name, argtypes, nanojit::ABI_FASTCALL, isPure, storeAccSet _JS_CI_NAME(name) }; \
    JS_STATIC_ASSERT_IF(isPure, (storeAccSet) == nanojit::ACCSET_NONE);
#endif

/*
 * This macro is used for builtin functions that can be called from JITted
 * code.  It declares a C function named <op> and a CallInfo struct named
 * <op>_ci so the tracer can call it.  The <N> in JS_DEFINE_CALLINFO_<N> is
 * the number of arguments the builtin takes.  Builtins with no arguments
 * are not supported.  Using a macro is clunky but ensures that the types
 * for each C function matches those for the corresponding CallInfo struct;
 * mismatched types can cause subtle problems.
 *
 * The macro arguments are:
 *
 * - The linkage for the function and the associated CallInfo global.  It
 *   can be extern, static, or FRIEND, which specifies JS_FRIEND_API linkage
 *   for the function.
 *
 * - The return type. This identifier must name one of the _JS_TYPEINFO_*
 *   macros defined in jsbuiltins.h.
 *
 * - The builtin name.
 *
 * - The parameter types.
 *
 * - The isPure flag.  Set to 1 if:
 *   (a) the function's return value is determined solely by its arguments
 *       (ie. no hidden state, no implicit inputs used such as global
 *       variables or the result of an I/O operation); and
 *   (b) the function causes no observable side-effects (ie. no writes to
 *       global variables, no I/O output).
 *   Multiple calls to a pure function can be merged during CSE.
 *
 * - The storeAccSet.  This indicates which memory access regions the function
 *   accesses.  It must be ACCSET_NONE if the function is pure;  use
 *   ACCSET_STORE_ANY if you're not sure.  Used to determine if each call site
 *   of the function aliases any loads.
 */
#define JS_DEFINE_CALLINFO_1(linkage, rt, op, at0, isPure, storeAccSet)                           \
    _JS_DEFINE_CALLINFO(linkage, op,                                                              \
                        _JS_CTYPE_TYPE(rt),                                                       \
                        (_JS_CTYPE_TYPE(at0)),                                                    \
                        nanojit::CallInfo::typeSig1(_JS_CTYPE_RETTYPE(rt),                        \
                                                    _JS_CTYPE_ARGTYPE(at0)),                      \
                        isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_2(linkage, rt, op, at0, at1, isPure, storeAccSet)                      \
    _JS_DEFINE_CALLINFO(linkage, op,                                                              \
                        _JS_CTYPE_TYPE(rt),                                                       \
                        (_JS_CTYPE_TYPE(at0),                                                     \
                         _JS_CTYPE_TYPE(at1)),                                                    \
                        nanojit::CallInfo::typeSig2(_JS_CTYPE_RETTYPE(rt),                        \
                                                    _JS_CTYPE_ARGTYPE(at0),                       \
                                                    _JS_CTYPE_ARGTYPE(at1)),                      \
                        isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_3(linkage, rt, op, at0, at1, at2, isPure, storeAccSet)                 \
    _JS_DEFINE_CALLINFO(linkage, op,                                                              \
                        _JS_CTYPE_TYPE(rt),                                                       \
                        (_JS_CTYPE_TYPE(at0),                                                     \
                         _JS_CTYPE_TYPE(at1),                                                     \
                         _JS_CTYPE_TYPE(at2)),                                                    \
                        nanojit::CallInfo::typeSig3(_JS_CTYPE_RETTYPE(rt),                        \
                                                    _JS_CTYPE_ARGTYPE(at0),                       \
                                                    _JS_CTYPE_ARGTYPE(at1),                       \
                                                    _JS_CTYPE_ARGTYPE(at2)),                      \
                        isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_4(linkage, rt, op, at0, at1, at2, at3, isPure, storeAccSet)            \
    _JS_DEFINE_CALLINFO(linkage, op,                                                              \
                        _JS_CTYPE_TYPE(rt),                                                       \
                        (_JS_CTYPE_TYPE(at0),                                                     \
                         _JS_CTYPE_TYPE(at1),                                                     \
                         _JS_CTYPE_TYPE(at2),                                                     \
                         _JS_CTYPE_TYPE(at3)),                                                    \
                        nanojit::CallInfo::typeSig4(_JS_CTYPE_RETTYPE(rt),                        \
                                                    _JS_CTYPE_ARGTYPE(at0),                       \
                                                    _JS_CTYPE_ARGTYPE(at1),                       \
                                                    _JS_CTYPE_ARGTYPE(at2),                       \
                                                    _JS_CTYPE_ARGTYPE(at3)),                      \
                        isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_5(linkage, rt, op, at0, at1, at2, at3, at4, isPure, storeAccSet)       \
    _JS_DEFINE_CALLINFO(linkage, op,                                                              \
                        _JS_CTYPE_TYPE(rt),                                                       \
                        (_JS_CTYPE_TYPE(at0),                                                     \
                         _JS_CTYPE_TYPE(at1),                                                     \
                         _JS_CTYPE_TYPE(at2),                                                     \
                         _JS_CTYPE_TYPE(at3),                                                     \
                         _JS_CTYPE_TYPE(at4)),                                                    \
                        nanojit::CallInfo::typeSig5(_JS_CTYPE_RETTYPE(rt),                        \
                                                    _JS_CTYPE_ARGTYPE(at0),                       \
                                                    _JS_CTYPE_ARGTYPE(at1),                       \
                                                    _JS_CTYPE_ARGTYPE(at2),                       \
                                                    _JS_CTYPE_ARGTYPE(at3),                       \
                                                    _JS_CTYPE_ARGTYPE(at4)),                      \
                        isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_6(linkage, rt, op, at0, at1, at2, at3, at4, at5, isPure, storeAccSet)  \
    _JS_DEFINE_CALLINFO(linkage, op,                                                              \
                        _JS_CTYPE_TYPE(rt),                                                       \
                        (_JS_CTYPE_TYPE(at0),                                                     \
                         _JS_CTYPE_TYPE(at1),                                                     \
                         _JS_CTYPE_TYPE(at2),                                                     \
                         _JS_CTYPE_TYPE(at3),                                                     \
                         _JS_CTYPE_TYPE(at4),                                                     \
                         _JS_CTYPE_TYPE(at5)),                                                    \
                        nanojit::CallInfo::typeSig6(_JS_CTYPE_RETTYPE(rt),                        \
                                                    _JS_CTYPE_ARGTYPE(at0),                       \
                                                    _JS_CTYPE_ARGTYPE(at1),                       \
                                                    _JS_CTYPE_ARGTYPE(at2),                       \
                                                    _JS_CTYPE_ARGTYPE(at3),                       \
                                                    _JS_CTYPE_ARGTYPE(at4),                       \
                                                    _JS_CTYPE_ARGTYPE(at5)),                      \
                        isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_7(linkage, rt, op, at0, at1, at2, at3, at4, at5, at6, isPure,          \
                             storeAccSet)                                                         \
    _JS_DEFINE_CALLINFO(linkage, op,                                                              \
                        _JS_CTYPE_TYPE(rt),                                                       \
                        (_JS_CTYPE_TYPE(at0),                                                     \
                         _JS_CTYPE_TYPE(at1),                                                     \
                         _JS_CTYPE_TYPE(at2),                                                     \
                         _JS_CTYPE_TYPE(at3),                                                     \
                         _JS_CTYPE_TYPE(at4),                                                     \
                         _JS_CTYPE_TYPE(at5),                                                     \
                         _JS_CTYPE_TYPE(at6)),                                                    \
                        nanojit::CallInfo::typeSig7(_JS_CTYPE_RETTYPE(rt),                        \
                                                    _JS_CTYPE_ARGTYPE(at0),                       \
                                                    _JS_CTYPE_ARGTYPE(at1),                       \
                                                    _JS_CTYPE_ARGTYPE(at2),                       \
                                                    _JS_CTYPE_ARGTYPE(at3),                       \
                                                    _JS_CTYPE_ARGTYPE(at4),                       \
                                                    _JS_CTYPE_ARGTYPE(at5),                       \
                                                    _JS_CTYPE_ARGTYPE(at6)),                      \
                        isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_8(linkage, rt, op, at0, at1, at2, at3, at4, at5, at6, at7, isPure,     \
                             storeAccSet)                                                         \
    _JS_DEFINE_CALLINFO(linkage, op,                                                              \
                        _JS_CTYPE_TYPE(rt),                                                       \
                        (_JS_CTYPE_TYPE(at0),                                                     \
                         _JS_CTYPE_TYPE(at1),                                                     \
                         _JS_CTYPE_TYPE(at2),                                                     \
                         _JS_CTYPE_TYPE(at3),                                                     \
                         _JS_CTYPE_TYPE(at4),                                                     \
                         _JS_CTYPE_TYPE(at5),                                                     \
                         _JS_CTYPE_TYPE(at6),                                                     \
                         _JS_CTYPE_TYPE(at7)),                                                    \
                        nanojit::CallInfo::typeSig8(_JS_CTYPE_RETTYPE(rt),                        \
                                                    _JS_CTYPE_ARGTYPE(at0),                       \
                                                    _JS_CTYPE_ARGTYPE(at1),                       \
                                                    _JS_CTYPE_ARGTYPE(at2),                       \
                                                    _JS_CTYPE_ARGTYPE(at3),                       \
                                                    _JS_CTYPE_ARGTYPE(at4),                       \
                                                    _JS_CTYPE_ARGTYPE(at5),                       \
                                                    _JS_CTYPE_ARGTYPE(at6),                       \
                                                    _JS_CTYPE_ARGTYPE(at7)),                      \
                        isPure, storeAccSet)

#define JS_DECLARE_CALLINFO(name)  extern const nanojit::CallInfo _JS_CALLINFO(name);

#define _JS_TN_INIT_HELPER_n(n, args)  _JS_TN_INIT_HELPER_##n args

#define _JS_TN_INIT_HELPER_1(linkage, rt, op, at0, isPure, storeAccSet)                           \
    &_JS_CALLINFO(op),                                                                            \
    _JS_CTYPE_PCH(at0),                                                                           \
    _JS_CTYPE_ACH(at0),                                                                           \
    _JS_CTYPE_FLAGS(rt)

#define _JS_TN_INIT_HELPER_2(linkage, rt, op, at0, at1, isPure, storeAccSet)                      \
    &_JS_CALLINFO(op),                                                                            \
    _JS_CTYPE_PCH(at1) _JS_CTYPE_PCH(at0),                                                        \
    _JS_CTYPE_ACH(at1) _JS_CTYPE_ACH(at0),                                                        \
    _JS_CTYPE_FLAGS(rt)

#define _JS_TN_INIT_HELPER_3(linkage, rt, op, at0, at1, at2, isPure, storeAccSet)                 \
    &_JS_CALLINFO(op),                                                                            \
    _JS_CTYPE_PCH(at2) _JS_CTYPE_PCH(at1) _JS_CTYPE_PCH(at0),                                     \
    _JS_CTYPE_ACH(at2) _JS_CTYPE_ACH(at1) _JS_CTYPE_ACH(at0),                                     \
    _JS_CTYPE_FLAGS(rt)

#define _JS_TN_INIT_HELPER_4(linkage, rt, op, at0, at1, at2, at3, isPure, storeAccSet)            \
    &_JS_CALLINFO(op),                                                                            \
    _JS_CTYPE_PCH(at3) _JS_CTYPE_PCH(at2) _JS_CTYPE_PCH(at1) _JS_CTYPE_PCH(at0),                  \
    _JS_CTYPE_ACH(at3) _JS_CTYPE_ACH(at2) _JS_CTYPE_ACH(at1) _JS_CTYPE_ACH(at0),                  \
    _JS_CTYPE_FLAGS(rt)

#define _JS_TN_INIT_HELPER_5(linkage, rt, op, at0, at1, at2, at3, at4, isPure, storeAccSet)       \
    &_JS_CALLINFO(op),                                                                            \
    _JS_CTYPE_PCH(at4) _JS_CTYPE_PCH(at3) _JS_CTYPE_PCH(at2) _JS_CTYPE_PCH(at1)                   \
        _JS_CTYPE_PCH(at0),                                                                       \
    _JS_CTYPE_ACH(at4) _JS_CTYPE_ACH(at3) _JS_CTYPE_ACH(at2) _JS_CTYPE_ACH(at1)                   \
        _JS_CTYPE_ACH(at0),                                                                       \
    _JS_CTYPE_FLAGS(rt)

#define _JS_TN_INIT_HELPER_6(linkage, rt, op, at0, at1, at2, at3, at4, at5, isPure, storeAccSet) \
    &_JS_CALLINFO(op),                                                                            \
    _JS_CTYPE_PCH(at5) _JS_CTYPE_PCH(at4) _JS_CTYPE_PCH(at3) _JS_CTYPE_PCH(at2)                   \
        _JS_CTYPE_PCH(at1) _JS_CTYPE_PCH(at0),                                                    \
    _JS_CTYPE_ACH(at5) _JS_CTYPE_ACH(at4) _JS_CTYPE_ACH(at3) _JS_CTYPE_ACH(at2)                   \
        _JS_CTYPE_ACH(at1) _JS_CTYPE_ACH(at0),                                                    \
    _JS_CTYPE_FLAGS(rt)

#define _JS_TN_INIT_HELPER_7(linkage, rt, op, at0, at1, at2, at3, at4, at5, at6, isPure, storeAccSet) \
    &_JS_CALLINFO(op),                                                                            \
    _JS_CTYPE_PCH(at6) _JS_CTYPE_PCH(at5) _JS_CTYPE_PCH(at4) _JS_CTYPE_PCH(at3)                   \
        _JS_CTYPE_PCH(at2)  _JS_CTYPE_PCH(at1) _JS_CTYPE_PCH(at0),                                \
    _JS_CTYPE_ACH(at6) _JS_CTYPE_ACH(at5) _JS_CTYPE_ACH(at4) _JS_CTYPE_ACH(at3)                   \
        _JS_CTYPE_ACH(at2) _JS_CTYPE_ACH(at1) _JS_CTYPE_ACH(at0),                                 \
    _JS_CTYPE_FLAGS(rt)

#define _JS_TN_INIT_HELPER_8(linkage, rt, op, at0, at1, at2, at3, at4, at5, at6, at7, isPure, storeAccSet) \
    &_JS_CALLINFO(op),                                                                            \
    _JS_CTYPE_PCH(at7) _JS_CTYPE_PCH(at6) _JS_CTYPE_PCH(at5) _JS_CTYPE_PCH(at4)                   \
        _JS_CTYPE_PCH(at3) _JS_CTYPE_PCH(at2)  _JS_CTYPE_PCH(at1) _JS_CTYPE_PCH(at0),             \
    _JS_CTYPE_ACH(at7) _JS_CTYPE_ACH(at6) _JS_CTYPE_ACH(at5) _JS_CTYPE_ACH(at4)                   \
        _JS_CTYPE_ACH(at3) _JS_CTYPE_ACH(at2) _JS_CTYPE_ACH(at1) _JS_CTYPE_ACH(at0),              \
    _JS_CTYPE_FLAGS(rt)

#define JS_DEFINE_TRCINFO_1(name, tn0)                                                            \
    _JS_DEFINE_CALLINFO_n tn0                                                                     \
    JSSpecializedNative name##_sns[] = {                                                          \
        { _JS_TN_INIT_HELPER_n tn0 }                                                              \
    };                                                                                            \
    JSNativeTraceInfo name##_trcinfo = { JS_VALUEIFY_NATIVE(name), name##_sns };

#define JS_DEFINE_TRCINFO_2(name, tn0, tn1)                                                       \
    _JS_DEFINE_CALLINFO_n tn0                                                                     \
    _JS_DEFINE_CALLINFO_n tn1                                                                     \
    JSSpecializedNative name##_sns[] = {                                                          \
        { _JS_TN_INIT_HELPER_n tn0 | JSTN_MORE },                                                 \
        { _JS_TN_INIT_HELPER_n tn1 }                                                              \
    };                                                                                            \
    JSNativeTraceInfo name##_trcinfo = { JS_VALUEIFY_NATIVE(name), name##_sns };

#define JS_DEFINE_TRCINFO_3(name, tn0, tn1, tn2)                                                  \
    _JS_DEFINE_CALLINFO_n tn0                                                                     \
    _JS_DEFINE_CALLINFO_n tn1                                                                     \
    _JS_DEFINE_CALLINFO_n tn2                                                                     \
    JSSpecializedNative name##_sns[] = {                                                          \
        { _JS_TN_INIT_HELPER_n tn0 | JSTN_MORE },                                                 \
        { _JS_TN_INIT_HELPER_n tn1 | JSTN_MORE },                                                 \
        { _JS_TN_INIT_HELPER_n tn2 }                                                              \
    };                                                                                            \
    JSNativeTraceInfo name##_trcinfo = { JS_VALUEIFY_NATIVE(name), name##_sns };

#define JS_DEFINE_TRCINFO_4(name, tn0, tn1, tn2, tn3)                                             \
    _JS_DEFINE_CALLINFO_n tn0                                                                     \
    _JS_DEFINE_CALLINFO_n tn1                                                                     \
    _JS_DEFINE_CALLINFO_n tn2                                                                     \
    _JS_DEFINE_CALLINFO_n tn3                                                                     \
    JSSpecializedNative name##_sns[] = {                                                          \
        { _JS_TN_INIT_HELPER_n tn0 | JSTN_MORE },                                                 \
        { _JS_TN_INIT_HELPER_n tn1 | JSTN_MORE },                                                 \
        { _JS_TN_INIT_HELPER_n tn2 | JSTN_MORE },                                                 \
        { _JS_TN_INIT_HELPER_n tn3 }                                                              \
    };                                                                                            \
    JSNativeTraceInfo name##_trcinfo = { JS_VALUEIFY_NATIVE(name), name##_sns };

#define _JS_DEFINE_CALLINFO_n(n, args)  JS_DEFINE_CALLINFO_##n args

jsdouble FASTCALL
js_StringToNumber(JSContext* cx, JSString* str, JSBool *ok);

/* Extern version of SetBuiltinError. */
extern JS_FRIEND_API(void)
js_SetTraceableNativeFailed(JSContext *cx);

extern jsdouble FASTCALL
js_dmod(jsdouble a, jsdouble b);

#else

#define JS_DEFINE_CALLINFO_1(linkage, rt, op, at0, isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_2(linkage, rt, op, at0, at1, isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_3(linkage, rt, op, at0, at1, at2, isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_4(linkage, rt, op, at0, at1, at2, at3, isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_5(linkage, rt, op, at0, at1, at2, at3, at4, isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_6(linkage, rt, op, at0, at1, at2, at3, at4, at5, isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_7(linkage, rt, op, at0, at1, at2, at3, at4, at5, at6, isPure, storeAccSet)
#define JS_DEFINE_CALLINFO_8(linkage, rt, op, at0, at1, at2, at3, at4, at5, at6, at7, isPure, storeAccSet)
#define JS_DECLARE_CALLINFO(name)
#define JS_DEFINE_TRCINFO_1(name, tn0)
#define JS_DEFINE_TRCINFO_2(name, tn0, tn1)
#define JS_DEFINE_TRCINFO_3(name, tn0, tn1, tn2)
#define JS_DEFINE_TRCINFO_4(name, tn0, tn1, tn2, tn3)

#endif /* !JS_TRACER */

/* Defined in jsarray.cpp. */
namespace js {
JS_DECLARE_CALLINFO(NewDenseEmptyArray)
JS_DECLARE_CALLINFO(NewDenseAllocatedArray)
JS_DECLARE_CALLINFO(NewDenseUnallocatedArray)
}
JS_DECLARE_CALLINFO(js_NewbornArrayPush_tn)
JS_DECLARE_CALLINFO(js_EnsureDenseArrayCapacity)

/* Defined in jsbuiltins.cpp. */
JS_DECLARE_CALLINFO(js_UnboxNumberAsDouble)
JS_DECLARE_CALLINFO(js_UnboxNumberAsInt32)
JS_DECLARE_CALLINFO(js_dmod)
JS_DECLARE_CALLINFO(js_imod)
JS_DECLARE_CALLINFO(js_DoubleToInt32)
JS_DECLARE_CALLINFO(js_DoubleToUint32)
JS_DECLARE_CALLINFO(js_StringToNumber)
JS_DECLARE_CALLINFO(js_StringToInt32)
JS_DECLARE_CALLINFO(js_AddProperty)
JS_DECLARE_CALLINFO(js_AddAtomProperty)
JS_DECLARE_CALLINFO(js_HasNamedProperty)
JS_DECLARE_CALLINFO(js_HasNamedPropertyInt32)
JS_DECLARE_CALLINFO(js_TypeOfObject)
JS_DECLARE_CALLINFO(js_BooleanIntToString)
JS_DECLARE_CALLINFO(js_NewNullClosure)

/* Defined in jsfun.cpp. */
JS_DECLARE_CALLINFO(js_AllocFlatClosure)
JS_DECLARE_CALLINFO(js_PutArgumentsOnTrace)
JS_DECLARE_CALLINFO(js_PutCallObjectOnTrace)
JS_DECLARE_CALLINFO(js_SetCallVar)
JS_DECLARE_CALLINFO(js_SetCallArg)
JS_DECLARE_CALLINFO(js_CloneFunctionObject)
JS_DECLARE_CALLINFO(js_CreateCallObjectOnTrace)
JS_DECLARE_CALLINFO(js_NewArgumentsOnTrace)

/* Defined in jsnum.cpp. */
JS_DECLARE_CALLINFO(js_NumberToString)

/* Defined in jsobj.cpp. */
JS_DECLARE_CALLINFO(js_Object_tn)
JS_DECLARE_CALLINFO(js_CreateThisFromTrace)
JS_DECLARE_CALLINFO(js_InitializerObject)

/* Defined in jsregexp.cpp. */
JS_DECLARE_CALLINFO(js_CloneRegExpObject)

/* Defined in jsstr.cpp. */
JS_DECLARE_CALLINFO(js_String_tn)
JS_DECLARE_CALLINFO(js_CompareStringsOnTrace)
JS_DECLARE_CALLINFO(js_ConcatStrings)
JS_DECLARE_CALLINFO(js_EqualStringsOnTrace)
JS_DECLARE_CALLINFO(js_FlattenOnTrace)

/* Defined in jstypedarray.cpp. */
JS_DECLARE_CALLINFO(js_TypedArray_uint8_clamp_double)

#endif /* jsbuiltins_h___ */

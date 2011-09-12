/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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

#ifndef jsfun_h___
#define jsfun_h___
/*
 * JS function definitions.
 */
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsobj.h"
#include "jsatom.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsopcode.h"

/*
 * The high two bits of JSFunction.flags encode whether the function is native
 * or interpreted, and if interpreted, what kind of optimized closure form (if
 * any) it might be.
 *
 *   00   not interpreted
 *   01   interpreted, neither flat nor null closure
 *   10   interpreted, flat closure
 *   11   interpreted, null closure
 *
 * isFlatClosure() implies isInterpreted() and u.i.script->upvarsOffset != 0.
 * isNullClosure() implies isInterpreted() and u.i.script->upvarsOffset == 0.
 *
 * isInterpreted() but not isFlatClosure() and u.i.script->upvarsOffset != 0
 * is an Algol-like function expression or nested function, i.e., a function
 * that never escapes upward or downward (heapward), and is only ever called.
 *
 * Finally, isInterpreted() and u.i.script->upvarsOffset == 0 could be either
 * a non-closure (a global function definition, or any function that uses no
 * outer names), or a closure of an escaping function that uses outer names
 * whose values can't be snapshot (because the outer names could be reassigned
 * after the closure is formed, or because assignments could not be analyzed
 * due to with or eval).
 *
 * Such a hard-case function must use JSOP_NAME, etc., and reify outer function
 * activations' call objects, etc. if it's not a global function.
 *
 * NB: JSFUN_EXPR_CLOSURE reuses JSFUN_STUB_GSOPS, which is an API request flag
 * bit only, never stored in fun->flags.
 *
 * If we need more bits in the future, all flags for interpreted functions can
 * move to u.i.script->flags. For now we use function flag bits to minimize
 * pointer-chasing.
 */
#define JSFUN_JOINABLE      0x0001  /* function is null closure that does not
                                       appear to call itself via its own name
                                       or arguments.callee */

#define JSFUN_PROTOTYPE     0x0800  /* function is Function.prototype for some
                                       global object */

#define JSFUN_EXPR_CLOSURE  0x1000  /* expression closure: function(x) x*x */
#define JSFUN_TRCINFO       0x2000  /* when set, u.n.trcinfo is non-null,
                                       JSFunctionSpec::call points to a
                                       JSNativeTraceInfo. */
#define JSFUN_INTERPRETED   0x4000  /* use u.i if kind >= this value else u.n */
#define JSFUN_FLAT_CLOSURE  0x8000  /* flat (aka "display") closure */
#define JSFUN_NULL_CLOSURE  0xc000  /* null closure entrains no scope chain */
#define JSFUN_KINDMASK      0xc000  /* encode interp vs. native and closure
                                       optimization level -- see above */

struct JSFunction : public JSObject_Slots2
{
    /* Functions always have two fixed slots (FUN_CLASS_RESERVED_SLOTS). */

    uint16          nargs;        /* maximum number of specified arguments,
                                     reflected as f.length/f.arity */
    uint16          flags;        /* flags, see JSFUN_* below and in jsapi.h */
    union U {
        struct {
            js::Native  native;   /* native method pointer or null */
            js::Class   *clasp;   /* class of objects constructed
                                     by this function */
            JSNativeTraceInfo *trcinfo;
        } n;
        struct Scripted {
            JSScript    *script;  /* interpreted bytecode descriptor or null */
            uint16       skipmin; /* net skip amount up (toward zero) from
                                     script->staticLevel to nearest upvar,
                                     including upvars in nested functions */
            js::Shape   *names;   /* argument and variable names */
        } i;
        void            *nativeOrScript;
    } u;
    JSAtom          *atom;        /* name for diagnostics and decompiling */

    bool optimizedClosure()  const { return kind() > JSFUN_INTERPRETED; }
    bool isInterpreted()     const { return kind() >= JSFUN_INTERPRETED; }
    bool isNative()          const { return !isInterpreted(); }
    bool isConstructor()     const { return flags & JSFUN_CONSTRUCTOR; }
    bool isHeavyweight()     const { return JSFUN_HEAVYWEIGHT_TEST(flags); }
    bool isNullClosure()     const { return kind() == JSFUN_NULL_CLOSURE; }
    bool isFlatClosure()     const { return kind() == JSFUN_FLAT_CLOSURE; }
    bool isFunctionPrototype() const { return flags & JSFUN_PROTOTYPE; }
    bool isInterpretedConstructor() const { return isInterpreted() && !isFunctionPrototype(); }

    uint16 kind()            const { return flags & JSFUN_KINDMASK; }
    void setKind(uint16 k) {
        JS_ASSERT(!(k & ~JSFUN_KINDMASK));
        flags = (flags & ~JSFUN_KINDMASK) | k;
    }

    /* Returns the strictness of this function, which must be interpreted. */
    inline bool inStrictMode() const;

    void setArgCount(uint16 nargs) {
        JS_ASSERT(this->nargs == 0);
        this->nargs = nargs;
    }

    /* uint16 representation bounds number of call object dynamic slots. */
    enum { MAX_ARGS_AND_VARS = 2 * ((1U << 16) - 1) };

#define JS_LOCAL_NAME_TO_ATOM(nameWord)  ((JSAtom *) ((nameWord) & ~(jsuword) 1))
#define JS_LOCAL_NAME_IS_CONST(nameWord) ((((nameWord) & (jsuword) 1)) != 0)

    bool mightEscape() const {
        return isInterpreted() && (isFlatClosure() || !script()->bindings.hasUpvars());
    }

    bool joinable() const {
        return flags & JSFUN_JOINABLE;
    }

    JSObject &compiledFunObj() {
        return *this;
    }

  private:
    /*
     * FunctionClass reserves two slots, which are free in JSObject::fslots
     * without requiring dslots allocation. Null closures that can be joined to
     * a compiler-created function object use the first one to hold a mutable
     * methodAtom() state variable, needed for correct foo.caller handling.
     */
    enum {
        METHOD_ATOM_SLOT  = JSSLOT_FUN_METHOD_ATOM
    };

  public:
    inline void setJoinable();

    /*
     * Method name imputed from property uniquely assigned to or initialized,
     * where the function does not need to be cloned to carry a scope chain or
     * flattened upvars.
     */
    JSAtom *methodAtom() const {
        return (joinable() && getSlot(METHOD_ATOM_SLOT).isString())
               ? &getSlot(METHOD_ATOM_SLOT).toString()->asAtom()
               : NULL;
    }

    inline void setMethodAtom(JSAtom *atom);

    JSScript *script() const {
        JS_ASSERT(isInterpreted());
        return u.i.script;
    }

    JSScript * maybeScript() const {
        return isInterpreted() ? script() : NULL;
    }

    js::Native native() const {
        JS_ASSERT(isNative());
        return u.n.native;
    }

    js::Native maybeNative() const {
        return isInterpreted() ? NULL : native();
    }

    static uintN offsetOfNativeOrScript() {
        JS_STATIC_ASSERT(offsetof(U, n.native) == offsetof(U, i.script));
        JS_STATIC_ASSERT(offsetof(U, n.native) == offsetof(U, nativeOrScript));
        return offsetof(JSFunction, u.nativeOrScript);
    }

    /* Number of extra fixed function object slots. */
    static const uint32 CLASS_RESERVED_SLOTS = JSObject::FUN_CLASS_RESERVED_SLOTS;


    js::Class *getConstructorClass() const {
        JS_ASSERT(isNative());
        return u.n.clasp;
    }

    void setConstructorClass(js::Class *clasp) {
        JS_ASSERT(isNative());
        u.n.clasp = clasp;
    }

    JSNativeTraceInfo *getTraceInfo() const {
        JS_ASSERT(isNative());
        JS_ASSERT(flags & JSFUN_TRCINFO);
        return u.n.trcinfo;
    }
};

/*
 * Trace-annotated native. This expands to a JSFunctionSpec initializer (like
 * JS_FN in jsapi.h). fastcall is a FastNative; trcinfo is a
 * JSNativeTraceInfo*.
 */
#ifdef JS_TRACER
/* MSVC demands the intermediate (void *) cast here. */
# define JS_TN(name,fastcall,nargs,flags,trcinfo)                             \
    JS_FN(name, JS_DATA_TO_FUNC_PTR(Native, trcinfo), nargs,                  \
          (flags) | JSFUN_STUB_GSOPS | JSFUN_TRCINFO)
#else
# define JS_TN(name,fastcall,nargs,flags,trcinfo)                             \
    JS_FN(name, fastcall, nargs, flags)
#endif

inline JSFunction *
JSObject::getFunctionPrivate() const
{
    JS_ASSERT(isFunction());
    return reinterpret_cast<JSFunction *>(getPrivate());
}

namespace js {

static JS_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v)
{
    return v.isObject() && v.toObject().isFunction();
}

static JS_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v, JSObject **funobj)
{
    return v.isObject() && (*funobj = &v.toObject())->isFunction();
}

static JS_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v, JSObject **funobj, JSFunction **fun)
{
    bool b = IsFunctionObject(v, funobj);
    if (b)
        *fun = (*funobj)->getFunctionPrivate();
    return b;
}

static JS_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v, JSFunction **fun)
{
    JSObject *funobj;
    return IsFunctionObject(v, &funobj, fun);
}

static JS_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v)
{
    JSFunction *fun;
    return IsFunctionObject(v, &fun) && fun->isNative();
}

static JS_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v, JSFunction **fun)
{
    return IsFunctionObject(v, fun) && (*fun)->isNative();
}

static JS_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v, Native native)
{
    JSFunction *fun;
    return IsFunctionObject(v, &fun) && fun->maybeNative() == native;
}

/*
 * When we have an object of a builtin class, we don't quite know what its
 * valueOf/toString methods are, since these methods may have been overwritten
 * or shadowed. However, we can still do better than the general case by
 * hard-coding the necessary properties for us to find the native we expect.
 *
 * TODO: a per-thread shape-based cache would be faster and simpler.
 */
static JS_ALWAYS_INLINE bool
ClassMethodIsNative(JSContext *cx, JSObject *obj, Class *clasp, jsid methodid, Native native)
{
    JS_ASSERT(obj->getClass() == clasp);

    Value v;
    if (!HasDataProperty(cx, obj, methodid, &v)) {
        JSObject *proto = obj->getProto();
        if (!proto || proto->getClass() != clasp || !HasDataProperty(cx, proto, methodid, &v))
            return false;
    }

    return js::IsNativeFunction(v, native);
}

extern JS_ALWAYS_INLINE bool
SameTraceType(const Value &lhs, const Value &rhs)
{
    return SameType(lhs, rhs) &&
           (lhs.isPrimitive() ||
            lhs.toObject().isFunction() == rhs.toObject().isFunction());
}

/*
 * Return true if this is a compiler-created internal function accessed by
 * its own object. Such a function object must not be accessible to script
 * or embedding code.
 */
inline bool
IsInternalFunctionObject(JSObject *funobj)
{
    JS_ASSERT(funobj->isFunction());
    JSFunction *fun = (JSFunction *) funobj->getPrivate();
    return funobj == fun && (fun->flags & JSFUN_LAMBDA) && !funobj->getParent();
}
    
/* Valueified JS_IsConstructing. */
static JS_ALWAYS_INLINE bool
IsConstructing(const Value *vp)
{
#ifdef DEBUG
    JSObject *callee = &JS_CALLEE(cx, vp).toObject();
    if (callee->isFunction()) {
        JSFunction *fun = callee->getFunctionPrivate();
        JS_ASSERT((fun->flags & JSFUN_CONSTRUCTOR) != 0);
    } else {
        JS_ASSERT(callee->getClass()->construct != NULL);
    }
#endif
    return vp[1].isMagic();
}

static JS_ALWAYS_INLINE bool
IsConstructing_PossiblyWithGivenThisObject(const Value *vp, JSObject **ctorThis)
{
#ifdef DEBUG
    JSObject *callee = &JS_CALLEE(cx, vp).toObject();
    if (callee->isFunction()) {
        JSFunction *fun = callee->getFunctionPrivate();
        JS_ASSERT((fun->flags & JSFUN_CONSTRUCTOR) != 0);
    } else {
        JS_ASSERT(callee->getClass()->construct != NULL);
    }
#endif
    bool isCtor = vp[1].isMagic();
    if (isCtor)
        *ctorThis = vp[1].getMagicObjectOrNullPayload();
    return isCtor;
}

inline const char *
GetFunctionNameBytes(JSContext *cx, JSFunction *fun, JSAutoByteString *bytes)
{
    if (fun->atom)
        return bytes->encode(cx, fun->atom);
    return js_anonymous_str;
}

extern bool
IsBuiltinFunctionConstructor(JSFunction *fun);

/*
 * Preconditions: funobj->isInterpreted() && !funobj->isFunctionPrototype() &&
 * !funobj->isBoundFunction(). This is sufficient to establish that funobj has
 * a non-configurable non-method .prototype data property, thought it might not
 * have been resolved yet, and its value could be anything.
 *
 * Return the shape of the .prototype property of funobj, resolving it if
 * needed. On error, return NULL.
 *
 * This is not safe to call on trace because it defines properties, which can
 * trigger lookups that could reenter.
 */
const Shape *
LookupInterpretedFunctionPrototype(JSContext *cx, JSObject *funobj);

} /* namespace js */

extern JSString *
fun_toStringHelper(JSContext *cx, JSObject *obj, uintN indent);

extern JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, js::Native native, uintN nargs,
               uintN flags, JSObject *parent, JSAtom *atom);

extern JSObject *
js_InitFunctionClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitArgumentsClass(JSContext *cx, JSObject *obj);

extern void
js_FinalizeFunction(JSContext *cx, JSFunction *fun);

extern JSObject * JS_FASTCALL
js_CloneFunctionObject(JSContext *cx, JSFunction *fun, JSObject *parent,
                       JSObject *proto);

inline JSObject *
CloneFunctionObject(JSContext *cx, JSFunction *fun, JSObject *parent,
                    bool ignoreSingletonClone = false)
{
    JS_ASSERT(parent);
    JSObject *proto;
    if (!js_GetClassPrototype(cx, parent, JSProto_Function, &proto))
        return NULL;

    /*
     * For attempts to clone functions at a function definition opcode or from
     * a method barrier, don't perform the clone if the function has singleton
     * type. CloneFunctionObject was called pessimistically, and we need to
     * preserve the type's property that if it is singleton there is only a
     * single object with its type in existence.
     */
    if (ignoreSingletonClone && fun->hasSingletonType()) {
        JS_ASSERT(fun->getProto() == proto);
        fun->setParent(parent);
        return fun;
    }

    return js_CloneFunctionObject(cx, fun, parent, proto);
}

extern JSObject * JS_FASTCALL
js_AllocFlatClosure(JSContext *cx, JSFunction *fun, JSObject *scopeChain);

extern JSObject *
js_NewFlatClosure(JSContext *cx, JSFunction *fun, JSOp op, size_t oplen);

extern JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, jsid id, js::Native native,
                  uintN nargs, uintN flags);

/*
 * Flags for js_ValueToFunction and js_ReportIsNotFunction.
 */
#define JSV2F_CONSTRUCT         INITIAL_CONSTRUCT
#define JSV2F_SEARCH_STACK      0x10000

extern JSFunction *
js_ValueToFunction(JSContext *cx, const js::Value *vp, uintN flags);

extern JSObject *
js_ValueToFunctionObject(JSContext *cx, js::Value *vp, uintN flags);

extern JSObject *
js_ValueToCallableObject(JSContext *cx, js::Value *vp, uintN flags);

extern void
js_ReportIsNotFunction(JSContext *cx, const js::Value *vp, uintN flags);

extern JSObject * JS_FASTCALL
js_CreateCallObjectOnTrace(JSContext *cx, JSFunction *fun, JSObject *callee, JSObject *scopeChain);

extern void
js_PutCallObject(js::StackFrame *fp);

extern JSBool JS_FASTCALL
js_PutCallObjectOnTrace(JSObject *scopeChain, uint32 nargs, js::Value *argv,
                        uint32 nvars, js::Value *slots);

namespace js {

CallObject *
CreateFunCallObject(JSContext *cx, StackFrame *fp);

CallObject *
CreateEvalCallObject(JSContext *cx, StackFrame *fp);

extern JSBool
GetCallArg(JSContext *cx, JSObject *obj, jsid id, js::Value *vp);

extern JSBool
GetCallVar(JSContext *cx, JSObject *obj, jsid id, js::Value *vp);

extern JSBool
GetCallUpvar(JSContext *cx, JSObject *obj, jsid id, js::Value *vp);

extern JSBool
SetCallArg(JSContext *cx, JSObject *obj, jsid id, JSBool strict, js::Value *vp);

extern JSBool
SetCallVar(JSContext *cx, JSObject *obj, jsid id, JSBool strict, js::Value *vp);

extern JSBool
SetCallUpvar(JSContext *cx, JSObject *obj, jsid id, JSBool strict, js::Value *vp);

} // namespace js

extern JSBool
js_GetArgsValue(JSContext *cx, js::StackFrame *fp, js::Value *vp);

extern JSBool
js_GetArgsProperty(JSContext *cx, js::StackFrame *fp, jsid id, js::Value *vp);

/*
 * Get the arguments object for the given frame.  If the frame is strict mode
 * code, its current arguments will be copied into the arguments object.
 *
 * NB: Callers *must* get the arguments object before any parameters are
 *     mutated when the frame is strict mode code!  The emitter ensures this
 *     occurs for strict mode functions containing syntax which might mutate a
 *     named parameter by synthesizing an arguments access at the start of the
 *     function.
 */
extern JSObject *
js_GetArgsObject(JSContext *cx, js::StackFrame *fp);

extern void
js_PutArgsObject(js::StackFrame *fp);

inline bool
js_IsNamedLambda(JSFunction *fun) { return (fun->flags & JSFUN_LAMBDA) && fun->atom; }

extern JSBool
js_XDRFunctionObject(JSXDRState *xdr, JSObject **objp);

extern JSBool
js_fun_apply(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_fun_call(JSContext *cx, uintN argc, js::Value *vp);

#endif /* jsfun_h___ */

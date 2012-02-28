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

#include "gc/Barrier.h"

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
#define JSFUN_EXTENDED      0x2000  /* structure is FunctionExtended */
#define JSFUN_INTERPRETED   0x4000  /* use u.i if kind >= this value else u.n */
#define JSFUN_FLAT_CLOSURE  0x8000  /* flat (aka "display") closure */
#define JSFUN_NULL_CLOSURE  0xc000  /* null closure entrains no scope chain */
#define JSFUN_KINDMASK      0xc000  /* encode interp vs. native and closure
                                       optimization level -- see above */

namespace js { class FunctionExtended; }

struct JSFunction : public JSObject
{
    uint16_t        nargs;        /* maximum number of specified arguments,
                                     reflected as f.length/f.arity */
    uint16_t        flags;        /* flags, see JSFUN_* below and in jsapi.h */
    union U {
        struct Native {
            js::Native  native;   /* native method pointer or null */
            js::Class   *clasp;   /* class of objects constructed
                                     by this function */
        } n;
        struct Scripted {
            JSScript    *script_; /* interpreted bytecode descriptor or null;
                                     use the accessor! */
            JSObject    *env_;    /* environment for new activations;
                                     use the accessor! */
        } i;
        void            *nativeOrScript;
    } u;
    JSAtom          *atom;        /* name for diagnostics and decompiling */

    bool optimizedClosure()  const { return kind() > JSFUN_INTERPRETED; }
    bool isInterpreted()     const { return kind() >= JSFUN_INTERPRETED; }
    bool isNative()          const { return !isInterpreted(); }
    bool isNativeConstructor() const { return flags & JSFUN_CONSTRUCTOR; }
    bool isHeavyweight()     const { return JSFUN_HEAVYWEIGHT_TEST(flags); }
    bool isNullClosure()     const { return kind() == JSFUN_NULL_CLOSURE; }
    bool isFlatClosure()     const { return kind() == JSFUN_FLAT_CLOSURE; }
    bool isFunctionPrototype() const { return flags & JSFUN_PROTOTYPE; }
    bool isInterpretedConstructor() const { return isInterpreted() && !isFunctionPrototype(); }

    uint16_t kind()          const { return flags & JSFUN_KINDMASK; }
    void setKind(uint16_t k) {
        JS_ASSERT(!(k & ~JSFUN_KINDMASK));
        flags = (flags & ~JSFUN_KINDMASK) | k;
    }

    /* Returns the strictness of this function, which must be interpreted. */
    inline bool inStrictMode() const;

    void setArgCount(uint16_t nargs) {
        JS_ASSERT(this->nargs == 0);
        this->nargs = nargs;
    }

    /* uint16_t representation bounds number of call object dynamic slots. */
    enum { MAX_ARGS_AND_VARS = 2 * ((1U << 16) - 1) };

#define JS_LOCAL_NAME_TO_ATOM(nameWord)  ((JSAtom *) ((nameWord) & ~uintptr_t(1)))
#define JS_LOCAL_NAME_IS_CONST(nameWord) ((((nameWord) & uintptr_t(1))) != 0)

    bool mightEscape() const {
        return isInterpreted() && (isFlatClosure() || !script()->bindings.hasUpvars());
    }

    bool joinable() const {
        return flags & JSFUN_JOINABLE;
    }

    /*
     * For an interpreted function, accessors for the initial scope object of
     * activations (stack frames) of the function.
     */
    inline JSObject *environment() const;
    inline void setEnvironment(JSObject *obj);
    inline void initEnvironment(JSObject *obj);

    static inline size_t offsetOfEnvironment() { return offsetof(JSFunction, u.i.env_); }

    inline void setJoinable();

    js::HeapPtrScript &script() const {
        JS_ASSERT(isInterpreted());
        return *(js::HeapPtrScript *)&u.i.script_;
    }

    inline void setScript(JSScript *script_);
    inline void initScript(JSScript *script_);

    JSScript *maybeScript() const {
        return isInterpreted() ? script().get() : NULL;
    }

    JSNative native() const {
        JS_ASSERT(isNative());
        return u.n.native;
    }

    JSNative maybeNative() const {
        return isInterpreted() ? NULL : native();
    }

    static unsigned offsetOfNativeOrScript() {
        JS_STATIC_ASSERT(offsetof(U, n.native) == offsetof(U, i.script_));
        JS_STATIC_ASSERT(offsetof(U, n.native) == offsetof(U, nativeOrScript));
        return offsetof(JSFunction, u.nativeOrScript);
    }

    js::Class *getConstructorClass() const {
        JS_ASSERT(isNative());
        return u.n.clasp;
    }

    void setConstructorClass(js::Class *clasp) {
        JS_ASSERT(isNative());
        u.n.clasp = clasp;
    }

#if JS_BITS_PER_WORD == 32
    static const js::gc::AllocKind FinalizeKind = js::gc::FINALIZE_OBJECT2;
    static const js::gc::AllocKind ExtendedFinalizeKind = js::gc::FINALIZE_OBJECT4;
#else
    static const js::gc::AllocKind FinalizeKind = js::gc::FINALIZE_OBJECT4;
    static const js::gc::AllocKind ExtendedFinalizeKind = js::gc::FINALIZE_OBJECT8;
#endif

    inline void trace(JSTracer *trc);

    /* Bound function accessors. */

    inline bool initBoundFunction(JSContext *cx, const js::Value &thisArg,
                                  const js::Value *args, unsigned argslen);

    inline JSObject *getBoundFunctionTarget() const;
    inline const js::Value &getBoundFunctionThis() const;
    inline const js::Value &getBoundFunctionArgument(unsigned which) const;
    inline size_t getBoundFunctionArgumentCount() const;

  private:
    inline js::FunctionExtended *toExtended();
    inline const js::FunctionExtended *toExtended() const;

    inline bool isExtended() const {
        JS_STATIC_ASSERT(FinalizeKind != ExtendedFinalizeKind);
        JS_ASSERT(!!(flags & JSFUN_EXTENDED) == (getAllocKind() == ExtendedFinalizeKind));
        return !!(flags & JSFUN_EXTENDED);
    }

  public:
    /* Accessors for data stored in extended functions. */

    inline void initializeExtended();

    inline void setExtendedSlot(size_t which, const js::Value &val);
    inline const js::Value &getExtendedSlot(size_t which) const;

    /*
     * Flat closures with one or more upvars snapshot the upvars' values
     * into a vector of js::Values referenced from here. This is a private
     * pointer but is set only at creation and does not need to be barriered.
     */
    static const uint32_t FLAT_CLOSURE_UPVARS_SLOT = 0;

    static inline size_t getFlatClosureUpvarsOffset();

    inline js::Value getFlatClosureUpvar(uint32_t i) const;
    inline void setFlatClosureUpvar(uint32_t i, const js::Value &v);
    inline void initFlatClosureUpvar(uint32_t i, const js::Value &v);

  private:
    inline bool hasFlatClosureUpvars() const;
    inline js::HeapValue *getFlatClosureUpvars() const;
  public:

    /* See comments in fun_finalize. */
    inline void finalizeUpvars();

    /* Slot holding associated method property, needed for foo.caller handling. */
    static const uint32_t METHOD_PROPERTY_SLOT = 0;

    /* For cloned methods, slot holding the object this was cloned as a property from. */
    static const uint32_t METHOD_OBJECT_SLOT = 1;

    /* Whether this is a function cloned from a method. */
    inline bool isClonedMethod() const;

    /* For a cloned method, pointer to the object the method was cloned for. */
    inline JSObject *methodObj() const;
    inline void setMethodObj(JSObject& obj);

    /*
     * Method name imputed from property uniquely assigned to or initialized,
     * where the function does not need to be cloned to carry a scope chain or
     * flattened upvars. This is set on both the original and cloned function.
     */
    inline JSAtom *methodAtom() const;
    inline void setMethodAtom(JSAtom *atom);

    /*
     * Measures things hanging off this JSFunction that are counted by the
     * |miscSize| argument in JSObject::sizeOfExcludingThis().
     */
    size_t sizeOfMisc(JSMallocSizeOfFun mallocSizeOf) const;

  private:
    /* 
     * These member functions are inherited from JSObject, but should never be applied to
     * a value statically known to be a JSFunction.
     */
    inline JSFunction *toFunction() MOZ_DELETE;
    inline const JSFunction *toFunction() const MOZ_DELETE;
};

inline JSFunction *
JSObject::toFunction()
{
    JS_ASSERT(JS_ObjectIsFunction(NULL, this));
    return static_cast<JSFunction *>(this);
}

inline const JSFunction *
JSObject::toFunction() const
{
    JS_ASSERT(JS_ObjectIsFunction(NULL, const_cast<JSObject *>(this)));
    return static_cast<const JSFunction *>(this);
}

extern JSString *
fun_toStringHelper(JSContext *cx, JSObject *obj, unsigned indent);

extern JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, JSNative native, unsigned nargs,
               unsigned flags, js::HandleObject parent, JSAtom *atom,
               js::gc::AllocKind kind = JSFunction::FinalizeKind);

extern JSFunction * JS_FASTCALL
js_CloneFunctionObject(JSContext *cx, JSFunction *fun, JSObject *parent, JSObject *proto,
                       js::gc::AllocKind kind = JSFunction::FinalizeKind);

extern JSFunction * JS_FASTCALL
js_AllocFlatClosure(JSContext *cx, JSFunction *fun, JSObject *scopeChain);

extern JSFunction *
js_NewFlatClosure(JSContext *cx, JSFunction *fun);

extern JSFunction *
js_DefineFunction(JSContext *cx, js::HandleObject obj, jsid id, JSNative native,
                  unsigned nargs, unsigned flags,
                  js::gc::AllocKind kind = JSFunction::FinalizeKind);

/*
 * Flags for js_ValueToFunction and js_ReportIsNotFunction.
 */
#define JSV2F_CONSTRUCT         INITIAL_CONSTRUCT
#define JSV2F_SEARCH_STACK      0x10000

extern JSFunction *
js_ValueToFunction(JSContext *cx, const js::Value *vp, unsigned flags);

extern JSObject *
js_ValueToCallableObject(JSContext *cx, js::Value *vp, unsigned flags);

extern void
js_ReportIsNotFunction(JSContext *cx, const js::Value *vp, unsigned flags);

extern void
js_PutCallObject(js::StackFrame *fp);

namespace js {

/*
 * Function extended with reserved slots for use by various kinds of functions.
 * Most functions do not have these extensions, but enough are that efficient
 * storage is required (no malloc'ed reserved slots).
 */
class FunctionExtended : public JSFunction
{
    friend struct JSFunction;

    /* Reserved slots available for storage by particular native functions. */
    HeapValue extendedSlots[2];
};

} // namespace js

inline js::FunctionExtended *
JSFunction::toExtended()
{
    JS_ASSERT(isExtended());
    return static_cast<js::FunctionExtended *>(this);
}

inline const js::FunctionExtended *
JSFunction::toExtended() const
{
    JS_ASSERT(isExtended());
    return static_cast<const js::FunctionExtended *>(this);
}

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
extern js::ArgumentsObject *
js_GetArgsObject(JSContext *cx, js::StackFrame *fp);

extern void
js_PutArgsObject(js::StackFrame *fp);

inline bool
js_IsNamedLambda(JSFunction *fun) { return (fun->flags & JSFUN_LAMBDA) && fun->atom; }

namespace js {

extern JSBool
XDRFunctionObject(JSXDRState *xdr, JSObject **objp);

} /* namespace js */

extern JSBool
js_fun_apply(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_fun_call(JSContext *cx, unsigned argc, js::Value *vp);

#endif /* jsfun_h___ */

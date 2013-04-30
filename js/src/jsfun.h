/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsfun_h___
#define jsfun_h___
/*
 * JS function definitions.
 */
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsobj.h"
#include "jsatom.h"
#include "jsstr.h"

#include "gc/Barrier.h"

namespace js { class FunctionExtended; }

class JSFunction : public JSObject
{
  public:
    enum Flags {
        INTERPRETED      = 0x0001,  /* function has a JSScript and environment. */
        NATIVE_CTOR      = 0x0002,  /* native that can be called as a constructor */
        EXTENDED         = 0x0004,  /* structure is FunctionExtended */
        HEAVYWEIGHT      = 0x0008,  /* activation requires a Call object */
        IS_FUN_PROTO     = 0x0010,  /* function is Function.prototype for some global object */
        EXPR_CLOSURE     = 0x0020,  /* expression closure: function(x) x*x */
        HAS_GUESSED_ATOM = 0x0040,  /* function had no explicit name, but a
                                       name was guessed for it anyway */
        LAMBDA           = 0x0080,  /* function comes from a FunctionExpression, ArrowFunction, or
                                       Function() call (not a FunctionDeclaration or nonstandard
                                       function-statement) */
        SELF_HOSTED      = 0x0100,  /* function is self-hosted builtin and must not be
                                       decompilable nor constructible. */
        SELF_HOSTED_CTOR = 0x0200,  /* function is self-hosted builtin constructor and
                                       must be constructible but not decompilable. */
        HAS_REST         = 0x0400,  /* function has a rest (...) parameter */
        HAS_DEFAULTS     = 0x0800,  /* function has at least one default parameter */
        INTERPRETED_LAZY = 0x1000,  /* function is interpreted but doesn't have a script yet */
        ARROW            = 0x2000,  /* ES6 '(args) => body' syntax */
        SH_WRAPPABLE     = 0x4000,  /* self-hosted function is wrappable, doesn't need to be cloned */

        /* Derived Flags values for convenience: */
        NATIVE_FUN = 0,
        INTERPRETED_LAMBDA = INTERPRETED | LAMBDA,
        INTERPRETED_LAMBDA_ARROW = INTERPRETED | LAMBDA | ARROW
    };

    static void staticAsserts() {
        JS_STATIC_ASSERT(INTERPRETED == JS_FUNCTION_INTERPRETED_BIT);
        MOZ_STATIC_ASSERT(sizeof(JSFunction) == sizeof(js::shadow::Function),
                          "shadow interface must match actual interface");
    }

    uint16_t        nargs;        /* maximum number of specified arguments,
                                     reflected as f.length/f.arity */
    uint16_t        flags;        /* bitfield composed of the above Flags enum */
    union U {
        class Native {
            friend class JSFunction;
            js::Native          native;       /* native method pointer or null */
            const JSJitInfo     *jitinfo;     /* Information about this function to be
                                                 used by the JIT;
                                                 use the accessor! */
        } n;
        struct Scripted {
            JSScript    *script_; /* interpreted bytecode descriptor or null;
                                     use the accessor! */
            JSObject    *env_;    /* environment for new activations;
                                     use the accessor! */
        } i;
        void            *nativeOrScript;
    } u;
  private:
    js::HeapPtrAtom  atom_;       /* name for diagnostics and decompiling */

  public:

    /* A function can be classified as either native (C++) or interpreted (JS): */
    bool isInterpreted()            const { return flags & (INTERPRETED | INTERPRETED_LAZY); }
    bool isNative()                 const { return !isInterpreted(); }

    /* Possible attributes of a native function: */
    bool isNativeConstructor()      const { return flags & NATIVE_CTOR; }

    /* Possible attributes of an interpreted function: */
    bool isHeavyweight()            const { return flags & HEAVYWEIGHT; }
    bool isFunctionPrototype()      const { return flags & IS_FUN_PROTO; }
    bool isInterpretedLazy()        const { return flags & INTERPRETED_LAZY; }
    bool hasScript()                const { return isInterpreted() && u.i.script_; }
    bool isExprClosure()            const { return flags & EXPR_CLOSURE; }
    bool hasGuessedAtom()           const { return flags & HAS_GUESSED_ATOM; }
    bool isLambda()                 const { return flags & LAMBDA; }
    bool isSelfHostedBuiltin()      const { return flags & SELF_HOSTED; }
    bool isSelfHostedConstructor()  const { return flags & SELF_HOSTED_CTOR; }
    bool hasRest()                  const { return flags & HAS_REST; }
    bool hasDefaults()              const { return flags & HAS_DEFAULTS; }
    bool isWrappable()              const {
        JS_ASSERT_IF(flags & SH_WRAPPABLE, isSelfHostedBuiltin());
        return flags & SH_WRAPPABLE;
    }

    // Arrow functions are a little weird.
    //
    // Like all functions, (1) when the compiler parses an arrow function, it
    // creates a function object that gets stored with the enclosing script;
    // and (2) at run time the script's function object is cloned.
    //
    // But then, unlike other functions, (3) a bound function is created with
    // the clone as its target.
    //
    // isArrow() is true for all three of these Function objects.
    // isBoundFunction() is true only for the last one.
    bool isArrow()                  const { return flags & ARROW; }

    /* Compound attributes: */
    bool isBuiltin() const {
        return isNative() || isSelfHostedBuiltin();
    }
    bool isInterpretedConstructor() const {
        return isInterpreted() && !isFunctionPrototype() &&
               (!isSelfHostedBuiltin() || isSelfHostedConstructor());
    }
    bool isNamedLambda()     const {
        return isLambda() && atom_ && !hasGuessedAtom();
    }

    /* Returns the strictness of this function, which must be interpreted. */
    inline bool strict() const;

    // Can be called multiple times by the parser.
    void setArgCount(uint16_t nargs) {
        JS_ASSERT(this->nargs == 0 || this->nargs == nargs);
        this->nargs = nargs;
    }

    // Can be called multiple times by the parser.
    void setHasRest() {
        flags |= HAS_REST;
    }

    // Can be called multiple times by the parser.
    void setHasDefaults() {
        flags |= HAS_DEFAULTS;
    }

    void setIsSelfHostedBuiltin() {
        JS_ASSERT(!isSelfHostedBuiltin());
        flags |= SELF_HOSTED;
    }

    void setIsSelfHostedConstructor() {
        JS_ASSERT(!isSelfHostedConstructor());
        flags |= SELF_HOSTED_CTOR;
    }

    void makeWrappable() {
        JS_ASSERT(isSelfHostedBuiltin());
        JS_ASSERT(!isWrappable());
        flags |= SH_WRAPPABLE;
    }

    void setIsFunctionPrototype() {
        JS_ASSERT(!isFunctionPrototype());
        flags |= IS_FUN_PROTO;
    }

    void setIsHeavyweight() {
        flags |= HEAVYWEIGHT;
    }

    // Can be called multiple times by the parser.
    void setIsExprClosure() {
        flags |= EXPR_CLOSURE;
    }

    void markNotLazy() {
        JS_ASSERT(isInterpretedLazy());
        JS_ASSERT(hasScript());
        flags |= INTERPRETED;
        flags &= ~INTERPRETED_LAZY;
    }

    JSAtom *atom() const { return hasGuessedAtom() ? NULL : atom_.get(); }
    js::PropertyName *name() const { return hasGuessedAtom() || !atom_ ? NULL : atom_->asPropertyName(); }
    inline void initAtom(JSAtom *atom);
    JSAtom *displayAtom() const { return atom_; }

    inline void setGuessedAtom(JSAtom *atom);

    /* uint16_t representation bounds number of call object dynamic slots. */
    enum { MAX_ARGS_AND_VARS = 2 * ((1U << 16) - 1) };

    /*
     * For an interpreted function, accessors for the initial scope object of
     * activations (stack frames) of the function.
     */
    inline JSObject *environment() const;
    inline void setEnvironment(JSObject *obj);
    inline void initEnvironment(JSObject *obj);

    static inline size_t offsetOfEnvironment() { return offsetof(JSFunction, u.i.env_); }
    static inline size_t offsetOfAtom() { return offsetof(JSFunction, atom_); }

    bool initializeLazyScript(JSContext *cx);

    JSScript *getOrCreateScript(JSContext *cx) {
        JS_ASSERT(isInterpreted());
        JS_ASSERT(cx);
        if (isInterpretedLazy()) {
            JS::RootedFunction self(cx, this);
            js::MaybeCheckStackRoots(cx);
            if (!self->initializeLazyScript(cx))
                return NULL;
            return self->u.i.script_;
        }
        JS_ASSERT(hasScript());
        return u.i.script_;
    }

    static bool maybeGetOrCreateScript(JSContext *cx, js::HandleFunction fun,
                                       js::MutableHandle<JSScript*> script)
    {
        if (fun->isNative()) {
            script.set(NULL);
            return true;
        }
        script.set(fun->getOrCreateScript(cx));
        return fun->hasScript();
    }

    JSScript *nonLazyScript() const {
        JS_ASSERT(hasScript());
        return JS::HandleScript::fromMarkedLocation(&u.i.script_);
    }

    JSScript *maybeNonLazyScript() const {
        return isInterpreted() ? nonLazyScript() : NULL;
    }

    js::HeapPtrScript &mutableScript() {
        JS_ASSERT(isInterpreted());
        return *(js::HeapPtrScript *)&u.i.script_;
    }

    inline void setScript(JSScript *script_);
    inline void initScript(JSScript *script_);

    JSNative native() const {
        JS_ASSERT(isNative());
        return u.n.native;
    }

    JSNative maybeNative() const {
        return isInterpreted() ? NULL : native();
    }

    inline void initNative(js::Native native, const JSJitInfo *jitinfo);
    inline const JSJitInfo *jitInfo() const;
    inline void setJitInfo(const JSJitInfo *data);

    static unsigned offsetOfNativeOrScript() {
        JS_STATIC_ASSERT(offsetof(U, n.native) == offsetof(U, i.script_));
        JS_STATIC_ASSERT(offsetof(U, n.native) == offsetof(U, nativeOrScript));
        return offsetof(JSFunction, u.nativeOrScript);
    }

#if JS_BITS_PER_WORD == 32
    static const js::gc::AllocKind FinalizeKind = js::gc::FINALIZE_OBJECT2_BACKGROUND;
    static const js::gc::AllocKind ExtendedFinalizeKind = js::gc::FINALIZE_OBJECT4_BACKGROUND;
#else
    static const js::gc::AllocKind FinalizeKind = js::gc::FINALIZE_OBJECT4_BACKGROUND;
    static const js::gc::AllocKind ExtendedFinalizeKind = js::gc::FINALIZE_OBJECT8_BACKGROUND;
#endif

    inline void trace(JSTracer *trc);

    /* Bound function accessors. */

    inline bool initBoundFunction(JSContext *cx, js::HandleValue thisArg,
                                  const js::Value *args, unsigned argslen);

    inline JSObject *getBoundFunctionTarget() const;
    inline const js::Value &getBoundFunctionThis() const;
    inline const js::Value &getBoundFunctionArgument(unsigned which) const;
    inline size_t getBoundFunctionArgumentCount() const;

  private:
    inline js::FunctionExtended *toExtended();
    inline const js::FunctionExtended *toExtended() const;

  public:
    inline bool isExtended() const {
        JS_STATIC_ASSERT(FinalizeKind != ExtendedFinalizeKind);
        JS_ASSERT_IF(isTenured(), !!(flags & EXTENDED) == (tenuredGetAllocKind() == ExtendedFinalizeKind));
        return !!(flags & EXTENDED);
    }

    /*
     * Accessors for data stored in extended functions. Use setExtendedSlot if
     * the function has already been initialized. Otherwise use
     * initExtendedSlot.
     */
    inline void initializeExtended();
    inline void initExtendedSlot(size_t which, const js::Value &val);
    inline void setExtendedSlot(size_t which, const js::Value &val);
    inline const js::Value &getExtendedSlot(size_t which) const;

    /* Constructs a new type for the function if necessary. */
    static bool setTypeForScriptedFunction(JSContext *cx, js::HandleFunction fun,
                                           bool singleton = false);

    /* GC support. */
    js::gc::AllocKind getAllocKind() const {
        js::gc::AllocKind kind = FinalizeKind;
        if (isExtended())
            kind = ExtendedFinalizeKind;
        JS_ASSERT_IF(isTenured(), kind == tenuredGetAllocKind());
        return kind;
    }

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
fun_toStringHelper(JSContext *cx, js::HandleObject obj, unsigned indent);

inline JSFunction::Flags
JSAPIToJSFunctionFlags(unsigned flags)
{
    return (flags & JSFUN_CONSTRUCTOR)
           ? JSFunction::NATIVE_CTOR
           : JSFunction::NATIVE_FUN;
}

namespace js {

extern JSFunction *
NewFunction(JSContext *cx, HandleObject funobj, JSNative native, unsigned nargs,
            JSFunction::Flags flags, HandleObject parent, HandleAtom atom,
            gc::AllocKind allocKind = JSFunction::FinalizeKind,
            NewObjectKind newKind = GenericObject);

extern JSFunction *
DefineFunction(JSContext *cx, HandleObject obj, HandleId id, JSNative native,
               unsigned nargs, unsigned flags,
               gc::AllocKind allocKind = JSFunction::FinalizeKind,
               NewObjectKind newKind = GenericObject);

/*
 * Function extended with reserved slots for use by various kinds of functions.
 * Most functions do not have these extensions, but enough do that efficient
 * storage is required (no malloc'ed reserved slots).
 */
class FunctionExtended : public JSFunction
{
  public:
    static const unsigned NUM_EXTENDED_SLOTS = 2;

  private:
    friend class JSFunction;

    /* Reserved slots available for storage by particular native functions. */
    HeapValue extendedSlots[NUM_EXTENDED_SLOTS];
};

extern JSFunction *
CloneFunctionObject(JSContext *cx, HandleFunction fun, HandleObject parent,
                    gc::AllocKind kind = JSFunction::FinalizeKind);

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

namespace js {

JSString *FunctionToString(JSContext *cx, HandleFunction fun, bool bodyOnly, bool lambdaParen);

template<XDRMode mode>
bool
XDRInterpretedFunction(XDRState<mode> *xdr, HandleObject enclosingScope,
                       HandleScript enclosingScript, MutableHandleObject objp);

extern JSObject *
CloneInterpretedFunction(JSContext *cx, HandleObject enclosingScope, HandleFunction fun);

/*
 * Report an error that call.thisv is not compatible with the specified class,
 * assuming that the method (clasp->name).prototype.<name of callee function>
 * is what was called.
 */
extern void
ReportIncompatibleMethod(JSContext *cx, CallReceiver call, Class *clasp);

/*
 * Report an error that call.thisv is not an acceptable this for the callee
 * function.
 */
extern void
ReportIncompatible(JSContext *cx, CallReceiver call);

JSBool
CallOrConstructBoundFunction(JSContext *, unsigned, js::Value *);

extern const JSFunctionSpec function_methods[];

} /* namespace js */

extern JSBool
js_fun_apply(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_fun_call(JSContext *cx, unsigned argc, js::Value *vp);

extern JSObject*
js_fun_bind(JSContext *cx, js::HandleObject target, js::HandleValue thisArg,
            js::Value *boundArgs, unsigned argslen);

#endif /* jsfun_h___ */

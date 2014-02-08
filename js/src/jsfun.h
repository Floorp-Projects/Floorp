/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsfun_h
#define jsfun_h

/*
 * JS function definitions.
 */

#include "jsobj.h"
#include "jsscript.h"
#include "jstypes.h"

namespace js {
class FunctionExtended;

typedef JSNative           Native;
typedef JSParallelNative   ParallelNative;
typedef JSThreadSafeNative ThreadSafeNative;
}

struct JSAtomState;

class JSFunction : public JSObject
{
  public:
    static const js::Class class_;

    enum Flags {
        INTERPRETED      = 0x0001,  /* function has a JSScript and environment. */
        NATIVE_CTOR      = 0x0002,  /* native that can be called as a constructor */
        EXTENDED         = 0x0004,  /* structure is FunctionExtended */
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
        // 0x0800 is available
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
        static_assert(sizeof(JSFunction) == sizeof(js::shadow::Function),
                      "shadow interface must match actual interface");
    }

  private:
    uint16_t        nargs_;       /* number of formal arguments
                                     (including defaults and the rest parameter unlike f.length) */
    uint16_t        flags_;       /* bitfield composed of the above Flags enum */
    union U {
        class Native {
            friend class JSFunction;
            js::Native          native;       /* native method pointer or null */
            const JSJitInfo     *jitinfo;     /* Information about this function to be
                                                 used by the JIT;
                                                 use the accessor! */
        } n;
        struct Scripted {
            union {
                JSScript *script_; /* interpreted bytecode descriptor or null;
                                      use the accessor! */
                js::LazyScript *lazy_; /* lazily compiled script, or nullptr */
            } s;
            JSObject    *env_;    /* environment for new activations;
                                     use the accessor! */
        } i;
        void            *nativeOrScript;
    } u;
    js::HeapPtrAtom  atom_;       /* name for diagnostics and decompiling */

  public:

    /* Call objects must be created for each invocation of a heavyweight function. */
    bool isHeavyweight() const {
        JS_ASSERT(!isInterpretedLazy());

        if (isNative())
            return false;

        // Note: this should be kept in sync with FunctionBox::isHeavyweight().
        return nonLazyScript()->hasAnyAliasedBindings() ||
               nonLazyScript()->funHasExtensibleScope() ||
               nonLazyScript()->funNeedsDeclEnvObject() ||
               isGenerator();
    }

    size_t nargs() const {
        js::AutoThreadSafeAccess ts(this);
        return nargs_;
    }

    uint16_t flags() const {
        js::AutoThreadSafeAccess ts(this);
        return flags_;
    }

    /* A function can be classified as either native (C++) or interpreted (JS): */
    bool isInterpreted()            const { return flags() & (INTERPRETED | INTERPRETED_LAZY); }
    bool isNative()                 const { return !isInterpreted(); }

    /* Possible attributes of a native function: */
    bool isNativeConstructor()      const { return flags() & NATIVE_CTOR; }

    /* Possible attributes of an interpreted function: */
    bool isFunctionPrototype()      const { return flags() & IS_FUN_PROTO; }
    bool isExprClosure()            const { return flags() & EXPR_CLOSURE; }
    bool hasGuessedAtom()           const { return flags() & HAS_GUESSED_ATOM; }
    bool isLambda()                 const { return flags() & LAMBDA; }
    bool isSelfHostedBuiltin()      const { return flags() & SELF_HOSTED; }
    bool isSelfHostedConstructor()  const { return flags() & SELF_HOSTED_CTOR; }
    bool hasRest()                  const { return flags() & HAS_REST; }
    bool isWrappable()              const {
        JS_ASSERT_IF(flags() & SH_WRAPPABLE, isSelfHostedBuiltin());
        return flags() & SH_WRAPPABLE;
    }

    // Functions can change between being lazily interpreted and having scripts
    // when under the compilation lock.
    bool isInterpretedLazy()        const {
        JS_ASSERT(js::CurrentThreadCanReadCompilationData());
        return flags() & INTERPRETED_LAZY;
    }
    bool hasScript()                const {
        JS_ASSERT(js::CurrentThreadCanReadCompilationData());
        return flags() & INTERPRETED;
    }

    bool hasJITCode() const {
        if (!hasScript())
            return false;

        return nonLazyScript()->hasBaselineScript() || nonLazyScript()->hasIonScript();
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
    bool isArrow()                  const { return flags() & ARROW; }

    /* Compound attributes: */
    bool isBuiltin() const {
        return isNative() || isSelfHostedBuiltin();
    }
    bool isInterpretedConstructor() const {
        // Note: the JITs inline this check, so be careful when making changes
        // here. See IonMacroAssembler::branchIfNotInterpretedConstructor.
        return isInterpreted() && !isFunctionPrototype() &&
               (!isSelfHostedBuiltin() || isSelfHostedConstructor());
    }
    bool isNamedLambda() const {
        return isLambda() && displayAtom() && !hasGuessedAtom();
    }
    bool hasParallelNative() const {
        return isNative() && jitInfo() && jitInfo()->hasParallelNative();
    }

    bool isBuiltinFunctionConstructor();

    /* Returns the strictness of this function, which must be interpreted. */
    bool strict() const {
        return nonLazyScript()->strict();
    }

    void setFlags(uint16_t flags) {
        this->flags_ = flags;
    }

    // Can be called multiple times by the parser.
    void setArgCount(uint16_t nargs) {
        this->nargs_ = nargs;
    }

    // Can be called multiple times by the parser.
    void setHasRest() {
        flags_ |= HAS_REST;
    }

    void setIsSelfHostedBuiltin() {
        JS_ASSERT(!isSelfHostedBuiltin());
        flags_ |= SELF_HOSTED;
    }

    void setIsSelfHostedConstructor() {
        JS_ASSERT(!isSelfHostedConstructor());
        flags_ |= SELF_HOSTED_CTOR;
    }

    void makeWrappable() {
        JS_ASSERT(isSelfHostedBuiltin());
        JS_ASSERT(!isWrappable());
        flags_ |= SH_WRAPPABLE;
    }

    void setIsFunctionPrototype() {
        JS_ASSERT(!isFunctionPrototype());
        flags_ |= IS_FUN_PROTO;
    }

    // Can be called multiple times by the parser.
    void setIsExprClosure() {
        flags_ |= EXPR_CLOSURE;
    }

    void setArrow() {
        flags_ |= ARROW;
    }

    JSAtom *atom() const { return hasGuessedAtom() ? nullptr : atom_.get(); }
    js::PropertyName *name() const { return hasGuessedAtom() || !atom_ ? nullptr : atom_->asPropertyName(); }
    void initAtom(JSAtom *atom) { atom_.init(atom); }

    JSAtom *displayAtom() const {
        js::AutoThreadSafeAccess ts(this);
        return atom_;
    }

    void setGuessedAtom(JSAtom *atom) {
        JS_ASSERT(atom_ == nullptr);
        JS_ASSERT(atom != nullptr);
        JS_ASSERT(!hasGuessedAtom());
        atom_ = atom;
        flags_ |= HAS_GUESSED_ATOM;
    }

    /* uint16_t representation bounds number of call object dynamic slots. */
    enum { MAX_ARGS_AND_VARS = 2 * ((1U << 16) - 1) };

    /*
     * For an interpreted function, accessors for the initial scope object of
     * activations (stack frames) of the function.
     */
    JSObject *environment() const {
        js::AutoThreadSafeAccess ts(this);
        JS_ASSERT(isInterpreted());
        return u.i.env_;
    }

    void setEnvironment(JSObject *obj) {
        JS_ASSERT(isInterpreted());
        *(js::HeapPtrObject *)&u.i.env_ = obj;
    }

    void initEnvironment(JSObject *obj) {
        JS_ASSERT(isInterpreted());
        ((js::HeapPtrObject *)&u.i.env_)->init(obj);
    }

    static inline size_t offsetOfNargs() { return offsetof(JSFunction, nargs_); }
    static inline size_t offsetOfFlags() { return offsetof(JSFunction, flags_); }
    static inline size_t offsetOfEnvironment() { return offsetof(JSFunction, u.i.env_); }
    static inline size_t offsetOfAtom() { return offsetof(JSFunction, atom_); }

    static bool createScriptForLazilyInterpretedFunction(JSContext *cx, js::HandleFunction fun);
    void relazify(JSTracer *trc);

    // Function Scripts
    //
    // Interpreted functions may either have an explicit JSScript (hasScript())
    // or be lazy with sufficient information to construct the JSScript if
    // necessary (isInterpretedLazy()).
    //
    // A lazy function will have a LazyScript if the function came from parsed
    // source, or nullptr if the function is a clone of a self hosted function.
    //
    // There are several methods to get the script of an interpreted function:
    //
    // - For all interpreted functions, getOrCreateScript() will get the
    //   JSScript, delazifying the function if necessary. This is the safest to
    //   use, but has extra checks, requires a cx and may trigger a GC.
    //
    // - For functions which may have a LazyScript but whose JSScript is known
    //   to exist, existingScript() will get the script and delazify the
    //   function if necessary.
    //
    // - For functions known to have a JSScript, nonLazyScript() will get it.

    JSScript *getOrCreateScript(JSContext *cx) {
        JS_ASSERT(isInterpreted());
        JS_ASSERT(cx);
        if (isInterpretedLazy()) {
            JS::RootedFunction self(cx, this);
            if (!createScriptForLazilyInterpretedFunction(cx, self))
                return nullptr;
            return self->nonLazyScript();
        }
        return nonLazyScript();
    }

    JSScript *existingScript() {
        JS_ASSERT(isInterpreted());
        if (isInterpretedLazy()) {
            js::LazyScript *lazy = lazyScript();
            JSScript *script = lazy->maybeScript();
            JS_ASSERT(script);

            if (shadowZone()->needsBarrier())
                js::LazyScript::writeBarrierPre(lazy);

            flags_ &= ~INTERPRETED_LAZY;
            flags_ |= INTERPRETED;
            initScript(script);
        }
        return nonLazyScript();
    }

    JSScript *nonLazyScript() const {
        js::AutoThreadSafeAccess ts(this);
        JS_ASSERT(js::CurrentThreadCanReadCompilationData());
        JS_ASSERT(hasScript());
        JS_ASSERT(u.i.s.script_);
        return u.i.s.script_;
    }

    // Returns non-callsited-clone version of this.  Use when return
    // value can flow to arbitrary JS (see Bug 944975).
    JSFunction* originalFunction() {
        if (this->hasScript() && this->nonLazyScript()->isCallsiteClone()) {
            return this->nonLazyScript()->donorFunction();
        } else {
            return this;
        }
    }

    js::HeapPtrScript &mutableScript() {
        JS_ASSERT(isInterpreted());
        return *(js::HeapPtrScript *)&u.i.s.script_;
    }

    js::LazyScript *lazyScript() const {
        js::AutoThreadSafeAccess ts(this);
        JS_ASSERT(isInterpretedLazy() && u.i.s.lazy_);
        JS_ASSERT(js::CurrentThreadCanReadCompilationData());
        return u.i.s.lazy_;
    }

    js::LazyScript *lazyScriptOrNull() const {
        js::AutoThreadSafeAccess ts(this);
        JS_ASSERT(isInterpretedLazy());
        JS_ASSERT(js::CurrentThreadCanReadCompilationData());
        return u.i.s.lazy_;
    }

    js::GeneratorKind generatorKind() const {
        if (!isInterpreted())
            return js::NotGenerator;
        if (hasScript())
            return nonLazyScript()->generatorKind();
        if (js::LazyScript *lazy = lazyScriptOrNull())
            return lazy->generatorKind();
        JS_ASSERT(isSelfHostedBuiltin());
        return js::NotGenerator;
    }

    bool isGenerator() const { return generatorKind() != js::NotGenerator; }

    bool isLegacyGenerator() const { return generatorKind() == js::LegacyGenerator; }

    bool isStarGenerator() const { return generatorKind() == js::StarGenerator; }

    void setScript(JSScript *script_) {
        JS_ASSERT(hasScript());
        mutableScript() = script_;
    }

    void initScript(JSScript *script_) {
        JS_ASSERT(hasScript());
        mutableScript().init(script_);
    }

    void setUnlazifiedScript(JSScript *script) {
        // Note: createScriptForLazilyInterpretedFunction triggers a barrier on
        // lazy script before it is overwritten here.
        JS_ASSERT(js::CurrentThreadCanWriteCompilationData());
        JS_ASSERT(isInterpretedLazy());
        if (!lazyScript()->maybeScript())
            lazyScript()->initScript(script);
        flags_ &= ~INTERPRETED_LAZY;
        flags_ |= INTERPRETED;
        initScript(script);
    }

    void initLazyScript(js::LazyScript *lazy) {
        JS_ASSERT(isInterpreted());
        flags_ &= ~INTERPRETED;
        flags_ |= INTERPRETED_LAZY;
        u.i.s.lazy_ = lazy;
    }

    JSNative native() const {
        js::AutoThreadSafeAccess ts(this);
        JS_ASSERT(isNative());
        return u.n.native;
    }

    JSNative maybeNative() const {
        return isInterpreted() ? nullptr : native();
    }

    JSParallelNative parallelNative() const {
        JS_ASSERT(hasParallelNative());
        return jitInfo()->parallelNative;
    }

    JSParallelNative maybeParallelNative() const {
        return hasParallelNative() ? parallelNative() : nullptr;
    }

    void initNative(js::Native native, const JSJitInfo *jitinfo) {
        JS_ASSERT(native);
        u.n.native = native;
        u.n.jitinfo = jitinfo;
    }

    const JSJitInfo *jitInfo() const {
        js::AutoThreadSafeAccess ts(this);
        JS_ASSERT(isNative());
        return u.n.jitinfo;
    }

    void setJitInfo(const JSJitInfo *data) {
        JS_ASSERT(isNative());
        u.n.jitinfo = data;
    }

    static unsigned offsetOfNativeOrScript() {
        JS_STATIC_ASSERT(offsetof(U, n.native) == offsetof(U, i.s.script_));
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

    JSObject *getBoundFunctionTarget() const {
        JS_ASSERT(isBoundFunction());

        /* Bound functions abuse |parent| to store their target function. */
        return getParent();
    }

    inline const js::Value &getBoundFunctionThis() const;
    inline const js::Value &getBoundFunctionArgument(unsigned which) const;
    inline size_t getBoundFunctionArgumentCount() const;

  private:
    inline js::FunctionExtended *toExtended();
    inline const js::FunctionExtended *toExtended() const;

  public:
    inline bool isExtended() const {
        JS_STATIC_ASSERT(FinalizeKind != ExtendedFinalizeKind);
        JS_ASSERT_IF(isTenured(), !!(flags() & EXTENDED) == (tenuredGetAllocKind() == ExtendedFinalizeKind));
        return !!(flags() & EXTENDED);
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
    static bool setTypeForScriptedFunction(js::ExclusiveContext *cx, js::HandleFunction fun,
                                           bool singleton = false);

    /* GC support. */
    js::gc::AllocKind getAllocKind() const {
        js::gc::AllocKind kind = FinalizeKind;
        if (isExtended())
            kind = ExtendedFinalizeKind;
        JS_ASSERT_IF(isTenured(), kind == tenuredGetAllocKind());
        return kind;
    }
};

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

extern bool
Function(JSContext *cx, unsigned argc, Value *vp);

extern bool
Generator(JSContext *cx, unsigned argc, Value *vp);

extern JSFunction *
NewFunction(ExclusiveContext *cx, HandleObject funobj, JSNative native, unsigned nargs,
            JSFunction::Flags flags, HandleObject parent, HandleAtom atom,
            gc::AllocKind allocKind = JSFunction::FinalizeKind,
            NewObjectKind newKind = GenericObject);

// If proto is nullptr, Function.prototype is used instead.
extern JSFunction *
NewFunctionWithProto(ExclusiveContext *cx, HandleObject funobj, JSNative native, unsigned nargs,
                     JSFunction::Flags flags, HandleObject parent, HandleAtom atom,
                     JSObject *proto, gc::AllocKind allocKind = JSFunction::FinalizeKind,
                     NewObjectKind newKind = GenericObject);

extern JSFunction *
DefineFunction(JSContext *cx, HandleObject obj, HandleId id, JSNative native,
               unsigned nargs, unsigned flags,
               gc::AllocKind allocKind = JSFunction::FinalizeKind,
               NewObjectKind newKind = GenericObject);

bool
FunctionHasResolveHook(const JSAtomState &atomState, PropertyName *name);

extern bool
fun_resolve(JSContext *cx, HandleObject obj, HandleId id,
            unsigned flags, MutableHandleObject objp);

// ES6 9.2.5 IsConstructor
bool IsConstructor(const Value &v);

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
                    gc::AllocKind kind = JSFunction::FinalizeKind,
                    NewObjectKind newKindArg = GenericObject);

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

inline void
JSFunction::initializeExtended()
{
    JS_ASSERT(isExtended());

    JS_ASSERT(mozilla::ArrayLength(toExtended()->extendedSlots) == 2);
    toExtended()->extendedSlots[0].init(js::UndefinedValue());
    toExtended()->extendedSlots[1].init(js::UndefinedValue());
}

inline void
JSFunction::initExtendedSlot(size_t which, const js::Value &val)
{
    JS_ASSERT(which < mozilla::ArrayLength(toExtended()->extendedSlots));
    toExtended()->extendedSlots[which].init(val);
}

inline void
JSFunction::setExtendedSlot(size_t which, const js::Value &val)
{
    JS_ASSERT(which < mozilla::ArrayLength(toExtended()->extendedSlots));
    toExtended()->extendedSlots[which] = val;
}

inline const js::Value &
JSFunction::getExtendedSlot(size_t which) const
{
    JS_ASSERT(which < mozilla::ArrayLength(toExtended()->extendedSlots));
    return toExtended()->extendedSlots[which];
}

namespace js {

JSString *FunctionToString(JSContext *cx, HandleFunction fun, bool bodyOnly, bool lambdaParen);

template<XDRMode mode>
bool
XDRInterpretedFunction(XDRState<mode> *xdr, HandleObject enclosingScope,
                       HandleScript enclosingScript, MutableHandleObject objp);

extern JSObject *
CloneFunctionAndScript(JSContext *cx, HandleObject enclosingScope, HandleFunction fun);

/*
 * Report an error that call.thisv is not compatible with the specified class,
 * assuming that the method (clasp->name).prototype.<name of callee function>
 * is what was called.
 */
extern void
ReportIncompatibleMethod(JSContext *cx, CallReceiver call, const Class *clasp);

/*
 * Report an error that call.thisv is not an acceptable this for the callee
 * function.
 */
extern void
ReportIncompatible(JSContext *cx, CallReceiver call);

bool
CallOrConstructBoundFunction(JSContext *, unsigned, js::Value *);

extern const JSFunctionSpec function_methods[];

} /* namespace js */

extern bool
js_fun_apply(JSContext *cx, unsigned argc, js::Value *vp);

extern bool
js_fun_call(JSContext *cx, unsigned argc, js::Value *vp);

extern JSObject*
js_fun_bind(JSContext *cx, js::HandleObject target, js::HandleValue thisArg,
            js::Value *boundArgs, unsigned argslen);

#ifdef DEBUG
namespace JS {
namespace detail {

JS_PUBLIC_API(void)
CheckIsValidConstructible(Value calleev);

} // namespace detail
} // namespace JS
#endif

#endif /* jsfun_h */

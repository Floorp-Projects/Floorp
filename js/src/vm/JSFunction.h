/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSFunction_h
#define vm_JSFunction_h

/*
 * JS function definitions.
 */

#include "jstypes.h"

#include "vm/JSObject.h"
#include "vm/JSScript.h"

namespace js {

class FunctionExtended;

typedef JSNative Native;
}  // namespace js

static const uint32_t JSSLOT_BOUND_FUNCTION_TARGET = 2;
static const uint32_t JSSLOT_BOUND_FUNCTION_THIS = 3;
static const uint32_t JSSLOT_BOUND_FUNCTION_ARGS = 4;

static const char FunctionConstructorMedialSigils[] = ") {\n";
static const char FunctionConstructorFinalBrace[] = "\n}";

enum class FunctionPrefixKind { None, Get, Set };

class JSFunction : public js::NativeObject {
 public:
  static const js::Class class_;

  enum FunctionKind {
    NormalFunction = 0,
    Arrow,  /* ES6 '(args) => body' syntax */
    Method, /* ES6 MethodDefinition */
    ClassConstructor,
    Getter,
    Setter,
    AsmJS, /* function is an asm.js module or exported function */
    Wasm,  /* function is an exported WebAssembly function */
    FunctionKindLimit
  };

  enum Flags {
    INTERPRETED = 0x0001, /* function has a JSScript and environment. */
    CONSTRUCTOR = 0x0002, /* function that can be called as a constructor */
    EXTENDED = 0x0004,    /* structure is FunctionExtended */
    BOUND_FUN = 0x0008, /* function was created with Function.prototype.bind. */
    WASM_JIT_ENTRY = 0x0010,   /* the wasm function has a jit entry */
    HAS_GUESSED_ATOM = 0x0020, /* function had no explicit name, but a
                                  name was guessed for it anyway. See
                                  atom_ for more info about this flag. */
    HAS_BOUND_FUNCTION_NAME_PREFIX =
        0x0020,      /* bound functions reuse the HAS_GUESSED_ATOM
                        flag to track if atom_ already contains the
                        "bound " function name prefix */
    LAMBDA = 0x0040, /* function comes from a FunctionExpression, ArrowFunction,
                        or Function() call (not a FunctionDeclaration or
                        nonstandard function-statement) */
    SELF_HOSTED =
        0x0080, /* On an interpreted function, indicates a self-hosted builtin,
                   which must not be decompilable nor constructible. On a native
                   function, indicates an 'intrinsic', intended for use from
                   self-hosted code only. */
    HAS_INFERRED_NAME = 0x0100, /* function had no explicit name, but a name was
                                   set by SetFunctionName at compile time or
                                   SetFunctionName at runtime. See atom_ for
                                   more info about this flag. */
    INTERPRETED_LAZY =
        0x0200, /* function is interpreted but doesn't have a script yet */
    RESOLVED_LENGTH =
        0x0400,             /* f.length has been resolved (see fun_resolve). */
    RESOLVED_NAME = 0x0800, /* f.name has been resolved (see fun_resolve). */
    NEW_SCRIPT_CLEARED =
        0x1000, /* For a function used as an interpreted constructor, whether
                   a 'new' type had constructor information cleared. */

    FUNCTION_KIND_SHIFT = 13,
    FUNCTION_KIND_MASK = 0x7 << FUNCTION_KIND_SHIFT,

    ASMJS_KIND = AsmJS << FUNCTION_KIND_SHIFT,
    WASM_KIND = Wasm << FUNCTION_KIND_SHIFT,
    ARROW_KIND = Arrow << FUNCTION_KIND_SHIFT,
    METHOD_KIND = Method << FUNCTION_KIND_SHIFT,
    CLASSCONSTRUCTOR_KIND = ClassConstructor << FUNCTION_KIND_SHIFT,
    GETTER_KIND = Getter << FUNCTION_KIND_SHIFT,
    SETTER_KIND = Setter << FUNCTION_KIND_SHIFT,

    /* Derived Flags values for convenience: */
    NATIVE_FUN = 0,
    NATIVE_CTOR = NATIVE_FUN | CONSTRUCTOR,
    NATIVE_CLASS_CTOR = NATIVE_FUN | CONSTRUCTOR | CLASSCONSTRUCTOR_KIND,
    ASMJS_CTOR = ASMJS_KIND | NATIVE_CTOR,
    ASMJS_LAMBDA_CTOR = ASMJS_KIND | NATIVE_CTOR | LAMBDA,
    WASM = WASM_KIND | NATIVE_FUN,
    INTERPRETED_METHOD = INTERPRETED | METHOD_KIND,
    INTERPRETED_CLASS_CONSTRUCTOR =
        INTERPRETED | CLASSCONSTRUCTOR_KIND | CONSTRUCTOR,
    INTERPRETED_GETTER = INTERPRETED | GETTER_KIND,
    INTERPRETED_SETTER = INTERPRETED | SETTER_KIND,
    INTERPRETED_LAMBDA = INTERPRETED | LAMBDA | CONSTRUCTOR,
    INTERPRETED_LAMBDA_ARROW = INTERPRETED | LAMBDA | ARROW_KIND,
    INTERPRETED_LAMBDA_GENERATOR_OR_ASYNC = INTERPRETED | LAMBDA,
    INTERPRETED_NORMAL = INTERPRETED | CONSTRUCTOR,
    INTERPRETED_GENERATOR_OR_ASYNC = INTERPRETED,
    NO_XDR_FLAGS = RESOLVED_LENGTH | RESOLVED_NAME,

    /* Flags preserved when cloning a function.
       (Exception: js::MakeDefaultConstructor produces default constructors for
       ECMAScript classes by cloning self-hosted functions, and then clearing
       their SELF_HOSTED bit, setting their CONSTRUCTOR bit, and otherwise
       munging them to look like they originated with the class definition.) */
    STABLE_ACROSS_CLONES =
        CONSTRUCTOR | LAMBDA | SELF_HOSTED | FUNCTION_KIND_MASK
  };

  static_assert((INTERPRETED | INTERPRETED_LAZY) ==
                    js::JS_FUNCTION_INTERPRETED_BITS,
                "jsfriendapi.h's JSFunction::INTERPRETED-alike is wrong");
  static_assert(((FunctionKindLimit - 1) << FUNCTION_KIND_SHIFT) <=
                    FUNCTION_KIND_MASK,
                "FunctionKind doesn't fit into flags_");

 private:
  /*
   * number of formal arguments
   * (including defaults and the rest parameter unlike f.length)
   */
  uint16_t nargs_;

  /*
   * Bitfield composed of the above Flags enum, as well as the kind.
   *
   * If any of these flags needs to be accessed in off-thread JIT
   * compilation, copy it to js::jit::WrappedFunction.
   */
  uint16_t flags_;
  union U {
    class {
      friend class JSFunction;
      js::Native func_; /* native method pointer or null */
      union {
        // Information about this function to be used by the JIT, only
        // used if isBuiltinNative(); use the accessor!
        const JSJitInfo* jitInfo_;
        // for wasm/asm.js without a jit entry
        size_t wasmFuncIndex_;
        // for wasm that has been given a jit entry
        void** wasmJitEntry_;
      } extra;
    } native;
    struct {
      JSObject* env_; /* environment for new activations */
      union {
        JSScript* script_;     /* interpreted bytecode descriptor or
                                  null; use the accessor! */
        js::LazyScript* lazy_; /* lazily compiled script, or nullptr */
      } s;
    } scripted;
  } u;

  // The |atom_| field can have different meanings depending on the function
  // type and flags. It is used for diagnostics, decompiling, and
  //
  // 1. If the function is not a bound function:
  //   a. If HAS_GUESSED_ATOM is not set, to store the initial value of the
  //      "name" property of functions. But also see RESOLVED_NAME.
  //   b. If HAS_GUESSED_ATOM is set, |atom_| is only used for diagnostics,
  //      but must not be used for the "name" property.
  //   c. If HAS_INFERRED_NAME is set, the function wasn't given an explicit
  //      name in the source text, e.g. |function fn(){}|, but instead it
  //      was inferred based on how the function was defined in the source
  //      text. The exact name inference rules are defined in the ECMAScript
  //      specification.
  //      Name inference can happen at compile-time, for example in
  //      |var fn = function(){}|, or it can happen at runtime, for example
  //      in |var o = {[Symbol.iterator]: function(){}}|. When it happens at
  //      compile-time, the HAS_INFERRED_NAME is set directly in the
  //      bytecode emitter, when it happens at runtime, the flag is set when
  //      evaluating the JSOP_SETFUNNAME bytecode.
  //   d. HAS_GUESSED_ATOM and HAS_INFERRED_NAME cannot both be set.
  //   e. |atom_| can be null if neither an explicit, nor inferred, nor a
  //      guessed name was set.
  //   f. HAS_INFERRED_NAME can be set for cloned singleton function, even
  //      though the clone shouldn't receive an inferred name. See the
  //      comments in NewFunctionClone() and SetFunctionName() for details.
  //
  // 2. If the function is a bound function:
  //   a. To store the initial value of the "name" property.
  //   b. If HAS_BOUND_FUNCTION_NAME_PREFIX is not set, |atom_| doesn't
  //      contain the "bound " prefix which is prepended to the "name"
  //      property of bound functions per ECMAScript.
  //   c. Bound functions can never have an inferred or guessed name.
  //   d. |atom_| is never null for bound functions.
  js::GCPtrAtom atom_;

 public:
  static inline JS::Result<JSFunction*, JS::OOM&> create(
      JSContext* cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
      js::HandleShape shape, js::HandleObjectGroup group);

  /* Call objects must be created for each invocation of this function. */
  bool needsCallObject() const {
    MOZ_ASSERT(!isInterpretedLazy());

    if (isNative()) {
      return false;
    }

    // Note: this should be kept in sync with
    // FunctionBox::needsCallObjectRegardlessOfBindings().
    MOZ_ASSERT_IF(nonLazyScript()->funHasExtensibleScope() ||
                      nonLazyScript()->needsHomeObject() ||
                      nonLazyScript()->isDerivedClassConstructor() ||
                      isGenerator() || isAsync(),
                  nonLazyScript()->bodyScope()->hasEnvironment());

    return nonLazyScript()->bodyScope()->hasEnvironment();
  }

  bool needsExtraBodyVarEnvironment() const;
  bool needsNamedLambdaEnvironment() const;

  bool needsFunctionEnvironmentObjects() const {
    bool res = nonLazyScript()->needsFunctionEnvironmentObjects();
    MOZ_ASSERT(res == (needsCallObject() || needsNamedLambdaEnvironment()));
    return res;
  }

  bool needsSomeEnvironmentObject() const {
    return needsFunctionEnvironmentObjects() || needsExtraBodyVarEnvironment();
  }

  static constexpr size_t NArgsBits = sizeof(nargs_) * CHAR_BIT;
  size_t nargs() const { return nargs_; }

  uint16_t flags() const { return flags_; }

  FunctionKind kind() const {
    return static_cast<FunctionKind>((flags_ & FUNCTION_KIND_MASK) >>
                                     FUNCTION_KIND_SHIFT);
  }

  /* A function can be classified as either native (C++) or interpreted (JS): */
  bool isInterpreted() const {
    return flags() & (INTERPRETED | INTERPRETED_LAZY);
  }
  bool isNative() const { return !isInterpreted(); }

  bool isConstructor() const { return flags() & CONSTRUCTOR; }

  /* Possible attributes of a native function: */
  bool isAsmJSNative() const {
    MOZ_ASSERT_IF(kind() == AsmJS, isNative());
    return kind() == AsmJS;
  }
  bool isWasm() const {
    MOZ_ASSERT_IF(kind() == Wasm, isNative());
    return kind() == Wasm;
  }
  bool isWasmWithJitEntry() const {
    MOZ_ASSERT_IF(flags() & WASM_JIT_ENTRY, isWasm());
    return flags() & WASM_JIT_ENTRY;
  }
  bool isNativeWithJitEntry() const {
    MOZ_ASSERT_IF(isWasmWithJitEntry(), isNative());
    return isWasmWithJitEntry();
  }
  bool isBuiltinNative() const {
    return isNative() && !isAsmJSNative() && !isWasm();
  }

  /* Possible attributes of an interpreted function: */
  bool isBoundFunction() const { return flags() & BOUND_FUN; }
  bool hasInferredName() const { return flags() & HAS_INFERRED_NAME; }
  bool hasGuessedAtom() const {
    static_assert(HAS_GUESSED_ATOM == HAS_BOUND_FUNCTION_NAME_PREFIX,
                  "HAS_GUESSED_ATOM is unused for bound functions");
    return (flags() & (HAS_GUESSED_ATOM | BOUND_FUN)) == HAS_GUESSED_ATOM;
  }
  bool hasBoundFunctionNamePrefix() const {
    static_assert(
        HAS_BOUND_FUNCTION_NAME_PREFIX == HAS_GUESSED_ATOM,
        "HAS_BOUND_FUNCTION_NAME_PREFIX is only used for bound functions");
    MOZ_ASSERT(isBoundFunction());
    return flags() & HAS_BOUND_FUNCTION_NAME_PREFIX;
  }
  bool isLambda() const { return flags() & LAMBDA; }
  bool isInterpretedLazy() const { return flags() & INTERPRETED_LAZY; }

  // This method doesn't check the non-nullness of u.scripted.s.script_,
  // because it's guaranteed to be non-null when this has INTERPRETED flag,
  // for live JSFunctions.
  //
  // When this JSFunction instance is reached via GC iteration, the above
  // doesn't hold, and hasUncompletedScript should also be checked.
  // (see the comment above hasUncompletedScript for more details).
  bool hasScript() const { return flags() & INTERPRETED; }

  // Arrow functions store their lexical new.target in the first extended slot.
  bool isArrow() const { return kind() == Arrow; }
  // Every class-constructor is also a method.
  bool isMethod() const {
    return kind() == Method || kind() == ClassConstructor;
  }
  bool isClassConstructor() const { return kind() == ClassConstructor; }

  bool isGetter() const { return kind() == Getter; }
  bool isSetter() const { return kind() == Setter; }

  bool allowSuperProperty() const {
    return isMethod() || isGetter() || isSetter();
  }

  bool hasResolvedLength() const { return flags() & RESOLVED_LENGTH; }
  bool hasResolvedName() const { return flags() & RESOLVED_NAME; }

  bool isSelfHostedOrIntrinsic() const { return flags() & SELF_HOSTED; }
  bool isSelfHostedBuiltin() const {
    return isSelfHostedOrIntrinsic() && !isNative();
  }
  bool isIntrinsic() const { return isSelfHostedOrIntrinsic() && isNative(); }

  bool hasJITCode() const {
    if (!hasScript()) {
      return false;
    }

    return nonLazyScript()->hasBaselineScript() ||
           nonLazyScript()->hasIonScript();
  }
  bool hasJitEntry() const { return hasScript() || isNativeWithJitEntry(); }

  /* Compound attributes: */
  bool isBuiltin() const { return isBuiltinNative() || isSelfHostedBuiltin(); }

  bool isNamedLambda() const {
    return isLambda() && displayAtom() && !hasInferredName() &&
           !hasGuessedAtom();
  }

  bool hasLexicalThis() const { return isArrow(); }

  bool isBuiltinFunctionConstructor();
  bool needsPrototypeProperty();

  /* Returns the strictness of this function, which must be interpreted. */
  bool strict() const {
    MOZ_ASSERT(isInterpreted());
    return isInterpretedLazy() ? lazyScript()->strict()
                               : nonLazyScript()->strict();
  }

  void setFlags(uint16_t flags) { this->flags_ = flags; }
  void setKind(FunctionKind kind) {
    this->flags_ &= ~FUNCTION_KIND_MASK;
    this->flags_ |= static_cast<uint16_t>(kind) << FUNCTION_KIND_SHIFT;
  }

  // Make the function constructible.
  void setIsConstructor() {
    MOZ_ASSERT(!isConstructor());
    MOZ_ASSERT(isSelfHostedBuiltin());
    flags_ |= CONSTRUCTOR;
  }

  void setIsClassConstructor() {
    MOZ_ASSERT(!isClassConstructor());
    MOZ_ASSERT(isConstructor());

    setKind(ClassConstructor);
  }

  void clearIsSelfHosted() { flags_ &= ~SELF_HOSTED; }

  // Can be called multiple times by the parser.
  void setArgCount(uint16_t nargs) { this->nargs_ = nargs; }

  void setIsBoundFunction() {
    MOZ_ASSERT(!isBoundFunction());
    flags_ |= BOUND_FUN;
  }

  void setIsSelfHostedBuiltin() {
    MOZ_ASSERT(isInterpreted());
    MOZ_ASSERT(!isSelfHostedBuiltin());
    flags_ |= SELF_HOSTED;
    // Self-hosted functions should not be constructable.
    flags_ &= ~CONSTRUCTOR;
  }
  void setIsIntrinsic() {
    MOZ_ASSERT(isNative());
    MOZ_ASSERT(!isIntrinsic());
    flags_ |= SELF_HOSTED;
  }

  void setArrow() { setKind(Arrow); }

  void setResolvedLength() { flags_ |= RESOLVED_LENGTH; }

  void setResolvedName() { flags_ |= RESOLVED_NAME; }

  // Mark a function as having its 'new' script information cleared.
  bool wasNewScriptCleared() const { return flags_ & NEW_SCRIPT_CLEARED; }
  void setNewScriptCleared() { flags_ |= NEW_SCRIPT_CLEARED; }

  static bool getUnresolvedLength(JSContext* cx, js::HandleFunction fun,
                                  js::MutableHandleValue v);

  JSAtom* infallibleGetUnresolvedName(JSContext* cx);

  static bool getUnresolvedName(JSContext* cx, js::HandleFunction fun,
                                js::MutableHandleString v);

  static JSLinearString* getBoundFunctionName(JSContext* cx,
                                              js::HandleFunction fun);

  JSAtom* explicitName() const {
    return (hasInferredName() || hasGuessedAtom()) ? nullptr : atom_.get();
  }
  JSAtom* explicitOrInferredName() const {
    return hasGuessedAtom() ? nullptr : atom_.get();
  }

  void initAtom(JSAtom* atom) {
    MOZ_ASSERT_IF(atom, js::AtomIsMarked(zone(), atom));
    atom_.init(atom);
  }

  void setAtom(JSAtom* atom) {
    MOZ_ASSERT_IF(atom, js::AtomIsMarked(zone(), atom));
    atom_ = atom;
  }

  JSAtom* displayAtom() const { return atom_; }

  void setInferredName(JSAtom* atom) {
    MOZ_ASSERT(!atom_);
    MOZ_ASSERT(atom);
    MOZ_ASSERT(!hasGuessedAtom());
    setAtom(atom);
    flags_ |= HAS_INFERRED_NAME;
  }
  void clearInferredName() {
    MOZ_ASSERT(hasInferredName());
    MOZ_ASSERT(atom_);
    setAtom(nullptr);
    flags_ &= ~HAS_INFERRED_NAME;
  }
  JSAtom* inferredName() const {
    MOZ_ASSERT(hasInferredName());
    MOZ_ASSERT(atom_);
    return atom_;
  }

  void setGuessedAtom(JSAtom* atom) {
    MOZ_ASSERT(!atom_);
    MOZ_ASSERT(atom);
    MOZ_ASSERT(!hasInferredName());
    MOZ_ASSERT(!hasGuessedAtom());
    MOZ_ASSERT(!isBoundFunction());
    setAtom(atom);
    flags_ |= HAS_GUESSED_ATOM;
  }
  void clearGuessedAtom() {
    MOZ_ASSERT(hasGuessedAtom());
    MOZ_ASSERT(!isBoundFunction());
    MOZ_ASSERT(atom_);
    setAtom(nullptr);
    flags_ &= ~HAS_GUESSED_ATOM;
  }

  void setPrefixedBoundFunctionName(JSAtom* atom) {
    MOZ_ASSERT(!hasBoundFunctionNamePrefix());
    MOZ_ASSERT(atom);
    flags_ |= HAS_BOUND_FUNCTION_NAME_PREFIX;
    setAtom(atom);
  }

  /* uint16_t representation bounds number of call object dynamic slots. */
  enum { MAX_ARGS_AND_VARS = 2 * ((1U << 16) - 1) };

  /*
   * For an interpreted function, accessors for the initial scope object of
   * activations (stack frames) of the function.
   */
  JSObject* environment() const {
    MOZ_ASSERT(isInterpreted());
    return u.scripted.env_;
  }

  void setEnvironment(JSObject* obj) {
    MOZ_ASSERT(isInterpreted());
    *reinterpret_cast<js::GCPtrObject*>(&u.scripted.env_) = obj;
  }

  void initEnvironment(JSObject* obj) {
    MOZ_ASSERT(isInterpreted());
    reinterpret_cast<js::GCPtrObject*>(&u.scripted.env_)->init(obj);
  }

  void unsetEnvironment() { setEnvironment(nullptr); }

 public:
  static constexpr size_t offsetOfNargs() {
    return offsetof(JSFunction, nargs_);
  }
  static constexpr size_t offsetOfFlags() {
    return offsetof(JSFunction, flags_);
  }
  static size_t offsetOfEnvironment() {
    return offsetof(JSFunction, u.scripted.env_);
  }
  static size_t offsetOfAtom() { return offsetof(JSFunction, atom_); }

  static bool createScriptForLazilyInterpretedFunction(JSContext* cx,
                                                       js::HandleFunction fun);
  void maybeRelazify(JSRuntime* rt);

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
  // - For inlined functions which may have a LazyScript but whose JSScript
  //   is known to exist, existingScript() will get the script and delazify
  //   the function if necessary. If the function should not be delazified,
  //   use existingScriptNonDelazifying().
  //
  // - For functions known to have a JSScript, nonLazyScript() will get it.

  static JSScript* getOrCreateScript(JSContext* cx, js::HandleFunction fun) {
    MOZ_ASSERT(fun->isInterpreted());
    MOZ_ASSERT(cx);
    if (fun->isInterpretedLazy()) {
      if (!createScriptForLazilyInterpretedFunction(cx, fun)) {
        return nullptr;
      }
      return fun->nonLazyScript();
    }
    return fun->nonLazyScript();
  }

  JSScript* existingScriptNonDelazifying() const {
    MOZ_ASSERT(isInterpreted());
    if (isInterpretedLazy()) {
      // Get the script from the canonical function. Ion used the
      // canonical function to inline the script and because it has
      // Baseline code it has not been relazified. Note that we can't
      // use lazyScript->script_ here as it may be null in some cases,
      // see bug 976536.
      js::LazyScript* lazy = lazyScript();
      JSFunction* fun = lazy->functionNonDelazifying();
      MOZ_ASSERT(fun);
      return fun->nonLazyScript();
    }
    return nonLazyScript();
  }

  JSScript* existingScript() {
    MOZ_ASSERT(isInterpreted());
    if (isInterpretedLazy()) {
      if (shadowZone()->needsIncrementalBarrier()) {
        js::LazyScript::writeBarrierPre(lazyScript());
      }
      JSScript* script = existingScriptNonDelazifying();
      flags_ &= ~INTERPRETED_LAZY;
      flags_ |= INTERPRETED;
      initScript(script);
    }
    return nonLazyScript();
  }

  // If this is a scripted function, returns its canonical function (the
  // original function allocated by the frontend). Note that lazy self-hosted
  // builtins don't have a lazy script so in that case we also return nullptr.
  JSFunction* maybeCanonicalFunction() const {
    if (hasScript()) {
      return nonLazyScript()->functionNonDelazifying();
    }
    if (isInterpretedLazy() && !isSelfHostedBuiltin()) {
      return lazyScript()->functionNonDelazifying();
    }
    return nullptr;
  }

  // The state of a JSFunction whose script errored out during bytecode
  // compilation. Such JSFunctions are only reachable via GC iteration and
  // not from script.
  // If u.scripted.s.script_ is non-null, the pointed JSScript is guaranteed
  // to be complete (see the comment above JSScript::initFromFunctionBox
  // callsite in JSScript::fullyInitFromEmitter).
  bool hasUncompletedScript() const {
    MOZ_ASSERT(hasScript());
    return !u.scripted.s.script_;
  }

  JSScript* nonLazyScript() const {
    MOZ_ASSERT(!hasUncompletedScript());
    return u.scripted.s.script_;
  }

  static bool getLength(JSContext* cx, js::HandleFunction fun,
                        uint16_t* length);

  js::LazyScript* lazyScript() const {
    MOZ_ASSERT(isInterpretedLazy() && u.scripted.s.lazy_);
    return u.scripted.s.lazy_;
  }

  js::LazyScript* lazyScriptOrNull() const {
    MOZ_ASSERT(isInterpretedLazy());
    return u.scripted.s.lazy_;
  }

  js::GeneratorKind generatorKind() const {
    if (!isInterpreted()) {
      return js::GeneratorKind::NotGenerator;
    }
    if (hasScript()) {
      return nonLazyScript()->generatorKind();
    }
    if (js::LazyScript* lazy = lazyScriptOrNull()) {
      return lazy->generatorKind();
    }
    MOZ_ASSERT(isSelfHostedBuiltin());
    return js::GeneratorKind::NotGenerator;
  }

  bool isGenerator() const {
    return generatorKind() == js::GeneratorKind::Generator;
  }

  js::FunctionAsyncKind asyncKind() const {
    if (!isInterpreted()) {
      return js::FunctionAsyncKind::SyncFunction;
    }
    if (hasScript()) {
      return nonLazyScript()->asyncKind();
    }
    if (js::LazyScript* lazy = lazyScriptOrNull()) {
      return lazy->asyncKind();
    }
    MOZ_ASSERT(isSelfHostedBuiltin());
    return js::FunctionAsyncKind::SyncFunction;
  }

  bool isAsync() const {
    return asyncKind() == js::FunctionAsyncKind::AsyncFunction;
  }

  void setScript(JSScript* script) {
    MOZ_ASSERT(realm() == script->realm());
    mutableScript() = script;
  }

  void initScript(JSScript* script) {
    MOZ_ASSERT_IF(script, realm() == script->realm());
    mutableScript().init(script);
  }

  void setUnlazifiedScript(JSScript* script) {
    MOZ_ASSERT(isInterpretedLazy());
    if (lazyScriptOrNull()) {
      // Trigger a pre barrier on the lazy script being overwritten.
      js::LazyScript::writeBarrierPre(lazyScriptOrNull());
      if (!lazyScript()->maybeScript()) {
        lazyScript()->initScript(script);
      }
    }
    flags_ &= ~INTERPRETED_LAZY;
    flags_ |= INTERPRETED;
    initScript(script);
  }

  void initLazyScript(js::LazyScript* lazy) {
    MOZ_ASSERT(isInterpreted());
    flags_ &= ~INTERPRETED;
    flags_ |= INTERPRETED_LAZY;
    u.scripted.s.lazy_ = lazy;
  }

  JSNative native() const {
    MOZ_ASSERT(isNative());
    return u.native.func_;
  }

  JSNative maybeNative() const { return isInterpreted() ? nullptr : native(); }

  void initNative(js::Native native, const JSJitInfo* jitInfo) {
    MOZ_ASSERT(isNative());
    MOZ_ASSERT_IF(jitInfo, isBuiltinNative());
    MOZ_ASSERT(native);
    u.native.func_ = native;
    u.native.extra.jitInfo_ = jitInfo;
  }
  bool hasJitInfo() const {
    return isBuiltinNative() && u.native.extra.jitInfo_;
  }
  const JSJitInfo* jitInfo() const {
    MOZ_ASSERT(hasJitInfo());
    return u.native.extra.jitInfo_;
  }
  void setJitInfo(const JSJitInfo* data) {
    MOZ_ASSERT(isBuiltinNative());
    u.native.extra.jitInfo_ = data;
  }

  // wasm functions are always natives and either:
  //  - store a function-index in u.n.extra and can only be called through the
  //    fun->native() entry point from C++.
  //  - store a jit-entry code pointer in u.n.extra and can be called by jit
  //    code directly. C++ callers can still use the fun->native() entry point
  //    (computing the function index from the jit-entry point).
  void setWasmFuncIndex(uint32_t funcIndex) {
    MOZ_ASSERT(isWasm() || isAsmJSNative());
    MOZ_ASSERT(!isWasmWithJitEntry());
    MOZ_ASSERT(!u.native.extra.wasmFuncIndex_);
    u.native.extra.wasmFuncIndex_ = funcIndex;
  }
  uint32_t wasmFuncIndex() const {
    MOZ_ASSERT(isWasm() || isAsmJSNative());
    MOZ_ASSERT(!isWasmWithJitEntry());
    return u.native.extra.wasmFuncIndex_;
  }
  void setWasmJitEntry(void** entry) {
    MOZ_ASSERT(*entry);
    MOZ_ASSERT(isWasm());
    MOZ_ASSERT(!isWasmWithJitEntry());
    flags_ |= WASM_JIT_ENTRY;
    u.native.extra.wasmJitEntry_ = entry;
    MOZ_ASSERT(isWasmWithJitEntry());
  }
  void** wasmJitEntry() const {
    MOZ_ASSERT(isWasmWithJitEntry());
    MOZ_ASSERT(u.native.extra.wasmJitEntry_);
    return u.native.extra.wasmJitEntry_;
  }

  bool isDerivedClassConstructor();

  static unsigned offsetOfNative() {
    return offsetof(JSFunction, u.native.func_);
  }
  static unsigned offsetOfScript() {
    static_assert(offsetof(U, scripted.s.script_) ==
                      offsetof(U, native.extra.wasmJitEntry_),
                  "scripted.s.script_ must be at the same offset as "
                  "native.extra.wasmJitEntry_");
    return offsetof(JSFunction, u.scripted.s.script_);
  }
  static unsigned offsetOfNativeOrEnv() {
    static_assert(
        offsetof(U, native.func_) == offsetof(U, scripted.env_),
        "U.native.func_ must be at the same offset as U.scripted.env_");
    return offsetOfNative();
  }
  static unsigned offsetOfScriptOrLazyScript() {
    static_assert(
        offsetof(U, scripted.s.script_) == offsetof(U, scripted.s.lazy_),
        "U.scripted.s.script_ must be at the same offset as lazy_");
    return offsetof(JSFunction, u.scripted.s.script_);
  }

  static unsigned offsetOfJitInfo() {
    return offsetof(JSFunction, u.native.extra.jitInfo_);
  }

  inline void trace(JSTracer* trc);

  /* Bound function accessors. */

  JSObject* getBoundFunctionTarget() const;
  const js::Value& getBoundFunctionThis() const;
  const js::Value& getBoundFunctionArgument(unsigned which) const;
  size_t getBoundFunctionArgumentCount() const;

  /*
   * Used to mark bound functions as such and make them constructible if the
   * target is. Also assigns the prototype and sets the name and correct length.
   */
  static bool finishBoundFunctionInit(JSContext* cx, js::HandleFunction bound,
                                      js::HandleObject targetObj,
                                      int32_t argCount);

 private:
  js::GCPtrScript& mutableScript() {
    MOZ_ASSERT(hasScript());
    return *(js::GCPtrScript*)&u.scripted.s.script_;
  }

  inline js::FunctionExtended* toExtended();
  inline const js::FunctionExtended* toExtended() const;

 public:
  inline bool isExtended() const {
    bool extended = !!(flags() & EXTENDED);
    MOZ_ASSERT_IF(isTenured(),
                  extended == (asTenured().getAllocKind() ==
                               js::gc::AllocKind::FUNCTION_EXTENDED));
    return extended;
  }

  /*
   * Accessors for data stored in extended functions. Use setExtendedSlot if
   * the function has already been initialized. Otherwise use
   * initExtendedSlot.
   */
  inline void initializeExtended();
  inline void initExtendedSlot(size_t which, const js::Value& val);
  inline void setExtendedSlot(size_t which, const js::Value& val);
  inline const js::Value& getExtendedSlot(size_t which) const;

  /*
   * Same as `toExtended` and `getExtendedSlot`, but `this` is guaranteed to be
   * an extended function.
   *
   * This function is supposed to be used off-thread, especially the JIT
   * compilation thread, that cannot access JSFunction.flags_, because of
   * a race condition.
   *
   * See Also: WrappedFunction.isExtended_
   */
  inline js::FunctionExtended* toExtendedOffMainThread();
  inline const js::FunctionExtended* toExtendedOffMainThread() const;
  inline const js::Value& getExtendedSlotOffMainThread(size_t which) const;

  /* Constructs a new type for the function if necessary. */
  static bool setTypeForScriptedFunction(JSContext* cx, js::HandleFunction fun,
                                         bool singleton = false);

  /* GC support. */
  js::gc::AllocKind getAllocKind() const {
    static_assert(
        js::gc::AllocKind::FUNCTION != js::gc::AllocKind::FUNCTION_EXTENDED,
        "extended/non-extended AllocKinds have to be different "
        "for getAllocKind() to have a reason to exist");

    js::gc::AllocKind kind = js::gc::AllocKind::FUNCTION;
    if (isExtended()) {
      kind = js::gc::AllocKind::FUNCTION_EXTENDED;
    }
    MOZ_ASSERT_IF(isTenured(), kind == asTenured().getAllocKind());
    return kind;
  }
};

static_assert(sizeof(JSFunction) == sizeof(js::shadow::Function),
              "shadow interface must match actual interface");

extern JSString* fun_toStringHelper(JSContext* cx, js::HandleObject obj,
                                    bool isToSource);

namespace js {

extern bool Function(JSContext* cx, unsigned argc, Value* vp);

extern bool Generator(JSContext* cx, unsigned argc, Value* vp);

extern bool AsyncFunctionConstructor(JSContext* cx, unsigned argc, Value* vp);

extern bool AsyncGeneratorConstructor(JSContext* cx, unsigned argc, Value* vp);

// If enclosingEnv is null, the function will have a null environment()
// (yes, null, not the global).  In all cases, the global will be used as the
// parent.

extern JSFunction* NewFunctionWithProto(
    JSContext* cx, JSNative native, unsigned nargs, JSFunction::Flags flags,
    HandleObject enclosingEnv, HandleAtom atom, HandleObject proto,
    gc::AllocKind allocKind = gc::AllocKind::FUNCTION,
    NewObjectKind newKind = GenericObject);

// Allocate a new function backed by a JSNative.  Note that by default this
// creates a singleton object.
inline JSFunction* NewNativeFunction(
    JSContext* cx, JSNative native, unsigned nargs, HandleAtom atom,
    gc::AllocKind allocKind = gc::AllocKind::FUNCTION,
    NewObjectKind newKind = SingletonObject,
    JSFunction::Flags flags = JSFunction::NATIVE_FUN) {
  MOZ_ASSERT(native);
  return NewFunctionWithProto(cx, native, nargs, flags, nullptr, atom, nullptr,
                              allocKind, newKind);
}

// Allocate a new constructor backed by a JSNative.  Note that by default this
// creates a singleton object.
inline JSFunction* NewNativeConstructor(
    JSContext* cx, JSNative native, unsigned nargs, HandleAtom atom,
    gc::AllocKind allocKind = gc::AllocKind::FUNCTION,
    NewObjectKind newKind = SingletonObject,
    JSFunction::Flags flags = JSFunction::NATIVE_CTOR) {
  MOZ_ASSERT(native);
  MOZ_ASSERT(flags & JSFunction::NATIVE_CTOR);
  return NewFunctionWithProto(cx, native, nargs, flags, nullptr, atom, nullptr,
                              allocKind, newKind);
}

// Allocate a new scripted function.  If enclosingEnv is null, the
// global will be used.  In all cases the parent of the resulting object will be
// the global.
extern JSFunction* NewScriptedFunction(
    JSContext* cx, unsigned nargs, JSFunction::Flags flags, HandleAtom atom,
    HandleObject proto = nullptr,
    gc::AllocKind allocKind = gc::AllocKind::FUNCTION,
    NewObjectKind newKind = GenericObject, HandleObject enclosingEnv = nullptr);

// Determine which [[Prototype]] to use when creating a new function using the
// requested generator and async kind.
//
// This sets `proto` to `nullptr` for non-generator, synchronous functions to
// mean "the builtin %FunctionPrototype% in the current realm", the common case.
//
// We could set it to `cx->global()->getOrCreateFunctionPrototype()`, but
// nullptr gets a fast path in e.g. js::NewObjectWithClassProtoCommon.
extern bool GetFunctionPrototype(JSContext* cx, js::GeneratorKind generatorKind,
                                 js::FunctionAsyncKind asyncKind,
                                 js::MutableHandleObject proto);

extern JSAtom* IdToFunctionName(
    JSContext* cx, HandleId id,
    FunctionPrefixKind prefixKind = FunctionPrefixKind::None);

extern bool SetFunctionName(JSContext* cx, HandleFunction fun, HandleValue name,
                            FunctionPrefixKind prefixKind);

extern JSFunction* DefineFunction(
    JSContext* cx, HandleObject obj, HandleId id, JSNative native,
    unsigned nargs, unsigned flags,
    gc::AllocKind allocKind = gc::AllocKind::FUNCTION);

extern bool fun_toString(JSContext* cx, unsigned argc, Value* vp);

struct WellKnownSymbols;

extern bool FunctionHasDefaultHasInstance(JSFunction* fun,
                                          const WellKnownSymbols& symbols);

extern void ThrowTypeErrorBehavior(JSContext* cx);

/*
 * Function extended with reserved slots for use by various kinds of functions.
 * Most functions do not have these extensions, but enough do that efficient
 * storage is required (no malloc'ed reserved slots).
 */
class FunctionExtended : public JSFunction {
 public:
  static const unsigned NUM_EXTENDED_SLOTS = 2;

  // Arrow functions store their lexical new.target in the first extended
  // slot.
  static const unsigned ARROW_NEWTARGET_SLOT = 0;

  static const unsigned METHOD_HOMEOBJECT_SLOT = 0;

  // Exported asm.js/wasm functions store their WasmInstanceObject in the
  // first slot.
  static const unsigned WASM_INSTANCE_SLOT = 0;

  // wasm/asm.js exported functions store the wasm::TlsData pointer of their
  // instance.
  static const unsigned WASM_TLSDATA_SLOT = 1;

  // asm.js module functions store their WasmModuleObject in the first slot.
  static const unsigned ASMJS_MODULE_SLOT = 0;

  static inline size_t offsetOfExtendedSlot(unsigned which) {
    MOZ_ASSERT(which < NUM_EXTENDED_SLOTS);
    return offsetof(FunctionExtended, extendedSlots) +
           which * sizeof(GCPtrValue);
  }
  static inline size_t offsetOfArrowNewTargetSlot() {
    return offsetOfExtendedSlot(ARROW_NEWTARGET_SLOT);
  }
  static inline size_t offsetOfMethodHomeObjectSlot() {
    return offsetOfExtendedSlot(METHOD_HOMEOBJECT_SLOT);
  }

 private:
  friend class JSFunction;

  /* Reserved slots available for storage by particular native functions. */
  GCPtrValue extendedSlots[NUM_EXTENDED_SLOTS];
};

extern bool CanReuseScriptForClone(JS::Realm* realm, HandleFunction fun,
                                   HandleObject newParent);

extern JSFunction* CloneFunctionReuseScript(
    JSContext* cx, HandleFunction fun, HandleObject parent,
    gc::AllocKind kind = gc::AllocKind::FUNCTION,
    NewObjectKind newKindArg = GenericObject, HandleObject proto = nullptr);

// Functions whose scripts are cloned are always given singleton types.
extern JSFunction* CloneFunctionAndScript(
    JSContext* cx, HandleFunction fun, HandleObject parent,
    HandleScope newScope, Handle<ScriptSourceObject*> sourceObject,
    gc::AllocKind kind = gc::AllocKind::FUNCTION, HandleObject proto = nullptr);

extern JSFunction* CloneAsmJSModuleFunction(JSContext* cx, HandleFunction fun);

extern JSFunction* CloneSelfHostingIntrinsic(JSContext* cx, HandleFunction fun);

}  // namespace js

inline js::FunctionExtended* JSFunction::toExtended() {
  MOZ_ASSERT(isExtended());
  return static_cast<js::FunctionExtended*>(this);
}

inline const js::FunctionExtended* JSFunction::toExtended() const {
  MOZ_ASSERT(isExtended());
  return static_cast<const js::FunctionExtended*>(this);
}

inline js::FunctionExtended* JSFunction::toExtendedOffMainThread() {
  return static_cast<js::FunctionExtended*>(this);
}

inline const js::FunctionExtended* JSFunction::toExtendedOffMainThread() const {
  return static_cast<const js::FunctionExtended*>(this);
}

inline void JSFunction::initializeExtended() {
  MOZ_ASSERT(isExtended());

  MOZ_ASSERT(mozilla::ArrayLength(toExtended()->extendedSlots) == 2);
  toExtended()->extendedSlots[0].init(js::UndefinedValue());
  toExtended()->extendedSlots[1].init(js::UndefinedValue());
}

inline void JSFunction::initExtendedSlot(size_t which, const js::Value& val) {
  MOZ_ASSERT(which < mozilla::ArrayLength(toExtended()->extendedSlots));
  MOZ_ASSERT(js::IsObjectValueInCompartment(val, compartment()));
  toExtended()->extendedSlots[which].init(val);
}

inline void JSFunction::setExtendedSlot(size_t which, const js::Value& val) {
  MOZ_ASSERT(which < mozilla::ArrayLength(toExtended()->extendedSlots));
  MOZ_ASSERT(js::IsObjectValueInCompartment(val, compartment()));
  toExtended()->extendedSlots[which] = val;
}

inline const js::Value& JSFunction::getExtendedSlot(size_t which) const {
  MOZ_ASSERT(which < mozilla::ArrayLength(toExtended()->extendedSlots));
  return toExtended()->extendedSlots[which];
}

inline const js::Value& JSFunction::getExtendedSlotOffMainThread(
    size_t which) const {
  MOZ_ASSERT(which <
             mozilla::ArrayLength(toExtendedOffMainThread()->extendedSlots));
  return toExtendedOffMainThread()->extendedSlots[which];
}

namespace js {

JSString* FunctionToString(JSContext* cx, HandleFunction fun, bool isToSource);

template <XDRMode mode>
XDRResult XDRInterpretedFunction(XDRState<mode>* xdr,
                                 HandleScope enclosingScope,
                                 HandleScriptSourceObject sourceObject,
                                 MutableHandleFunction objp);

/*
 * Report an error that call.thisv is not compatible with the specified class,
 * assuming that the method (clasp->name).prototype.<name of callee function>
 * is what was called.
 */
extern void ReportIncompatibleMethod(JSContext* cx, const CallArgs& args,
                                     const Class* clasp);

/*
 * Report an error that call.thisv is not an acceptable this for the callee
 * function.
 */
extern void ReportIncompatible(JSContext* cx, const CallArgs& args);

extern bool fun_apply(JSContext* cx, unsigned argc, Value* vp);

extern bool fun_call(JSContext* cx, unsigned argc, Value* vp);

} /* namespace js */

#ifdef DEBUG
namespace JS {
namespace detail {

JS_PUBLIC_API void CheckIsValidConstructible(const Value& calleev);

}  // namespace detail
}  // namespace JS
#endif

#endif /* vm_JSFunction_h */

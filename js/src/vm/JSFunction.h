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
struct SelfHostedLazyScript;

using Native = JSNative;

static constexpr uint32_t BoundFunctionEnvTargetSlot = 2;
static constexpr uint32_t BoundFunctionEnvThisSlot = 3;
static constexpr uint32_t BoundFunctionEnvArgsSlot = 4;

static const char FunctionConstructorMedialSigils[] = ") {\n";
static const char FunctionConstructorFinalBrace[] = "\n}";

enum class FunctionPrefixKind { None, Get, Set };

class FunctionFlags {
 public:
  enum FunctionKind {
    NormalFunction = 0,
    Arrow,   // ES6 '(args) => body' syntax
    Method,  // ES6 MethodDefinition
    ClassConstructor,
    Getter,
    Setter,
    AsmJS,  // An asm.js module or exported function
    Wasm,   // An exported WebAssembly function
    FunctionKindLimit
  };

  enum Flags : uint16_t {
    // The general kind of a function. This is used to describe characteristics
    // of functions that do not merit a dedicated flag bit below.
    FUNCTION_KIND_SHIFT = 0,
    FUNCTION_KIND_MASK = 0x0007,

    // The AllocKind used was FunctionExtended and extra slots were allocated.
    // These slots may be used by the engine or the embedding so care must be
    // taken to avoid conflicts.
    EXTENDED = 1 << 3,

    // Set if function is a self-hosted builtin or intrinsic. An 'intrinsic'
    // here means a native function used inside self-hosted code. In general, a
    // self-hosted function should appear to script as though it were a native
    // builtin.
    SELF_HOSTED = 1 << 4,

    // An interpreted function has or may-have bytecode and an environment. Only
    // one of these flags may be used at a time. As a memory optimization, the
    // SELFHOSTLAZY flag indicates there is no js::BaseScript at all and we must
    // clone from the self-hosted realm in order to get bytecode.
    BASESCRIPT = 1 << 5,
    SELFHOSTLAZY = 1 << 6,

    // Function may be called as a constructor. This corresponds in the spec as
    // having a [[Construct]] internal method.
    CONSTRUCTOR = 1 << 7,

    // A 'Bound Function Exotic Object' created by Function.prototype.bind.
    BOUND_FUN = 1 << 8,

    // Function comes from a FunctionExpression, ArrowFunction, or Function()
    // call (not a FunctionDeclaration or nonstandard function-statement).
    LAMBDA = 1 << 9,

    // The WASM function has a JIT entry which emulates the
    // js::BaseScript::jitCodeRaw_ mechanism.
    WASM_JIT_ENTRY = 1 << 10,

    // Function had no explicit name, but a name was set by SetFunctionName at
    // compile time or SetFunctionName at runtime.
    HAS_INFERRED_NAME = 1 << 11,

    // Function had no explicit name, but a name was guessed for it anyway. For
    // a Bound function, tracks if atom_ already contains the "bound " prefix.
    ATOM_EXTRA_FLAG = 1 << 12,
    HAS_GUESSED_ATOM = ATOM_EXTRA_FLAG,
    HAS_BOUND_FUNCTION_NAME_PREFIX = ATOM_EXTRA_FLAG,

    // The 'length' or 'name property has been resolved. See fun_resolve.
    RESOLVED_NAME = 1 << 13,
    RESOLVED_LENGTH = 1 << 14,

    // For a function used as an interpreted constructor, whether a 'new' type
    // had constructor information cleared.
    NEW_SCRIPT_CLEARED = 1 << 15,

    // Shifted form of FunctionKinds.
    NORMAL_KIND = NormalFunction << FUNCTION_KIND_SHIFT,
    ASMJS_KIND = AsmJS << FUNCTION_KIND_SHIFT,
    WASM_KIND = Wasm << FUNCTION_KIND_SHIFT,
    ARROW_KIND = Arrow << FUNCTION_KIND_SHIFT,
    METHOD_KIND = Method << FUNCTION_KIND_SHIFT,
    CLASSCONSTRUCTOR_KIND = ClassConstructor << FUNCTION_KIND_SHIFT,
    GETTER_KIND = Getter << FUNCTION_KIND_SHIFT,
    SETTER_KIND = Setter << FUNCTION_KIND_SHIFT,

    // Derived Flags combinations to use when creating functions.
    NATIVE_FUN = NORMAL_KIND,
    NATIVE_CTOR = CONSTRUCTOR | NORMAL_KIND,
    ASMJS_CTOR = CONSTRUCTOR | ASMJS_KIND,
    ASMJS_LAMBDA_CTOR = CONSTRUCTOR | LAMBDA | ASMJS_KIND,
    WASM = WASM_KIND,
    INTERPRETED_NORMAL = BASESCRIPT | CONSTRUCTOR | NORMAL_KIND,
    INTERPRETED_CLASS_CTOR = BASESCRIPT | CONSTRUCTOR | CLASSCONSTRUCTOR_KIND,
    INTERPRETED_GENERATOR_OR_ASYNC = BASESCRIPT | NORMAL_KIND,
    INTERPRETED_LAMBDA = BASESCRIPT | LAMBDA | CONSTRUCTOR | NORMAL_KIND,
    INTERPRETED_LAMBDA_ARROW = BASESCRIPT | LAMBDA | ARROW_KIND,
    INTERPRETED_LAMBDA_GENERATOR_OR_ASYNC = BASESCRIPT | LAMBDA | NORMAL_KIND,
    INTERPRETED_GETTER = BASESCRIPT | GETTER_KIND,
    INTERPRETED_SETTER = BASESCRIPT | SETTER_KIND,
    INTERPRETED_METHOD = BASESCRIPT | METHOD_KIND,

    // Flags that XDR ignores. See also: js::BaseScript::MutableFlags.
    MUTABLE_FLAGS = RESOLVED_NAME | RESOLVED_LENGTH | NEW_SCRIPT_CLEARED,

    // Flags preserved when cloning a function. (Exception:
    // js::MakeDefaultConstructor produces default constructors for ECMAScript
    // classes by cloning self-hosted functions, and then clearing their
    // SELF_HOSTED bit, setting their CONSTRUCTOR bit, and otherwise munging
    // them to look like they originated with the class definition.) */
    STABLE_ACROSS_CLONES =
        CONSTRUCTOR | LAMBDA | SELF_HOSTED | FUNCTION_KIND_MASK
  };

  uint16_t flags_;

 public:
  FunctionFlags() : flags_() {
    static_assert(sizeof(FunctionFlags) == sizeof(flags_),
                  "No extra members allowed is it'll grow JSFunction");
    static_assert(offsetof(FunctionFlags, flags_) == 0,
                  "Required for JIT flag access");
  }

  explicit FunctionFlags(uint16_t flags) : flags_(flags) {}
  MOZ_IMPLICIT FunctionFlags(Flags f) : flags_(f) {}

  static_assert((BASESCRIPT | SELFHOSTLAZY) == js::JS_FUNCTION_INTERPRETED_BITS,
                "jsfriendapi.h's FunctionFlags::INTERPRETED-alike is wrong");
  static_assert(((FunctionKindLimit - 1) << FUNCTION_KIND_SHIFT) <=
                    FUNCTION_KIND_MASK,
                "FunctionKind doesn't fit into flags_");

  uint16_t toRaw() const { return flags_; }

  uint16_t stableAcrossClones() const { return flags_ & STABLE_ACROSS_CLONES; }

  // For flag combinations the type is int.
  bool hasFlags(uint16_t flags) const { return flags_ & flags; }
  void setFlags(uint16_t flags) { flags_ |= flags; }
  void clearFlags(uint16_t flags) { flags_ &= ~flags; }
  void setFlags(uint16_t flags, bool set) {
    if (set) {
      setFlags(flags);
    } else {
      clearFlags(flags);
    }
  }

  FunctionKind kind() const {
    return static_cast<FunctionKind>((flags_ & FUNCTION_KIND_MASK) >>
                                     FUNCTION_KIND_SHIFT);
  }

  /* A function can be classified as either native (C++) or interpreted (JS): */
  bool isInterpreted() const {
    return hasFlags(BASESCRIPT) || hasFlags(SELFHOSTLAZY);
  }
  bool isNative() const { return !isInterpreted(); }

  bool isConstructor() const { return hasFlags(CONSTRUCTOR); }

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
    MOZ_ASSERT_IF(hasFlags(WASM_JIT_ENTRY), isWasm());
    return hasFlags(WASM_JIT_ENTRY);
  }
  bool isNativeWithJitEntry() const {
    MOZ_ASSERT_IF(isWasmWithJitEntry(), isNative());
    return isWasmWithJitEntry();
  }
  bool isBuiltinNative() const {
    return isNative() && !isAsmJSNative() && !isWasm();
  }

  /* Possible attributes of an interpreted function: */
  bool isBoundFunction() const { return hasFlags(BOUND_FUN); }
  bool hasInferredName() const { return hasFlags(HAS_INFERRED_NAME); }
  bool hasGuessedAtom() const {
    static_assert(HAS_GUESSED_ATOM == HAS_BOUND_FUNCTION_NAME_PREFIX,
                  "HAS_GUESSED_ATOM is unused for bound functions");
    bool hasGuessedAtom = hasFlags(HAS_GUESSED_ATOM);
    bool boundFun = hasFlags(BOUND_FUN);
    return hasGuessedAtom && !boundFun;
  }
  bool hasBoundFunctionNamePrefix() const {
    static_assert(
        HAS_BOUND_FUNCTION_NAME_PREFIX == HAS_GUESSED_ATOM,
        "HAS_BOUND_FUNCTION_NAME_PREFIX is only used for bound functions");
    MOZ_ASSERT(isBoundFunction());
    return hasFlags(HAS_BOUND_FUNCTION_NAME_PREFIX);
  }
  bool isLambda() const { return hasFlags(LAMBDA); }

  bool isNamedLambda(JSAtom* atom) const {
    return isLambda() && atom && !hasInferredName() && !hasGuessedAtom();
  }

  // These methods determine which of the u.scripted.s union arms are active.
  // For live JSFunctions the pointer values will always be non-null, but due
  // to partial initialization the GC (and other features that scan the heap
  // directly) may still return a null pointer.
  bool hasBaseScript() const { return hasFlags(BASESCRIPT); }
  bool hasSelfHostedLazyScript() const { return hasFlags(SELFHOSTLAZY); }

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

  bool hasResolvedLength() const { return hasFlags(RESOLVED_LENGTH); }
  bool hasResolvedName() const { return hasFlags(RESOLVED_NAME); }

  bool isSelfHostedOrIntrinsic() const { return hasFlags(SELF_HOSTED); }
  bool isSelfHostedBuiltin() const {
    return isSelfHostedOrIntrinsic() && !isNative();
  }
  bool isIntrinsic() const { return isSelfHostedOrIntrinsic() && isNative(); }

  void setKind(FunctionKind kind) {
    this->flags_ &= ~FUNCTION_KIND_MASK;
    this->flags_ |= static_cast<uint16_t>(kind) << FUNCTION_KIND_SHIFT;
  }

  // Make the function constructible.
  void setIsConstructor() {
    MOZ_ASSERT(!isConstructor());
    MOZ_ASSERT(isSelfHostedBuiltin());
    setFlags(CONSTRUCTOR);
  }

  void setIsClassConstructor() {
    MOZ_ASSERT(!isClassConstructor());
    MOZ_ASSERT(isConstructor());

    setKind(ClassConstructor);
  }

  void clearIsSelfHosted() { clearFlags(SELF_HOSTED); }

  void setIsBoundFunction() {
    MOZ_ASSERT(!isBoundFunction());
    setFlags(BOUND_FUN);
  }

  void setIsSelfHostedBuiltin() {
    MOZ_ASSERT(isInterpreted());
    MOZ_ASSERT(!isSelfHostedBuiltin());
    setFlags(SELF_HOSTED);
    // Self-hosted functions should not be constructable.
    clearFlags(CONSTRUCTOR);
  }
  void setIsIntrinsic() {
    MOZ_ASSERT(isNative());
    MOZ_ASSERT(!isIntrinsic());
    setFlags(SELF_HOSTED);
  }

  void setResolvedLength() { setFlags(RESOLVED_LENGTH); }
  void setResolvedName() { setFlags(RESOLVED_NAME); }

  // Mark a function as having its 'new' script information cleared.
  bool wasNewScriptCleared() const { return hasFlags(NEW_SCRIPT_CLEARED); }
  void setNewScriptCleared() { setFlags(NEW_SCRIPT_CLEARED); }

  void setInferredName() { setFlags(HAS_INFERRED_NAME); }
  void clearInferredName() { clearFlags(HAS_INFERRED_NAME); }

  void setGuessedAtom() { setFlags(HAS_GUESSED_ATOM); }

  void setPrefixedBoundFunctionName() {
    setFlags(HAS_BOUND_FUNCTION_NAME_PREFIX);
  }

  void setSelfHostedLazy() { setFlags(SELFHOSTLAZY); }
  void clearSelfHostedLazy() { clearFlags(SELFHOSTLAZY); }
  void setBaseScript() { setFlags(BASESCRIPT); }
  void clearBaseScript() { clearFlags(BASESCRIPT); }

  void setWasmJitEntry() { setFlags(WASM_JIT_ENTRY); }

  bool isExtended() const { return hasFlags(EXTENDED); }
  void setIsExtended() { setFlags(EXTENDED); }

  bool isNativeConstructor() const { return hasFlags(NATIVE_CTOR); }
};

}  // namespace js

class JSFunction : public js::NativeObject {
 public:
  static const JSClass class_;

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
  using FunctionFlags = js::FunctionFlags;
  FunctionFlags flags_;

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
        js::BaseScript* script_;
        js::SelfHostedLazyScript* selfHostedLazy_;
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
  //      evaluating the JSOp::SetFunName bytecode.
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
    if (isNative()) {
      return false;
    }

    MOZ_ASSERT(hasBytecode());

    // Note: this should be kept in sync with
    // FunctionBox::needsCallObjectRegardlessOfBindings().
    MOZ_ASSERT_IF(
        baseScript()->funHasExtensibleScope() || isGenerator() || isAsync(),
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

  FunctionFlags flags() { return flags_; }

  FunctionFlags::FunctionKind kind() const { return flags_.kind(); }

  /* A function can be classified as either native (C++) or interpreted (JS): */
  bool isInterpreted() const { return flags_.isInterpreted(); }
  bool isNative() const { return flags_.isNative(); }

  bool isConstructor() const { return flags_.isConstructor(); }

  /* Possible attributes of a native function: */
  bool isAsmJSNative() const { return flags_.isAsmJSNative(); }

  bool isWasm() const { return flags_.isWasm(); }
  bool isWasmWithJitEntry() const { return flags_.isWasmWithJitEntry(); }
  bool isNativeWithJitEntry() const { return flags_.isNativeWithJitEntry(); }
  bool isBuiltinNative() const { return flags_.isBuiltinNative(); }

  /* Possible attributes of an interpreted function: */
  bool isBoundFunction() const { return flags_.isBoundFunction(); }
  bool hasInferredName() const { return flags_.hasInferredName(); }
  bool hasGuessedAtom() const { return flags_.hasGuessedAtom(); }
  bool hasBoundFunctionNamePrefix() const {
    return flags_.hasBoundFunctionNamePrefix();
  }

  bool isLambda() const { return flags_.isLambda(); }

  // These methods determine which of the u.scripted.s union arms are active.
  // For live JSFunctions the pointer values will always be non-null, but due
  // to partial initialization the GC (and other features that scan the heap
  // directly) may still return a null pointer.
  bool hasSelfHostedLazyScript() const {
    return flags_.hasSelfHostedLazyScript();
  }
  bool hasBaseScript() const { return flags_.hasBaseScript(); }

  bool hasBytecode() const {
    MOZ_ASSERT(!isIncomplete());
    return hasBaseScript() && baseScript()->hasBytecode();
  }

  // Arrow functions store their lexical new.target in the first extended slot.
  bool isArrow() const { return flags_.isArrow(); }
  // Every class-constructor is also a method.
  bool isMethod() const { return flags_.isMethod(); }
  bool isClassConstructor() const { return flags_.isClassConstructor(); }

  bool isGetter() const { return flags_.isGetter(); }
  bool isSetter() const { return flags_.isSetter(); }

  bool allowSuperProperty() const { return flags_.allowSuperProperty(); }

  bool hasResolvedLength() const { return flags_.hasResolvedLength(); }
  bool hasResolvedName() const { return flags_.hasResolvedName(); }

  bool isSelfHostedOrIntrinsic() const {
    return flags_.isSelfHostedOrIntrinsic();
  }
  bool isSelfHostedBuiltin() const { return flags_.isSelfHostedBuiltin(); }

  bool isIntrinsic() const { return flags_.isIntrinsic(); }

  bool hasJitScript() const {
    if (!hasBaseScript()) {
      return false;
    }

    return baseScript()->hasJitScript();
  }
  bool hasJitEntry() const {
    return hasBaseScript() || hasSelfHostedLazyScript() ||
           isNativeWithJitEntry();
  }

  /* Compound attributes: */
  bool isBuiltin() const { return isBuiltinNative() || isSelfHostedBuiltin(); }

  bool isNamedLambda() const { return flags_.isNamedLambda(displayAtom()); }

  bool hasLexicalThis() const { return isArrow(); }

  bool isBuiltinFunctionConstructor();
  bool needsPrototypeProperty();

  // Returns true if this function must have a non-configurable .prototype data
  // property. This is used to ensure looking up .prototype elsewhere will have
  // no side-effects.
  bool hasNonConfigurablePrototypeDataProperty();

  // Returns true if |new Fun()| should not allocate a new object caller-side
  // but pass the uninitialized-lexical MagicValue and rely on the callee to
  // construct its own |this| object.
  bool constructorNeedsUninitializedThis() const {
    MOZ_ASSERT(isConstructor());
    MOZ_ASSERT(isInterpreted());
    return isBoundFunction() || isDerivedClassConstructor();
  }

  /* Returns the strictness of this function, which must be interpreted. */
  bool strict() const { return baseScript()->strict(); }

  void setFlags(uint16_t flags) { flags_ = FunctionFlags(flags); }
  void setFlags(FunctionFlags flags) { flags_ = flags; }

  // Make the function constructible.
  void setIsConstructor() { flags_.setIsConstructor(); }
  void setIsClassConstructor() { flags_.setIsClassConstructor(); }

  void clearIsSelfHosted() { flags_.clearIsSelfHosted(); }

  // Can be called multiple times by the parser.
  void setArgCount(uint16_t nargs) { this->nargs_ = nargs; }

  void setIsBoundFunction() { flags_.setIsBoundFunction(); }
  void setIsSelfHostedBuiltin() { flags_.setIsSelfHostedBuiltin(); }
  void setIsIntrinsic() { flags_.setIsIntrinsic(); }

  void setResolvedLength() { flags_.setResolvedLength(); }
  void setResolvedName() { flags_.setResolvedName(); }

  // Mark a function as having its 'new' script information cleared.
  bool wasNewScriptCleared() const { return flags_.wasNewScriptCleared(); }
  void setNewScriptCleared() { flags_.setNewScriptCleared(); }

  static bool getUnresolvedLength(JSContext* cx, js::HandleFunction fun,
                                  js::MutableHandleValue v);

  JSAtom* infallibleGetUnresolvedName(JSContext* cx);

  static bool getUnresolvedName(JSContext* cx, js::HandleFunction fun,
                                js::MutableHandleValue v);

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
    flags_.setInferredName();
  }
  void clearInferredName() {
    MOZ_ASSERT(hasInferredName());
    MOZ_ASSERT(atom_);
    setAtom(nullptr);
    flags_.clearInferredName();
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
    flags_.setGuessedAtom();
  }

  void setPrefixedBoundFunctionName(JSAtom* atom) {
    MOZ_ASSERT(!hasBoundFunctionNamePrefix());
    MOZ_ASSERT(atom);
    flags_.setPrefixedBoundFunctionName();
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

  static bool delazifyLazilyInterpretedFunction(JSContext* cx,
                                                js::HandleFunction fun);
  static bool delazifySelfHostedLazyFunction(JSContext* cx,
                                             js::HandleFunction fun);
  void maybeRelazify(JSRuntime* rt);

  // Function Scripts
  //
  // Interpreted functions have either a BaseScript or a SelfHostedLazyScript. A
  // BaseScript may either be lazy or non-lazy (hasBytecode()). Methods may
  // return a JSScript* if underlying BaseScript is known to have bytecode.
  //
  // There are several methods to get the script of an interpreted function:
  //
  // - For all interpreted functions, getOrCreateScript() will get the
  //   JSScript, delazifying the function if necessary. This is the safest to
  //   use, but has extra checks, requires a cx and may trigger a GC.
  //
  // - For functions known to have a JSScript, nonLazyScript() will get it.

  static JSScript* getOrCreateScript(JSContext* cx, js::HandleFunction fun) {
    MOZ_ASSERT(fun->isInterpreted());
    MOZ_ASSERT(cx);

    if (fun->hasSelfHostedLazyScript()) {
      if (!delazifySelfHostedLazyFunction(cx, fun)) {
        return nullptr;
      }
      return fun->nonLazyScript();
    }

    MOZ_ASSERT(fun->hasBaseScript());
    JS::Rooted<js::BaseScript*> script(cx, fun->baseScript());

    if (!script->hasBytecode()) {
      if (!delazifyLazilyInterpretedFunction(cx, fun)) {
        return nullptr;
      }
    }
    return fun->nonLazyScript();
  }

  // If this is a scripted function, returns its canonical function (the
  // original function allocated by the frontend). Note that lazy self-hosted
  // builtins don't have a lazy script so in that case we also return nullptr.
  JSFunction* maybeCanonicalFunction() const {
    if (hasBaseScript()) {
      return baseScript()->function();
    }
    return nullptr;
  }

  // The default state of a JSFunction that is not ready for execution. This is
  // generally the result of failure during bytecode compilation.
  //
  // If u.scripted.s.script_ is non-null, the pointed JSScript is guaranteed to
  // be complete (see the comment above JSScript::initFromFunctionBox callsite
  // in JSScript::fullyInitFromEmitter).
  bool isIncomplete() const { return isInterpreted() && !u.scripted.s.script_; }

  JSScript* nonLazyScript() const {
    MOZ_ASSERT(hasBaseScript());
    MOZ_ASSERT(u.scripted.s.script_);
    return static_cast<JSScript*>(u.scripted.s.script_);
  }

  js::SelfHostedLazyScript* selfHostedLazyScript() const {
    MOZ_ASSERT(hasSelfHostedLazyScript());
    MOZ_ASSERT(u.scripted.s.selfHostedLazy_);
    return u.scripted.s.selfHostedLazy_;
  }

  // Access fields defined on both lazy and non-lazy scripts.
  js::BaseScript* baseScript() const {
    MOZ_ASSERT(hasBaseScript());
    MOZ_ASSERT(u.scripted.s.script_);
    return u.scripted.s.script_;
  }

  static bool getLength(JSContext* cx, js::HandleFunction fun,
                        uint16_t* length);

  js::Scope* enclosingScope() const { return baseScript()->enclosingScope(); }

  void setEnclosingScope(js::Scope* enclosingScope) {
    baseScript()->setEnclosingScope(enclosingScope);
  }

  void setEnclosingLazyScript(js::BaseScript* enclosingScript) {
    baseScript()->setEnclosingScript(enclosingScript);
  }

  js::GeneratorKind generatorKind() const {
    if (hasBaseScript()) {
      return baseScript()->generatorKind();
    }
    return js::GeneratorKind::NotGenerator;
  }

  bool isGenerator() const {
    return generatorKind() == js::GeneratorKind::Generator;
  }

  js::FunctionAsyncKind asyncKind() const {
    if (hasBaseScript()) {
      return baseScript()->asyncKind();
    }
    return js::FunctionAsyncKind::SyncFunction;
  }

  bool isAsync() const {
    return asyncKind() == js::FunctionAsyncKind::AsyncFunction;
  }

  void initScript(JSScript* script) {
    MOZ_ASSERT_IF(script, realm() == script->realm());

    u.scripted.s.script_ = script;
  }

  void initLazyScript(js::BaseScript* lazy) {
    MOZ_ASSERT(isInterpreted());
    u.scripted.s.script_ = lazy;
  }

  // Release the lazyScript() pointer while triggering barriers.
  void clearLazyScript() {
    js::BaseScript::writeBarrierPre(baseScript());
    u.scripted.s.script_ = nullptr;
    MOZ_ASSERT(isIncomplete());
  }

  void initSelfHostedLazyScript(js::SelfHostedLazyScript* lazy) {
    MOZ_ASSERT(isSelfHostedBuiltin());
    MOZ_ASSERT(isInterpreted());
    flags_.clearBaseScript();
    flags_.setSelfHostedLazy();
    u.scripted.s.selfHostedLazy_ = lazy;
    MOZ_ASSERT(hasSelfHostedLazyScript());
  }

  void clearSelfHostedLazyScript() {
    // Note: The selfHostedLazy_ field is not a GC-thing pointer so we don't
    // need to trigger barriers.
    flags_.clearSelfHostedLazy();
    flags_.setBaseScript();
    u.scripted.s.script_ = nullptr;
    MOZ_ASSERT(isIncomplete());
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
    flags_.setWasmJitEntry();
    u.native.extra.wasmJitEntry_ = entry;
    MOZ_ASSERT(isWasmWithJitEntry());
  }
  void** wasmJitEntry() const {
    MOZ_ASSERT(isWasmWithJitEntry());
    MOZ_ASSERT(u.native.extra.wasmJitEntry_);
    return u.native.extra.wasmJitEntry_;
  }

  bool isDerivedClassConstructor() const;

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
  static unsigned offsetOfBaseScript() {
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
  inline js::FunctionExtended* toExtended();
  inline const js::FunctionExtended* toExtended() const;

 public:
  inline bool isExtended() const {
    bool extended = flags_.isExtended();
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
// (yes, null, not the global lexical environment).  In all cases, the global
// will be used as the terminating environment.

extern JSFunction* NewFunctionWithProto(
    JSContext* cx, JSNative native, unsigned nargs, FunctionFlags flags,
    HandleObject enclosingEnv, HandleAtom atom, HandleObject proto,
    gc::AllocKind allocKind = gc::AllocKind::FUNCTION,
    NewObjectKind newKind = GenericObject);

// Allocate a new function backed by a JSNative.  Note that by default this
// creates a singleton object.
inline JSFunction* NewNativeFunction(
    JSContext* cx, JSNative native, unsigned nargs, HandleAtom atom,
    gc::AllocKind allocKind = gc::AllocKind::FUNCTION,
    NewObjectKind newKind = SingletonObject,
    FunctionFlags flags = FunctionFlags::NATIVE_FUN) {
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
    FunctionFlags flags = FunctionFlags::NATIVE_CTOR) {
  MOZ_ASSERT(native);
  MOZ_ASSERT(flags.isNativeConstructor());
  return NewFunctionWithProto(cx, native, nargs, flags, nullptr, atom, nullptr,
                              allocKind, newKind);
}

// Allocate a new scripted function.  If enclosingEnv is null, the
// global lexical environment will be used.  In all cases the terminating
// environment of the resulting object will be the global.
extern JSFunction* NewScriptedFunction(
    JSContext* cx, unsigned nargs, FunctionFlags flags, HandleAtom atom,
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

// Assumes that fun.__proto__ === Function.__proto__, i.e., does not check for
// the case where a function with a non-default __proto__ has an overridden
// @@hasInstance handler. Will assert if not.
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
                                   HandleObject newEnclosingEnv);

extern JSFunction* CloneFunctionReuseScript(JSContext* cx, HandleFunction fun,
                                            HandleObject enclosingEnv,
                                            gc::AllocKind kind,
                                            NewObjectKind newKindArg,
                                            HandleObject proto);

// Functions whose scripts are cloned are always given singleton types.
extern JSFunction* CloneFunctionAndScript(
    JSContext* cx, HandleFunction fun, HandleObject enclosingEnv,
    HandleScope newScope, Handle<ScriptSourceObject*> sourceObject,
    gc::AllocKind kind, HandleObject proto = nullptr);

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
                                     const JSClass* clasp);

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

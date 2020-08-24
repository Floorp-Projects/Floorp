/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SharedContext_h
#define frontend_SharedContext_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include "jstypes.h"

#include "frontend/AbstractScopePtr.h"    // ScopeIndex
#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/ParseNode.h"
#include "frontend/Stencil.h"          // FunctionIndex
#include "js/WasmModule.h"             // JS::WasmModule
#include "vm/FunctionFlags.h"          // js::FunctionFlags
#include "vm/GeneratorAndAsyncKind.h"  // js::GeneratorKind, js::FunctionAsyncKind
#include "vm/JSFunction.h"
#include "vm/JSScript.h"
#include "vm/Scope.h"
#include "vm/SharedStencil.h"

namespace js {
namespace frontend {

class ParseContext;
class ScriptStencil;
struct ScopeContext;

enum class StatementKind : uint8_t {
  Label,
  Block,
  If,
  Switch,
  With,
  Catch,
  Try,
  Finally,
  ForLoopLexicalHead,
  ForLoop,
  ForInLoop,
  ForOfLoop,
  DoLoop,
  WhileLoop,
  Class,

  // Used only by BytecodeEmitter.
  Spread,
  YieldStar,
};

static inline bool StatementKindIsLoop(StatementKind kind) {
  return kind == StatementKind::ForLoop || kind == StatementKind::ForInLoop ||
         kind == StatementKind::ForOfLoop || kind == StatementKind::DoLoop ||
         kind == StatementKind::WhileLoop || kind == StatementKind::Spread ||
         kind == StatementKind::YieldStar;
}

static inline bool StatementKindIsUnlabeledBreakTarget(StatementKind kind) {
  return StatementKindIsLoop(kind) || kind == StatementKind::Switch;
}

// List of directives that may be encountered in a Directive Prologue
// (ES5 15.1).
class Directives {
  bool strict_;
  bool asmJS_;

 public:
  explicit Directives(bool strict) : strict_(strict), asmJS_(false) {}
  explicit Directives(ParseContext* parent);

  void setStrict() { strict_ = true; }
  bool strict() const { return strict_; }

  void setAsmJS() { asmJS_ = true; }
  bool asmJS() const { return asmJS_; }

  Directives& operator=(Directives rhs) {
    strict_ = rhs.strict_;
    asmJS_ = rhs.asmJS_;
    return *this;
  }
  bool operator==(const Directives& rhs) const {
    return strict_ == rhs.strict_ && asmJS_ == rhs.asmJS_;
  }
  bool operator!=(const Directives& rhs) const { return !(*this == rhs); }
};

// The kind of this-binding for the current scope. Note that arrow functions
// have a lexical this-binding so their ThisBinding is the same as the
// ThisBinding of their enclosing scope and can be any value. Derived
// constructors require TDZ checks when accessing the binding.
enum class ThisBinding : uint8_t {
  Global,
  Module,
  Function,
  DerivedConstructor
};

class GlobalSharedContext;
class EvalSharedContext;
class ModuleSharedContext;

#define FLAG_GETTER(enumName, enumEntry, lowerName, name) \
 public:                                                  \
  bool lowerName() const { return hasFlag(enumName::enumEntry); }

#define FLAG_SETTER(enumName, enumEntry, lowerName, name) \
 public:                                                  \
  void set##name() { setFlag(enumName::enumEntry); }      \
  void set##name(bool b) { setFlag(enumName::enumEntry, b); }

#define IMMUTABLE_FLAG_GETTER_SETTER(lowerName, name) \
  FLAG_GETTER(ImmutableFlags, name, lowerName, name)  \
  FLAG_SETTER(ImmutableFlags, name, lowerName, name)

#define IMMUTABLE_FLAG_GETTER(lowerName, name) \
  FLAG_GETTER(ImmutableFlags, name, lowerName, name)

/*
 * The struct SharedContext is part of the current parser context (see
 * ParseContext). It stores information that is reused between the parser and
 * the bytecode emitter.
 */
class SharedContext {
 public:
  JSContext* const cx_;

 protected:
  CompilationInfo& compilationInfo_;

  // See: BaseScript::immutableFlags_
  ImmutableScriptFlags immutableFlags_ = {};

  // The location of this script in the source. Note that the value here differs
  // from the final BaseScript for the case of standalone functions.
  // This field is copied to ScriptStencil, and shouldn't be modified after the
  // copy.
  SourceExtent extent_ = {};

 protected:
  // See: ThisBinding
  ThisBinding thisBinding_ = ThisBinding::Global;

  // These flags do not have corresponding script flags and may be inherited
  // from the scope chain in the case of eval and arrows.
  bool allowNewTarget_ : 1;
  bool allowSuperProperty_ : 1;
  bool allowSuperCall_ : 1;
  bool allowArguments_ : 1;
  bool inWith_ : 1;
  bool inClass_ : 1;

  // See `strict()` below.
  bool localStrict : 1;

  // True if "use strict"; appears in the body instead of being inherited.
  bool hasExplicitUseStrict_ : 1;

  // Tracks if script-related fields are already copied to ScriptStencil.
  //
  // If this field is true, those fileds shouldn't be modified.
  //
  // For FunctionBox, some fields are allowed to be modified, but the
  // modification should be synced with ScriptStencil by
  // FunctionBox::copyUpdated* methods.
  bool isScriptFieldCopiedToStencil : 1;

  // End of fields.

  enum class Kind : uint8_t { FunctionBox, Global, Eval, Module };

  // Alias enum into SharedContext
  using ImmutableFlags = ImmutableScriptFlagsEnum;

  MOZ_MUST_USE bool hasFlag(ImmutableFlags flag) const {
    return immutableFlags_.hasFlag(flag);
  }
  void setFlag(ImmutableFlags flag, bool b = true) {
    MOZ_ASSERT(!isScriptFieldCopiedToStencil);
    immutableFlags_.setFlag(flag, b);
  }

 public:
  SharedContext(JSContext* cx, Kind kind, CompilationInfo& compilationInfo,
                Directives directives, SourceExtent extent);

  IMMUTABLE_FLAG_GETTER_SETTER(isForEval, IsForEval)
  IMMUTABLE_FLAG_GETTER_SETTER(isModule, IsModule)
  IMMUTABLE_FLAG_GETTER_SETTER(isFunction, IsFunction)
  IMMUTABLE_FLAG_GETTER_SETTER(selfHosted, SelfHosted)
  IMMUTABLE_FLAG_GETTER_SETTER(forceStrict, ForceStrict)
  IMMUTABLE_FLAG_GETTER_SETTER(hasNonSyntacticScope, HasNonSyntacticScope)
  IMMUTABLE_FLAG_GETTER_SETTER(noScriptRval, NoScriptRval)
  IMMUTABLE_FLAG_GETTER(treatAsRunOnce, TreatAsRunOnce)
  // Strict: custom logic below
  IMMUTABLE_FLAG_GETTER_SETTER(hasModuleGoal, HasModuleGoal)
  IMMUTABLE_FLAG_GETTER_SETTER(hasInnerFunctions, HasInnerFunctions)
  IMMUTABLE_FLAG_GETTER_SETTER(hasDirectEval, HasDirectEval)
  IMMUTABLE_FLAG_GETTER_SETTER(bindingsAccessedDynamically,
                               BindingsAccessedDynamically)
  IMMUTABLE_FLAG_GETTER_SETTER(hasCallSiteObj, HasCallSiteObj)

  const SourceExtent& extent() const { return extent_; }

  bool isFunctionBox() const { return isFunction(); }
  inline FunctionBox* asFunctionBox();
  bool isModuleContext() const { return isModule(); }
  inline ModuleSharedContext* asModuleContext();
  bool isGlobalContext() const {
    return !(isFunction() || isModule() || isForEval());
  }
  inline GlobalSharedContext* asGlobalContext();
  bool isEvalContext() const { return isForEval(); }
  inline EvalSharedContext* asEvalContext();

  bool isTopLevelContext() const { return !isFunction(); }

  CompilationInfo& compilationInfo() const { return compilationInfo_; }

  ThisBinding thisBinding() const { return thisBinding_; }
  bool hasFunctionThisBinding() const {
    return thisBinding() == ThisBinding::Function ||
           thisBinding() == ThisBinding::DerivedConstructor;
  }
  bool needsThisTDZChecks() const {
    return thisBinding() == ThisBinding::DerivedConstructor;
  }

  bool isSelfHosted() const { return selfHosted(); }
  bool allowNewTarget() const { return allowNewTarget_; }
  bool allowSuperProperty() const { return allowSuperProperty_; }
  bool allowSuperCall() const { return allowSuperCall_; }
  bool allowArguments() const { return allowArguments_; }
  bool inWith() const { return inWith_; }
  bool inClass() const { return inClass_; }

  bool hasExplicitUseStrict() const { return hasExplicitUseStrict_; }
  void setExplicitUseStrict() { hasExplicitUseStrict_ = true; }

  ImmutableScriptFlags immutableFlags() { return immutableFlags_; }

  inline bool allBindingsClosedOver();

  // The ImmutableFlag tracks if the entire script is strict, while the
  // localStrict flag indicates the current region (such as class body) should
  // be treated as strict. The localStrict flag will always be reset to false
  // before the end of the script.
  bool strict() const { return hasFlag(ImmutableFlags::Strict) || localStrict; }
  void setStrictScript() { setFlag(ImmutableFlags::Strict); }
  bool setLocalStrictMode(bool strict) {
    bool retVal = localStrict;
    localStrict = strict;
    return retVal;
  }

  void copyScriptFields(ScriptStencil& stencil);
};

class MOZ_STACK_CLASS GlobalSharedContext : public SharedContext {
  ScopeKind scopeKind_;

 public:
  Rooted<GlobalScope::Data*> bindings;

  GlobalSharedContext(JSContext* cx, ScopeKind scopeKind,
                      CompilationInfo& compilationInfo, Directives directives,
                      SourceExtent extent)
      : SharedContext(cx, Kind::Global, compilationInfo, directives, extent),
        scopeKind_(scopeKind),
        bindings(cx) {
    MOZ_ASSERT(scopeKind == ScopeKind::Global ||
               scopeKind == ScopeKind::NonSyntactic);
    MOZ_ASSERT(thisBinding_ == ThisBinding::Global);
  }

  ScopeKind scopeKind() const { return scopeKind_; }
};

inline GlobalSharedContext* SharedContext::asGlobalContext() {
  MOZ_ASSERT(isGlobalContext());
  return static_cast<GlobalSharedContext*>(this);
}

class MOZ_STACK_CLASS EvalSharedContext : public SharedContext {
 public:
  Rooted<EvalScope::Data*> bindings;

  EvalSharedContext(JSContext* cx, CompilationInfo& compilationInfo,
                    Directives directives, SourceExtent extent);
};

inline EvalSharedContext* SharedContext::asEvalContext() {
  MOZ_ASSERT(isEvalContext());
  return static_cast<EvalSharedContext*>(this);
}

enum class HasHeritage { No, Yes };
enum class TopLevelFunction { No, Yes };

class FunctionBox : public SharedContext {
  friend struct GCThingList;

  // The parser handles tracing the fields below via the FunctionBox linked
  // list represented by |traceLink_|.
  FunctionBox* traceLink_ = nullptr;

  // If this FunctionBox refers to a lazy child of the function being
  // compiled, this field holds the child's immediately enclosing scope's index.
  // Once compilation succeeds, we will store the scope pointed by this in the
  // child's BaseScript.  (Debugger may become confused if lazy scripts refer to
  // partially initialized enclosing scopes, so we must avoid storing the
  // scope in the BaseScript until compilation has completed
  // successfully.)
  // This is copied to ScriptStencil.
  // Any update after the copy should be synced to the ScriptStencil.
  mozilla::Maybe<ScopeIndex> enclosingScopeIndex_;

  // Names from the named lambda scope, if a named lambda.
  LexicalScope::Data* namedLambdaBindings_ = nullptr;

  // Names from the function scope.
  FunctionScope::Data* functionScopeBindings_ = nullptr;

  // Names from the extra 'var' scope of the function, if the parameter list
  // has expressions.
  VarScope::Data* extraVarScopeBindings_ = nullptr;

  // The explicit or implicit name of the function. The FunctionFlags indicate
  // the kind of name.
  // This is copied to ScriptStencil.
  // Any update after the copy should be synced to the ScriptStencil.
  JSAtom* atom_ = nullptr;

  // Index into CompilationInfo::{funcData, functions}.
  FunctionIndex funcDataIndex_ = FunctionIndex(-1);

  // See: FunctionFlags
  // This is copied to ScriptStencil.
  // Any update after the copy should be synced to the ScriptStencil.
  FunctionFlags flags_ = {};

  // See: ImmutableScriptData::funLength
  uint16_t length_ = 0;

  // JSFunction::nargs_
  // This field is copied to ScriptStencil, and shouldn't be modified after the
  // copy.
  uint16_t nargs_ = 0;

  // See: PrivateScriptData::memberInitializers_
  // This field is copied to ScriptStencil, and shouldn't be modified after the
  // copy.
  mozilla::Maybe<MemberInitializers> memberInitializers_ = {};

 public:
  // Back pointer used by asm.js for error messages.
  FunctionNode* functionNode = nullptr;

  TopLevelFunction isTopLevel_ = TopLevelFunction::No;

  // True if bytecode will be emitted for this function in the current
  // compilation.
  bool emitBytecode : 1;

  // This function is a standalone function that is not syntactically part of
  // another script. Eg. Created by `new Function("")`.
  bool isStandalone_ : 1;

  // This is set by the BytecodeEmitter of the enclosing script when a reference
  // to this function is generated. This is also used to determine a hoisted
  // function already is referenced by the bytecode.
  bool wasEmitted_ : 1;

  // This function should be marked as a singleton. It is expected to be defined
  // at most once. This is a heuristic only and does not affect correctness.
  bool isSingleton_ : 1;

  // Need to emit a synthesized Annex B assignment
  bool isAnnexB : 1;

  // Track if we saw "use asm".
  // If we successfully validated it, `flags_` is seto to `AsmJS` kind.
  bool useAsm : 1;

  // Analysis of parameter list
  bool hasParameterExprs : 1;
  bool hasDestructuringArgs : 1;
  bool hasDuplicateParameters : 1;

  // Arrow function with expression body like: `() => 1`.
  bool hasExprBody_ : 1;

  // Analysis for use in heuristics.
  bool usesApply : 1;   // Contains an f.apply() call
  bool usesThis : 1;    // Contains 'this'
  bool usesReturn : 1;  // Contains a 'return' statement

  // Tracks if function-related fields are already copied to ScriptStencil.
  // If this field is true, modification to those fields should be synced with
  // ScriptStencil by copyUpdated* methods.
  bool isFunctionFieldCopiedToStencil : 1;

  // End of fields.

  FunctionBox(JSContext* cx, FunctionBox* traceListHead, SourceExtent extent,
              CompilationInfo& compilationInfo, Directives directives,
              GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
              JSAtom* explicitName, FunctionFlags flags, FunctionIndex index,
              TopLevelFunction isTopLevel);

  MutableHandle<ScriptStencil> functionStencil() const;

#ifdef DEBUG
  bool atomsAreKept();
#endif

  MutableHandle<LexicalScope::Data*> namedLambdaBindings() {
    MOZ_ASSERT(atomsAreKept());
    return MutableHandle<LexicalScope::Data*>::fromMarkedLocation(
        &namedLambdaBindings_);
  }

  MutableHandle<FunctionScope::Data*> functionScopeBindings() {
    MOZ_ASSERT(atomsAreKept());
    return MutableHandle<FunctionScope::Data*>::fromMarkedLocation(
        &functionScopeBindings_);
  }

  MutableHandle<VarScope::Data*> extraVarScopeBindings() {
    MOZ_ASSERT(atomsAreKept());
    return MutableHandle<VarScope::Data*>::fromMarkedLocation(
        &extraVarScopeBindings_);
  }

  void initFromLazyFunction(JSFunction* fun);

  void initStandalone(ScopeContext& scopeContext, FunctionFlags flags,
                      FunctionSyntaxKind kind);

  void initWithEnclosingParseContext(ParseContext* enclosing,
                                     FunctionFlags flags,
                                     FunctionSyntaxKind kind);

  void setEnclosingScopeForInnerLazyFunction(ScopeIndex scopeIndex);

  bool isStandalone() const { return isStandalone_; }
  void setIsStandalone(bool isStandalone) {
    MOZ_ASSERT(!isFunctionFieldCopiedToStencil);
    isStandalone_ = isStandalone;
  }

  bool wasEmitted() const { return wasEmitted_; }
  void setWasEmitted(bool wasEmitted) {
    wasEmitted_ = wasEmitted;
    if (isFunctionFieldCopiedToStencil) {
      copyUpdatedWasEmitted();
    }
  }

  bool isSingleton() const { return isSingleton_; }
  void setIsSingleton(bool isSingleton) {
    isSingleton_ = isSingleton;
    if (isFunctionFieldCopiedToStencil) {
      copyUpdatedIsSingleton();
    }
  }

  MOZ_MUST_USE bool setAsmJSModule(const JS::WasmModule* module);
  bool isAsmJSModule() const { return flags_.isAsmJSNative(); }

  bool hasEnclosingScopeIndex() const { return enclosingScopeIndex_.isSome(); }
  ScopeIndex getEnclosingScopeIndex() const { return *enclosingScopeIndex_; }

  IMMUTABLE_FLAG_GETTER_SETTER(isAsync, IsAsync)
  IMMUTABLE_FLAG_GETTER_SETTER(isGenerator, IsGenerator)
  IMMUTABLE_FLAG_GETTER_SETTER(funHasExtensibleScope, FunHasExtensibleScope)
  IMMUTABLE_FLAG_GETTER_SETTER(functionHasThisBinding, FunctionHasThisBinding)
  // NeedsHomeObject: custom logic below.
  // IsDerivedClassConstructor: custom logic below.
  // IsFieldInitializer: custom logic below.
  IMMUTABLE_FLAG_GETTER_SETTER(hasRest, HasRest)
  IMMUTABLE_FLAG_GETTER_SETTER(needsFunctionEnvironmentObjects,
                               NeedsFunctionEnvironmentObjects)
  IMMUTABLE_FLAG_GETTER_SETTER(functionHasExtraBodyVarScope,
                               FunctionHasExtraBodyVarScope)
  IMMUTABLE_FLAG_GETTER_SETTER(shouldDeclareArguments, ShouldDeclareArguments)
  IMMUTABLE_FLAG_GETTER_SETTER(argumentsHasVarBinding, ArgumentsHasVarBinding)
  // AlwaysNeedsArgsObj: custom logic below.
  // HasMappedArgsObj: custom logic below.
  // IsLikelyConstructorWrapper: custom logic below.

  bool needsCallObjectRegardlessOfBindings() const {
    // Always create a CallObject if:
    // - The scope is extensible at runtime due to sloppy eval.
    // - The function is a generator or async function. (The debugger reads the
    //   generator object directly from the frame.)

    return funHasExtensibleScope() || isGenerator() || isAsync();
  }

  bool needsExtraBodyVarEnvironmentRegardlessOfBindings() const {
    MOZ_ASSERT(hasParameterExprs);
    return funHasExtensibleScope();
  }

  bool isLikelyConstructorWrapper() const {
    return argumentsHasVarBinding() && usesApply && usesThis && !usesReturn;
  }

  GeneratorKind generatorKind() const {
    return isGenerator() ? GeneratorKind::Generator
                         : GeneratorKind::NotGenerator;
  }

  FunctionAsyncKind asyncKind() const {
    return isAsync() ? FunctionAsyncKind::AsyncFunction
                     : FunctionAsyncKind::SyncFunction;
  }

  bool needsFinalYield() const { return isGenerator() || isAsync(); }
  bool needsDotGeneratorName() const { return isGenerator() || isAsync(); }
  bool needsIteratorResult() const { return isGenerator() && !isAsync(); }
  bool needsPromiseResult() const { return isAsync() && !isGenerator(); }

  bool isArrow() const { return flags_.isArrow(); }
  bool isLambda() const { return flags_.isLambda(); }

  bool hasExprBody() const { return hasExprBody_; }
  void setHasExprBody() {
    MOZ_ASSERT(isArrow());
    hasExprBody_ = true;
  }

  bool isNamedLambda() const {
    return flags_.isNamedLambda(explicitName() != nullptr);
  }
  bool isGetter() const { return flags_.isGetter(); }
  bool isSetter() const { return flags_.isSetter(); }
  bool isMethod() const { return flags_.isMethod(); }
  bool isClassConstructor() const { return flags_.isClassConstructor(); }

  bool isInterpreted() const { return flags_.hasBaseScript(); }

  FunctionFlags::FunctionKind kind() { return flags_.kind(); }

  bool hasInferredName() const { return flags_.hasInferredName(); }
  bool hasGuessedAtom() const { return flags_.hasGuessedAtom(); }

  JSAtom* displayAtom() const { return atom_; }
  JSAtom* explicitName() const {
    return (hasInferredName() || hasGuessedAtom()) ? nullptr : atom_;
  }

  // NOTE: We propagate to any existing functions for now. This handles both the
  // delazification case where functions already exist, and also handles
  // code-coverage which is not yet deferred.
  void setInferredName(JSAtom* atom) {
    atom_ = atom;
    flags_.setInferredName();
    if (isFunctionFieldCopiedToStencil) {
      copyUpdatedAtomAndFlags();
    }
  }
  void setGuessedAtom(JSAtom* atom) {
    atom_ = atom;
    flags_.setGuessedAtom();
    if (isFunctionFieldCopiedToStencil) {
      copyUpdatedAtomAndFlags();
    }
  }

  void setAlwaysNeedsArgsObj() {
    MOZ_ASSERT(argumentsHasVarBinding());
    setFlag(ImmutableFlags::AlwaysNeedsArgsObj);
  }

  bool needsHomeObject() const {
    return hasFlag(ImmutableFlags::NeedsHomeObject);
  }
  void setNeedsHomeObject() {
    MOZ_ASSERT(flags_.allowSuperProperty());
    setFlag(ImmutableFlags::NeedsHomeObject);
  }

  bool isDerivedClassConstructor() const {
    return hasFlag(ImmutableFlags::IsDerivedClassConstructor);
  }
  void setDerivedClassConstructor() {
    MOZ_ASSERT(flags_.isClassConstructor());
    setFlag(ImmutableFlags::IsDerivedClassConstructor);
  }

  bool isFieldInitializer() const {
    return hasFlag(ImmutableFlags::IsFieldInitializer);
  }
  void setFieldInitializer() {
    MOZ_ASSERT(flags_.isMethod());
    setFlag(ImmutableFlags::IsFieldInitializer);
  }

  void setTreatAsRunOnce(bool treatAsRunOnce) {
    immutableFlags_.setFlag(ImmutableFlags::TreatAsRunOnce, treatAsRunOnce);
    if (isScriptFieldCopiedToStencil) {
      copyUpdatedImmutableFlags();
    }
  }

  bool hasSimpleParameterList() const {
    return !hasRest() && !hasParameterExprs && !hasDestructuringArgs;
  }

  bool hasMappedArgsObj() const {
    return !strict() && hasSimpleParameterList();
  }

  bool shouldSuppressRunOnce() const {
    // These heuristics suppress the run-once optimization if we expect that
    // script-cloning will have more impact than TI type-precision would gain.
    //
    // See also: Bug 864218
    return explicitName() || argumentsHasVarBinding() || isGenerator() ||
           isAsync();
  }

  // Return whether this or an enclosing function is being parsed and
  // validated as asm.js. Note: if asm.js validation fails, this will be false
  // while the function is being reparsed. This flag can be used to disable
  // certain parsing features that are necessary in general, but unnecessary
  // for validated asm.js.
  bool useAsmOrInsideUseAsm() const { return useAsm; }

  void setStart(uint32_t offset, uint32_t line, uint32_t column) {
    MOZ_ASSERT(!isScriptFieldCopiedToStencil);
    extent_.sourceStart = offset;
    extent_.lineno = line;
    extent_.column = column;
  }

  void setEnd(uint32_t end) {
    MOZ_ASSERT(!isScriptFieldCopiedToStencil);
    // For all functions except class constructors, the buffer and
    // toString ending positions are the same. Class constructors override
    // the toString ending position with the end of the class definition.
    extent_.sourceEnd = end;
    extent_.toStringEnd = end;
  }

  void setCtorToStringEnd(uint32_t end) {
    extent_.toStringEnd = end;
    if (isScriptFieldCopiedToStencil) {
      copyUpdatedExtent();
    }
  }

  void setCtorFunctionHasThisBinding() {
    immutableFlags_.setFlag(ImmutableFlags::FunctionHasThisBinding, true);
    if (isScriptFieldCopiedToStencil) {
      copyUpdatedImmutableFlags();
    }
  }

  uint16_t length() { return length_; }
  void setLength(uint16_t length) { length_ = length; }

  void setArgCount(uint16_t args) {
    MOZ_ASSERT(!isFunctionFieldCopiedToStencil);
    nargs_ = args;
  }

  size_t nargs() { return nargs_; }

  bool hasMemberInitializers() const { return memberInitializers_.isSome(); }
  const MemberInitializers& memberInitializers() const {
    return *memberInitializers_;
  }
  void setMemberInitializers(MemberInitializers memberInitializers) {
    MOZ_ASSERT(memberInitializers_.isNothing());
    memberInitializers_ = mozilla::Some(memberInitializers);
    if (isScriptFieldCopiedToStencil) {
      copyUpdatedMemberInitializers();
    }
  }

  FunctionIndex index() { return funcDataIndex_; }

  void trace(JSTracer* trc);

  static void TraceList(JSTracer* trc, FunctionBox* listHead);

  FunctionBox* traceLink() { return traceLink_; }

  void finishScriptFlags();
  void copyScriptFields(ScriptStencil& stencil);
  void copyFunctionFields(ScriptStencil& stencil);

  // * setTreatAsRunOnce can be called to a lazy function, while emitting
  //   enclosing script
  // * setCtorFunctionHasThisBinding can be called to a class constructor
  //   with a lazy function, while parsing enclosing class
  void copyUpdatedImmutableFlags();

  // * setCtorToStringEnd bcan be called to a class constructor with a lazy
  //   function, while parsing enclosing class
  void copyUpdatedExtent();

  // * setMemberInitializers can be called to a class constructor with a lazy
  //   function, while emitting enclosing script
  void copyUpdatedMemberInitializers();

  // * setEnclosingScopeForInnerLazyFunction can be called to a lazy function,
  //   while emitting enclosing script
  void copyUpdatedEnclosingScopeIndex();

  // * setInferredName can be called to a lazy function, while emitting
  //   enclosing script
  // * setGuessedAtom can be called to both lazy/non-lazy functions,
  //   while running NameFunctions
  void copyUpdatedAtomAndFlags();

  // * setWasEmitted can be called to a lazy function, while emitting
  //   enclosing script
  void copyUpdatedWasEmitted();

  // * setIsSingleton can be called to a lazy function, while emitting
  //   enclosing script
  void copyUpdatedIsSingleton();
};

#undef FLAG_GETTER_SETTER
#undef IMMUTABLE_FLAG_GETTER_SETTER

inline FunctionBox* SharedContext::asFunctionBox() {
  MOZ_ASSERT(isFunctionBox());
  return static_cast<FunctionBox*>(this);
}

// In generators, we treat all bindings as closed so that they get stored on
// the heap.  This way there is less information to copy off the stack when
// suspending, and back on when resuming.  It also avoids the need to create
// and invalidate DebugScope proxies for unaliased locals in a generator
// frame, as the generator frame will be copied out to the heap and released
// only by GC.
inline bool SharedContext::allBindingsClosedOver() {
  return bindingsAccessedDynamically() ||
         (isFunctionBox() &&
          (asFunctionBox()->isGenerator() || asFunctionBox()->isAsync()));
}

}  // namespace frontend
}  // namespace js

#endif /* frontend_SharedContext_h */

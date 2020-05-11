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

#include "frontend/AbstractScopePtr.h"
#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/ParseNode.h"
#include "frontend/Stencil.h"
#include "vm/FunctionFlags.h"          // js::FunctionFlags
#include "vm/GeneratorAndAsyncKind.h"  // js::GeneratorKind, js::FunctionAsyncKind
#include "vm/JSFunction.h"
#include "vm/JSScript.h"
#include "vm/Scope.h"
#include "vm/SharedStencil.h"

namespace js {
namespace frontend {

class ParseContext;

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
// ThisBinding of their enclosing scope and can be any value.
enum class ThisBinding : uint8_t { Global, Function, Module };

class GlobalSharedContext;
class EvalSharedContext;
class ModuleSharedContext;

#define FLAG_GETTER_SETTER(enumName, enumEntry, lowerName, name)  \
 public:                                                          \
  bool lowerName() const { return hasFlag(enumName::enumEntry); } \
  void set##name() { setFlag(enumName::enumEntry); }              \
  void set##name(bool b) { setFlag(enumName::enumEntry, b); }

#define IMMUTABLE_FLAG_GETTER_SETTER(lowerName, name) \
  FLAG_GETTER_SETTER(ImmutableFlags, name, lowerName, name)

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

 public:
  // See: BaseScript::extent_
  SourceExtent extent = {};

  // If defined, this is used to allocate a JSScript instead of the parser
  // determined extent (above). This is used for certain top level contexts.
  mozilla::Maybe<SourceExtent> scriptExtent = {};

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
  bool needsThisTDZChecks_ : 1;

  // See `strict()` below.
  bool localStrict : 1;

  // True if "use strict"; appears in the body instead of being inherited.
  bool hasExplicitUseStrict_ : 1;

  // End of fields.

  enum class Kind : uint8_t { FunctionBox, Global, Eval, Module };

  // Alias enum into SharedContext
  using ImmutableFlags = ImmutableScriptFlagsEnum;

  void computeAllowSyntax(Scope* scope);
  void computeInWith(Scope* scope);
  void computeThisBinding(Scope* scope);

  MOZ_MUST_USE bool hasFlag(ImmutableFlags flag) const {
    return immutableFlags_.hasFlag(flag);
  }
  void setFlag(ImmutableFlags flag, bool b = true) {
    immutableFlags_.setFlag(flag, b);
  }

 public:
  SharedContext(JSContext* cx, Kind kind, CompilationInfo& compilationInfo,
                Directives directives, SourceExtent extent);

  // If this is the outermost SharedContext, the Scope that encloses
  // it. Otherwise nullptr.
  virtual Scope* compilationEnclosingScope() const = 0;

  IMMUTABLE_FLAG_GETTER_SETTER(isForEval, IsForEval)
  IMMUTABLE_FLAG_GETTER_SETTER(isModule, IsModule)
  IMMUTABLE_FLAG_GETTER_SETTER(isFunction, IsFunction)
  IMMUTABLE_FLAG_GETTER_SETTER(selfHosted, SelfHosted)
  IMMUTABLE_FLAG_GETTER_SETTER(forceStrict, ForceStrict)
  IMMUTABLE_FLAG_GETTER_SETTER(hasNonSyntacticScope, HasNonSyntacticScope)
  IMMUTABLE_FLAG_GETTER_SETTER(noScriptRval, NoScriptRval)
  IMMUTABLE_FLAG_GETTER_SETTER(treatAsRunOnce, TreatAsRunOnce)
  // Strict: custom logic below
  IMMUTABLE_FLAG_GETTER_SETTER(hasModuleGoal, HasModuleGoal)
  IMMUTABLE_FLAG_GETTER_SETTER(hasInnerFunctions, HasInnerFunctions)
  IMMUTABLE_FLAG_GETTER_SETTER(hasDirectEval, HasDirectEval)
  IMMUTABLE_FLAG_GETTER_SETTER(bindingsAccessedDynamically,
                               BindingsAccessedDynamically)
  IMMUTABLE_FLAG_GETTER_SETTER(hasCallSiteObj, HasCallSiteObj)

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

  bool isSelfHosted() const { return selfHosted(); }
  bool allowNewTarget() const { return allowNewTarget_; }
  bool allowSuperProperty() const { return allowSuperProperty_; }
  bool allowSuperCall() const { return allowSuperCall_; }
  bool allowArguments() const { return allowArguments_; }
  bool inWith() const { return inWith_; }
  bool needsThisTDZChecks() const { return needsThisTDZChecks_; }

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

  SourceExtent getScriptExtent() { return scriptExtent.refOr(extent); }
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
    thisBinding_ = ThisBinding::Global;
  }

  Scope* compilationEnclosingScope() const override { return nullptr; }

  ScopeKind scopeKind() const { return scopeKind_; }
};

inline GlobalSharedContext* SharedContext::asGlobalContext() {
  MOZ_ASSERT(isGlobalContext());
  return static_cast<GlobalSharedContext*>(this);
}

class MOZ_STACK_CLASS EvalSharedContext : public SharedContext {
  RootedScope enclosingScope_;

 public:
  Rooted<EvalScope::Data*> bindings;

  EvalSharedContext(JSContext* cx, JSObject* enclosingEnv,
                    CompilationInfo& compilationInfo, Scope* enclosingScope,
                    Directives directives, SourceExtent extent);

  Scope* compilationEnclosingScope() const override { return enclosingScope_; }
};

inline EvalSharedContext* SharedContext::asEvalContext() {
  MOZ_ASSERT(isEvalContext());
  return static_cast<EvalSharedContext*>(this);
}

enum class HasHeritage : bool { No, Yes };

class FunctionBox : public SharedContext {
  friend struct GCThingList;

  // The parser handles tracing the fields below via the FunctionBox linked
  // list represented by |traceLink_|.
  FunctionBox* traceLink_ = nullptr;
  FunctionBox* emitLink_ = nullptr;

  // This field is used for two purposes:
  //   * If this FunctionBox refers to the function being compiled, this field
  //     holds its enclosing scope, used for compilation.
  //   * If this FunctionBox refers to a lazy child of the function being
  //     compiled, this field holds the child's immediately enclosing scope.
  //     Once compilation succeeds, we will store it in the child's
  //     BaseScript.  (Debugger may become confused if lazy scripts refer to
  //     partially initialized enclosing scopes, so we must avoid storing the
  //     scope in the BaseScript until compilation has completed
  //     successfully.)
  AbstractScopePtr enclosingScope_ = {};

  // Names from the named lambda scope, if a named lambda.
  LexicalScope::Data* namedLambdaBindings_ = nullptr;

  // Names from the function scope.
  FunctionScope::Data* functionScopeBindings_ = nullptr;

  // Names from the extra 'var' scope of the function, if the parameter list
  // has expressions.
  VarScope::Data* extraVarScopeBindings_ = nullptr;

  // The explicit name of the function.
  JSAtom* explicitName_ = nullptr;

  // Index into CompilationInfo::funcData, which contains the function
  // information, either a JSFunction* (for a FunctionBox representing a real
  // function) or a ScriptStencilBase.
  size_t funcDataIndex_ = (size_t)(-1);

 public:
  // Back pointer used by asm.js for error messages.
  FunctionNode* functionNode = nullptr;

  // See: PrivateScriptData::fieldInitializers_
  mozilla::Maybe<FieldInitializers> fieldInitializers = {};

  FunctionFlags flags_ = {};  // See: FunctionFlags
  uint16_t length = 0;        // See: ImmutableScriptData::funLength
  uint16_t nargs_ = 0;        // JSFunction::nargs_

  // Track if bytecode should be generated and if we have done so.
  bool emitBytecode : 1;
  bool emitLazy : 1;
  bool wasEmitted : 1;

  // True if the enclosing script is generating a JSOp::Lambda/LambdaArrow op
  // referring to this function. This function will potentially be exposed to
  // script as a result. We may or may not be a lazy function.
  //
  // In particular, this criteria excludes functions inside lazy functions. We
  // require our parent to generate bytecode in order for this function to have
  // complete information such as its enclosing scope.
  //
  // See: FunctionBox::finish()
  bool exposeScript : 1;

  // Need to emit a synthesized Annex B assignment
  bool isAnnexB : 1;

  // Track if we saw "use asm" and if we successfully validated.
  bool useAsm : 1;
  bool isAsmJSModule_ : 1;

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

  // End of fields.

  FunctionBox(JSContext* cx, FunctionBox* traceListHead, SourceExtent extent,
              CompilationInfo& compilationInfo, Directives directives,
              GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
              JSAtom* explicitName, FunctionFlags flags, size_t index);

  JSFunction* createFunction(JSContext* cx);

  MutableHandle<ScriptStencilBase> functionStencil() const;

  bool hasFunctionStencil() const;
  bool hasFunction() const;

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
  void initStandaloneFunction(Scope* enclosingScope);

  void initWithEnclosingScope(JSFunction* fun);

  void initWithEnclosingParseContext(ParseContext* enclosing,
                                     FunctionFlags flags,
                                     FunctionSyntaxKind kind);

  void initWithEnclosingParseContext(ParseContext* enclosing, JSFunction* fun,
                                     FunctionSyntaxKind kind) {
    initWithEnclosingParseContext(enclosing, fun->flags(), kind);
  }

  void initFieldInitializer(ParseContext* enclosing, FunctionFlags flags);

  void setEnclosingScopeForInnerLazyFunction(
      const AbstractScopePtr& enclosingScope);
  void finish();

  JSFunction* function() const;

  // Initialize FunctionBox with a deferred allocation Function
  void initializeFunction(JSFunction* fun) { clobberFunction(fun); }

  void setAsmJSModule(JSFunction* function);
  bool isAsmJSModule() { return isAsmJSModule_; }

  void clobberFunction(JSFunction* function);

  Scope* compilationEnclosingScope() const override {
    // This is used when emitting code for the current FunctionBox and therefore
    // the enclosingScope_ must have be set correctly during initalization.

    MOZ_ASSERT(enclosingScope_);
    return enclosingScope_.scope();
  }

  IMMUTABLE_FLAG_GETTER_SETTER(isAsync, IsAsync)
  IMMUTABLE_FLAG_GETTER_SETTER(isGenerator, IsGenerator)
  IMMUTABLE_FLAG_GETTER_SETTER(funHasExtensibleScope, FunHasExtensibleScope)
  IMMUTABLE_FLAG_GETTER_SETTER(functionHasThisBinding, FunctionHasThisBinding)
  // NeedsHomeObject: custom logic below.
  // IsDerivedClassConstructor: custom logic below.
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
  void setIsInterpreted(bool interpreted) {
    flags_.setFlags(FunctionFlags::BASESCRIPT, interpreted);
  }

  FunctionFlags::FunctionKind kind() { return flags_.kind(); }

  JSAtom* explicitName() const { return explicitName_; }

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
    extent.sourceStart = offset;
    extent.lineno = line;
    extent.column = column;
  }

  void setEnd(uint32_t end) {
    // For all functions except class constructors, the buffer and
    // toString ending positions are the same. Class constructors override
    // the toString ending position with the end of the class definition.
    extent.sourceEnd = extent.toStringEnd = end;
  }

  void setArgCount(uint16_t args) { nargs_ = args; }

  size_t nargs() { return nargs_; }

  // Flush the acquired argCount to the associated function.
  // If the function doesn't exist yet, this of course isn't necessary;
  void synchronizeArgCount() {
    if (hasFunction()) {
      function()->setArgCount(nargs_);
    }
  }

  bool setTypeForScriptedFunction(JSContext* cx, bool singleton) {
    RootedFunction fun(cx, function());
    return JSFunction::setTypeForScriptedFunction(cx, fun, singleton);
  }

  void setInferredName(JSAtom* atom) { function()->setInferredName(atom); }

  JSAtom* inferredName() const { return function()->inferredName(); }

  bool hasInferredName() const { return function()->hasInferredName(); }

  size_t index() { return funcDataIndex_; }

  void trace(JSTracer* trc);

  static void TraceList(JSTracer* trc, FunctionBox* listHead);

  FunctionBox* traceLink() { return traceLink_; }
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

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
#include "frontend/ParseNode.h"
#include "frontend/Stencil.h"
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

/*
 * The struct SharedContext is part of the current parser context (see
 * ParseContext). It stores information that is reused between the parser and
 * the bytecode emitter.
 */
class SharedContext {
 public:
  JSContext* const cx_;

 protected:
  enum class Kind : uint8_t { FunctionBox, Global, Eval, Module };

  Kind kind_;

  CompilationInfo& compilationInfo_;

  ThisBinding thisBinding_;

 public:
  bool strictScript : 1;
  bool localStrict : 1;

  SourceExtent extent;

 protected:
  bool allowNewTarget_ : 1;
  bool allowSuperProperty_ : 1;
  bool allowSuperCall_ : 1;
  bool allowArguments_ : 1;
  bool inWith_ : 1;
  bool needsThisTDZChecks_ : 1;

  // True if "use strict"; appears in the body instead of being inherited.
  bool hasExplicitUseStrict_ : 1;

  // Alias enum into SharedContext
  using ImmutableFlags = ImmutableScriptFlagsEnum;

  ImmutableScriptFlags immutableFlags_ = {};

  void computeAllowSyntax(Scope* scope);
  void computeInWith(Scope* scope);
  void computeThisBinding(Scope* scope);

 public:
  SharedContext(JSContext* cx, Kind kind, CompilationInfo& compilationInfo,
                Directives directives, SourceExtent extent)
      : cx_(cx),
        kind_(kind),
        compilationInfo_(compilationInfo),
        thisBinding_(ThisBinding::Global),
        strictScript(directives.strict()),
        localStrict(false),
        extent(extent),
        allowNewTarget_(false),
        allowSuperProperty_(false),
        allowSuperCall_(false),
        allowArguments_(true),
        inWith_(false),
        needsThisTDZChecks_(false),
        hasExplicitUseStrict_(false) {
    if (kind_ == Kind::FunctionBox) {
      immutableFlags_.setFlag(ImmutableFlags::IsFunction);
    } else if (kind_ == Kind::Module) {
      immutableFlags_.setFlag(ImmutableFlags::IsModule);
    } else if (kind_ == Kind::Eval) {
      immutableFlags_.setFlag(ImmutableFlags::IsForEval);
    } else {
      MOZ_ASSERT(kind_ == Kind::Global);
    }
  }

  // If this is the outermost SharedContext, the Scope that encloses
  // it. Otherwise nullptr.
  virtual Scope* compilationEnclosingScope() const = 0;

  bool isFunctionBox() const { return kind_ == Kind::FunctionBox; }
  inline FunctionBox* asFunctionBox();
  bool isModuleContext() const { return kind_ == Kind::Module; }
  inline ModuleSharedContext* asModuleContext();
  bool isGlobalContext() const { return kind_ == Kind::Global; }
  inline GlobalSharedContext* asGlobalContext();
  bool isEvalContext() const { return kind_ == Kind::Eval; }
  inline EvalSharedContext* asEvalContext();

  bool isTopLevelContext() const {
    switch (kind_) {
      case Kind::Module:
      case Kind::Global:
      case Kind::Eval:
        return true;
      case Kind::FunctionBox:
        break;
    }
    MOZ_ASSERT(kind_ == Kind::FunctionBox);
    return false;
  }

  CompilationInfo& compilationInfo() const { return compilationInfo_; }

  ThisBinding thisBinding() const { return thisBinding_; }

  bool hasModuleGoal() const {
    return immutableFlags_.hasFlag(ImmutableFlags::HasModuleGoal);
  }
  bool hasInnerFunctions() const {
    return immutableFlags_.hasFlag(ImmutableFlags::HasInnerFunctions);
  }
  bool allowNewTarget() const { return allowNewTarget_; }
  bool allowSuperProperty() const { return allowSuperProperty_; }
  bool allowSuperCall() const { return allowSuperCall_; }
  bool allowArguments() const { return allowArguments_; }
  bool inWith() const { return inWith_; }
  bool needsThisTDZChecks() const { return needsThisTDZChecks_; }

  bool hasExplicitUseStrict() const { return hasExplicitUseStrict_; }
  bool bindingsAccessedDynamically() const {
    return immutableFlags_.hasFlag(ImmutableFlags::BindingsAccessedDynamically);
  }
  bool hasDirectEval() const {
    return immutableFlags_.hasFlag(ImmutableFlags::HasDirectEval);
  }
  bool hasCallSiteObj() const {
    return immutableFlags_.hasFlag(ImmutableFlags::HasCallSiteObj);
  }
  bool treatAsRunOnce() const {
    return immutableFlags_.hasFlag(ImmutableFlags::TreatAsRunOnce);
  }

  void setExplicitUseStrict() { hasExplicitUseStrict_ = true; }
  void setBindingsAccessedDynamically() {
    immutableFlags_.setFlag(ImmutableFlags::BindingsAccessedDynamically);
  }
  void setHasDirectEval() {
    immutableFlags_.setFlag(ImmutableFlags::HasDirectEval);
  }
  void setHasCallSiteObj() {
    immutableFlags_.setFlag(ImmutableFlags::HasCallSiteObj);
  }
  void setHasModuleGoal(bool flag = true) {
    immutableFlags_.setFlag(ImmutableFlags::HasModuleGoal, flag);
  }
  void setHasInnerFunctions() {
    immutableFlags_.setFlag(ImmutableFlags::HasInnerFunctions);
  }
  void setTreatAsRunOnce(bool flag = true) {
    immutableFlags_.setFlag(ImmutableFlags::TreatAsRunOnce, flag);
  }

  ImmutableScriptFlags immutableFlags() { return immutableFlags_; }

  inline bool allBindingsClosedOver();

  bool strict() const { return strictScript || localStrict; }
  bool setLocalStrictMode(bool strict) {
    bool retVal = localStrict;
    localStrict = strict;
    return retVal;
  }
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
  FunctionBox* traceLink_;
  FunctionBox* emitLink_;

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
  AbstractScopePtr enclosingScope_;

  // Names from the named lambda scope, if a named lambda.
  LexicalScope::Data* namedLambdaBindings_;

  // Names from the function scope.
  FunctionScope::Data* functionScopeBindings_;

  // Names from the extra 'var' scope of the function, if the parameter list
  // has expressions.
  VarScope::Data* extraVarScopeBindings_;

  // Index into CompilationInfo::funcData, which contains the function
  // information, either a JSFunction* (for a FunctionBox representing a real
  // function) or a FunctionCreationData.
  size_t funcDataIndex_;

  void initWithEnclosingParseContext(ParseContext* enclosing,
                                     FunctionSyntaxKind kind, bool isArrow,
                                     bool allowSuperProperty);

 public:
  FunctionBox(JSContext* cx, FunctionBox* traceListHead, SourceExtent extent,
              CompilationInfo& compilationInfo, Directives directives,
              GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
              JSAtom* explicitName, FunctionFlags flags, size_t index);

  // Back pointer used by asm.js for error messages.
  FunctionNode* functionNode;

  mozilla::Maybe<FieldInitializers> fieldInitializers;

  uint16_t length;

  bool hasDestructuringArgs : 1;   /* parameter list contains destructuring
                                      expression */
  bool hasParameterExprs : 1;      /* parameter list contains expressions */
  bool hasDuplicateParameters : 1; /* parameter list contains duplicate names */
  bool useAsm : 1;                 /* see useAsmOrInsideUseAsm */
  bool isAnnexB : 1;     /* need to emit a synthesized Annex B assignment */
  bool wasEmitted : 1;   /* Bytecode has been emitted for this function. */
  bool emitBytecode : 1; /* need to generate bytecode for this function. */

  // Fields for use in heuristics.
  bool usesArguments : 1;  /* contains a free use of 'arguments' */
  bool usesApply : 1;      /* contains an f.apply() call */
  bool usesThis : 1;       /* contains 'this' */
  bool usesReturn : 1;     /* contains a 'return' statement */
  bool hasExprBody_ : 1;   /* arrow function with expression
                            * body like: () => 1
                            * Only used by Reflect.parse */
  bool isAsmJSModule_ : 1; /* Represents an AsmJS module */
  uint16_t nargs_;

  JSAtom* explicitName_;
  FunctionFlags flags_;

  MutableHandle<FunctionCreationData> functionCreationData() const;

  bool hasFunctionCreationData() const;
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
                                     Handle<FunctionCreationData> fun,
                                     FunctionSyntaxKind kind) {
    MOZ_ASSERT(fun.get().kind == kind);
    initWithEnclosingParseContext(enclosing, kind, fun.get().flags.isArrow(),
                                  fun.get().flags.allowSuperProperty());
  }

  void initWithEnclosingParseContext(ParseContext* enclosing, JSFunction* fun,
                                     FunctionSyntaxKind kind) {
    initWithEnclosingParseContext(enclosing, kind, fun->isArrow(),
                                  fun->allowSuperProperty());
  }

  void initFieldInitializer(ParseContext* enclosing,
                            Handle<FunctionCreationData> data);

  void setEnclosingScopeForInnerLazyFunction(
      const AbstractScopePtr& enclosingScope);
  void finish();

  JSFunction* function() const;

  // Initialize FunctionBox with a deferred allocation Function
  void initializeFunction(JSFunction* fun) {
    clobberFunction(fun);
    synchronizeArgCount();
  }

  void setAsmJSModule(JSFunction* function);
  bool isAsmJSModule() { return isAsmJSModule_; }

  void clobberFunction(JSFunction* function);

  Scope* compilationEnclosingScope() const override {
    // This is used when emitting code for the current FunctionBox and therefore
    // the enclosingScope_ must have be set correctly during initalization.

    MOZ_ASSERT(enclosingScope_);
    return enclosingScope_.scope();
  }

  bool needsCallObjectRegardlessOfBindings() const {
    // Always create a CallObject if:
    // - The scope is extensible at runtime due to sloppy eval.
    // - The function is a generator or async function. (The debugger reads the
    //   generator object directly from the frame.)

    return hasExtensibleScope() || isGenerator() || isAsync();
  }

  bool needsExtraBodyVarEnvironmentRegardlessOfBindings() const {
    MOZ_ASSERT(hasParameterExprs);
    return hasExtensibleScope();
  }

  bool isLikelyConstructorWrapper() const {
    return usesArguments && usesApply && usesThis && !usesReturn;
  }

  bool isGenerator() const {
    return immutableFlags_.hasFlag(ImmutableFlags::IsGenerator);
  }
  GeneratorKind generatorKind() const {
    return isGenerator() ? GeneratorKind::Generator
                         : GeneratorKind::NotGenerator;
  }

  bool isAsync() const {
    return immutableFlags_.hasFlag(ImmutableFlags::IsAsync);
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
  bool isLambda() const {
    if (hasFunction()) {
      return function()->isLambda();
    }
    return functionCreationData().get().flags.isLambda();
  }

  void setDeclaredArguments() {
    immutableFlags_.setFlag(ImmutableFlags::ShouldDeclareArguments);
  }
  bool declaredArguments() const {
    return immutableFlags_.hasFlag(ImmutableFlags::ShouldDeclareArguments);
  }

  bool hasRest() const {
    return immutableFlags_.hasFlag(ImmutableFlags::HasRest);
  }
  void setHasRest() { immutableFlags_.setFlag(ImmutableFlags::HasRest); }

  bool hasExprBody() const { return hasExprBody_; }
  void setHasExprBody() {
    MOZ_ASSERT(isArrow());
    hasExprBody_ = true;
  }

  bool functionHasExtraBodyVarScope() {
    return immutableFlags_.hasFlag(
        ImmutableFlags::FunctionHasExtraBodyVarScope);
  }
  bool hasExtensibleScope() const {
    return immutableFlags_.hasFlag(ImmutableFlags::FunHasExtensibleScope);
  }
  bool hasThisBinding() const {
    return immutableFlags_.hasFlag(ImmutableFlags::FunctionHasThisBinding);
  }
  bool argumentsHasVarBinding() const {
    return immutableFlags_.hasFlag(ImmutableFlags::ArgumentsHasVarBinding);
  }
  bool alwaysNeedsArgsObj() const {
    return immutableFlags_.hasFlag(ImmutableFlags::AlwaysNeedsArgsObj);
  }
  bool needsHomeObject() const {
    return immutableFlags_.hasFlag(ImmutableFlags::NeedsHomeObject);
  }
  bool isDerivedClassConstructor() const {
    return immutableFlags_.hasFlag(ImmutableFlags::IsDerivedClassConstructor);
  }
  bool isNamedLambda() const { return flags_.isNamedLambda(explicitName()); }
  bool isGetter() const { return flags_.isGetter(); }
  bool isSetter() const { return flags_.isSetter(); }
  bool isMethod() const { return flags_.isMethod(); }
  bool isClassConstructor() const { return flags_.isClassConstructor(); }

  bool isInterpreted() const { return flags_.hasBaseScript(); }
  void setIsInterpreted(bool interpreted) {
    flags_.setFlags(FunctionFlags::BASESCRIPT, interpreted);
  }

  void initLazyScript(BaseScript* script) {
    function()->initLazyScript(script);
  }

  FunctionFlags::FunctionKind kind() { return flags_.kind(); }

  JSAtom* explicitName() const { return explicitName_; }

  void setHasExtensibleScope() {
    immutableFlags_.setFlag(ImmutableFlags::FunHasExtensibleScope);
  }
  void setHasThisBinding() {
    immutableFlags_.setFlag(ImmutableFlags::FunctionHasThisBinding);
  }
  void setArgumentsHasVarBinding() {
    immutableFlags_.setFlag(ImmutableFlags::ArgumentsHasVarBinding);
  }
  void setAlwaysNeedsArgsObj() {
    MOZ_ASSERT(argumentsHasVarBinding());
    immutableFlags_.setFlag(ImmutableFlags::AlwaysNeedsArgsObj);
  }
  void setNeedsHomeObject() {
    MOZ_ASSERT_IF(hasFunction(), function()->allowSuperProperty());
    MOZ_ASSERT_IF(!hasFunction(),
                  functionCreationData().get().flags.allowSuperProperty());
    immutableFlags_.setFlag(ImmutableFlags::NeedsHomeObject);
  }
  void setDerivedClassConstructor() {
    MOZ_ASSERT_IF(hasFunction(), function()->isClassConstructor());
    MOZ_ASSERT_IF(!hasFunction(),
                  functionCreationData().get().flags.isClassConstructor());
    immutableFlags_.setFlag(ImmutableFlags::IsDerivedClassConstructor);
  }
  void setFunctionHasExtraBodyVarScope() {
    immutableFlags_.setFlag(ImmutableFlags::FunctionHasExtraBodyVarScope);
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
};

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

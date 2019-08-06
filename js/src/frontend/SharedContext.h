/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SharedContext_h
#define frontend_SharedContext_h

#include "jspubtd.h"
#include "jstypes.h"

#include "ds/InlineTable.h"
#include "frontend/FunctionCreationData.h"
#include "frontend/ParseNode.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSFunction.h"
#include "vm/JSScript.h"

namespace js {
namespace frontend {

class ParseContext;
class ParseNode;

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
  Spread
};

static inline bool StatementKindIsLoop(StatementKind kind) {
  return kind == StatementKind::ForLoop || kind == StatementKind::ForInLoop ||
         kind == StatementKind::ForOfLoop || kind == StatementKind::DoLoop ||
         kind == StatementKind::WhileLoop || kind == StatementKind::Spread;
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

  ThisBinding thisBinding_;

 public:
  bool strictScript : 1;
  bool localStrict : 1;
  bool extraWarnings : 1;

 protected:
  bool allowNewTarget_ : 1;
  bool allowSuperProperty_ : 1;
  bool allowSuperCall_ : 1;
  bool allowArguments_ : 1;
  bool inWith_ : 1;
  bool needsThisTDZChecks_ : 1;

  // True if "use strict"; appears in the body instead of being inherited.
  bool hasExplicitUseStrict_ : 1;

  // The (static) bindings of this script need to support dynamic name
  // read/write access. Here, 'dynamic' means dynamic dictionary lookup on
  // the scope chain for a dynamic set of keys. The primary examples are:
  //  - direct eval
  //  - function::
  //  - with
  // since both effectively allow any name to be accessed. Non-examples are:
  //  - upvars of nested functions
  //  - function statement
  // since the set of assigned name is known dynamically.
  //
  // Note: access through the arguments object is not considered dynamic
  // binding access since it does not go through the normal name lookup
  // mechanism. This is debatable and could be changed (although care must be
  // taken not to turn off the whole 'arguments' optimization). To answer the
  // more general "is this argument aliased" question, script->needsArgsObj
  // should be tested (see JSScript::argIsAliased).
  bool bindingsAccessedDynamically_ : 1;

  // A direct eval occurs in the body of the script.
  bool hasDirectEval_ : 1;

  void computeAllowSyntax(Scope* scope);
  void computeInWith(Scope* scope);
  void computeThisBinding(Scope* scope);

 public:
  SharedContext(JSContext* cx, Kind kind, Directives directives,
                bool extraWarnings)
      : cx_(cx),
        kind_(kind),
        thisBinding_(ThisBinding::Global),
        strictScript(directives.strict()),
        localStrict(false),
        extraWarnings(extraWarnings),
        allowNewTarget_(false),
        allowSuperProperty_(false),
        allowSuperCall_(false),
        allowArguments_(true),
        inWith_(false),
        needsThisTDZChecks_(false),
        hasExplicitUseStrict_(false),
        bindingsAccessedDynamically_(false),
        hasDirectEval_(false) {}

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

  ThisBinding thisBinding() const { return thisBinding_; }

  bool allowNewTarget() const { return allowNewTarget_; }
  bool allowSuperProperty() const { return allowSuperProperty_; }
  bool allowSuperCall() const { return allowSuperCall_; }
  bool allowArguments() const { return allowArguments_; }
  bool inWith() const { return inWith_; }
  bool needsThisTDZChecks() const { return needsThisTDZChecks_; }

  bool hasExplicitUseStrict() const { return hasExplicitUseStrict_; }
  bool bindingsAccessedDynamically() const {
    return bindingsAccessedDynamically_;
  }
  bool hasDirectEval() const { return hasDirectEval_; }

  void setExplicitUseStrict() { hasExplicitUseStrict_ = true; }
  void setBindingsAccessedDynamically() { bindingsAccessedDynamically_ = true; }
  void setHasDirectEval() { hasDirectEval_ = true; }

  inline bool allBindingsClosedOver();

  bool strict() const { return strictScript || localStrict; }
  bool setLocalStrictMode(bool strict) {
    bool retVal = localStrict;
    localStrict = strict;
    return retVal;
  }

  // JSOPTION_EXTRA_WARNINGS warnings or strict mode errors.
  bool needStrictChecks() const { return strict() || extraWarnings; }
};

class MOZ_STACK_CLASS GlobalSharedContext : public SharedContext {
  ScopeKind scopeKind_;

 public:
  Rooted<GlobalScope::Data*> bindings;

  GlobalSharedContext(JSContext* cx, ScopeKind scopeKind, Directives directives,
                      bool extraWarnings)
      : SharedContext(cx, Kind::Global, directives, extraWarnings),
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
                    Scope* enclosingScope, Directives directives,
                    bool extraWarnings);

  Scope* compilationEnclosingScope() const override { return enclosingScope_; }
};

inline EvalSharedContext* SharedContext::asEvalContext() {
  MOZ_ASSERT(isEvalContext());
  return static_cast<EvalSharedContext*>(this);
}

enum class HasHeritage : bool { No, Yes };

// Data used to instantiate the lazy script before script emission.
struct LazyScriptCreationData {
  frontend::AtomVector closedOverBindings;

  // This is traced by the functionbox which owns this LazyScriptCreationData
  FunctionBoxVector innerFunctionBoxes;
  bool strict;

  explicit LazyScriptCreationData(JSContext* cx)
      : closedOverBindings(), innerFunctionBoxes(cx), strict(false) {}

  bool init(JSContext* cx, const frontend::AtomVector& COB,
            FunctionBoxVector& innerBoxes, bool isStrict) {
    strict = isStrict;
    // Copy out of the stack allocated vectors.
    if (!innerFunctionBoxes.appendAll(innerBoxes)) {
      return false;
    }

    if (!closedOverBindings.appendAll(COB)) {
      return false;
    }
    return true;
  }
};

class FunctionBox : public ObjectBox, public SharedContext {
  // The parser handles tracing the fields below via the TraceListNode linked
  // list.

  // This field is used for two purposes:
  //   * If this FunctionBox refers to the function being compiled, this field
  //     holds its enclosing scope, used for compilation.
  //   * If this FunctionBox refers to a lazy child of the function being
  //     compiled, this field holds the child's immediately enclosing scope.
  //     Once compilation succeeds, we will store it in the child's
  //     LazyScript.  (Debugger may become confused if LazyScripts refer to
  //     partially initialized enclosing scopes, so we must avoid storing the
  //     scope in the LazyScript until compilation has completed
  //     successfully.)
  Scope* enclosingScope_;

  // Names from the named lambda scope, if a named lambda.
  LexicalScope::Data* namedLambdaBindings_;

  // Names from the function scope.
  FunctionScope::Data* functionScopeBindings_;

  // Names from the extra 'var' scope of the function, if the parameter list
  // has expressions.
  VarScope::Data* extraVarScopeBindings_;

  FunctionBox(JSContext* cx, TraceListNode* traceListHead,
              uint32_t toStringStart, Directives directives, bool extraWarnings,
              GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
              bool isArrow, bool isNamedLambda, bool isGetter, bool isSetter,
              bool isMethod, bool isInterpreted, bool isInterpretedLazy,
              FunctionFlags::FunctionKind kind, JSAtom* explicitName);

  void initWithEnclosingScope(Scope* enclosingScope, JSFunction* fun);

  void initWithEnclosingParseContext(ParseContext* enclosing,
                                     FunctionSyntaxKind kind, bool isArrow,
                                     bool allowSuperProperty);

 public:
  // Back pointer used by asm.js for error messages.
  FunctionNode* functionNode;

  uint32_t bufStart;
  uint32_t bufEnd;
  uint32_t startLine;
  uint32_t startColumn;
  uint32_t toStringStart;
  uint32_t toStringEnd;
  uint16_t length;

  bool isGenerator_ : 1;         /* generator function or async generator */
  bool isAsync_ : 1;             /* async function or async generator */
  bool hasDestructuringArgs : 1; /* parameter list contains destructuring
                                    expression */
  bool hasParameterExprs : 1;    /* parameter list contains expressions */
  bool hasDirectEvalInParameterExpr : 1; /* parameter list contains direct eval
                                          */
  bool hasDuplicateParameters : 1; /* parameter list contains duplicate names */
  bool useAsm : 1;                 /* see useAsmOrInsideUseAsm */
  bool isAnnexB : 1;   /* need to emit a synthesized Annex B assignment */
  bool wasEmitted : 1; /* Bytecode has been emitted for this function. */

  // Fields for use in heuristics.
  bool declaredArguments : 1; /* the Parser declared 'arguments' */
  bool usesArguments : 1;     /* contains a free use of 'arguments' */
  bool usesApply : 1;         /* contains an f.apply() call */
  bool usesThis : 1;          /* contains 'this' */
  bool usesReturn : 1;        /* contains a 'return' statement */
  bool hasRest_ : 1;          /* has rest parameter */
  bool hasExprBody_ : 1;      /* arrow function with expression
                               * body like: () => 1
                               * Only used by Reflect.parse */

  // This function does something that can extend the set of bindings in its
  // call objects --- it does a direct eval in non-strict code, or includes a
  // function statement (as opposed to a function definition).
  //
  // This flag is *not* inherited by enclosed or enclosing functions; it
  // applies only to the function in whose flags it appears.
  //
  bool hasExtensibleScope_ : 1;

  // Technically, every function has a binding named 'arguments'. Internally,
  // this binding is only added when 'arguments' is mentioned by the function
  // body. This flag indicates whether 'arguments' has been bound either
  // through implicit use:
  //   function f() { return arguments }
  // or explicit redeclaration:
  //   function f() { var arguments; return arguments }
  //
  // Note 1: overwritten arguments (function() { arguments = 3 }) will cause
  // this flag to be set but otherwise require no special handling:
  // 'arguments' is just a local variable and uses of 'arguments' will just
  // read the local's current slot which may have been assigned. The only
  // special semantics is that the initial value of 'arguments' is the
  // arguments object (not undefined, like normal locals).
  //
  // Note 2: if 'arguments' is bound as a formal parameter, there will be an
  // 'arguments' in Bindings, but, as the "LOCAL" in the name indicates, this
  // flag will not be set. This is because, as a formal, 'arguments' will
  // have no special semantics: the initial value is unconditionally the
  // actual argument (or undefined if nactual < nformal).
  //
  bool argumentsHasLocalBinding_ : 1;

  // In many cases where 'arguments' has a local binding (as described above)
  // we do not need to actually create an arguments object in the function
  // prologue: instead we can analyze how 'arguments' is used (using the
  // simple dataflow analysis in analyzeSSA) to determine that uses of
  // 'arguments' can just read from the stack frame directly. However, the
  // dataflow analysis only looks at how JSOP_ARGUMENTS is used, so it will
  // be unsound in several cases. The frontend filters out such cases by
  // setting this flag which eagerly sets script->needsArgsObj to true.
  //
  bool definitelyNeedsArgsObj_ : 1;

  bool needsHomeObject_ : 1;
  bool isDerivedClassConstructor_ : 1;

  // Whether this function has a .this binding. If true, we need to emit
  // JSOP_FUNCTIONTHIS in the prologue to initialize it.
  bool hasThisBinding_ : 1;

  // Whether this function has nested functions.
  bool hasInnerFunctions_ : 1;

  // Whether this function is an arrow function
  bool isArrow_ : 1;

  bool isNamedLambda_ : 1;
  bool isGetter_ : 1;
  bool isSetter_ : 1;
  bool isMethod_ : 1;

  bool isInterpreted_ : 1;
  bool isInterpretedLazy_ : 1;

  FunctionFlags::FunctionKind kind_;
  JSAtom* explicitName_;

  uint16_t nargs_;

  mozilla::Maybe<LazyScriptCreationData> lazyScriptData_;

  mozilla::Maybe<LazyScriptCreationData>& lazyScriptData() {
    return lazyScriptData_;
  }

  mozilla::Maybe<FunctionCreationData> functionCreationData_;

  bool hasFunctionCreationData() { return functionCreationData_.isSome(); }

  const mozilla::Maybe<FunctionCreationData>& functionCreationData() const {
    return functionCreationData_;
  }
  mozilla::Maybe<FunctionCreationData>& functionCreationData() {
    return functionCreationData_;
  }

  Handle<FunctionCreationData> functionCreationDataHandle() {
    // This is safe because the FunctionCreationData are marked
    // via ParserBase -> FunctionBox -> FunctionCreationData.
    return Handle<FunctionCreationData>::fromMarkedLocation(
        functionCreationData_.ptr());
  }

  FunctionBox(JSContext* cx, TraceListNode* traceListHead, JSFunction* fun,
              uint32_t toStringStart, Directives directives, bool extraWarnings,
              GeneratorKind generatorKind, FunctionAsyncKind asyncKind);

  FunctionBox(JSContext* cx, TraceListNode* traceListHead,
              Handle<FunctionCreationData> data, uint32_t toStringStart,
              Directives directives, bool extraWarnings,
              GeneratorKind generatorKind, FunctionAsyncKind asyncKind);

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

  void initWithEnclosingParseContext(ParseContext* enclosing,
                                     Handle<FunctionCreationData> fun,
                                     FunctionSyntaxKind kind) {
    initWithEnclosingParseContext(enclosing, kind, fun.get().flags.isArrow(),
                                  fun.get().flags.allowSuperProperty());
  }

  void initWithEnclosingParseContext(ParseContext* enclosing, JSFunction* fun,
                                     FunctionSyntaxKind kind) {
    initWithEnclosingParseContext(enclosing, kind, fun->isArrow(),
                                  fun->allowSuperProperty());
  }

  void initFieldInitializer(ParseContext* enclosing, JSFunction* fun,
                            HasHeritage hasHeritage);
  void initFieldInitializer(ParseContext* enclosing,
                            Handle<FunctionCreationData> data,
                            HasHeritage hasHeritage);

  inline bool isLazyFunctionWithoutEnclosingScope() const {
    return isInterpretedLazy() &&
           !function()->lazyScript()->hasEnclosingScope();
  }
  void setEnclosingScopeForInnerLazyFunction(Scope* enclosingScope);
  void finish();

  JSFunction* function() const { return &object()->as<JSFunction>(); }

  // Initialize FunctionBox with a deferred allocation Function
  void initializeFunction(JSFunction* fun) {
    clobberFunction(fun);
    synchronizeArgCount();
  }

  void clobberFunction(JSFunction* function) {
    gcThing = function;
    // After clobbering, these flags need to be updated
    setIsInterpreted(function->isInterpreted());
    setIsInterpretedLazy(function->isInterpretedLazy());
  }

  Scope* compilationEnclosingScope() const override {
    // This method is used to distinguish the outermost SharedContext. If
    // a FunctionBox is the outermost SharedContext, it must be a lazy
    // function.

    // If the function is lazy and it has enclosing scope, the function is
    // being delazified.  In that case the enclosingScope_ field is copied
    // from the lazy function at the beginning of delazification and should
    // keep pointing the same scope.
    MOZ_ASSERT_IF(
        isInterpretedLazy() && function()->lazyScript()->hasEnclosingScope(),
        enclosingScope_ == function()->lazyScript()->enclosingScope());

    // If this FunctionBox is a lazy child of the function we're actually
    // compiling, then it is not the outermost SharedContext, so this
    // method should return nullptr."
    if (isLazyFunctionWithoutEnclosingScope()) {
      return nullptr;
    }

    return enclosingScope_;
  }

  bool needsCallObjectRegardlessOfBindings() const {
    return hasExtensibleScope() || needsHomeObject() ||
           isDerivedClassConstructor() || isGenerator() || isAsync();
  }

  bool hasExtraBodyVarScope() const {
    return hasParameterExprs &&
           (extraVarScopeBindings_ ||
            needsExtraBodyVarEnvironmentRegardlessOfBindings());
  }

  bool needsExtraBodyVarEnvironmentRegardlessOfBindings() const {
    MOZ_ASSERT(hasParameterExprs);
    return hasExtensibleScope() || needsDotGeneratorName();
  }

  bool isLikelyConstructorWrapper() const {
    return usesArguments && usesApply && usesThis && !usesReturn;
  }

  bool isGenerator() const { return isGenerator_; }
  GeneratorKind generatorKind() const {
    return isGenerator() ? GeneratorKind::Generator
                         : GeneratorKind::NotGenerator;
  }

  bool isAsync() const { return isAsync_; }
  FunctionAsyncKind asyncKind() const {
    return isAsync() ? FunctionAsyncKind::AsyncFunction
                     : FunctionAsyncKind::SyncFunction;
  }

  bool needsFinalYield() const { return isGenerator() || isAsync(); }
  bool needsDotGeneratorName() const { return isGenerator() || isAsync(); }
  bool needsIteratorResult() const { return isGenerator() && !isAsync(); }
  bool needsPromiseResult() const { return isAsync() && !isGenerator(); }

  bool isArrow() const { return isArrow_; }
  bool isLambda() const {
    if (hasObject()) {
      return function()->isLambda();
    }
    MOZ_ASSERT(functionCreationData().isSome());
    return functionCreationData()->flags.isLambda();
  }

  bool hasRest() const { return hasRest_; }
  void setHasRest() { hasRest_ = true; }

  bool hasExprBody() const { return hasExprBody_; }
  void setHasExprBody() {
    MOZ_ASSERT(isArrow());
    hasExprBody_ = true;
  }

  bool hasExtensibleScope() const { return hasExtensibleScope_; }
  bool hasThisBinding() const { return hasThisBinding_; }
  bool argumentsHasLocalBinding() const { return argumentsHasLocalBinding_; }
  bool definitelyNeedsArgsObj() const { return definitelyNeedsArgsObj_; }
  bool needsHomeObject() const { return needsHomeObject_; }
  bool isDerivedClassConstructor() const { return isDerivedClassConstructor_; }
  bool hasInnerFunctions() const { return hasInnerFunctions_; }
  bool isNamedLambda() const { return isNamedLambda_; }
  bool isGetter() const { return isGetter_; }
  bool isSetter() const { return isSetter_; }
  bool isMethod() const { return isMethod_; }

  bool isInterpreted() const { return isInterpreted_; }
  void setIsInterpreted(bool interpreted) { isInterpreted_ = interpreted; }
  bool isInterpretedLazy() const { return isInterpretedLazy_; }
  void setIsInterpretedLazy(bool interpretedLazy) {
    isInterpretedLazy_ = interpretedLazy;
  }

  void initLazyScript(LazyScript* script) {
    function()->initLazyScript(script);
    setIsInterpretedLazy(function()->isInterpretedLazy());
  }

  FunctionFlags::FunctionKind kind() { return kind_; }

  JSAtom* explicitName() const { return explicitName_; }

  void setHasExtensibleScope() { hasExtensibleScope_ = true; }
  void setHasThisBinding() { hasThisBinding_ = true; }
  void setArgumentsHasLocalBinding() { argumentsHasLocalBinding_ = true; }
  void setDefinitelyNeedsArgsObj() {
    MOZ_ASSERT(argumentsHasLocalBinding_);
    definitelyNeedsArgsObj_ = true;
  }
  void setNeedsHomeObject() {
    MOZ_ASSERT_IF(hasObject(), function()->allowSuperProperty());
    MOZ_ASSERT_IF(!hasObject(), functionCreationData().isSome());
    MOZ_ASSERT_IF(!hasObject(),
                  functionCreationData()->flags.allowSuperProperty());
    needsHomeObject_ = true;
  }
  void setDerivedClassConstructor() {
    MOZ_ASSERT_IF(hasObject(), function()->isClassConstructor());
    MOZ_ASSERT_IF(!hasObject(), functionCreationData().isSome());
    MOZ_ASSERT_IF(!hasObject(),
                  functionCreationData()->flags.isClassConstructor());
    isDerivedClassConstructor_ = true;
  }
  void setHasInnerFunctions() { hasInnerFunctions_ = true; }

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
    return explicitName() || argumentsHasLocalBinding() || isGenerator() ||
           isAsync();
  }

  // Return whether this or an enclosing function is being parsed and
  // validated as asm.js. Note: if asm.js validation fails, this will be false
  // while the function is being reparsed. This flag can be used to disable
  // certain parsing features that are necessary in general, but unnecessary
  // for validated asm.js.
  bool useAsmOrInsideUseAsm() const { return useAsm; }

  void setStart(uint32_t offset, uint32_t line, uint32_t column) {
    bufStart = offset;
    startLine = line;
    startColumn = column;
  }

  void setEnd(uint32_t end) {
    // For all functions except class constructors, the buffer and
    // toString ending positions are the same. Class constructors override
    // the toString ending position with the end of the class definition.
    bufEnd = toStringEnd = end;
  }

  void setArgCount(uint16_t args) { nargs_ = args; }

  size_t nargs() { return nargs_; }

  // Flush the acquired argCount to the associated function.
  // If the function doesn't exist yet, this of course isn't necessary;
  void synchronizeArgCount() {
    if (hasObject()) {
      function()->setArgCount(nargs_);
    }
  }

  void trace(JSTracer* trc) override;
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

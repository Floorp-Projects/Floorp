/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SharedContext_h
#define frontend_SharedContext_h

#include "jsatom.h"
#include "jsopcode.h"
#include "jspubtd.h"
#include "jsscript.h"
#include "jstypes.h"

#include "builtin/ModuleObject.h"
#include "ds/InlineTable.h"
#include "frontend/ParseNode.h"
#include "frontend/TokenStream.h"
#include "vm/EnvironmentObject.h"

namespace js {
namespace frontend {

enum class StatementKind : uint8_t
{
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

    // Used only by BytecodeEmitter.
    Spread
};

static inline bool
StatementKindIsLoop(StatementKind kind)
{
    return kind == StatementKind::ForLoop ||
           kind == StatementKind::ForInLoop ||
           kind == StatementKind::ForOfLoop ||
           kind == StatementKind::DoLoop ||
           kind == StatementKind::WhileLoop ||
           kind == StatementKind::Spread;
}

static inline bool
StatementKindIsUnlabeledBreakTarget(StatementKind kind)
{
    return StatementKindIsLoop(kind) || kind == StatementKind::Switch;
}

// A base class for nestable structures in the frontend, such as statements
// and scopes.
template <typename Concrete>
class MOZ_STACK_CLASS Nestable
{
    Concrete** stack_;
    Concrete*  enclosing_;

  protected:
    explicit Nestable(Concrete** stack)
      : stack_(stack),
        enclosing_(*stack)
    {
        *stack_ = static_cast<Concrete*>(this);
    }

    // These method are protected. Some derived classes, such as ParseContext,
    // do not expose the ability to walk the stack.
    Concrete* enclosing() const {
        return enclosing_;
    }

    template <typename Predicate /* (Concrete*) -> bool */>
    static Concrete* findNearest(Concrete* it, Predicate predicate) {
        while (it && !predicate(it))
            it = it->enclosing();
        return it;
    }

    template <typename T>
    static T* findNearest(Concrete* it) {
        while (it && !it->template is<T>())
            it = it->enclosing();
        return it ? &it->template as<T>() : nullptr;
    }

    template <typename T, typename Predicate /* (T*) -> bool */>
    static T* findNearest(Concrete* it, Predicate predicate) {
        while (it && (!it->template is<T>() || !predicate(&it->template as<T>())))
            it = it->enclosing();
        return it ? &it->template as<T>() : nullptr;
    }

  public:
    ~Nestable() {
        MOZ_ASSERT(*stack_ == static_cast<Concrete*>(this));
        *stack_ = enclosing_;
    }
};

// These flags apply to both global and function contexts.
class AnyContextFlags
{
    // This class's data is all private and so only visible to these friends.
    friend class SharedContext;

    // True if "use strict"; appears in the body instead of being inherited.
    bool hasExplicitUseStrict:1;

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
    bool bindingsAccessedDynamically:1;

    // Whether this script, or any of its inner scripts contains a debugger
    // statement which could potentially read or write anywhere along the
    // scope chain.
    bool hasDebuggerStatement:1;

    // A direct eval occurs in the body of the script.
    bool hasDirectEval:1;

  public:
    AnyContextFlags()
     :  hasExplicitUseStrict(false),
        bindingsAccessedDynamically(false),
        hasDebuggerStatement(false),
        hasDirectEval(false)
    { }
};

class FunctionContextFlags
{
    // This class's data is all private and so only visible to these friends.
    friend class FunctionBox;

    // The function or a function that encloses it may define new local names
    // at runtime through means other than calling eval.
    bool mightAliasLocals:1;

    // This function does something that can extend the set of bindings in its
    // call objects --- it does a direct eval in non-strict code, or includes a
    // function statement (as opposed to a function definition).
    //
    // This flag is *not* inherited by enclosed or enclosing functions; it
    // applies only to the function in whose flags it appears.
    //
    bool hasExtensibleScope:1;

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
    bool argumentsHasLocalBinding:1;

    // In many cases where 'arguments' has a local binding (as described above)
    // we do not need to actually create an arguments object in the function
    // prologue: instead we can analyze how 'arguments' is used (using the
    // simple dataflow analysis in analyzeSSA) to determine that uses of
    // 'arguments' can just read from the stack frame directly. However, the
    // dataflow analysis only looks at how JSOP_ARGUMENTS is used, so it will
    // be unsound in several cases. The frontend filters out such cases by
    // setting this flag which eagerly sets script->needsArgsObj to true.
    //
    bool definitelyNeedsArgsObj:1;

    bool needsHomeObject:1;
    bool isDerivedClassConstructor:1;

    // Whether this function has a .this binding. If true, we need to emit
    // JSOP_FUNCTIONTHIS in the prologue to initialize it.
    bool hasThisBinding:1;

    // Whether this function has nested functions.
    bool hasInnerFunctions:1;

  public:
    FunctionContextFlags()
     :  mightAliasLocals(false),
        hasExtensibleScope(false),
        argumentsHasLocalBinding(false),
        definitelyNeedsArgsObj(false),
        needsHomeObject(false),
        isDerivedClassConstructor(false),
        hasThisBinding(false),
        hasInnerFunctions(false)
    { }
};

// List of directives that may be encountered in a Directive Prologue (ES5 15.1).
class Directives
{
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
    bool operator!=(const Directives& rhs) const {
        return !(*this == rhs);
    }
};

// The kind of this-binding for the current scope. Note that arrow functions
// (and generator expression lambdas) have a lexical this-binding so their
// ThisBinding is the same as the ThisBinding of their enclosing scope and can
// be any value.
enum class ThisBinding { Global, Function, Module };

class GlobalSharedContext;
class EvalSharedContext;
class ModuleSharedContext;

/*
 * The struct SharedContext is part of the current parser context (see
 * ParseContext). It stores information that is reused between the parser and
 * the bytecode emitter.
 */
class SharedContext
{
  public:
    ExclusiveContext* const context;
    AnyContextFlags anyCxFlags;
    bool strictScript;
    bool localStrict;
    bool extraWarnings;

  protected:
    enum class Kind {
        ObjectBox,
        Global,
        Eval,
        Module
    };

    Kind kind_;

    ThisBinding thisBinding_;

    bool allowNewTarget_;
    bool allowSuperProperty_;
    bool allowSuperCall_;
    bool inWith_;
    bool needsThisTDZChecks_;

    void computeAllowSyntax(Scope* scope);
    void computeInWith(Scope* scope);
    void computeThisBinding(Scope* scope);

  public:
    SharedContext(ExclusiveContext* cx, Kind kind, Directives directives, bool extraWarnings)
      : context(cx),
        anyCxFlags(),
        strictScript(directives.strict()),
        localStrict(false),
        extraWarnings(extraWarnings),
        kind_(kind),
        thisBinding_(ThisBinding::Global),
        allowNewTarget_(false),
        allowSuperProperty_(false),
        allowSuperCall_(false),
        inWith_(false),
        needsThisTDZChecks_(false)
    { }

    // If this is the outermost SharedContext, the Scope that encloses
    // it. Otherwise nullptr.
    virtual Scope* compilationEnclosingScope() const = 0;

    virtual ObjectBox* toObjectBox() { return nullptr; }
    bool isObjectBox() { return toObjectBox(); }
    bool isFunctionBox() { return isObjectBox() && toObjectBox()->isFunctionBox(); }
    inline FunctionBox* asFunctionBox();
    bool isModuleContext() { return kind_ == Kind::Module; }
    inline ModuleSharedContext* asModuleContext();
    bool isGlobalContext() { return kind_ == Kind::Global; }
    inline GlobalSharedContext* asGlobalContext();
    bool isEvalContext() { return kind_ == Kind::Eval; }
    inline EvalSharedContext* asEvalContext();

    ThisBinding thisBinding()          const { return thisBinding_; }

    bool allowNewTarget()              const { return allowNewTarget_; }
    bool allowSuperProperty()          const { return allowSuperProperty_; }
    bool allowSuperCall()              const { return allowSuperCall_; }
    bool inWith()                      const { return inWith_; }
    bool needsThisTDZChecks()          const { return needsThisTDZChecks_; }

    bool hasExplicitUseStrict()        const { return anyCxFlags.hasExplicitUseStrict; }
    bool bindingsAccessedDynamically() const { return anyCxFlags.bindingsAccessedDynamically; }
    bool hasDebuggerStatement()        const { return anyCxFlags.hasDebuggerStatement; }
    bool hasDirectEval()               const { return anyCxFlags.hasDirectEval; }

    void setExplicitUseStrict()           { anyCxFlags.hasExplicitUseStrict        = true; }
    void setBindingsAccessedDynamically() { anyCxFlags.bindingsAccessedDynamically = true; }
    void setHasDebuggerStatement()        { anyCxFlags.hasDebuggerStatement        = true; }
    void setHasDirectEval()               { anyCxFlags.hasDirectEval               = true; }

    inline bool allBindingsClosedOver();

    bool strict() const {
        return strictScript || localStrict;
    }
    bool setLocalStrictMode(bool strict) {
        bool retVal = localStrict;
        localStrict = strict;
        return retVal;
    }

    // JSOPTION_EXTRA_WARNINGS warnings or strict mode errors.
    bool needStrictChecks() const {
        return strict() || extraWarnings;
    }

    bool isDotVariable(JSAtom* atom) const {
        return atom == context->names().dotGenerator || atom == context->names().dotThis;
    }
};

class MOZ_STACK_CLASS GlobalSharedContext : public SharedContext
{
    ScopeKind scopeKind_;

  public:
    Rooted<GlobalScope::Data*> bindings;

    GlobalSharedContext(ExclusiveContext* cx, ScopeKind scopeKind, Directives directives,
                        bool extraWarnings)
      : SharedContext(cx, Kind::Global, directives, extraWarnings),
        scopeKind_(scopeKind),
        bindings(cx)
    {
        MOZ_ASSERT(scopeKind == ScopeKind::Global || scopeKind == ScopeKind::NonSyntactic);
        thisBinding_ = ThisBinding::Global;
    }

    Scope* compilationEnclosingScope() const override {
        return nullptr;
    }

    ScopeKind scopeKind() const {
        return scopeKind_;
    }
};

inline GlobalSharedContext*
SharedContext::asGlobalContext()
{
    MOZ_ASSERT(isGlobalContext());
    return static_cast<GlobalSharedContext*>(this);
}

class MOZ_STACK_CLASS EvalSharedContext : public SharedContext
{
    RootedScope enclosingScope_;

  public:
    Rooted<EvalScope::Data*> bindings;

    EvalSharedContext(ExclusiveContext* cx, JSObject* enclosingEnv, Scope* enclosingScope,
                      Directives directives, bool extraWarnings);

    Scope* compilationEnclosingScope() const override {
        return enclosingScope_;
    }
};

inline EvalSharedContext*
SharedContext::asEvalContext()
{
    MOZ_ASSERT(isEvalContext());
    return static_cast<EvalSharedContext*>(this);
}

class FunctionBox : public ObjectBox, public SharedContext
{
    // The parser handles tracing the fields below via the ObjectBox linked
    // list.

    Scope* enclosingScope_;

    // Names from the named lambda scope, if a named lambda.
    LexicalScope::Data* namedLambdaBindings_;

    // Names from the function scope.
    FunctionScope::Data* functionScopeBindings_;

    // Names from the extra 'var' scope of the function, if the parameter list
    // has expressions.
    VarScope::Data* extraVarScopeBindings_;

    void initWithEnclosingScope(Scope* enclosingScope);

  public:
    ParseNode*      functionNode;           /* back pointer used by asm.js for error messages */
    uint32_t        bufStart;
    uint32_t        bufEnd;
    uint32_t        startLine;
    uint32_t        startColumn;
    uint16_t        length;

    uint8_t         generatorKindBits_;     /* The GeneratorKind of this function. */
    bool            isGenexpLambda:1;       /* lambda from generator expression */
    bool            hasDestructuringArgs:1; /* parameter list contains destructuring expression */
    bool            hasParameterExprs:1;    /* parameter list contains expressions */
    bool            hasDirectEvalInParameterExpr:1; /* parameter list contains direct eval */
    bool            useAsm:1;               /* see useAsmOrInsideUseAsm */
    bool            insideUseAsm:1;         /* see useAsmOrInsideUseAsm */
    bool            isAnnexB:1;             /* need to emit a synthesized Annex B assignment */
    bool            wasEmitted:1;           /* Bytecode has been emitted for this function. */

    // Fields for use in heuristics.
    bool            declaredArguments:1;    /* the Parser declared 'arguments' */
    bool            usesArguments:1;        /* contains a free use of 'arguments' */
    bool            usesApply:1;            /* contains an f.apply() call */
    bool            usesThis:1;             /* contains 'this' */
    bool            usesReturn:1;           /* contains a 'return' statement */

    FunctionContextFlags funCxFlags;

    FunctionBox(ExclusiveContext* cx, LifoAlloc& alloc, ObjectBox* traceListHead, JSFunction* fun,
                Directives directives, bool extraWarnings, GeneratorKind generatorKind);

    MutableHandle<LexicalScope::Data*> namedLambdaBindings() {
        MOZ_ASSERT(context->compartment()->runtimeFromAnyThread()->keepAtoms());
        return MutableHandle<LexicalScope::Data*>::fromMarkedLocation(&namedLambdaBindings_);
    }

    MutableHandle<FunctionScope::Data*> functionScopeBindings() {
        MOZ_ASSERT(context->compartment()->runtimeFromAnyThread()->keepAtoms());
        return MutableHandle<FunctionScope::Data*>::fromMarkedLocation(&functionScopeBindings_);
    }

    MutableHandle<VarScope::Data*> extraVarScopeBindings() {
        MOZ_ASSERT(context->compartment()->runtimeFromAnyThread()->keepAtoms());
        return MutableHandle<VarScope::Data*>::fromMarkedLocation(&extraVarScopeBindings_);
    }

    void initFromLazyFunction();
    void initStandaloneFunction(Scope* enclosingScope);
    void initWithEnclosingParseContext(ParseContext* enclosing, FunctionSyntaxKind kind);

    ObjectBox* toObjectBox() override { return this; }
    JSFunction* function() const { return &object->as<JSFunction>(); }

    Scope* compilationEnclosingScope() const override {
        // This method is used to distinguish the outermost SharedContext. If
        // a FunctionBox is the outermost SharedContext, it must be a lazy
        // function.
        MOZ_ASSERT_IF(function()->isInterpretedLazy(),
                      enclosingScope_ == function()->lazyScript()->enclosingScope());
        return enclosingScope_;
    }

    bool needsCallObjectRegardlessOfBindings() const {
        return hasExtensibleScope() ||
               needsHomeObject() ||
               isDerivedClassConstructor() ||
               isGenerator();
    }

    bool hasExtraBodyVarScope() const {
        return hasParameterExprs &&
               (extraVarScopeBindings_ ||
                needsExtraBodyVarEnvironmentRegardlessOfBindings());
    }

    bool needsExtraBodyVarEnvironmentRegardlessOfBindings() const {
        MOZ_ASSERT(hasParameterExprs);
        return hasExtensibleScope() || isGenerator();
    }

    bool isLikelyConstructorWrapper() const {
        return usesArguments && usesApply && usesThis && !usesReturn;
    }

    GeneratorKind generatorKind() const { return GeneratorKindFromBits(generatorKindBits_); }
    bool isGenerator() const { return generatorKind() != NotGenerator; }
    bool isLegacyGenerator() const { return generatorKind() == LegacyGenerator; }
    bool isStarGenerator() const { return generatorKind() == StarGenerator; }
    bool isArrow() const { return function()->isArrow(); }

    void setGeneratorKind(GeneratorKind kind) {
        // A generator kind can be set at initialization, or when "yield" is
        // first seen.  In both cases the transition can only happen from
        // NotGenerator.
        MOZ_ASSERT(!isGenerator());
        generatorKindBits_ = GeneratorKindAsBits(kind);
    }

    bool mightAliasLocals()          const { return funCxFlags.mightAliasLocals; }
    bool hasExtensibleScope()        const { return funCxFlags.hasExtensibleScope; }
    bool hasThisBinding()            const { return funCxFlags.hasThisBinding; }
    bool argumentsHasLocalBinding()  const { return funCxFlags.argumentsHasLocalBinding; }
    bool definitelyNeedsArgsObj()    const { return funCxFlags.definitelyNeedsArgsObj; }
    bool needsHomeObject()           const { return funCxFlags.needsHomeObject; }
    bool isDerivedClassConstructor() const { return funCxFlags.isDerivedClassConstructor; }
    bool hasInnerFunctions()         const { return funCxFlags.hasInnerFunctions; }

    void setMightAliasLocals()             { funCxFlags.mightAliasLocals         = true; }
    void setHasExtensibleScope()           { funCxFlags.hasExtensibleScope       = true; }
    void setHasThisBinding()               { funCxFlags.hasThisBinding           = true; }
    void setArgumentsHasLocalBinding()     { funCxFlags.argumentsHasLocalBinding = true; }
    void setDefinitelyNeedsArgsObj()       { MOZ_ASSERT(funCxFlags.argumentsHasLocalBinding);
                                             funCxFlags.definitelyNeedsArgsObj   = true; }
    void setNeedsHomeObject()              { MOZ_ASSERT(function()->allowSuperProperty());
                                             funCxFlags.needsHomeObject          = true; }
    void setDerivedClassConstructor()      { MOZ_ASSERT(function()->isClassConstructor());
                                             funCxFlags.isDerivedClassConstructor = true; }
    void setHasInnerFunctions()            { funCxFlags.hasInnerFunctions         = true; }

    bool hasMappedArgsObj() const {
        return !strict() && !function()->hasRest() && !hasParameterExprs && !hasDestructuringArgs;
    }

    // Return whether this or an enclosing function is being parsed and
    // validated as asm.js. Note: if asm.js validation fails, this will be false
    // while the function is being reparsed. This flag can be used to disable
    // certain parsing features that are necessary in general, but unnecessary
    // for validated asm.js.
    bool useAsmOrInsideUseAsm() const {
        return useAsm || insideUseAsm;
    }

    void setStart(const TokenStream& tokenStream) {
        bufStart = tokenStream.currentToken().pos.begin;
        startLine = tokenStream.getLineno();
        startColumn = tokenStream.getColumn();
    }

    void trace(JSTracer* trc) override;
};

inline FunctionBox*
SharedContext::asFunctionBox()
{
    MOZ_ASSERT(isFunctionBox());
    return static_cast<FunctionBox*>(this);
}

class MOZ_STACK_CLASS ModuleSharedContext : public SharedContext
{
    RootedModuleObject module_;
    RootedScope enclosingScope_;

  public:
    Rooted<ModuleScope::Data*> bindings;
    ModuleBuilder& builder;

    ModuleSharedContext(ExclusiveContext* cx, ModuleObject* module, Scope* enclosingScope,
                        ModuleBuilder& builder);

    HandleModuleObject module() const { return module_; }
    Scope* compilationEnclosingScope() const override { return enclosingScope_; }
};

inline ModuleSharedContext*
SharedContext::asModuleContext()
{
    MOZ_ASSERT(isModuleContext());
    return static_cast<ModuleSharedContext*>(this);
}

// In generators, we treat all bindings as closed so that they get stored on
// the heap.  This way there is less information to copy off the stack when
// suspending, and back on when resuming.  It also avoids the need to create
// and invalidate DebugScope proxies for unaliased locals in a generator
// frame, as the generator frame will be copied out to the heap and released
// only by GC.
inline bool
SharedContext::allBindingsClosedOver()
{
    return bindingsAccessedDynamically() || (isFunctionBox() && asFunctionBox()->isGenerator());
}

} // namespace frontend
} // namespace js

#endif /* frontend_SharedContext_h */

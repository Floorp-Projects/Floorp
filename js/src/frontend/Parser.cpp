/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS parser.
 *
 * This is a recursive-descent parser for the JavaScript language specified by
 * "The ECMAScript Language Specification" (Standard ECMA-262).  It uses
 * lexical and semantic feedback to disambiguate non-LL(1) structures.  It
 * generates trees of nodes induced by the recursive parsing (not precise
 * syntax trees, see Parser.h).  After tree construction, it rewrites trees to
 * fold constants and evaluate compile-time expressions.
 *
 * This parser attempts no error recovery.
 */

#include "frontend/Parser.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jstypes.h"

#include "asmjs/AsmJS.h"
#include "builtin/ModuleObject.h"
#include "builtin/SelfHostingDefines.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/FoldConstants.h"
#include "frontend/TokenStream.h"

#include "jsatominlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseNode-inl.h"
#include "vm/EnvironmentObject-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::Maybe;
using mozilla::Move;
using mozilla::Nothing;
using mozilla::PodCopy;
using mozilla::PodZero;
using mozilla::Some;

using JS::AutoGCRooter;

namespace js {
namespace frontend {

using DeclaredNamePtr = ParseContext::Scope::DeclaredNamePtr;
using AddDeclaredNamePtr = ParseContext::Scope::AddDeclaredNamePtr;
using BindingIter = ParseContext::Scope::BindingIter;
using UsedNamePtr = UsedNameTracker::UsedNameMap::Ptr;

/* Read a token. Report an error and return null() if that token isn't of type tt. */
#define MUST_MATCH_TOKEN_MOD(tt, modifier, errno)                                           \
    JS_BEGIN_MACRO                                                                          \
        TokenKind token;                                                                    \
        if (!tokenStream.getToken(&token, modifier))                                        \
            return null();                                                                  \
        if (token != tt) {                                                                  \
            report(ParseError, false, null(), errno);                                       \
            return null();                                                                  \
        }                                                                                   \
    JS_END_MACRO

#define MUST_MATCH_TOKEN(tt, errno) MUST_MATCH_TOKEN_MOD(tt, TokenStream::None, errno)

template <class T, class U>
static inline void
PropagateTransitiveParseFlags(const T* inner, U* outer)
{
    if (inner->bindingsAccessedDynamically())
        outer->setBindingsAccessedDynamically();
    if (inner->hasDebuggerStatement())
        outer->setHasDebuggerStatement();
    if (inner->hasDirectEval())
        outer->setHasDirectEval();
}

static const char*
DeclarationKindString(DeclarationKind kind)
{
    switch (kind) {
      case DeclarationKind::PositionalFormalParameter:
      case DeclarationKind::FormalParameter:
        return "formal parameter";
      case DeclarationKind::Var:
        return "var";
      case DeclarationKind::Let:
        return "let";
      case DeclarationKind::Const:
        return "const";
      case DeclarationKind::Import:
        return "import";
      case DeclarationKind::BodyLevelFunction:
      case DeclarationKind::LexicalFunction:
        return "function";
      case DeclarationKind::VarForAnnexBLexicalFunction:
        return "annex b var";
      case DeclarationKind::ForOfVar:
        return "var in for-of";
      case DeclarationKind::SimpleCatchParameter:
      case DeclarationKind::CatchParameter:
        return "catch parameter";
    }

    MOZ_CRASH("Bad DeclarationKind");
}

static bool
StatementKindIsBraced(StatementKind kind)
{
    return kind == StatementKind::Block ||
           kind == StatementKind::Switch ||
           kind == StatementKind::Try ||
           kind == StatementKind::Catch ||
           kind == StatementKind::Finally;
}

void
ParseContext::Scope::dump(ParseContext* pc)
{
    ExclusiveContext* cx = pc->sc()->context;

    fprintf(stdout, "ParseScope %p", this);

    fprintf(stdout, "\n  decls:\n");
    for (DeclaredNameMap::Range r = declared_->all(); !r.empty(); r.popFront()) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(cx, r.front().key(), &bytes))
            return;
        DeclaredNameInfo& info = r.front().value().wrapped;
        fprintf(stdout, "    %s %s%s\n",
                DeclarationKindString(info.kind()),
                bytes.ptr(),
                info.closedOver() ? " (closed over)" : "");
    }

    fprintf(stdout, "\n");
}

/* static */ void
ParseContext::Scope::removeVarForAnnexBLexicalFunction(ParseContext* pc, JSAtom* name)
{
    // Local strict mode is allowed, e.g., a class binding removing a
    // synthesized Annex B binding.
    MOZ_ASSERT(!pc->sc()->strictScript);

    for (ParseContext::Scope* scope = pc->innermostScope();
         scope != pc->varScope().enclosing();
         scope = scope->enclosing())
    {
        if (DeclaredNamePtr p = scope->declared_->lookup(name)) {
            if (p->value()->kind() == DeclarationKind::VarForAnnexBLexicalFunction)
                scope->declared_->remove(p);
        }
    }

    // Annex B semantics no longer applies to any functions with this name, as
    // an early error would have occurred.
    pc->removeInnerFunctionBoxesForAnnexB(name);
}

void
ParseContext::Scope::removeSimpleCatchParameter(ParseContext* pc, JSAtom* name)
{
    if (pc->useAsmOrInsideUseAsm())
        return;

    DeclaredNamePtr p = declared_->lookup(name);
    MOZ_ASSERT(p && p->value()->kind() == DeclarationKind::SimpleCatchParameter);
    declared_->remove(p);
}

void
SharedContext::computeAllowSyntax(Scope* scope)
{
    for (ScopeIter si(scope); si; si++) {
        if (si.kind() == ScopeKind::Function) {
            JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();
            if (fun->isArrow())
                continue;
            allowNewTarget_ = true;
            allowSuperProperty_ = fun->allowSuperProperty();
            allowSuperCall_ = fun->isDerivedClassConstructor();
            return;
        }
    }
}

void
SharedContext::computeThisBinding(Scope* scope)
{
    for (ScopeIter si(scope); si; si++) {
        if (si.kind() == ScopeKind::Module) {
            thisBinding_ = ThisBinding::Module;
            return;
        }

        if (si.kind() == ScopeKind::Function) {
            JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();

            // Arrow functions and generator expression lambdas don't have
            // their own `this` binding.
            if (fun->isArrow() || fun->nonLazyScript()->isGeneratorExp())
                continue;

            // Derived class constructors (including nested arrow functions and
            // eval) need TDZ checks when accessing |this|.
            if (fun->isDerivedClassConstructor())
                needsThisTDZChecks_ = true;

            thisBinding_ = ThisBinding::Function;
            return;
        }
    }

    thisBinding_ = ThisBinding::Global;
}

void
SharedContext::computeInWith(Scope* scope)
{
    for (ScopeIter si(scope); si; si++) {
        if (si.kind() == ScopeKind::With) {
            inWith_ = true;
            break;
        }
    }
}

EvalSharedContext::EvalSharedContext(ExclusiveContext* cx, JSObject* enclosingEnv,
                                     Scope* enclosingScope, Directives directives,
                                     bool extraWarnings)
  : SharedContext(cx, Kind::Eval, directives, extraWarnings),
    enclosingScope_(cx, enclosingScope),
    functionBindingEnd(0),
    bindings(cx)
{
    computeAllowSyntax(enclosingScope);
    computeInWith(enclosingScope);
    computeThisBinding(enclosingScope);

    // Like all things Debugger, Debugger.Frame.eval needs special
    // handling. Since the environment chain of such evals are non-syntactic
    // (DebuggerEnvironmentProxy is not an EnvironmentObject), computing the
    // this binding with respect to enclosingScope is incorrect if the
    // Debugger.Frame is a function frame. Recompute the this binding if we
    // are such an eval.
    if (enclosingEnv && enclosingEnv->is<DebugEnvironmentProxy>()) {
        JSObject* env = &enclosingEnv->as<DebugEnvironmentProxy>().environment();
        while (env) {
            if (env->is<CallObject>()) {
                computeThisBinding(env->as<CallObject>().callee().nonLazyScript()->bodyScope());
                break;
            }
            env = env->enclosingEnvironment();
        }
    }
}

bool
ParseContext::init()
{
    if (scriptId_ == UINT32_MAX) {
        tokenStream_.reportError(JSMSG_NEED_DIET, js_script_str);
        return false;
    }

    ExclusiveContext* cx = sc()->context;

    if (isFunctionBox()) {
        // Named lambdas always need a binding for their own name. If this
        // binding is closed over when we finish parsing the function in
        // finishExtraFunctionScopes, the function box needs to be marked as
        // needing a dynamic DeclEnv object.
        RootedFunction fun(cx, functionBox()->function());
        if (fun->isNamedLambda()) {
            if (!namedLambdaScope_->init(this))
                return false;
            AddDeclaredNamePtr p = namedLambdaScope_->lookupDeclaredNameForAdd(fun->name());
            MOZ_ASSERT(!p);
            if (!namedLambdaScope_->addDeclaredName(this, p, fun->name(), DeclarationKind::Const))
                return false;
        }

        if (!functionScope_->init(this))
            return false;

        if (!positionalFormalParameterNames_.acquire(cx))
            return false;
    }

    if (!closedOverBindingsForLazy_.acquire(cx))
        return false;

    if (!sc()->strict()) {
        if (!innerFunctionBoxesForAnnexB_.acquire(cx))
            return false;
    }

    return true;
}

bool
ParseContext::addInnerFunctionBoxForAnnexB(FunctionBox* funbox)
{
    for (uint32_t i = 0; i < innerFunctionBoxesForAnnexB_->length(); i++) {
        if (!innerFunctionBoxesForAnnexB_[i]) {
            innerFunctionBoxesForAnnexB_[i] = funbox;
            return true;
        }
    }
    return innerFunctionBoxesForAnnexB_->append(funbox);
}

void
ParseContext::removeInnerFunctionBoxesForAnnexB(JSAtom* name)
{
    for (uint32_t i = 0; i < innerFunctionBoxesForAnnexB_->length(); i++) {
        if (FunctionBox* funbox = innerFunctionBoxesForAnnexB_[i]) {
            if (funbox->function()->name() == name)
                innerFunctionBoxesForAnnexB_[i] = nullptr;
        }
    }
}

void
ParseContext::finishInnerFunctionBoxesForAnnexB()
{
    // Strict mode doesn't have wack Annex B function semantics. Or we
    // could've failed to initialize ParseContext.
    if (sc()->strict() || !innerFunctionBoxesForAnnexB_)
        return;

    for (uint32_t i = 0; i < innerFunctionBoxesForAnnexB_->length(); i++) {
        if (FunctionBox* funbox = innerFunctionBoxesForAnnexB_[i])
            funbox->isAnnexB = true;
    }
}

ParseContext::~ParseContext()
{
    // Any funboxes still in the list at the end of parsing means no early
    // error would have occurred for declaring a binding in the nearest var
    // scope. Mark them as needing extra assignments to this var binding.
    finishInnerFunctionBoxesForAnnexB();
}

bool
UsedNameTracker::noteUse(ExclusiveContext* cx, JSAtom* name, uint32_t scriptId, uint32_t scopeId)
{
    if (UsedNameMap::AddPtr p = map_.lookupForAdd(name)) {
        if (!p->value().noteUsedInScope(scriptId, scopeId))
            return false;
    } else {
        UsedNameInfo info(cx);
        if (!info.noteUsedInScope(scriptId, scopeId))
            return false;
        if (!map_.add(p, name, Move(info)))
            return false;
    }

    return true;
}

void
UsedNameTracker::UsedNameInfo::resetToScope(uint32_t scriptId, uint32_t scopeId)
{
    while (!uses_.empty()) {
        Use& innermost = uses_.back();
        if (innermost.scopeId < scopeId)
            break;
        MOZ_ASSERT(innermost.scriptId >= scriptId);
        uses_.popBack();
    }
}

void
UsedNameTracker::rewind(RewindToken token)
{
    scriptCounter_ = token.scriptId;
    scopeCounter_ = token.scopeId;

    for (UsedNameMap::Range r = map_.all(); !r.empty(); r.popFront())
        r.front().value().resetToScope(token.scriptId, token.scopeId);
}

FunctionBox::FunctionBox(ExclusiveContext* cx, LifoAlloc& alloc, ObjectBox* traceListHead,
                         JSFunction* fun, Directives directives, bool extraWarnings,
                         GeneratorKind generatorKind)
  : ObjectBox(fun, traceListHead),
    SharedContext(cx, Kind::ObjectBox, directives, extraWarnings),
    enclosingScope_(nullptr),
    namedLambdaBindings_(nullptr),
    functionScopeBindings_(nullptr),
    extraVarScopeBindings_(nullptr),
    functionNode(nullptr),
    bufStart(0),
    bufEnd(0),
    startLine(1),
    startColumn(0),
    length(0),
    generatorKindBits_(GeneratorKindAsBits(generatorKind)),
    isGenexpLambda(false),
    hasDestructuringArgs(false),
    hasParameterExprs(false),
    hasDirectEvalInParameterExpr(false),
    useAsm(false),
    insideUseAsm(false),
    isAnnexB(false),
    wasEmitted(false),
    declaredArguments(false),
    usesArguments(false),
    usesApply(false),
    usesThis(false),
    usesReturn(false),
    funCxFlags()
{
    // Functions created at parse time may be set singleton after parsing and
    // baked into JIT code, so they must be allocated tenured. They are held by
    // the JSScript so cannot be collected during a minor GC anyway.
    MOZ_ASSERT(fun->isTenured());
}

void
FunctionBox::initFromLazyFunction()
{
    JSFunction* fun = function();
    length = fun->nargs() - fun->hasRest();
    if (fun->lazyScript()->isDerivedClassConstructor())
        setDerivedClassConstructor();
    enclosingScope_ = fun->lazyScript()->enclosingScope();
    initWithEnclosingScope(enclosingScope_);
}

void
FunctionBox::initStandaloneFunction(Scope* enclosingScope)
{
    // Standalone functions are Function or Generator constructors and are
    // always scoped to the global.
    MOZ_ASSERT(enclosingScope->is<GlobalScope>());
    JSFunction* fun = function();
    length = fun->nargs() - fun->hasRest();
    enclosingScope_ = enclosingScope;
    allowNewTarget_ = true;
    thisBinding_ = ThisBinding::Function;
}

void
FunctionBox::initWithEnclosingParseContext(ParseContext* enclosing, FunctionSyntaxKind kind)
{
    SharedContext* sc = enclosing->sc();
    useAsm = sc->isFunctionBox() && sc->asFunctionBox()->useAsmOrInsideUseAsm();

    JSFunction* fun = function();

    // Arrow functions and generator expression lambdas don't have
    // their own `this` binding.
    if (fun->isArrow()) {
        allowNewTarget_ = sc->allowNewTarget();
        allowSuperProperty_ = sc->allowSuperProperty();
        allowSuperCall_ = sc->allowSuperCall();
        needsThisTDZChecks_ = sc->needsThisTDZChecks();
        thisBinding_ = sc->thisBinding();
    } else {
        allowNewTarget_ = true;
        allowSuperProperty_ = fun->allowSuperProperty();

        if (kind == DerivedClassConstructor) {
            setDerivedClassConstructor();
            allowSuperCall_ = true;
            needsThisTDZChecks_ = true;
        }

        if (isGenexpLambda)
            thisBinding_ = sc->thisBinding();
        else
            thisBinding_ = ThisBinding::Function;
    }

    if (sc->inWith()) {
        inWith_ = true;
    } else {
        auto isWith = [](ParseContext::Statement* stmt) {
            return stmt->kind() == StatementKind::With;
        };

        inWith_ = enclosing->findInnermostStatement(isWith);
    }
}

void
FunctionBox::initWithEnclosingScope(Scope* enclosingScope)
{
    if (!function()->isArrow()) {
        allowNewTarget_ = true;
        allowSuperProperty_ = function()->allowSuperProperty();

        if (isDerivedClassConstructor()) {
            setDerivedClassConstructor();
            allowSuperCall_ = true;
            needsThisTDZChecks_ = true;
        }

        thisBinding_ = ThisBinding::Function;
    } else {
        computeAllowSyntax(enclosingScope);
        computeThisBinding(enclosingScope);
    }

    computeInWith(enclosingScope);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportHelper(ParseReportKind kind, bool strict, uint32_t offset,
                                   unsigned errorNumber, va_list args)
{
    bool result = false;
    switch (kind) {
      case ParseError:
        result = tokenStream.reportCompileErrorNumberVA(offset, JSREPORT_ERROR, errorNumber, args);
        break;
      case ParseWarning:
        result =
            tokenStream.reportCompileErrorNumberVA(offset, JSREPORT_WARNING, errorNumber, args);
        break;
      case ParseExtraWarning:
        result = tokenStream.reportStrictWarningErrorNumberVA(offset, errorNumber, args);
        break;
      case ParseStrictError:
        result = tokenStream.reportStrictModeErrorNumberVA(offset, strict, errorNumber, args);
        break;
    }
    return result;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::report(ParseReportKind kind, bool strict, Node pn, unsigned errorNumber, ...)
{
    uint32_t offset = (pn ? handler.getPosition(pn) : pos()).begin;

    va_list args;
    va_start(args, errorNumber);
    bool result = reportHelper(kind, strict, offset, errorNumber, args);
    va_end(args);
    return result;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportNoOffset(ParseReportKind kind, bool strict, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = reportHelper(kind, strict, TokenStream::NoOffset, errorNumber, args);
    va_end(args);
    return result;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportWithOffset(ParseReportKind kind, bool strict, uint32_t offset,
                                       unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = reportHelper(kind, strict, offset, errorNumber, args);
    va_end(args);
    return result;
}

template <>
bool
Parser<FullParseHandler>::abortIfSyntaxParser()
{
    handler.disableSyntaxParser();
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::abortIfSyntaxParser()
{
    abortedSyntaxParse = true;
    return false;
}

template <typename ParseHandler>
Parser<ParseHandler>::Parser(ExclusiveContext* cx, LifoAlloc& alloc,
                             const ReadOnlyCompileOptions& options,
                             const char16_t* chars, size_t length,
                             bool foldConstants,
                             UsedNameTracker& usedNames,
                             Parser<SyntaxParseHandler>* syntaxParser,
                             LazyScript* lazyOuterFunction)
  : AutoGCRooter(cx, PARSER),
    context(cx),
    alloc(alloc),
    tokenStream(cx, options, chars, length, thisForCtor()),
    traceListHead(nullptr),
    pc(nullptr),
    usedNames(usedNames),
    sct(nullptr),
    ss(nullptr),
    keepAtoms(cx->perThreadData),
    foldConstants(foldConstants),
#ifdef DEBUG
    checkOptionsCalled(false),
#endif
    abortedSyntaxParse(false),
    isUnexpectedEOF_(false),
    handler(cx, alloc, tokenStream, syntaxParser, lazyOuterFunction)
{
    cx->perThreadData->frontendCollectionPool.addActiveCompilation();

    // The Mozilla specific JSOPTION_EXTRA_WARNINGS option adds extra warnings
    // which are not generated if functions are parsed lazily. Note that the
    // standard "use strict" does not inhibit lazy parsing.
    if (options.extraWarningsOption)
        handler.disableSyntaxParser();

    tempPoolMark = alloc.mark();
}

template<typename ParseHandler>
bool
Parser<ParseHandler>::checkOptions()
{
#ifdef DEBUG
    checkOptionsCalled = true;
#endif

    if (!tokenStream.checkOptions())
        return false;

    return true;
}

template <typename ParseHandler>
Parser<ParseHandler>::~Parser()
{
    MOZ_ASSERT(checkOptionsCalled);
    alloc.release(tempPoolMark);

    /*
     * The parser can allocate enormous amounts of memory for large functions.
     * Eagerly free the memory now (which otherwise won't be freed until the
     * next GC) to avoid unnecessary OOMs.
     */
    alloc.freeAllIfHugeAndUnused();

    context->perThreadData->frontendCollectionPool.removeActiveCompilation();
}

template <typename ParseHandler>
ObjectBox*
Parser<ParseHandler>::newObjectBox(JSObject* obj)
{
    MOZ_ASSERT(obj);

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */

    ObjectBox* objbox = alloc.new_<ObjectBox>(obj, traceListHead);
    if (!objbox) {
        ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = objbox;

    return objbox;
}

template <typename ParseHandler>
FunctionBox*
Parser<ParseHandler>::newFunctionBox(Node fn, JSFunction* fun, Directives inheritedDirectives,
                                     GeneratorKind generatorKind, bool tryAnnexB)
{
    MOZ_ASSERT(fun);
    MOZ_ASSERT_IF(tryAnnexB, !pc->sc()->strict());

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    FunctionBox* funbox =
        alloc.new_<FunctionBox>(context, alloc, traceListHead, fun, inheritedDirectives,
                                options().extraWarningsOption, generatorKind);
    if (!funbox) {
        ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = funbox;
    if (fn)
        handler.setFunctionBox(fn, funbox);

    if (tryAnnexB && !pc->addInnerFunctionBoxForAnnexB(funbox))
        return nullptr;

    return funbox;
}

ModuleSharedContext::ModuleSharedContext(ExclusiveContext* cx, ModuleObject* module,
                                         Scope* enclosingScope, ModuleBuilder& builder)
  : SharedContext(cx, Kind::Module, Directives(true), false),
    module_(cx, module),
    enclosingScope_(cx, enclosingScope),
    bindings(cx),
    builder(builder)
{
    thisBinding_ = ThisBinding::Module;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::trace(JSTracer* trc)
{
    ObjectBox::TraceList(trc, traceListHead);
}

void
MarkParser(JSTracer* trc, AutoGCRooter* parser)
{
    static_cast<Parser<FullParseHandler>*>(parser)->trace(trc);
}

/*
 * Parse a top-level JS script.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::parse()
{
    MOZ_ASSERT(checkOptionsCalled);

    Directives directives(options().strictOption);
    GlobalSharedContext globalsc(context, ScopeKind::Global,
                                 directives, options().extraWarningsOption);
    ParseContext globalpc(this, &globalsc, /* newDirectives = */ nullptr);
    if (!globalpc.init())
        return null();

    ParseContext::VarScope varScope(this);
    if (!varScope.init(pc))
        return null();

    Node pn = statementList(YieldIsName);
    if (!pn)
        return null();

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    if (tt != TOK_EOF) {
        report(ParseError, false, null(), JSMSG_GARBAGE_AFTER_INPUT,
               "script", TokenKindToDesc(tt));
        return null();
    }
    if (foldConstants) {
        if (!FoldConstants(context, &pn, this))
            return null();
    }

    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportBadReturn(Node pn, ParseReportKind kind,
                                      unsigned errnum, unsigned anonerrnum)
{
    JSAutoByteString name;
    if (JSAtom* atom = pc->functionBox()->function()->name()) {
        if (!AtomToPrintableString(context, atom, &name))
            return false;
    } else {
        errnum = anonerrnum;
    }
    return report(kind, pc->sc()->strict(), pn, errnum, name.ptr());
}

/*
 * Check that it is permitted to introduce a binding for atom.  Strict mode
 * forbids introducing new definitions for 'eval', 'arguments', or for any
 * strict mode reserved keyword.  Use pn for reporting error locations, or use
 * pc's token stream if pn is nullptr.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::checkStrictBinding(PropertyName* name, TokenPos pos)
{
    if (!pc->sc()->needStrictChecks())
        return true;

    if (name == context->names().eval || name == context->names().arguments || IsKeyword(name)) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(context, name, &bytes))
            return false;
        return reportWithOffset(ParseStrictError, pc->sc()->strict(), pos.begin,
                                JSMSG_BAD_BINDING, bytes.ptr());
    }

    return true;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::reportRedeclaration(HandlePropertyName name, DeclarationKind kind,
                                          TokenPos pos)
{
    JSAutoByteString bytes;
    if (!AtomToPrintableString(context, name, &bytes))
        return;
    reportWithOffset(ParseError, false, pos.begin, JSMSG_REDECLARED_VAR,
                     DeclarationKindString(kind), bytes.ptr());
}

// notePositionalFormalParameter is called for both the arguments of a regular
// function definition and the arguments specified by the Function
// constructor.
//
// The 'disallowDuplicateParams' bool indicates whether the use of another
// feature (destructuring or default arguments) disables duplicate arguments.
// (ECMA-262 requires us to support duplicate parameter names, but, for newer
// features, we consider the code to have "opted in" to higher standards and
// forbid duplicates.)
template <typename ParseHandler>
bool
Parser<ParseHandler>::notePositionalFormalParameter(Node fn, HandlePropertyName name,
                                                    bool disallowDuplicateParams,
                                                    bool* duplicatedParam)
{
    if (AddDeclaredNamePtr p = pc->functionScope().lookupDeclaredNameForAdd(name)) {
        if (disallowDuplicateParams) {
            report(ParseError, false, null(), JSMSG_BAD_DUP_ARGS);
            return false;
        }

        // Strict-mode disallows duplicate args. We may not know whether we are
        // in strict mode or not (since the function body hasn't been parsed).
        // In such cases, report will queue up the potential error and return
        // 'true'.
        if (pc->sc()->needStrictChecks()) {
            JSAutoByteString bytes;
            if (!AtomToPrintableString(context, name, &bytes))
                return false;
            if (!report(ParseStrictError, pc->sc()->strict(), null(),
                        JSMSG_DUPLICATE_FORMAL, bytes.ptr()))
            {
                return false;
            }
        }

        if (duplicatedParam)
            *duplicatedParam = true;
    } else {
        DeclarationKind kind = DeclarationKind::PositionalFormalParameter;
        if (!pc->functionScope().addDeclaredName(pc, p, name, kind))
            return false;
    }

    if (!pc->positionalFormalParameterNames().append(name)) {
        ReportOutOfMemory(context);
        return false;
    }

    Node paramNode = newName(name);
    if (!paramNode)
        return false;

    if (!checkStrictBinding(name, pos()))
        return false;

    handler.addFunctionFormalParameter(fn, paramNode);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteDestructuredPositionalFormalParameter(Node fn, Node destruct)
{
    // Append an empty name to the positional formals vector to keep track of
    // argument slots when making FunctionScope::Data.
    if (!pc->positionalFormalParameterNames().append(nullptr)) {
        ReportOutOfMemory(context);
        return false;
    }

    handler.addFunctionFormalParameter(fn, destruct);
    return true;
}

static bool
DeclarationKindIsVar(DeclarationKind kind)
{
    return kind == DeclarationKind::Var ||
           kind == DeclarationKind::BodyLevelFunction ||
           kind == DeclarationKind::VarForAnnexBLexicalFunction ||
           kind == DeclarationKind::ForOfVar;
}

static bool
DeclarationKindIsParameter(DeclarationKind kind)
{
    return kind == DeclarationKind::PositionalFormalParameter ||
           kind == DeclarationKind::FormalParameter;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::tryDeclareVar(HandlePropertyName name, DeclarationKind kind,
                                    Maybe<DeclarationKind>* redeclaredKind)
{
    MOZ_ASSERT(DeclarationKindIsVar(kind));

    // It is an early error if a 'var' declaration appears inside a
    // scope contour that has a lexical declaration of the same name. For
    // example, the following are early errors:
    //
    //   { let x; var x; }
    //   { { var x; } let x; }
    //
    // And the following are not:
    //
    //   { var x; var x; }
    //   { { let x; } var x; }

    for (ParseContext::Scope* scope = pc->innermostScope();
         scope != pc->varScope().enclosing();
         scope = scope->enclosing())
    {
        if (AddDeclaredNamePtr p = scope->lookupDeclaredNameForAdd(name)) {
            DeclarationKind declaredKind = p->value()->kind();
            if (!DeclarationKindIsVar(declaredKind) && !DeclarationKindIsParameter(declaredKind)) {
                // Annex B.3.5 allows redeclaring simple (non-destructured)
                // catch parameters with var declarations, except when it
                // appears in a for-of.
                bool annexB35Allowance = declaredKind == DeclarationKind::SimpleCatchParameter &&
                                         kind != DeclarationKind::ForOfVar;

                // Annex B.3.3 allows redeclaring functions in the same block.
                bool annexB33Allowance = declaredKind == DeclarationKind::LexicalFunction &&
                                         kind == DeclarationKind::VarForAnnexBLexicalFunction &&
                                         scope == pc->innermostScope();

                if (!annexB35Allowance && !annexB33Allowance) {
                    *redeclaredKind = Some(declaredKind);
                    return true;
                }
            }
        } else {
            if (!scope->addDeclaredName(pc, p, name, kind))
                return false;
        }
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::tryDeclareVarForAnnexBLexicalFunction(HandlePropertyName name,
                                                            bool* tryAnnexB)
{
    Maybe<DeclarationKind> redeclaredKind;
    if (!tryDeclareVar(name, DeclarationKind::VarForAnnexBLexicalFunction, &redeclaredKind))
        return false;

    // In the case of eval, we also need to check enclosing VM scopes to see
    // if an Annex B var should be synthesized.
    if (!redeclaredKind && pc->sc()->isEvalContext()) {
        Scope* enclosingScope = pc->sc()->compilationEnclosingScope();
        Scope* varScope = EvalScope::nearestVarScopeForDirectEval(enclosingScope);
        MOZ_ASSERT(varScope);
        for (ScopeIter si(enclosingScope); si; si++) {
            for (js::BindingIter bi(si.scope()); bi; bi++) {
                if (bi.name() != name)
                    continue;

                switch (bi.kind()) {
                  case BindingKind::Let: {
                    // Annex B.3.5 allows redeclaring simple (non-destructured)
                    // catch parameters with var declarations, except when it
                    // appears in a for-of, which a function declaration is
                    // definitely not.
                    bool annexB35Allowance = si.kind() == ScopeKind::Catch;
                    if (!annexB35Allowance)
                        redeclaredKind = Some(DeclarationKind::Let);
                    break;
                  }

                  case BindingKind::Const:
                    redeclaredKind = Some(DeclarationKind::Const);
                    break;

                  case BindingKind::Import:
                  case BindingKind::FormalParameter:
                  case BindingKind::Var:
                  case BindingKind::NamedLambdaCallee:
                    break;
                }
            }

            if (si.scope() == varScope)
                break;
        }
    }

    if (redeclaredKind) {
        // If an early error would have occurred, undo all the
        // VarForAnnexBLexicalFunction declarations.
        *tryAnnexB = false;
        ParseContext::Scope::removeVarForAnnexBLexicalFunction(pc, name);
    } else {
        *tryAnnexB = true;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkLexicalDeclarationDirectlyWithinBlock(ParseContext::Statement& stmt,
                                                                 DeclarationKind kind,
                                                                 TokenPos pos)
{
    MOZ_ASSERT(DeclarationKindIsLexical(kind));

    // It is an early error to declare a lexical binding not directly
    // within a block.
    if (!StatementKindIsBraced(stmt.kind()) &&
        stmt.kind() != StatementKind::ForLoopLexicalHead)
    {
        reportWithOffset(ParseError, false, pos.begin,
                         stmt.kind() == StatementKind::Label
                         ? JSMSG_LEXICAL_DECL_LABEL
                         : JSMSG_LEXICAL_DECL_NOT_IN_BLOCK,
                         DeclarationKindString(kind));
        return false;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteDeclaredName(HandlePropertyName name, DeclarationKind kind,
                                       TokenPos pos)
{
    // The asm.js validator does all its own symbol-table management so, as an
    // optimization, avoid doing any work here.
    if (pc->useAsmOrInsideUseAsm())
        return true;

    if (!checkStrictBinding(name, pos))
        return false;

    switch (kind) {
      case DeclarationKind::Var:
      case DeclarationKind::BodyLevelFunction:
      case DeclarationKind::ForOfVar: {
        Maybe<DeclarationKind> redeclaredKind;
        if (!tryDeclareVar(name, kind, &redeclaredKind))
            return false;

        if (redeclaredKind) {
            reportRedeclaration(name, *redeclaredKind, pos);
            return false;
        }

        break;
      }

      case DeclarationKind::FormalParameter: {
        // It is an early error if any non-positional formal parameter name
        // (e.g., destructuring formal parameter) is duplicated.

        AddDeclaredNamePtr p = pc->functionScope().lookupDeclaredNameForAdd(name);
        if (p) {
            report(ParseError, false, null(), JSMSG_BAD_DUP_ARGS);
            return false;
        }

        if (!pc->functionScope().addDeclaredName(pc, p, name, kind))
            return false;

        break;
      }

      case DeclarationKind::LexicalFunction: {
        // Functions in block have complex allowances in sloppy mode for being
        // labelled that other lexical declarations do not have. Those checks
        // are more complex than calling checkLexicalDeclarationDirectlyWithin-
        // Block and are done in checkFunctionDefinition.

        ParseContext::Scope* scope = pc->innermostScope();
        if (AddDeclaredNamePtr p = scope->lookupDeclaredNameForAdd(name)) {
            // It is usually an early error if there is another declaration
            // with the same name in the same scope.
            //
            // In sloppy mode, lexical functions may redeclare other lexical
            // functions for web compatibility reasons.
            if (pc->sc()->strict() ||
                (p->value()->kind() != DeclarationKind::LexicalFunction &&
                 p->value()->kind() != DeclarationKind::VarForAnnexBLexicalFunction))
            {
                reportRedeclaration(name, p->value()->kind(), pos);
                return false;
            }

            // Update the DeclarationKind to make a LexicalFunction
            // declaration that shadows the VarForAnnexBLexicalFunction.
            p->value()->alterKind(kind);
        } else {
            if (!scope->addDeclaredName(pc, p, name, kind))
                return false;
        }

        break;
      }

      case DeclarationKind::Let:
      case DeclarationKind::Const:
        // The BoundNames of LexicalDeclaration and ForDeclaration must not
        // contain 'let'. (CatchParameter is the only lexical binding form
        // without this restriction.)
        if (name == context->names().let) {
            reportWithOffset(ParseError, false, pos.begin, JSMSG_LEXICAL_DECL_DEFINES_LET);
            return false;
        }

        MOZ_FALLTHROUGH;

      case DeclarationKind::Import:
        // Module code is always strict, so 'let' is always a keyword and never a name.
        MOZ_ASSERT(name != context->names().let);
        MOZ_FALLTHROUGH;

      case DeclarationKind::SimpleCatchParameter:
      case DeclarationKind::CatchParameter: {
        if (ParseContext::Statement* stmt = pc->innermostStatement()) {
            if (!checkLexicalDeclarationDirectlyWithinBlock(*stmt, kind, pos))
                return false;
        }

        ParseContext::Scope* scope = pc->innermostScope();

        // For body-level lexically declared names in a function, it is an
        // early error if there is a formal parameter of the same name. This
        // needs a special check if there is an extra var scope due to
        // parameter expressions.
        if (pc->isFunctionExtraBodyVarScopeInnermost()) {
            DeclaredNamePtr p = pc->functionScope().lookupDeclaredName(name);
            if (p && DeclarationKindIsParameter(p->value()->kind())) {
                reportRedeclaration(name, p->value()->kind(), pos);
                return false;
            }
        }

        // It is an early error if there is another declaration with the same
        // name in the same scope.
        AddDeclaredNamePtr p = scope->lookupDeclaredNameForAdd(name);
        if (p) {
            // If the early error would have occurred due to Annex B.3.3
            // semantics, remove the synthesized Annex B var declaration, do
            // not report the redeclaration, and declare the lexical name.
            if (p->value()->kind() == DeclarationKind::VarForAnnexBLexicalFunction) {
                ParseContext::Scope::removeVarForAnnexBLexicalFunction(pc, name);
                p = scope->lookupDeclaredNameForAdd(name);
                MOZ_ASSERT(!p);
            } else {
                reportRedeclaration(name, p->value()->kind(), pos);
                return false;
            }
        }

        if (!p && !scope->addDeclaredName(pc, p, name, kind))
            return false;

        break;
      }

      case DeclarationKind::PositionalFormalParameter:
        MOZ_CRASH("Positional formal parameter names should use "
                  "notePositionalFormalParameter");
        break;

      case DeclarationKind::VarForAnnexBLexicalFunction:
        MOZ_CRASH("Synthesized Annex B vars should go through "
                  "tryDeclareVarForAnnexBLexicalFunction");
        break;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteUsedName(HandlePropertyName name)
{
    // If the we are delazifying, the LazyScript already has all the
    // closed-over info for bindings and there's no need to track used names.
    if (handler.canSkipLazyClosedOverBindings())
        return true;

    // The asm.js validator does all its own symbol-table management so, as an
    // optimization, avoid doing any work here.
    if (pc->useAsmOrInsideUseAsm())
        return true;

    // Global bindings are properties and not actual bindings; we don't need
    // to know if they are closed over. So no need to track used name at the
    // global scope. It is not incorrect to track them, this is an
    // optimization.
    ParseContext::Scope* scope = pc->innermostScope();
    if (pc->sc()->isGlobalContext() && scope == &pc->varScope())
        return true;

    return usedNames.noteUse(context, name, pc->scriptId(), scope->id());
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::hasUsedName(HandlePropertyName name)
{
    if (UsedNamePtr p = usedNames.lookup(name))
        return p->value().isUsedInScript(pc->scriptId());
    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::propagateFreeNamesAndMarkClosedOverBindings(ParseContext::Scope& scope)
{
    if (handler.canSkipLazyClosedOverBindings()) {
        // Scopes are nullptr-delimited in the LazyScript closed over bindings
        // array.
        while (JSAtom* name = handler.nextLazyClosedOverBinding())
            scope.lookupDeclaredName(name)->value()->setClosedOver();
        return true;
    }

    bool isSyntaxParser = mozilla::IsSame<ParseHandler, SyntaxParseHandler>::value;
    uint32_t scriptId = pc->scriptId();
    uint32_t scopeId = scope.id();
    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        if (UsedNamePtr p = usedNames.lookup(bi.name())) {
            bool closedOver;
            p->value().noteBoundInScope(scriptId, scopeId, &closedOver);
            if (closedOver) {
                bi.setClosedOver();

                if (isSyntaxParser && !pc->closedOverBindingsForLazy().append(bi.name())) {
                    ReportOutOfMemory(context);
                    return false;
                }
            }
        }
    }

    // Append a nullptr to denote end-of-scope.
    if (isSyntaxParser && !pc->closedOverBindingsForLazy().append(nullptr)) {
        ReportOutOfMemory(context);
        return false;
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::checkStatementsEOF()
{
    // This is designed to be paired with parsing a statement list at the top
    // level.
    //
    // The statementList() call breaks on TOK_RC, so make sure we've
    // reached EOF here.
    TokenKind tt;
    if (!tokenStream.peekToken(&tt, TokenStream::Operand))
        return false;
    if (tt != TOK_EOF) {
        report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
               "expression", TokenKindToDesc(tt));
        return false;
    }
    return true;
}

template <typename Scope>
static typename Scope::Data*
NewEmptyBindingData(ExclusiveContext* cx, LifoAlloc& alloc, uint32_t numBindings)
{
    size_t allocSize = Scope::sizeOfData(numBindings);
    typename Scope::Data* bindings = static_cast<typename Scope::Data*>(alloc.alloc(allocSize));
    if (!bindings) {
        ReportOutOfMemory(cx);
        return nullptr;
    }
    PodZero(bindings);
    return bindings;
}

template <>
Maybe<GlobalScope::Data*>
Parser<FullParseHandler>::newGlobalScopeData(ParseContext::Scope& scope,
                                             uint32_t* functionBindingEnd)
{
    Vector<BindingName> funs(context);
    Vector<BindingName> vars(context);
    Vector<BindingName> lets(context);
    Vector<BindingName> consts(context);

    bool allBindingsClosedOver = pc->sc()->allBindingsClosedOver();
    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        BindingName binding(bi.name(), allBindingsClosedOver || bi.closedOver());
        switch (bi.kind()) {
          case BindingKind::Var:
            if (bi.declarationKind() == DeclarationKind::BodyLevelFunction) {
                if (!funs.append(binding))
                    return Nothing();
            } else {
                if (!vars.append(binding))
                    return Nothing();
            }
            break;
          case BindingKind::Let:
            if (!lets.append(binding))
                return Nothing();
            break;
          case BindingKind::Const:
            if (!consts.append(binding))
                return Nothing();
            break;
          default:
            MOZ_CRASH("Bad global scope BindingKind");
        }
    }

    GlobalScope::Data* bindings = nullptr;
    uint32_t numBindings = funs.length() + vars.length() + lets.length() + consts.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<GlobalScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in GlobalScope.
        BindingName* start = bindings->names;
        BindingName* cursor = start;

        // Keep track of what vars are functions. This is only used in BCE to omit
        // superfluous DEFVARs.
        PodCopy(cursor, funs.begin(), funs.length());
        cursor += funs.length();
        *functionBindingEnd = cursor - start;

        PodCopy(cursor, vars.begin(), vars.length());
        cursor += vars.length();

        bindings->letStart = cursor - start;
        PodCopy(cursor, lets.begin(), lets.length());
        cursor += lets.length();

        bindings->constStart = cursor - start;
        PodCopy(cursor, consts.begin(), consts.length());
        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<ModuleScope::Data*>
Parser<FullParseHandler>::newModuleScopeData(ParseContext::Scope& scope)
{
    Vector<BindingName> imports(context);
    Vector<BindingName> vars(context);
    Vector<BindingName> lets(context);
    Vector<BindingName> consts(context);

    bool allBindingsClosedOver = pc->sc()->allBindingsClosedOver();
    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        // Imports are indirect bindings and must not be given known slots.
        BindingName binding(bi.name(), (allBindingsClosedOver || bi.closedOver()) &&
                                       bi.kind() != BindingKind::Import);
        switch (bi.kind()) {
          case BindingKind::Import:
            if (!imports.append(binding))
                return Nothing();
            break;
          case BindingKind::Var:
            if (!vars.append(binding))
                return Nothing();
            break;
          case BindingKind::Let:
            if (!lets.append(binding))
                return Nothing();
            break;
          case BindingKind::Const:
            if (!consts.append(binding))
                return Nothing();
            break;
          default:
            MOZ_CRASH("Bad module scope BindingKind");
        }
    }

    ModuleScope::Data* bindings = nullptr;
    uint32_t numBindings = imports.length() + vars.length() + lets.length() + consts.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<ModuleScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in ModuleScope.
        BindingName* start = bindings->names;
        BindingName* cursor = start;

        PodCopy(cursor, imports.begin(), imports.length());
        cursor += imports.length();

        bindings->varStart = cursor - start;
        PodCopy(cursor, vars.begin(), vars.length());
        cursor += vars.length();

        bindings->letStart = cursor - start;
        PodCopy(cursor, lets.begin(), lets.length());
        cursor += lets.length();

        bindings->constStart = cursor - start;
        PodCopy(cursor, consts.begin(), consts.length());
        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<EvalScope::Data*>
Parser<FullParseHandler>::newEvalScopeData(ParseContext::Scope& scope,
                                           uint32_t* functionBindingEnd)
{
    Vector<BindingName> funs(context);
    Vector<BindingName> vars(context);

    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        // Eval scopes only contain 'var' bindings. Make all bindings aliased
        // for now.
        MOZ_ASSERT(bi.kind() == BindingKind::Var);
        BindingName binding(bi.name(), true);
        if (bi.declarationKind() == DeclarationKind::BodyLevelFunction) {
            if (!funs.append(binding))
                return Nothing();
        } else {
            if (!vars.append(binding))
                return Nothing();
        }
    }

    EvalScope::Data* bindings = nullptr;
    uint32_t numBindings = funs.length() + vars.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<EvalScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        BindingName* start = bindings->names;
        BindingName* cursor = start;

        // Keep track of what vars are functions. This is only used in BCE to omit
        // superfluous DEFVARs.
        PodCopy(cursor, funs.begin(), funs.length());
        cursor += funs.length();
        *functionBindingEnd = cursor - start;

        PodCopy(cursor, vars.begin(), vars.length());
        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<FunctionScope::Data*>
Parser<FullParseHandler>::newFunctionScopeData(ParseContext::Scope& scope, bool hasParameterExprs)
{
    Vector<BindingName> positionalFormals(context);
    Vector<BindingName> formals(context);
    Vector<BindingName> vars(context);

    bool allBindingsClosedOver = pc->sc()->allBindingsClosedOver();

    // Positional parameter names must be added in order of appearance as they are
    // referenced using argument slots.
    for (size_t i = 0; i < pc->positionalFormalParameterNames().length(); i++) {
        JSAtom* name = pc->positionalFormalParameterNames()[i];

        BindingName bindName;
        if (name) {
            DeclaredNamePtr p = scope.lookupDeclaredName(name);

            // Do not consider any positional formal parameters closed over if
            // there are parameter defaults. It is the binding in the defaults
            // scope that is closed over instead.
            bindName = BindingName(name, (allBindingsClosedOver ||
                                          (p && p->value()->closedOver())));
        }

        if (!positionalFormals.append(bindName))
            return Nothing();
    }

    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        BindingName binding(bi.name(), allBindingsClosedOver || bi.closedOver());
        switch (bi.kind()) {
          case BindingKind::FormalParameter:
            // Positional parameter names are already handled above.
            if (bi.declarationKind() == DeclarationKind::FormalParameter) {
                if (!formals.append(binding))
                    return Nothing();
            }
            break;
          case BindingKind::Var:
            // The only vars in the function scope when there are parameter
            // exprs, which induces a separate var environment, should be the
            // special internal bindings.
            MOZ_ASSERT_IF(hasParameterExprs,
                          bi.name() == context->names().arguments ||
                          bi.name() == context->names().dotThis ||
                          bi.name() == context->names().dotGenerator);
            if (!vars.append(binding))
                return Nothing();
            break;
          default:
            break;
        }
    }

    FunctionScope::Data* bindings = nullptr;
    uint32_t numBindings = positionalFormals.length() + formals.length() + vars.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<FunctionScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in FunctionScope.
        BindingName* start = bindings->names;
        BindingName* cursor = start;

        PodCopy(cursor, positionalFormals.begin(), positionalFormals.length());
        cursor += positionalFormals.length();

        bindings->nonPositionalFormalStart = cursor - start;
        PodCopy(cursor, formals.begin(), formals.length());
        cursor += formals.length();

        bindings->varStart = cursor - start;
        PodCopy(cursor, vars.begin(), vars.length());
        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<VarScope::Data*>
Parser<FullParseHandler>::newVarScopeData(ParseContext::Scope& scope)
{
    Vector<BindingName> vars(context);

    bool allBindingsClosedOver = pc->sc()->allBindingsClosedOver();

    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        BindingName binding(bi.name(), allBindingsClosedOver || bi.closedOver());
        if (!vars.append(binding))
            return Nothing();
    }

    VarScope::Data* bindings = nullptr;
    uint32_t numBindings = vars.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<VarScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in FunctionScope.
        BindingName* start = bindings->names;
        BindingName* cursor = start;

        PodCopy(cursor, vars.begin(), vars.length());
        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<LexicalScope::Data*>
Parser<FullParseHandler>::newLexicalScopeData(ParseContext::Scope& scope)
{
    Vector<BindingName> lets(context);
    Vector<BindingName> consts(context);

    // Unlike other scopes with bindings which are body-level, it is unknown
    // if pc->sc()->allBindingsClosedOver() is correct at the time of
    // finishing parsing a lexical scope.
    //
    // Instead, pc->sc()->allBindingsClosedOver() is checked in
    // EmitterScope::enterLexical. Also see comment there.
    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        BindingName binding(bi.name(), bi.closedOver());
        switch (bi.kind()) {
          case BindingKind::Let:
            if (!lets.append(binding))
                return Nothing();
            break;
          case BindingKind::Const:
            if (!consts.append(binding))
                return Nothing();
            break;
          default:
            break;
        }
    }

    LexicalScope::Data* bindings = nullptr;
    uint32_t numBindings = lets.length() + consts.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<LexicalScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in LexicalScope.
        BindingName* cursor = bindings->names;
        BindingName* start = cursor;

        PodCopy(cursor, lets.begin(), lets.length());
        cursor += lets.length();

        bindings->constStart = cursor - start;
        PodCopy(cursor, consts.begin(), consts.length());
        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::finishLexicalScope(ParseContext::Scope& scope, Node body)
{
    if (!propagateFreeNamesAndMarkClosedOverBindings(scope))
        return null();
    return body;
}

template <>
ParseNode*
Parser<FullParseHandler>::finishLexicalScope(ParseContext::Scope& scope, ParseNode* body)
{
    if (!propagateFreeNamesAndMarkClosedOverBindings(scope))
        return nullptr;
    Maybe<LexicalScope::Data*> bindings = newLexicalScopeData(scope);
    if (!bindings)
        return nullptr;
    return handler.newLexicalScope(*bindings, body);
}

static bool
IsArgumentsUsedInLegacyGenerator(ExclusiveContext* cx, Scope* scope)
{
    JSAtom* argumentsName = cx->names().arguments;
    for (ScopeIter si(scope); si; si++) {
        if (si.scope()->is<LexicalScope>()) {
            // Using a shadowed lexical 'arguments' is okay.
            for (::BindingIter bi(si.scope()); bi; bi++) {
                if (bi.name() == argumentsName)
                    return false;
            }
        } else if (si.scope()->is<FunctionScope>()) {
            // It's an error to use 'arguments' in a legacy generator expression.
            JSScript* script = si.scope()->as<FunctionScope>().script();
            return script->isGeneratorExp() && script->isLegacyGenerator();
        }
    }

    return false;
}

template <>
ParseNode*
Parser<FullParseHandler>::evalBody(EvalSharedContext* evalsc)
{
    ParseContext evalpc(this, evalsc, /* newDirectives = */ nullptr);
    if (!evalpc.init())
        return nullptr;

    ParseContext::VarScope varScope(this);
    if (!varScope.init(pc))
        return nullptr;

    // All evals have an implicit non-extensible lexical scope.
    ParseContext::Scope lexicalScope(this);
    if (!lexicalScope.init(pc))
        return nullptr;

    ParseNode* body = statementList(YieldIsName);
    if (!body)
        return nullptr;

    if (!checkStatementsEOF())
        return nullptr;

    body = finishLexicalScope(lexicalScope, body);
    if (!body)
        return nullptr;

    // It's an error to use 'arguments' in a legacy generator expression.
    //
    // If 'arguments' appears free (i.e. not a declared name) or if the
    // declaration does not shadow the enclosing script's 'arguments'
    // binding (i.e. not a lexical declaration), check the enclosing
    // script.
    if (hasUsedName(context->names().arguments)) {
        if (IsArgumentsUsedInLegacyGenerator(context, pc->sc()->compilationEnclosingScope())) {
            report(ParseError, false, nullptr, JSMSG_BAD_GENEXP_BODY, js_arguments_str);
            return nullptr;
        }
    }

#ifdef DEBUG
    if (evalpc.superScopeNeedsHomeObject() && evalsc->compilationEnclosingScope()) {
        // If superScopeNeedsHomeObject_ is set and we are an entry-point
        // ParseContext, then we must be emitting an eval script, and the
        // outer function must already be marked as needing a home object
        // since it contains an eval.
        ScopeIter si(evalsc->compilationEnclosingScope());
        for (; si; si++) {
            if (si.kind() == ScopeKind::Function) {
                JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();
                if (fun->isArrow())
                    continue;
                MOZ_ASSERT(fun->allowSuperProperty());
                MOZ_ASSERT(fun->nonLazyScript()->needsHomeObject());
                break;
            }
        }
        MOZ_ASSERT(!si.done(),
                   "Eval must have found an enclosing function box scope that allows super.property");
    }
#endif

    if (!FoldConstants(context, &body, this))
        return nullptr;

    uint32_t functionBindingEnd = 0;
    Maybe<EvalScope::Data*> bindings =
        newEvalScopeData(pc->varScope(), &functionBindingEnd);
    if (!bindings)
        return nullptr;

    evalsc->bindings = *bindings;
    evalsc->functionBindingEnd = functionBindingEnd;

    return body;
}

template <>
ParseNode*
Parser<FullParseHandler>::globalBody(GlobalSharedContext* globalsc)
{
    ParseContext globalpc(this, globalsc, /* newDirectives = */ nullptr);
    if (!globalpc.init())
        return nullptr;

    ParseContext::VarScope varScope(this);
    if (!varScope.init(pc))
        return nullptr;

    ParseNode* body = statementList(YieldIsName);
    if (!body)
        return nullptr;

    if (!checkStatementsEOF())
        return nullptr;

    if (!FoldConstants(context, &body, this))
        return nullptr;

    uint32_t functionBindingEnd = 0;
    Maybe<GlobalScope::Data*> bindings = newGlobalScopeData(pc->varScope(), &functionBindingEnd);
    if (!bindings)
        return nullptr;

    globalsc->bindings = *bindings;
    globalsc->functionBindingEnd = functionBindingEnd;

    return body;
}

template <>
ParseNode*
Parser<FullParseHandler>::moduleBody(ModuleSharedContext* modulesc)
{
    MOZ_ASSERT(checkOptionsCalled);

    ParseContext modulepc(this, modulesc, nullptr);
    if (!modulepc.init())
        return null();

    ParseContext::VarScope varScope(this);
    if (!varScope.init(pc))
        return nullptr;

    Node mn = handler.newModule();
    if (!mn)
        return null();

    ParseNode* pn = statementList(YieldIsKeyword);
    if (!pn)
        return null();

    MOZ_ASSERT(pn->isKind(PNK_STATEMENTLIST));
    mn->pn_body = pn;

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    if (tt != TOK_EOF) {
        report(ParseError, false, null(), JSMSG_GARBAGE_AFTER_INPUT, "module", TokenKindToDesc(tt));
        return null();
    }

    if (!modulesc->builder.buildTables())
        return null();

    // Check exported local bindings exist and mark them as closed over.
    for (auto entry : modulesc->builder.localExportEntries()) {
        JSAtom* name = entry->localName();
        MOZ_ASSERT(name);

        DeclaredNamePtr p = modulepc.varScope().lookupDeclaredName(name);
        if (!p) {
            JSAutoByteString str;
            if (!str.encodeLatin1(context, name))
                return null();

            JS_ReportErrorNumber(context->asJSContext(), GetErrorMessage, nullptr,
                                 JSMSG_MISSING_EXPORT, str.ptr());
            return null();
        }

        p->value()->setClosedOver();
    }

    if (!FoldConstants(context, &pn, this))
        return null();

    if (!propagateFreeNamesAndMarkClosedOverBindings(modulepc.varScope()))
        return null();

    Maybe<ModuleScope::Data*> bindings = newModuleScopeData(modulepc.varScope());
    if (!bindings)
        return nullptr;

    modulesc->bindings = *bindings;
    return mn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::moduleBody(ModuleSharedContext* modulesc)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::hasUsedFunctionSpecialName(HandlePropertyName name)
{
    MOZ_ASSERT(name == context->names().arguments || name == context->names().dotThis);
    return hasUsedName(name) || pc->functionBox()->bindingsAccessedDynamically();
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::declareFunctionThis()
{
    // The asm.js validator does all its own symbol-table management so, as an
    // optimization, avoid doing any work here.
    if (pc->useAsmOrInsideUseAsm())
        return true;

    // Derived class constructors emit JSOP_CHECKRETURN, which requires
    // '.this' to be bound.
    FunctionBox* funbox = pc->functionBox();
    HandlePropertyName dotThis = context->names().dotThis;

    bool declareThis;
    if (handler.canSkipLazyClosedOverBindings())
        declareThis = funbox->function()->lazyScript()->hasThisBinding();
    else
        declareThis = hasUsedFunctionSpecialName(dotThis) || funbox->isDerivedClassConstructor();

    if (declareThis) {
        ParseContext::Scope& funScope = pc->functionScope();
        AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotThis);
        MOZ_ASSERT(!p);
        if (!funScope.addDeclaredName(pc, p, dotThis, DeclarationKind::Var))
            return false;
        funbox->setHasThisBinding();
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newInternalDotName(HandlePropertyName name)
{
    Node nameNode = newName(name);
    if (!nameNode)
        return null();
    if (!noteUsedName(name))
        return null();
    return nameNode;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newThisName()
{
    return newInternalDotName(context->names().dotThis);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newDotGeneratorName()
{
    return newInternalDotName(context->names().dotGenerator);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::declareDotGeneratorName()
{
    // The special '.generator' binding must be on the function scope, as
    // generators expect to find it on the CallObject.
    ParseContext::Scope& funScope = pc->functionScope();
    HandlePropertyName dotGenerator = context->names().dotGenerator;
    AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotGenerator);
    if (!p && !funScope.addDeclaredName(pc, p, dotGenerator, DeclarationKind::Var))
        return false;
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::finishFunctionScopes()
{
    FunctionBox* funbox = pc->functionBox();

    if (funbox->hasParameterExprs) {
        if (!propagateFreeNamesAndMarkClosedOverBindings(pc->functionScope()))
            return false;
    }

    if (funbox->function()->isNamedLambda()) {
        if (!propagateFreeNamesAndMarkClosedOverBindings(pc->namedLambdaScope()))
            return false;
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::finishFunction()
{
    if (!finishFunctionScopes())
        return false;

    FunctionBox* funbox = pc->functionBox();
    bool hasParameterExprs = funbox->hasParameterExprs;

    if (hasParameterExprs) {
        Maybe<VarScope::Data*> bindings = newVarScopeData(pc->varScope());
        if (!bindings)
            return false;
        funbox->extraVarScopeBindings().set(*bindings);
    }

    {
        Maybe<FunctionScope::Data*> bindings = newFunctionScopeData(pc->functionScope(),
                                                                    hasParameterExprs);
        if (!bindings)
            return false;
        funbox->functionScopeBindings().set(*bindings);
    }

    if (funbox->function()->isNamedLambda()) {
        Maybe<LexicalScope::Data*> bindings = newLexicalScopeData(pc->namedLambdaScope());
        if (!bindings)
            return false;
        funbox->namedLambdaBindings().set(*bindings);
    }

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::finishFunction()
{
    // The LazyScript for a lazily parsed function needs to know its set of
    // free variables and inner functions so that when it is fully parsed, we
    // can skip over any already syntax parsed inner functions and still
    // retain correct scope information.

    if (!finishFunctionScopes())
        return false;

    // There are too many bindings or inner functions to be saved into the
    // LazyScript. Do a full parse.
    if (pc->closedOverBindingsForLazy().length() >= LazyScript::NumClosedOverBindingsLimit ||
        pc->innerFunctionsForLazy.length() >= LazyScript::NumInnerFunctionsLimit)
    {
        MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
        return false;
    }

    FunctionBox* funbox = pc->functionBox();
    RootedFunction fun(context, funbox->function());
    LazyScript* lazy = LazyScript::Create(context, fun, pc->closedOverBindingsForLazy(),
                                          pc->innerFunctionsForLazy, versionNumber(),
                                          funbox->bufStart, funbox->bufEnd,
                                          funbox->startLine, funbox->startColumn);
    if (!lazy)
        return false;

    // Flags that need to be copied into the JSScript when we do the full
    // parse.
    if (pc->sc()->strict())
        lazy->setStrict();
    lazy->setGeneratorKind(funbox->generatorKind());
    if (funbox->isLikelyConstructorWrapper())
        lazy->setLikelyConstructorWrapper();
    if (funbox->isDerivedClassConstructor())
        lazy->setIsDerivedClassConstructor();
    if (funbox->needsHomeObject())
        lazy->setNeedsHomeObject();
    if (funbox->declaredArguments)
        lazy->setShouldDeclareArguments();
    if (funbox->hasThisBinding())
        lazy->setHasThisBinding();

    // Flags that need to copied back into the parser when we do the full
    // parse.
    PropagateTransitiveParseFlags(funbox, lazy);

    fun->initLazyScript(lazy);
    return true;
}

template <>
ParseNode*
Parser<FullParseHandler>::standaloneFunctionBody(HandleFunction fun,
                                                 HandleScope enclosingScope,
                                                 Handle<PropertyNameVector> formals,
                                                 GeneratorKind generatorKind,
                                                 Directives inheritedDirectives,
                                                 Directives* newDirectives)
{
    MOZ_ASSERT(checkOptionsCalled);

    Node fn = handler.newFunctionDefinition();
    if (!fn)
        return null();

    ParseNode* argsbody = handler.newList(PNK_PARAMSBODY);
    if (!argsbody)
        return null();
    fn->pn_body = argsbody;

    FunctionBox* funbox = newFunctionBox(fn, fun, inheritedDirectives, generatorKind,
                                         /* tryAnnexB = */ false);
    if (!funbox)
        return null();
    funbox->initStandaloneFunction(enclosingScope);

    ParseContext funpc(this, funbox, newDirectives);
    if (!funpc.init())
        return null();
    funpc.setIsStandaloneFunctionBody();
    funpc.functionScope().useAsVarScope(&funpc);

    if (formals.length() >= ARGNO_LIMIT) {
        report(ParseError, false, null(), JSMSG_TOO_MANY_FUN_ARGS);
        return null();
    }

    for (uint32_t i = 0; i < formals.length(); i++) {
        if (!notePositionalFormalParameter(fn, formals[i]))
            return null();
    }

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    ParseNode* pn = functionBody(InAllowed, yieldHandling, Statement, StatementListBody);
    if (!pn)
        return null();

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    if (tt != TOK_EOF) {
        report(ParseError, false, null(), JSMSG_GARBAGE_AFTER_INPUT,
               "function body", TokenKindToDesc(tt));
        return null();
    }

    if (!FoldConstants(context, &pn, this))
        return null();

    fn->pn_pos.end = pos().end;

    MOZ_ASSERT(fn->pn_body->isKind(PNK_PARAMSBODY));
    fn->pn_body->append(pn);

    if (!finishFunction())
        return null();

    return fn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::declareFunctionArgumentsObject()
{
    FunctionBox* funbox = pc->functionBox();
    ParseContext::Scope& funScope = pc->functionScope();
    ParseContext::Scope& varScope = pc->varScope();

    bool hasExtraBodyVarScope = &funScope != &varScope;

    // Time to implement the odd semantics of 'arguments'.
    HandlePropertyName argumentsName = context->names().arguments;

    bool tryDeclareArguments;
    if (handler.canSkipLazyClosedOverBindings())
        tryDeclareArguments = funbox->function()->lazyScript()->shouldDeclareArguments();
    else
        tryDeclareArguments = hasUsedFunctionSpecialName(argumentsName);

    // ES 9.2.12 steps 19 and 20 say formal parameters, lexical bindings,
    // and body-level functions named 'arguments' shadow the arguments
    // object.
    //
    // So even if there wasn't a free use of 'arguments' but there is a var
    // binding of 'arguments', we still might need the arguments object.
    //
    // If we have an extra var scope due to parameter expressions and the body
    // declared 'var arguments', we still need to declare 'arguments' in the
    // function scope.
    DeclaredNamePtr p = varScope.lookupDeclaredName(argumentsName);
    if (p && (p->value()->kind() == DeclarationKind::Var ||
              p->value()->kind() == DeclarationKind::ForOfVar))
    {
        if (hasExtraBodyVarScope)
            tryDeclareArguments = true;
        else
            funbox->usesArguments = true;
    }

    if (tryDeclareArguments) {
        AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(argumentsName);
        if (!p) {
            if (!funScope.addDeclaredName(pc, p, argumentsName, DeclarationKind::Var))
                return false;
            funbox->declaredArguments = true;
            funbox->usesArguments = true;
        } else if (hasExtraBodyVarScope) {
            // Formal parameters shadow the arguments object.
            return true;
        }
    }

    // Compute if we need an arguments object.
    if (funbox->usesArguments) {
        // There is an 'arguments' binding. Is the arguments object definitely
        // needed?
        //
        // Also see the flags' comments in ContextFlags.
        funbox->setArgumentsHasLocalBinding();

        // Dynamic scope access destroys all hope of optimization.
        if (pc->sc()->bindingsAccessedDynamically())
            funbox->setDefinitelyNeedsArgsObj();

        // If a script contains the debugger statement either directly or
        // within an inner function, the arguments object should be created
        // eagerly so the Debugger API may observe bindings.
        if (pc->sc()->hasDebuggerStatement())
            funbox->setDefinitelyNeedsArgsObj();
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionBody(InHandling inHandling, YieldHandling yieldHandling,
                                   FunctionSyntaxKind kind, FunctionBodyType type)
{
    MOZ_ASSERT(pc->isFunctionBox());
    MOZ_ASSERT(!pc->funHasReturnExpr && !pc->funHasReturnVoid);

#ifdef DEBUG
    uint32_t startYieldOffset = pc->lastYieldOffset;
#endif

    Node pn;
    if (type == StatementListBody) {
        pn = statementList(yieldHandling);
        if (!pn)
            return null();
    } else {
        MOZ_ASSERT(type == ExpressionBody);

        Node kid = assignExpr(inHandling, yieldHandling, TripledotProhibited);
        if (!kid)
            return null();

        pn = handler.newReturnStatement(kid, handler.getPosition(kid));
        if (!pn)
            return null();
    }

    switch (pc->generatorKind()) {
      case NotGenerator:
        MOZ_ASSERT(pc->lastYieldOffset == startYieldOffset);
        break;

      case LegacyGenerator:
        MOZ_ASSERT(pc->lastYieldOffset != startYieldOffset);

        // These should throw while parsing the yield expression.
        MOZ_ASSERT(kind != Arrow);
        MOZ_ASSERT(!IsGetterKind(kind));
        MOZ_ASSERT(!IsSetterKind(kind));
        MOZ_ASSERT(!IsConstructorKind(kind));
        MOZ_ASSERT(kind != Method);
        MOZ_ASSERT(type != ExpressionBody);
        break;

      case StarGenerator:
        MOZ_ASSERT(kind != Arrow);
        MOZ_ASSERT(type == StatementListBody);
        break;
    }

    if (pc->isGenerator()) {
        MOZ_ASSERT(type == StatementListBody);
        if (!declareDotGeneratorName())
            return null();
        Node generator = newDotGeneratorName();
        if (!generator)
            return null();
        if (!handler.prependInitialYield(pn, generator))
            return null();
    }

    // Declare the 'arguments' and 'this' bindings if necessary before
    // finishing up the scope so these special bindings get marked as closed
    // over if necessary. Arrow functions don't have these bindings.
    if (kind != Arrow) {
        if (!declareFunctionArgumentsObject())
            return null();
        if (!declareFunctionThis())
            return null();
    }

    return finishLexicalScope(pc->varScope(), pn);
}

template <typename ParseHandler>
JSFunction*
Parser<ParseHandler>::newFunction(HandleAtom atom, FunctionSyntaxKind kind,
                                  GeneratorKind generatorKind, HandleObject proto)
{
    MOZ_ASSERT_IF(kind == Statement, atom != nullptr);

    RootedFunction fun(context);

    gc::AllocKind allocKind = gc::AllocKind::FUNCTION;
    JSFunction::Flags flags;
#ifdef DEBUG
    bool isGlobalSelfHostedBuiltin = false;
#endif
    switch (kind) {
      case Expression:
        flags = (generatorKind == NotGenerator
                 ? JSFunction::INTERPRETED_LAMBDA
                 : JSFunction::INTERPRETED_LAMBDA_GENERATOR);
        break;
      case Arrow:
        flags = JSFunction::INTERPRETED_LAMBDA_ARROW;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case Method:
        MOZ_ASSERT(generatorKind == NotGenerator || generatorKind == StarGenerator);
        flags = (generatorKind == NotGenerator
                 ? JSFunction::INTERPRETED_METHOD
                 : JSFunction::INTERPRETED_METHOD_GENERATOR);
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case ClassConstructor:
      case DerivedClassConstructor:
        flags = JSFunction::INTERPRETED_CLASS_CONSTRUCTOR;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case Getter:
      case GetterNoExpressionClosure:
        flags = JSFunction::INTERPRETED_GETTER;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case Setter:
      case SetterNoExpressionClosure:
        flags = JSFunction::INTERPRETED_SETTER;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      default:
        MOZ_ASSERT(kind == Statement);
#ifdef DEBUG
        if (options().selfHostingMode && !pc->isFunctionBox()) {
            isGlobalSelfHostedBuiltin = true;
            allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        }
#endif
        flags = (generatorKind == NotGenerator
                 ? JSFunction::INTERPRETED_NORMAL
                 : JSFunction::INTERPRETED_GENERATOR);
    }

    fun = NewFunctionWithProto(context, nullptr, 0, flags, nullptr, atom, proto,
                               allocKind, TenuredObject);
    if (!fun)
        return nullptr;
    if (options().selfHostingMode) {
        fun->setIsSelfHostedBuiltin();
#ifdef DEBUG
        if (isGlobalSelfHostedBuiltin)
            fun->setExtendedSlot(HAS_SELFHOSTED_CANONICAL_NAME_SLOT, BooleanValue(false));
#endif
    }
    return fun;
}

/*
 * WARNING: Do not call this function directly.
 * Call either MatchOrInsertSemicolonAfterExpression or
 * MatchOrInsertSemicolonAfterNonExpression instead, depending on context.
 */
static bool
MatchOrInsertSemicolonHelper(TokenStream& ts, TokenStream::Modifier modifier)
{
    TokenKind tt = TOK_EOF;
    if (!ts.peekTokenSameLine(&tt, modifier))
        return false;
    if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
        /* Advance the scanner for proper error location reporting. */
        ts.consumeKnownToken(tt, modifier);
        ts.reportError(JSMSG_SEMI_BEFORE_STMNT);
        return false;
    }
    bool matched;
    if (!ts.matchToken(&matched, TOK_SEMI, modifier))
        return false;
    if (!matched && modifier == TokenStream::None)
        ts.addModifierException(TokenStream::OperandIsNone);
    return true;
}

static bool
MatchOrInsertSemicolonAfterExpression(TokenStream& ts)
{
    return MatchOrInsertSemicolonHelper(ts, TokenStream::None);
}

static bool
MatchOrInsertSemicolonAfterNonExpression(TokenStream& ts)
{
    return MatchOrInsertSemicolonHelper(ts, TokenStream::Operand);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::leaveInnerFunction(ParseContext* outerpc)
{
    MOZ_ASSERT(pc != outerpc);

    // If the current function allows super.property but cannot have a home
    // object, i.e., it is an arrow function, we need to propagate the flag to
    // the outer ParseContext.
    if (pc->superScopeNeedsHomeObject()) {
        if (!pc->isArrowFunction())
            MOZ_ASSERT(pc->functionBox()->needsHomeObject());
        else
            outerpc->setSuperScopeNeedsHomeObject();
    }

    // Lazy functions inner to another lazy function need to be remembered by
    // the inner function so that if the outer function is eventually parsed
    // we do not need any further parsing or processing of the inner function.
    //
    // Append the inner function here unconditionally; the vector is only used
    // if the Parser using outerpc is a syntax parsing. See
    // Parser<SyntaxParseHandler>::finishFunction.
    if (!outerpc->innerFunctionsForLazy.append(pc->functionBox()->function()))
        return false;

    PropagateTransitiveParseFlags(pc->functionBox(), outerpc->sc());

    return true;
}

template <typename ParseHandler>
JSAtom*
Parser<ParseHandler>::prefixAccessorName(PropertyType propType, HandleAtom propAtom)
{
    RootedAtom prefix(context);
    if (propType == PropertyType::Setter || propType == PropertyType::SetterNoExpressionClosure) {
        prefix = context->names().setPrefix;
    } else {
        MOZ_ASSERT(propType == PropertyType::Getter || propType == PropertyType::GetterNoExpressionClosure);
        prefix = context->names().getPrefix;
    }

    RootedString str(context, ConcatStrings<CanGC>(context, prefix, propAtom));
    if (!str)
        return nullptr;

    return AtomizeString(context, str);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionArguments(YieldHandling yieldHandling, FunctionSyntaxKind kind,
                                        Node funcpn)
{
    FunctionBox* funbox = pc->functionBox();

    bool parenFreeArrow = false;
    TokenStream::Modifier modifier = TokenStream::None;
    if (kind == Arrow) {
        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return false;
        if (tt == TOK_NAME)
            parenFreeArrow = true;
        else
            modifier = TokenStream::Operand;
    }
    if (!parenFreeArrow) {
        TokenKind tt;
        if (!tokenStream.getToken(&tt, modifier))
            return false;
        if (tt != TOK_LP) {
            report(ParseError, false, null(),
                   kind == Arrow ? JSMSG_BAD_ARROW_ARGS : JSMSG_PAREN_BEFORE_FORMAL);
            return false;
        }

        // Record the start of function source (for FunctionToString). If we
        // are parenFreeArrow, we will set this below, after consuming the NAME.
        funbox->setStart(tokenStream);
    }

    Node argsbody = handler.newList(PNK_PARAMSBODY);
    if (!argsbody)
        return false;
    handler.setFunctionFormalParametersAndBody(funcpn, argsbody);

    bool hasArguments = false;
    if (parenFreeArrow) {
        hasArguments = true;
    } else {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_RP, TokenStream::Operand))
            return false;
        if (!matched)
            hasArguments = true;
    }
    if (hasArguments) {
        bool hasRest = false;
        bool duplicatedParam = false;
        bool disallowDuplicateParams = kind == Arrow || kind == Method || kind == ClassConstructor;
        AtomVector& positionalFormals = pc->positionalFormalParameterNames();

        if (IsGetterKind(kind)) {
            report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "getter", "no", "s");
            return false;
        }

        while (true) {
            if (hasRest) {
                report(ParseError, false, null(), JSMSG_PARAMETER_AFTER_REST);
                return false;
            }

            TokenKind tt;
            if (!tokenStream.getToken(&tt, TokenStream::Operand))
                return false;
            MOZ_ASSERT_IF(parenFreeArrow, tt == TOK_NAME);
            switch (tt) {
              case TOK_LB:
              case TOK_LC: {
                /* See comment below in the TOK_NAME case. */
                disallowDuplicateParams = true;
                if (duplicatedParam) {
                    report(ParseError, false, null(), JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                funbox->hasDestructuringArgs = true;

                Node destruct = destructuringDeclarationWithoutYield(
                    DeclarationKind::FormalParameter,
                    yieldHandling, tt,
                    JSMSG_YIELD_IN_DEFAULT);
                if (!destruct)
                    return false;

                if (!noteDestructuredPositionalFormalParameter(funcpn, destruct))
                    return false;

                break;
              }

              case TOK_YIELD:
                if (!checkYieldNameValidity())
                    return false;
                MOZ_ASSERT(yieldHandling == YieldIsName);
                goto TOK_NAME;

              case TOK_TRIPLEDOT: {
                if (IsSetterKind(kind)) {
                    report(ParseError, false, null(),
                           JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
                    return false;
                }

                hasRest = true;
                funbox->function()->setHasRest();

                if (!tokenStream.getToken(&tt))
                    return false;
                // FIXME: This fails to handle a rest parameter named |yield|
                //        correctly outside of generators: that is,
                //        |var f = (...yield) => 42;| should be valid code!
                //        When this is fixed, make sure to consult both
                //        |yieldHandling| and |checkYieldNameValidity| for
                //        correctness until legacy generator syntax is removed.
                if (tt != TOK_NAME) {
                    report(ParseError, false, null(), JSMSG_NO_REST_NAME);
                    return false;
                }
                disallowDuplicateParams = true;
                if (duplicatedParam) {
                    // Has duplicated args before the rest parameter.
                    report(ParseError, false, null(), JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                goto TOK_NAME;
              }

              TOK_NAME:
              case TOK_NAME: {
                if (parenFreeArrow)
                    funbox->setStart(tokenStream);

                RootedPropertyName name(context, tokenStream.currentName());
                if (!notePositionalFormalParameter(funcpn, name, disallowDuplicateParams,
                                                   &duplicatedParam))
                {
                    return false;
                }

                break;
              }

              default:
                report(ParseError, false, null(), JSMSG_MISSING_FORMAL);
                return false;
            }

            if (positionalFormals.length() >= ARGNO_LIMIT) {
                report(ParseError, false, null(), JSMSG_TOO_MANY_FUN_ARGS);
                return false;
            }

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_ASSIGN))
                return false;
            if (matched) {
                // A default argument without parentheses would look like:
                // a = expr => body, but both operators are right-associative, so
                // that would have been parsed as a = (expr => body) instead.
                // Therefore it's impossible to get here with parenFreeArrow.
                MOZ_ASSERT(!parenFreeArrow);

                if (hasRest) {
                    report(ParseError, false, null(), JSMSG_REST_WITH_DEFAULT);
                    return false;
                }
                disallowDuplicateParams = true;
                if (duplicatedParam) {
                    report(ParseError, false, null(), JSMSG_BAD_DUP_ARGS);
                    return false;
                }
                if (!funbox->hasParameterExprs) {
                    funbox->hasParameterExprs = true;

                    // The Function.length property is the number of formals
                    // before the first default argument.
                    funbox->length = positionalFormals.length() - 1;
                }

                Node def_expr = assignExprWithoutYield(yieldHandling, JSMSG_YIELD_IN_DEFAULT);
                if (!def_expr)
                    return false;
                if (!handler.setLastFunctionFormalParameterDefault(funcpn, def_expr))
                    return false;
            }

            if (parenFreeArrow || IsSetterKind(kind))
                break;

            if (!tokenStream.matchToken(&matched, TOK_COMMA))
                return false;
            if (!matched)
                break;
        }

        if (!parenFreeArrow) {
            TokenKind tt;
            if (!tokenStream.getToken(&tt))
                return false;
            if (tt != TOK_RP) {
                if (IsSetterKind(kind)) {
                    report(ParseError, false, null(),
                           JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
                    return false;
                }

                report(ParseError, false, null(), JSMSG_PAREN_AFTER_FORMAL);
                return false;
            }
        }

        if (!funbox->hasParameterExprs)
            funbox->length = positionalFormals.length() - hasRest;
        else if (funbox->hasDirectEval())
            funbox->hasDirectEvalInParameterExpr = true;

        funbox->function()->setArgCount(positionalFormals.length());
    } else if (IsSetterKind(kind)) {
        report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
        return false;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkFunctionDefinition(HandleAtom funAtom, Node pn, FunctionSyntaxKind kind,
                                              bool *tryAnnexB)
{
    if (kind == Statement) {
        TokenPos pos = handler.getPosition(pn);
        RootedPropertyName funName(context, funAtom->asPropertyName());

        // In sloppy mode, Annex B.3.2 allows labelled function
        // declarations. Otherwise it is a parse error.
        ParseContext::Statement* declaredInStmt = pc->innermostStatement();
        if (declaredInStmt && declaredInStmt->kind() == StatementKind::Label) {
            if (pc->sc()->strict()) {
                reportWithOffset(ParseError, false, pos.begin, JSMSG_FUNCTION_LABEL);
                return false;
            }

            // Find the innermost non-label statement.
            while (declaredInStmt && declaredInStmt->kind() == StatementKind::Label)
                declaredInStmt = declaredInStmt->enclosing();

            if (declaredInStmt && !StatementKindIsBraced(declaredInStmt->kind())) {
                reportWithOffset(ParseError, false, pos.begin, JSMSG_SLOPPY_FUNCTION_LABEL);
                return false;
            }
        }

        if (declaredInStmt) {
            DeclarationKind declKind = DeclarationKind::LexicalFunction;
            if (!checkLexicalDeclarationDirectlyWithinBlock(*declaredInStmt, declKind, pos))
                return false;

            if (!pc->sc()->strict()) {
                // Under sloppy mode, try Annex B.3.3 semantics. If making an
                // additional 'var' binding of the same name does not throw an
                // early error, do so. This 'var' binding would be assigned
                // the function object when its declaration is reached, not at
                // the start of the block.

                if (!tryDeclareVarForAnnexBLexicalFunction(funName, tryAnnexB))
                    return false;
            }

            if (!noteDeclaredName(funName, declKind, pos))
                return false;
        } else {
            if (!noteDeclaredName(funName, DeclarationKind::BodyLevelFunction, pos))
                return false;

            // Body-level functions in modules are always closed over.
            if (pc->atModuleLevel())
                pc->varScope().lookupDeclaredName(funName)->value()->setClosedOver();
        }
    } else {
        // A function expression does not introduce any binding.
        if (kind == Arrow) {
            /* Arrow functions cannot yet be parsed lazily. */
            if (!abortIfSyntaxParser())
                return false;
            handler.setOp(pn, JSOP_LAMBDA_ARROW);
        } else {
            handler.setOp(pn, JSOP_LAMBDA);
        }
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::skipLazyInnerFunction(ParseNode* pn, bool tryAnnexB)
{
    // When a lazily-parsed function is called, we only fully parse (and emit)
    // that function, not any of its nested children. The initial syntax-only
    // parse recorded the free variables of nested functions and their extents,
    // so we can skip over them after accounting for their free variables.

    RootedFunction fun(context, handler.nextLazyInnerFunction());
    MOZ_ASSERT(!fun->isLegacyGenerator());
    FunctionBox* funbox = newFunctionBox(pn, fun, Directives(/* strict = */ false),
                                         fun->generatorKind(), tryAnnexB);
    if (!funbox)
        return false;

    LazyScript* lazy = fun->lazyScript();
    if (lazy->needsHomeObject())
        funbox->setNeedsHomeObject();

    PropagateTransitiveParseFlags(lazy, pc->sc());

    // The position passed to tokenStream.advance() is an offset of the sort
    // returned by userbuf.offset() and expected by userbuf.rawCharPtrAt(),
    // while LazyScript::{begin,end} offsets are relative to the outermost
    // script source.
    Rooted<LazyScript*> lazyOuter(context, handler.lazyOuterFunction());
    uint32_t userbufBase = lazyOuter->begin() - lazyOuter->column();
    return tokenStream.advance(fun->lazyScript()->end() - userbufBase);
}

template <>
bool
Parser<SyntaxParseHandler>::skipLazyInnerFunction(Node pn, bool tryAnnexB)
{
    MOZ_CRASH("Cannot skip lazy inner functions when syntax parsing");
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::addExprAndGetNextTemplStrToken(YieldHandling yieldHandling, Node nodeList,
                                                     TokenKind* ttp)
{
    Node pn = expr(InAllowed, yieldHandling, TripledotProhibited);
    if (!pn)
        return false;
    handler.addList(nodeList, pn);

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;
    if (tt != TOK_RC) {
        report(ParseError, false, null(), JSMSG_TEMPLSTR_UNTERM_EXPR);
        return false;
    }

    return tokenStream.getToken(ttp, TokenStream::TemplateTail);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::taggedTemplate(YieldHandling yieldHandling, Node nodeList, TokenKind tt)
{
    Node callSiteObjNode = handler.newCallSiteObject(pos().begin);
    if (!callSiteObjNode)
        return false;
    handler.addList(nodeList, callSiteObjNode);

    while (true) {
        if (!appendToCallSiteObj(callSiteObjNode))
            return false;
        if (tt != TOK_TEMPLATE_HEAD)
            break;

        if (!addExprAndGetNextTemplStrToken(yieldHandling, nodeList, &tt))
            return false;
    }
    handler.setEndPosition(nodeList, callSiteObjNode);
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::templateLiteral(YieldHandling yieldHandling)
{
    Node pn = noSubstitutionTemplate();
    if (!pn)
        return null();

    Node nodeList = handler.newList(PNK_TEMPLATE_STRING_LIST, pn);
    if (!nodeList)
        return null();

    TokenKind tt;
    do {
        if (!addExprAndGetNextTemplStrToken(yieldHandling, nodeList, &tt))
            return null();

        pn = noSubstitutionTemplate();
        if (!pn)
            return null();

        handler.addList(nodeList, pn);
    } while (tt == TOK_TEMPLATE_HEAD);
    return nodeList;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionDefinition(InHandling inHandling, YieldHandling yieldHandling,
                                         HandleAtom funName, FunctionSyntaxKind kind,
                                         GeneratorKind generatorKind, InvokedPrediction invoked)
{
    MOZ_ASSERT_IF(kind == Statement, funName);

    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();

    if (invoked)
        pn = handler.setLikelyIIFE(pn);

    // Note the declared name and check for early errors.
    bool tryAnnexB = false;
    if (!checkFunctionDefinition(funName, pn, kind, &tryAnnexB))
        return null();

    // When fully parsing a LazyScript, we do not fully reparse its inner
    // functions, which are also lazy. Instead, their free variables and
    // source extents are recorded and may be skipped.
    if (handler.canSkipLazyInnerFunctions()) {
        if (!skipLazyInnerFunction(pn, tryAnnexB))
            return null();
        return pn;
    }

    RootedObject proto(context);
    if (generatorKind == StarGenerator) {
        // If we are off the main thread, the generator meta-objects have
        // already been created by js::StartOffThreadParseScript, so cx will not
        // be necessary.
        JSContext* cx = context->maybeJSContext();
        proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, context->global());
        if (!proto)
            return null();
    }
    RootedFunction fun(context, newFunction(funName, kind, generatorKind, proto));
    if (!fun)
        return null();

    // Speculatively parse using the directives of the parent parsing context.
    // If a directive is encountered (e.g., "use strict") that changes how the
    // function should have been parsed, we backup and reparse with the new set
    // of directives.
    Directives directives(pc);
    Directives newDirectives = directives;

    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    // Parse the inner function. The following is a loop as we may attempt to
    // reparse a function due to failed syntax parsing and encountering new
    // "use foo" directives.
    while (true) {
        if (trySyntaxParseInnerFunction(pn, fun, inHandling, kind, generatorKind, tryAnnexB,
                                        directives, &newDirectives))
        {
            break;
        }

        // Return on error.
        if (tokenStream.hadError() || directives == newDirectives)
            return null();

        // Assignment must be monotonic to prevent infinitely attempting to
        // reparse.
        MOZ_ASSERT_IF(directives.strict(), newDirectives.strict());
        MOZ_ASSERT_IF(directives.asmJS(), newDirectives.asmJS());
        directives = newDirectives;

        tokenStream.seek(start);

        // functionFormalParametersAndBody may have already set pn->pn_body before failing.
        handler.setFunctionFormalParametersAndBody(pn, null());
    }

    return pn;
}

template <>
bool
Parser<FullParseHandler>::trySyntaxParseInnerFunction(ParseNode* pn, HandleFunction fun,
                                                      InHandling inHandling,
                                                      FunctionSyntaxKind kind,
                                                      GeneratorKind generatorKind,
                                                      bool tryAnnexB,
                                                      Directives inheritedDirectives,
                                                      Directives* newDirectives)
{
    // Try a syntax parse for this inner function.
    do {
        // If we're assuming this function is an IIFE, always perform a full
        // parse to avoid the overhead of a lazy syntax-only parse. Although
        // the prediction may be incorrect, IIFEs are common enough that it
        // pays off for lots of code.
        if (pn->isLikelyIIFE() && generatorKind == NotGenerator)
            break;

        Parser<SyntaxParseHandler>* parser = handler.syntaxParser;
        if (!parser)
            break;

        UsedNameTracker::RewindToken token = usedNames.getRewindToken();

        // Move the syntax parser to the current position in the stream.
        TokenStream::Position position(keepAtoms);
        tokenStream.tell(&position);
        if (!parser->tokenStream.seek(position, tokenStream))
            return false;

        // Make a FunctionBox before we enter the syntax parser, because |pn|
        // still expects a FunctionBox to be attached to it during BCE, and
        // the syntax parser cannot attach one to it.
        FunctionBox* funbox = newFunctionBox(pn, fun, inheritedDirectives, generatorKind,
                                             tryAnnexB);
        if (!funbox)
            return false;
        funbox->initWithEnclosingParseContext(pc, kind);

        if (!parser->innerFunction(SyntaxParseHandler::NodeGeneric, pc, funbox, inHandling,
                                   kind, generatorKind, inheritedDirectives, newDirectives))
        {
            if (parser->hadAbortedSyntaxParse()) {
                // Try again with a full parse. UsedNameTracker needs to be
                // rewound to just before we tried the syntax parse for
                // correctness.
                parser->clearAbortedSyntaxParse();
                usedNames.rewind(token);
                MOZ_ASSERT_IF(parser->context->isJSContext(),
                              !parser->context->asJSContext()->isExceptionPending());
                break;
            }
            return false;
        }

        // Advance this parser over tokens processed by the syntax parser.
        parser->tokenStream.tell(&position);
        if (!tokenStream.seek(position, parser->tokenStream))
            return false;

        // Update the end position of the parse node.
        pn->pn_pos.end = tokenStream.currentToken().pos.end;
        return true;
    } while (false);

    // We failed to do a syntax parse above, so do the full parse.
    return innerFunction(pn, pc, fun, inHandling, kind, generatorKind, tryAnnexB,
                         inheritedDirectives, newDirectives);
}

template <>
bool
Parser<SyntaxParseHandler>::trySyntaxParseInnerFunction(Node pn, HandleFunction fun,
                                                        InHandling inHandling,
                                                        FunctionSyntaxKind kind,
                                                        GeneratorKind generatorKind,
                                                        bool tryAnnexB,
                                                        Directives inheritedDirectives,
                                                        Directives* newDirectives)
{
    // This is already a syntax parser, so just parse the inner function.
    return innerFunction(pn, pc, fun, inHandling, kind, generatorKind, tryAnnexB,
                         inheritedDirectives, newDirectives);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::innerFunction(Node pn, ParseContext* outerpc, FunctionBox* funbox,
                                    InHandling inHandling, FunctionSyntaxKind kind,
                                    GeneratorKind generatorKind, Directives inheritedDirectives,
                                    Directives* newDirectives)
{
    // Note that it is possible for outerpc != this->pc, as we may be
    // attempting to syntax parse an inner function from an outer full
    // parser. In that case, outerpc is a ParseContext from the full parser
    // instead of the current top of the stack of the syntax parser.

    // Push a new ParseContext.
    ParseContext funpc(this, funbox, newDirectives);
    if (!funpc.init())
        return false;

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    if (!functionFormalParametersAndBody(inHandling, yieldHandling, pn, kind))
        return false;

    return leaveInnerFunction(outerpc);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::innerFunction(Node pn, ParseContext* outerpc, HandleFunction fun,
                                    InHandling inHandling, FunctionSyntaxKind kind,
                                    GeneratorKind generatorKind, bool tryAnnexB,
                                    Directives inheritedDirectives, Directives* newDirectives)
{
    // Note that it is possible for outerpc != this->pc, as we may be
    // attempting to syntax parse an inner function from an outer full
    // parser. In that case, outerpc is a ParseContext from the full parser
    // instead of the current top of the stack of the syntax parser.

    FunctionBox* funbox = newFunctionBox(pn, fun, inheritedDirectives, generatorKind, tryAnnexB);
    if (!funbox)
        return false;
    funbox->initWithEnclosingParseContext(outerpc, kind);

    return innerFunction(pn, outerpc, funbox, inHandling, kind, generatorKind,
                         inheritedDirectives, newDirectives);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::appendToCallSiteObj(Node callSiteObj)
{
    Node cookedNode = noSubstitutionTemplate();
    if (!cookedNode)
        return false;

    JSAtom* atom = tokenStream.getRawTemplateStringAtom();
    if (!atom)
        return false;
    Node rawNode = handler.newTemplateStringLiteral(atom, pos());
    if (!rawNode)
        return false;

    handler.addToCallSiteObject(callSiteObj, rawNode, cookedNode);
    return true;
}

template <>
ParseNode*
Parser<FullParseHandler>::standaloneLazyFunction(HandleFunction fun, bool strict,
                                                 GeneratorKind generatorKind)
{
    MOZ_ASSERT(checkOptionsCalled);

    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();

    // Our tokenStream has no current token, so pn's position is garbage.
    // Substitute the position of the first token in our source.
    if (!tokenStream.peekTokenPos(&pn->pn_pos))
        return null();

    Directives directives(strict);
    FunctionBox* funbox = newFunctionBox(pn, fun, directives, generatorKind,
                                         /* tryAnnexB = */ false);
    if (!funbox)
        return null();
    funbox->initFromLazyFunction();

    Directives newDirectives = directives;
    ParseContext funpc(this, funbox, &newDirectives);
    if (!funpc.init())
        return null();

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    FunctionSyntaxKind syntaxKind = Statement;
    if (fun->isClassConstructor())
        syntaxKind = ClassConstructor;
    else if (fun->isMethod())
        syntaxKind = Method;
    else if (fun->isGetter())
        syntaxKind = Getter;
    else if (fun->isSetter())
        syntaxKind = Setter;

    if (!functionFormalParametersAndBody(InAllowed, yieldHandling, pn, syntaxKind)) {
        MOZ_ASSERT(directives == newDirectives);
        return null();
    }

    if (!FoldConstants(context, &pn, this))
        return null();

    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionFormalParametersAndBody(InHandling inHandling,
                                                      YieldHandling yieldHandling,
                                                      Node pn, FunctionSyntaxKind kind)
{
    // Given a properly initialized parse context, try to parse an actual
    // function without concern for conversion to strict mode, use of lazy
    // parsing and such.

    FunctionBox* funbox = pc->functionBox();
    RootedFunction fun(context, funbox->function());

    if (!functionArguments(yieldHandling, kind, pn))
        return false;

    Maybe<ParseContext::VarScope> varScope;
    if (funbox->hasParameterExprs) {
        varScope.emplace(this);
        if (!varScope->init(pc))
            return false;
    } else {
        pc->functionScope().useAsVarScope(pc);
    }

    if (kind == Arrow) {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_ARROW))
            return false;
        if (!matched) {
            report(ParseError, false, null(), JSMSG_BAD_ARROW_ARGS);
            return false;
        }
    }

    // Parse the function body.
    FunctionBodyType bodyType = StatementListBody;
    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return false;
    if (tt != TOK_LC) {
        if (funbox->isStarGenerator() || kind == Method ||
            kind == GetterNoExpressionClosure || kind == SetterNoExpressionClosure ||
            IsConstructorKind(kind)) {
            report(ParseError, false, null(), JSMSG_CURLY_BEFORE_BODY);
            return false;
        }

        if (kind != Arrow) {
#if JS_HAS_EXPR_CLOSURES
            addTelemetry(JSCompartment::DeprecatedExpressionClosure);
            if (!warnOnceAboutExprClosure())
                return false;
#else
            report(ParseError, false, null(), JSMSG_CURLY_BEFORE_BODY);
            return false;
#endif
        }

        tokenStream.ungetToken();
        bodyType = ExpressionBody;
#if JS_HAS_EXPR_CLOSURES
        fun->setIsExprBody();
#endif
    }

    Node body = functionBody(inHandling, yieldHandling, kind, bodyType);
    if (!body)
        return false;

    if ((kind != Method && !IsConstructorKind(kind)) && fun->name()) {
        RootedPropertyName propertyName(context, fun->name()->asPropertyName());
        if (!checkStrictBinding(propertyName, handler.getPosition(pn)))
            return false;
    }

    if (bodyType == StatementListBody) {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_RC, TokenStream::Operand))
            return false;
        if (!matched) {
            report(ParseError, false, null(), JSMSG_CURLY_AFTER_BODY);
            return false;
        }
        funbox->bufEnd = pos().begin + 1;
    } else {
#if !JS_HAS_EXPR_CLOSURES
        MOZ_ASSERT(kind == Arrow);
#endif
        if (tokenStream.hadError())
            return false;
        funbox->bufEnd = pos().end;
        if (kind == Statement && !MatchOrInsertSemicolonAfterExpression(tokenStream))
            return false;
    }

    if (IsMethodDefinitionKind(kind) && pc->superScopeNeedsHomeObject())
        funbox->setNeedsHomeObject();

    if (!finishFunction())
        return false;

    handler.setEndPosition(body, pos().begin);
    handler.setEndPosition(pn, pos().end);
    handler.setFunctionBody(pn, body);

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkYieldNameValidity()
{
    // In star generators and in JS >= 1.7, yield is a keyword.  Otherwise in
    // strict mode, yield is a future reserved word.
    if (pc->isStarGenerator() || versionNumber() >= JSVERSION_1_7 || pc->sc()->strict()) {
        report(ParseError, false, null(), JSMSG_RESERVED_ID, "yield");
        return false;
    }
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionStmt(YieldHandling yieldHandling, DefaultHandling defaultHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    // Annex B.3.4 says we can parse function declarations unbraced under if
    // or else as if it were braced. That is, |if (x) function f() {}| is
    // parsed as |if (x) { function f() {} }|.
    Maybe<ParseContext::Statement> synthesizedStmtForAnnexB;
    Maybe<ParseContext::Scope> synthesizedScopeForAnnexB;
    if (!pc->sc()->strict()) {
        ParseContext::Statement* stmt = pc->innermostStatement();
        if (stmt && stmt->kind() == StatementKind::If) {
            synthesizedStmtForAnnexB.emplace(pc, StatementKind::Block);
            synthesizedScopeForAnnexB.emplace(this);
            if (!synthesizedScopeForAnnexB->init(pc))
                return null();
        }
    }

    RootedPropertyName name(context);
    GeneratorKind generatorKind = NotGenerator;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    if (tt == TOK_MUL) {
        generatorKind = StarGenerator;
        if (!tokenStream.getToken(&tt))
            return null();
    }

    if (tt == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tt == TOK_YIELD) {
        if (!checkYieldNameValidity())
            return null();
        name = tokenStream.currentName();
    } else if (defaultHandling == AllowDefaultName) {
        name = context->names().starDefaultStar;
        tokenStream.ungetToken();
    } else {
        /* Unnamed function expressions are forbidden in statement context. */
        report(ParseError, false, null(), JSMSG_UNNAMED_FUNCTION_STMT);
        return null();
    }

    Node fun = functionDefinition(InAllowed, yieldHandling, name, Statement, generatorKind,
                                  PredictUninvoked);
    if (!fun)
        return null();

    if (synthesizedStmtForAnnexB) {
        Node synthesizedStmtList = handler.newStatementList(handler.getPosition(fun));
        if (!synthesizedStmtList)
            return null();
        handler.addStatementToList(synthesizedStmtList, fun);
        return finishLexicalScope(*synthesizedScopeForAnnexB, synthesizedStmtList);
    }

    return fun;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionExpr(InvokedPrediction invoked)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    GeneratorKind generatorKind = NotGenerator;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    if (tt == TOK_MUL) {
        generatorKind = StarGenerator;
        if (!tokenStream.getToken(&tt))
            return null();
    }

    RootedPropertyName name(context);
    if (tt == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tt == TOK_YIELD) {
        if (!checkYieldNameValidity())
            return null();
        name = tokenStream.currentName();
    } else {
        tokenStream.ungetToken();
    }

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    return functionDefinition(InAllowed, yieldHandling, name, Expression, generatorKind, invoked);
}

/*
 * Return true if this node, known to be an unparenthesized string literal,
 * could be the string of a directive in a Directive Prologue. Directive
 * strings never contain escape sequences or line continuations.
 * isEscapeFreeStringLiteral, below, checks whether the node itself could be
 * a directive.
 */
static inline bool
IsEscapeFreeStringLiteral(const TokenPos& pos, JSAtom* str)
{
    /*
     * If the string's length in the source code is its length as a value,
     * accounting for the quotes, then it must not contain any escape
     * sequences or line continuations.
     */
    return pos.begin + str->length() + 2 == pos.end;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkUnescapedName()
{
    if (!tokenStream.currentToken().nameContainsEscape())
        return true;

    report(ParseError, false, null(), JSMSG_ESCAPED_KEYWORD);
    return false;
}

template <>
bool
Parser<SyntaxParseHandler>::asmJS(Node list)
{
    // While asm.js could technically be validated and compiled during syntax
    // parsing, we have no guarantee that some later JS wouldn't abort the
    // syntax parse and cause us to re-parse (and re-compile) the asm.js module.
    // For simplicity, unconditionally abort the syntax parse when "use asm" is
    // encountered so that asm.js is always validated/compiled exactly once
    // during a full parse.
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template <>
bool
Parser<FullParseHandler>::asmJS(Node list)
{
    // Disable syntax parsing in anything nested inside the asm.js module.
    handler.disableSyntaxParser();

    // We should be encountering the "use asm" directive for the first time; if
    // the directive is already, we must have failed asm.js validation and we're
    // reparsing. In that case, don't try to validate again. A non-null
    // newDirectives means we're not in a normal function.
    if (!pc->newDirectives || pc->newDirectives->asmJS())
        return true;

    // If there is no ScriptSource, then we are doing a non-compiling parse and
    // so we shouldn't (and can't, without a ScriptSource) compile.
    if (ss == nullptr)
        return true;

    pc->functionBox()->useAsm = true;

    // Attempt to validate and compile this asm.js module. On success, the
    // tokenStream has been advanced to the closing }. On failure, the
    // tokenStream is in an indeterminate state and we must reparse the
    // function from the beginning. Reparsing is triggered by marking that a
    // new directive has been encountered and returning 'false'.
    bool validated;
    if (!CompileAsmJS(context, *this, list, &validated))
        return false;
    if (!validated) {
        pc->newDirectives->setAsmJS();
        return false;
    }

    return true;
}

/*
 * Recognize Directive Prologue members and directives. Assuming |pn| is a
 * candidate for membership in a directive prologue, recognize directives and
 * set |pc|'s flags accordingly. If |pn| is indeed part of a prologue, set its
 * |pn_prologue| flag.
 *
 * Note that the following is a strict mode function:
 *
 * function foo() {
 *   "blah" // inserted semi colon
 *        "blurgh"
 *   "use\x20loose"
 *   "use strict"
 * }
 *
 * That is, even though "use\x20loose" can never be a directive, now or in the
 * future (because of the hex escape), the Directive Prologue extends through it
 * to the "use strict" statement, which is indeed a directive.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::maybeParseDirective(Node list, Node pn, bool* cont)
{
    TokenPos directivePos;
    JSAtom* directive = handler.isStringExprStatement(pn, &directivePos);

    *cont = !!directive;
    if (!*cont)
        return true;

    if (IsEscapeFreeStringLiteral(directivePos, directive)) {
        // Mark this statement as being a possibly legitimate part of a
        // directive prologue, so the bytecode emitter won't warn about it being
        // useless code. (We mustn't just omit the statement entirely yet, as it
        // could be producing the value of an eval or JSScript execution.)
        //
        // Note that even if the string isn't one we recognize as a directive,
        // the emitter still shouldn't flag it as useless, as it could become a
        // directive in the future. We don't want to interfere with people
        // taking advantage of directive-prologue-enabled features that appear
        // in other browsers first.
        handler.setPrologue(pn);

        if (directive == context->names().useStrict) {
            // We're going to be in strict mode. Note that this scope explicitly
            // had "use strict";
            pc->sc()->setExplicitUseStrict();
            if (!pc->sc()->strict()) {
                if (pc->isFunctionBox()) {
                    // Request that this function be reparsed as strict.
                    pc->newDirectives->setStrict();
                    return false;
                }
                // We don't reparse global scopes, so we keep track of the one
                // possible strict violation that could occur in the directive
                // prologue -- octal escapes -- and complain now.
                if (tokenStream.sawOctalEscape()) {
                    report(ParseError, false, null(), JSMSG_DEPRECATED_OCTAL);
                    return false;
                }
                pc->sc()->strictScript = true;
            }
        } else if (directive == context->names().useAsm) {
            if (pc->isFunctionBox())
                return asmJS(list);
            return report(ParseWarning, false, pn, JSMSG_USE_ASM_DIRECTIVE_FAIL);
        }
    }
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statementList(YieldHandling yieldHandling)
{
    JS_CHECK_RECURSION(context, return null());

    Node pn = handler.newStatementList(pos());
    if (!pn)
        return null();

    bool canHaveDirectives = pc->atBodyLevel();
    bool afterReturn = false;
    bool warnedAboutStatementsAfterReturn = false;
    uint32_t statementBegin = 0;
    for (;;) {
        TokenKind tt = TOK_EOF;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand)) {
            if (tokenStream.isEOF())
                isUnexpectedEOF_ = true;
            return null();
        }
        if (tt == TOK_EOF || tt == TOK_RC)
            break;
        if (afterReturn) {
            TokenPos pos(0, 0);
            if (!tokenStream.peekTokenPos(&pos, TokenStream::Operand))
                return null();
            statementBegin = pos.begin;
        }
        Node next = statement(yieldHandling, canHaveDirectives);
        if (!next) {
            if (tokenStream.isEOF())
                isUnexpectedEOF_ = true;
            return null();
        }
        if (!warnedAboutStatementsAfterReturn) {
            if (afterReturn) {
                if (!handler.isStatementPermittedAfterReturnStatement(next)) {
                    if (!reportWithOffset(ParseWarning, false, statementBegin,
                                          JSMSG_STMT_AFTER_RETURN))
                    {
                        return null();
                    }
                    warnedAboutStatementsAfterReturn = true;
                }
            } else if (handler.isReturnStatement(next)) {
                afterReturn = true;
            }
        }

        if (canHaveDirectives) {
            if (!maybeParseDirective(pn, next, &canHaveDirectives))
                return null();
        }

        handler.addStatementToList(pn, next);
    }

    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::condition(InHandling inHandling, YieldHandling yieldHandling)
{
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    Node pn = exprInParens(inHandling, yieldHandling, TripledotProhibited);
    if (!pn)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isUnparenthesizedAssignment(pn)) {
        if (!report(ParseExtraWarning, false, null(), JSMSG_EQUAL_AS_ASSIGN))
            return null();
    }
    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchLabel(YieldHandling yieldHandling, MutableHandle<PropertyName*> label)
{
    TokenKind tt = TOK_EOF;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return false;

    if (tt == TOK_NAME) {
        tokenStream.consumeKnownToken(TOK_NAME, TokenStream::Operand);
        MOZ_ASSERT_IF(tokenStream.currentName() == context->names().yield,
                      yieldHandling == YieldIsName);
        label.set(tokenStream.currentName());
    } else if (tt == TOK_YIELD) {
        // We might still consider |yield| to be valid here, contrary to ES6.
        // Fix bug 1104014, then stop shipping legacy generators in chrome
        // code, then remove this check!
        tokenStream.consumeKnownToken(TOK_YIELD, TokenStream::Operand);
        if (!checkYieldNameValidity())
            return false;
        label.set(tokenStream.currentName());
    } else {
        label.set(nullptr);
    }
    return true;
}

template <typename ParseHandler>
Parser<ParseHandler>::PossibleError::PossibleError(Parser<ParseHandler>& parser)
                                                   : parser_(parser)
{
    state_ = ErrorState::None;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::setPending(ParseReportKind kind, unsigned errorNumber,
                                                bool strict)
{
    if (hasError())
        return false;

    // If we report an error later, we'll do it from the position where we set
    // the state to pending.
    offset_      = parser_.pos().begin;
    reportKind_  = kind;
    strict_      = strict;
    errorNumber_ = errorNumber;
    state_       = ErrorState::Pending;

    return true;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::setResolved()
{
    state_ = ErrorState::None;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::hasError()
{
    return state_ == ErrorState::Pending;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::checkForExprErrors()
{
    bool err = hasError();
    if (err)
        parser_.reportWithOffset(reportKind_, strict_, offset_, errorNumber_);
    return !err;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::transferErrorTo(PossibleError* other)
{
    if (other) {
        MOZ_ASSERT(this != other);
        MOZ_ASSERT(!other->hasError());

        // We should never allow fields to be copied between instances
        // that point to different underlying parsers.
        MOZ_ASSERT(&parser_ == &other->parser_);
        other->offset_        = offset_;
        other->reportKind_    = reportKind_;
        other->errorNumber_   = errorNumber_;
        other->strict_        = strict_;
        other->state_         = state_;
    }
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::makeSetCall(Node target, unsigned msg)
{
    MOZ_ASSERT(handler.isFunctionCall(target));

    // Assignment to function calls is forbidden in ES6.  We're still somewhat
    // concerned about sites using this in dead code, so forbid it only in
    // strict mode code (or if the werror option has been set), and otherwise
    // warn.
    if (!report(ParseStrictError, pc->sc()->strict(), target, msg))
        return false;

    handler.markAsSetCall(target);
    return true;
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringName(ParseNode* expr, Maybe<DeclarationKind> maybeDecl)
{
    MOZ_ASSERT(!handler.isUnparenthesizedDestructuringPattern(expr));

    // Parentheses are forbidden around destructuring *patterns* (but allowed
    // around names).  Use our nicer error message for parenthesized, nested
    // patterns.
    if (handler.isParenthesizedDestructuringPattern(expr)) {
        report(ParseError, false, expr, JSMSG_BAD_DESTRUCT_PARENS);
        return false;
    }

    // This expression might be in a variable-binding pattern where only plain,
    // unparenthesized names are permitted.
    if (maybeDecl) {
        // Destructuring patterns in declarations must only contain
        // unparenthesized names.
        if (!handler.isUnparenthesizedName(expr)) {
            report(ParseError, false, expr, JSMSG_NO_VARIABLE_NAME);
            return false;
        }

        RootedPropertyName name(context, expr->name());
        return noteDeclaredName(name, *maybeDecl, handler.getPosition(expr));
    }

    // Otherwise this is an expression in destructuring outside a declaration.
    if (!reportIfNotValidSimpleAssignmentTarget(expr, KeyedDestructuringAssignment))
        return false;

    MOZ_ASSERT(!handler.isFunctionCall(expr),
               "function calls shouldn't be considered valid targets in "
               "destructuring patterns");

    if (handler.isNameAnyParentheses(expr)) {
        // The arguments/eval identifiers are simple in non-strict mode code.
        // Warn to discourage their use nonetheless.
        return reportIfArgumentsEvalTarget(expr);
    }

    // Nothing further to do for property accesses.
    MOZ_ASSERT(handler.isPropertyAccess(expr));
    return true;
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringPattern(ParseNode* pattern,
                                                    Maybe<DeclarationKind> maybeDecl);

template <>
bool
Parser<FullParseHandler>::checkDestructuringObject(ParseNode* objectPattern,
                                                   Maybe<DeclarationKind> maybeDecl)
{
    MOZ_ASSERT(objectPattern->isKind(PNK_OBJECT));

    for (ParseNode* member = objectPattern->pn_head; member; member = member->pn_next) {
        ParseNode* target;
        if (member->isKind(PNK_MUTATEPROTO)) {
            target = member->pn_kid;
        } else {
            MOZ_ASSERT(member->isKind(PNK_COLON) || member->isKind(PNK_SHORTHAND));
            MOZ_ASSERT_IF(member->isKind(PNK_SHORTHAND),
                          member->pn_left->isKind(PNK_OBJECT_PROPERTY_NAME) &&
                          member->pn_right->isKind(PNK_NAME) &&
                          member->pn_left->pn_atom == member->pn_right->pn_atom);

            target = member->pn_right;
        }
        if (handler.isUnparenthesizedAssignment(target))
            target = target->pn_left;

        if (handler.isUnparenthesizedDestructuringPattern(target)) {
            if (!checkDestructuringPattern(target, maybeDecl))
                return false;
        } else {
            if (!checkDestructuringName(target, maybeDecl))
                return false;
        }
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringArray(ParseNode* arrayPattern,
                                                  Maybe<DeclarationKind> maybeDecl)
{
    MOZ_ASSERT(arrayPattern->isKind(PNK_ARRAY));

    for (ParseNode* element = arrayPattern->pn_head; element; element = element->pn_next) {
        if (element->isKind(PNK_ELISION))
            continue;

        ParseNode* target;
        if (element->isKind(PNK_SPREAD)) {
            if (element->pn_next) {
                report(ParseError, false, element->pn_next, JSMSG_PARAMETER_AFTER_REST);
                return false;
            }
            target = element->pn_kid;
        } else if (handler.isUnparenthesizedAssignment(element)) {
            target = element->pn_left;
        } else {
            target = element;
        }

        if (handler.isUnparenthesizedDestructuringPattern(target)) {
            if (!checkDestructuringPattern(target, maybeDecl))
                return false;
        } else {
            if (!checkDestructuringName(target, maybeDecl))
                return false;
        }
    }

    return true;
}

/*
 * Destructuring patterns can appear in two kinds of contexts:
 *
 * - assignment-like: assignment expressions and |for| loop heads.  In
 *   these cases, the patterns' property value positions can be
 *   arbitrary lvalue expressions; the destructuring is just a fancy
 *   assignment.
 *
 * - binding-like: |var| and |let| declarations, functions' formal
 *   parameter lists, |catch| clauses, and comprehension tails.  In
 *   these cases, the patterns' property value positions must be
 *   simple names; the destructuring defines them as new variables.
 *
 * In both cases, other code parses the pattern as an arbitrary
 * primaryExpr, and then, here in checkDestructuringPattern, verify
 * that the tree is a valid AssignmentPattern or BindingPattern.
 *
 * In assignment-like contexts, we parse the pattern with
 * pc->inDestructuringDecl clear, so the lvalue expressions in the
 * pattern are parsed normally.  primaryExpr links variable references
 * into the appropriate use chains; creates placeholder definitions;
 * and so on.  checkDestructuringPattern is called with |data| nullptr
 * (since we won't be binding any new names), and we specialize lvalues
 * as appropriate.
 *
 * In declaration-like contexts, the normal variable reference
 * processing would just be an obstruction, because we're going to
 * define the names that appear in the property value positions as new
 * variables anyway.  In this case, we parse the pattern with
 * pc->inDestructuringDecl set, which directs primaryExpr to leave
 * whatever name nodes it creates unconnected.  Then, here in
 * checkDestructuringPattern, we require the pattern's property value
 * positions to be simple names, and define them as appropriate to the
 * context.  For these calls, |data| points to the right sort of
 * BindData.
 */
template <>
bool
Parser<FullParseHandler>::checkDestructuringPattern(ParseNode* pattern,
                                                    Maybe<DeclarationKind> maybeDecl)
{
    if (pattern->isKind(PNK_ARRAYCOMP)) {
        report(ParseError, false, pattern, JSMSG_ARRAY_COMP_LEFTSIDE);
        return false;
    }

    if (pattern->isKind(PNK_ARRAY))
        return checkDestructuringArray(pattern, maybeDecl);
    return checkDestructuringObject(pattern, maybeDecl);
}

template <>
bool
Parser<SyntaxParseHandler>::checkDestructuringPattern(Node pattern,
                                                      Maybe<DeclarationKind> maybeDecl)
{
    return abortIfSyntaxParser();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringDeclaration(DeclarationKind kind, YieldHandling yieldHandling,
                                               TokenKind tt)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));

    pc->inDestructuringDecl = Some(kind);
    PossibleError possibleError(*this);
    Node pn = primaryExpr(yieldHandling, TripledotProhibited,
                          &possibleError, tt);

    // Resolve asap instead of checking since we already know that we are
    // destructuring.
    possibleError.setResolved();
    pc->inDestructuringDecl = Nothing();
    if (!pn)
        return null();
    if (!checkDestructuringPattern(pn, Some(kind)))
        return null();
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringDeclarationWithoutYield(DeclarationKind kind,
                                                           YieldHandling yieldHandling,
                                                           TokenKind tt, unsigned msg)
{
    uint32_t startYieldOffset = pc->lastYieldOffset;
    Node res = destructuringDeclaration(kind, yieldHandling, tt);
    if (res && pc->lastYieldOffset != startYieldOffset) {
        reportWithOffset(ParseError, false, pc->lastYieldOffset,
                         msg, js_yield_str);
        return null();
    }
    return res;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::blockStatement(YieldHandling yieldHandling, unsigned errorNumber)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    ParseContext::Statement stmt(pc, StatementKind::Block);
    ParseContext::Scope scope(this);
    if (!scope.init(pc))
        return null();

    Node list = statementList(yieldHandling);
    if (!list)
        return null();

    MUST_MATCH_TOKEN_MOD(TOK_RC, TokenStream::Operand, errorNumber);

    return finishLexicalScope(scope, list);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expressionAfterForInOrOf(ParseNodeKind forHeadKind,
                                               YieldHandling yieldHandling)
{
    MOZ_ASSERT(forHeadKind == PNK_FORIN || forHeadKind == PNK_FOROF);
    Node pn = forHeadKind == PNK_FOROF
           ? assignExpr(InAllowed, yieldHandling, TripledotProhibited)
           : expr(InAllowed, yieldHandling, TripledotProhibited);
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::declarationPattern(Node decl, DeclarationKind declKind, TokenKind tt,
                                         bool initialDeclaration, YieldHandling yieldHandling,
                                         ParseNodeKind* forHeadKind, Node* forInOrOfExpression)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LB) ||
               tokenStream.isCurrentTokenType(TOK_LC));

    Node pattern;
    {
        pc->inDestructuringDecl = Some(declKind);

        PossibleError possibleError(*this);
        pattern = primaryExpr(yieldHandling, TripledotProhibited,
                              &possibleError, tt);

        // Resolve asap instead of checking since we already know that we are
        // destructuring.
        possibleError.setResolved();
        pc->inDestructuringDecl = Nothing();
    }
    if (!pattern)
        return null();

    if (initialDeclaration && forHeadKind) {
        bool isForIn, isForOf;
        if (!matchInOrOf(&isForIn, &isForOf))
            return null();

        if (isForIn) {
            *forHeadKind = PNK_FORIN;
        } else if (isForOf) {
            *forHeadKind = PNK_FOROF;

            // Annex B.3.5 has different early errors for vars in for-of loops.
            if (declKind == DeclarationKind::Var)
                declKind = DeclarationKind::ForOfVar;
        } else {
            *forHeadKind = PNK_FORHEAD;
        }

        if (*forHeadKind != PNK_FORHEAD) {
            if (!checkDestructuringPattern(pattern, Some(declKind)))
                return null();

            *forInOrOfExpression = expressionAfterForInOrOf(*forHeadKind, yieldHandling);
            if (!*forInOrOfExpression)
                return null();

            return pattern;
        }
    }

    if (!checkDestructuringPattern(pattern, Some(declKind)))
        return null();

    TokenKind token;
    if (!tokenStream.getToken(&token, TokenStream::None))
        return null();

    if (token != TOK_ASSIGN) {
        report(ParseError, false, null(), JSMSG_BAD_DESTRUCT_DECL);
        return null();
    }

    Node init = assignExpr(forHeadKind ? InProhibited : InAllowed,
                           yieldHandling, TripledotProhibited);
    if (!init)
        return null();

    if (forHeadKind) {
        // For for(;;) declarations, consistency with |for (;| parsing requires
        // that the ';' first be examined as Operand, even though absence of a
        // binary operator (examined with modifier None) terminated |init|.
        // For all other declarations, through ASI's infinite majesty, a next
        // token on a new line would begin an expression.
        tokenStream.addModifierException(TokenStream::OperandIsNone);
    }

    return handler.newBinary(PNK_ASSIGN, pattern, init);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::initializerInNameDeclaration(Node decl, Node binding,
                                                   Handle<PropertyName*> name,
                                                   DeclarationKind declKind,
                                                   bool initialDeclaration,
                                                   YieldHandling yieldHandling,
                                                   ParseNodeKind* forHeadKind,
                                                   Node* forInOrOfExpression)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_ASSIGN));

    Node initializer = assignExpr(forHeadKind ? InProhibited : InAllowed,
                                  yieldHandling, TripledotProhibited);
    if (!initializer)
        return false;

    if (forHeadKind) {
        if (initialDeclaration) {
            bool isForIn, isForOf;
            if (!matchInOrOf(&isForIn, &isForOf))
                return false;

            // An initialized declaration can't appear in a for-of:
            //
            //   for (var/let/const x = ... of ...); // BAD
            if (isForOf) {
                report(ParseError, false, binding, JSMSG_BAD_FOR_LEFTSIDE);
                return false;
            }

            if (isForIn) {
                // Lexical declarations in for-in loops can't be initialized:
                //
                //   for (let/const x = ... in ...); // BAD
                if (DeclarationKindIsLexical(declKind)) {
                    report(ParseError, false, binding, JSMSG_BAD_FOR_LEFTSIDE);
                    return false;
                }

                // This leaves only initialized for-in |var| declarations.  ES6
                // forbids these, yet they sadly still occur, rarely, on the
                // web.  *Don't* assign, and warn about this invalid syntax to
                // incrementally move to ES6 semantics.
                *forHeadKind = PNK_FORIN;
                if (!report(ParseWarning, pc->sc()->strict(), initializer,
                            JSMSG_INVALID_FOR_IN_DECL_WITH_INIT))
                {
                    return false;
                }

                *forInOrOfExpression = expressionAfterForInOrOf(PNK_FORIN, yieldHandling);
                return *forInOrOfExpression != null();
            }

            *forHeadKind = PNK_FORHEAD;
        } else {
            MOZ_ASSERT(*forHeadKind == PNK_FORHEAD);
        }

        // Per Parser::forHeadStart, the semicolon in |for (;| is ultimately
        // gotten as Operand.  But initializer expressions terminate with the
        // absence of an operator gotten as None, so we need an exception.
        tokenStream.addModifierException(TokenStream::OperandIsNone);
    }

    return handler.finishInitializerAssignment(binding, initializer);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::declarationName(Node decl, DeclarationKind declKind, TokenKind tt,
                                      bool initialDeclaration, YieldHandling yieldHandling,
                                      ParseNodeKind* forHeadKind, Node* forInOrOfExpression)
{
    if (tt != TOK_NAME) {
        // Anything other than TOK_YIELD or TOK_NAME is an error.
        if (tt != TOK_YIELD) {
            report(ParseError, false, null(), JSMSG_NO_VARIABLE_NAME);
            return null();
        }

        // TOK_YIELD is only okay if it's treated as a name.
        if (!checkYieldNameValidity())
            return null();
    }

    RootedPropertyName name(context, tokenStream.currentName());
    Node binding = newName(name);
    if (!binding)
        return null();

    TokenPos namePos = pos();

    // The '=' context after a variable name in a declaration is an opportunity
    // for ASI, and thus for the next token to start an ExpressionStatement:
    //
    //  var foo   // VariableDeclaration
    //  /bar/g;   // ExpressionStatement
    //
    // Therefore get the token here as Operand.
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_ASSIGN, TokenStream::Operand))
        return null();

    if (matched) {
        if (!initializerInNameDeclaration(decl, binding, name, declKind, initialDeclaration,
                                          yieldHandling, forHeadKind, forInOrOfExpression))
        {
            return null();
        }
    } else {
        tokenStream.addModifierException(TokenStream::NoneIsOperand);

        if (initialDeclaration && forHeadKind) {
            bool isForIn, isForOf;
            if (!matchInOrOf(&isForIn, &isForOf))
                return null();

            if (isForIn) {
                *forHeadKind = PNK_FORIN;
            } else if (isForOf) {
                *forHeadKind = PNK_FOROF;

                // Annex B.3.5 has different early errors for vars in for-of loops.
                if (declKind == DeclarationKind::Var)
                    declKind = DeclarationKind::ForOfVar;
            } else {
                *forHeadKind = PNK_FORHEAD;
            }
        }

        if (forHeadKind && *forHeadKind != PNK_FORHEAD) {
            *forInOrOfExpression = expressionAfterForInOrOf(*forHeadKind, yieldHandling);
            if (!*forInOrOfExpression)
                return null();
        } else {
            // Normal const declarations, and const declarations in for(;;)
            // heads, must be initialized.
            if (declKind == DeclarationKind::Const) {
                report(ParseError, false, binding, JSMSG_BAD_CONST_DECL);
                return null();
            }
        }
    }

    // Note the declared name after knowing whether or not we are in a for-of
    // loop, due to special early error semantics in Annex B.3.5.
    if (!noteDeclaredName(name, declKind, namePos))
        return null();

    return binding;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::declarationList(YieldHandling yieldHandling,
                                      ParseNodeKind kind,
                                      ParseNodeKind* forHeadKind /* = nullptr */,
                                      Node* forInOrOfExpression /* = nullptr */)
{
    MOZ_ASSERT(kind == PNK_VAR || kind == PNK_LET || kind == PNK_CONST);

    JSOp op;
    DeclarationKind declKind;
    switch (kind) {
      case PNK_VAR:
        op = JSOP_DEFVAR;
        declKind = DeclarationKind::Var;
        break;
      case PNK_CONST:
        op = JSOP_DEFCONST;
        declKind = DeclarationKind::Const;
        break;
      case PNK_LET:
        op = JSOP_DEFLET;
        declKind = DeclarationKind::Let;
        break;
      default:
        MOZ_CRASH("Unknown declaration kind");
    }

    Node decl = handler.newDeclarationList(kind, op);
    if (!decl)
        return null();

    bool matched;
    bool initialDeclaration = true;
    do {
        MOZ_ASSERT_IF(!initialDeclaration && forHeadKind,
                      *forHeadKind == PNK_FORHEAD);

        TokenKind tt;
        if (!tokenStream.getToken(&tt))
            return null();

        Node binding = (tt == TOK_LB || tt == TOK_LC)
                       ? declarationPattern(decl, declKind, tt, initialDeclaration, yieldHandling,
                                            forHeadKind, forInOrOfExpression)
                       : declarationName(decl, declKind, tt, initialDeclaration, yieldHandling,
                                         forHeadKind, forInOrOfExpression);
        if (!binding)
            return null();

        handler.addList(decl, binding);

        if (forHeadKind && *forHeadKind != PNK_FORHEAD)
            break;

        initialDeclaration = false;

        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return null();
    } while (matched);

    return decl;
}

template <>
ParseNode*
Parser<FullParseHandler>::lexicalDeclaration(YieldHandling yieldHandling, bool isConst)
{
    handler.disableSyntaxParser();

    /*
     * Parse body-level lets without a new block object. ES6 specs
     * that an execution environment's initial lexical environment
     * is the VariableEnvironment, i.e., body-level lets are in
     * the same environment record as vars.
     *
     * However, they cannot be parsed exactly as vars, as ES6
     * requires that uninitialized lets throw ReferenceError on use.
     *
     * See 8.1.1.1.6 and the note in 13.2.1.
     */
    ParseNode* decl = declarationList(yieldHandling, isConst ? PNK_CONST : PNK_LET);
    if (!decl || !MatchOrInsertSemicolonAfterExpression(tokenStream))
        return null();

    return decl;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::lexicalDeclaration(YieldHandling, bool)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <>
bool
Parser<FullParseHandler>::namedImportsOrNamespaceImport(TokenKind tt, Node importSpecSet)
{
    if (tt == TOK_LC) {
        TokenStream::Modifier modifier = TokenStream::KeywordIsName;
        while (true) {
            // Handle the forms |import {} from 'a'| and
            // |import { ..., } from 'a'| (where ... is non empty), by
            // escaping the loop early if the next token is }.
            if (!tokenStream.peekToken(&tt, TokenStream::KeywordIsName))
                return false;

            if (tt == TOK_RC)
                break;

            // If the next token is a keyword, the previous call to
            // peekToken matched it as a TOK_NAME, and put it in the
            // lookahead buffer, so this call will match keywords as well.
            MUST_MATCH_TOKEN_MOD(TOK_NAME, TokenStream::KeywordIsName, JSMSG_NO_IMPORT_NAME);
            Node importName = newName(tokenStream.currentName());
            if (!importName)
                return false;

            bool foundAs;
            if (!tokenStream.matchContextualKeyword(&foundAs, context->names().as))
                return false;

            if (foundAs) {
                MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_BINDING_NAME);
            } else {
                // Keywords cannot be bound to themselves, so an import name
                // that is a keyword is a syntax error if it is not followed
                // by the keyword 'as'.
                // See the ImportSpecifier production in ES6 section 15.2.2.
                if (IsKeyword(importName->name())) {
                    JSAutoByteString bytes;
                    if (!AtomToPrintableString(context, importName->name(), &bytes))
                        return false;
                    report(ParseError, false, null(), JSMSG_AS_AFTER_RESERVED_WORD, bytes.ptr());
                    return false;
                }
            }

            RootedPropertyName bindingAtom(context, tokenStream.currentName());
            Node bindingName = newName(bindingAtom);
            if (!bindingName)
                return false;
            if (!noteDeclaredName(bindingAtom, DeclarationKind::Import, pos()))
                return false;

            Node importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingName);
            if (!importSpec)
                return false;

            handler.addList(importSpecSet, importSpec);

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_COMMA))
                return false;

            if (!matched) {
                modifier = TokenStream::None;
                break;
            }
        }

        MUST_MATCH_TOKEN_MOD(TOK_RC, modifier, JSMSG_RC_AFTER_IMPORT_SPEC_LIST);
    } else {
        MOZ_ASSERT(tt == TOK_MUL);
        if (!tokenStream.getToken(&tt))
            return false;

        if (tt != TOK_NAME || tokenStream.currentName() != context->names().as) {
            report(ParseError, false, null(), JSMSG_AS_AFTER_IMPORT_STAR);
            return false;
        }

        if (!checkUnescapedName())
            return false;

        MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_BINDING_NAME);

        Node importName = newName(context->names().star);
        if (!importName)
            return false;

        // Namespace imports are are not indirect bindings but lexical
        // definitions that hold a module namespace object. They are treated
        // as const variables which are initialized during the
        // ModuleDeclarationInstantiation step.
        RootedPropertyName bindingName(context, tokenStream.currentName());
        Node bindingNameNode = newName(bindingName);
        if (!bindingNameNode)
            return false;
        if (!noteDeclaredName(bindingName, DeclarationKind::Const, pos()))
            return false;

        // The namespace import name is currently required to live on the
        // environment.
        pc->varScope().lookupDeclaredName(bindingName)->value()->setClosedOver();

        Node importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingNameNode);
        if (!importSpec)
            return false;

        handler.addList(importSpecSet, importSpec);
    }

    return true;
}

template<>
bool
Parser<SyntaxParseHandler>::namedImportsOrNamespaceImport(TokenKind tt, Node importSpecSet)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
ParseNode*
Parser<FullParseHandler>::importDeclaration()
{
    MOZ_ASSERT(tokenStream.currentToken().type == TOK_IMPORT);

    if (!pc->atModuleLevel()) {
        report(ParseError, false, null(), JSMSG_IMPORT_DECL_AT_TOP_LEVEL);
        return null();
    }

    uint32_t begin = pos().begin;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    Node importSpecSet = handler.newList(PNK_IMPORT_SPEC_LIST);
    if (!importSpecSet)
        return null();

    if (tt == TOK_NAME || tt == TOK_LC || tt == TOK_MUL) {
        if (tt == TOK_NAME) {
            // Handle the form |import a from 'b'|, by adding a single import
            // specifier to the list, with 'default' as the import name and
            // 'a' as the binding name. This is equivalent to
            // |import { default as a } from 'b'|.
            Node importName = newName(context->names().default_);
            if (!importName)
                return null();

            RootedPropertyName bindingAtom(context, tokenStream.currentName());
            Node bindingName = newName(bindingAtom);
            if (!bindingName)
                return null();
            if (!noteDeclaredName(bindingAtom, DeclarationKind::Import, pos()))
                return null();

            Node importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingName);
            if (!importSpec)
                return null();

            handler.addList(importSpecSet, importSpec);

            if (!tokenStream.peekToken(&tt))
                return null();

            if (tt == TOK_COMMA) {
                if (!tokenStream.getToken(&tt) || !tokenStream.getToken(&tt))
                    return null();

                if (tt != TOK_LC && tt != TOK_MUL) {
                    report(ParseError, false, null(), JSMSG_NAMED_IMPORTS_OR_NAMESPACE_IMPORT);
                    return null();
                }

                if (!namedImportsOrNamespaceImport(tt, importSpecSet))
                    return null();
            }
        } else {
            if (!namedImportsOrNamespaceImport(tt, importSpecSet))
                return null();
        }

        if (!tokenStream.getToken(&tt))
            return null();

        if (tt != TOK_NAME || tokenStream.currentName() != context->names().from) {
            report(ParseError, false, null(), JSMSG_FROM_AFTER_IMPORT_CLAUSE);
            return null();
        }

        if (!checkUnescapedName())
            return null();

        MUST_MATCH_TOKEN(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM);
    } else if (tt == TOK_STRING) {
        // Handle the form |import 'a'| by leaving the list empty. This is
        // equivalent to |import {} from 'a'|.
        importSpecSet->pn_pos.end = importSpecSet->pn_pos.begin;
    } else {
        report(ParseError, false, null(), JSMSG_DECLARATION_AFTER_IMPORT);
        return null();
    }

    Node moduleSpec = stringLiteral();
    if (!moduleSpec)
        return null();

    if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
        return null();

    ParseNode* node =
        handler.newImportDeclaration(importSpecSet, moduleSpec, TokenPos(begin, pos().end));
    if (!node || !pc->sc()->asModuleContext()->builder.processImport(node))
        return null();

    return node;
}

template<>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::importDeclaration()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <>
ParseNode*
Parser<FullParseHandler>::classDefinition(YieldHandling yieldHandling,
                                          ClassContext classContext,
                                          DefaultHandling defaultHandling);

template<>
bool
Parser<FullParseHandler>::checkExportedName(JSAtom* exportName)
{
    if (!pc->sc()->asModuleContext()->builder.hasExportedName(exportName))
        return true;

    JSAutoByteString str;
    if (!AtomToPrintableString(context, exportName, &str))
        return false;

    report(ParseError, false, null(), JSMSG_DUPLICATE_EXPORT_NAME, str.ptr());
    return false;
}

template<>
bool
Parser<SyntaxParseHandler>::checkExportedName(JSAtom* exportName)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::checkExportedNamesForDeclaration(ParseNode* node)
{
    MOZ_ASSERT(node->isArity(PN_LIST));
    for (ParseNode* binding = node->pn_head; binding; binding = binding->pn_next) {
        if (binding->isKind(PNK_ASSIGN))
            binding = binding->pn_left;
        MOZ_ASSERT(binding->isKind(PNK_NAME));
        if (!checkExportedName(binding->pn_atom))
            return false;
    }

    return true;
}

template<>
bool
Parser<SyntaxParseHandler>::checkExportedNamesForDeclaration(Node node)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
ParseNode*
Parser<FullParseHandler>::exportDeclaration()
{
    MOZ_ASSERT(tokenStream.currentToken().type == TOK_EXPORT);

    if (!pc->atModuleLevel()) {
        report(ParseError, false, null(), JSMSG_EXPORT_DECL_AT_TOP_LEVEL);
        return null();
    }

    uint32_t begin = pos().begin;

    Node kid;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();
    switch (tt) {
      case TOK_LC: {
        kid = handler.newList(PNK_EXPORT_SPEC_LIST);
        if (!kid)
            return null();

        while (true) {
            // Handle the forms |export {}| and |export { ..., }| (where ...
            // is non empty), by escaping the loop early if the next token
            // is }.
            if (!tokenStream.peekToken(&tt))
                return null();
            if (tt == TOK_RC)
                break;

            MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_BINDING_NAME);
            Node bindingName = newName(tokenStream.currentName());
            if (!bindingName)
                return null();

            bool foundAs;
            if (!tokenStream.matchContextualKeyword(&foundAs, context->names().as))
                return null();
            if (foundAs) {
                if (!tokenStream.getToken(&tt, TokenStream::KeywordIsName))
                    return null();
                if (tt != TOK_NAME) {
                    report(ParseError, false, null(), JSMSG_NO_EXPORT_NAME);
                    return null();
                }
            }

            Node exportName = newName(tokenStream.currentName());
            if (!exportName)
                return null();

            if (!checkExportedName(exportName->pn_atom))
                return null();

            Node exportSpec = handler.newBinary(PNK_EXPORT_SPEC, bindingName, exportName);
            if (!exportSpec)
                return null();

            handler.addList(kid, exportSpec);

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_COMMA))
                return null();
            if (!matched)
                break;
        }

        MUST_MATCH_TOKEN(TOK_RC, JSMSG_RC_AFTER_EXPORT_SPEC_LIST);

        // Careful!  If |from| follows, even on a new line, it must start a
        // FromClause:
        //
        //   export { x }
        //   from "foo"; // a single ExportDeclaration
        //
        // But if it doesn't, we might have an ASI opportunity in Operand
        // context, so simply matching a contextual keyword won't work:
        //
        //   export { x }   // ExportDeclaration, terminated by ASI
        //   fro\u006D      // ExpressionStatement, the name "from"
        //
        // In that case let MatchOrInsertSemicolonAfterNonExpression sort out
        // ASI or any necessary error.
        TokenKind tt;
        if (!tokenStream.getToken(&tt, TokenStream::Operand))
            return null();

        if (tt == TOK_NAME &&
            tokenStream.currentToken().name() == context->names().from &&
            !tokenStream.currentToken().nameContainsEscape())
        {
            MUST_MATCH_TOKEN(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM);

            Node moduleSpec = stringLiteral();
            if (!moduleSpec)
                return null();

            if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
                return null();

            ParseNode* node = handler.newExportFromDeclaration(begin, kid, moduleSpec);
            if (!node || !pc->sc()->asModuleContext()->builder.processExportFrom(node))
                return null();

            return node;
        }

        tokenStream.ungetToken();

        if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
            return null();
        break;
      }

      case TOK_MUL: {
        kid = handler.newList(PNK_EXPORT_SPEC_LIST);
        if (!kid)
            return null();

        // Handle the form |export *| by adding a special export batch
        // specifier to the list.
        Node exportSpec = handler.newNullary(PNK_EXPORT_BATCH_SPEC, JSOP_NOP, pos());
        if (!exportSpec)
            return null();

        handler.addList(kid, exportSpec);

        if (!tokenStream.getToken(&tt))
            return null();
        if (tt != TOK_NAME || tokenStream.currentName() != context->names().from) {
            report(ParseError, false, null(), JSMSG_FROM_AFTER_EXPORT_STAR);
            return null();
        }

        if (!checkUnescapedName())
            return null();

        MUST_MATCH_TOKEN(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM);

        Node moduleSpec = stringLiteral();
        if (!moduleSpec)
            return null();

        if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
            return null();

        ParseNode* node = handler.newExportFromDeclaration(begin, kid, moduleSpec);
        if (!node || !pc->sc()->asModuleContext()->builder.processExportFrom(node))
            return null();

        return node;

      }

      case TOK_FUNCTION:
        kid = functionStmt(YieldIsKeyword, NameRequired);
        if (!kid)
            return null();

        if (!checkExportedName(kid->pn_funbox->function()->name()))
            return null();
        break;

      case TOK_CLASS: {
        kid = classDefinition(YieldIsKeyword, ClassStatement, NameRequired);
        if (!kid)
            return null();

        const ClassNode& cls = kid->as<ClassNode>();
        MOZ_ASSERT(cls.names());
        if (!checkExportedName(cls.names()->innerBinding()->pn_atom))
            return null();
        break;
      }

      case TOK_VAR:
        kid = declarationList(YieldIsName, PNK_VAR);
        if (!kid)
            return null();
        if (!MatchOrInsertSemicolonAfterExpression(tokenStream))
            return null();
        if (!checkExportedNamesForDeclaration(kid))
            return null();
        break;

      case TOK_DEFAULT: {
        if (!tokenStream.getToken(&tt, TokenStream::Operand))
            return null();

        if (!checkExportedName(context->names().default_))
            return null();

        ParseNode* nameNode = nullptr;
        switch (tt) {
          case TOK_FUNCTION:
            kid = functionStmt(YieldIsKeyword, AllowDefaultName);
            if (!kid)
                return null();
            break;
          case TOK_CLASS:
            kid = classDefinition(YieldIsKeyword, ClassStatement, AllowDefaultName);
            if (!kid)
                return null();
            break;
          default: {
            tokenStream.ungetToken();
            RootedPropertyName name(context, context->names().starDefaultStar);
            nameNode = newName(name);
            if (!nameNode)
                return null();
            if (!noteDeclaredName(name, DeclarationKind::Const, pos()))
                return null();
            kid = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
            if (!kid)
                return null();
            if (!MatchOrInsertSemicolonAfterExpression(tokenStream))
                return null();
            break;
          }
        }

        ParseNode* node = handler.newExportDefaultDeclaration(kid, nameNode,
                                                              TokenPos(begin, pos().end));
        if (!node || !pc->sc()->asModuleContext()->builder.processExport(node))
            return null();

        return node;
      }

      case TOK_LET:
      case TOK_CONST:
        kid = lexicalDeclaration(YieldIsName, tt == TOK_CONST);
        if (!kid)
            return null();
        if (!checkExportedNamesForDeclaration(kid))
            return null();
        break;

      default:
        report(ParseError, false, null(), JSMSG_DECLARATION_AFTER_EXPORT);
        return null();
    }

    ParseNode* node = handler.newExportDeclaration(kid, TokenPos(begin, pos().end));
    if (!node || !pc->sc()->asModuleContext()->builder.processExport(node))
        return null();

    return node;
}

template<>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::exportDeclaration()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expressionStatement(YieldHandling yieldHandling, InvokedPrediction invoked)
{
    tokenStream.ungetToken();
    Node pnexpr = expr(InAllowed, yieldHandling, TripledotProhibited, invoked);
    if (!pnexpr)
        return null();
    if (!MatchOrInsertSemicolonAfterExpression(tokenStream))
        return null();
    return handler.newExprStatement(pnexpr, pos().end);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::ifStatement(YieldHandling yieldHandling)
{
    Vector<Node, 4> condList(context), thenList(context);
    Vector<uint32_t, 4> posList(context);
    Node elseBranch;

    ParseContext::Statement stmt(pc, StatementKind::If);

    while (true) {
        uint32_t begin = pos().begin;

        /* An IF node has three kids: condition, then, and optional else. */
        Node cond = condition(InAllowed, yieldHandling);
        if (!cond)
            return null();

        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_SEMI) {
            if (!report(ParseExtraWarning, false, null(), JSMSG_EMPTY_CONSEQUENT))
                return null();
        }

        Node thenBranch = statement(yieldHandling);
        if (!thenBranch)
            return null();

        if (!condList.append(cond) || !thenList.append(thenBranch) || !posList.append(begin))
            return null();

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_ELSE, TokenStream::Operand))
            return null();
        if (matched) {
            if (!tokenStream.matchToken(&matched, TOK_IF, TokenStream::Operand))
                return null();
            if (matched)
                continue;
            elseBranch = statement(yieldHandling);
            if (!elseBranch)
                return null();
        } else {
            elseBranch = null();
        }
        break;
    }

    for (int i = condList.length() - 1; i >= 0; i--) {
        elseBranch = handler.newIfStatement(posList[i], condList[i], thenList[i], elseBranch);
        if (!elseBranch)
            return null();
    }

    return elseBranch;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::doWhileStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    ParseContext::Statement stmt(pc, StatementKind::DoLoop);
    Node body = statement(yieldHandling);
    if (!body)
        return null();
    MUST_MATCH_TOKEN_MOD(TOK_WHILE, TokenStream::Operand, JSMSG_WHILE_AFTER_DO);
    Node cond = condition(InAllowed, yieldHandling);
    if (!cond)
        return null();

    // The semicolon after do-while is even more optional than most
    // semicolons in JS.  Web compat required this by 2004:
    //   http://bugzilla.mozilla.org/show_bug.cgi?id=238945
    // ES3 and ES5 disagreed, but ES6 conforms to Web reality:
    //   https://bugs.ecmascript.org/show_bug.cgi?id=157
    // To parse |do {} while (true) false| correctly, use Operand.
    bool ignored;
    if (!tokenStream.matchToken(&ignored, TOK_SEMI, TokenStream::Operand))
        return null();
    return handler.newDoWhileStatement(body, cond, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::whileStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    ParseContext::Statement stmt(pc, StatementKind::WhileLoop);
    Node cond = condition(InAllowed, yieldHandling);
    if (!cond)
        return null();
    Node body = statement(yieldHandling);
    if (!body)
        return null();
    return handler.newWhileStatement(begin, cond, body);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchInOrOf(bool* isForInp, bool* isForOfp)
{
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;

    *isForInp = tt == TOK_IN;
    *isForOfp = tt == TOK_NAME && tokenStream.currentToken().name() == context->names().of;
    if (!*isForInp && !*isForOfp) {
        tokenStream.ungetToken();
    } else {
        if (tt == TOK_NAME && !checkUnescapedName())
            return false;
    }

    MOZ_ASSERT_IF(*isForInp || *isForOfp, *isForInp != *isForOfp);
    return true;
}

template <class ParseHandler>
bool
Parser<ParseHandler>::validateForInOrOfLHSExpression(Node target)
{
    if (handler.isUnparenthesizedDestructuringPattern(target))
        return checkDestructuringPattern(target);

    // All other permitted targets are simple.
    if (!reportIfNotValidSimpleAssignmentTarget(target, ForInOrOfTarget))
        return false;

    if (handler.isPropertyAccess(target))
        return true;

    if (handler.isNameAnyParentheses(target)) {
        // The arguments/eval identifiers are simple in non-strict mode code,
        // but warn to discourage use nonetheless.
        if (!reportIfArgumentsEvalTarget(target))
            return false;

        handler.adjustGetToSet(target);
        return true;
    }

    if (handler.isFunctionCall(target))
        return makeSetCall(target, JSMSG_BAD_FOR_LEFTSIDE);

    report(ParseError, false, target, JSMSG_BAD_FOR_LEFTSIDE);
    return false;
}

template <class ParseHandler>
bool
Parser<ParseHandler>::forHeadStart(YieldHandling yieldHandling,
                                   ParseNodeKind* forHeadKind,
                                   Node* forInitialPart,
                                   Maybe<ParseContext::Scope>& forLoopLexicalScope,
                                   Node* forInOrOfExpression)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));

    TokenKind tt;
    if (!tokenStream.peekToken(&tt, TokenStream::Operand))
        return null();

    // Super-duper easy case: |for (;| is a C-style for-loop with no init
    // component.
    if (tt == TOK_SEMI) {
        *forInitialPart = null();
        *forHeadKind = PNK_FORHEAD;
        return true;
    }

    // Parsing after |for (var| is also relatively simple (from this method's
    // point of view).  No block-related work complicates matters, so delegate
    // to Parser::declaration.
    if (tt == TOK_VAR) {
        tokenStream.consumeKnownToken(tt, TokenStream::Operand);

        // Pass null for block object because |var| declarations don't use one.
        *forInitialPart = declarationList(yieldHandling, PNK_VAR, forHeadKind,
                                          forInOrOfExpression);
        return *forInitialPart != null();
    }

    // Otherwise we have a lexical declaration or an expression.

    // For-in loop backwards compatibility requires that |let| starting a
    // for-loop that's not a (new to ES6) for-of loop, in non-strict mode code,
    // parse as an identifier.  (|let| in for-of is always a declaration.)
    // Thus we must can't just sniff out TOK_CONST/TOK_LET here.  :-(
    bool parsingLexicalDeclaration = false;
    bool letIsIdentifier = false;
    if (tt == TOK_LET || tt == TOK_CONST) {
        parsingLexicalDeclaration = true;
        tokenStream.consumeKnownToken(tt, TokenStream::Operand);
    } else if (tt == TOK_NAME && tokenStream.nextName() == context->names().let) {
        // Check for the backwards-compatibility corner case in sloppy
        // mode like |for (let in e)| where the 'let' token should be
        // parsed as an identifier.
        if (!peekShouldParseLetDeclaration(&parsingLexicalDeclaration, TokenStream::Operand))
            return false;

        letIsIdentifier = !parsingLexicalDeclaration;
    }

    if (parsingLexicalDeclaration) {
        forLoopLexicalScope.emplace(this);
        if (!forLoopLexicalScope->init(pc))
            return null();

        // Push a temporary ForLoopLexicalHead Statement that allows for
        // lexical declarations, as they are usually allowed only in braced
        // statements.
        ParseContext::Statement forHeadStmt(pc, StatementKind::ForLoopLexicalHead);

        *forInitialPart = declarationList(yieldHandling, tt == TOK_CONST ? PNK_CONST : PNK_LET,
                                          forHeadKind, forInOrOfExpression);
        return *forInitialPart != null();
    }

    // Finally, handle for-loops that start with expressions.  Pass
    // |InProhibited| so that |in| isn't parsed in a RelationalExpression as a
    // binary operator.  |in| makes it a for-in loop, *not* an |in| expression.
    *forInitialPart = expr(InProhibited, yieldHandling, TripledotProhibited);
    if (!*forInitialPart)
        return false;

    bool isForIn, isForOf;
    if (!matchInOrOf(&isForIn, &isForOf))
        return false;

    // If we don't encounter 'in'/'of', we have a for(;;) loop.  We've handled
    // the init expression; the caller handles the rest.  Allow the Operand
    // modifier when regetting: Operand must be used to examine the ';' in
    // |for (;|, and our caller handles this case and that.
    if (!isForIn && !isForOf) {
        *forHeadKind = PNK_FORHEAD;
        tokenStream.addModifierException(TokenStream::OperandIsNone);
        return true;
    }

    MOZ_ASSERT(isForIn != isForOf);

    // In a for-of loop, 'let' that starts the loop head is a |let| keyword,
    // per the [lookahead ≠ let] restriction on the LeftHandSideExpression
    // variant of such loops.  Expressions that start with |let| can't be used
    // here.
    //
    //   var let = {};
    //   for (let.prop of [1]) // BAD
    //     break;
    //
    // See ES6 13.7.
    if (isForOf && letIsIdentifier) {
        report(ParseError, false, *forInitialPart, JSMSG_LET_STARTING_FOROF_LHS);
        return false;
    }

    *forHeadKind = isForIn ? PNK_FORIN : PNK_FOROF;

    if (!validateForInOrOfLHSExpression(*forInitialPart))
        return false;

    // Finally, parse the iterated expression, making the for-loop's closing
    // ')' the next token.
    *forInOrOfExpression = expressionAfterForInOrOf(*forHeadKind, yieldHandling);
    return *forInOrOfExpression != null();
}

template <class ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::forStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));
    uint32_t begin = pos().begin;

    ParseContext::Statement stmt(pc, StatementKind::ForLoop);

    bool isForEach = false;
    unsigned iflags = 0;

    if (allowsForEachIn()) {
        bool matched;
        if (!tokenStream.matchContextualKeyword(&matched, context->names().each))
            return null();
        if (matched) {
            iflags = JSITER_FOREACH;
            isForEach = true;
            addTelemetry(JSCompartment::DeprecatedForEach);
            if (versionNumber() < JSVERSION_LATEST) {
                if (!report(ParseWarning, pc->sc()->strict(), null(), JSMSG_DEPRECATED_FOR_EACH))
                    return null();
            }
        }
    }

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    // PNK_FORHEAD, PNK_FORIN, or PNK_FOROF depending on the loop type.
    ParseNodeKind headKind;

    // |x| in either |for (x; ...; ...)| or |for (x in/of ...)|.
    Node startNode;

    // The next two variables are used to implement `for (let/const ...)`.
    //
    // We generate an implicit block, wrapping the whole loop, to store loop
    // variables declared this way. Note that if the loop uses `for (var...)`
    // instead, those variables go on some existing enclosing scope, so no
    // implicit block scope is created.
    //
    // Both variables remain null/none if the loop is any other form.

    // The static block scope for the implicit block scope.
    Maybe<ParseContext::Scope> forLoopLexicalScope;

    // The expression being iterated over, for for-in/of loops only.  Unused
    // for for(;;) loops.
    Node iteratedExpr;

    // Parse the entirety of the loop-head for a for-in/of loop (so the next
    // token is the closing ')'):
    //
    //   for (... in/of ...) ...
    //                     ^next token
    //
    // ...OR, parse up to the first ';' in a C-style for-loop:
    //
    //   for (...; ...; ...) ...
    //           ^next token
    //
    // In either case the subsequent token can be consistently accessed using
    // TokenStream::None semantics.
    if (!forHeadStart(yieldHandling, &headKind, &startNode, forLoopLexicalScope,
                      &iteratedExpr))
    {
        return null();
    }

    MOZ_ASSERT(headKind == PNK_FORIN || headKind == PNK_FOROF || headKind == PNK_FORHEAD);

    Node forHead;
    if (headKind == PNK_FORHEAD) {
        Node init = startNode;

        if (isForEach) {
            reportWithOffset(ParseError, false, begin, JSMSG_BAD_FOR_EACH_LOOP);
            return null();
        }

        // Look for an operand: |for (;| means we might have already examined
        // this semicolon with that modifier.
        MUST_MATCH_TOKEN_MOD(TOK_SEMI, TokenStream::Operand, JSMSG_SEMI_AFTER_FOR_INIT);

        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();

        Node test;
        TokenStream::Modifier mod;
        if (tt == TOK_SEMI) {
            test = null();
            mod = TokenStream::Operand;
        } else {
            test = expr(InAllowed, yieldHandling, TripledotProhibited);
            if (!test)
                return null();
            mod = TokenStream::None;
        }

        MUST_MATCH_TOKEN_MOD(TOK_SEMI, mod, JSMSG_SEMI_AFTER_FOR_COND);

        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();

        Node update;
        if (tt == TOK_RP) {
            update = null();
            mod = TokenStream::Operand;
        } else {
            update = expr(InAllowed, yieldHandling, TripledotProhibited);
            if (!update)
                return null();
            mod = TokenStream::None;
        }

        MUST_MATCH_TOKEN_MOD(TOK_RP, mod, JSMSG_PAREN_AFTER_FOR_CTRL);

        TokenPos headPos(begin, pos().end);
        forHead = handler.newForHead(init, test, update, headPos);
        if (!forHead)
            return null();
    } else {
        MOZ_ASSERT(headKind == PNK_FORIN || headKind == PNK_FOROF);

        // |target| is the LeftHandSideExpression or declaration to which the
        // per-iteration value (an arbitrary value exposed by the iteration
        // protocol, or a string naming a property) is assigned.
        Node target = startNode;

        // Parse the rest of the for-in/of head.
        if (headKind == PNK_FORIN) {
            stmt.refineForKind(StatementKind::ForInLoop);
            iflags |= JSITER_ENUMERATE;
        } else {
            if (isForEach) {
                report(ParseError, false, startNode, JSMSG_BAD_FOR_EACH_LOOP);
                return null();
            }

            stmt.refineForKind(StatementKind::ForOfLoop);
        }

        if (!handler.isDeclarationList(target)) {
            MOZ_ASSERT(!forLoopLexicalScope);
            if (!checkAndMarkAsAssignmentLhs(target, PlainAssignment))
                return null();
        }

        // Parser::declaration consumed everything up to the closing ')'.  That
        // token follows an {Assignment,}Expression, so the next token must be
        // consumed as if an operator continued the expression, i.e. as None.
        MUST_MATCH_TOKEN_MOD(TOK_RP, TokenStream::None, JSMSG_PAREN_AFTER_FOR_CTRL);

        TokenPos headPos(begin, pos().end);
        forHead = handler.newForInOrOfHead(headKind, target, iteratedExpr, headPos);
        if (!forHead)
            return null();
    }

    Node body = statement(yieldHandling);
    if (!body)
        return null();

    Node forLoop = handler.newForStatement(begin, forHead, body, iflags);
    if (!forLoop)
        return null();

    if (forLoopLexicalScope)
        return finishLexicalScope(*forLoopLexicalScope, forLoop);

    return forLoop;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::switchStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_SWITCH));
    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_SWITCH);

    Node discriminant = exprInParens(InAllowed, yieldHandling, TripledotProhibited);
    if (!discriminant)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_SWITCH);
    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_SWITCH);

    ParseContext::Statement stmt(pc, StatementKind::Switch);
    ParseContext::Scope scope(this);
    if (!scope.init(pc))
        return null();

    Node caseList = handler.newStatementList(pos());
    if (!caseList)
        return null();

    bool seenDefault = false;
    TokenKind tt;
    while (true) {
        if (!tokenStream.getToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_RC)
            break;
        uint32_t caseBegin = pos().begin;

        Node caseExpr;
        switch (tt) {
          case TOK_DEFAULT:
            if (seenDefault) {
                report(ParseError, false, null(), JSMSG_TOO_MANY_DEFAULTS);
                return null();
            }
            seenDefault = true;
            caseExpr = null();  // The default case has pn_left == nullptr.
            break;

          case TOK_CASE:
            caseExpr = expr(InAllowed, yieldHandling, TripledotProhibited);
            if (!caseExpr)
                return null();
            break;

          default:
            report(ParseError, false, null(), JSMSG_BAD_SWITCH);
            return null();
        }

        MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_CASE);

        Node body = handler.newStatementList(pos());
        if (!body)
            return null();

        bool afterReturn = false;
        bool warnedAboutStatementsAfterReturn = false;
        uint32_t statementBegin = 0;
        while (true) {
            if (!tokenStream.peekToken(&tt, TokenStream::Operand))
                return null();
            if (tt == TOK_RC || tt == TOK_CASE || tt == TOK_DEFAULT)
                break;
            if (afterReturn) {
                TokenPos pos(0, 0);
                if (!tokenStream.peekTokenPos(&pos, TokenStream::Operand))
                    return null();
                statementBegin = pos.begin;
            }
            Node stmt = statement(yieldHandling);
            if (!stmt)
                return null();
            if (!warnedAboutStatementsAfterReturn) {
                if (afterReturn) {
                    if (!handler.isStatementPermittedAfterReturnStatement(stmt)) {
                        if (!reportWithOffset(ParseWarning, false, statementBegin,
                                              JSMSG_STMT_AFTER_RETURN))
                        {
                            return null();
                        }
                        warnedAboutStatementsAfterReturn = true;
                    }
                } else if (handler.isReturnStatement(stmt)) {
                    afterReturn = true;
                }
            }
            handler.addStatementToList(body, stmt);
        }

        Node casepn = handler.newCaseOrDefault(caseBegin, caseExpr, body);
        if (!casepn)
            return null();
        handler.addCaseStatementToList(caseList, casepn);
    }

    caseList = finishLexicalScope(scope, caseList);
    if (!caseList)
        return null();

    handler.setEndPosition(caseList, pos().end);

    return handler.newSwitchStatement(begin, discriminant, caseList);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::continueStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_CONTINUE));
    uint32_t begin = pos().begin;

    RootedPropertyName label(context);
    if (!matchLabel(yieldHandling, &label))
        return null();

    // Labeled 'continue' statements target the nearest labeled loop
    // statements with the same label. Unlabeled 'continue' statements target
    // the innermost loop statement.
    auto isLoop = [](ParseContext::Statement* stmt) {
        return StatementKindIsLoop(stmt->kind());
    };

    if (label) {
        bool foundTarget = false;
        ParseContext::Statement* stmt = pc->innermostStatement();

        for (;;) {
            stmt = ParseContext::Statement::findNearest(stmt, isLoop);
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_BAD_CONTINUE);
                return null();
            }

            // Is it labeled by our label?
            stmt = stmt->enclosing();
            while (stmt && stmt->is<ParseContext::LabelStatement>()) {
                if (stmt->as<ParseContext::LabelStatement>().label() == label) {
                    foundTarget = true;
                    break;
                }
                stmt = stmt->enclosing();
            }
            if (foundTarget)
                break;
        }

        if (!foundTarget) {
            report(ParseError, false, null(), JSMSG_LABEL_NOT_FOUND);
            return null();
        }
    } else if (!pc->findInnermostStatement(isLoop)) {
        report(ParseError, false, null(), JSMSG_BAD_CONTINUE);
        return null();
    }

    if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
        return null();

    return handler.newContinueStatement(label, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::breakStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_BREAK));
    uint32_t begin = pos().begin;

    RootedPropertyName label(context);
    if (!matchLabel(yieldHandling, &label))
        return null();

    // Labeled 'break' statements target the nearest labeled statements (could
    // be any kind) with the same label. Unlabeled 'break' statements target
    // the innermost loop or switch statement.
    if (label) {
        auto hasSameLabel = [&label](ParseContext::LabelStatement* stmt) {
            return stmt->label() == label;
        };

        if (!pc->findInnermostStatement<ParseContext::LabelStatement>(hasSameLabel)) {
            report(ParseError, false, null(), JSMSG_LABEL_NOT_FOUND);
            return null();
        }
    } else {
        auto isBreakTarget = [](ParseContext::Statement* stmt) {
            return StatementKindIsUnlabeledBreakTarget(stmt->kind());
        };

        if (!pc->findInnermostStatement(isBreakTarget)) {
            report(ParseError, false, null(), JSMSG_TOUGH_BREAK);
            return null();
        }
    }

    if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
        return null();

    return handler.newBreakStatement(label, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::returnStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_RETURN));
    uint32_t begin = pos().begin;

    MOZ_ASSERT(pc->isFunctionBox());
    pc->functionBox()->usesReturn = true;

    // Parse an optional operand.
    //
    // This is ugly, but we don't want to require a semicolon.
    Node exprNode;
    TokenKind tt = TOK_EOF;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return null();
    switch (tt) {
      case TOK_EOL:
      case TOK_EOF:
      case TOK_SEMI:
      case TOK_RC:
        exprNode = null();
        pc->funHasReturnVoid = true;
        break;
      default: {
        exprNode = expr(InAllowed, yieldHandling, TripledotProhibited);
        if (!exprNode)
            return null();
        pc->funHasReturnExpr = true;
      }
    }

    if (exprNode) {
        if (!MatchOrInsertSemicolonAfterExpression(tokenStream))
            return null();
    } else {
        if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
            return null();
    }

    Node pn = handler.newReturnStatement(exprNode, TokenPos(begin, pos().end));
    if (!pn)
        return null();

    if (pc->isLegacyGenerator() && exprNode) {
        /* Disallow "return v;" in legacy generators. */
        reportBadReturn(pn, ParseError, JSMSG_BAD_GENERATOR_RETURN,
                        JSMSG_BAD_ANON_GENERATOR_RETURN);
        return null();
    }

    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newYieldExpression(uint32_t begin, typename ParseHandler::Node expr,
                                         bool isYieldStar)
{
    Node generator = newDotGeneratorName();
    if (!generator)
        return null();
    if (isYieldStar)
        return handler.newYieldStarExpression(begin, expr, generator);
    return handler.newYieldExpression(begin, expr, generator);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::yieldExpression(InHandling inHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_YIELD));
    uint32_t begin = pos().begin;

    switch (pc->generatorKind()) {
      case StarGenerator:
      {
        MOZ_ASSERT(pc->isFunctionBox());

        pc->lastYieldOffset = begin;

        Node exprNode;
        ParseNodeKind kind = PNK_YIELD;
        TokenKind tt = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
            return null();
        switch (tt) {
          // TOK_EOL is special; it implements the [no LineTerminator here]
          // quirk in the grammar.
          case TOK_EOL:
          // The rest of these make up the complete set of tokens that can
          // appear after any of the places where AssignmentExpression is used
          // throughout the grammar.  Conveniently, none of them can also be the
          // start an expression.
          case TOK_EOF:
          case TOK_SEMI:
          case TOK_RC:
          case TOK_RB:
          case TOK_RP:
          case TOK_COLON:
          case TOK_COMMA:
            // No value.
            exprNode = null();
            tokenStream.addModifierException(TokenStream::NoneIsOperand);
            break;
          case TOK_MUL:
            kind = PNK_YIELD_STAR;
            tokenStream.consumeKnownToken(TOK_MUL, TokenStream::Operand);
            MOZ_FALLTHROUGH;
          default:
            exprNode = assignExpr(inHandling, YieldIsKeyword, TripledotProhibited);
            if (!exprNode)
                return null();
        }
        return newYieldExpression(begin, exprNode, kind == PNK_YIELD_STAR);
      }

      case NotGenerator:
        // We are in code that has not seen a yield, but we are in JS 1.7 or
        // later.  Try to transition to being a legacy generator.
        MOZ_ASSERT(tokenStream.versionNumber() >= JSVERSION_1_7);
        MOZ_ASSERT(pc->lastYieldOffset == ParseContext::NoYieldOffset);

        if (!abortIfSyntaxParser())
            return null();

        if (!pc->isFunctionBox()) {
            report(ParseError, false, null(), JSMSG_BAD_RETURN_OR_YIELD, js_yield_str);
            return null();
        }

        if (pc->functionBox()->isArrow()) {
            reportWithOffset(ParseError, false, begin,
                             JSMSG_YIELD_IN_ARROW, js_yield_str);
            return null();
        }

        if (pc->functionBox()->function()->isMethod() ||
            pc->functionBox()->function()->isGetter() ||
            pc->functionBox()->function()->isSetter())
        {
            reportWithOffset(ParseError, false, begin,
                             JSMSG_YIELD_IN_METHOD, js_yield_str);
            return null();
        }

        if (pc->funHasReturnExpr
#if JS_HAS_EXPR_CLOSURES
            || pc->functionBox()->function()->isExprBody()
#endif
            )
        {
            /* As in Python (see PEP-255), disallow return v; in generators. */
            reportBadReturn(null(), ParseError, JSMSG_BAD_GENERATOR_RETURN,
                            JSMSG_BAD_ANON_GENERATOR_RETURN);
            return null();
        }

        pc->functionBox()->setGeneratorKind(LegacyGenerator);
        addTelemetry(JSCompartment::DeprecatedLegacyGenerator);

        MOZ_FALLTHROUGH;

      case LegacyGenerator:
      {
        // We are in a legacy generator: a function that has already seen a
        // yield.
        MOZ_ASSERT(pc->isFunctionBox());

        pc->lastYieldOffset = begin;

        // Legacy generators do not require a value.
        Node exprNode;
        TokenKind tt = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
            return null();
        switch (tt) {
          case TOK_EOF:
          case TOK_EOL:
          case TOK_SEMI:
          case TOK_RC:
          case TOK_RB:
          case TOK_RP:
          case TOK_COLON:
          case TOK_COMMA:
            // No value.
            exprNode = null();
            tokenStream.addModifierException(TokenStream::NoneIsOperand);
            break;
          default:
            exprNode = assignExpr(inHandling, YieldIsKeyword, TripledotProhibited);
            if (!exprNode)
                return null();
        }

        return newYieldExpression(begin, exprNode);
      }
    }

    MOZ_CRASH("yieldExpr");
}

template <>
ParseNode*
Parser<FullParseHandler>::withStatement(YieldHandling yieldHandling)
{
    // test262/ch12/12.10/12.10-0-1.js fails if we try to parse with-statements
    // in syntax-parse mode. See bug 892583.
    if (handler.syntaxParser) {
        handler.disableSyntaxParser();
        abortedSyntaxParse = true;
        return null();
    }

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_WITH));
    uint32_t begin = pos().begin;

    // In most cases, we want the constructs forbidden in strict mode code to be
    // a subset of those that JSOPTION_EXTRA_WARNINGS warns about, and we should
    // use reportStrictModeError.  However, 'with' is the sole instance of a
    // construct that is forbidden in strict mode code, but doesn't even merit a
    // warning under JSOPTION_EXTRA_WARNINGS.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=514576#c1.
    if (pc->sc()->strict()) {
        if (!report(ParseStrictError, true, null(), JSMSG_STRICT_CODE_WITH))
            return null();
    }

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_WITH);
    Node objectExpr = exprInParens(InAllowed, yieldHandling, TripledotProhibited);
    if (!objectExpr)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_WITH);

    Node innerBlock;
    {
        ParseContext::Statement stmt(pc, StatementKind::With);
        innerBlock = statement(yieldHandling);
        if (!innerBlock)
            return null();
    }

    pc->sc()->setBindingsAccessedDynamically();

    return handler.newWithStatement(begin, objectExpr, innerBlock);
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::withStatement(YieldHandling yieldHandling)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return null();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::labeledItem(YieldHandling yieldHandling)
{
    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    if (tt == TOK_FUNCTION)
        return functionStmt(yieldHandling, NameRequired);

    tokenStream.ungetToken();
    return statement(yieldHandling);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::labeledStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    RootedPropertyName label(context, tokenStream.currentName());

    auto hasSameLabel = [&label](ParseContext::LabelStatement* stmt) {
        return stmt->label() == label;
    };

    if (pc->findInnermostStatement<ParseContext::LabelStatement>(hasSameLabel)) {
        report(ParseError, false, null(), JSMSG_DUPLICATE_LABEL);
        return null();
    }

    tokenStream.consumeKnownToken(TOK_COLON);

    /* Push a label struct and parse the statement. */
    ParseContext::LabelStatement stmt(pc, label);
    Node pn = labeledItem(yieldHandling);
    if (!pn)
        return null();

    return handler.newLabeledStatement(label, pn, begin);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::throwStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_THROW));
    uint32_t begin = pos().begin;

    /* ECMA-262 Edition 3 says 'throw [no LineTerminator here] Expr'. */
    TokenKind tt = TOK_EOF;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return null();
    if (tt == TOK_EOF || tt == TOK_SEMI || tt == TOK_RC) {
        report(ParseError, false, null(), JSMSG_MISSING_EXPR_AFTER_THROW);
        return null();
    }
    if (tt == TOK_EOL) {
        report(ParseError, false, null(), JSMSG_LINE_BREAK_AFTER_THROW);
        return null();
    }

    Node throwExpr = expr(InAllowed, yieldHandling, TripledotProhibited);
    if (!throwExpr)
        return null();

    if (!MatchOrInsertSemicolonAfterExpression(tokenStream))
        return null();

    return handler.newThrowStatement(throwExpr, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::tryStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_TRY));
    uint32_t begin = pos().begin;

    /*
     * try nodes are ternary.
     * kid1 is the try statement
     * kid2 is the catch node list or null
     * kid3 is the finally statement
     *
     * catch nodes are ternary.
     * kid1 is the lvalue (TOK_NAME, TOK_LB, or TOK_LC)
     * kid2 is the catch guard or null if no guard
     * kid3 is the catch block
     *
     * catch lvalue nodes are either:
     *   TOK_NAME for a single identifier
     *   TOK_RB or TOK_RC for a destructuring left-hand side
     *
     * finally nodes are TOK_LC statement lists.
     */

    Node innerBlock;
    {
        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_TRY);

        ParseContext::Statement stmt(pc, StatementKind::Try);
        ParseContext::Scope scope(this);
        if (!scope.init(pc))
            return null();

        innerBlock = statementList(yieldHandling);
        if (!innerBlock)
            return null();

        innerBlock = finishLexicalScope(scope, innerBlock);
        if (!innerBlock)
            return null();

        MUST_MATCH_TOKEN_MOD(TOK_RC, TokenStream::Operand, JSMSG_CURLY_AFTER_TRY);
    }

    bool hasUnconditionalCatch = false;
    Node catchList = null();
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();
    if (tt == TOK_CATCH) {
        catchList = handler.newCatchList();
        if (!catchList)
            return null();

        do {
            Node pnblock;

            /* Check for another catch after unconditional catch. */
            if (hasUnconditionalCatch) {
                report(ParseError, false, null(), JSMSG_CATCH_AFTER_GENERAL);
                return null();
            }

            /*
             * Create a lexical scope node around the whole catch clause,
             * including the head.
             */
            ParseContext::Statement stmt(pc, StatementKind::Catch);
            ParseContext::Scope scope(this);
            if (!scope.init(pc))
                return null();

            /*
             * Legal catch forms are:
             *   catch (lhs)
             *   catch (lhs if <boolean_expression>)
             * where lhs is a name or a destructuring left-hand side.
             * (the latter is legal only #ifdef JS_HAS_CATCH_GUARD)
             */
            MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_CATCH);

            RootedPropertyName simpleCatchParam(context);

            if (!tokenStream.getToken(&tt))
                return null();
            Node catchName;
            switch (tt) {
              case TOK_LB:
              case TOK_LC:
                catchName = destructuringDeclaration(DeclarationKind::CatchParameter,
                                                     yieldHandling, tt);
                if (!catchName)
                    return null();
                break;
              case TOK_YIELD:
                if (yieldHandling == YieldIsKeyword) {
                    report(ParseError, false, null(), JSMSG_RESERVED_ID, "yield");
                    return null();
                }

                // Even if yield is *not* necessarily a keyword, we still must
                // check its validity for legacy generators.
                if (!checkYieldNameValidity())
                    return null();
                MOZ_FALLTHROUGH;
              case TOK_NAME:
                simpleCatchParam = tokenStream.currentName();
                catchName = newName(simpleCatchParam);
                if (!catchName)
                    return null();
                if (!noteDeclaredName(simpleCatchParam, DeclarationKind::SimpleCatchParameter,
                                      pos()))
                {
                    return null();
                }
                break;

              default:
                report(ParseError, false, null(), JSMSG_CATCH_IDENTIFIER);
                return null();
            }

            Node catchGuard = null();
#if JS_HAS_CATCH_GUARD
            /*
             * We use 'catch (x if x === 5)' (not 'catch (x : x === 5)')
             * to avoid conflicting with the JS2/ECMAv4 type annotation
             * catchguard syntax.
             */
            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_IF))
                return null();
            if (matched) {
                catchGuard = expr(InAllowed, yieldHandling, TripledotProhibited);
                if (!catchGuard)
                    return null();
            }
#endif
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_CATCH);

            MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CATCH);

            Node catchBody = catchBlockStatement(yieldHandling, simpleCatchParam);
            if (!catchBody)
                return null();

            if (!catchGuard)
                hasUnconditionalCatch = true;

            pnblock = finishLexicalScope(scope, catchBody);
            if (!pnblock)
                return null();

            if (!handler.addCatchBlock(catchList, pnblock, catchName, catchGuard, catchBody))
                return null();
            handler.setEndPosition(catchList, pos().end);
            handler.setEndPosition(pnblock, pos().end);

            if (!tokenStream.getToken(&tt, TokenStream::Operand))
                return null();
        } while (tt == TOK_CATCH);
    }

    Node finallyBlock = null();

    if (tt == TOK_FINALLY) {
        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY);

        ParseContext::Statement stmt(pc, StatementKind::Finally);
        ParseContext::Scope scope(this);
        if (!scope.init(pc))
            return null();

        finallyBlock = statementList(yieldHandling);
        if (!finallyBlock)
            return null();

        finallyBlock = finishLexicalScope(scope, finallyBlock);
        if (!finallyBlock)
            return null();

        MUST_MATCH_TOKEN_MOD(TOK_RC, TokenStream::Operand, JSMSG_CURLY_AFTER_FINALLY);
    } else {
        tokenStream.ungetToken();
    }
    if (!catchList && !finallyBlock) {
        report(ParseError, false, null(), JSMSG_CATCH_OR_FINALLY);
        return null();
    }

    return handler.newTryStatement(begin, innerBlock, catchList, finallyBlock);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::catchBlockStatement(YieldHandling yieldHandling,
                                          HandlePropertyName simpleCatchParam)
{
    ParseContext::Statement stmt(pc, StatementKind::Block);

    // Annex B.3.5 requires that vars be allowed to redeclare a simple
    // (non-destructured) catch parameter (including via a direct eval), so
    // the catch parameter needs to live in its own scope. So if we have a
    // simple catch parameter, make a new scope.
    Node body;
    if (simpleCatchParam) {
        ParseContext::Scope scope(this);
        if (!scope.init(pc))
            return null();

        // The catch parameter name cannot be redeclared inside the catch
        // block, so declare the name in the inner scope.
        if (!noteDeclaredName(simpleCatchParam, DeclarationKind::SimpleCatchParameter, pos()))
            return null();

        Node list = statementList(yieldHandling);
        if (!list)
            return null();

        // The catch parameter name is not bound in this scope, so remove it
        // before generating bindings.
        scope.removeSimpleCatchParameter(pc, simpleCatchParam);

        body = finishLexicalScope(scope, list);
    } else {
        body = statementList(yieldHandling);
    }
    if (!body)
        return null();

    MUST_MATCH_TOKEN_MOD(TOK_RC, TokenStream::Operand, JSMSG_CURLY_AFTER_CATCH);

    return body;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::debuggerStatement()
{
    TokenPos p;
    p.begin = pos().begin;
    if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
        return null();
    p.end = pos().end;

    pc->sc()->setBindingsAccessedDynamically();
    pc->sc()->setHasDebuggerStatement();

    return handler.newDebuggerStatement(p);
}

static JSOp
JSOpFromPropertyType(PropertyType propType)
{
    switch (propType) {
      case PropertyType::Getter:
      case PropertyType::GetterNoExpressionClosure:
        return JSOP_INITPROP_GETTER;
      case PropertyType::Setter:
      case PropertyType::SetterNoExpressionClosure:
        return JSOP_INITPROP_SETTER;
      case PropertyType::Normal:
      case PropertyType::Method:
      case PropertyType::GeneratorMethod:
      case PropertyType::Constructor:
      case PropertyType::DerivedConstructor:
        return JSOP_INITPROP;
      default:
        MOZ_CRASH("unexpected property type");
    }
}

static FunctionSyntaxKind
FunctionSyntaxKindFromPropertyType(PropertyType propType)
{
    switch (propType) {
      case PropertyType::Getter:
        return Getter;
      case PropertyType::GetterNoExpressionClosure:
        return GetterNoExpressionClosure;
      case PropertyType::Setter:
        return Setter;
      case PropertyType::SetterNoExpressionClosure:
        return SetterNoExpressionClosure;
      case PropertyType::Method:
        return Method;
      case PropertyType::GeneratorMethod:
        return Method;
      case PropertyType::Constructor:
        return ClassConstructor;
      case PropertyType::DerivedConstructor:
        return DerivedClassConstructor;
      default:
        MOZ_CRASH("unexpected property type");
    }
}

static GeneratorKind
GeneratorKindFromPropertyType(PropertyType propType)
{
    return propType == PropertyType::GeneratorMethod ? StarGenerator : NotGenerator;
}

template <>
ParseNode*
Parser<FullParseHandler>::classDefinition(YieldHandling yieldHandling,
                                          ClassContext classContext,
                                          DefaultHandling defaultHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_CLASS));

    bool savedStrictness = setLocalStrictMode(true);

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    RootedPropertyName name(context);
    if (tt == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tt == TOK_YIELD) {
        if (!checkYieldNameValidity())
            return null();
        MOZ_ASSERT(yieldHandling != YieldIsKeyword);
        name = tokenStream.currentName();
    } else if (classContext == ClassStatement) {
        if (defaultHandling == AllowDefaultName) {
            name = context->names().starDefaultStar;
            tokenStream.ungetToken();
        } else {
            // Class statements must have a bound name
            report(ParseError, false, null(), JSMSG_UNNAMED_CLASS_STMT);
            return null();
        }
    } else {
        // Make sure to put it back, whatever it was
        tokenStream.ungetToken();
    }

    if (name == context->names().let) {
        report(ParseError, false, null(), JSMSG_LET_CLASS_BINDING);
        return null();
    }

    RootedAtom propAtom(context);

    // A named class creates a new lexical scope with a const binding of the
    // class name.
    Maybe<ParseContext::Statement> classStmt;
    Maybe<ParseContext::Scope> classScope;
    if (name) {
        classStmt.emplace(pc, StatementKind::Block);
        classScope.emplace(this);
        if (!classScope->init(pc))
            return null();
    }

    // Because the binding definitions keep track of their blockId, we need to
    // create at least the inner binding later. Keep track of the name's position
    // in order to provide it for the nodes created later.
    TokenPos namePos = pos();

    ParseNode* classHeritage = null();
    bool hasHeritage;
    if (!tokenStream.matchToken(&hasHeritage, TOK_EXTENDS))
        return null();
    if (hasHeritage) {
        if (!tokenStream.getToken(&tt))
            return null();
        classHeritage = memberExpr(yieldHandling, TripledotProhibited, tt, true);
        if (!classHeritage)
            return null();
    }

    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CLASS);

    ParseNode* classMethods = handler.newClassMethodList(pos().begin);
    if (!classMethods)
        return null();

    bool seenConstructor = false;
    for (;;) {
        TokenKind tt;
        if (!tokenStream.getToken(&tt, TokenStream::KeywordIsName))
            return null();
        if (tt == TOK_RC)
            break;

        if (tt == TOK_SEMI)
            continue;

        bool isStatic = false;
        if (tt == TOK_NAME && tokenStream.currentName() == context->names().static_) {
            if (!tokenStream.peekToken(&tt, TokenStream::KeywordIsName))
                return null();
            if (tt == TOK_RC) {
                tokenStream.consumeKnownToken(tt, TokenStream::KeywordIsName);
                report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                       "property name", TokenKindToDesc(tt));
                return null();
            }

            if (tt != TOK_LP) {
                if (!checkUnescapedName())
                    return null();

                isStatic = true;
            } else {
                tokenStream.addModifierException(TokenStream::NoneIsKeywordIsName);
                tokenStream.ungetToken();
            }
        } else {
            tokenStream.ungetToken();
        }

        PropertyType propType;
        ParseNode* propName = propertyName(yieldHandling, classMethods, &propType, &propAtom);
        if (!propName)
            return null();

        if (propType != PropertyType::Getter && propType != PropertyType::Setter &&
            propType != PropertyType::Method && propType != PropertyType::GeneratorMethod &&
            propType != PropertyType::Constructor && propType != PropertyType::DerivedConstructor)
        {
            report(ParseError, false, null(), JSMSG_BAD_METHOD_DEF);
            return null();
        }

        if (propType == PropertyType::Getter)
            propType = PropertyType::GetterNoExpressionClosure;
        if (propType == PropertyType::Setter)
            propType = PropertyType::SetterNoExpressionClosure;
        if (!isStatic && propAtom == context->names().constructor) {
            if (propType != PropertyType::Method) {
                report(ParseError, false, propName, JSMSG_BAD_METHOD_DEF);
                return null();
            }
            if (seenConstructor) {
                report(ParseError, false, propName, JSMSG_DUPLICATE_PROPERTY, "constructor");
                return null();
            }
            seenConstructor = true;
            propType = hasHeritage ? PropertyType::DerivedConstructor : PropertyType::Constructor;
        } else if (isStatic && propAtom == context->names().prototype) {
            report(ParseError, false, propName, JSMSG_BAD_METHOD_DEF);
            return null();
        }

        // FIXME: Implement ES6 function "name" property semantics
        // (bug 883377).
        RootedAtom funName(context);
        switch (propType) {
          case PropertyType::GetterNoExpressionClosure:
          case PropertyType::SetterNoExpressionClosure:
            if (!tokenStream.isCurrentTokenType(TOK_RB)) {
                funName = prefixAccessorName(propType, propAtom);
                if (!funName)
                    return null();
            }
            break;
          case PropertyType::Constructor:
          case PropertyType::DerivedConstructor:
            funName = name;
            break;
          default:
            if (!tokenStream.isCurrentTokenType(TOK_RB))
                funName = propAtom;
        }
        ParseNode* fn = methodDefinition(yieldHandling, propType, funName);
        if (!fn)
            return null();

        JSOp op = JSOpFromPropertyType(propType);
        if (!handler.addClassMethodDefinition(classMethods, propName, fn, op, isStatic))
            return null();
    }

    ParseNode* nameNode = null();
    ParseNode* methodsOrBlock = classMethods;
    if (name) {
        // The inner name is immutable.
        if (!noteDeclaredName(name, DeclarationKind::Const, namePos))
            return null();

        ParseNode* innerName = newName(name, namePos);
        if (!innerName)
            return null();

        ParseNode* classBlock = finishLexicalScope(*classScope, classMethods);
        if (!classBlock)
            return null();

        methodsOrBlock = classBlock;

        // Pop the inner scope.
        classScope.reset();
        classStmt.reset();

        ParseNode* outerName = null();
        if (classContext == ClassStatement) {
            // The outer name is mutable.
            if (!noteDeclaredName(name, DeclarationKind::Let, namePos))
                return null();

            outerName = newName(name, namePos);
            if (!outerName)
                return null();
        }

        nameNode = handler.newClassNames(outerName, innerName, namePos);
        if (!nameNode)
            return null();
    }

    MOZ_ALWAYS_TRUE(setLocalStrictMode(savedStrictness));

    return handler.newClass(nameNode, classHeritage, methodsOrBlock);
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::classDefinition(YieldHandling yieldHandling,
                                            ClassContext classContext,
                                            DefaultHandling defaultHandling)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::shouldParseLetDeclaration(bool* parseDeclOut)
{
    TokenKind tt;
    if (!tokenStream.peekToken(&tt))
        return false;

    if (tt == TOK_NAME) {
        // |let| followed by a name is a lexical declaration.  This is so even
        // if the name is on a new line.  ASI applies *only* if an offending
        // token not allowed by the grammar is encountered, and there's no
        // [no LineTerminator here] restriction in LexicalDeclaration or
        // ForDeclaration forbidding a line break.
        //
        // It's a tricky point, but this is true *even if* the name is "let", a
        // name that can't be bound by LexicalDeclaration or ForDeclaration.
        // Per ES6 5.3, static semantics early errors are validated *after*
        // determining productions matching the source text.  So in this
        // example:
        //
        //   let   // ASI opportunity...except not
        //   let;
        //
        // the text matches LexicalDeclaration.  *Then* static semantics in
        // ES6 13.3.1.1 (corresponding to the LexicalDeclaration production
        // just chosen), per ES6 5.3, are validated to recognize the Script as
        // invalid.  It can't be evaluated, so a SyntaxError is thrown.
        *parseDeclOut = true;
    } else if (tt == TOK_LB || tt == TOK_LC) {
        *parseDeclOut = true;
    } else {
        // Whatever we have isn't a declaration.  Either it's an expression, or
        // it's invalid: expression-parsing code will decide.
        *parseDeclOut = false;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::peekShouldParseLetDeclaration(bool* parseDeclOut,
                                                    TokenStream::Modifier modifier)
{
    // 'let' is a reserved keyword in strict mode and we shouldn't get here.
    MOZ_ASSERT(!pc->sc()->strict());

    *parseDeclOut = false;

#ifdef DEBUG
    TokenKind tt;
    if (!tokenStream.peekToken(&tt, modifier))
        return false;
    MOZ_ASSERT(tt == TOK_NAME && tokenStream.nextName() == context->names().let);
#endif

    tokenStream.consumeKnownToken(TOK_NAME, modifier);
    if (!shouldParseLetDeclaration(parseDeclOut))
        return false;

    // Unget the TOK_NAME of 'let' if not parsing a declaration.
    if (!*parseDeclOut)
        tokenStream.ungetToken();

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::variableStatement(YieldHandling yieldHandling)
{
    Node vars = declarationList(yieldHandling, PNK_VAR);
    if (!vars)
        return null();
    if (!MatchOrInsertSemicolonAfterExpression(tokenStream))
        return null();
    return vars;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statement(YieldHandling yieldHandling, bool canHaveDirectives)
{
    MOZ_ASSERT(checkOptionsCalled);

    JS_CHECK_RECURSION(context, return null());

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    switch (tt) {
      // BlockStatement[?Yield, ?Return]
      case TOK_LC:
        return blockStatement(yieldHandling);

      // VariableStatement[?Yield]
      case TOK_VAR:
        return variableStatement(yieldHandling);

      // EmptyStatement
      case TOK_SEMI:
        return handler.newEmptyStatement(pos());

      // ExpressionStatement[?Yield].
      //
      // These should probably be handled by a single ExpressionStatement
      // function in a default, not split up this way.
      case TOK_STRING:
        if (!canHaveDirectives && tokenStream.currentToken().atom() == context->names().useAsm) {
            if (!abortIfSyntaxParser())
                return null();
            if (!report(ParseWarning, false, null(), JSMSG_USE_ASM_DIRECTIVE_FAIL))
                return null();
        }
        return expressionStatement(yieldHandling);

      case TOK_YIELD: {
        // Don't use a ternary operator here due to obscure linker issues
        // around using static consts in the arms of a ternary.
        TokenStream::Modifier modifier;
        if (yieldExpressionsSupported())
            modifier = TokenStream::Operand;
        else
            modifier = TokenStream::None;

        TokenKind next;
        if (!tokenStream.peekToken(&next, modifier))
            return null();
        if (next == TOK_COLON) {
            if (!checkYieldNameValidity())
                return null();
            return labeledStatement(yieldHandling);
        }
        return expressionStatement(yieldHandling);
      }

      case TOK_NAME: {
        // 'let' is a contextual keyword outside strict mode.  In strict mode
        // it's always tokenized as TOK_LET except in this one weird case:
        //
        //   "use strict" // ExpressionStatement, terminated by ASI
        //   let a = 1;   // LexicalDeclaration
        //
        // We can't apply strict mode until we know "use strict" is the entire
        // statement, but we can't know "use strict" is the entire statement
        // until we see the next token.  So 'let' is still TOK_NAME here.
        if (tokenStream.currentName() == context->names().let) {
            bool parseDecl;
            if (!shouldParseLetDeclaration(&parseDecl))
                return null();

            if (parseDecl)
                return lexicalDeclaration(yieldHandling, /* isConst = */ false);
        }

        TokenKind next;
        if (!tokenStream.peekToken(&next))
            return null();
        if (next == TOK_COLON)
            return labeledStatement(yieldHandling);
        return expressionStatement(yieldHandling);
      }

      case TOK_NEW:
        return expressionStatement(yieldHandling, PredictInvoked);

      default:
        return expressionStatement(yieldHandling);

      // IfStatement[?Yield, ?Return]
      case TOK_IF:
        return ifStatement(yieldHandling);

      // BreakableStatement[?Yield, ?Return]
      //
      // BreakableStatement[Yield, Return]:
      //   IterationStatement[?Yield, ?Return]
      //   SwitchStatement[?Yield, ?Return]
      case TOK_DO:
        return doWhileStatement(yieldHandling);

      case TOK_WHILE:
        return whileStatement(yieldHandling);

      case TOK_FOR:
        return forStatement(yieldHandling);

      case TOK_SWITCH:
        return switchStatement(yieldHandling);

      // ContinueStatement[?Yield]
      case TOK_CONTINUE:
        return continueStatement(yieldHandling);

      // BreakStatement[?Yield]
      case TOK_BREAK:
        return breakStatement(yieldHandling);

      // [+Return] ReturnStatement[?Yield]
      case TOK_RETURN:
        // The Return parameter is only used here, and the effect is easily
        // detected this way, so don't bother passing around an extra parameter
        // everywhere.
        if (!pc->isFunctionBox()) {
            report(ParseError, false, null(), JSMSG_BAD_RETURN_OR_YIELD, js_return_str);
            return null();
        }
        return returnStatement(yieldHandling);

      // WithStatement[?Yield, ?Return]
      case TOK_WITH:
        return withStatement(yieldHandling);

      // LabelledStatement[?Yield, ?Return]
      // This is really handled by TOK_NAME and TOK_YIELD cases above.

      // ThrowStatement[?Yield]
      case TOK_THROW:
        return throwStatement(yieldHandling);

      // TryStatement[?Yield, ?Return]
      case TOK_TRY:
        return tryStatement(yieldHandling);

      // DebuggerStatement
      case TOK_DEBUGGER:
        return debuggerStatement();

      // HoistableDeclaration[?Yield]
      case TOK_FUNCTION:
        return functionStmt(yieldHandling, NameRequired);

      // ClassDeclaration[?Yield]
      case TOK_CLASS:
        if (!abortIfSyntaxParser())
            return null();
        return classDefinition(yieldHandling, ClassStatement, NameRequired);

      // LexicalDeclaration[In, ?Yield]
      case TOK_LET:
      case TOK_CONST:
        if (!abortIfSyntaxParser())
            return null();
        // [In] is the default behavior, because for-loops currently specially
        // parse their heads to handle |in| in this situation.
        return lexicalDeclaration(yieldHandling, /* isConst = */ tt == TOK_CONST);

      // ImportDeclaration (only inside modules)
      case TOK_IMPORT:
        return importDeclaration();

      // ExportDeclaration (only inside modules)
      case TOK_EXPORT:
        return exportDeclaration();

      // Miscellaneous error cases arguably better caught here than elsewhere.

      case TOK_CATCH:
        report(ParseError, false, null(), JSMSG_CATCH_WITHOUT_TRY);
        return null();

      case TOK_FINALLY:
        report(ParseError, false, null(), JSMSG_FINALLY_WITHOUT_TRY);
        return null();

      // NOTE: default case handled in the ExpressionStatement section.
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expr(InHandling inHandling, YieldHandling yieldHandling,
                           TripledotHandling tripledotHandling,
                           PossibleError* possibleError,
                           InvokedPrediction invoked)
{
    Node pn = assignExpr(inHandling, yieldHandling, tripledotHandling,
                         possibleError, invoked);
    if (!pn)
        return null();

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_COMMA))
        return null();
    if (!matched)
        return pn;

    Node seq = handler.newCommaExpressionList(pn);
    if (!seq)
        return null();
    while (true) {

        // Additional calls to assignExpr should not reuse the possibleError
        // which had been passed into the function. Otherwise we would lose
        // information needed to determine whether or not we're dealing with
        // a non-recoverable situation.
        PossibleError possibleErrorInner(*this);
        pn = assignExpr(inHandling, yieldHandling, tripledotHandling,
                        &possibleErrorInner);
        if (!pn)
            return null();

        // If we find an error here we should report it immedately instead of
        // passing it back out of the function.
        if (possibleErrorInner.hasError())  {

            // We begin by checking for an outer pending error since it would
            // have occurred first.
            if (possibleError && !possibleError->checkForExprErrors())
                return null();

            // Go ahead and report the inner error.
            possibleErrorInner.checkForExprErrors();
            return null();
        }
        handler.addList(seq, pn);

        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return null();
        if (!matched)
            break;
    }
    return seq;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expr(InHandling inHandling, YieldHandling yieldHandling,
                           TripledotHandling tripledotHandling,
                           InvokedPrediction invoked)
{
    PossibleError possibleError(*this);
    Node pn = expr(inHandling, yieldHandling, tripledotHandling, &possibleError, invoked);
    if (!pn || !possibleError.checkForExprErrors())
        return null();
    return pn;
}

static const JSOp ParseNodeKindToJSOp[] = {
    JSOP_OR,
    JSOP_AND,
    JSOP_BITOR,
    JSOP_BITXOR,
    JSOP_BITAND,
    JSOP_STRICTEQ,
    JSOP_EQ,
    JSOP_STRICTNE,
    JSOP_NE,
    JSOP_LT,
    JSOP_LE,
    JSOP_GT,
    JSOP_GE,
    JSOP_INSTANCEOF,
    JSOP_IN,
    JSOP_LSH,
    JSOP_RSH,
    JSOP_URSH,
    JSOP_ADD,
    JSOP_SUB,
    JSOP_MUL,
    JSOP_DIV,
    JSOP_MOD,
    JSOP_POW
};

static inline JSOp
BinaryOpParseNodeKindToJSOp(ParseNodeKind pnk)
{
    MOZ_ASSERT(pnk >= PNK_BINOP_FIRST);
    MOZ_ASSERT(pnk <= PNK_BINOP_LAST);
    return ParseNodeKindToJSOp[pnk - PNK_BINOP_FIRST];
}

static ParseNodeKind
BinaryOpTokenKindToParseNodeKind(TokenKind tok)
{
    MOZ_ASSERT(TokenKindIsBinaryOp(tok));
    return ParseNodeKind(PNK_BINOP_FIRST + (tok - TOK_BINOP_FIRST));
}

static const int PrecedenceTable[] = {
    1, /* PNK_OR */
    2, /* PNK_AND */
    3, /* PNK_BITOR */
    4, /* PNK_BITXOR */
    5, /* PNK_BITAND */
    6, /* PNK_STRICTEQ */
    6, /* PNK_EQ */
    6, /* PNK_STRICTNE */
    6, /* PNK_NE */
    7, /* PNK_LT */
    7, /* PNK_LE */
    7, /* PNK_GT */
    7, /* PNK_GE */
    7, /* PNK_INSTANCEOF */
    7, /* PNK_IN */
    8, /* PNK_LSH */
    8, /* PNK_RSH */
    8, /* PNK_URSH */
    9, /* PNK_ADD */
    9, /* PNK_SUB */
    10, /* PNK_STAR */
    10, /* PNK_DIV */
    10, /* PNK_MOD */
    11  /* PNK_POW */
};

static const int PRECEDENCE_CLASSES = 11;

static int
Precedence(ParseNodeKind pnk) {
    // Everything binds tighter than PNK_LIMIT, because we want to reduce all
    // nodes to a single node when we reach a token that is not another binary
    // operator.
    if (pnk == PNK_LIMIT)
        return 0;

    MOZ_ASSERT(pnk >= PNK_BINOP_FIRST);
    MOZ_ASSERT(pnk <= PNK_BINOP_LAST);
    return PrecedenceTable[pnk - PNK_BINOP_FIRST];
}

template <typename ParseHandler>
MOZ_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::orExpr1(InHandling inHandling, YieldHandling yieldHandling,
                              TripledotHandling tripledotHandling,
                              PossibleError* possibleError,
                              InvokedPrediction invoked)
{
    // Shift-reduce parser for the binary operator part of the JS expression
    // syntax.

    // Conceptually there's just one stack, a stack of pairs (lhs, op).
    // It's implemented using two separate arrays, though.
    Node nodeStack[PRECEDENCE_CLASSES];
    ParseNodeKind kindStack[PRECEDENCE_CLASSES];
    int depth = 0;
    Node pn;
    for (;;) {
        pn = unaryExpr(yieldHandling, tripledotHandling, possibleError, invoked);
        if (!pn)
            return pn;

        // If a binary operator follows, consume it and compute the
        // corresponding operator.
        TokenKind tok;
        if (!tokenStream.getToken(&tok))
            return null();

        ParseNodeKind pnk;
        if (tok == TOK_IN ? inHandling == InAllowed : TokenKindIsBinaryOp(tok)) {
            // Destructuring defaults are an error in this context
            if (possibleError && !possibleError->checkForExprErrors())
                return null();
            pnk = BinaryOpTokenKindToParseNodeKind(tok);
        } else {
            tok = TOK_EOF;
            pnk = PNK_LIMIT;
        }

        // From this point on, destructuring defaults are definitely an error.
        possibleError = nullptr;

        // If pnk has precedence less than or equal to another operator on the
        // stack, reduce. This combines nodes on the stack until we form the
        // actual lhs of pnk.
        //
        // The >= in this condition works because it is appendOrCreateList's
        // job to decide if the operator in question is left- or
        // right-associative, and build the corresponding tree.
        while (depth > 0 && Precedence(kindStack[depth - 1]) >= Precedence(pnk)) {
            depth--;
            ParseNodeKind combiningPnk = kindStack[depth];
            JSOp combiningOp = BinaryOpParseNodeKindToJSOp(combiningPnk);
            pn = handler.appendOrCreateList(combiningPnk, nodeStack[depth], pn, pc, combiningOp);
            if (!pn)
                return pn;
        }

        if (pnk == PNK_LIMIT)
            break;

        nodeStack[depth] = pn;
        kindStack[depth] = pnk;
        depth++;
        MOZ_ASSERT(depth <= PRECEDENCE_CLASSES);
    }

    MOZ_ASSERT(depth == 0);
    return pn;
}

template <typename ParseHandler>
MOZ_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::condExpr1(InHandling inHandling, YieldHandling yieldHandling,
                                TripledotHandling tripledotHandling,
                                PossibleError* possibleError,
                                InvokedPrediction invoked)
{
    Node condition = orExpr1(inHandling, yieldHandling, tripledotHandling, possibleError, invoked);

    if (!condition || !tokenStream.isCurrentTokenType(TOK_HOOK))
        return condition;

    Node thenExpr = assignExpr(InAllowed, yieldHandling, TripledotProhibited,
                               nullptr /* possibleError */);
    if (!thenExpr)
        return null();

    MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_IN_COND);

    Node elseExpr = assignExpr(inHandling, yieldHandling, TripledotProhibited,
                               nullptr /* possibleError */);
    if (!elseExpr)
        return null();

    // Advance to the next token; the caller is responsible for interpreting it.
    TokenKind ignored;
    if (!tokenStream.getToken(&ignored))
        return null();
    return handler.newConditional(condition, thenExpr, elseExpr);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkAndMarkAsAssignmentLhs(Node target, AssignmentFlavor flavor,
                                                  PossibleError* possibleError)
{
    MOZ_ASSERT(flavor != KeyedDestructuringAssignment,
               "destructuring must use special checking/marking code, not "
               "this method");

    if (handler.isUnparenthesizedDestructuringPattern(target)) {
        if (flavor == CompoundAssignment) {
            report(ParseError, false, null(), JSMSG_BAD_DESTRUCT_ASS);
            return false;
        }

        bool isDestructuring = checkDestructuringPattern(target);
        // Here we've successfully distinguished between destructuring and
        // an object literal. In the case where "CoverInitializedName"
        // syntax was used there will be a pending error that needs clearing.
        if (possibleError && isDestructuring)
            possibleError->setResolved();
        return isDestructuring;
    }

    // All other permitted targets are simple.
    if (!reportIfNotValidSimpleAssignmentTarget(target, flavor))
        return false;

    if (handler.isPropertyAccess(target))
        return true;

    if (handler.isNameAnyParentheses(target)) {
        // The arguments/eval identifiers are simple in non-strict mode code,
        // but warn to discourage use nonetheless.
        if (!reportIfArgumentsEvalTarget(target))
            return false;

        handler.adjustGetToSet(target);
        return true;
    }

    MOZ_ASSERT(handler.isFunctionCall(target));
    return makeSetCall(target, JSMSG_BAD_LEFTSIDE_OF_ASS);
}

class AutoClearInDestructuringDecl
{
    ParseContext* pc_;
    Maybe<DeclarationKind> saved_;

  public:
    explicit AutoClearInDestructuringDecl(ParseContext* pc)
      : pc_(pc),
        saved_(pc->inDestructuringDecl)
    {
        pc->inDestructuringDecl = Nothing();
        if (saved_ && *saved_ == DeclarationKind::FormalParameter)
            pc->functionBox()->hasParameterExprs = true;
    }

    ~AutoClearInDestructuringDecl() {
        pc_->inDestructuringDecl = saved_;
    }
};

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                                 TripledotHandling tripledotHandling,
                                 PossibleError* possibleError,
                                 InvokedPrediction invoked)
{
    JS_CHECK_RECURSION(context, return null());

    // It's very common at this point to have a "detectably simple" expression,
    // i.e. a name/number/string token followed by one of the following tokens
    // that obviously isn't part of an expression: , ; : ) ] }
    //
    // (In Parsemark this happens 81.4% of the time;  in code with large
    // numeric arrays, such as some Kraken benchmarks, it happens more often.)
    //
    // In such cases, we can avoid the full expression parsing route through
    // assignExpr(), condExpr1(), orExpr1(), unaryExpr(), memberExpr(), and
    // primaryExpr().

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    bool endsExpr;

    if (tt == TOK_NAME) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr)
            return identifierName(yieldHandling);
    }

    if (tt == TOK_NUMBER) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr)
            return newNumber(tokenStream.currentToken());
    }

    if (tt == TOK_STRING) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr)
            return stringLiteral();
    }

    if (tt == TOK_YIELD && yieldExpressionsSupported())
        return yieldExpression(inHandling);

    tokenStream.ungetToken();

    // Save the tokenizer state in case we find an arrow function and have to
    // rewind.
    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    PossibleError possibleErrorInner(*this);
    Node lhs = condExpr1(inHandling, yieldHandling, tripledotHandling, &possibleErrorInner, invoked);
    if (!lhs) {
        return null();
    }

    ParseNodeKind kind;
    JSOp op;
    switch (tokenStream.currentToken().type) {
      case TOK_ASSIGN:       kind = PNK_ASSIGN;       op = JSOP_NOP;    break;
      case TOK_ADDASSIGN:    kind = PNK_ADDASSIGN;    op = JSOP_ADD;    break;
      case TOK_SUBASSIGN:    kind = PNK_SUBASSIGN;    op = JSOP_SUB;    break;
      case TOK_BITORASSIGN:  kind = PNK_BITORASSIGN;  op = JSOP_BITOR;  break;
      case TOK_BITXORASSIGN: kind = PNK_BITXORASSIGN; op = JSOP_BITXOR; break;
      case TOK_BITANDASSIGN: kind = PNK_BITANDASSIGN; op = JSOP_BITAND; break;
      case TOK_LSHASSIGN:    kind = PNK_LSHASSIGN;    op = JSOP_LSH;    break;
      case TOK_RSHASSIGN:    kind = PNK_RSHASSIGN;    op = JSOP_RSH;    break;
      case TOK_URSHASSIGN:   kind = PNK_URSHASSIGN;   op = JSOP_URSH;   break;
      case TOK_MULASSIGN:    kind = PNK_MULASSIGN;    op = JSOP_MUL;    break;
      case TOK_DIVASSIGN:    kind = PNK_DIVASSIGN;    op = JSOP_DIV;    break;
      case TOK_MODASSIGN:    kind = PNK_MODASSIGN;    op = JSOP_MOD;    break;
      case TOK_POWASSIGN:    kind = PNK_POWASSIGN;    op = JSOP_POW;    break;

      case TOK_ARROW: {

        // A line terminator between ArrowParameters and the => should trigger a SyntaxError.
        tokenStream.ungetToken();
        TokenKind next = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&next) || next != TOK_ARROW) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "expression", TokenKindToDesc(TOK_ARROW));
            return null();
        }
        tokenStream.consumeKnownToken(TOK_ARROW);

        bool isBlock = false;
        if (!tokenStream.peekToken(&next, TokenStream::Operand))
            return null();
        if (next == TOK_LC)
            isBlock = true;

        tokenStream.seek(start);
        if (!abortIfSyntaxParser())
            return null();

        TokenKind ignored;
        if (!tokenStream.peekToken(&ignored, TokenStream::Operand))
            return null();

        Node arrowFunc = functionDefinition(inHandling, yieldHandling, nullptr,
                                            Arrow, NotGenerator);
        if (!arrowFunc)
            return null();

        if (isBlock) {
            // This arrow function could be a non-trailing member of a comma
            // expression or a semicolon terminating a full expression.  If so,
            // the next token is that comma/semicolon, gotten with None:
            //
            //   a => {}, b; // as if (a => {}), b;
            //   a => {};
            //
            // But if this arrow function ends a statement, ASI permits the
            // next token to start an expression statement.  In that case the
            // next token must be gotten as Operand:
            //
            //   a => {} // complete expression statement
            //   /x/g;   // regular expression as a statement, *not* division
            //
            // Getting the second case right requires the first token-peek
            // after the arrow function use Operand, and that peek must occur
            // before Parser::expr() looks for a comma.  Do so here, then
            // immediately add the modifier exception needed for the first
            // case.
            //
            // Note that the second case occurs *only* if the arrow function
            // has block body.  An arrow function not ending in such, ends in
            // another AssignmentExpression that we can inductively assume was
            // peeked consistently.
            if (!tokenStream.peekToken(&ignored, TokenStream::Operand))
                return null();
            tokenStream.addModifierException(TokenStream::NoneIsOperand);
        }
        return arrowFunc;
      }

      default:
        MOZ_ASSERT(!tokenStream.isCurrentTokenAssignment());
        if (!possibleError) {
            if (!possibleErrorInner.checkForExprErrors())
                return null();
        } else {
            possibleErrorInner.transferErrorTo(possibleError);
        }
        tokenStream.ungetToken();
        return lhs;
    }

    AssignmentFlavor flavor = kind == PNK_ASSIGN ? PlainAssignment : CompoundAssignment;
    if (!checkAndMarkAsAssignmentLhs(lhs, flavor, &possibleErrorInner))
        return null();
    if (!possibleErrorInner.checkForExprErrors())
        return null();

    Node rhs;
    {
        AutoClearInDestructuringDecl autoClear(pc);
        rhs = assignExpr(inHandling, yieldHandling, TripledotProhibited, possibleError);
        if (!rhs)
            return null();
    }

    return handler.newAssignment(kind, lhs, rhs, op);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                                 TripledotHandling tripledotHandling,
                                 InvokedPrediction invoked)
{
    PossibleError possibleError(*this);
    Node expr = assignExpr(inHandling, yieldHandling, tripledotHandling, &possibleError, invoked);
    if (!expr || !possibleError.checkForExprErrors())
        return null();

    return expr;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::isValidSimpleAssignmentTarget(Node node,
                                                    FunctionCallBehavior behavior /* = ForbidAssignmentToFunctionCalls */)
{
    // Note that this method implements *only* a boolean test.  Reporting an
    // error for the various syntaxes that fail this, and warning for the
    // various syntaxes that "pass" this but should not, occurs elsewhere.

    if (handler.isNameAnyParentheses(node)) {
        if (!pc->sc()->strict())
            return true;

        return !handler.nameIsArgumentsEvalAnyParentheses(node, context);
    }

    if (handler.isPropertyAccess(node))
        return true;

    if (behavior == PermitAssignmentToFunctionCalls) {
        if (handler.isFunctionCall(node))
            return true;
    }

    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportIfArgumentsEvalTarget(Node nameNode)
{
    const char* chars = handler.nameIsArgumentsEvalAnyParentheses(nameNode, context);
    if (!chars)
        return true;

    if (!report(ParseStrictError, pc->sc()->strict(), nameNode, JSMSG_BAD_STRICT_ASSIGN, chars))
        return false;

    MOZ_ASSERT(!pc->sc()->strict(),
               "an error should have been reported if this was strict mode "
               "code");
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportIfNotValidSimpleAssignmentTarget(Node target, AssignmentFlavor flavor)
{
    FunctionCallBehavior behavior = flavor == KeyedDestructuringAssignment
                                    ? ForbidAssignmentToFunctionCalls
                                    : PermitAssignmentToFunctionCalls;
    if (isValidSimpleAssignmentTarget(target, behavior))
        return true;

    if (handler.isNameAnyParentheses(target)) {
        // Use a special error if the target is arguments/eval.  This ensures
        // targeting these names is consistently a SyntaxError (which error numbers
        // below don't guarantee) while giving us a nicer error message.
        if (!reportIfArgumentsEvalTarget(target))
            return false;
    }

    unsigned errnum = 0;
    const char* extra = nullptr;

    switch (flavor) {
      case IncrementAssignment:
        errnum = JSMSG_BAD_OPERAND;
        extra = "increment";
        break;

      case DecrementAssignment:
        errnum = JSMSG_BAD_OPERAND;
        extra = "decrement";
        break;

      case KeyedDestructuringAssignment:
        errnum = JSMSG_BAD_DESTRUCT_TARGET;
        break;

      case PlainAssignment:
      case CompoundAssignment:
        errnum = JSMSG_BAD_LEFTSIDE_OF_ASS;
        break;

      case ForInOrOfTarget:
        errnum = JSMSG_BAD_FOR_LEFTSIDE;
        break;
    }

    report(ParseError, pc->sc()->strict(), target, errnum, extra);
    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkAndMarkAsIncOperand(Node target, AssignmentFlavor flavor)
{
    MOZ_ASSERT(flavor == IncrementAssignment || flavor == DecrementAssignment);

    // Check.
    if (!reportIfNotValidSimpleAssignmentTarget(target, flavor))
        return false;

    // Mark.
    if (handler.isNameAnyParentheses(target)) {
        // Assignment to arguments/eval is allowed outside strict mode code,
        // but it's dodgy.  Report a strict warning (error, if werror was set).
        if (!reportIfArgumentsEvalTarget(target))
            return false;
    } else if (handler.isFunctionCall(target)) {
        if (!makeSetCall(target, JSMSG_BAD_INCOP_OPERAND))
            return false;
    }
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryOpExpr(YieldHandling yieldHandling, ParseNodeKind kind, JSOp op,
                                  uint32_t begin)
{
    PossibleError possibleError(*this);
    Node kid = unaryExpr(yieldHandling, TripledotProhibited, &possibleError);
    if (!kid || !possibleError.checkForExprErrors())
        return null();
    return handler.newUnary(kind, op, begin, kid);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                                PossibleError* possibleError, InvokedPrediction invoked)
{
    JS_CHECK_RECURSION(context, return null());

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    uint32_t begin = pos().begin;
    switch (tt) {
      case TOK_VOID:
        return unaryOpExpr(yieldHandling, PNK_VOID, JSOP_VOID, begin);
      case TOK_NOT:
        return unaryOpExpr(yieldHandling, PNK_NOT, JSOP_NOT, begin);
      case TOK_BITNOT:
        return unaryOpExpr(yieldHandling, PNK_BITNOT, JSOP_BITNOT, begin);
      case TOK_ADD:
        return unaryOpExpr(yieldHandling, PNK_POS, JSOP_POS, begin);
      case TOK_SUB:
        return unaryOpExpr(yieldHandling, PNK_NEG, JSOP_NEG, begin);

      case TOK_TYPEOF: {
        // The |typeof| operator is specially parsed to distinguish its
        // application to a name, from its application to a non-name
        // expression:
        //
        //   // Looks up the name, doesn't find it and so evaluates to
        //   // "undefined".
        //   assertEq(typeof nonExistentName, "undefined");
        //
        //   // Evaluates expression, triggering a runtime ReferenceError for
        //   // the undefined name.
        //   typeof (1, nonExistentName);
        Node kid = unaryExpr(yieldHandling, TripledotProhibited, nullptr /* possibleError */);
        if (!kid)
            return null();

        return handler.newTypeof(begin, kid);
      }

      case TOK_INC:
      case TOK_DEC:
      {
        TokenKind tt2;
        if (!tokenStream.getToken(&tt2, TokenStream::Operand))
            return null();
        Node pn2 = memberExpr(yieldHandling, TripledotProhibited, nullptr /* possibleError */,
                              tt2, true);
        if (!pn2)
            return null();
        AssignmentFlavor flavor = (tt == TOK_INC) ? IncrementAssignment : DecrementAssignment;
        if (!checkAndMarkAsIncOperand(pn2, flavor))
            return null();
        return handler.newUnary((tt == TOK_INC) ? PNK_PREINCREMENT : PNK_PREDECREMENT,
                                JSOP_NOP,
                                begin,
                                pn2);
      }

      case TOK_DELETE: {
        Node expr = unaryExpr(yieldHandling, TripledotProhibited, nullptr /* possibleError */);
        if (!expr)
            return null();

        // Per spec, deleting any unary expression is valid -- it simply
        // returns true -- except for one case that is illegal in strict mode.
        if (handler.isNameAnyParentheses(expr)) {
            if (!report(ParseStrictError, pc->sc()->strict(), expr, JSMSG_DEPRECATED_DELETE_OPERAND))
                return null();
            pc->sc()->setBindingsAccessedDynamically();
        }

        return handler.newDelete(begin, expr);
      }

      default: {
        Node pn = memberExpr(yieldHandling, tripledotHandling, possibleError, tt,
                             /* allowCallSyntax = */ true, invoked);
        if (!pn)
            return null();

        /* Don't look across a newline boundary for a postfix incop. */
        if (!tokenStream.peekTokenSameLine(&tt))
            return null();
        if (tt == TOK_INC || tt == TOK_DEC) {
            tokenStream.consumeKnownToken(tt);
            AssignmentFlavor flavor = (tt == TOK_INC) ? IncrementAssignment : DecrementAssignment;
            if (!checkAndMarkAsIncOperand(pn, flavor))
                return null();
            return handler.newUnary((tt == TOK_INC) ? PNK_POSTINCREMENT : PNK_POSTDECREMENT,
                                    JSOP_NOP,
                                    begin,
                                    pn);
        }
        return pn;
      }
    }
}


/*** Comprehensions *******************************************************************************
 *
 * We currently support two flavors of comprehensions, all deprecated:
 *
 *     [for (V of OBJ) if (COND) EXPR]  // ES6-era array comprehension
 *     (for (V of OBJ) if (COND) EXPR)  // ES6-era generator expression
 *
 * (These flavors are called "ES6-era" because they were in ES6 draft
 * specifications for a while. Shortly after this syntax was implemented in SM,
 * TC39 decided to drop it.)
 */

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::generatorComprehensionLambda(unsigned begin)
{
    Node genfn = handler.newFunctionDefinition();
    if (!genfn)
        return null();
    handler.setOp(genfn, JSOP_LAMBDA);

    ParseContext* outerpc = pc;

    // If we are off the main thread, the generator meta-objects have
    // already been created by js::StartOffThreadParseScript, so cx will not
    // be necessary.
    RootedObject proto(context);
    JSContext* cx = context->maybeJSContext();
    proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, context->global());
    if (!proto)
        return null();

    RootedFunction fun(context, newFunction(/* atom = */ nullptr, Expression,
                                            StarGenerator, proto));
    if (!fun)
        return null();

    // Create box for fun->object early to root it.
    Directives directives(/* strict = */ outerpc->sc()->strict());
    FunctionBox* genFunbox = newFunctionBox(genfn, fun, directives, StarGenerator,
                                            /* tryAnnexB = */ false);
    if (!genFunbox)
        return null();
    genFunbox->isGenexpLambda = true;
    genFunbox->initWithEnclosingParseContext(outerpc, Expression);

    ParseContext genpc(this, genFunbox, /* newDirectives = */ nullptr);
    if (!genpc.init())
        return null();
    genpc.functionScope().useAsVarScope(&genpc);

    /*
     * We assume conservatively that any deoptimization flags in pc->sc()
     * come from the kid. So we propagate these flags into genfn. For code
     * simplicity we also do not detect if the flags were only set in the
     * kid and could be removed from pc->sc().
     */
    genFunbox->anyCxFlags = outerpc->sc()->anyCxFlags;
    if (outerpc->isFunctionBox())
        genFunbox->funCxFlags = outerpc->functionBox()->funCxFlags;

    if (!declareDotGeneratorName())
        return null();

    Node body = handler.newStatementList(TokenPos(begin, pos().end));
    if (!body)
        return null();

    Node comp = comprehension(StarGenerator);
    if (!comp)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);

    handler.setBeginPosition(comp, begin);
    handler.setEndPosition(comp, pos().end);
    handler.addStatementToList(body, comp);
    handler.setEndPosition(body, pos().end);
    handler.setBeginPosition(genfn, begin);
    handler.setEndPosition(genfn, pos().end);

    Node generator = newDotGeneratorName();
    if (!generator)
        return null();
    if (!handler.prependInitialYield(body, generator))
        return null();

    if (!propagateFreeNamesAndMarkClosedOverBindings(pc->varScope()))
        return null();
    if (!finishFunction())
        return null();
    if (!leaveInnerFunction(outerpc))
        return null();

    // Note that if we ever start syntax-parsing generators, we will also
    // need to propagate the closed-over variable set to the inner
    // lazyscript.
    if (!handler.setComprehensionLambdaBody(genfn, body))
        return null();

    return genfn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionFor(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    // FIXME: Destructuring binding (bug 980828).

    MUST_MATCH_TOKEN(TOK_NAME, JSMSG_NO_VARIABLE_NAME);
    RootedPropertyName name(context, tokenStream.currentName());
    if (name == context->names().let) {
        report(ParseError, false, null(), JSMSG_LET_COMP_BINDING);
        return null();
    }
    TokenPos namePos = pos();
    Node lhs = newName(name);
    if (!lhs)
        return null();
    bool matched;
    if (!tokenStream.matchContextualKeyword(&matched, context->names().of))
        return null();
    if (!matched) {
        report(ParseError, false, null(), JSMSG_OF_AFTER_FOR_NAME);
        return null();
    }

    Node rhs = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
    if (!rhs)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_OF_ITERABLE);

    TokenPos headPos(begin, pos().end);

    ParseContext::Scope scope(this);
    if (!scope.init(pc))
        return null();

    {
        // Push a temporary ForLoopLexicalHead Statement that allows for
        // lexical declarations, as they are usually allowed only in braced
        // statements.
        ParseContext::Statement forHeadStmt(pc, StatementKind::ForLoopLexicalHead);
        if (!noteDeclaredName(name, DeclarationKind::Let, namePos))
            return null();
    }

    Node decls = handler.newComprehensionBinding(lhs);
    if (!decls)
        return null();

    Node tail = comprehensionTail(comprehensionKind);
    if (!tail)
        return null();

    // Finish the lexical scope after parsing the tail.
    Node lexicalScope = finishLexicalScope(scope, decls);
    if (!lexicalScope)
        return null();

    Node head = handler.newForInOrOfHead(PNK_FOROF, lexicalScope, rhs, headPos);
    if (!head)
        return null();

    return handler.newComprehensionFor(begin, head, tail);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionIf(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_IF));

    uint32_t begin = pos().begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    Node cond = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
    if (!cond)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isUnparenthesizedAssignment(cond)) {
        if (!report(ParseExtraWarning, false, null(), JSMSG_EQUAL_AS_ASSIGN))
            return null();
    }

    Node then = comprehensionTail(comprehensionKind);
    if (!then)
        return null();

    return handler.newIfStatement(begin, cond, then, null());
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionTail(GeneratorKind comprehensionKind)
{
    JS_CHECK_RECURSION(context, return null());

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_FOR, TokenStream::Operand))
        return null();
    if (matched)
        return comprehensionFor(comprehensionKind);

    if (!tokenStream.matchToken(&matched, TOK_IF, TokenStream::Operand))
        return null();
    if (matched)
        return comprehensionIf(comprehensionKind);

    uint32_t begin = pos().begin;

    Node bodyExpr = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
    if (!bodyExpr)
        return null();

    if (comprehensionKind == NotGenerator)
        return handler.newUnary(PNK_ARRAYPUSH, JSOP_ARRAYPUSH, begin, bodyExpr);

    MOZ_ASSERT(comprehensionKind == StarGenerator);
    Node yieldExpr = newYieldExpression(begin, bodyExpr);
    if (!yieldExpr)
        return null();
    yieldExpr = handler.parenthesize(yieldExpr);

    return handler.newExprStatement(yieldExpr, pos().end);
}

// Parse an ES6-era generator or array comprehension, starting at the first
// `for`. The caller is responsible for matching the ending TOK_RP or TOK_RB.
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehension(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    uint32_t startYieldOffset = pc->lastYieldOffset;

    Node body = comprehensionFor(comprehensionKind);
    if (!body)
        return null();

    if (comprehensionKind != NotGenerator && pc->lastYieldOffset != startYieldOffset) {
        reportWithOffset(ParseError, false, pc->lastYieldOffset,
                         JSMSG_BAD_GENEXP_BODY, js_yield_str);
        return null();
    }

    return body;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::arrayComprehension(uint32_t begin)
{
    Node inner = comprehension(NotGenerator);
    if (!inner)
        return null();

    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_ARRAY_COMPREHENSION);

    Node comp = handler.newList(PNK_ARRAYCOMP, inner);
    if (!comp)
        return null();

    handler.setBeginPosition(comp, begin);
    handler.setEndPosition(comp, pos().end);

    return comp;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::generatorComprehension(uint32_t begin)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    // We have no problem parsing generator comprehensions inside lazy
    // functions, but the bytecode emitter currently can't handle them that way,
    // because when it goes to emit the code for the inner generator function,
    // it expects outer functions to have non-lazy scripts.
    if (!abortIfSyntaxParser())
        return null();

    Node genfn = generatorComprehensionLambda(begin);
    if (!genfn)
        return null();

    Node result = handler.newList(PNK_GENEXP, genfn, JSOP_CALL);
    if (!result)
        return null();
    handler.setBeginPosition(result, begin);
    handler.setEndPosition(result, pos().end);

    return result;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExprWithoutYield(YieldHandling yieldHandling, unsigned msg)
{
    uint32_t startYieldOffset = pc->lastYieldOffset;
    Node res = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
    if (res && pc->lastYieldOffset != startYieldOffset) {
        reportWithOffset(ParseError, false, pc->lastYieldOffset,
                         msg, js_yield_str);
        return null();
    }
    return res;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::argumentList(YieldHandling yieldHandling, Node listNode, bool* isSpread)
{
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_RP, TokenStream::Operand))
        return false;
    if (matched) {
        handler.setEndPosition(listNode, pos().end);
        return true;
    }

    while (true) {
        bool spread = false;
        uint32_t begin = 0;
        if (!tokenStream.matchToken(&matched, TOK_TRIPLEDOT, TokenStream::Operand))
            return false;
        if (matched) {
            spread = true;
            begin = pos().begin;
            *isSpread = true;
        }

        Node argNode = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
        if (!argNode)
            return false;
        if (spread) {
            argNode = handler.newUnary(PNK_SPREAD, JSOP_NOP, begin, argNode);
            if (!argNode)
                return false;
        }

        handler.addList(listNode, argNode);

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return false;
        if (!matched)
            break;
    }

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;
    if (tt != TOK_RP) {
        report(ParseError, false, null(), JSMSG_PAREN_AFTER_ARGS);
        return false;
    }
    handler.setEndPosition(listNode, pos().end);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkAndMarkSuperScope()
{
    if (!pc->sc()->allowSuperProperty())
        return false;
    pc->setSuperScopeNeedsHomeObject();
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                                 PossibleError* possibleError, TokenKind tt,
                                 bool allowCallSyntax, InvokedPrediction invoked)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));

    Node lhs;

    JS_CHECK_RECURSION(context, return null());

    /* Check for new expression first. */
    if (tt == TOK_NEW) {
        uint32_t newBegin = pos().begin;
        // Make sure this wasn't a |new.target| in disguise.
        Node newTarget;
        if (!tryNewTarget(newTarget))
            return null();
        if (newTarget) {
            lhs = newTarget;
        } else {
            lhs = handler.newList(PNK_NEW, newBegin, JSOP_NEW);
            if (!lhs)
                return null();

            // Gotten by tryNewTarget
            tt = tokenStream.currentToken().type;
            Node ctorExpr = memberExpr(yieldHandling, TripledotProhibited,
                                       nullptr /* possibleError */, tt, false, PredictInvoked);
            if (!ctorExpr)
                return null();

            handler.addList(lhs, ctorExpr);

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_LP))
                return null();
            if (matched) {
                bool isSpread = false;
                if (!argumentList(yieldHandling, lhs, &isSpread))
                    return null();
                if (isSpread)
                    handler.setOp(lhs, JSOP_SPREADNEW);
            }
        }
    } else if (tt == TOK_SUPER) {
        Node thisName = newThisName();
        if (!thisName)
            return null();
        lhs = handler.newSuperBase(thisName, pos());
        if (!lhs)
            return null();
    } else {
        lhs = primaryExpr(yieldHandling, tripledotHandling, possibleError, tt, invoked);
        if (!lhs)
            return null();
    }

    MOZ_ASSERT_IF(handler.isSuperBase(lhs), tokenStream.isCurrentTokenType(TOK_SUPER));

    while (true) {
        if (!tokenStream.getToken(&tt))
            return null();
        if (tt == TOK_EOF)
            break;

        Node nextMember;
        if (tt == TOK_DOT) {
            if (!tokenStream.getToken(&tt, TokenStream::KeywordIsName))
                return null();
            if (tt == TOK_NAME) {
                PropertyName* field = tokenStream.currentName();
                if (handler.isSuperBase(lhs) && !checkAndMarkSuperScope()) {
                    report(ParseError, false, null(), JSMSG_BAD_SUPERPROP, "property");
                    return null();
                }
                nextMember = handler.newPropertyAccess(lhs, field, pos().end);
                if (!nextMember)
                    return null();
            } else {
                report(ParseError, false, null(), JSMSG_NAME_AFTER_DOT);
                return null();
            }
        } else if (tt == TOK_LB) {
            Node propExpr = expr(InAllowed, yieldHandling, TripledotProhibited, nullptr /* possibleError */);
            if (!propExpr)
                return null();

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);

            if (handler.isSuperBase(lhs) && !checkAndMarkSuperScope()) {
                    report(ParseError, false, null(), JSMSG_BAD_SUPERPROP, "member");
                    return null();
            }
            nextMember = handler.newPropertyByValue(lhs, propExpr, pos().end);
            if (!nextMember)
                return null();
        } else if ((allowCallSyntax && tt == TOK_LP) ||
                   tt == TOK_TEMPLATE_HEAD ||
                   tt == TOK_NO_SUBS_TEMPLATE)
        {
            if (handler.isSuperBase(lhs)) {
                if (!pc->sc()->allowSuperCall()) {
                    report(ParseError, false, null(), JSMSG_BAD_SUPERCALL);
                    return null();
                }

                if (tt != TOK_LP) {
                    report(ParseError, false, null(), JSMSG_BAD_SUPER);
                    return null();
                }

                nextMember = handler.newList(PNK_SUPERCALL, lhs, JSOP_SUPERCALL);
                if (!nextMember)
                    return null();

                // Despite the fact that it's impossible to have |super()| is a
                // generator, we still inherit the yieldHandling of the
                // memberExpression, per spec. Curious.
                bool isSpread = false;
                if (!argumentList(yieldHandling, nextMember, &isSpread))
                    return null();

                if (isSpread)
                    handler.setOp(nextMember, JSOP_SPREADSUPERCALL);

                Node thisName = newThisName();
                if (!thisName)
                    return null();
                return handler.newSetThis(thisName, nextMember);
            }

            if (options().selfHostingMode && handler.isPropertyAccess(lhs)) {
                report(ParseError, false, null(), JSMSG_SELFHOSTED_METHOD_CALL);
                return null();
            }

            nextMember = tt == TOK_LP ? handler.newCall() : handler.newTaggedTemplate();
            if (!nextMember)
                return null();

            JSOp op = JSOP_CALL;
            if (handler.isNameAnyParentheses(lhs)) {
                if (tt == TOK_LP && handler.nameIsEvalAnyParentheses(lhs, context)) {
                    // Select the right EVAL op and flag pc as having a direct eval.
                    op = pc->sc()->strict() ? JSOP_STRICTEVAL : JSOP_EVAL;
                    pc->sc()->setBindingsAccessedDynamically();
                    pc->sc()->setHasDirectEval();

                    /*
                     * In non-strict mode code, direct calls to eval can add
                     * variables to the call object.
                     */
                    if (pc->isFunctionBox() && !pc->sc()->strict())
                        pc->functionBox()->setHasExtensibleScope();

                    // If we're in a method, mark the method as requiring
                    // support for 'super', since direct eval code can use it.
                    // (If we're not in a method, that's fine, so ignore the
                    // return value.)
                    checkAndMarkSuperScope();
                }
            } else if (PropertyName* prop = handler.maybeDottedProperty(lhs)) {
                // Use the JSOP_FUN{APPLY,CALL} optimizations given the right
                // syntax.
                if (prop == context->names().apply) {
                    op = JSOP_FUNAPPLY;
                    if (pc->isFunctionBox())
                        pc->functionBox()->usesApply = true;
                } else if (prop == context->names().call) {
                    op = JSOP_FUNCALL;
                }
            }

            handler.setBeginPosition(nextMember, lhs);
            handler.addList(nextMember, lhs);

            if (tt == TOK_LP) {
                bool isSpread = false;
                if (!argumentList(yieldHandling, nextMember, &isSpread))
                    return null();
                if (isSpread) {
                    if (op == JSOP_EVAL)
                        op = JSOP_SPREADEVAL;
                    else if (op == JSOP_STRICTEVAL)
                        op = JSOP_STRICTSPREADEVAL;
                    else
                        op = JSOP_SPREADCALL;
                }
            } else {
                if (!taggedTemplate(yieldHandling, nextMember, tt))
                    return null();
            }
            handler.setOp(nextMember, op);
        } else {
            tokenStream.ungetToken();
            if (handler.isSuperBase(lhs))
                break;
            return lhs;
        }

        lhs = nextMember;
    }

    if (handler.isSuperBase(lhs)) {
        report(ParseError, false, null(), JSMSG_BAD_SUPER);
        return null();
    }

    return lhs;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling, TokenKind tt,
                                 bool allowCallSyntax, InvokedPrediction invoked)
{
    PossibleError possibleError(*this);
    Node pn = memberExpr(yieldHandling, tripledotHandling, &possibleError, tt,
                         allowCallSyntax, invoked);
    if (!pn || !possibleError.checkForExprErrors())
        return null();
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newName(PropertyName* name)
{
    return newName(name, pos());
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newName(PropertyName* name, TokenPos pos)
{
    return handler.newName(name, pos, context);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::identifierName(YieldHandling yieldHandling)
{
    RootedPropertyName name(context, tokenStream.currentName());
    if (yieldHandling == YieldIsKeyword && name == context->names().yield) {
        report(ParseError, false, null(), JSMSG_RESERVED_ID, "yield");
        return null();
    }

    // If we're inside a function that later becomes a legacy generator, then
    // a |yield| identifier name here will be detected by a subsequent
    // |checkYieldNameValidity| call.
    Node pn = newName(name);
    if (!pn)
        return null();

    if (!pc->inDestructuringDecl && !noteUsedName(name))
        return null();

    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::stringLiteral()
{
    return handler.newStringLiteral(stopStringCompression(), pos());
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::noSubstitutionTemplate()
{
    return handler.newTemplateStringLiteral(stopStringCompression(), pos());
}

template <typename ParseHandler>
JSAtom * Parser<ParseHandler>::stopStringCompression() {
    JSAtom* atom = tokenStream.currentToken().atom();

    // Large strings are fast to parse but slow to compress. Stop compression on
    // them, so we don't wait for a long time for compression to finish at the
    // end of compilation.
    const size_t HUGE_STRING = 50000;
    if (sct && sct->active() && atom->length() >= HUGE_STRING)
        sct->abort();
    return atom;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newRegExp()
{
    MOZ_ASSERT(!options().selfHostingMode);
    // Create the regexp even when doing a syntax parse, to check the regexp's syntax.
    const char16_t* chars = tokenStream.getTokenbuf().begin();
    size_t length = tokenStream.getTokenbuf().length();
    RegExpFlag flags = tokenStream.currentToken().regExpFlags();

    Rooted<RegExpObject*> reobj(context);
    reobj = RegExpObject::create(context, chars, length, flags, &tokenStream, alloc);
    if (!reobj)
        return null();

    return handler.newRegExp(reobj, pos(), *this);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::arrayInitializer(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LB));

    uint32_t begin = pos().begin;
    Node literal = handler.newArrayLiteral(begin);
    if (!literal)
        return null();

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    // Handle an ES6-era array comprehension first.
    if (tt == TOK_FOR)
        return arrayComprehension(begin);

    if (tt == TOK_RB) {
        /*
         * Mark empty arrays as non-constant, since we cannot easily
         * determine their type.
         */
        handler.setListFlag(literal, PNX_NONCONST);
    } else {
        tokenStream.ungetToken();

        uint32_t index = 0;
        TokenStream::Modifier modifier = TokenStream::Operand;
        for (; ; index++) {
            if (index >= NativeObject::MAX_DENSE_ELEMENTS_COUNT) {
                report(ParseError, false, null(), JSMSG_ARRAY_INIT_TOO_BIG);
                return null();
            }

            TokenKind tt;
            if (!tokenStream.peekToken(&tt, TokenStream::Operand))
                return null();
            if (tt == TOK_RB)
                break;

            if (tt == TOK_COMMA) {
                tokenStream.consumeKnownToken(TOK_COMMA, TokenStream::Operand);
                if (!handler.addElision(literal, pos()))
                    return null();
            } else if (tt == TOK_TRIPLEDOT) {
                tokenStream.consumeKnownToken(TOK_TRIPLEDOT, TokenStream::Operand);
                uint32_t begin = pos().begin;
                Node inner = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
                if (!inner)
                    return null();
                if (!handler.addSpreadElement(literal, begin, inner))
                    return null();
            } else {
                Node element = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
                if (!element)
                    return null();
                if (foldConstants && !FoldConstants(context, &element, this))
                    return null();
                handler.addArrayElement(literal, element);
            }

            if (tt != TOK_COMMA) {
                /* If we didn't already match TOK_COMMA in above case. */
                bool matched;
                if (!tokenStream.matchToken(&matched, TOK_COMMA))
                    return null();
                if (!matched) {
                    modifier = TokenStream::None;
                    break;
                }
            }
        }

        MUST_MATCH_TOKEN_MOD(TOK_RB, modifier, JSMSG_BRACKET_AFTER_LIST);
    }
    handler.setEndPosition(literal, pos().end);
    return literal;
}

static JSAtom*
DoubleToAtom(ExclusiveContext* cx, double value)
{
    // This is safe because doubles can not be moved.
    Value tmp = DoubleValue(value);
    return ToAtom<CanGC>(cx, HandleValue::fromMarkedLocation(&tmp));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::propertyName(YieldHandling yieldHandling, Node propList,
                                   PropertyType* propType, MutableHandleAtom propAtom)
{
    TokenKind ltok;
    if (!tokenStream.getToken(&ltok, TokenStream::KeywordIsName))
        return null();

    MOZ_ASSERT(ltok != TOK_RC, "caller should have handled TOK_RC");

    bool isGenerator = false;
    if (ltok == TOK_MUL) {
        isGenerator = true;
        if (!tokenStream.getToken(&ltok, TokenStream::KeywordIsName))
            return null();
    }

    propAtom.set(nullptr);
    Node propName;
    switch (ltok) {
      case TOK_NUMBER:
        propAtom.set(DoubleToAtom(context, tokenStream.currentToken().number()));
        if (!propAtom.get())
            return null();
        propName = newNumber(tokenStream.currentToken());
        if (!propName)
            return null();
        break;

      case TOK_LB:
        propName = computedPropertyName(yieldHandling, propList);
        if (!propName)
            return null();
        break;

      case TOK_NAME: {
        propAtom.set(tokenStream.currentName());
        // Do not look for accessor syntax on generators
        if (isGenerator ||
            !(propAtom.get() == context->names().get ||
              propAtom.get() == context->names().set))
        {
            propName = handler.newObjectLiteralPropertyName(propAtom, pos());
            if (!propName)
                return null();
            break;
        }

        *propType = propAtom.get() == context->names().get ? PropertyType::Getter
                                                           : PropertyType::Setter;

        // We have parsed |get| or |set|. Look for an accessor property
        // name next.
        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::KeywordIsName))
            return null();
        if (tt == TOK_NAME) {
            if (!checkUnescapedName())
                return null();

            tokenStream.consumeKnownToken(TOK_NAME, TokenStream::KeywordIsName);

            propAtom.set(tokenStream.currentName());
            return handler.newObjectLiteralPropertyName(propAtom, pos());
        }
        if (tt == TOK_STRING) {
            if (!checkUnescapedName())
                return null();

            tokenStream.consumeKnownToken(TOK_STRING, TokenStream::KeywordIsName);

            propAtom.set(tokenStream.currentToken().atom());

            uint32_t index;
            if (propAtom->isIndex(&index)) {
                propAtom.set(DoubleToAtom(context, index));
                if (!propAtom.get())
                    return null();
                return handler.newNumber(index, NoDecimal, pos());
            }
            return stringLiteral();
        }
        if (tt == TOK_NUMBER) {
            if (!checkUnescapedName())
                return null();

            tokenStream.consumeKnownToken(TOK_NUMBER, TokenStream::KeywordIsName);

            propAtom.set(DoubleToAtom(context, tokenStream.currentToken().number()));
            if (!propAtom.get())
                return null();
            return newNumber(tokenStream.currentToken());
        }
        if (tt == TOK_LB) {
            if (!checkUnescapedName())
                return null();

            tokenStream.consumeKnownToken(TOK_LB, TokenStream::KeywordIsName);

            return computedPropertyName(yieldHandling, propList);
        }

        // Not an accessor property after all.
        propName = handler.newObjectLiteralPropertyName(propAtom.get(), pos());
        if (!propName)
            return null();
        tokenStream.addModifierException(TokenStream::NoneIsKeywordIsName);
        break;
      }

      case TOK_STRING: {
        propAtom.set(tokenStream.currentToken().atom());
        uint32_t index;
        if (propAtom->isIndex(&index)) {
            propName = handler.newNumber(index, NoDecimal, pos());
            if (!propName)
                return null();
            break;
        }
        propName = stringLiteral();
        if (!propName)
            return null();
        break;
      }

      default:
        report(ParseError, false, null(), JSMSG_BAD_PROP_ID);
        return null();
    }

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    if (tt == TOK_COLON) {
        if (isGenerator) {
            report(ParseError, false, null(), JSMSG_BAD_PROP_ID);
            return null();
        }
        *propType = PropertyType::Normal;
        return propName;
    }

    if (ltok == TOK_NAME && (tt == TOK_COMMA || tt == TOK_RC || tt == TOK_ASSIGN)) {
        if (isGenerator) {
            report(ParseError, false, null(), JSMSG_BAD_PROP_ID);
            return null();
        }
        tokenStream.ungetToken();
        *propType = tt == TOK_ASSIGN ?
                          PropertyType::CoverInitializedName :
                          PropertyType::Shorthand;
        return propName;
    }

    if (tt == TOK_LP) {
        tokenStream.ungetToken();
        *propType = isGenerator ? PropertyType::GeneratorMethod : PropertyType::Method;
        return propName;
    }

    report(ParseError, false, null(), JSMSG_COLON_AFTER_ID);
    return null();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::computedPropertyName(YieldHandling yieldHandling, Node literal)
{
    uint32_t begin = pos().begin;

    Node assignNode;
    {
        // Turn off the inDestructuringDecl flag when parsing computed property
        // names. In short, when parsing 'let {[x + y]: z} = obj;', noteUsedName()
        // should be called on x and y, but not on z. See the comment on
        // Parser<>::checkDestructuringPattern() for details.
        AutoClearInDestructuringDecl autoClear(pc);
        assignNode = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
        if (!assignNode)
            return null();
    }

    MUST_MATCH_TOKEN(TOK_RB, JSMSG_COMP_PROP_UNTERM_EXPR);
    Node propname = handler.newComputedName(assignNode, begin, pos().end);
    if (!propname)
        return null();
    handler.setListFlag(literal, PNX_NONCONST);
    return propname;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::objectLiteral(YieldHandling yieldHandling, PossibleError* possibleError)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    Node literal = handler.newObjectLiteral(pos().begin);
    if (!literal)
        return null();

    bool seenPrototypeMutation = false;
    bool seenCoverInitializedName = false;
    RootedAtom propAtom(context);
    for (;;) {
        TokenKind tt;
        if (!tokenStream.getToken(&tt, TokenStream::KeywordIsName))
            return null();
        if (tt == TOK_RC)
            break;

        tokenStream.ungetToken();

        PropertyType propType;
        Node propName = propertyName(yieldHandling, literal, &propType, &propAtom);
        if (!propName)
            return null();

        if (propType == PropertyType::Normal) {
            Node propExpr = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
            if (!propExpr)
                return null();

            if (foldConstants && !FoldConstants(context, &propExpr, this))
                return null();

            if (propAtom == context->names().proto) {
                if (seenPrototypeMutation) {
                    report(ParseError, false, propName, JSMSG_DUPLICATE_PROPERTY, "__proto__");
                    return null();
                }
                seenPrototypeMutation = true;

                // Note: this occurs *only* if we observe TOK_COLON!  Only
                // __proto__: v mutates [[Prototype]].  Getters, setters,
                // method/generator definitions, computed property name
                // versions of all of these, and shorthands do not.
                uint32_t begin = handler.getPosition(propName).begin;
                if (!handler.addPrototypeMutation(literal, begin, propExpr))
                    return null();
            } else {
                if (!handler.isConstant(propExpr))
                    handler.setListFlag(literal, PNX_NONCONST);

                if (!handler.addPropertyDefinition(literal, propName, propExpr))
                    return null();
            }
        } else if (propType == PropertyType::Shorthand) {
            /*
             * Support, e.g., |var {x, y} = o| as destructuring shorthand
             * for |var {x: x, y: y} = o|, per proposed JS2/ES4 for JS1.8.
             */
            if (!tokenStream.checkForKeyword(propAtom, nullptr))
                return null();

            Node nameExpr = identifierName(yieldHandling);
            if (!nameExpr)
                return null();

            if (!handler.addShorthand(literal, propName, nameExpr))
                return null();
        } else if (propType == PropertyType::CoverInitializedName) {
            /*
             * Support, e.g., |var {x=1, y=2} = o| as destructuring shorthand
             * with default values, as per ES6 12.14.5 (2016/2/4)
             */
            if (!tokenStream.checkForKeyword(propAtom, nullptr))
                return null();

            Node lhs = identifierName(yieldHandling);
            if (!lhs)
                return null();

            tokenStream.consumeKnownToken(TOK_ASSIGN);

            Node rhs;
            {
                // Clearing `inDestructuringDecl` allows name use to be noted
                // in `identifierName`. See bug 1255167.
                AutoClearInDestructuringDecl autoClear(pc);
                rhs = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
                if (!rhs)
                    return null();
            }

            Node propExpr = handler.newAssignment(PNK_ASSIGN, lhs, rhs, JSOP_NOP);
            if (!propExpr)
                return null();

            if (!handler.addPropertyDefinition(literal, propName, propExpr))
                return null();

            if (!abortIfSyntaxParser())
                return null();

            if (!seenCoverInitializedName) {

                // "shorthand default" or "CoverInitializedName" syntax is only
                // valid in the case of destructuring.
                seenCoverInitializedName = true;

                if (!possibleError) {
                    // Destructuring defaults are definitely not allowed in this object literal,
                    // because of something the caller knows about the preceding code.
                    // For example, maybe the preceding token is an operator: `x + {y=z}`.
                    report(ParseError, false, null(), JSMSG_COLON_AFTER_ID);
                    return null();
                }

                // Here we set a pending error so that later in the parse, once we've
                // determined whether or not we're destructuring, the error can be
                // reported or ignored appropriately.
                if (!possibleError->setPending(ParseError, JSMSG_COLON_AFTER_ID, false)) {

                    // Report any previously pending error.
                    possibleError->checkForExprErrors();
                    return null();
                }
            }

        } else {
            // FIXME: Implement ES6 function "name" property semantics
            // (bug 883377).
            RootedAtom funName(context);
            if (!tokenStream.isCurrentTokenType(TOK_RB)) {
                funName = propAtom;

                if (propType == PropertyType::Getter || propType == PropertyType::Setter) {
                    funName = prefixAccessorName(propType, propAtom);
                    if (!funName)
                        return null();
                }
            }

            Node fn = methodDefinition(yieldHandling, propType, funName);
            if (!fn)
                return null();

            JSOp op = JSOpFromPropertyType(propType);
            if (!handler.addObjectMethodDefinition(literal, propName, fn, op))
                return null();
        }

        if (!tokenStream.getToken(&tt))
            return null();
        if (tt == TOK_RC)
            break;
        if (tt != TOK_COMMA) {
            report(ParseError, false, null(), JSMSG_CURLY_AFTER_LIST);
            return null();
        }
    }

    handler.setEndPosition(literal, pos().end);
    return literal;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::methodDefinition(YieldHandling yieldHandling, PropertyType propType,
                                       HandleAtom funName)
{
    FunctionSyntaxKind kind = FunctionSyntaxKindFromPropertyType(propType);
    GeneratorKind generatorKind = GeneratorKindFromPropertyType(propType);
    return functionDefinition(InAllowed, yieldHandling, funName, kind, generatorKind);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::tryNewTarget(Node &newTarget)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_NEW));

    newTarget = null();

    Node newHolder = handler.newPosHolder(pos());
    if (!newHolder)
        return false;

    uint32_t begin = pos().begin;

    // |new| expects to look for an operand, so we will honor that.
    TokenKind next;
    if (!tokenStream.getToken(&next, TokenStream::Operand))
        return false;

    // Don't unget the token, since lookahead cannot handle someone calling
    // getToken() with a different modifier. Callers should inspect currentToken().
    if (next != TOK_DOT)
        return true;

    if (!tokenStream.getToken(&next))
        return false;
    if (next != TOK_NAME || tokenStream.currentName() != context->names().target) {
        report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
               "target", TokenKindToDesc(next));
        return false;
    }

    if (!checkUnescapedName())
        return false;

    if (!pc->sc()->allowNewTarget()) {
        reportWithOffset(ParseError, false, begin, JSMSG_BAD_NEWTARGET);
        return false;
    }

    Node targetHolder = handler.newPosHolder(pos());
    if (!targetHolder)
        return false;

    newTarget = handler.newNewTarget(newHolder, targetHolder);
    return !!newTarget;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::primaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                                  PossibleError* possibleError, TokenKind tt,
                                  InvokedPrediction invoked)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));
    JS_CHECK_RECURSION(context, return null());

    switch (tt) {
      case TOK_FUNCTION:
        return functionExpr(invoked);

      case TOK_CLASS:
        return classDefinition(yieldHandling, ClassExpression, NameRequired);

      case TOK_LB:
        return arrayInitializer(yieldHandling);

      case TOK_LC:
        return objectLiteral(yieldHandling, possibleError);

      case TOK_LP: {
        TokenKind next;
        if (!tokenStream.peekToken(&next, TokenStream::Operand))
            return null();

        if (next == TOK_RP) {
            // Not valid expression syntax, but this is valid in an arrow function
            // with no params: `() => body`.
            tokenStream.consumeKnownToken(next, TokenStream::Operand);

            if (!tokenStream.peekToken(&next))
                return null();
            if (next != TOK_ARROW) {
                report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                       "expression", TokenKindToDesc(TOK_RP));
                return null();
            }

            // Now just return something that will allow parsing to continue.
            // It doesn't matter what; when we reach the =>, we will rewind and
            // reparse the whole arrow function. See Parser::assignExpr.
            return handler.newNullLiteral(pos());
        }

        if (next == TOK_FOR) {
            uint32_t begin = pos().begin;
            tokenStream.consumeKnownToken(next, TokenStream::Operand);
            return generatorComprehension(begin);
        }

        Node expr = exprInParens(InAllowed, yieldHandling, TripledotAllowed, possibleError);
        if (!expr)
            return null();
        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);
        handler.setEndPosition(expr, pos().end);
        return handler.parenthesize(expr);
      }

      case TOK_TEMPLATE_HEAD:
        return templateLiteral(yieldHandling);

      case TOK_NO_SUBS_TEMPLATE:
        return noSubstitutionTemplate();

      case TOK_STRING:
        return stringLiteral();

      case TOK_YIELD:
        if (!checkYieldNameValidity())
            return null();
        MOZ_FALLTHROUGH;
      case TOK_NAME:
        return identifierName(yieldHandling);

      case TOK_REGEXP:
        return newRegExp();

      case TOK_NUMBER:
        return newNumber(tokenStream.currentToken());

      case TOK_TRUE:
        return handler.newBooleanLiteral(true, pos());
      case TOK_FALSE:
        return handler.newBooleanLiteral(false, pos());
      case TOK_THIS: {
        if (pc->isFunctionBox())
            pc->functionBox()->usesThis = true;
        Node thisName = null();
        if (pc->sc()->thisBinding() == ThisBinding::Function) {
            thisName = newThisName();
            if (!thisName)
                return null();
        }
        return handler.newThisLiteral(pos(), thisName);
      }
      case TOK_NULL:
        return handler.newNullLiteral(pos());

      case TOK_TRIPLEDOT: {
        // This isn't valid expression syntax, but it's valid in an arrow
        // function as a trailing rest param: `(a, b, ...rest) => body`.  Check
        // if it's directly under
        // CoverParenthesizedExpressionAndArrowParameterList, and check for a
        // name, closing parenthesis, and arrow, and allow it only if all are
        // present.
        if (tripledotHandling != TripledotAllowed) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "expression", TokenKindToDesc(tt));
            return null();
        }

        TokenKind next;
        if (!tokenStream.getToken(&next))
            return null();
        // FIXME: This fails to handle a rest parameter named |yield| correctly
        //        outside of generators: |var f = (...yield) => 42;| should be
        //        valid code!  When this is fixed, make sure to consult both
        //        |yieldHandling| and |checkYieldNameValidity| for correctness
        //        until legacy generator syntax is removed.
        if (next != TOK_NAME) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "rest argument name", TokenKindToDesc(next));
            return null();
        }

        if (!tokenStream.getToken(&next))
            return null();
        if (next != TOK_RP) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "closing parenthesis", TokenKindToDesc(next));
            return null();
        }

        if (!tokenStream.peekTokenSameLine(&next))
            return null();
        if (next != TOK_ARROW) {
            report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
                   "'=>' after argument list", TokenKindToDesc(next));
            return null();
        }

        tokenStream.ungetToken();  // put back right paren

        // Return an arbitrary expression node. See case TOK_RP above.
        return handler.newNullLiteral(pos());
      }

      default:
        report(ParseError, false, null(), JSMSG_UNEXPECTED_TOKEN,
               "expression", TokenKindToDesc(tt));
        return null();
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::exprInParens(InHandling inHandling, YieldHandling yieldHandling,
                                   TripledotHandling tripledotHandling,
                                   PossibleError* possibleError)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));
    return expr(inHandling, yieldHandling, tripledotHandling, possibleError, PredictInvoked);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::exprInParens(InHandling inHandling, YieldHandling yieldHandling,
                                   TripledotHandling tripledotHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));
    return expr(inHandling, yieldHandling, tripledotHandling, PredictInvoked);
}

template <typename ParseHandler>
void
Parser<ParseHandler>::addTelemetry(JSCompartment::DeprecatedLanguageExtension e)
{
    JSContext* cx = context->maybeJSContext();
    if (!cx)
        return;
    cx->compartment()->addTelemetry(getFilename(), e);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::warnOnceAboutExprClosure()
{
#ifndef RELEASE_BUILD
    JSContext* cx = context->maybeJSContext();
    if (!cx)
        return true;

    if (!cx->compartment()->warnedAboutExprClosure) {
        if (!report(ParseWarning, false, null(), JSMSG_DEPRECATED_EXPR_CLOSURE))
            return false;
        cx->compartment()->warnedAboutExprClosure = true;
    }
#endif
    return true;
}

template class Parser<FullParseHandler>;
template class Parser<SyntaxParseHandler>;

} /* namespace frontend */
} /* namespace js */

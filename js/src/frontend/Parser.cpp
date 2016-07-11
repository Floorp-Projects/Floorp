/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS parser.
 *
 * This is a recursive-descent parser for the JavaScript language specified by
 * "The JavaScript 1.5 Language Specification".  It uses lexical and semantic
 * feedback to disambiguate non-LL(1) structures.  It generates trees of nodes
 * induced by the recursive parsing (not precise syntax trees, see Parser.h).
 * After tree construction, it rewrites trees to fold constants and evaluate
 * compile-time expressions.
 *
 * This parser attempts no error recovery.
 */

#include "frontend/Parser-inl.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jstypes.h"

#include "asmjs/AsmJS.h"
#include "builtin/ModuleObject.h"
#include "builtin/SelfHostingDefines.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/FoldConstants.h"
#include "frontend/ParseMaps.h"
#include "frontend/TokenStream.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseNode-inl.h"
#include "vm/ScopeObject-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::Maybe;

using JS::AutoGCRooter;

JSFunction::AutoParseUsingFunctionBox::AutoParseUsingFunctionBox(ExclusiveContext* cx,
                                                                 frontend::FunctionBox* funbox)
  : fun_(cx, funbox->function()),
    oldEnv_(cx, fun_->environment())
{
    fun_->unsetEnvironment();
    fun_->setFunctionBox(funbox);
    funbox->computeAllowSyntax(fun_);
    funbox->computeInWith(fun_);
    funbox->computeThisBinding(fun_);
}

JSFunction::AutoParseUsingFunctionBox::~AutoParseUsingFunctionBox()
{
    fun_->unsetFunctionBox();
    fun_->initEnvironment(oldEnv_);
}

namespace js {
namespace frontend {

typedef Rooted<StaticBlockScope*> RootedStaticBlockScope;
typedef Handle<StaticBlockScope*> HandleStaticBlockScope;
typedef Rooted<NestedStaticScope*> RootedNestedStaticScope;
typedef Handle<NestedStaticScope*> HandleNestedStaticScope;

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

template <>
bool
ParseContext<FullParseHandler>::checkLocalsOverflow(TokenStream& ts)
{
    if (vars_.length() + bodyLevelLexicals_.length() >= LOCALNO_LIMIT ||
        bodyLevelLexicals_.length() >= Bindings::BODY_LEVEL_LEXICAL_LIMIT)
    {
        ts.reportError(JSMSG_TOO_MANY_LOCALS);
        return false;
    }
    return true;
}

static void
MarkUsesAsHoistedLexical(ParseNode* pn)
{
    Definition* dn = &pn->as<Definition>();
    ParseNode** pnup = &dn->dn_uses;
    ParseNode* pnu;
    unsigned start = pn->pn_blockid;

    // In ES6, lexical bindings cannot be accessed until initialized.
    // Distinguish hoisted uses as a different JSOp for easier compilation.
    while ((pnu = *pnup) != nullptr && pnu->pn_blockid >= start) {
        MOZ_ASSERT(pnu->isUsed());
        pnu->pn_dflags |= PND_LEXICAL;
        pnup = &pnu->pn_link;
    }
}

void
SharedContext::computeAllowSyntax(JSObject* staticScope)
{
    for (StaticScopeIter<CanGC> it(context, staticScope); !it.done(); it++) {
        if (it.type() == StaticScopeIter<CanGC>::Function && !it.fun().isArrow()) {
            // Any function supports new.target.
            allowNewTarget_ = true;
            allowSuperProperty_ = it.fun().allowSuperProperty();
            if (it.maybeFunctionBox()) {
                superScopeAlreadyNeedsHomeObject_ = it.maybeFunctionBox()->needsHomeObject();
                allowSuperCall_ = it.maybeFunctionBox()->isDerivedClassConstructor();
            } else {
                allowSuperCall_ = it.fun().isDerivedClassConstructor();
            }
            break;
        }
    }
}

void
SharedContext::computeThisBinding(JSObject* staticScope)
{
    for (StaticScopeIter<CanGC> it(context, staticScope); !it.done(); it++) {
        if (it.type() == StaticScopeIter<CanGC>::Module) {
            thisBinding_ = ThisBinding::Module;
            return;
        }

        if (it.type() == StaticScopeIter<CanGC>::Function) {
            // Arrow functions and generator expression lambdas don't have
            // their own `this` binding.
            if (it.fun().isArrow())
                continue;
            bool isDerived;
            if (it.maybeFunctionBox()) {
                if (it.maybeFunctionBox()->inGenexpLambda)
                    continue;
                isDerived = it.maybeFunctionBox()->isDerivedClassConstructor();
            } else {
                if (it.fun().nonLazyScript()->isGeneratorExp())
                    continue;
                isDerived = it.fun().isDerivedClassConstructor();
            }

            // Derived class constructors (including nested arrow functions and
            // eval) need TDZ checks when accessing |this|.
            if (isDerived)
                needsThisTDZChecks_ = true;

            thisBinding_ = ThisBinding::Function;
            return;
        }
    }

    thisBinding_ = ThisBinding::Global;
}

void
SharedContext::computeInWith(JSObject* staticScope)
{
    for (StaticScopeIter<CanGC> it(context, staticScope); !it.done(); it++) {
        if (it.type() == StaticScopeIter<CanGC>::With) {
            inWith_ = true;
            break;
        }
    }
}

void
SharedContext::markSuperScopeNeedsHomeObject()
{
    MOZ_ASSERT(allowSuperProperty());

    if (superScopeAlreadyNeedsHomeObject_)
        return;

    for (StaticScopeIter<CanGC> it(context, staticScope()); !it.done(); it++) {
        if (it.type() == StaticScopeIter<CanGC>::Function && !it.fun().isArrow()) {
            MOZ_ASSERT(it.fun().allowSuperProperty());
            // If we are still emitting the outer function that needs a home
            // object, mark it as needing one. Otherwise, we must be emitting
            // an eval script, and the outer function must already be marked
            // as needing a home object since it contains an eval.
            if (it.maybeFunctionBox())
                it.maybeFunctionBox()->setNeedsHomeObject();
            else
                MOZ_ASSERT(it.fun().nonLazyScript()->needsHomeObject());
            superScopeAlreadyNeedsHomeObject_ = true;
            return;
        }
    }
    MOZ_CRASH("Must have found an enclosing function box scope that allows super.property");
}

// See comment on member function declaration.
template <>
bool
ParseContext<FullParseHandler>::define(TokenStream& ts, HandlePropertyName name, ParseNode* pn,
                                       Definition::Kind kind, bool declaringVarInCatchBody)
{
    MOZ_ASSERT(!pn->isUsed());
    MOZ_ASSERT_IF(pn->isDefn(), pn->isPlaceholder());
    MOZ_ASSERT_IF(declaringVarInCatchBody, kind == Definition::VAR);

    pn->setDefn(true);

    Definition* prevDef = nullptr;
    if (kind == Definition::LET || kind == Definition::CONSTANT)
        prevDef = decls_.lookupFirst(name);
    else if (declaringVarInCatchBody)
        MOZ_ASSERT(decls_.lookupLast(name)->kind() != Definition::VAR);
    else
        MOZ_ASSERT(!decls_.lookupFirst(name));

    if (!prevDef)
        prevDef = lexdeps.lookupDefn<FullParseHandler>(name);

    if (prevDef) {
        ParseNode** pnup = &prevDef->dn_uses;
        ParseNode* pnu;
        unsigned start = (kind == Definition::LET || kind == Definition::CONSTANT)
                         ? pn->pn_blockid : bodyid;

        while ((pnu = *pnup) != nullptr && pnu->pn_blockid >= start) {
            MOZ_ASSERT(pnu->pn_blockid >= bodyid);
            MOZ_ASSERT(pnu->isUsed());
            pnu->pn_lexdef = &pn->as<Definition>();
            pn->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
            pnup = &pnu->pn_link;
        }

        if (!pnu || pnu != prevDef->dn_uses) {
            *pnup = pn->dn_uses;
            pn->dn_uses = prevDef->dn_uses;
            prevDef->dn_uses = pnu;

            if (!pnu && prevDef->isPlaceholder())
                lexdeps->remove(name);
        }

        pn->pn_dflags |= prevDef->pn_dflags & PND_CLOSED;
    }

    MOZ_ASSERT_IF(kind != Definition::LET && kind != Definition::CONSTANT, !lexdeps->lookup(name));
    pn->pn_dflags &= ~PND_PLACEHOLDER;
    if (kind == Definition::CONSTANT)
        pn->pn_dflags |= PND_CONST;

    Definition* dn = &pn->as<Definition>();
    switch (kind) {
      case Definition::ARG:
        MOZ_ASSERT(sc->isFunctionBox());
        dn->setOp((CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETARG : JSOP_GETARG);
        dn->pn_blockid = bodyid;
        dn->pn_dflags |= PND_BOUND;
        if (!dn->pn_scopecoord.setSlot(ts, args_.length()))
            return false;
        if (!args_.append(dn))
            return false;
        if (args_.length() >= ARGNO_LIMIT) {
            ts.reportError(JSMSG_TOO_MANY_FUN_ARGS);
            return false;
        }
        if (name == ts.names().empty)
            break;
        if (!decls_.addUnique(name, dn))
            return false;
        break;

      case Definition::VAR:
        // Unlike args, var bindings keep the blockid of where the statement
        // was found until ParseContext::generateBindings. In practice, this
        // means when we emit bytecode for function scripts, var Definition
        // nodes will have their static scopes correctly set to the static
        // scope of the body. For global scripts, vars are dynamically defined
        // on the global object and their static scope is never consulted.
        if (!vars_.append(dn))
            return false;

        // We always track vars for redeclaration checks, but only non-global
        // and non-deoptimized (e.g., inside a with scope) vars live in frame
        // or CallObject slots.
        if (!sc->isGlobalContext() && !dn->isDeoptimized()) {
            dn->setOp((CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETLOCAL : JSOP_GETLOCAL);
            dn->pn_dflags |= PND_BOUND;
            if (!dn->pn_scopecoord.setSlot(ts, vars_.length() - 1))
                return false;
            if (!checkLocalsOverflow(ts))
                return false;
        }

        // Set when declaring a 'var' binding despite a shadowing lexical binding
        // of the same name already existing as a catch parameter. Covered by ES6
        // Annex B.3.5. Also see note in Parser::bindVar
        if (declaringVarInCatchBody) {
            if (!decls_.addShadowedForAnnexB(name, dn))
                return false;
        } else {
            if (!decls_.addUnique(name, dn))
                return false;
        }

        break;

      case Definition::LET:
      case Definition::CONSTANT:
        // See FullParseHandler::setLexicalDeclarationOp.
        dn->setOp(dn->pn_scopecoord.isFree() ? JSOP_INITGLEXICAL : JSOP_INITLEXICAL);
        dn->pn_dflags |= (PND_LEXICAL | PND_BOUND);
        if (atBodyLevel()) {
            if (!bodyLevelLexicals_.append(dn))
                return false;
            if (!addBodyLevelLexicallyDeclaredName(ts, name))
                return false;
            if (!checkLocalsOverflow(ts))
                return false;
        }

        // In ES6, lexical bindings cannot be accessed until initialized. If
        // the definition has existing uses, they need to be marked so that we
        // emit dead zone checks.
        MarkUsesAsHoistedLexical(pn);

        if (!decls_.addShadow(name, dn))
            return false;
        break;

      case Definition::IMPORT:
        dn->pn_dflags |= PND_LEXICAL;
        MOZ_ASSERT(atBodyLevel());
        if (!decls_.addShadow(name, dn))
            return false;
        break;

      default:
        MOZ_CRASH("unexpected kind");
    }

    return true;
}

template <>
bool
ParseContext<SyntaxParseHandler>::checkLocalsOverflow(TokenStream& ts)
{
    return true;
}

template <>
bool
ParseContext<SyntaxParseHandler>::define(TokenStream& ts, HandlePropertyName name, Node pn,
                                         Definition::Kind kind, bool declaringVarInCatchBody)
{
    MOZ_ASSERT(!decls_.lookupFirst(name));

    if (lexdeps.lookupDefn<SyntaxParseHandler>(name))
        lexdeps->remove(name);

    // Keep track of the number of arguments in args_, for fun->nargs.
    if (kind == Definition::ARG) {
        if (!args_.append((Definition*) nullptr))
            return false;
        if (args_.length() >= ARGNO_LIMIT) {
            ts.reportError(JSMSG_TOO_MANY_FUN_ARGS);
            return false;
        }
    }

    return decls_.addUnique(name, kind);
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::prepareToAddDuplicateArg(HandlePropertyName name, DefinitionNode prevDecl)
{
    MOZ_ASSERT(decls_.lookupFirst(name) == prevDecl);
    decls_.remove(name);
}

template <typename ParseHandler>
bool
ParseContext<ParseHandler>::updateDecl(TokenStream& ts, JSAtom* atom, Node pn)
{
    Definition* oldDecl = decls_.lookupFirst(atom);

    pn->setDefn(true);
    Definition* newDecl = &pn->template as<Definition>();
    decls_.updateFirst(atom, newDecl);

    if (oldDecl->isOp(JSOP_INITLEXICAL)) {
        // XXXshu Special case used only for phasing in block-scope function
        // XXXshu early errors.
        // XXXshu
        // XXXshu Back out when major version >= 50. See [1].
        // XXXshu
        // XXXshu [1] https://bugzilla.mozilla.org/show_bug.cgi?id=1235590#c10
        MOZ_ASSERT(oldDecl->getKind() == PNK_FUNCTION);
        MOZ_ASSERT(newDecl->getKind() == PNK_FUNCTION);
        MOZ_ASSERT(!sc->strict());
        MOZ_ASSERT(oldDecl->isBound());
        MOZ_ASSERT(!oldDecl->pn_scopecoord.isFree());
        newDecl->pn_scopecoord = oldDecl->pn_scopecoord;
        newDecl->pn_dflags |= PND_BOUND;
        newDecl->setOp(JSOP_INITLEXICAL);
        return true;
    }

    if (sc->isGlobalContext() || oldDecl->isDeoptimized()) {
        MOZ_ASSERT(newDecl->isFreeVar());
        // Global 'var' bindings have no slots, but are still tracked for
        // redeclaration checks.
        for (uint32_t i = 0; i < vars_.length(); i++) {
            if (vars_[i] == oldDecl) {
                // Terribly, deoptimized bindings may be updated with
                // optimized bindings due to hoisted function statements, so
                // give the new declaration a slot.
                //
                // Global bindings are excluded as currently they are never
                // frame slots. The notion of being deoptimized is not
                // applicable to them.
                if (oldDecl->isDeoptimized() && !newDecl->isDeoptimized() &&
                    !sc->isGlobalContext())
                {
                    newDecl->pn_dflags |= PND_BOUND;
                    if (!newDecl->pn_scopecoord.setSlot(ts, i)) {
                        return false;
                    }
                    newDecl->setOp(JSOP_GETLOCAL);
                }
                vars_[i] = newDecl;
                break;
            }
        }
        return true;
    }

    MOZ_ASSERT(oldDecl->isBound());
    MOZ_ASSERT(!oldDecl->pn_scopecoord.isFree());
    newDecl->pn_scopecoord = oldDecl->pn_scopecoord;
    newDecl->pn_dflags |= PND_BOUND;
    if (IsArgOp(oldDecl->getOp())) {
        newDecl->setOp(JSOP_GETARG);
        MOZ_ASSERT(args_[oldDecl->pn_scopecoord.slot()] == oldDecl);
        args_[oldDecl->pn_scopecoord.slot()] = newDecl;
    } else {
        MOZ_ASSERT(IsLocalOp(oldDecl->getOp()));
        newDecl->setOp(JSOP_GETLOCAL);
        MOZ_ASSERT(vars_[oldDecl->pn_scopecoord.slot()] == oldDecl);
        vars_[oldDecl->pn_scopecoord.slot()] = newDecl;
    }
    return true;
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::popLetDecl(JSAtom* atom)
{
    MOZ_ASSERT(ParseHandler::getDefinitionKind(decls_.lookupFirst(atom)) == Definition::LET ||
               ParseHandler::getDefinitionKind(decls_.lookupFirst(atom)) == Definition::CONSTANT);
    decls_.remove(atom);
}

template <typename ParseHandler>
static void
AppendPackedBindings(const ParseContext<ParseHandler>* pc, const DeclVector& vec, Binding* dst,
                     uint32_t* numUnaliased = nullptr)
{
    for (size_t i = 0; i < vec.length(); ++i, ++dst) {
        Definition* dn = vec[i];
        PropertyName* name = dn->name();

        Binding::Kind kind;
        switch (dn->kind()) {
          case Definition::LET:
            // Treat body-level let declarations as var bindings by falling
            // through. The fact that the binding is in fact a let declaration
            // is reflected in the slot. All body-level lets go after the
            // vars.
          case Definition::VAR:
            kind = Binding::VARIABLE;
            break;
          case Definition::CONSTANT:
            kind = Binding::CONSTANT;
            break;
          case Definition::ARG:
            kind = Binding::ARGUMENT;
            break;
          case Definition::IMPORT:
            // Skip module imports.
            continue;
          default:
            MOZ_CRASH("unexpected dn->kind");
        }

        bool aliased;
        if (pc->sc->isGlobalContext()) {
            // Bindings for global and eval scripts are used solely for redeclaration
            // checks in the prologue. Neither 'true' nor 'false' accurately describe
            // their aliased-ness. These bindings don't live in CallObjects or the
            // frame, but either on the global object and the global lexical
            // scope. Force aliased to be false to avoid confusing other analyses in
            // the engine that assumes the frame has a call object if there are
            // aliased bindings.
            aliased = false;
        } else {
            /*
             * Bindings::init does not check for duplicates so we must ensure that
             * only one binding with a given name is marked aliased. pc->decls
             * maintains the canonical definition for each name, so use that.
             */
            MOZ_ASSERT_IF(dn->isClosed(), pc->decls().lookupFirst(name) == dn);
            aliased = dn->isClosed() ||
                      (pc->sc->allLocalsAliased() &&
                       pc->decls().lookupFirst(name) == dn);
        }

        *dst = Binding(name, kind, aliased);
        if (!aliased && numUnaliased)
            ++*numUnaliased;
    }
}

template <typename ParseHandler>
bool
ParseContext<ParseHandler>::generateBindings(ExclusiveContext* cx, TokenStream& ts, LifoAlloc& alloc,
                                             MutableHandle<Bindings> bindings) const
{
    MOZ_ASSERT_IF(sc->isFunctionBox(), args_.length() < ARGNO_LIMIT);
    MOZ_ASSERT_IF(sc->isModuleBox(), args_.length() == 0);
    MOZ_ASSERT(vars_.length() + bodyLevelLexicals_.length() < LOCALNO_LIMIT);

    /*
     * Avoid pathological edge cases by explicitly limiting the total number of
     * bindings to what will fit in a uint32_t.
     */
    if (UINT32_MAX - args_.length() <= vars_.length() + bodyLevelLexicals_.length())
        return ts.reportError(JSMSG_TOO_MANY_LOCALS);

    if (blockScopeDepth >= Bindings::BLOCK_SCOPED_LIMIT)
        return ts.reportError(JSMSG_TOO_MANY_LOCALS);

    // Fix up slots in non-global contexts. In global contexts all body-level
    // names are dynamically defined and do not live in either frame or
    // CallObject slots.
    if (!sc->isGlobalContext()) {
        // Fix up the blockids of vars, whose static scope is always at the body
        // level. This could not be done up front in ParseContext::define, as
        // the original blockids are used for redeclaration checks.
        for (size_t i = 0; i < vars_.length(); i++)
            vars_[i]->pn_blockid = bodyid;

        // Fix up the slots of body-level lets to come after the vars now that we
        // know how many vars there are.
        for (size_t i = 0; i < bodyLevelLexicals_.length(); i++) {
            Definition* dn = bodyLevelLexicals_[i];
            if (!dn->pn_scopecoord.setSlot(ts, vars_.length() + i))
                return false;
        }
    }

    uint32_t count = args_.length() + vars_.length() + bodyLevelLexicals_.length();
    Binding* packedBindings = alloc.newArrayUninitialized<Binding>(count);
    if (!packedBindings) {
        ReportOutOfMemory(cx);
        return false;
    }

    uint32_t numUnaliasedVars = 0;
    uint32_t numUnaliasedBodyLevelLexicals = 0;

    AppendPackedBindings(this, args_, packedBindings);
    AppendPackedBindings(this, vars_, packedBindings + args_.length(), &numUnaliasedVars);
    AppendPackedBindings(this, bodyLevelLexicals_,
                         packedBindings + args_.length() + vars_.length(), &numUnaliasedBodyLevelLexicals);

    return Bindings::initWithTemporaryStorage(cx, bindings, args_.length(), vars_.length(),
                                              bodyLevelLexicals_.length(), blockScopeDepth,
                                              numUnaliasedVars, numUnaliasedBodyLevelLexicals,
                                              packedBindings, sc->isModuleBox());
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
Parser<ParseHandler>::Parser(ExclusiveContext* cx, LifoAlloc* alloc,
                             const ReadOnlyCompileOptions& options,
                             const char16_t* chars, size_t length,
                             bool foldConstants,
                             Parser<SyntaxParseHandler>* syntaxParser,
                             LazyScript* lazyOuterFunction)
  : AutoGCRooter(cx, PARSER),
    context(cx),
    alloc(*alloc),
    tokenStream(cx, options, chars, length, thisForCtor()),
    traceListHead(nullptr),
    pc(nullptr),
    blockScopes(cx),
    sct(nullptr),
    ss(nullptr),
    keepAtoms(cx->perThreadData),
    foldConstants(foldConstants),
#ifdef DEBUG
    checkOptionsCalled(false),
#endif
    abortedSyntaxParse(false),
    isUnexpectedEOF_(false),
    handler(cx, *alloc, tokenStream, syntaxParser, lazyOuterFunction)
{
    {
        AutoLockForExclusiveAccess lock(cx);
        cx->perThreadData->addActiveCompilation(lock);
    }

    // The Mozilla specific JSOPTION_EXTRA_WARNINGS option adds extra warnings
    // which are not generated if functions are parsed lazily. Note that the
    // standard "use strict" does not inhibit lazy parsing.
    if (options.extraWarningsOption)
        handler.disableSyntaxParser();

    tempPoolMark = alloc->mark();
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

    {
        AutoLockForExclusiveAccess lock(context);
        context->perThreadData->removeActiveCompilation(lock);
    }
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
FunctionBox::FunctionBox(ExclusiveContext* cx, ObjectBox* traceListHead, JSFunction* fun,
                         JSObject* enclosingStaticScope, ParseContext<ParseHandler>* outerpc,
                         Directives directives, bool extraWarnings, GeneratorKind generatorKind)
  : ObjectBox(fun, traceListHead),
    SharedContext(cx, directives, extraWarnings),
    bindings(),
    enclosingStaticScope_(enclosingStaticScope),
    bufStart(0),
    bufEnd(0),
    startLine(1),
    startColumn(0),
    length(0),
    generatorKindBits_(GeneratorKindAsBits(generatorKind)),
    inGenexpLambda(false),
    hasDestructuringArgs(false),
    useAsm(false),
    insideUseAsm(outerpc && outerpc->useAsmOrInsideUseAsm()),
    wasEmitted(false),
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

template <typename ParseHandler>
FunctionBox*
Parser<ParseHandler>::newFunctionBox(Node fn, JSFunction* fun,
                                     ParseContext<ParseHandler>* outerpc,
                                     Directives inheritedDirectives,
                                     GeneratorKind generatorKind,
                                     JSObject* enclosingStaticScope)
{
    MOZ_ASSERT_IF(outerpc, enclosingStaticScope == outerpc->innermostStaticScope());
    MOZ_ASSERT(fun);

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    FunctionBox* funbox =
        alloc.new_<FunctionBox>(context, traceListHead, fun, enclosingStaticScope, outerpc,
                                inheritedDirectives, options().extraWarningsOption,
                                generatorKind);
    if (!funbox) {
        ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = funbox;
    if (fn)
        handler.setFunctionBox(fn, funbox);

    return funbox;
}

template <typename ParseHandler>
ModuleBox::ModuleBox(ExclusiveContext* cx, ObjectBox* traceListHead, ModuleObject* module,
                     ModuleBuilder& builder, ParseContext<ParseHandler>* outerpc)
  : ObjectBox(module, traceListHead),
    SharedContext(cx, Directives(true), false),
    bindings(),
    builder(builder)
{
    computeThisBinding(staticScope());
}

template <typename ParseHandler>
ModuleBox*
Parser<ParseHandler>::newModuleBox(Node pn, HandleModuleObject module, ModuleBuilder& builder)
{
    MOZ_ASSERT(module);

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole module.
     */
    ParseContext<ParseHandler>* outerpc = nullptr;
    ModuleBox* modbox =
        alloc.new_<ModuleBox>(context, traceListHead, module, builder, outerpc);
    if (!modbox) {
        ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = modbox;
    if (pn)
        handler.setModuleBox(pn, modbox);

    return modbox;
}

template <>
ModuleBox*
Parser<SyntaxParseHandler>::newModuleBox(Node pn, HandleModuleObject module, ModuleBuilder& builder)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return nullptr;
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

    /*
     * Protect atoms from being collected by a GC activation, which might
     * - nest on this thread due to out of memory (the so-called "last ditch"
     *   GC attempted within js_NewGCThing), or
     * - run for any reason on another thread if this thread is suspended on
     *   an object lock before it finishes generating bytecode into a script
     *   protected from the GC by a root or a stack frame reference.
     */
    Rooted<StaticScope*> staticLexical(context, &context->global()->lexicalScope().staticBlock());
    Directives directives(options().strictOption);
    GlobalSharedContext globalsc(context, staticLexical, directives,
                                 options().extraWarningsOption);
    ParseContext<ParseHandler> globalpc(this, /* parent = */ nullptr, ParseHandler::null(),
                                        &globalsc, /* newDirectives = */ nullptr);
    if (!globalpc.init(*this))
        return null();

    Node pn = statements(YieldIsName);
    if (pn) {
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
    }
    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportBadReturn(Node pn, ParseReportKind kind,
                                      unsigned errnum, unsigned anonerrnum)
{
    JSAutoByteString name;
    if (JSAtom* atom = pc->sc->asFunctionBox()->function()->name()) {
        if (!AtomToPrintableString(context, atom, &name))
            return false;
    } else {
        errnum = anonerrnum;
    }
    return report(kind, pc->sc->strict(), pn, errnum, name.ptr());
}

/*
 * Check that it is permitted to introduce a binding for atom.  Strict mode
 * forbids introducing new definitions for 'eval', 'arguments', or for any
 * strict mode reserved keyword.  Use pn for reporting error locations, or use
 * pc's token stream if pn is nullptr.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::checkStrictBinding(PropertyName* name, Node pn)
{
    if (!pc->sc->needStrictChecks())
        return true;

    if (name == context->names().eval || name == context->names().arguments || IsKeyword(name)) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(context, name, &bytes))
            return false;
        return report(ParseStrictError, pc->sc->strict(), pn,
                      JSMSG_BAD_BINDING, bytes.ptr());
    }

    return true;
}

template <>
ParseNode*
Parser<FullParseHandler>::standaloneModule(HandleModuleObject module, ModuleBuilder& builder)
{
    MOZ_ASSERT(checkOptionsCalled);

    Node mn = handler.newModule();
    if (!mn)
        return null();

    ModuleBox* modulebox = newModuleBox(mn, module, builder);
    if (!modulebox)
        return null();
    handler.setModuleBox(mn, modulebox);

    ParseContext<FullParseHandler> modulepc(this, pc, mn, modulebox, nullptr);
    if (!modulepc.init(*this))
        return null();

    ParseNode* pn = statements(YieldIsKeyword);
    if (!pn)
        return null();

    pn->pn_blockid = modulepc.blockid();

    MOZ_ASSERT(pn->isKind(PNK_STATEMENTLIST));
    mn->pn_body = pn;

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    if (tt != TOK_EOF) {
        report(ParseError, false, null(), JSMSG_GARBAGE_AFTER_INPUT, "module", TokenKindToDesc(tt));
        return null();
    }

    if (!builder.buildTables())
        return null();

    // Check exported local bindings exist and mark them as closed over.
    for (auto entry : modulebox->builder.localExportEntries()) {
        JSAtom* name = entry->localName();
        MOZ_ASSERT(name);

        Definition* def = modulepc.decls().lookupFirst(name);
        if (!def) {
            JSAutoByteString str;
            if (!str.encodeLatin1(context, name))
                return null();

            JS_ReportErrorNumber(context->asJSContext(), GetErrorMessage, nullptr,
                                 JSMSG_MISSING_EXPORT, str.ptr());
            return null();
        }

        def->pn_dflags |= PND_CLOSED;
    }

    if (!FoldConstants(context, &pn, this))
        return null();

    Rooted<Bindings> bindings(context, modulebox->bindings);
    if (!modulepc.generateBindings(context, tokenStream, alloc, &bindings))
        return null();
    modulebox->bindings = bindings;

    MOZ_ASSERT(mn->pn_modulebox == modulebox);
    return mn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::standaloneModule(HandleModuleObject module, ModuleBuilder& builder)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <>
bool
Parser<FullParseHandler>::checkStatementsEOF()
{
    // This is designed to be paired with parsing a statement list at the top
    // level.
    //
    // The statements() call breaks on TOK_RC, so make sure we've
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

template <>
ParseNode*
Parser<FullParseHandler>::evalBody()
{
    AutoPushStmtInfoPC stmtInfo(*this, StmtType::BLOCK);
    ParseNode* block = pushLexicalScope(stmtInfo);
    if (!block)
        return nullptr;

    // For parsing declarations and directives, eval scripts must be
    // considered body level despite having a lexical scope.
    MOZ_ASSERT(pc->atBodyLevel());

    ParseNode* body = statements(YieldIsName);
    if (!body)
        return nullptr;

    if (!checkStatementsEOF())
        return nullptr;

    block->pn_expr = body;
    block->pn_pos = body->pn_pos;
    return block;
}

template <>
ParseNode*
Parser<FullParseHandler>::globalBody()
{
    MOZ_ASSERT(pc->atGlobalLevel());

    ParseNode* body = statements(YieldIsName);
    if (!body)
        return nullptr;

    if (!checkStatementsEOF())
        return nullptr;

    return body;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newThisName()
{
    Node thisName = newName(context->names().dotThis);
    if (!thisName)
        return null();
    if (!noteNameUse(context->names().dotThis, thisName))
        return null();
    return thisName;
}

template <>
bool
Parser<FullParseHandler>::defineFunctionThis()
{
    HandlePropertyName dotThis = context->names().dotThis;

    // Create a declaration for '.this' if there are any unbound uses in the
    // function body.
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        if (r.front().key() == dotThis) {
            Definition* dn = r.front().value().get<FullParseHandler>();
            MOZ_ASSERT(dn->isPlaceholder());
            pc->sc->asFunctionBox()->setHasThisBinding();
            return pc->define(tokenStream, dotThis, dn, Definition::VAR);
        }
    }

    // Also define a this-binding if direct eval is used, in derived class
    // constructors (JSOP_CHECKRETURN relies on it) or if there's a debugger
    // statement.
    if (pc->sc->hasDirectEval() ||
        pc->sc->asFunctionBox()->isDerivedClassConstructor() ||
        pc->sc->hasDebuggerStatement())
    {
        ParseNode* pn = newName(dotThis);
        if (!pn)
            return false;
        pc->sc->asFunctionBox()->setHasThisBinding();
        return pc->define(tokenStream, dotThis, pn, Definition::VAR);
    }

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::defineFunctionThis()
{
    return true;
}

template <>
ParseNode*
Parser<FullParseHandler>::standaloneFunctionBody(HandleFunction fun,
                                                 Handle<PropertyNameVector> formals,
                                                 GeneratorKind generatorKind,
                                                 Directives inheritedDirectives,
                                                 Directives* newDirectives,
                                                 HandleObject enclosingStaticScope)
{
    MOZ_ASSERT(checkOptionsCalled);

    Node fn = handler.newFunctionDefinition();
    if (!fn)
        return null();

    ParseNode* argsbody = handler.newList(PNK_ARGSBODY);
    if (!argsbody)
        return null();
    fn->pn_body = argsbody;

    FunctionBox* funbox = newFunctionBox(fn, fun, inheritedDirectives, generatorKind,
                                         enclosingStaticScope);
    if (!funbox)
        return null();
    funbox->length = fun->nargs() - fun->hasRest();
    handler.setFunctionBox(fn, funbox);

    ParseContext<FullParseHandler> funpc(this, pc, fn, funbox, newDirectives);
    if (!funpc.init(*this))
        return null();

    for (unsigned i = 0; i < formals.length(); i++) {
        if (!defineArg(fn, formals[i]))
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

    MOZ_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
    fn->pn_body->append(pn);

    /*
     * Make sure to deoptimize lexical dependencies that are polluted
     * by eval and function statements (which both flag the function as
     * having an extensible scope).
     */
    if (funbox->hasExtensibleScope() && pc->lexdeps->count()) {
        for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
            Definition* dn = r.front().value().get<FullParseHandler>();
            MOZ_ASSERT(dn->isPlaceholder());

            handler.deoptimizeUsesWithin(dn, fn->pn_pos);
        }
    }

    Rooted<Bindings> bindings(context, funbox->bindings);
    if (!funpc.generateBindings(context, tokenStream, alloc, &bindings))
        return null();
    funbox->bindings = bindings;

    return fn;
}

template <>
bool
Parser<FullParseHandler>::checkFunctionArguments()
{
    /* Time to implement the odd semantics of 'arguments'. */
    HandlePropertyName arguments = context->names().arguments;

    /*
     * As explained by the ContextFlags::funArgumentsHasLocalBinding comment,
     * create a declaration for 'arguments' if there are any unbound uses in
     * the function body.
     */
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        if (r.front().key() == arguments) {
            Definition* dn = r.front().value().get<FullParseHandler>();
            pc->lexdeps->remove(arguments);
            dn->pn_dflags |= PND_IMPLICITARGUMENTS;
            if (!pc->define(tokenStream, arguments, dn, Definition::VAR))
                return false;
            pc->sc->asFunctionBox()->usesArguments = true;
            break;
        }
    }

    Definition* maybeArgDef = pc->decls().lookupFirst(arguments);
    bool argumentsHasBinding = !!maybeArgDef;
    // ES6 9.2.13.17 says that a lexical binding of 'arguments' shadows the
    // arguments object.
    bool argumentsHasLocalBinding = maybeArgDef && (maybeArgDef->kind() != Definition::ARG &&
                                                    maybeArgDef->kind() != Definition::LET &&
                                                    maybeArgDef->kind() != Definition::CONSTANT);

    /*
     * Even if 'arguments' isn't explicitly mentioned, dynamic name lookup
     * forces an 'arguments' binding.
     */
    if (!argumentsHasBinding && pc->sc->bindingsAccessedDynamically()) {
        ParseNode* pn = newName(arguments);
        if (!pn)
            return false;
        if (!pc->define(tokenStream, arguments, pn, Definition::VAR))
            return false;
        argumentsHasBinding = true;
        argumentsHasLocalBinding = true;
    }

    /*
     * Now that all possible 'arguments' bindings have been added, note whether
     * 'arguments' has a local binding and whether it unconditionally needs an
     * arguments object. (Also see the flags' comments in ContextFlags.)
     */
    if (argumentsHasLocalBinding) {
        FunctionBox* funbox = pc->sc->asFunctionBox();
        funbox->setArgumentsHasLocalBinding();

        /* Dynamic scope access destroys all hope of optimization. */
        if (pc->sc->bindingsAccessedDynamically())
            funbox->setDefinitelyNeedsArgsObj();

        /*
         * If a script contains the debugger statement either directly or
         * within an inner function, the arguments object must be created
         * eagerly. The debugger can walk the scope chain and observe any
         * values along it.
         */
        if (pc->sc->hasDebuggerStatement())
            funbox->setDefinitelyNeedsArgsObj();

        /*
         * Check whether any parameters have been assigned within this
         * function. If the arguments object is unmapped (strict mode or
         * function with default/rest/destructing args), parameters do not alias
         * arguments[i], and to make the arguments object reflect initial
         * parameter values prior to any mutation we create it eagerly whenever
         * parameters are (or might, in the case of calls to eval) assigned.
         */
        if (!funbox->hasMappedArgsObj()) {
            for (AtomDefnListMap::Range r = pc->decls().all(); !r.empty(); r.popFront()) {
                DefinitionList& dlist = r.front().value();
                for (DefinitionList::Range dr = dlist.all(); !dr.empty(); dr.popFront()) {
                    Definition* dn = dr.front<FullParseHandler>();
                    if (dn->kind() == Definition::ARG && dn->isAssigned())
                        funbox->setDefinitelyNeedsArgsObj();
                }
            }
        }
    }

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkFunctionArguments()
{
    if (pc->lexdeps->lookup(context->names().arguments))
        pc->sc->asFunctionBox()->usesArguments = true;

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionBody(InHandling inHandling, YieldHandling yieldHandling,
                                   FunctionSyntaxKind kind, FunctionBodyType type)
{
    MOZ_ASSERT(pc->sc->isFunctionBox());
    MOZ_ASSERT(!pc->funHasReturnExpr && !pc->funHasReturnVoid);

#ifdef DEBUG
    uint32_t startYieldOffset = pc->lastYieldOffset;
#endif

    Node pn;
    if (type == StatementListBody) {
        pn = statements(yieldHandling);
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
        Node generator = newName(context->names().dotGenerator);
        if (!generator)
            return null();
        if (!pc->define(tokenStream, context->names().dotGenerator, generator, Definition::VAR))
            return null();

        generator = newName(context->names().dotGenerator);
        if (!generator)
            return null();
        if (!noteNameUse(context->names().dotGenerator, generator))
            return null();
        if (!handler.prependInitialYield(pn, generator))
            return null();
    }

    if (kind != Arrow) {
        // Define the 'arguments' and 'this' bindings if necessary. Arrow
        // functions don't have these bindings.
        if (!checkFunctionArguments())
            return null();
        if (!defineFunctionThis())
            return null();
    }

    return pn;
}

/* See comment for use in Parser::functionDef. */
template <>
bool
Parser<FullParseHandler>::makeDefIntoUse(Definition* dn, ParseNode* pn, HandleAtom atom)
{
    /* Turn pn into a definition. */
    if (!pc->updateDecl(tokenStream, atom, pn))
        return false;

    /* Change all uses of dn to be uses of pn. */
    for (ParseNode* pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
        MOZ_ASSERT(pnu->isUsed());
        MOZ_ASSERT(!pnu->isDefn());
        pnu->pn_lexdef = &pn->as<Definition>();
        pn->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
    }
    pn->pn_dflags |= dn->pn_dflags & PND_USE2DEF_FLAGS;
    pn->dn_uses = dn;

    /*
     * A PNK_FUNCTION node must be a definition, so convert shadowed function
     * statements into nops. This is valid since all body-level function
     * statement initialization happens at the beginning of the function
     * (thus, only the last statement's effect is visible). E.g., in
     *
     *   function outer() {
     *     function g() { return 1 }
     *     assertEq(g(), 2);
     *     function g() { return 2 }
     *     assertEq(g(), 2);
     *   }
     *
     * both asserts are valid.
     */
    if (dn->getKind() == PNK_FUNCTION) {
        MOZ_ASSERT(dn->functionIsHoisted());
        pn->dn_uses = dn->pn_link;
        handler.prepareNodeForMutation(dn);
        dn->setKind(PNK_NOP);
        dn->setArity(PN_NULLARY);
        dn->setDefn(false);
        return true;
    }

    /*
     * If dn is in [var, const, let] and has an initializer, then we
     * must rewrite it to be an assignment node, whose freshly allocated
     * left-hand side becomes a use of pn.
     */
    if (dn->canHaveInitializer()) {
        if (ParseNode* rhs = dn->expr()) {
            ParseNode* lhs = handler.makeAssignment(dn, rhs);
            if (!lhs)
                return false;
            pn->dn_uses = lhs;
            dn->pn_link = nullptr;
            dn = &lhs->as<Definition>();
        }
    }

    /* Turn dn into a use of pn. */
    MOZ_ASSERT(dn->isKind(PNK_NAME));
    MOZ_ASSERT(dn->isArity(PN_NAME));
    MOZ_ASSERT(dn->pn_atom == atom);
    dn->setOp((CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETNAME : JSOP_GETNAME);
    dn->setDefn(false);
    dn->setUsed(true);
    dn->pn_lexdef = &pn->as<Definition>();
    dn->pn_scopecoord.makeFree();
    dn->pn_dflags &= ~PND_BOUND;
    return true;
}

/*
 * Helper class for creating bindings.
 *
 * The same instance can be used more than once by repeatedly calling
 * setNameNode() followed by bind().
 *
 * In the destructuring case, bind() is called indirectly from the variable
 * declaration parser by way of checkDestructuringPattern and its friends.
 */
template <typename ParseHandler>
struct BindData
{
    struct LetData {
        explicit LetData(ExclusiveContext* cx) : blockScope(cx) {}
        VarContext varContext;
        RootedStaticBlockScope blockScope;
        unsigned overflow;
    };

    explicit BindData(ExclusiveContext* cx)
      : kind_(Uninitialized),
        nameNode_(ParseHandler::null()),
        letData_(cx),
        isForOf(false)
    {}

    void initLexical(VarContext varContext, JSOp op, StaticBlockScope* blockScope,
                     unsigned overflow)
    {
        init(LexicalBinding, op, op == JSOP_DEFCONST, false);
        letData_.varContext = varContext;
        letData_.blockScope = blockScope;
        letData_.overflow = overflow;
    }

    void initVar(JSOp op) {
        init(VarBinding, op, false, false);
    }

    void initAnnexBVar() {
        init(VarBinding, JSOP_DEFVAR, false, true);
    }

    void initDestructuring(JSOp op) {
        init(DestructuringBinding, op, false, false);
    }

    void setNameNode(typename ParseHandler::Node pn) {
        MOZ_ASSERT(isInitialized());
        nameNode_ = pn;
    }

    typename ParseHandler::Node nameNode() {
        MOZ_ASSERT(isInitialized());
        return nameNode_;
    }

    JSOp op() {
        MOZ_ASSERT(isInitialized());
        return op_;
    }

    bool isConst() {
        MOZ_ASSERT(isInitialized());
        return isConst_;
    }

    bool isAnnexB() {
        MOZ_ASSERT(isInitialized());
        return isAnnexB_;
    }

    // The BoundNames of LexicalDeclaration and ForDeclaration must not contain
    // 'let'.  (CatchParameter is the only lexical binding form without this
    // restriction.)
    bool mustNotBindLet() {
        MOZ_ASSERT(isInitialized());
        return isConst_ ||
               (kind_ == LexicalBinding && letData_.overflow != JSMSG_TOO_MANY_CATCH_VARS);
    }

    const LetData& letData() {
        MOZ_ASSERT(kind_ == LexicalBinding);
        return letData_;
    }

    bool bind(HandlePropertyName name, Parser<ParseHandler>* parser) {
        MOZ_ASSERT(isInitialized());
        MOZ_ASSERT(nameNode_ != ParseHandler::null());
        switch (kind_) {
          case LexicalBinding:
            return Parser<ParseHandler>::bindLexical(this, name, parser);
          case VarBinding:
            return Parser<ParseHandler>::bindVar(this, name, parser);
          case DestructuringBinding:
            return Parser<ParseHandler>::bindDestructuringArg(this, name, parser);
          default:
            MOZ_CRASH();
        }
        nameNode_ = ParseHandler::null();
    }

  private:
    enum BindingKind {
        Uninitialized,
        LexicalBinding,
        VarBinding,
        DestructuringBinding
    };

    BindingKind kind_;

    // Name node for definition processing and error source coordinates.
    typename ParseHandler::Node nameNode_;

    JSOp op_;         // Prologue bytecode or nop.
    bool isConst_;    // Whether this is a const binding.
    bool isAnnexB_;   // Whether this is a synthesized 'var' binding for Annex B.3.
    LetData letData_;

  public:
    bool isForOf;     // Whether this is binding a for-of head.

  private:
    bool isInitialized() {
        return kind_ != Uninitialized;
    }

    void init(BindingKind kind, JSOp op, bool isConst, bool isAnnexB) {
        MOZ_ASSERT(!isInitialized());
        kind_ = kind;
        op_ = op;
        isConst_ = isConst;
        isAnnexB_ = isAnnexB;
    }
};

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
        if (options().selfHostingMode && !pc->sc->isFunctionBox()) {
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

/*
 * The function LexicalLookup searches a static binding for the given name in
 * the stack of statements enclosing the statement currently being parsed. Each
 * statement that introduces a new scope has a corresponding scope object, on
 * which the bindings for that scope are stored. LexicalLookup either returns
 * the innermost statement which has a scope object containing a binding with
 * the given name, or nullptr.
 */
template <class ContextT>
static StmtInfoPC*
LexicalLookup(ContextT* ct, HandleAtom atom, StmtInfoPC* stmt = nullptr)
{
    RootedId id(ct->sc->context, AtomToId(atom));

    if (!stmt)
        stmt = ct->innermostScopeStmt();
    for (; stmt; stmt = stmt->enclosingScope) {
        /*
         * With-statements introduce dynamic bindings. Since dynamic bindings
         * can potentially override any static bindings introduced by statements
         * further up the stack, we have to abort the search.
         */
        if (stmt->type == StmtType::WITH && !ct->sc->isDotVariable(atom))
            break;

        // Skip statements that do not introduce a new scope
        if (!stmt->isBlockScope)
            continue;

        StaticBlockScope& blockScope = stmt->staticBlock();
        Shape* shape = blockScope.lookup(ct->sc->context, id);
        if (shape)
            return stmt;
    }

    return stmt;
}

template <typename ParseHandler>
typename ParseHandler::DefinitionNode
Parser<ParseHandler>::getOrCreateLexicalDependency(ParseContext<ParseHandler>* pc, JSAtom* atom)
{
    AtomDefnAddPtr p = pc->lexdeps->lookupForAdd(atom);
    if (p)
        return p.value().get<ParseHandler>();

    DefinitionNode dn = handler.newPlaceholder(atom, pc->blockid(), pos());
    if (!dn)
        return ParseHandler::nullDefinition();
    DefinitionSingle def = DefinitionSingle::new_<ParseHandler>(dn);
    if (!pc->lexdeps->add(p, atom, def))
        return ParseHandler::nullDefinition();
    return dn;
}

static bool
ConvertDefinitionToNamedLambdaUse(TokenStream& ts, ParseContext<FullParseHandler>* pc,
                                  FunctionBox* funbox, Definition* dn)
{
    dn->setOp(JSOP_CALLEE);
    if (!dn->pn_scopecoord.setSlot(ts, 0))
        return false;
    dn->pn_blockid = pc->blockid();
    dn->pn_dflags |= PND_BOUND;
    MOZ_ASSERT(dn->kind() == Definition::NAMED_LAMBDA);

    /*
     * Since 'dn' is a placeholder, it has not been defined in the
     * ParseContext and hence we must manually flag a closed-over
     * callee name as needing a dynamic scope (this is done for all
     * definitions in the ParseContext by generateBindings).
     *
     * If 'dn' has been assigned to, then we also flag the function
     * scope has needing a dynamic scope so that dynamic scope
     * setter can either ignore the set (in non-strict mode) or
     * produce an error (in strict mode).
     */
    if (dn->isClosed() || dn->isAssigned())
        funbox->setNeedsDeclEnvObject();
    return true;
}

static bool
IsNonDominatingInScopedSwitch(ParseContext<FullParseHandler>* pc, HandleAtom name,
                              Definition* dn)
{
    MOZ_ASSERT(dn->isLexical());
    StmtInfoPC* stmt = LexicalLookup(pc, name);
    if (stmt && stmt->type == StmtType::SWITCH)
        return dn->pn_scopecoord.slot() < stmt->firstDominatingLexicalInCase;
    return false;
}

static void
AssociateUsesWithOuterDefinition(ParseNode* pnu, Definition* dn, Definition* outer_dn,
                                 bool markUsesAsLexical)
{
    uint32_t dflags = markUsesAsLexical ? PND_LEXICAL : 0;
    while (true) {
        pnu->pn_lexdef = outer_dn;
        pnu->pn_dflags |= dflags;
        if (!pnu->pn_link)
            break;
        pnu = pnu->pn_link;
    }
    pnu->pn_link = outer_dn->dn_uses;
    outer_dn->dn_uses = dn->dn_uses;
    dn->dn_uses = nullptr;
}

/*
 * Beware: this function is called for functions nested in other functions or
 * global scripts but not for functions compiled through the Function
 * constructor or JSAPI. To always execute code when a function has finished
 * parsing, use Parser::functionBody.
 */
template <>
bool
Parser<FullParseHandler>::leaveFunction(ParseNode* fn, ParseContext<FullParseHandler>* outerpc,
                                        FunctionSyntaxKind kind)
{
    FunctionBox* funbox = fn->pn_funbox;
    MOZ_ASSERT(funbox == pc->sc->asFunctionBox());

    /* Propagate unresolved lexical names up to outerpc->lexdeps. */
    if (pc->lexdeps->count()) {
        for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
            JSAtom* atom = r.front().key();
            Definition* dn = r.front().value().get<FullParseHandler>();
            MOZ_ASSERT(dn->isPlaceholder());

            if (atom == funbox->function()->name() && kind == Expression) {
                if (!ConvertDefinitionToNamedLambdaUse(tokenStream, pc, funbox, dn))
                    return false;
                continue;
            }

            Definition* outer_dn = outerpc->decls().lookupFirst(atom);

            /*
             * Make sure to deoptimize lexical dependencies that are polluted
             * by eval and function statements (which both flag the function as
             * having an extensible scope) or any enclosing 'with'.
             */
            if (funbox->hasExtensibleScope() || funbox->inWith())
                handler.deoptimizeUsesWithin(dn, fn->pn_pos);

            if (!outer_dn) {
                /*
                 * Create a new placeholder for our outer lexdep. We could
                 * simply re-use the inner placeholder, but that introduces
                 * subtleties in the case where we find a later definition
                 * that captures an existing lexdep. For example:
                 *
                 *   function f() { function g() { x; } let x; }
                 *
                 * Here, g's TOK_UPVARS node lists the placeholder for x,
                 * which must be captured by the 'let' declaration later,
                 * since 'let's are hoisted.  Taking g's placeholder as our
                 * own would work fine. But consider:
                 *
                 *   function f() { x; { function g() { x; } let x; } }
                 *
                 * Here, the 'let' must not capture all the uses of f's
                 * lexdep entry for x, but it must capture the x node
                 * referred to from g's TOK_UPVARS node.  Always turning
                 * inherited lexdeps into uses of a new outer definition
                 * allows us to handle both these cases in a natural way.
                 */
                outer_dn = getOrCreateLexicalDependency(outerpc, atom);
                if (!outer_dn)
                    return false;
            }

            /*
             * Insert dn's uses list at the front of outer_dn's list.
             *
             * Without loss of generality or correctness, we allow a dn to
             * be in inner and outer lexdeps, since the purpose of lexdeps
             * is one-pass coordination of name use and definition across
             * functions, and if different dn's are used we'll merge lists
             * when leaving the inner function.
             */
            if (dn != outer_dn) {
                if (ParseNode* pnu = dn->dn_uses) {
                    // In ES6, lexical bindings cannot be accessed until
                    // initialized. If we are parsing a function statement it
                    // is hoisted to the top of its lexical scope, so we
                    // conservatively mark all uses linked to an outer lexical
                    // binding as needing TDZ checks. e.g.,
                    //
                    // function outer() {
                    //   inner2();
                    //   function inner() { use(x); }
                    //   function inner2() { inner(); }
                    //   let x;
                    // }
                    //
                    // The use of 'x' inside 'inner' needs to be marked.
                    //
                    // Note that to not be fully conservative requires a call
                    // graph analysis of all body-level functions to compute
                    // the transitive closure of which hoisted body level use
                    // of which function forces TDZ checks on which uses. This
                    // is unreasonably difficult to do in a single pass parser
                    // like ours.
                    //
                    // Similarly, if we are closing over a lexical binding
                    // from another case in a switch, those uses also need to
                    // be marked as needing dead zone checks.
                    RootedAtom name(context, atom);
                    bool markUsesAsLexical = outer_dn->isLexical() &&
                                             (kind == Statement ||
                                              IsNonDominatingInScopedSwitch(outerpc, name, outer_dn));
                    AssociateUsesWithOuterDefinition(pnu, dn, outer_dn, markUsesAsLexical);
                }

                outer_dn->pn_dflags |= dn->pn_dflags & ~PND_PLACEHOLDER;
            }

            /* Mark the outer dn as escaping. */
            outer_dn->pn_dflags |= PND_CLOSED;
        }
    }

    Rooted<Bindings> bindings(context, funbox->bindings);
    if (!pc->generateBindings(context, tokenStream, alloc, &bindings))
        return false;
    funbox->bindings = bindings;

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::leaveFunction(Node fn, ParseContext<SyntaxParseHandler>* outerpc,
                                          FunctionSyntaxKind kind)
{
    FunctionBox* funbox = pc->sc->asFunctionBox();
    return addFreeVariablesFromLazyFunction(funbox->function(), outerpc);
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

/*
 * defineArg is called for both the arguments of a regular function definition
 * and the arguments specified by the Function constructor.
 *
 * The 'disallowDuplicateArgs' bool indicates whether the use of another
 * feature (destructuring or default arguments) disables duplicate arguments.
 * (ECMA-262 requires us to support duplicate parameter names, but, for newer
 * features, we consider the code to have "opted in" to higher standards and
 * forbid duplicates.)
 *
 * If 'duplicatedArg' is non-null, then DefineArg assigns to it any previous
 * argument with the same name. The caller may use this to report an error when
 * one of the abovementioned features occurs after a duplicate.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::defineArg(Node funcpn, HandlePropertyName name,
                                bool disallowDuplicateArgs, Node* duplicatedArg)
{
    SharedContext* sc = pc->sc;

    /* Handle duplicate argument names. */
    if (DefinitionNode prevDecl = pc->decls().lookupFirst(name)) {
        Node pn = handler.getDefinitionNode(prevDecl);

        /*
         * Strict-mode disallows duplicate args. We may not know whether we are
         * in strict mode or not (since the function body hasn't been parsed).
         * In such cases, report will queue up the potential error and return
         * 'true'.
         */
        if (sc->needStrictChecks()) {
            JSAutoByteString bytes;
            if (!AtomToPrintableString(context, name, &bytes))
                return false;
            if (!report(ParseStrictError, pc->sc->strict(), pn,
                        JSMSG_DUPLICATE_FORMAL, bytes.ptr()))
            {
                return false;
            }
        }

        if (disallowDuplicateArgs) {
            report(ParseError, false, pn, JSMSG_BAD_DUP_ARGS);
            return false;
        }

        if (duplicatedArg)
            *duplicatedArg = pn;

        /* ParseContext::define assumes and asserts prevDecl is not in decls. */
        MOZ_ASSERT(handler.getDefinitionKind(prevDecl) == Definition::ARG);
        pc->prepareToAddDuplicateArg(name, prevDecl);
    }

    Node argpn = newName(name);
    if (!argpn)
        return false;

    if (!checkStrictBinding(name, argpn))
        return false;

    handler.addFunctionArgument(funcpn, argpn);
    return pc->define(tokenStream, name, argpn, Definition::ARG);
}

template <typename ParseHandler>
/* static */ bool
Parser<ParseHandler>::bindDestructuringArg(BindData<ParseHandler>* data,
                                           HandlePropertyName name, Parser<ParseHandler>* parser)
{
    ParseContext<ParseHandler>* pc = parser->pc;
    MOZ_ASSERT(pc->sc->isFunctionBox());

    if (pc->decls().lookupFirst(name)) {
        parser->report(ParseError, false, null(), JSMSG_BAD_DUP_ARGS);
        return false;
    }

    if (!parser->checkStrictBinding(name, data->nameNode()))
        return false;

    return pc->define(parser->tokenStream, name, data->nameNode(), Definition::VAR);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionArguments(YieldHandling yieldHandling, FunctionSyntaxKind kind,
                                        Node funcpn, bool* hasRest)
{
    FunctionBox* funbox = pc->sc->asFunctionBox();

    *hasRest = false;

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

    Node argsbody = handler.newList(PNK_ARGSBODY);
    if (!argsbody)
        return false;
    handler.setFunctionBody(funcpn, argsbody);

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
        bool hasDefaults = false;
        Node duplicatedArg = null();
        bool disallowDuplicateArgs = kind == Arrow || kind == Method || kind == ClassConstructor;

        if (IsGetterKind(kind)) {
            report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "getter", "no", "s");
            return false;
        }

        while (true) {
            if (*hasRest) {
                report(ParseError, false, null(), JSMSG_PARAMETER_AFTER_REST);
                return false;
            }

            TokenKind tt;
            if (!tokenStream.getToken(&tt, TokenStream::Operand))
                return false;
            MOZ_ASSERT_IF(parenFreeArrow, tt == TOK_NAME);
            switch (tt) {
              case TOK_LB:
              case TOK_LC:
              {
                /* See comment below in the TOK_NAME case. */
                disallowDuplicateArgs = true;
                if (duplicatedArg) {
                    report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                funbox->hasDestructuringArgs = true;

                /*
                 * A destructuring formal parameter turns into one or more
                 * local variables initialized from properties of a single
                 * anonymous positional parameter, so here we must tweak our
                 * binder and its data.
                 */
                BindData<ParseHandler> data(context);
                data.initDestructuring(JSOP_DEFVAR);
                Node destruct = destructuringExprWithoutYield(yieldHandling, &data, tt,
                                                              JSMSG_YIELD_IN_DEFAULT);
                if (!destruct)
                    return false;

                /*
                 * Make a single anonymous positional parameter, and store
                 * destructuring expression into the node.
                 */
                HandlePropertyName name = context->names().empty;
                Node arg = newName(name);
                if (!arg)
                    return false;

                handler.addFunctionArgument(funcpn, arg);
                if (!pc->define(tokenStream, name, arg, Definition::ARG))
                    return false;

                handler.setLastFunctionArgumentDestructuring(funcpn, destruct);
                break;
              }

              case TOK_YIELD:
                if (!checkYieldNameValidity())
                    return false;
                MOZ_ASSERT(yieldHandling == YieldIsName);
                goto TOK_NAME;

              case TOK_TRIPLEDOT:
              {
                if (IsSetterKind(kind)) {
                    report(ParseError, false, null(),
                           JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
                    return false;
                }
                *hasRest = true;
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
                disallowDuplicateArgs = true;
                if (duplicatedArg) {
                    // Has duplicated args before the rest parameter.
                    report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }
                goto TOK_NAME;
              }

              TOK_NAME:
              case TOK_NAME:
              {
                if (parenFreeArrow)
                    funbox->setStart(tokenStream);

                RootedPropertyName name(context, tokenStream.currentName());
                if (!defineArg(funcpn, name, disallowDuplicateArgs, &duplicatedArg))
                    return false;
                break;
              }

              default:
                report(ParseError, false, null(), JSMSG_MISSING_FORMAL);
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

                if (*hasRest) {
                    report(ParseError, false, null(), JSMSG_REST_WITH_DEFAULT);
                    return false;
                }
                disallowDuplicateArgs = true;
                if (duplicatedArg) {
                    report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }
                if (!hasDefaults) {
                    hasDefaults = true;

                    // The Function.length property is the number of formals
                    // before the first default argument.
                    funbox->length = pc->numArgs() - 1;
                }
                Node def_expr = assignExprWithoutYield(yieldHandling, JSMSG_YIELD_IN_DEFAULT);
                if (!def_expr)
                    return false;
                if (!handler.setLastFunctionArgumentDefault(funcpn, def_expr))
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

        if (!hasDefaults)
            funbox->length = pc->numArgs() - *hasRest;
    } else if (IsSetterKind(kind)) {
        report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
        return false;
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::bindBodyLevelFunctionName(HandlePropertyName funName,
                                                    ParseNode** pn_)
{
    MOZ_ASSERT(pc->atBodyLevel() || !pc->sc->strict());

    ParseNode*& pn = *pn_;

    /*
     * Handle redeclaration and optimize cases where we can statically bind the
     * function (thereby avoiding JSOP_DEFFUN and dynamic name lookup).
     */
    if (Definition* dn = pc->decls().lookupFirst(funName)) {
        MOZ_ASSERT(!dn->isUsed());
        MOZ_ASSERT(dn->isDefn());

        Definition::Kind kind = dn->kind();
        if (kind == Definition::CONSTANT || kind == Definition::LET || kind == Definition::IMPORT)
            return reportRedeclaration(nullptr, kind, funName);

        /*
         * Body-level function statements are effectively variable
         * declarations where the initialization is hoisted to the
         * beginning of the block. This means that any other variable
         * declaration with the same name is really just an assignment to
         * the function's binding (which is mutable), so turn any existing
         * declaration into a use.
         */
        if (kind == Definition::ARG) {
            // The exception to the above comment is when the function
            // has the same name as an argument. Then the argument node
            // remains a definition. But change the function node pn so
            // that it knows where the argument is located.
            pn->setOp(JSOP_GETARG);
            pn->setDefn(true);
            pn->pn_scopecoord = dn->pn_scopecoord;
            pn->pn_blockid = dn->pn_blockid;
            pn->pn_dflags |= PND_BOUND;
            dn->markAsAssigned();
        } else {
            if (!makeDefIntoUse(dn, pn, funName))
                return false;
        }
    } else {
        /*
         * If this function was used before it was defined, claim the
         * pre-created definition node for this function that primaryExpr
         * put in pc->lexdeps on first forward reference, and recycle pn.
         */
        if (Definition* fn = pc->lexdeps.lookupDefn<FullParseHandler>(funName)) {
            MOZ_ASSERT(fn->isDefn());
            fn->setKind(PNK_FUNCTION);
            fn->setArity(PN_CODE);
            fn->pn_pos.begin = pn->pn_pos.begin;
            fn->pn_pos.end = pn->pn_pos.end;

            fn->pn_body = nullptr;
            fn->pn_scopecoord.makeFree();

            pc->lexdeps->remove(funName);
            handler.freeTree(pn);
            pn = fn;
        }

        if (!pc->define(tokenStream, funName, pn, Definition::VAR))
            return false;
    }

    /* No further binding (in BindNameToSlot) is needed for functions. */
    pn->pn_dflags |= PND_BOUND;

    MOZ_ASSERT(pn->functionIsHoisted());
    MOZ_ASSERT(pc->sc->isGlobalContext() == pn->pn_scopecoord.isFree());

    return true;
}

template <>
bool
Parser<FullParseHandler>::bindLexicalFunctionName(HandlePropertyName funName,
                                                  ParseNode* pn);

template <>
bool
Parser<FullParseHandler>::checkFunctionDefinition(HandleAtom funAtom,
                                                  ParseNode** pn_, FunctionSyntaxKind kind,
                                                  bool* pbodyProcessed,
                                                  ParseNode** assignmentForAnnexBOut)
{
    ParseNode*& pn = *pn_;
    *pbodyProcessed = false;

    if (kind == Statement) {
        RootedPropertyName funName(context, funAtom->asPropertyName());
        MOZ_ASSERT(assignmentForAnnexBOut);
        *assignmentForAnnexBOut = nullptr;

        // In sloppy mode, ES6 Annex B.3.2 allows labelled function
        // declarations. Otherwise it is a parse error.
        bool bodyLevelFunction = pc->atBodyLevel();
        if (!bodyLevelFunction) {
            StmtInfoPC* stmt = pc->innermostStmt();
            if (stmt->type == StmtType::LABEL) {
                if (pc->sc->strict()) {
                    report(ParseError, false, null(), JSMSG_FUNCTION_LABEL);
                    return false;
                }

                stmt = pc->innermostNonLabelStmt();
                // A switch statement is always braced, so it's okay to label
                // functions in sloppy mode under switch.
                if (stmt && stmt->type != StmtType::BLOCK && stmt->type != StmtType::SWITCH) {
                    report(ParseError, false, null(), JSMSG_SLOPPY_FUNCTION_LABEL);
                    return false;
                }

                bodyLevelFunction = pc->atBodyLevel(stmt);
            }
        }

        if (bodyLevelFunction) {
            if (!bindBodyLevelFunctionName(funName, pn_))
                return false;
        } else {
            Definition* annexDef = nullptr;
            Node synthesizedDeclarationList = null();

            if (!pc->sc->strict()) {
                // Under non-strict mode, try ES6 Annex B.3.3 semantics. If
                // making an additional 'var' binding of the same name does
                // not throw an early error, do so. This 'var' binding would
                // be assigned the function object when its declaration is
                // reached, not at the start of the block.

                annexDef = pc->decls().lookupFirst(funName);
                if (annexDef) {
                    if (annexDef->kind() == Definition::CONSTANT ||
                        annexDef->kind() == Definition::LET)
                    {
                        if (annexDef->isKind(PNK_FUNCTION)) {
                            // XXXshu Code used only for phasing in block-scope
                            // XXXshu function early errors. Ignore redeclarations
                            // XXXshu here and generate Annex B assignments for
                            // XXXshu block-scoped functions that redeclare other
                            // XXXshu block-scoped functions.
                            // XXXshu
                            // XXXshu Get the possibly-synthesized var that was
                            // XXXshu already made for the first of the block-scoped
                            // XXXshu functions.
                            // XXXshu
                            // XXXshu Back out when major version >= 50. See [1].
                            // XXXshu
                            // XXXshu [1] https://bugzilla.mozilla.org/show_bug.cgi?id=1235590#c10
                            annexDef = pc->decls().lookupLast(funName);
                            if (annexDef->kind() == Definition::CONSTANT ||
                                annexDef->kind() == Definition::LET)
                            {
                                annexDef = nullptr;
                            }
                        } else {
                            // Do not emit Annex B assignment if we would've
                            // thrown a redeclaration error.
                            annexDef = nullptr;
                        }
                    }
                } else {
                    // Synthesize a new 'var' binding if one does not exist.
                    ParseNode* varNode = newBindingNode(funName, /* functionScope = */ true);
                    if (!varNode)
                        return false;

                    // Treat the 'var' binding as body level. Otherwise the
                    // lexical binding of the function name below would result
                    // in a redeclaration. That is,
                    // { var x; let x; } is an early error.
                    // var x; { let x; } is not.
                    varNode->pn_blockid = pc->bodyid;

                    BindData<FullParseHandler> data(context);
                    data.initAnnexBVar();
                    data.setNameNode(varNode);
                    if (!data.bind(funName, this))
                        return false;

                    annexDef = &varNode->as<Definition>();

                    synthesizedDeclarationList = handler.newDeclarationList(PNK_VAR, JSOP_DEFVAR);
                    if (!synthesizedDeclarationList)
                        return false;
                    handler.addList(synthesizedDeclarationList, annexDef);
                }
            }

            if (!bindLexicalFunctionName(funName, pn))
                return false;

            if (annexDef) {
                MOZ_ASSERT(!pc->sc->strict());

                // Synthesize an assignment assigning the lexical name to the
                // 'var' name for Annex B.

                ParseNode* rhs = newName(funName);
                if (!rhs)
                    return false;
                if (!noteNameUse(funName, rhs))
                    return false;

                // If we synthesized a new definition, emit the declaration to
                // ensure DEFVAR is correctly emitted in global scripts.
                // Otherwise, synthesize a simple assignment and emit that.
                if (synthesizedDeclarationList) {
                    if (!handler.finishInitializerAssignment(annexDef, rhs))
                        return false;
                    *assignmentForAnnexBOut = synthesizedDeclarationList;
                } else {
                    ParseNode* lhs = newName(funName);
                    if (!lhs)
                        return false;
                    lhs->setOp(JSOP_SETNAME);

                    // Manually link up the LHS with the non-lexical definition.
                    handler.linkUseToDef(lhs, annexDef);

                    ParseNode* assign = handler.newAssignment(PNK_ASSIGN, lhs, rhs, pc, JSOP_NOP);
                    if (!assign)
                        return false;

                    *assignmentForAnnexBOut = assign;
                }
            }
        }
    } else {
        /* A function expression does not introduce any binding. */
        pn->setOp(kind == Arrow ? JSOP_LAMBDA_ARROW : JSOP_LAMBDA);
    }

    // When a lazily-parsed function is called, we only fully parse (and emit)
    // that function, not any of its nested children. The initial syntax-only
    // parse recorded the free variables of nested functions and their extents,
    // so we can skip over them after accounting for their free variables.
    Rooted<LazyScript*> lazyOuter(context, handler.lazyOuterFunction());
    if (lazyOuter) {
        RootedFunction fun(context, handler.nextLazyInnerFunction());
        MOZ_ASSERT(!fun->isLegacyGenerator());
        FunctionBox* funbox = newFunctionBox(pn, fun, pc, Directives(/* strict = */ false),
                                             fun->generatorKind());
        if (!funbox)
            return false;

        if (fun->lazyScript()->needsHomeObject())
            funbox->setNeedsHomeObject();

        if (!addFreeVariablesFromLazyFunction(fun, pc))
            return false;

        // The position passed to tokenStream.advance() is an offset of the sort
        // returned by userbuf.offset() and expected by userbuf.rawCharPtrAt(),
        // while LazyScript::{begin,end} offsets are relative to the outermost
        // script source.
        uint32_t userbufBase = lazyOuter->begin() - lazyOuter->column();
        if (!tokenStream.advance(fun->lazyScript()->end() - userbufBase))
            return false;

        *pbodyProcessed = true;
        return true;
    }

    return true;
}

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

template <typename ParseHandler>
bool
Parser<ParseHandler>::addFreeVariablesFromLazyFunction(JSFunction* fun,
                                                       ParseContext<ParseHandler>* pc)
{
    // Update any definition nodes in this context according to free variables
    // in a lazily parsed inner function.

    bool bodyLevel = pc->atBodyLevel();
    LazyScript* lazy = fun->lazyScript();
    LazyScript::FreeVariable* freeVariables = lazy->freeVariables();
    for (size_t i = 0; i < lazy->numFreeVariables(); i++) {
        JSAtom* atom = freeVariables[i].atom();

        // 'arguments' will be implicitly bound within the inner function,
        // except if the inner function is an arrow function.
        if (atom == context->names().arguments && !fun->isArrow())
            continue;

        DefinitionNode dn = pc->decls().lookupFirst(atom);

        if (!dn) {
            dn = getOrCreateLexicalDependency(pc, atom);
            if (!dn)
                return false;
        }

        // In ES6, lexical bindings are unaccessible before initialization. If
        // the inner function closes over a placeholder definition, we need to
        // mark the variable as maybe needing a dead zone check when we emit
        // bytecode.
        //
        // Note that body-level function declaration statements are always
        // hoisted to the top, so all accesses to free let variables need the
        // dead zone check.
        //
        // Subtlety: we don't need to check for closing over a non-dominating
        // lexical binding in a switch, as lexical declarations currently
        // disable syntax parsing. So a non-dominating but textually preceding
        // lexical declaration would have aborted syntax parsing, and a
        // textually following declaration would return true for
        // handler.isPlaceholderDefinition(dn) below.
        if (handler.isPlaceholderDefinition(dn) || bodyLevel)
            freeVariables[i].setIsHoistedUse();

        /* Mark the outer dn as escaping. */
        handler.setFlag(handler.getDefinitionNode(dn), PND_CLOSED);
    }

    PropagateTransitiveParseFlags(lazy, pc->sc);
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkFunctionDefinition(HandleAtom funAtom,
                                                    Node* pn, FunctionSyntaxKind kind,
                                                    bool* pbodyProcessed,
                                                    Node* assignmentForAnnexBOut)
{
    *pbodyProcessed = false;

    /* Function statements add a binding to the enclosing scope. */
    bool bodyLevel = pc->atBodyLevel();

    if (kind == Statement) {
        RootedPropertyName funName(context, funAtom->asPropertyName());
        *assignmentForAnnexBOut = null();

        if (!bodyLevel) {
            // Block-scoped functions cannot yet be parsed lazily.
            return abortIfSyntaxParser();
        }

        /*
         * Handle redeclaration and optimize cases where we can statically bind the
         * function (thereby avoiding JSOP_DEFFUN and dynamic name lookup).
         */

        if (DefinitionNode dn = pc->decls().lookupFirst(funName)) {
            if (dn == Definition::CONSTANT || dn == Definition::LET) {
                JSAutoByteString name;
                if (!AtomToPrintableString(context, funName, &name) ||
                    !report(ParseError, false, null(), JSMSG_REDECLARED_VAR,
                            Definition::kindString(dn), name.ptr()))
                {
                    return false;
                }
            }
        } else {
            if (pc->lexdeps.lookupDefn<SyntaxParseHandler>(funName))
                pc->lexdeps->remove(funName);

            if (!pc->define(tokenStream, funName, *pn, Definition::VAR))
                return false;
        }
    }

    if (kind == Arrow) {
        /* Arrow functions cannot yet be parsed lazily. */
        return abortIfSyntaxParser();
    }

    return true;
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
Parser<ParseHandler>::functionDef(InHandling inHandling, YieldHandling yieldHandling,
                                  HandleAtom funName, FunctionSyntaxKind kind,
                                  GeneratorKind generatorKind, InvokedPrediction invoked,
                                  Node* assignmentForAnnexBOut)
{
    MOZ_ASSERT_IF(kind == Statement, funName);

    /* Make a TOK_FUNCTION node. */
    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();
    handler.setBlockId(pn, pc->blockid());

    if (invoked)
        pn = handler.setLikelyIIFE(pn);

    bool bodyProcessed;
    if (!checkFunctionDefinition(funName, &pn, kind, &bodyProcessed, assignmentForAnnexBOut))
        return null();

    if (bodyProcessed)
        return pn;

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

    while (true) {
        if (functionArgsAndBody(inHandling, pn, fun, kind, generatorKind, directives,
                                &newDirectives))
        {
            break;
        }
        if (tokenStream.hadError() || directives == newDirectives)
            return null();

        // Assignment must be monotonic to prevent reparsing iloops
        MOZ_ASSERT_IF(directives.strict(), newDirectives.strict());
        MOZ_ASSERT_IF(directives.asmJS(), newDirectives.asmJS());
        directives = newDirectives;

        tokenStream.seek(start);

        // functionArgsAndBody may have already set pn->pn_body before failing.
        handler.setFunctionBody(pn, null());
    }

    return pn;
}

template <>
bool
Parser<FullParseHandler>::finishFunctionDefinition(ParseNode* pn, FunctionBox* funbox,
                                                   ParseNode* body)
{
    pn->pn_pos.end = pos().end;

    MOZ_ASSERT(pn->pn_funbox == funbox);
    MOZ_ASSERT(pn->pn_body->isKind(PNK_ARGSBODY));
    pn->pn_body->append(body);

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::finishFunctionDefinition(Node pn, FunctionBox* funbox,
                                                     Node body)
{
    // The LazyScript for a lazily parsed function needs to be constructed
    // while its ParseContext and associated lexdeps and inner functions are
    // still available.

    if (funbox->inWith())
        return abortIfSyntaxParser();

    size_t numFreeVariables = pc->lexdeps->count();
    size_t numInnerFunctions = pc->innerFunctions.length();

    RootedFunction fun(context, funbox->function());
    LazyScript* lazy = LazyScript::CreateRaw(context, fun, numFreeVariables, numInnerFunctions,
                                             versionNumber(), funbox->bufStart, funbox->bufEnd,
                                             funbox->startLine, funbox->startColumn);
    if (!lazy)
        return false;

    LazyScript::FreeVariable* freeVariables = lazy->freeVariables();
    size_t i = 0;
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront())
        freeVariables[i++] = LazyScript::FreeVariable(r.front().key());
    MOZ_ASSERT(i == numFreeVariables);

    GCPtrFunction* innerFunctions = lazy->innerFunctions();
    for (size_t i = 0; i < numInnerFunctions; i++)
        innerFunctions[i].init(pc->innerFunctions[i]);

    if (pc->sc->strict())
        lazy->setStrict();
    lazy->setGeneratorKind(funbox->generatorKind());
    if (funbox->isLikelyConstructorWrapper())
        lazy->setLikelyConstructorWrapper();
    if (funbox->isDerivedClassConstructor())
        lazy->setIsDerivedClassConstructor();
    if (funbox->needsHomeObject())
        lazy->setNeedsHomeObject();
    PropagateTransitiveParseFlags(funbox, lazy);

    fun->initLazyScript(lazy);
    return true;
}

template <>
bool
Parser<FullParseHandler>::functionArgsAndBody(InHandling inHandling, ParseNode* pn,
                                              HandleFunction fun, FunctionSyntaxKind kind,
                                              GeneratorKind generatorKind,
                                              Directives inheritedDirectives,
                                              Directives* newDirectives)
{
    ParseContext<FullParseHandler>* outerpc = pc;

    // Close over top level function definitions in modules.
    if (pc->sc->isModuleBox())
        pn->pn_dflags |= PND_CLOSED;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox* funbox = newFunctionBox(pn, fun, pc, inheritedDirectives, generatorKind);
    if (!funbox)
        return false;

    if (kind == DerivedClassConstructor)
        funbox->setDerivedClassConstructor();

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;

    // We need to roll back the block scope vector if syntax parsing fails.
    uint32_t oldBlockScopesLength = blockScopes.length();

    // Try a syntax parse for this inner function.
    do {
        // If we're assuming this function is an IIFE, always perform a full
        // parse to avoid the overhead of a lazy syntax-only parse. Although
        // the prediction may be incorrect, IIFEs are common enough that it
        // pays off for lots of code.
        if (pn->isLikelyIIFE() && !funbox->isGenerator())
            break;

        Parser<SyntaxParseHandler>* parser = handler.syntaxParser;
        if (!parser)
            break;

        {
            // Move the syntax parser to the current position in the stream.
            TokenStream::Position position(keepAtoms);
            tokenStream.tell(&position);
            if (!parser->tokenStream.seek(position, tokenStream))
                return false;

            ParseContext<SyntaxParseHandler> funpc(parser, outerpc, SyntaxParseHandler::null(),
                                                   funbox, newDirectives);
            if (!funpc.init(*parser))
                return false;

            if (!parser->functionArgsAndBodyGeneric(inHandling, yieldHandling,
                                                    SyntaxParseHandler::NodeGeneric, fun, kind))
            {
                if (parser->hadAbortedSyntaxParse()) {
                    // Try again with a full parse.
                    parser->clearAbortedSyntaxParse();
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
        }

        if (!addFreeVariablesFromLazyFunction(fun, pc))
            return false;

        PropagateTransitiveParseFlags(funbox, outerpc->sc);
        return true;
    } while (false);

    if (!blockScopes.resize(oldBlockScopesLength))
        return false;

    // Continue doing a full parse for this inner function.
    ParseContext<FullParseHandler> funpc(this, pc, pn, funbox, newDirectives);
    if (!funpc.init(*this))
        return false;

    if (!functionArgsAndBodyGeneric(inHandling, yieldHandling, pn, fun, kind))
        return false;

    if (!leaveFunction(pn, outerpc, kind))
        return false;

    /*
     * Fruit of the poisonous tree: if a closure contains a dynamic name access
     * (eval, with, etc), we consider the parent to do the same. The reason is
     * that the deoptimizing effects of dynamic name access apply equally to
     * parents: any local can be read at runtime.
     */
    PropagateTransitiveParseFlags(funbox, outerpc->sc);
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::functionArgsAndBody(InHandling inHandling, Node pn, HandleFunction fun,
                                                FunctionSyntaxKind kind,
                                                GeneratorKind generatorKind,
                                                Directives inheritedDirectives,
                                                Directives* newDirectives)
{
    ParseContext<SyntaxParseHandler>* outerpc = pc;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox* funbox = newFunctionBox(pn, fun, pc, inheritedDirectives, generatorKind);
    if (!funbox)
        return false;

    // Initialize early for possible flags mutation via destructuringExpr.
    ParseContext<SyntaxParseHandler> funpc(this, pc, handler.null(), funbox, newDirectives);
    if (!funpc.init(*this))
        return false;

    YieldHandling yieldHandling = generatorKind != NotGenerator ? YieldIsKeyword : YieldIsName;
    if (!functionArgsAndBodyGeneric(inHandling, yieldHandling, pn, fun, kind))
        return false;

    if (!leaveFunction(pn, outerpc, kind))
        return false;

    // This is a lazy function inner to another lazy function. Remember the
    // inner function so that if the outer function is eventually parsed we do
    // not need any further parsing or processing of the inner function.
    MOZ_ASSERT(fun->lazyScript());
    return outerpc->innerFunctions.append(fun);
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

    RootedObject enclosing(context, fun->lazyScript()->enclosingScope());
    Directives directives(/* strict = */ strict);
    FunctionBox* funbox = newFunctionBox(pn, fun, directives, generatorKind, enclosing);
    if (!funbox)
        return null();
    funbox->length = fun->nargs() - fun->hasRest();

    if (fun->lazyScript()->isDerivedClassConstructor())
        funbox->setDerivedClassConstructor();

    Directives newDirectives = directives;
    ParseContext<FullParseHandler> funpc(this, /* parent = */ nullptr, pn, funbox,
                                         &newDirectives);
    if (!funpc.init(*this))
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
    if (!functionArgsAndBodyGeneric(InAllowed, yieldHandling, pn, fun, syntaxKind)) {
        MOZ_ASSERT(directives == newDirectives);
        return null();
    }

    if (fun->isNamedLambda()) {
        if (AtomDefnPtr p = pc->lexdeps->lookup(fun->name())) {
            Definition* dn = p.value().get<FullParseHandler>();
            if (!ConvertDefinitionToNamedLambdaUse(tokenStream, pc, funbox, dn))
                return nullptr;
        }
    }

    Rooted<Bindings> bindings(context, funbox->bindings);
    if (!pc->generateBindings(context, tokenStream, alloc, &bindings))
        return null();
    funbox->bindings = bindings;

    if (!FoldConstants(context, &pn, this))
        return null();

    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionArgsAndBodyGeneric(InHandling inHandling,
                                                 YieldHandling yieldHandling, Node pn,
                                                 HandleFunction fun, FunctionSyntaxKind kind)
{
    // Given a properly initialized parse context, try to parse an actual
    // function without concern for conversion to strict mode, use of lazy
    // parsing and such.

    bool hasRest;
    if (!functionArguments(yieldHandling, kind, pn, &hasRest))
        return false;

    FunctionBox* funbox = pc->sc->asFunctionBox();

    fun->setArgCount(pc->numArgs());
    if (hasRest)
        fun->setHasRest();

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
        if (!checkStrictBinding(propertyName, pn))
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

    handler.setEndPosition(body, pos().begin);

    return finishFunctionDefinition(pn, funbox, body);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkYieldNameValidity()
{
    // In star generators and in JS >= 1.7, yield is a keyword.  Otherwise in
    // strict mode, yield is a future reserved word.
    if (pc->isStarGenerator() || versionNumber() >= JSVERSION_1_7 || pc->sc->strict()) {
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

    // ES6 Annex B.3.4 says we can parse function declarations unbraced under if or
    // else as if it were braced. That is, |if (x) function f() {}| is parsed as
    // |if (x) { function f() {} }|.
    Maybe<AutoPushStmtInfoPC> synthesizedStmtInfoForAnnexB;
    Node synthesizedBlockForAnnexB = null();
    StmtInfoPC *stmt = pc->innermostStmt();
    if (!pc->sc->strict() && stmt) {
        if (stmt->type == StmtType::IF || stmt->type == StmtType::ELSE) {
            if (!abortIfSyntaxParser())
                return null();

            synthesizedStmtInfoForAnnexB.emplace(*this, StmtType::BLOCK);
            synthesizedBlockForAnnexB = pushLexicalScope(*synthesizedStmtInfoForAnnexB);
            if (!synthesizedBlockForAnnexB)
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

    Node assignmentForAnnexB;
    Node fun = functionDef(InAllowed, yieldHandling, name, Statement, generatorKind,
                           PredictUninvoked, &assignmentForAnnexB);
    if (!fun)
        return null();

    if (assignmentForAnnexB) {
        fun = handler.newFunctionDefinitionForAnnexB(fun, assignmentForAnnexB);
        if (!fun)
            return null();
    }

    // Note that we may have synthesized a block for Annex B.3.4 without
    // having synthesized an assignment for Annex B.3.3, e.g.,
    //
    //   let f = 1;
    //   {
    //     if (1) function f() {}
    //   }
    if (synthesizedBlockForAnnexB) {
        Node body = handler.newStatementList(pc->blockid(), handler.getPosition(fun));
        if (!body)
            return null();
        handler.addStatementToList(body, fun, pc);
        handler.setLexicalScopeBody(synthesizedBlockForAnnexB, body);
        return synthesizedBlockForAnnexB;
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
    return functionDef(InAllowed, yieldHandling, name, Expression, generatorKind, invoked);
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

    pc->sc->asFunctionBox()->useAsm = true;

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
            pc->sc->setExplicitUseStrict();
            if (!pc->sc->strict()) {
                if (pc->sc->isFunctionBox()) {
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
                pc->sc->strictScript = true;
            }
        } else if (directive == context->names().useAsm) {
            if (pc->sc->isFunctionBox())
                return asmJS(list);
            return report(ParseWarning, false, pn, JSMSG_USE_ASM_DIRECTIVE_FAIL);
        }
    }
    return true;
}

/*
 * Parse the statements in a block, creating a StatementList node that lists
 * the statements.  If called from block-parsing code, the caller must match
 * '{' before and '}' after.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statements(YieldHandling yieldHandling)
{
    JS_CHECK_RECURSION(context, return null());

    Node pn = handler.newStatementList(pc->blockid(), pos());
    if (!pn)
        return null();

    Node saveBlock = pc->blockNode;
    pc->blockNode = pn;

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

        handler.addStatementToList(pn, next, pc);
    }

    /*
     * Handle the case where there was a let declaration under this block.  If
     * it replaced pc->blockNode with a new block node then we must refresh pn
     * and then restore pc->blockNode.
     */
    if (pc->blockNode != pn)
        pn = pc->blockNode;
    pc->blockNode = saveBlock;
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
bool
Parser<ParseHandler>::reportRedeclaration(Node pn, Definition::Kind redeclKind, HandlePropertyName name)
{
    JSAutoByteString printable;
    if (!AtomToPrintableString(context, name, &printable))
        return false;

    StmtInfoPC* stmt = LexicalLookup(pc, name);
    if (stmt && stmt->type == StmtType::CATCH) {
        report(ParseError, false, pn, JSMSG_REDECLARED_CATCH_IDENTIFIER, printable.ptr());
    } else {
        if (redeclKind == Definition::ARG) {
            report(ParseError, false, pn, JSMSG_REDECLARED_PARAM, printable.ptr());
        } else {
            report(ParseError, false, pn, JSMSG_REDECLARED_VAR, Definition::kindString(redeclKind),
                   printable.ptr());
        }
    }
    return false;
}

/*
 * Define a lexical binding in a block, or comprehension scope. pc must
 * already be in such a scope.
 *
 * Throw a SyntaxError if 'atom' is an invalid name. Otherwise create a
 * property for the new variable on the block object,
 * pc->topStmtScope()->staticScope; populate
 * data->pn->pn_{op,cookie,defn,dflags}; and stash a pointer to data->pn in a
 * slot of the block object.
 */
template <>
/* static */ bool
Parser<FullParseHandler>::bindLexical(BindData<FullParseHandler>* data,
                                      HandlePropertyName name, Parser<FullParseHandler>* parser)
{
    ParseContext<FullParseHandler>* pc = parser->pc;
    ParseNode* pn = data->nameNode();
    if (!parser->checkStrictBinding(name, pn))
        return false;

    ExclusiveContext* cx = parser->context;

    // Most lexical declaration patterns can't bind the name 'let'.
    if (data->mustNotBindLet() && name == cx->names().let) {
        parser->report(ParseError, false, pn, JSMSG_LEXICAL_DECL_DEFINES_LET);
        return false;
    }

    Rooted<StaticBlockScope*> blockScope(cx, data->letData().blockScope);

    uint32_t index = StaticBlockScope::LOCAL_INDEX_LIMIT;
    if (blockScope) {
        // Leave the scope coordinate free on global lexicals.
        //
        // For block-level lets, assign block-local index to pn->pn_scopecoord
        // right away. The emitter will adjust the node's slot based on its
        // stack depth model -- and, for global and eval code,
        // js::frontend::CompileScript will adjust the slot again to include
        // script->nfixed and body-level lets.
        //
        // XXXshu: I should like to optimize global lexicals, but we rely on
        // being able to clone JSScripts to run on multiple globals and to be
        // able to parse scripts off-thread in a different compartment.
        if (!blockScope->isGlobal()) {
            index = blockScope->numVariables();
            if (index >= StaticBlockScope::LOCAL_INDEX_LIMIT) {
                parser->report(ParseError, false, pn, data->letData().overflow);
                return false;
            }
            if (!pn->pn_scopecoord.setSlot(parser->tokenStream, index))
                return false;
        }
    } else if (!pc->sc->isGlobalContext()) {
        // If we don't have a block object and are parsing a function
        // body-level let, use a bogus index. It is adjusted when creating the
        // function's Bindings. See ParseContext::generateFunctionBindings and
        // AppendPackedBindings.
        index = 0;
        if (!pn->pn_scopecoord.setSlot(parser->tokenStream, index))
            return false;
    }

    Definition::Kind bindingKind;
    if (pn->isImport())
        bindingKind = Definition::IMPORT;
    else if (data->isConst())
        bindingKind = Definition::CONSTANT;
    else
        bindingKind = Definition::LET;

    Definition* dn = pc->decls().lookupFirst(name);

    /*
     * For bindings that are hoisted to the beginning of the block/function,
     * define() right now. Otherwise, delay define until pushLetScope.
     */
    if (data->letData().varContext == HoistVars) {
        if (dn) {
            // The reason we compare using >= instead of == on the block id is to
            // detect redeclarations where a 'var' binding first appeared in a
            // nested block: |{ var x; } let x;|
            if (dn->pn_blockid >= pc->blockid()) {
                // XXXshu Used only for phasing in block-scope function early
                // XXXshu errors.
                // XXXshu
                // XXXshu Back out when major version >= 50. See [1].
                // XXXshu
                // XXXshu [1] https://bugzilla.mozilla.org/show_bug.cgi?id=1235590#c10
                if (pn->isKind(PNK_FUNCTION) && dn->isKind(PNK_FUNCTION) && !pc->sc->strict()) {
                    if (!parser->makeDefIntoUse(dn, pn, name))
                        return false;

                    MOZ_ASSERT(blockScope);
                    Shape* shape = blockScope->lastProperty()->search(cx, NameToId(name));
                    MOZ_ASSERT(shape);
                    uint32_t oldDefIndex = blockScope->shapeToIndex(*shape);
                    blockScope->updateDefinitionParseNode(oldDefIndex, dn,
                                                          reinterpret_cast<Definition*>(pn));

                    parser->addTelemetry(JSCompartment::DeprecatedBlockScopeFunRedecl);
                    JSAutoByteString bytes;
                    if (!AtomToPrintableString(cx, name, &bytes))
                        return false;
                    if (!parser->report(ParseWarning, false, null(),
                                        JSMSG_DEPRECATED_BLOCK_SCOPE_FUN_REDECL,
                                        bytes.ptr()))
                    {
                        return false;
                    }

                    return true;
                }

                return parser->reportRedeclaration(pn, dn->kind(), name);
            }

            // Lexical declarations inside catch blocks need special
            // attention. The catch parameter is in the enclosing block scope
            // from the catch body, but lexical declarations inside the catch
            // body block cannot shadow any catch parameter name.
            //
            // The reason the catch parameter is not in the same scope as the
            // catch body is to satisfy Annex B.3.5, which allows 'var'
            // redeclaration of the catch parameter but not of lexical
            // bindings introduced in the catch body.
            StmtInfoPC* enclosingStmt = pc->innermostScopeStmt()
                                      ? pc->innermostScopeStmt()->enclosing
                                      : nullptr;
            if (enclosingStmt && enclosingStmt->type == StmtType::CATCH &&
                dn->pn_blockid == enclosingStmt->blockid)
            {
                MOZ_ASSERT(LexicalLookup(pc, name) == enclosingStmt);
                return parser->reportRedeclaration(pn, dn->kind(), name);
            }
        }

        if (!pc->define(parser->tokenStream, name, pn, bindingKind))
            return false;
    }

    if (blockScope) {
        if (!blockScope->isGlobal()) {
            bool redeclared;
            RootedId id(cx, NameToId(name));
            RootedShape shape(cx, StaticBlockScope::addVar(cx, blockScope, id,
                                                           data->isConst(), index, &redeclared));
            if (!shape) {
                if (redeclared) {
                    // The only way to be redeclared without a previous definition is if we're in a
                    // comma separated list in a DontHoistVars block, so a let block of for header. In
                    // that case, we must be redeclaring the same type of definition as we're trying to
                    // make.
                    Definition::Kind dnKind = dn ? dn->kind() : bindingKind;
                    parser->reportRedeclaration(pn, dnKind, name);
                }
                return false;
            }

            /* Store pn in the static block object. */
            blockScope->setDefinitionParseNode(index, reinterpret_cast<Definition*>(pn));
        }
    } else {
        // Body-level lets are hoisted and need to have been defined via
        // pc->define above.
        MOZ_ASSERT(data->letData().varContext == HoistVars);
        MOZ_ASSERT(pc->decls().lookupFirst(name));
    }

    return true;
}

template <>
/* static */ bool
Parser<SyntaxParseHandler>::bindLexical(BindData<SyntaxParseHandler>* data,
                                        HandlePropertyName name, Parser<SyntaxParseHandler>* parser)
{
    if (!parser->checkStrictBinding(name, data->nameNode()))
        return false;

    return true;
}

template <typename ParseHandler, class Op>
static inline bool
ForEachLetDef(TokenStream& ts, ParseContext<ParseHandler>* pc,
              HandleStaticBlockScope blockScope, Op op)
{
    for (Shape::Range<CanGC> r(ts.context(), blockScope->lastProperty()); !r.empty(); r.popFront()) {
        Shape& shape = r.front();

        /* Beware the destructuring dummy slots. */
        if (JSID_IS_INT(shape.propid()))
            continue;

        if (!op(ts, pc, blockScope, shape, JSID_TO_ATOM(shape.propid())))
            return false;
    }
    return true;
}

template <typename ParseHandler>
struct PopLetDecl {
    bool operator()(TokenStream&, ParseContext<ParseHandler>* pc, HandleStaticBlockScope,
                    const Shape&, JSAtom* atom)
    {
        pc->popLetDecl(atom);
        return true;
    }
};

// We compute the maximum block scope depth, in slots, of a compilation unit at
// parse-time.  Each nested statement has a field indicating the maximum block
// scope depth that is nested inside it.  When we leave a nested statement, we
// add the number of slots in the statement to the nested depth, and use that to
// update the maximum block scope depth of the outer statement or parse
// context.  In the end, pc->blockScopeDepth will indicate the number of slots
// to reserve in the fixed part of a stack frame.
//
template <typename ParseHandler>
static void
AccumulateBlockScopeDepth(ParseContext<ParseHandler>* pc)
{
    StmtInfoPC* stmt = pc->innermostStmt();
    uint32_t innerDepth = stmt->innerBlockScopeDepth;
    StmtInfoPC* outer = stmt->enclosing;

    if (stmt->isBlockScope)
        innerDepth += stmt->staticScope->template as<StaticBlockScope>().numVariables();

    if (outer) {
        if (outer->innerBlockScopeDepth < innerDepth)
            outer->innerBlockScopeDepth = innerDepth;
    } else {
        if (pc->blockScopeDepth < innerDepth)
            pc->blockScopeDepth = innerDepth;
    }
}

template <typename ParseHandler>
Parser<ParseHandler>::AutoPushStmtInfoPC::AutoPushStmtInfoPC(Parser<ParseHandler>& parser,
                                                             StmtType type)
  : parser_(parser),
    stmt_(parser.context)
{
    stmt_.blockid = parser.pc->blockid();
    parser.pc->stmtStack.push(&stmt_, type);
}

template <typename ParseHandler>
Parser<ParseHandler>::AutoPushStmtInfoPC::AutoPushStmtInfoPC(Parser<ParseHandler>& parser,
                                                             StmtType type,
                                                             NestedStaticScope& staticScope)
  : parser_(parser),
    stmt_(parser.context)
{
    stmt_.blockid = parser.pc->blockid();
    staticScope.initEnclosingScopeFromParser(parser.pc->innermostStaticScope());
    parser.pc->stmtStack.pushNestedScope(&stmt_, type, staticScope);
}

template <typename ParseHandler>
Parser<ParseHandler>::AutoPushStmtInfoPC::~AutoPushStmtInfoPC()
{
    // While this destructor is infallible, it is preferable to fail fast on
    // aborted syntax parses.
    if (parser_.hadAbortedSyntaxParse())
        return;

    ParseContext<ParseHandler>* pc = parser_.pc;
    TokenStream& ts = parser_.tokenStream;

    MOZ_ASSERT(pc->innermostStmt() == &stmt_);
    RootedNestedStaticScope scope(parser_.context, stmt_.staticScope);

    AccumulateBlockScopeDepth(pc);
    pc->stmtStack.pop();

    if (scope) {
        if (scope->is<StaticBlockScope>()) {
            RootedStaticBlockScope blockScope(parser_.context, &scope->as<StaticBlockScope>());
            MOZ_ASSERT(!blockScope->inDictionaryMode());
            ForEachLetDef(ts, pc, blockScope, PopLetDecl<ParseHandler>());
        }
        scope->resetEnclosingScopeFromParser();
    }
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::AutoPushStmtInfoPC::generateBlockId()
{
    return parser_.generateBlockId(stmt_.staticScope, &stmt_.blockid);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::AutoPushStmtInfoPC::makeInnermostLexicalScope(StaticBlockScope& blockScope)
{
    MOZ_ASSERT(parser_.pc->stmtStack.innermost() == &stmt_);
    parser_.pc->stmtStack.makeInnermostLexicalScope(blockScope);
    return generateBlockId();
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
static inline bool
HasOuterLexicalBinding(ParseContext<ParseHandler>* pc, StmtInfoPC* stmt, HandleAtom atom)
{
    while (stmt->enclosingScope) {
        stmt = LexicalLookup(pc, atom, stmt->enclosingScope);
        if (!stmt)
            break;
        if (stmt->type == StmtType::BLOCK)
            return true;
    }

    // Even if the binding doesn't appear in any blocks, it might still be a
    // body-level lexical.
    return pc->isBodyLevelLexicallyDeclaredName(atom);
}

template <typename ParseHandler>
/* static */ bool
Parser<ParseHandler>::bindVar(BindData<ParseHandler>* data,
                              HandlePropertyName name, Parser<ParseHandler>* parser)
{
    ExclusiveContext* cx = parser->context;
    ParseContext<ParseHandler>* pc = parser->pc;
    Node pn = data->nameNode();

    /* Default best op for pn is JSOP_GETNAME; we'll try to improve below. */
    parser->handler.setOp(pn, JSOP_GETNAME);

    if (!parser->checkStrictBinding(name, pn))
        return false;

    // Special case var bindings in for-of heads for bailing out of syntax
    // parsing to satisfy early errors per ES6 Annex B.3.5.
    //
    // 'var' bindings in for-of heads do not trigger Annex B.3.5 (i.e.,
    // regular lexical redeclaration early errors apply). When syntax parsing
    // we do not have enough binding information to detect early errors, so
    // abort when binding vars in for-of head inside catch. We cannot use stmt
    // as syntax parsing does not keep enough info to find the correct scope
    // via LexicalLookup above.
    if (data->isForOf) {
        for (StmtInfoPC* scopeStmt = pc->innermostScopeStmt();
             scopeStmt;
             scopeStmt = scopeStmt->enclosingScope)
        {
            if (scopeStmt->type == StmtType::CATCH) {
                if (!parser->abortIfSyntaxParser())
                    return false;
            }
        }
    }

    StmtInfoPC* stmt = LexicalLookup(pc, name);

    if (stmt && stmt->type == StmtType::WITH) {
        // Do not deoptimize if we are binding a synthesized 'var' binding for
        // Annex B.3.3, which states that the synthesized binding is to go on
        // the nearest VariableEnvironment. Deoptimizing here would
        // erroneously emit NAME ops when assigning to the Annex B 'var'.
        if (!data->isAnnexB()) {
            parser->handler.setFlag(pn, PND_DEOPTIMIZED);
            if (pc->sc->isFunctionBox()) {
                FunctionBox* funbox = pc->sc->asFunctionBox();
                funbox->setMightAliasLocals();
            }

            // Make sure to indicate the need to deoptimize the script's
            // arguments object. Mark the function as if it contained a
            // debugger statement, which will deoptimize arguments as much as
            // possible.
            if (name == cx->names().arguments)
                pc->sc->setHasDebuggerStatement();
        }

        // Find the nearest enclosing non-with scope that defined name, if
        // any, for redeclaration checks below.
        while (stmt && stmt->type == StmtType::WITH) {
            if (stmt->enclosingScope)
                stmt = LexicalLookup(pc, name, stmt->enclosingScope);
            else
                stmt = nullptr;
        }
    }

    DefinitionList::Range defs = pc->decls().lookupMulti(name);
    MOZ_ASSERT_IF(stmt, !defs.empty());

    if (defs.empty())
        return pc->define(parser->tokenStream, name, pn, Definition::VAR);

    // ES6 Annex B.3.5 allows for var declarations inside catch blocks with
    // the same name as the catch parameter.
    bool nameIsCatchParam = stmt && stmt->type == StmtType::CATCH;
    bool declaredVarInCatchBody = false;
    if (nameIsCatchParam && !data->isForOf && !HasOuterLexicalBinding(pc, stmt, name)) {
        declaredVarInCatchBody = true;

        // Deoptimize the original name node, set the shadowing lexical
        // name as aliased. Consider the following:
        //
        // try {} catch (e) { var e = 42; }
        //
        // While a new var 'e' is declared, the initializer '= 42' needs
        // to be assigned to the lexically bound catch parameter
        // 'e'. Deoptimizing the original parse node ensures that happen
        // by emitting {BIND,SET}NAME ops.
        //
        // (Ideally, the 'e' in 'e = 42' can be linked up as a use to the
        // def of the catch parameter. However, in practice this is messy
        // because we then need to emit the synthesized var name node to
        // ensure that functionless scopes get the proper DEFVAR emits.)
        parser->handler.setFlag(pn, PND_DEOPTIMIZED);

        // Synthesize a new 'var' binding if one does not exist.
        DefinitionNode last = pc->decls().lookupLast(name);
        Definition::Kind lastKind = parser->handler.getDefinitionKind(last);
        if (last && lastKind != Definition::VAR && lastKind != Definition::ARG) {
            parser->handler.setFlag(parser->handler.getDefinitionNode(last), PND_CLOSED);

            Node synthesizedVarName = parser->newName(name);
            if (!synthesizedVarName)
                return false;
            if (!pc->define(parser->tokenStream, name, synthesizedVarName, Definition::VAR,
                            /* declaringVarInCatchBody = */ true))
            {
                return false;
            }
        }
    }

    /*
     * There was a previous declaration with the same name. The standard
     * disallows several forms of redeclaration. Critically,
     *   let (x) { var x; } // error
     * is not allowed which allows us to turn any non-error redeclaration
     * into a use of the initial declaration.
     */
    DefinitionNode dn = defs.front<ParseHandler>();
    Definition::Kind dn_kind = parser->handler.getDefinitionKind(dn);
    if (dn_kind == Definition::ARG) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(cx, name, &bytes))
            return false;
        if (!parser->report(ParseExtraWarning, false, pn, JSMSG_VAR_HIDES_ARG, bytes.ptr()))
            return false;
    } else {
        bool error = (dn_kind == Definition::IMPORT ||
                      dn_kind == Definition::CONSTANT ||
                      (dn_kind == Definition::LET && !declaredVarInCatchBody));

        if (parser->options().extraWarningsOption
            ? data->op() != JSOP_DEFVAR || dn_kind != Definition::VAR
            : error)
        {
            JSAutoByteString bytes;
            if (!AtomToPrintableString(cx, name, &bytes))
                return false;

            ParseReportKind reporter = error ? ParseError : ParseExtraWarning;
            if (!(nameIsCatchParam && !declaredVarInCatchBody
                  ? parser->report(reporter, false, pn,
                                   JSMSG_REDECLARED_CATCH_IDENTIFIER, bytes.ptr())
                  : parser->report(reporter, false, pn, JSMSG_REDECLARED_VAR,
                                   Definition::kindString(dn_kind), bytes.ptr())))
            {
                return false;
            }
        }
    }

    parser->handler.linkUseToDef(pn, dn);
    return true;
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
    if (!report(ParseStrictError, pc->sc->strict(), target, msg))
        return false;

    handler.markAsSetCall(target);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteNameUse(HandlePropertyName name, Node pn)
{
    /*
     * The asm.js validator does all its own symbol-table management so, as an
     * optimization, avoid doing any work here. Use-def links are only necessary
     * for emitting bytecode and successfully-validated asm.js does not emit
     * bytecode. (On validation failure, the asm.js module is reparsed.)
     */
    if (pc->useAsmOrInsideUseAsm())
        return true;

    StmtInfoPC* stmt = LexicalLookup(pc, name);

    DefinitionList::Range defs = pc->decls().lookupMulti(name);

    DefinitionNode dn;
    if (!defs.empty()) {
        dn = defs.front<ParseHandler>();
    } else {
        /*
         * No definition before this use in any lexical scope.
         * Create a placeholder definition node to either:
         * - Be adopted when we parse the real defining
         *   declaration, or
         * - Be left as a free variable definition if we never
         *   see the real definition.
         */
        dn = getOrCreateLexicalDependency(pc, name);
        if (!dn)
            return false;
    }

    handler.linkUseToDef(pn, dn);

    if (stmt) {
        if (stmt->type == StmtType::WITH) {
            handler.setFlag(pn, PND_DEOPTIMIZED);
        } else if (stmt->type == StmtType::SWITCH && stmt->isBlockScope) {
            // See comments above StmtInfoPC and switchStatement for how
            // firstDominatingLexicalInCase is computed.
            MOZ_ASSERT(stmt->firstDominatingLexicalInCase <= stmt->staticBlock().numVariables());
            handler.markMaybeUninitializedLexicalUseInSwitch(pn, dn,
                                                             stmt->firstDominatingLexicalInCase);
        }
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::bindUninitialized(BindData<FullParseHandler>* data, HandlePropertyName name,
                                            ParseNode* pn)
{
    data->setNameNode(pn);
    return data->bind(name, this);
}

template <>
bool
Parser<FullParseHandler>::bindUninitialized(BindData<FullParseHandler>* data, ParseNode* pn)
{
    RootedPropertyName name(context, pn->name());
    return bindUninitialized(data, name, pn);
}

template <>
bool
Parser<FullParseHandler>::bindInitialized(BindData<FullParseHandler>* data, HandlePropertyName name,
                                          ParseNode* pn)
{
    if (!bindUninitialized(data, name, pn))
        return false;

    /*
     * Select the appropriate name-setting opcode, respecting eager selection
     * done by the data->bind function.
     */
    if (data->op() == JSOP_DEFLET || data->op() == JSOP_DEFCONST)
        pn->setOp(pn->pn_scopecoord.isFree() ? JSOP_INITGLEXICAL : JSOP_INITLEXICAL);
    else if (pn->pn_dflags & PND_BOUND)
        pn->setOp(JSOP_SETLOCAL);
    else
        pn->setOp(JSOP_SETNAME);

    if (data->op() == JSOP_DEFCONST)
        pn->pn_dflags |= PND_CONST;

    pn->markAsAssigned();
    return true;
}

template <>
bool
Parser<FullParseHandler>::bindInitialized(BindData<FullParseHandler>* data, ParseNode* pn)
{
    RootedPropertyName name(context, pn->name());
    return bindInitialized(data, name, pn);
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringName(BindData<FullParseHandler>* data, ParseNode* expr)
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
    if (data) {
        // Destructuring patterns in declarations must only contain
        // unparenthesized names.
        if (!handler.isUnparenthesizedName(expr)) {
            report(ParseError, false, expr, JSMSG_NO_VARIABLE_NAME);
            return false;
        }

        return bindInitialized(data, expr);
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
        if (!reportIfArgumentsEvalTarget(expr))
            return false;

        // We may be called on a name node that has already been
        // specialized, in the very weird "for (var [x] = i in o) ..."
        // case. See bug 558633.
        //
        // XXX Is this necessary with the changes in bug 1164741?  This is
        //     likely removable now.
        handler.maybeDespecializeSet(expr);

        handler.markAsAssigned(expr);
        return true;
    }

    // Nothing further to do for property accesses.
    MOZ_ASSERT(handler.isPropertyAccess(expr));
    return true;
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringPattern(BindData<FullParseHandler>* data, ParseNode* pattern);

template <>
bool
Parser<FullParseHandler>::checkDestructuringObject(BindData<FullParseHandler>* data,
                                                   ParseNode* objectPattern)
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
            if (!checkDestructuringPattern(data, target))
                return false;
        } else {
            if (!checkDestructuringName(data, target))
                return false;
        }
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::checkDestructuringArray(BindData<FullParseHandler>* data,
                                                  ParseNode* arrayPattern)
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
            if (!checkDestructuringPattern(data, target))
                return false;
        } else {
            if (!checkDestructuringName(data, target))
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
 * pc->inDeclDestructuring clear, so the lvalue expressions in the
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
 * pc->inDeclDestructuring set, which directs primaryExpr to leave
 * whatever name nodes it creates unconnected.  Then, here in
 * checkDestructuringPattern, we require the pattern's property value
 * positions to be simple names, and define them as appropriate to the
 * context.  For these calls, |data| points to the right sort of
 * BindData.
 */
template <>
bool
Parser<FullParseHandler>::checkDestructuringPattern(BindData<FullParseHandler>* data, ParseNode* pattern)
{
    if (pattern->isKind(PNK_ARRAYCOMP)) {
        report(ParseError, false, pattern, JSMSG_ARRAY_COMP_LEFTSIDE);
        return false;
    }

    if (pattern->isKind(PNK_ARRAY))
        return checkDestructuringArray(data, pattern);
    return checkDestructuringObject(data, pattern);
}

template <>
bool
Parser<SyntaxParseHandler>::checkDestructuringPattern(BindData<SyntaxParseHandler>* data, Node pattern)
{
    return abortIfSyntaxParser();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringExpr(YieldHandling yieldHandling, BindData<ParseHandler>* data,
                                        TokenKind tt)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));

    pc->inDeclDestructuring = true;
    PossibleError possibleError(*this);
    Node pn = primaryExpr(yieldHandling, TripledotProhibited,
                          &possibleError, tt);

    // Resolve asap instead of checking since we already know that we are
    // destructuring.
    possibleError.setResolved();
    pc->inDeclDestructuring = false;
    if (!pn)
        return null();
    if (!checkDestructuringPattern(data, pn))
        return null();
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringExprWithoutYield(YieldHandling yieldHandling,
                                                    BindData<ParseHandler>* data, TokenKind tt,
                                                    unsigned msg)
{
    uint32_t startYieldOffset = pc->lastYieldOffset;
    Node res = destructuringExpr(yieldHandling, data, tt);
    if (res && pc->lastYieldOffset != startYieldOffset) {
        reportWithOffset(ParseError, false, pc->lastYieldOffset,
                         msg, js_yield_str);
        return null();
    }
    return res;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::pushLexicalScope(HandleStaticBlockScope blockScope,
                                       AutoPushStmtInfoPC& stmt)
{
    ObjectBox* blockbox = newObjectBox(blockScope);
    if (!blockbox)
        return null();

    Node pn = handler.newLexicalScope(blockbox);
    if (!pn)
        return null();

    blockScope->initEnclosingScopeFromParser(pc->innermostStaticScope());
    if (!stmt.makeInnermostLexicalScope(*blockScope))
        return null();
    handler.setBlockId(pn, stmt->blockid);
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::pushLexicalScope(AutoPushStmtInfoPC& stmt)
{
    RootedStaticBlockScope blockScope(context, StaticBlockScope::create(context));
    if (!blockScope)
        return null();

    return pushLexicalScope(blockScope, stmt);
}

struct AddLetDecl
{
    uint32_t blockid;

    explicit AddLetDecl(uint32_t blockid) : blockid(blockid) {}

    bool operator()(TokenStream& ts, ParseContext<FullParseHandler>* pc,
                    HandleStaticBlockScope blockScope, const Shape& shape, JSAtom*)
    {
        ParseNode* def = (ParseNode*) blockScope->getSlot(shape.slot()).toPrivate();
        def->pn_blockid = blockid;
        RootedPropertyName name(ts.context(), def->name());
        return pc->define(ts, name, def, Definition::LET);
    }
};

template <>
ParseNode*
Parser<FullParseHandler>::pushLetScope(HandleStaticBlockScope blockScope, AutoPushStmtInfoPC& stmt)
{
    MOZ_ASSERT(blockScope);
    ParseNode* pn = pushLexicalScope(blockScope, stmt);
    if (!pn)
        return null();

    pn->pn_dflags |= PND_LEXICAL;

    /* Populate the new scope with decls found in the head with updated blockid. */
    if (!ForEachLetDef(tokenStream, pc, blockScope, AddLetDecl(stmt->blockid)))
        return null();

    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::pushLetScope(HandleStaticBlockScope blockScope,
                                         AutoPushStmtInfoPC& stmt)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::blockStatement(YieldHandling yieldHandling, unsigned errorNumber)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    AutoPushStmtInfoPC stmtInfo(*this, StmtType::BLOCK);
    if (!stmtInfo.generateBlockId())
        return null();

    Node list = statements(yieldHandling);
    if (!list)
        return null();

    MUST_MATCH_TOKEN_MOD(TOK_RC, TokenStream::Operand, errorNumber);
    return list;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newBindingNode(PropertyName* name, bool functionScope, VarContext varContext)
{
    /*
     * If this name is being injected into an existing block/function, see if
     * it has already been declared or if it resolves an outstanding lexdep.
     * Otherwise, this is a let block/expr that introduces a new scope and thus
     * shadows existing decls and doesn't resolve existing lexdeps. Duplicate
     * names are caught by bindLet.
     */
    if (varContext == HoistVars) {
        if (AtomDefnPtr p = pc->lexdeps->lookup(name)) {
            DefinitionNode lexdep = p.value().get<ParseHandler>();
            MOZ_ASSERT(handler.getDefinitionKind(lexdep) == Definition::PLACEHOLDER);

            Node pn = handler.getDefinitionNode(lexdep);
            if (handler.dependencyCovered(pn, pc->blockid(), functionScope)) {
                handler.setBlockId(pn, pc->blockid());
                pc->lexdeps->remove(p);
                handler.setPosition(pn, pos());
                return pn;
            }
        }
    }

    /* Make a new node for this declarator name (or destructuring pattern). */
    return newName(name);
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
Parser<ParseHandler>::declarationPattern(Node decl, TokenKind tt, BindData<ParseHandler>* data,
                                         bool initialDeclaration, YieldHandling yieldHandling,
                                         ParseNodeKind* forHeadKind,
                                         Node* forInOrOfExpression)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LB) ||
               tokenStream.isCurrentTokenType(TOK_LC));

    Node pattern;
    {
        pc->inDeclDestructuring = true;

        PossibleError possibleError(*this);
        pattern = primaryExpr(yieldHandling, TripledotProhibited,
                              &possibleError, tt);

        // Resolve asap instead of checking since we already know that we are
        // destructuring.
        possibleError.setResolved();
        pc->inDeclDestructuring = false;
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
            data->isForOf = true;
            *forHeadKind = PNK_FOROF;
        } else {
            *forHeadKind = PNK_FORHEAD;
        }

        if (*forHeadKind != PNK_FORHEAD) {
            // |for (const ... in ...);| and |for (const ... of ...);| are
            // syntax errors for now.  We'll fix this in bug 449811.
            if (handler.declarationIsConst(decl)) {
                report(ParseError, false, pattern, JSMSG_BAD_CONST_DECL);
                return null();
            }

            if (!checkDestructuringPattern(data, pattern))
                return null();

            *forInOrOfExpression = expressionAfterForInOrOf(*forHeadKind, yieldHandling);
            if (!*forInOrOfExpression)
                return null();

            return pattern;
        }
    }

    // See comment below for bindBeforeInitializer in the code that
    // handles the non-destructuring case.
    bool bindBeforeInitializer = handler.declarationIsVar(decl);
    if (bindBeforeInitializer) {
        if (!checkDestructuringPattern(data, pattern))
            return null();
    }

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

    if (!bindBeforeInitializer) {
        if (!checkDestructuringPattern(data, pattern))
            return null();
    }

    return handler.newBinary(PNK_ASSIGN, pattern, init);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::initializerInNameDeclaration(Node decl, Node binding,
                                                   Handle<PropertyName*> name,
                                                   BindData<ParseHandler>* data,
                                                   bool initialDeclaration,
                                                   YieldHandling yieldHandling,
                                                   ParseNodeKind* forHeadKind,
                                                   Node* forInOrOfExpression)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_ASSIGN));

    // Lexical bindings can't be accessed until initialized.  A declaration
    // of the form |let x = x| raises a ReferenceError, as the 'x' on the
    // RHS accesses the binding before it's initialized.
    //
    // If we're not parsing a lexical declaration, bind the name now.
    // Otherwise we must wait until after parsing the initializing
    // assignment.
    bool bindBeforeInitializer = handler.declarationIsVar(decl);
    if (bindBeforeInitializer) {
        if (!data->bind(name, this))
            return false;
    }

    Node initializer = assignExpr(forHeadKind ? InProhibited : InAllowed,
                                  yieldHandling, TripledotProhibited);
    if (!initializer)
        return false;

    bool performAssignment = true;
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
                if (!handler.declarationIsVar(decl)) {
                    report(ParseError, false, binding, JSMSG_BAD_FOR_LEFTSIDE);
                    return false;
                }

                // This leaves only initialized for-in |var| declarations.  ES6
                // forbids these, yet they sadly still occur, rarely, on the
                // web.  *Don't* assign, and warn about this invalid syntax to
                // incrementally move to ES6 semantics.
                *forHeadKind = PNK_FORIN;
                performAssignment = false;
                if (!report(ParseWarning, pc->sc->strict(), initializer,
                            JSMSG_INVALID_FOR_IN_DECL_WITH_INIT))
                {
                    return false;
                }

                *forInOrOfExpression = expressionAfterForInOrOf(PNK_FORIN, yieldHandling);
                if (!*forInOrOfExpression)
                    return null();
            } else {
                *forHeadKind = PNK_FORHEAD;
            }
        }

        if (*forHeadKind == PNK_FORHEAD) {
            // Per Parser::forHeadStart, the semicolon in |for (;| is
            // ultimately gotten as Operand.  But initializer expressions
            // terminate with the absence of an operator gotten as None, so we
            // need an exception.
            tokenStream.addModifierException(TokenStream::OperandIsNone);
        }
    }

    if (performAssignment) {
        if (!bindBeforeInitializer && !data->bind(name, this))
            return false;

        if (!handler.finishInitializerAssignment(binding, initializer))
            return false;
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::declarationName(Node decl, TokenKind tt, BindData<ParseHandler>* data,
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
    Node binding = newBindingNode(name, handler.declarationIsVar(decl), HoistVars);
    if (!binding)
        return null();
    MOZ_ASSERT(data->isConst() == handler.declarationIsConst(decl));
    if (data->isConst())
        handler.setFlag(binding, PND_CONST);
    data->setNameNode(binding);

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
        if (!initializerInNameDeclaration(decl, binding, name, data, initialDeclaration,
                                          yieldHandling, forHeadKind, forInOrOfExpression))
        {
            return null();
        }
    } else {
        tokenStream.addModifierException(TokenStream::NoneIsOperand);

        bool constRequiringInitializer = handler.declarationIsConst(decl);
        if (initialDeclaration && forHeadKind) {
            bool isForIn, isForOf;
            if (!matchInOrOf(&isForIn, &isForOf))
                return null();

            if (isForIn) {
                // XXX Uncomment this when fixing bug 449811.  Until then,
                //     |for (const ... in/of ...)| remains an error.
                //constRequiringInitializer = false;

                *forHeadKind = PNK_FORIN;
            } else if (isForOf) {
                data->isForOf = true;
                *forHeadKind = PNK_FOROF;
            } else {
                *forHeadKind = PNK_FORHEAD;
            }
        }

        if (constRequiringInitializer) {
            report(ParseError, false, binding, JSMSG_BAD_CONST_DECL);
            return null();
        }

        bool bindBeforeInitializer = handler.declarationIsVar(decl);
        if (bindBeforeInitializer) {
            if (!data->bind(name, this))
                return null();
        }

        if (forHeadKind && *forHeadKind != PNK_FORHEAD) {
            *forInOrOfExpression = expressionAfterForInOrOf(*forHeadKind, yieldHandling);
            if (!*forInOrOfExpression)
                return null();
        }

        if (!bindBeforeInitializer) {
            if (!data->bind(name, this))
                return null();
        }
    }

    handler.setLexicalDeclarationOp(binding, data->op());
    return binding;
}

/*
 * The 'blockScope' parameter is non-null when parsing the 'vars' in a let
 * expression, block statement, non-top-level let declaration in statement
 * context, and the let-initializer of a for-statement.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::declarationList(YieldHandling yieldHandling,
                                      ParseNodeKind kind,
                                      StaticBlockScope* blockScope /* = nullptr */,
                                      ParseNodeKind* forHeadKind /* = nullptr */,
                                      Node* forInOrOfExpression /* = nullptr */)
{
    MOZ_ASSERT(kind == PNK_VAR || kind == PNK_LET || kind == PNK_CONST);
    MOZ_ASSERT_IF(blockScope != nullptr, kind == PNK_LET || kind == PNK_CONST);

    JSOp op;
    switch (kind) {
      case PNK_VAR:   op = JSOP_DEFVAR;   break;
      case PNK_CONST: op = JSOP_DEFCONST; break;
      case PNK_LET:   op = JSOP_DEFLET;   break;
      default: MOZ_CRASH("unknown variable kind");
    }

    Node decl = handler.newDeclarationList(kind, op);
    if (!decl)
        return null();

    BindData<ParseHandler> data(context);
    if (kind == PNK_VAR)
        data.initVar(op);
    else
        data.initLexical(HoistVars, op, blockScope, JSMSG_TOO_MANY_LOCALS);

    bool matched;
    bool initialDeclaration = true;
    do {
        MOZ_ASSERT_IF(!initialDeclaration && forHeadKind,
                      *forHeadKind == PNK_FORHEAD);

        TokenKind tt;
        if (!tokenStream.getToken(&tt))
            return null();

        Node binding = (tt == TOK_LB || tt == TOK_LC)
                       ? declarationPattern(decl, tt, &data, initialDeclaration, yieldHandling,
                                            forHeadKind, forInOrOfExpression)
                       : declarationName(decl, tt, &data, initialDeclaration, yieldHandling,
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
bool
Parser<FullParseHandler>::checkAndPrepareLexical(PrepareLexicalKind prepareWhat,
                                                 const TokenPos& errorPos)
{
    /*
     * This is a lexical declaration. We must be directly under a block for
     * 'let' and 'const' declarations. If we pass this error test, make the
     * enclosing StmtInfoPC be our scope. Further let declarations in this
     * block will find this scope statement and use the same block object.
     *
     * Function declarations behave like 'let', except that they are allowed
     * per ES6 Annex B.3.2 to be labeled, unlike plain 'let' and 'const'
     * declarations.
     *
     * If we are the first let declaration in this block (i.e., when the
     * enclosing maybe-scope StmtInfoPC isn't yet a scope statement) then
     * we also need to set pc->blockNode to be our PNK_LEXICALSCOPE.
     */

    // ES6 Annex B.3.2 does not apply in strict mode, and labeled functions in
    // strict mode should have been rejected by checkFunctionDefinition.
    MOZ_ASSERT_IF(pc->innermostStmt() &&
                  pc->innermostStmt()->type == StmtType::LABEL &&
                  prepareWhat == PrepareFunction,
                  !pc->sc->strict());

    StmtInfoPC* stmt = prepareWhat == PrepareFunction
                       ? pc->innermostNonLabelStmt()
                       : pc->innermostStmt();
    if (stmt && (!stmt->maybeScope() || stmt->isForLetBlock)) {
        reportWithOffset(ParseError, false, errorPos.begin,
                         stmt->type == StmtType::LABEL
                         ? JSMSG_LEXICAL_DECL_LABEL
                         : JSMSG_LEXICAL_DECL_NOT_IN_BLOCK,
                         prepareWhat == PrepareConst ? "const" : "lexical");
        return false;
    }

    if (!stmt) {
        MOZ_ASSERT_IF(prepareWhat != PrepareFunction, pc->atBodyLevel());

        /*
         * Self-hosted code must be usable against *any* global object,
         * including ones with other let variables -- variables possibly
         * placed in conflicting slots.  Forbid top-level let declarations to
         * prevent such conflicts from ever occurring.
         */
        bool isGlobal = !pc->sc->isFunctionBox() && stmt == pc->innermostScopeStmt();
        if (options().selfHostingMode && isGlobal) {
            report(ParseError, false, null(), JSMSG_SELFHOSTED_TOP_LEVEL_LEXICAL,
                   prepareWhat == PrepareConst ? "'const'" : "'let'");
            return false;
        }
        return true;
    }

    if (stmt->isBlockScope) {
        // Nothing to do, the top statement already has a block scope.
        MOZ_ASSERT(pc->innermostScopeStmt() == stmt);
    } else {
        /* Convert the block statement into a scope statement. */
        StaticBlockScope* blockScope = StaticBlockScope::create(context);
        if (!blockScope)
            return false;
        blockScope->initEnclosingScopeFromParser(pc->innermostStaticScope());

        ObjectBox* blockbox = newObjectBox(blockScope);
        if (!blockbox)
            return false;

        /*
         * Some obvious assertions here, but they may help clarify the
         * situation. This stmt is not yet a scope, so it must not be a
         * catch block (catch is a lexical scope by definition).
         */
        MOZ_ASSERT(stmt->canBeBlockScope() && stmt->type != StmtType::CATCH);
        if (prepareWhat == PrepareFunction) {
            stmt->isBlockScope = true;
            pc->stmtStack.linkAsInnermostScopeStmt(stmt, *blockScope);
        } else {
            pc->stmtStack.makeInnermostLexicalScope(*blockScope);
        }
        MOZ_ASSERT(!blockScopes[stmt->blockid]);
        blockScopes[stmt->blockid].set(blockScope);

#ifdef DEBUG
        ParseNode* tmp = pc->blockNode;
        MOZ_ASSERT(!tmp || !tmp->isKind(PNK_LEXICALSCOPE));
#endif

        /* Create a new lexical scope node for these statements. */
        ParseNode* pn1 = handler.new_<LexicalScopeNode>(blockbox, pc->blockNode);
        if (!pn1)
            return false;;
        pc->blockNode = pn1;
    }
    return true;
}

static StaticBlockScope*
CurrentLexicalStaticBlock(ParseContext<FullParseHandler>* pc)
{
    if (pc->innermostStaticScope()->is<StaticBlockScope>())
        return &pc->innermostStaticScope()->as<StaticBlockScope>();
    MOZ_ASSERT(pc->atBodyLevel() &&
               (!pc->sc->isGlobalContext() ||
                HasNonSyntacticStaticScopeChain(pc->innermostStaticScope())));
    return nullptr;
}

template <>
void
Parser<SyntaxParseHandler>::assertCurrentLexicalStaticBlockIs(ParseContext<SyntaxParseHandler>* pc,
                                                              Handle<StaticBlockScope*> blockScope)
{
}

template <>
void
Parser<FullParseHandler>::assertCurrentLexicalStaticBlockIs(ParseContext<FullParseHandler>* pc,
                                                            Handle<StaticBlockScope*> blockScope)
{
    MOZ_ASSERT(CurrentLexicalStaticBlock(pc) == blockScope);
}

template <>
bool
Parser<FullParseHandler>::prepareAndBindInitializedLexicalWithNode(HandlePropertyName name,
                                                                   PrepareLexicalKind prepareWhat,
                                                                   ParseNode* pn,
                                                                   const TokenPos& pos)
{
    BindData<FullParseHandler> data(context);
    if (!checkAndPrepareLexical(prepareWhat, pos))
        return false;
    data.initLexical(HoistVars, prepareWhat == PrepareConst ? JSOP_DEFCONST : JSOP_DEFLET,
                     CurrentLexicalStaticBlock(pc), JSMSG_TOO_MANY_LOCALS);
    return bindInitialized(&data, name, pn);
}

template <>
ParseNode*
Parser<FullParseHandler>::makeInitializedLexicalBinding(HandlePropertyName name,
                                                        PrepareLexicalKind prepareWhat,
                                                        const TokenPos& pos)
{
    ParseNode* dn = newBindingNode(name, false);
    if (!dn)
        return null();
    handler.setPosition(dn, pos);

    if (!prepareAndBindInitializedLexicalWithNode(name, prepareWhat, dn, pos))
        return null();

    return dn;
}

template <>
bool
Parser<FullParseHandler>::bindLexicalFunctionName(HandlePropertyName funName,
                                                  ParseNode* pn)
{
    MOZ_ASSERT(!pc->atBodyLevel());
    pn->pn_blockid = pc->blockid();
    return prepareAndBindInitializedLexicalWithNode(funName, PrepareFunction, pn, pos());
}

template <>
ParseNode*
Parser<FullParseHandler>::lexicalDeclaration(YieldHandling yieldHandling, bool isConst)
{
    handler.disableSyntaxParser();

    if (!checkAndPrepareLexical(isConst ? PrepareConst : PrepareLet, pos()))
        return null();

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
    ParseNode* decl =
        declarationList(yieldHandling, isConst ? PNK_CONST : PNK_LET,
                        CurrentLexicalStaticBlock(pc));
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

template<>
ParseNode*
Parser<FullParseHandler>::newBoundImportForCurrentName()
{
    Node importNode = newName(tokenStream.currentName());
    if (!importNode)
        return null();

    importNode->pn_dflags |= PND_CONST | PND_IMPORT;
    BindData<FullParseHandler> data(context);
    data.initLexical(HoistVars, JSOP_DEFLET, nullptr, JSMSG_TOO_MANY_LOCALS);
    handler.setPosition(importNode, pos());
    if (!bindUninitialized(&data, importNode))
        return null();

    return importNode;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::newBoundImportForCurrentName()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template<>
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

            Node bindingName = newBoundImportForCurrentName();
            if (!bindingName)
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
            return null();

        Node bindingName = newName(tokenStream.currentName());
        if (!bindingName)
            return null();

        // Namespace imports are are not indirect bindings but lexical
        // definitions that hold a module namespace object. They are treated as
        // const variables which are initialized during the
        // ModuleDeclarationInstantiation step. Thus we don't set the PND_IMPORT
        // flag on the definition.
        bindingName->pn_dflags |= PND_CONST | PND_CLOSED;
        BindData<FullParseHandler> data(context);
        data.initLexical(HoistVars, JSOP_DEFLET, nullptr, JSMSG_TOO_MANY_LOCALS);
        handler.setPosition(bindingName, pos());
        if (!bindUninitialized(&data, bindingName))
            return null();

        Node importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingName);
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

            Node bindingName = newBoundImportForCurrentName();
            if (!bindingName)
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
    if (!node || !pc->sc->asModuleBox()->builder.processImport(node))
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
    if (!pc->sc->asModuleBox()->builder.hasExportedName(exportName))
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
            if (!node || !pc->sc->asModuleBox()->builder.processExportFrom(node))
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
        if (!node || !pc->sc->asModuleBox()->builder.processExportFrom(node))
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

        ParseNode* binding = nullptr;
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
          default:
            tokenStream.ungetToken();
            RootedPropertyName name(context, context->names().starDefaultStar);
            binding = makeInitializedLexicalBinding(name, PrepareConst, pos());
            if (!binding)
                return null();
            kid = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
            if (!kid)
                return null();
            if (!MatchOrInsertSemicolonAfterExpression(tokenStream))
                return null();
            break;
        }

        ParseNode* node = handler.newExportDefaultDeclaration(kid, binding, TokenPos(begin, pos().end));
        if (!node || !pc->sc->asModuleBox()->builder.processExport(node))
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
    if (!node || !pc->sc->asModuleBox()->builder.processExport(node))
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

    AutoPushStmtInfoPC stmtInfo(*this, StmtType::IF);

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
    AutoPushStmtInfoPC stmtInfo(*this, StmtType::DO_LOOP);
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
    AutoPushStmtInfoPC stmtInfo(*this, StmtType::WHILE_LOOP);
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
        return checkDestructuringPattern(nullptr, target);

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
        handler.markAsAssigned(target);
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
                                   Maybe<AutoPushStmtInfoPC>& letStmt,
                                   MutableHandle<StaticBlockScope*> blockScope,
                                   Node* forLetImpliedBlock,
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
        *forInitialPart = declarationList(yieldHandling, PNK_VAR, nullptr, forHeadKind,
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
        handler.disableSyntaxParser();

        // Set up the block chain for the lexical declaration.
        blockScope.set(StaticBlockScope::create(context));
        if (!blockScope)
            return false;
        blockScope->initEnclosingScopeFromParser(pc->innermostStaticScope());
        letStmt.emplace(*this, StmtType::BLOCK);
        *forLetImpliedBlock = pushLetScope(blockScope, *letStmt);
        if (!*forLetImpliedBlock)
            return false;
        (*letStmt)->isForLetBlock = true;

        assertCurrentLexicalStaticBlockIs(pc, blockScope);
        *forInitialPart = declarationList(yieldHandling, tt == TOK_CONST ? PNK_CONST : PNK_LET,
                                          blockScope, forHeadKind, forInOrOfExpression);
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
Parser<ParseHandler>::cloneForInOrOfDeclarationForAssignment(Node decl)
{
    return cloneLeftHandSide(handler.singleBindingFromDeclaration(decl));
}

template <class ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::forStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));
    uint32_t begin = pos().begin;

    AutoPushStmtInfoPC forStmt(*this, StmtType::FOR_LOOP);

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
                if (!report(ParseWarning, pc->sc->strict(), null(), JSMSG_DEPRECATED_FOR_EACH))
                    return null();
            }
        }
    }

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    // PNK_FORHEAD, PNK_FORIN, or PNK_FOROF depending on the loop type.
    ParseNodeKind headKind;

    // |x| in either |for (x; ...; ...)| or |for (x in/of ...)|.
    Node startNode;

    // The next three variables are used to implement `for (let/const ...)`.
    //
    // We generate an implicit block, wrapping the whole loop, to store loop
    // variables declared this way. Note that if the loop uses `for (var...)`
    // instead, those variables go on some existing enclosing scope, so no
    // implicit block scope is created.
    //
    // All three variables remain null/none if the loop is any other form.
    //
    // blockScope is the static block scope for the implicit block scope.
    RootedStaticBlockScope blockScope(context);

    // The PNK_LEXICALSCOPE node containing blockScope's ObjectBox.
    Node forLetImpliedBlock = null();

    // letStmt is the BLOCK StmtInfo for the implicit block.
    //
    // Caution: `letStmt.emplace()` creates some Rooted objects. Rooteds must
    // be created/destroyed in FIFO order. Therefore adding a Rooted in this
    // function, between this point and the .emplace() call below, would trip
    // assertions.
    Maybe<AutoPushStmtInfoPC> letStmt;

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
    if (!forHeadStart(yieldHandling, &headKind, &startNode, letStmt, &blockScope,
                      &forLetImpliedBlock, &iteratedExpr))
    {
        return null();
    }

    MOZ_ASSERT(headKind == PNK_FORIN || headKind == PNK_FOROF || headKind == PNK_FORHEAD);

    Node pn1;
    Node pn2;
    Node pn3;
    TokenStream::Modifier modifier;
    if (headKind == PNK_FOROF || headKind == PNK_FORIN) {
        // |target| is the LeftHandSideExpression or declaration to which the
        // per-iteration value (an arbitrary value exposed by the iteration
        // protocol, or a string naming a property) is assigned.
        Node target = startNode;

        /*
         * Parse the rest of the for/in or for/of head.
         *
         * Here pn1 is everything to the left of 'in' or 'of'. At the end of
         * this block, pn1 is a decl or nullptr, pn2 is the assignment target
         * that receives the enumeration value each iteration, and pn3 is the
         * rhs of 'in'.
         */
        if (headKind == PNK_FOROF) {
            forStmt->type = StmtType::FOR_OF_LOOP;
            if (isForEach) {
                report(ParseError, false, startNode, JSMSG_BAD_FOR_EACH_LOOP);
                return null();
            }
        } else {
            forStmt->type = StmtType::FOR_IN_LOOP;
            iflags |= JSITER_ENUMERATE;
        }

        /*
         * After the following if-else, pn2 will point to the name or
         * destructuring pattern on in's left. pn1 will point to the decl, if
         * any, else nullptr. Note that the "declaration with initializer" case
         * rewrites the loop-head, moving the decl and setting pn1 to nullptr.
         */
        if (handler.isDeclarationList(target)) {
            pn1 = target;

            // Make a copy of the declaration that can be passed to
            // BytecodeEmitter::emitAssignment.
            pn2 = cloneForInOrOfDeclarationForAssignment(target);
            if (!pn2)
                return null();
        } else {
            MOZ_ASSERT(!letStmt);
            pn1 = null();
            pn2 = target;

            if (!checkAndMarkAsAssignmentLhs(pn2, PlainAssignment))
                return null();
        }

        pn3 = iteratedExpr;

        if (handler.isNameAnyParentheses(pn2)) {
            // Beware 'for (arguments in ...)' with or without a 'var'.
            handler.markAsAssigned(pn2);
        }

        // Parser::declaration consumed everything up to the closing ')'.  That
        // token follows an {Assignment,}Expression, so the next token must be
        // consumed as if an operator continued the expression, i.e. as None.
        modifier = TokenStream::None;
    } else {
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

        modifier = mod;
        pn1 = init;
        pn2 = test;
        pn3 = update;
    }

    MUST_MATCH_TOKEN_MOD(TOK_RP, modifier, JSMSG_PAREN_AFTER_FOR_CTRL);

    TokenPos headPos(begin, pos().end);
    Node forHead = handler.newForHead(headKind, pn1, pn2, pn3, headPos);
    if (!forHead)
        return null();

    Node body = statement(yieldHandling);
    if (!body)
        return null();

    Node forLoop = handler.newForStatement(begin, forHead, body, iflags);
    if (!forLoop)
        return null();

    if (forLetImpliedBlock) {
        handler.initForLetBlock(forLetImpliedBlock, forLoop);
        return forLetImpliedBlock;
    }
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

    AutoPushStmtInfoPC stmtInfo(*this, StmtType::SWITCH);
    if (!stmtInfo.generateBlockId())
        return null();

    Node caseList = handler.newStatementList(pc->blockid(), pos());
    if (!caseList)
        return null();

    Node saveBlock = pc->blockNode;
    pc->blockNode = caseList;

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

        Node body = handler.newStatementList(pc->blockid(), pos());
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
            handler.addStatementToList(body, stmt, pc);
        }

        // In ES6, lexical bindings cannot be accessed until initialized. If
        // there was a 'let' declaration in the case we just parsed, remember
        // the slot starting at which new lexical bindings will be
        // assigned. Since lexical bindings from previous cases will not
        // dominate uses in the current case, any such uses will require a
        // dead zone check.
        //
        // Currently this is overly conservative; we could do better, but
        // declaring lexical bindings within switch cases without introducing
        // a new block is poor form and should be avoided.
        if (stmtInfo->isBlockScope)
            stmtInfo->firstDominatingLexicalInCase = stmtInfo->staticBlock().numVariables();

        Node casepn = handler.newCaseOrDefault(caseBegin, caseExpr, body);
        if (!casepn)
            return null();
        handler.addCaseStatementToList(caseList, casepn, pc);
    }

    /*
     * Handle the case where there was a let declaration in any case in
     * the switch body, but not within an inner block.  If it replaced
     * pc->blockNode with a new block node then we must refresh caseList and
     * then restore pc->blockNode.
     */
    if (pc->blockNode != caseList)
        caseList = pc->blockNode;
    pc->blockNode = saveBlock;

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

    StmtInfoPC* stmt = pc->innermostStmt();
    if (label) {
        for (StmtInfoPC* stmt2 = nullptr; ; stmt = stmt->enclosing) {
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_LABEL_NOT_FOUND);
                return null();
            }
            if (stmt->type == StmtType::LABEL) {
                if (stmt->label == label) {
                    if (!stmt2 || !stmt2->isLoop()) {
                        report(ParseError, false, null(), JSMSG_BAD_CONTINUE);
                        return null();
                    }
                    break;
                }
            } else {
                stmt2 = stmt;
            }
        }
    } else {
        for (; ; stmt = stmt->enclosing) {
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_BAD_CONTINUE);
                return null();
            }
            if (stmt->isLoop())
                break;
        }
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
    StmtInfoPC* stmt = pc->innermostStmt();
    if (label) {
        for (; ; stmt = stmt->enclosing) {
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_LABEL_NOT_FOUND);
                return null();
            }
            if (stmt->type == StmtType::LABEL && stmt->label == label)
                break;
        }
    } else {
        for (; ; stmt = stmt->enclosing) {
            if (!stmt) {
                report(ParseError, false, null(), JSMSG_TOUGH_BREAK);
                return null();
            }
            if (stmt->isLoop() || stmt->type == StmtType::SWITCH)
                break;
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

    MOZ_ASSERT(pc->sc->isFunctionBox());
    pc->sc->asFunctionBox()->usesReturn = true;

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
    Node generator = newName(context->names().dotGenerator);
    if (!generator)
        return null();
    if (!noteNameUse(context->names().dotGenerator, generator))
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
        MOZ_ASSERT(pc->sc->isFunctionBox());

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
        MOZ_ASSERT(pc->lastYieldOffset == ParseContext<ParseHandler>::NoYieldOffset);

        if (!abortIfSyntaxParser())
            return null();

        if (!pc->sc->isFunctionBox()) {
            report(ParseError, false, null(), JSMSG_BAD_RETURN_OR_YIELD, js_yield_str);
            return null();
        }

        if (pc->sc->asFunctionBox()->isArrow()) {
            reportWithOffset(ParseError, false, begin,
                             JSMSG_YIELD_IN_ARROW, js_yield_str);
            return null();
        }

        if (pc->sc->asFunctionBox()->function()->isMethod() ||
            pc->sc->asFunctionBox()->function()->isGetter() ||
            pc->sc->asFunctionBox()->function()->isSetter())
        {
            reportWithOffset(ParseError, false, begin,
                             JSMSG_YIELD_IN_METHOD, js_yield_str);
            return null();
        }


        if (pc->funHasReturnExpr
#if JS_HAS_EXPR_CLOSURES
            || pc->sc->asFunctionBox()->function()->isExprBody()
#endif
            )
        {
            /* As in Python (see PEP-255), disallow return v; in generators. */
            reportBadReturn(null(), ParseError, JSMSG_BAD_GENERATOR_RETURN,
                            JSMSG_BAD_ANON_GENERATOR_RETURN);
            return null();
        }

        pc->sc->asFunctionBox()->setGeneratorKind(LegacyGenerator);
        addTelemetry(JSCompartment::DeprecatedLegacyGenerator);

        MOZ_FALLTHROUGH;

      case LegacyGenerator:
      {
        // We are in a legacy generator: a function that has already seen a
        // yield.
        MOZ_ASSERT(pc->sc->isFunctionBox());

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
    if (pc->sc->strict() && !report(ParseStrictError, true, null(), JSMSG_STRICT_CODE_WITH))
        return null();

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_WITH);
    Node objectExpr = exprInParens(InAllowed, yieldHandling, TripledotProhibited);
    if (!objectExpr)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_WITH);

    Rooted<StaticWithScope*> staticWith(context, StaticWithScope::create(context));
    if (!staticWith)
        return null();

    Node innerBlock;
    {
        AutoPushStmtInfoPC stmtInfo(*this, StmtType::WITH, *staticWith);
        innerBlock = statement(yieldHandling);
        if (!innerBlock)
            return null();
    }

    pc->sc->setBindingsAccessedDynamically();

    /*
     * Make sure to deoptimize lexical dependencies inside the |with|
     * to safely optimize binding globals (see bug 561923).
     */
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        DefinitionNode defn = r.front().value().get<FullParseHandler>();
        DefinitionNode lexdep = handler.resolve(defn);
        if (!pc->sc->isDotVariable(lexdep->name()))
            handler.deoptimizeUsesWithin(lexdep, TokenPos(begin, pos().begin));
    }

    ObjectBox* staticWithBox = newObjectBox(staticWith);
    if (!staticWithBox)
        return null();
    return handler.newWithStatement(begin, objectExpr, innerBlock, staticWithBox);
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
Parser<ParseHandler>::labeledStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    RootedPropertyName label(context, tokenStream.currentName());
    for (StmtInfoPC* stmt = pc->innermostStmt(); stmt; stmt = stmt->enclosing) {
        if (stmt->type == StmtType::LABEL && stmt->label == label) {
            report(ParseError, false, null(), JSMSG_DUPLICATE_LABEL);
            return null();
        }
    }

    tokenStream.consumeKnownToken(TOK_COLON);

    /* Push a label struct and parse the statement. */
    AutoPushStmtInfoPC stmtInfo(*this, StmtType::LABEL);
    stmtInfo->label = label;
    Node pn = statement(yieldHandling);
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
        AutoPushStmtInfoPC stmtInfo(*this, StmtType::TRY);
        if (!stmtInfo.generateBlockId())
            return null();
        innerBlock = statements(yieldHandling);
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
            BindData<ParseHandler> data(context);

            /* Check for another catch after unconditional catch. */
            if (hasUnconditionalCatch) {
                report(ParseError, false, null(), JSMSG_CATCH_AFTER_GENERAL);
                return null();
            }

            /*
             * Create a lexical scope node around the whole catch clause,
             * including the head.
             */
            AutoPushStmtInfoPC stmtInfo(*this, StmtType::CATCH);
            pnblock = pushLexicalScope(stmtInfo);
            if (!pnblock)
                return null();

            /*
             * Legal catch forms are:
             *   catch (lhs)
             *   catch (lhs if <boolean_expression>)
             * where lhs is a name or a destructuring left-hand side.
             * (the latter is legal only #ifdef JS_HAS_CATCH_GUARD)
             */
            MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_CATCH);

            /*
             * Contrary to ECMA Ed. 3, the catch variable is lexically
             * scoped, not a property of a new Object instance.  This is
             * an intentional change that anticipates ECMA Ed. 4.
             */
            data.initLexical(HoistVars, JSOP_DEFLET,
                             &stmtInfo->staticScope->template as<StaticBlockScope>(),
                             JSMSG_TOO_MANY_CATCH_VARS);
            MOZ_ASSERT(data.letData().blockScope);

            if (!tokenStream.getToken(&tt))
                return null();
            Node catchName;
            switch (tt) {
              case TOK_LB:
              case TOK_LC:
                catchName = destructuringExpr(yieldHandling, &data, tt);
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
              {
                RootedPropertyName label(context, tokenStream.currentName());
                catchName = newBindingNode(label, false);
                if (!catchName)
                    return null();
                data.setNameNode(catchName);
                if (!data.bind(label, this))
                    return null();
                break;
              }

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
            Node catchBody = blockStatement(yieldHandling, JSMSG_CURLY_AFTER_CATCH);
            if (!catchBody)
                return null();

            if (!catchGuard)
                hasUnconditionalCatch = true;

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
        AutoPushStmtInfoPC stmtInfo(*this, StmtType::TRY);
        if (!stmtInfo.generateBlockId())
            return null();
        finallyBlock = statements(yieldHandling);
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
Parser<ParseHandler>::debuggerStatement()
{
    TokenPos p;
    p.begin = pos().begin;
    if (!MatchOrInsertSemicolonAfterNonExpression(tokenStream))
        return null();
    p.end = pos().end;

    pc->sc->setBindingsAccessedDynamically();
    pc->sc->setHasDebuggerStatement();

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

    ParseNode* classBlock = null();

    RootedAtom propAtom(context);

    // A named class creates a new lexical scope with a const binding of the
    // class name.
    Maybe<AutoPushStmtInfoPC> classStmt;
    if (name) {
        classStmt.emplace(*this, StmtType::BLOCK);
        classBlock = pushLexicalScope(*classStmt);
        if (!classBlock)
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
        ParseNode* innerBinding = makeInitializedLexicalBinding(name, PrepareConst, namePos);
        if (!innerBinding)
            return null();

        MOZ_ASSERT(classBlock);
        handler.setLexicalScopeBody(classBlock, classMethods);
        methodsOrBlock = classBlock;
        classStmt.reset();

        ParseNode* outerBinding = null();
        if (classContext == ClassStatement) {
            outerBinding = makeInitializedLexicalBinding(name, PrepareLet, namePos);
            if (!outerBinding)
                return null();
        }

        nameNode = handler.newClassNames(outerBinding, innerBinding, namePos);
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
    MOZ_ASSERT(!pc->sc->strict());

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
      case TOK_VAR: {
        Node pn = declarationList(yieldHandling, PNK_VAR);
        if (!pn)
            return null();
        if (!MatchOrInsertSemicolonAfterExpression(tokenStream))
            return null();
        return pn;
      }

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
        if (!pc->sc->isFunctionBox()) {
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

        bool isDestructuring = checkDestructuringPattern(nullptr, target);
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
        handler.markAsAssigned(target);
        return true;
    }

    MOZ_ASSERT(handler.isFunctionCall(target));
    return makeSetCall(target, JSMSG_BAD_LEFTSIDE_OF_ASS);
}

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

        Node arrowFunc = functionDef(inHandling, yieldHandling, nullptr, Arrow, NotGenerator);
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

    bool saved = pc->inDeclDestructuring;
    pc->inDeclDestructuring = false;
    Node rhs = assignExpr(inHandling, yieldHandling, TripledotProhibited,
                          possibleError);
    pc->inDeclDestructuring = saved;
    if (!rhs)
        return null();
    return handler.newAssignment(kind, lhs, rhs, pc, op);
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
        if (!pc->sc->strict())
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

    if (!report(ParseStrictError, pc->sc->strict(), nameNode, JSMSG_BAD_STRICT_ASSIGN, chars))
        return false;

    MOZ_ASSERT(!pc->sc->strict(),
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

    report(ParseError, pc->sc->strict(), target, errnum, extra);
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

        handler.markAsAssigned(target);
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
        Node pn2 = memberExpr(yieldHandling, TripledotProhibited, nullptr /* possibleError */, tt2, true);
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
            if (!report(ParseStrictError, pc->sc->strict(), expr, JSMSG_DEPRECATED_DELETE_OPERAND))
                return null();
            pc->sc->setBindingsAccessedDynamically();
        }

        return handler.newDelete(begin, expr);
      }

      default: {
        Node pn = memberExpr(yieldHandling, tripledotHandling, possibleError, tt, /* allowCallSyntax = */ true,
                             invoked);
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

    ParseContext<ParseHandler>* outerpc = pc;

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
    Directives directives(/* strict = */ outerpc->sc->strict());
    FunctionBox* genFunbox = newFunctionBox(genfn, fun, outerpc, directives, StarGenerator);
    if (!genFunbox)
        return null();

    genFunbox->inGenexpLambda = true;

    ParseContext<ParseHandler> genpc(this, outerpc, genfn, genFunbox,
                                     /* newDirectives = */ nullptr);
    if (!genpc.init(*this))
        return null();

    /*
     * We assume conservatively that any deoptimization flags in pc->sc
     * come from the kid. So we propagate these flags into genfn. For code
     * simplicity we also do not detect if the flags were only set in the
     * kid and could be removed from pc->sc.
     */
    genFunbox->anyCxFlags = outerpc->sc->anyCxFlags;
    if (outerpc->sc->isFunctionBox())
        genFunbox->funCxFlags = outerpc->sc->asFunctionBox()->funCxFlags;

    handler.setBlockId(genfn, genpc.bodyid);

    Node generator = newName(context->names().dotGenerator);
    if (!generator)
        return null();
    if (!pc->define(tokenStream, context->names().dotGenerator, generator, Definition::VAR))
        return null();

    Node body = handler.newStatementList(pc->blockid(), TokenPos(begin, pos().end));
    if (!body)
        return null();

    Node comp = comprehension(StarGenerator);
    if (!comp)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);

    handler.setBeginPosition(comp, begin);
    handler.setEndPosition(comp, pos().end);
    handler.addStatementToList(body, comp, pc);
    handler.setEndPosition(body, pos().end);
    handler.setBeginPosition(genfn, begin);
    handler.setEndPosition(genfn, pos().end);

    generator = newName(context->names().dotGenerator);
    if (!generator)
        return null();
    if (!noteNameUse(context->names().dotGenerator, generator))
        return null();
    if (!handler.prependInitialYield(body, generator))
        return null();

    // Note that if we ever start syntax-parsing generators, we will also
    // need to propagate the closed-over variable set to the inner
    // lazyscript, as in finishFunctionDefinition.
    handler.setFunctionBody(genfn, body);

    PropagateTransitiveParseFlags(genFunbox, outerpc->sc);

    if (!leaveFunction(genfn, outerpc))
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
    Node assignLhs = newName(name);
    if (!assignLhs)
        return null();
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

    AutoPushStmtInfoPC stmtInfo(*this, StmtType::BLOCK);
    BindData<ParseHandler> data(context);

    RootedStaticBlockScope blockScope(context, StaticBlockScope::create(context));
    if (!blockScope)
        return null();

    // Initialize the enclosing scope manually for the call to |bind|
    // below, which is before the call to |pushLetScope|.
    blockScope->initEnclosingScopeFromParser(pc->innermostStaticScope());

    data.initLexical(DontHoistVars, JSOP_DEFLET, blockScope, JSMSG_TOO_MANY_LOCALS);
    Node decls = handler.newComprehensionBinding(lhs);
    if (!decls)
        return null();
    data.setNameNode(lhs);
    if (!data.bind(name, this))
        return null();
    Node letScope = pushLetScope(blockScope, stmtInfo);
    if (!letScope)
        return null();
    handler.setLexicalScopeBody(letScope, decls);

    if (!noteNameUse(name, assignLhs))
        return null();
    handler.setOp(assignLhs, JSOP_SETNAME);

    Node head = handler.newForHead(PNK_FOROF, letScope, assignLhs, rhs, headPos);
    if (!head)
        return null();

    Node tail = comprehensionTail(comprehensionKind);
    if (!tail)
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


/* * */

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
    if (!pc->sc->allowSuperProperty())
        return false;
    pc->sc->markSuperScopeNeedsHomeObject();
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
                if (!pc->sc->allowSuperCall()) {
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
                // generator, we still inherity the yieldHandling of the
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
                    /* Select JSOP_EVAL and flag pc as needsCallObject. */
                    op = pc->sc->strict() ? JSOP_STRICTEVAL : JSOP_EVAL;
                    pc->sc->setBindingsAccessedDynamically();
                    pc->sc->setHasDirectEval();

                    /*
                     * In non-strict mode code, direct calls to eval can add
                     * variables to the call object.
                     */
                    if (pc->sc->isFunctionBox() && !pc->sc->strict())
                        pc->sc->asFunctionBox()->setHasExtensibleScope();

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
                    if (pc->sc->isFunctionBox())
                        pc->sc->asFunctionBox()->usesApply = true;
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
    return handler.newName(name, pc->blockid(), pos(), context);
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

    if (!pc->inDeclDestructuring && !noteNameUse(name, pn))
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

    // Turn off the inDeclDestructuring flag when parsing computed property
    // names. In short, when parsing 'let {[x + y]: z} = obj;', noteNameUse()
    // should be called on x and y, but not on z. See the comment on
    // Parser<>::checkDestructuringPattern() for details.
    bool saved = pc->inDeclDestructuring;
    pc->inDeclDestructuring = false;
    Node assignNode = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
    pc->inDeclDestructuring = saved;
    if (!assignNode)
        return null();

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
            bool saved = pc->inDeclDestructuring;
            // Setting `inDeclDestructuring` to false allows name use to be noted
            // in `identifierName` See Bug: 1255167.
            pc->inDeclDestructuring = false;
            Node rhs = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
            pc->inDeclDestructuring = saved;
            if (!rhs)
                return null();

            Node propExpr = handler.newAssignment(PNK_ASSIGN, lhs, rhs, pc, JSOP_NOP);
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
    return functionDef(InAllowed, yieldHandling, funName, kind, generatorKind);
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

    if (!pc->sc->allowNewTarget()) {
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
        if (pc->sc->isFunctionBox())
            pc->sc->asFunctionBox()->usesThis = true;
        Node thisName = null();
        if (pc->sc->thisBinding() == ThisBinding::Function) {
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

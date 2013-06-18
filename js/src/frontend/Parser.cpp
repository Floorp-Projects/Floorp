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

#include "frontend/Parser.h"

#include "jstypes.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscript.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/FoldConstants.h"
#include "frontend/ParseMaps.h"
#include "frontend/TokenStream.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseMaps-inl.h"
#include "frontend/ParseNode-inl.h"
#include "frontend/Parser-inl.h"
#include "frontend/SharedContext-inl.h"

#include "vm/NumericConversions.h"
#include "vm/RegExpStatics-inl.h"

using namespace js;
using namespace js::gc;
using mozilla::Maybe;

namespace js {
namespace frontend {

typedef Rooted<StaticBlockObject*> RootedStaticBlockObject;
typedef Handle<StaticBlockObject*> HandleStaticBlockObject;

typedef MutableHandle<PropertyName*> MutableHandlePropertyName;

/*
 * Insist that the next token be of type tt, or report errno and return null.
 * NB: this macro uses cx and ts from its lexical environment.
 */
#define MUST_MATCH_TOKEN_WITH_FLAGS(tt, errno, __flags)                                     \
    JS_BEGIN_MACRO                                                                          \
        if (tokenStream.getToken((__flags)) != tt) {                                        \
            report(ParseError, false, null(), errno);                                       \
            return null();                                                                  \
        }                                                                                   \
    JS_END_MACRO
#define MUST_MATCH_TOKEN(tt, errno) MUST_MATCH_TOKEN_WITH_FLAGS(tt, errno, 0)

template <typename ParseHandler>
bool
GenerateBlockId(ParseContext<ParseHandler> *pc, uint32_t &blockid)
{
    if (pc->blockidGen == JS_BIT(20)) {
        JS_ReportErrorNumber(pc->sc->context, js_GetErrorMessage, NULL, JSMSG_NEED_DIET, "program");
        return false;
    }
    JS_ASSERT(pc->blockidGen < JS_BIT(20));
    blockid = pc->blockidGen++;
    return true;
}

template bool
GenerateBlockId(ParseContext<SyntaxParseHandler> *pc, uint32_t &blockid);

template bool
GenerateBlockId(ParseContext<FullParseHandler> *pc, uint32_t &blockid);

template <typename ParseHandler>
static void
PushStatementPC(ParseContext<ParseHandler> *pc, StmtInfoPC *stmt, StmtType type)
{
    stmt->blockid = pc->blockid();
    PushStatement(pc, stmt, type);
}

// See comment on member function declaration.
template <>
bool
ParseContext<FullParseHandler>::define(JSContext *cx, HandlePropertyName name,
                                       ParseNode *pn, Definition::Kind kind)
{
    JS_ASSERT(!pn->isUsed());
    JS_ASSERT_IF(pn->isDefn(), pn->isPlaceholder());

    Definition *prevDef = NULL;
    if (kind == Definition::LET)
        prevDef = decls_.lookupFirst(name);
    else
        JS_ASSERT(!decls_.lookupFirst(name));

    if (!prevDef)
        prevDef = lexdeps.lookupDefn<FullParseHandler>(name);

    if (prevDef) {
        ParseNode **pnup = &prevDef->dn_uses;
        ParseNode *pnu;
        unsigned start = (kind == Definition::LET) ? pn->pn_blockid : bodyid;

        while ((pnu = *pnup) != NULL && pnu->pn_blockid >= start) {
            JS_ASSERT(pnu->pn_blockid >= bodyid);
            JS_ASSERT(pnu->isUsed());
            pnu->pn_lexdef = (Definition *) pn;
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

    JS_ASSERT_IF(kind != Definition::LET, !lexdeps->lookup(name));
    pn->setDefn(true);
    pn->pn_dflags &= ~PND_PLACEHOLDER;
    if (kind == Definition::CONST)
        pn->pn_dflags |= PND_CONST;

    Definition *dn = (Definition *)pn;
    switch (kind) {
      case Definition::ARG:
        JS_ASSERT(sc->isFunctionBox());
        dn->setOp(JSOP_GETARG);
        dn->pn_dflags |= PND_BOUND;
        if (!dn->pn_cookie.set(cx, staticLevel, args_.length()))
            return false;
        if (!args_.append(dn))
            return false;
        if (name == cx->names().empty)
            break;
        if (!decls_.addUnique(name, dn))
            return false;
        break;

      case Definition::CONST:
      case Definition::VAR:
        if (sc->isFunctionBox()) {
            dn->setOp(JSOP_GETLOCAL);
            dn->pn_dflags |= PND_BOUND;
            if (!dn->pn_cookie.set(cx, staticLevel, vars_.length()))
                return false;
            if (!vars_.append(dn))
                return false;
        }
        if (!decls_.addUnique(name, dn))
            return false;
        break;

      case Definition::LET:
        dn->setOp(JSOP_GETLOCAL);
        dn->pn_dflags |= (PND_LET | PND_BOUND);
        JS_ASSERT(dn->pn_cookie.level() == staticLevel); /* see bindLet */
        if (!decls_.addShadow(name, dn))
            return false;
        break;

      default:
        JS_NOT_REACHED("unexpected kind");
        break;
    }

    return true;
}

template <>
bool
ParseContext<SyntaxParseHandler>::define(JSContext *cx, HandlePropertyName name, Node pn,
                                         Definition::Kind kind)
{
    JS_ASSERT(!decls_.lookupFirst(name));

    if (lexdeps.lookupDefn<SyntaxParseHandler>(name))
        lexdeps->remove(name);

    // Keep track of the number of arguments in args_, for fun->nargs.
    if (kind == Definition::ARG && !args_.append((Definition *) NULL))
        return false;

    return decls_.addUnique(name, kind);
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::prepareToAddDuplicateArg(HandlePropertyName name, DefinitionNode prevDecl)
{
    JS_ASSERT(decls_.lookupFirst(name) == prevDecl);
    decls_.remove(name);
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::updateDecl(JSAtom *atom, Node pn)
{
    Definition *oldDecl = decls_.lookupFirst(atom);

    pn->setDefn(true);
    Definition *newDecl = (Definition *)pn;
    decls_.updateFirst(atom, newDecl);

    if (!sc->isFunctionBox()) {
        JS_ASSERT(newDecl->isFreeVar());
        return;
    }

    JS_ASSERT(oldDecl->isBound());
    JS_ASSERT(!oldDecl->pn_cookie.isFree());
    newDecl->pn_cookie = oldDecl->pn_cookie;
    newDecl->pn_dflags |= PND_BOUND;
    if (JOF_OPTYPE(oldDecl->getOp()) == JOF_QARG) {
        newDecl->setOp(JSOP_GETARG);
        JS_ASSERT(args_[oldDecl->pn_cookie.slot()] == oldDecl);
        args_[oldDecl->pn_cookie.slot()] = newDecl;
    } else {
        JS_ASSERT(JOF_OPTYPE(oldDecl->getOp()) == JOF_LOCAL);
        newDecl->setOp(JSOP_GETLOCAL);
        JS_ASSERT(vars_[oldDecl->pn_cookie.slot()] == oldDecl);
        vars_[oldDecl->pn_cookie.slot()] = newDecl;
    }
}

template <typename ParseHandler>
void
ParseContext<ParseHandler>::popLetDecl(JSAtom *atom)
{
    JS_ASSERT(ParseHandler::getDefinitionKind(decls_.lookupFirst(atom)) == Definition::LET);
    decls_.remove(atom);
}

template <typename ParseHandler>
static void
AppendPackedBindings(const ParseContext<ParseHandler> *pc, const DeclVector &vec, Binding *dst)
{
    for (unsigned i = 0; i < vec.length(); ++i, ++dst) {
        Definition *dn = vec[i];
        PropertyName *name = dn->name();

        BindingKind kind;
        switch (dn->kind()) {
          case Definition::VAR:
            kind = VARIABLE;
            break;
          case Definition::CONST:
            kind = CONSTANT;
            break;
          case Definition::ARG:
            kind = ARGUMENT;
            break;
          default:
            JS_NOT_REACHED("unexpected dn->kind");
        }

        /*
         * Bindings::init does not check for duplicates so we must ensure that
         * only one binding with a given name is marked aliased. pc->decls
         * maintains the canonical definition for each name, so use that.
         */
        JS_ASSERT_IF(dn->isClosed(), pc->decls().lookupFirst(name) == dn);
        bool aliased = dn->isClosed() ||
                       (pc->sc->bindingsAccessedDynamically() &&
                        pc->decls().lookupFirst(name) == dn);

        *dst = Binding(name, kind, aliased);
    }
}

template <typename ParseHandler>
bool
ParseContext<ParseHandler>::generateFunctionBindings(JSContext *cx, InternalHandle<Bindings*> bindings) const
{
    JS_ASSERT(sc->isFunctionBox());

    unsigned count = args_.length() + vars_.length();
    Binding *packedBindings = cx->tempLifoAlloc().newArrayUninitialized<Binding>(count);
    if (!packedBindings) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    AppendPackedBindings(this, args_, packedBindings);
    AppendPackedBindings(this, vars_, packedBindings + args_.length());

    if (!Bindings::initWithTemporaryStorage(cx, bindings, args_.length(), vars_.length(),
                                            packedBindings))
    {
        return false;
    }

    FunctionBox *funbox = sc->asFunctionBox();
    if (bindings->hasAnyAliasedBindings() || funbox->hasExtensibleScope())
        funbox->function()->setIsHeavyweight();

    return true;
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
    uint32_t offset = (pn ? handler.getPosition(pn) : tokenStream.currentToken().pos).begin;

    va_list args;
    va_start(args, errorNumber);
    bool result = reportHelper(kind, strict, offset, errorNumber, args);
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
Parser<ParseHandler>::Parser(JSContext *cx, const CompileOptions &options,
                             const jschar *chars, size_t length, bool foldConstants,
                             Parser<SyntaxParseHandler> *syntaxParser,
                             LazyScript *lazyOuterFunction)
  : AutoGCRooter(cx, PARSER),
    context(cx),
    tokenStream(cx, options, chars, length, thisForCtor(), keepAtoms),
    traceListHead(NULL),
    pc(NULL),
    sct(NULL),
    keepAtoms(cx->runtime()),
    foldConstants(foldConstants),
    compileAndGo(options.compileAndGo),
    selfHostingMode(options.selfHostingMode),
    abortedSyntaxParse(false),
    handler(cx, tokenStream, foldConstants, syntaxParser, lazyOuterFunction)
{
    cx->runtime()->activeCompilations++;

    // The Mozilla specific JSOPTION_EXTRA_WARNINGS option adds extra warnings
    // which are not generated if functions are parsed lazily. Note that the
    // standard "use strict" does not inhibit lazy parsing.
    if (context->hasExtraWarningsOption())
        handler.disableSyntaxParser();

    tempPoolMark = cx->tempLifoAlloc().mark();
}

template <typename ParseHandler>
Parser<ParseHandler>::~Parser()
{
    JSContext *cx = context;
    cx->tempLifoAlloc().release(tempPoolMark);
    cx->runtime()->activeCompilations--;

    /*
     * The parser can allocate enormous amounts of memory for large functions.
     * Eagerly free the memory now (which otherwise won't be freed until the
     * next GC) to avoid unnecessary OOMs.
     */
    cx->tempLifoAlloc().freeAllIfHugeAndUnused();
}

template <typename ParseHandler>
ObjectBox *
Parser<ParseHandler>::newObjectBox(JSObject *obj)
{
    JS_ASSERT(obj && !IsPoisonedPtr(obj));

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */

    ObjectBox *objbox = context->tempLifoAlloc().new_<ObjectBox>(obj, traceListHead);
    if (!objbox) {
        js_ReportOutOfMemory(context);
        return NULL;
    }

    traceListHead = objbox;

    return objbox;
}

template <typename ParseHandler>
FunctionBox::FunctionBox(JSContext *cx, ObjectBox* traceListHead, JSFunction *fun,
                         ParseContext<ParseHandler> *outerpc, bool strict)
  : ObjectBox(fun, traceListHead),
    SharedContext(cx, strict),
    bindings(),
    bufStart(0),
    bufEnd(0),
    asmStart(0),
    ndefaults(0),
    inWith(false),                  // initialized below
    inGenexpLambda(false),
    useAsm(false),
    insideUseAsm(outerpc && outerpc->useAsmOrInsideUseAsm()),
    usesArguments(false),
    usesApply(false),
    funCxFlags()
{
    JS_ASSERT(fun->isTenured());

    if (!outerpc) {
        inWith = false;

    } else if (outerpc->parsingWith) {
        // This covers cases that don't involve eval().  For example:
        //
        //   with (o) { (function() { g(); })(); }
        //
        // In this case, |outerpc| corresponds to global code, and
        // outerpc->parsingWith is true.
        inWith = true;

    } else if (outerpc->sc->isGlobalSharedContext()) {
        // This covers the case where a function is nested within an eval()
        // within a |with| statement.
        //
        //   with (o) { eval("(function() { g(); })();"); }
        //
        // In this case, |outerpc| corresponds to the eval(),
        // outerpc->parsingWith is false because the eval() breaks the
        // ParseContext chain, and |parent| is NULL (again because of the
        // eval(), so we have to look at |outerpc|'s scopeChain.
        //
        JSObject *scope = outerpc->sc->asGlobalSharedContext()->scopeChain();
        while (scope) {
            if (scope->isWith())
                inWith = true;
            scope = scope->enclosingScope();
        }
    } else if (outerpc->sc->isFunctionBox()) {
        // This is like the above case, but for more deeply nested functions.
        // For example:
        //
        //   with (o) { eval("(function() { (function() { g(); })(); })();"); } }
        //
        // In this case, the inner anonymous function needs to inherit the
        // setting of |inWith| from the outer one.
        FunctionBox *parent = outerpc->sc->asFunctionBox();
        if (parent && parent->inWith)
            inWith = true;
    }
}

template <typename ParseHandler>
FunctionBox *
Parser<ParseHandler>::newFunctionBox(JSFunction *fun,
                                     ParseContext<ParseHandler> *outerpc, bool strict)
{
    JS_ASSERT(fun && !IsPoisonedPtr(fun));

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    FunctionBox *funbox =
        context->tempLifoAlloc().new_<FunctionBox>(context, traceListHead, fun, outerpc, strict);
    if (!funbox) {
        js_ReportOutOfMemory(context);
        return NULL;
    }

    traceListHead = funbox;

    return funbox;
}

ModuleBox::ModuleBox(JSContext *cx, ObjectBox *traceListHead, Module *module,
                     ParseContext<FullParseHandler> *pc)
    : ObjectBox(module, traceListHead),
      SharedContext(cx, true)
{
}

template <>
ModuleBox *
Parser<FullParseHandler>::newModuleBox(Module *module, ParseContext<FullParseHandler> *outerpc)
{
    JS_ASSERT(module && !IsPoisonedPtr(module));

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    ModuleBox *modulebox =
        context->tempLifoAlloc().new_<ModuleBox>(context, traceListHead, module, outerpc);
    if (!modulebox) {
        js_ReportOutOfMemory(context);
        return NULL;
    }

    traceListHead = modulebox;

    return modulebox;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::trace(JSTracer *trc)
{
    traceListHead->trace(trc);
}

void
MarkParser(JSTracer *trc, AutoGCRooter *parser)
{
    static_cast<Parser<FullParseHandler> *>(parser)->trace(trc);
}

/*
 * Parse a top-level JS script.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::parse(JSObject *chain)
{
    /*
     * Protect atoms from being collected by a GC activation, which might
     * - nest on this thread due to out of memory (the so-called "last ditch"
     *   GC attempted within js_NewGCThing), or
     * - run for any reason on another thread if this thread is suspended on
     *   an object lock before it finishes generating bytecode into a script
     *   protected from the GC by a root or a stack frame reference.
     */
    GlobalSharedContext globalsc(context, chain, StrictModeFromContext(context));
    ParseContext<ParseHandler> globalpc(this, NULL, &globalsc, /* staticLevel = */ 0, /* bodyid = */ 0);
    if (!globalpc.init())
        return null();

    Node pn = statements();
    if (pn) {
        if (!tokenStream.matchToken(TOK_EOF)) {
            report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
            return null();
        }
        if (foldConstants) {
            if (!FoldConstants(context, &pn, this))
                return null();
        }
    }
    return pn;
}

/*
 * Insist on a final return before control flows out of pn.  Try to be a bit
 * smart about loops: do {...; return e2;} while(0) at the end of a function
 * that contains an early return e1 will get a strict warning.  Similarly for
 * iloops: while (true){...} is treated as though ... returns.
 */
enum {
    ENDS_IN_OTHER = 0,
    ENDS_IN_RETURN = 1,
    ENDS_IN_BREAK = 2
};

static int
HasFinalReturn(ParseNode *pn)
{
    ParseNode *pn2, *pn3;
    unsigned rv, rv2, hasDefault;

    switch (pn->getKind()) {
      case PNK_STATEMENTLIST:
        if (!pn->pn_head)
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->last());

      case PNK_IF:
        if (!pn->pn_kid3)
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->pn_kid2) & HasFinalReturn(pn->pn_kid3);

      case PNK_WHILE:
        pn2 = pn->pn_left;
        if (pn2->isKind(PNK_TRUE))
            return ENDS_IN_RETURN;
        if (pn2->isKind(PNK_NUMBER) && pn2->pn_dval)
            return ENDS_IN_RETURN;
        return ENDS_IN_OTHER;

      case PNK_DOWHILE:
        pn2 = pn->pn_right;
        if (pn2->isKind(PNK_FALSE))
            return HasFinalReturn(pn->pn_left);
        if (pn2->isKind(PNK_TRUE))
            return ENDS_IN_RETURN;
        if (pn2->isKind(PNK_NUMBER)) {
            if (pn2->pn_dval == 0)
                return HasFinalReturn(pn->pn_left);
            return ENDS_IN_RETURN;
        }
        return ENDS_IN_OTHER;

      case PNK_FOR:
        pn2 = pn->pn_left;
        if (pn2->isArity(PN_TERNARY) && !pn2->pn_kid2)
            return ENDS_IN_RETURN;
        return ENDS_IN_OTHER;

      case PNK_SWITCH:
        rv = ENDS_IN_RETURN;
        hasDefault = ENDS_IN_OTHER;
        pn2 = pn->pn_right;
        if (pn2->isKind(PNK_LEXICALSCOPE))
            pn2 = pn2->expr();
        for (pn2 = pn2->pn_head; rv && pn2; pn2 = pn2->pn_next) {
            if (pn2->isKind(PNK_DEFAULT))
                hasDefault = ENDS_IN_RETURN;
            pn3 = pn2->pn_right;
            JS_ASSERT(pn3->isKind(PNK_STATEMENTLIST));
            if (pn3->pn_head) {
                rv2 = HasFinalReturn(pn3->last());
                if (rv2 == ENDS_IN_OTHER && pn2->pn_next)
                    /* Falling through to next case or default. */;
                else
                    rv &= rv2;
            }
        }
        /* If a final switch has no default case, we judge it harshly. */
        rv &= hasDefault;
        return rv;

      case PNK_BREAK:
        return ENDS_IN_BREAK;

      case PNK_WITH:
        return HasFinalReturn(pn->pn_right);

      case PNK_RETURN:
        return ENDS_IN_RETURN;

      case PNK_COLON:
      case PNK_LEXICALSCOPE:
        return HasFinalReturn(pn->expr());

      case PNK_THROW:
        return ENDS_IN_RETURN;

      case PNK_TRY:
        /* If we have a finally block that returns, we are done. */
        if (pn->pn_kid3) {
            rv = HasFinalReturn(pn->pn_kid3);
            if (rv == ENDS_IN_RETURN)
                return rv;
        }

        /* Else check the try block and any and all catch statements. */
        rv = HasFinalReturn(pn->pn_kid1);
        if (pn->pn_kid2) {
            JS_ASSERT(pn->pn_kid2->isArity(PN_LIST));
            for (pn2 = pn->pn_kid2->pn_head; pn2; pn2 = pn2->pn_next)
                rv &= HasFinalReturn(pn2);
        }
        return rv;

      case PNK_CATCH:
        /* Check this catch block's body. */
        return HasFinalReturn(pn->pn_kid3);

      case PNK_LET:
        /* Non-binary let statements are let declarations. */
        if (!pn->isArity(PN_BINARY))
            return ENDS_IN_OTHER;
        return HasFinalReturn(pn->pn_right);

      default:
        return ENDS_IN_OTHER;
    }
}

static int
HasFinalReturn(SyntaxParseHandler::Node pn)
{
    return ENDS_IN_RETURN;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportBadReturn(Node pn, ParseReportKind kind,
                                      unsigned errnum, unsigned anonerrnum)
{
    JSAutoByteString name;
    JSAtom *atom = pc->sc->asFunctionBox()->function()->atom();
    if (atom) {
        if (!js_AtomToPrintableString(context, atom, &name))
            return false;
    } else {
        errnum = anonerrnum;
    }
    return report(kind, pc->sc->strict, pn, errnum, name.ptr());
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkFinalReturn(Node pn)
{
    JS_ASSERT(pc->sc->isFunctionBox());
    return HasFinalReturn(pn) == ENDS_IN_RETURN ||
           reportBadReturn(pn, ParseExtraWarning,
                           JSMSG_NO_RETURN_VALUE, JSMSG_ANON_NO_RETURN_VALUE);
}

/*
 * Check that it is permitted to assign to lhs.  Strict mode code may not
 * assign to 'eval' or 'arguments'.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::checkStrictAssignment(Node lhs)
{
    if (!pc->sc->needStrictChecks())
        return true;

    JSAtom *atom = handler.isName(lhs);
    if (!atom)
        return true;

    if (atom == context->names().eval || atom == context->names().arguments) {
        JSAutoByteString name;
        if (!js_AtomToPrintableString(context, atom, &name) ||
            !report(ParseStrictError, pc->sc->strict, lhs,
                    JSMSG_DEPRECATED_ASSIGN, name.ptr()))
        {
            return false;
        }
    }
    return true;
}

/*
 * Check that it is permitted to introduce a binding for atom.  Strict mode
 * forbids introducing new definitions for 'eval', 'arguments', or for any
 * strict mode reserved keyword.  Use pn for reporting error locations, or use
 * pc's token stream if pn is NULL.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::checkStrictBinding(HandlePropertyName name, Node pn)
{
    if (!pc->sc->needStrictChecks())
        return true;

    if (name == context->names().eval || name == context->names().arguments || IsKeyword(name)) {
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(context, name, &bytes))
            return false;
        return report(ParseStrictError, pc->sc->strict, pn,
                      JSMSG_BAD_BINDING, bytes.ptr());
    }

    return true;
}

template <>
ParseNode *
Parser<FullParseHandler>::standaloneFunctionBody(HandleFunction fun, const AutoNameVector &formals,
                                                 HandleScript script, Node fn, FunctionBox **funbox,
                                                 bool strict, bool *becameStrict)
{
    if (becameStrict)
        *becameStrict = false;

    *funbox = newFunctionBox(fun, /* outerpc = */ NULL, strict);
    if (!funbox)
        return null();
    handler.setFunctionBox(fn, *funbox);

    ParseContext<FullParseHandler> funpc(this, pc, *funbox, /* staticLevel = */ 0, /* bodyid = */ 0);
    if (!funpc.init())
        return null();

    for (unsigned i = 0; i < formals.length(); i++) {
        if (!defineArg(fn, formals[i]))
            return null();
    }

    ParseNode *pn = functionBody(Statement, StatementListBody);
    if (!pn) {
        if (becameStrict && pc->funBecameStrict)
            *becameStrict = true;
        return null();
    }

    if (!tokenStream.matchToken(TOK_EOF)) {
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();
    }

    if (!FoldConstants(context, &pn, this))
        return null();

    InternalHandle<Bindings*> bindings(script, &script->bindings);
    if (!funpc.generateFunctionBindings(context, bindings))
        return null();

    return pn;
}

template <>
bool
Parser<FullParseHandler>::checkFunctionArguments()
{
    /*
     * Non-top-level functions use JSOP_DEFFUN which is a dynamic scope
     * operation which means it aliases any bindings with the same name.
     */
    if (FuncStmtSet *set = pc->funcStmts) {
        for (FuncStmtSet::Range r = set->all(); !r.empty(); r.popFront()) {
            PropertyName *name = r.front()->asPropertyName();
            if (Definition *dn = pc->decls().lookupFirst(name))
                dn->pn_dflags |= PND_CLOSED;
        }
    }

    /* Time to implement the odd semantics of 'arguments'. */
    HandlePropertyName arguments = context->names().arguments;

    /*
     * As explained by the ContextFlags::funArgumentsHasLocalBinding comment,
     * create a declaration for 'arguments' if there are any unbound uses in
     * the function body.
     */
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        if (r.front().key() == arguments) {
            Definition *dn = r.front().value().get<FullParseHandler>();
            pc->lexdeps->remove(arguments);
            dn->pn_dflags |= PND_IMPLICITARGUMENTS;
            if (!pc->define(context, arguments, dn, Definition::VAR))
                return false;
            pc->sc->asFunctionBox()->usesArguments = true;
            break;
        }
    }

    /*
     * Report error if both rest parameters and 'arguments' are used. Do this
     * check before adding artificial 'arguments' below.
     */
    Definition *maybeArgDef = pc->decls().lookupFirst(arguments);
    bool argumentsHasBinding = !!maybeArgDef;
    bool argumentsHasLocalBinding = maybeArgDef && maybeArgDef->kind() != Definition::ARG;
    bool hasRest = pc->sc->asFunctionBox()->function()->hasRest();
    if (hasRest && argumentsHasLocalBinding) {
        report(ParseError, false, NULL, JSMSG_ARGUMENTS_AND_REST);
        return false;
    }

    /*
     * Even if 'arguments' isn't explicitly mentioned, dynamic name lookup
     * forces an 'arguments' binding. The exception is that functions with rest
     * parameters are free from 'arguments'.
     */
    if (!argumentsHasBinding && pc->sc->bindingsAccessedDynamically() && !hasRest) {
        ParseNode *pn = NameNode::create(PNK_NAME, arguments, &handler, pc);
        if (!pn)
            return false;
        if (!pc->define(context, arguments, pn, Definition::VAR))
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
        FunctionBox *funbox = pc->sc->asFunctionBox();
        funbox->setArgumentsHasLocalBinding();

        /*
         * If a script has both explicit mentions of 'arguments' and dynamic
         * name lookups which could access the arguments, an arguments object
         * must be created eagerly. The SSA analysis used for lazy arguments
         * cannot cope with dynamic name accesses, so any 'arguments' accessed
         * via a NAME opcode must force construction of the arguments object.
         */
        if (pc->sc->bindingsAccessedDynamically() && maybeArgDef)
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
         * function. In strict mode parameters do not alias arguments[i], and
         * to make the arguments object reflect initial parameter values prior
         * to any mutation we create it eagerly whenever parameters are (or
         * might, in the case of calls to eval) be assigned.
         */
        if (pc->sc->needStrictChecks()) {
            for (AtomDefnListMap::Range r = pc->decls().all(); !r.empty(); r.popFront()) {
                DefinitionList &dlist = r.front().value();
                for (DefinitionList::Range dr = dlist.all(); !dr.empty(); dr.popFront()) {
                    Definition *dn = dr.front<FullParseHandler>();
                    if (dn->kind() == Definition::ARG && dn->isAssigned())
                        funbox->setDefinitelyNeedsArgsObj();
                }
            }
            /* Watch for mutation of arguments through e.g. eval(). */
            if (pc->sc->bindingsAccessedDynamically())
                funbox->setDefinitelyNeedsArgsObj();
        }
    }

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkFunctionArguments()
{
    bool hasRest = pc->sc->asFunctionBox()->function()->hasRest();

    if (pc->lexdeps->lookup(context->names().arguments)) {
        pc->sc->asFunctionBox()->usesArguments = true;
        if (hasRest) {
            report(ParseError, false, null(), JSMSG_ARGUMENTS_AND_REST);
            return false;
        }
    } else if (hasRest) {
        DefinitionNode maybeArgDef = pc->decls().lookupFirst(context->names().arguments);
        if (maybeArgDef && handler.getDefinitionKind(maybeArgDef) != Definition::ARG) {
            report(ParseError, false, null(), JSMSG_ARGUMENTS_AND_REST);
            return false;
        }
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionBody(FunctionSyntaxKind kind, FunctionBodyType type)
{
    JS_ASSERT(pc->sc->isFunctionBox());
    JS_ASSERT(!pc->funHasReturnExpr && !pc->funHasReturnVoid);

    Node pn;
    if (type == StatementListBody) {
        pn = statements();
        if (!pn)
            return null();
    } else {
        JS_ASSERT(type == ExpressionBody);
        JS_ASSERT(JS_HAS_EXPR_CLOSURES);

        Node kid = assignExpr();
        if (!kid)
            return null();

        pn = handler.newUnary(PNK_RETURN, kid, JSOP_RETURN);
        if (!pn)
            return null();

        if (pc->sc->asFunctionBox()->isGenerator()) {
            reportBadReturn(pn, ParseError,
                            JSMSG_BAD_GENERATOR_RETURN,
                            JSMSG_BAD_ANON_GENERATOR_RETURN);
            return null();
        }
    }

    /* Check for falling off the end of a function that returns a value. */
    if (context->hasExtraWarningsOption() && pc->funHasReturnExpr && !checkFinalReturn(pn))
        return null();

    if (kind != Arrow) {
        /* Define the 'arguments' binding if necessary. Arrow functions don't have 'arguments'. */
        if (!checkFunctionArguments())
            return null();
    }

    return pn;
}

/* See comment for use in Parser::functionDef. */
template <>
bool
Parser<FullParseHandler>::makeDefIntoUse(Definition *dn, ParseNode *pn, JSAtom *atom)
{
    /* Turn pn into a definition. */
    pc->updateDecl(atom, pn);

    /* Change all uses of dn to be uses of pn. */
    for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
        JS_ASSERT(pnu->isUsed());
        JS_ASSERT(!pnu->isDefn());
        pnu->pn_lexdef = (Definition *) pn;
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
        JS_ASSERT(dn->functionIsHoisted());
        pn->dn_uses = dn->pn_link;
        handler.prepareNodeForMutation(dn);
        dn->setKind(PNK_NOP);
        dn->setArity(PN_NULLARY);
        return true;
    }

    /*
     * If dn is arg, or in [var, const, let] and has an initializer, then we
     * must rewrite it to be an assignment node, whose freshly allocated
     * left-hand side becomes a use of pn.
     */
    if (dn->canHaveInitializer()) {
        if (ParseNode *rhs = dn->expr()) {
            ParseNode *lhs = handler.makeAssignment(dn, rhs);
            if (!lhs)
                return false;
            pn->dn_uses = lhs;
            dn->pn_link = NULL;
            dn = (Definition *) lhs;
        }
    }

    /* Turn dn into a use of pn. */
    JS_ASSERT(dn->isKind(PNK_NAME));
    JS_ASSERT(dn->isArity(PN_NAME));
    JS_ASSERT(dn->pn_atom == atom);
    dn->setOp((js_CodeSpec[dn->getOp()].format & JOF_SET) ? JSOP_SETNAME : JSOP_NAME);
    dn->setDefn(false);
    dn->setUsed(true);
    dn->pn_lexdef = (Definition *) pn;
    dn->pn_cookie.makeFree();
    dn->pn_dflags &= ~PND_BOUND;
    return true;
}

/*
 * Parameter block types for the several Binder functions.  We use a common
 * helper function signature in order to share code among destructuring and
 * simple variable declaration parsers.  In the destructuring case, the binder
 * function is called indirectly from the variable declaration parser by way
 * of CheckDestructuring and its friends.
 */

template <typename ParseHandler>
struct BindData
{
    BindData(JSContext *cx) : let(cx) {}

    typedef bool
    (*Binder)(JSContext *cx, BindData *data, HandlePropertyName name, Parser<ParseHandler> *parser);

    /* name node for definition processing and error source coordinates */
    typename ParseHandler::Node pn;

    JSOp            op;         /* prolog bytecode or nop */
    Binder          binder;     /* binder, discriminates u */

    struct LetData {
        LetData(JSContext *cx) : blockObj(cx) {}
        VarContext varContext;
        RootedStaticBlockObject blockObj;
        unsigned   overflow;
    } let;

    void initLet(VarContext varContext, StaticBlockObject &blockObj, unsigned overflow) {
        this->pn = ParseHandler::null();
        this->op = JSOP_NOP;
        this->binder = Parser<ParseHandler>::bindLet;
        this->let.varContext = varContext;
        this->let.blockObj = &blockObj;
        this->let.overflow = overflow;
    }

    void initVarOrConst(JSOp op) {
        this->op = op;
        this->binder = Parser<ParseHandler>::bindVarOrConst;
    }
};

template <typename ParseHandler>
JSFunction *
Parser<ParseHandler>::newFunction(GenericParseContext *pc, HandleAtom atom,
                                  FunctionSyntaxKind kind)
{
    JS_ASSERT_IF(kind == Statement, atom != NULL);

    /*
     * Find the global compilation context in order to pre-set the newborn
     * function's parent slot to pc->sc->asGlobal()->scopeChain. If the global
     * context is a compile-and-go one, we leave the pre-set parent intact;
     * otherwise we clear parent and proto.
     */
    while (pc->parent)
        pc = pc->parent;

    RootedObject parent(context);
    parent = pc->sc->isFunctionBox() ? NULL : pc->sc->asGlobalSharedContext()->scopeChain();

    RootedFunction fun(context);
    JSFunction::Flags flags = (kind == Expression)
                              ? JSFunction::INTERPRETED_LAMBDA
                              : (kind == Arrow)
                                ? JSFunction::INTERPRETED_LAMBDA_ARROW
                                : JSFunction::INTERPRETED;
    fun = NewFunction(context, NullPtr(), NULL, 0, flags, parent, atom,
                      JSFunction::FinalizeKind, MaybeSingletonObject);
    if (selfHostingMode)
        fun->setIsSelfHostedBuiltin();
    if (fun && !compileAndGo) {
        if (!JSObject::clearParent(context, fun))
            return NULL;
        if (!JSObject::clearType(context, fun))
            return NULL;
        fun->setEnvironment(NULL);
    }
    return fun;
}

static bool
MatchOrInsertSemicolon(JSContext *cx, TokenStream *ts)
{
    TokenKind tt = ts->peekTokenSameLine(TSF_OPERAND);
    if (tt == TOK_ERROR)
        return false;
    if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
        /* Advance the scanner for proper error location reporting. */
        ts->getToken(TSF_OPERAND);
        ts->reportError(JSMSG_SEMI_BEFORE_STMNT);
        return false;
    }
    (void) ts->matchToken(TOK_SEMI);
    return true;
}

template <typename ParseHandler>
typename ParseHandler::DefinitionNode
Parser<ParseHandler>::getOrCreateLexicalDependency(ParseContext<ParseHandler> *pc, JSAtom *atom)
{
    AtomDefnAddPtr p = pc->lexdeps->lookupForAdd(atom);
    if (p)
        return p.value().get<ParseHandler>();

    DefinitionNode dn = handler.newPlaceholder(atom, pc);
    if (!dn)
        return ParseHandler::nullDefinition();
    DefinitionSingle def = DefinitionSingle::new_<ParseHandler>(dn);
    if (!pc->lexdeps->add(p, atom, def))
        return ParseHandler::nullDefinition();
    return dn;
}

static bool
ConvertDefinitionToNamedLambdaUse(JSContext *cx, ParseContext<FullParseHandler> *pc,
                                  FunctionBox *funbox, Definition *dn)
{
    dn->setOp(JSOP_CALLEE);
    if (!dn->pn_cookie.set(cx, pc->staticLevel, UpvarCookie::CALLEE_SLOT))
        return false;
    dn->pn_dflags |= PND_BOUND;
    JS_ASSERT(dn->kind() == Definition::NAMED_LAMBDA);

    /*
     * Since 'dn' is a placeholder, it has not been defined in the
     * ParseContext and hence we must manually flag a closed-over
     * callee name as needing a dynamic scope (this is done for all
     * definitions in the ParseContext by generateFunctionBindings).
     *
     * If 'dn' has been assigned to, then we also flag the function
     * scope has needing a dynamic scope so that dynamic scope
     * setter can either ignore the set (in non-strict mode) or
     * produce an error (in strict mode).
     */
    if (dn->isClosed() || dn->isAssigned())
        funbox->function()->setIsHeavyweight();
    return true;
}

/*
 * Beware: this function is called for functions nested in other functions or
 * global scripts but not for functions compiled through the Function
 * constructor or JSAPI. To always execute code when a function has finished
 * parsing, use Parser::functionBody.
 */
template <>
bool
Parser<FullParseHandler>::leaveFunction(ParseNode *fn, HandlePropertyName funName,
                                        ParseContext<FullParseHandler> *outerpc,
                                        FunctionSyntaxKind kind)
{
    outerpc->blockidGen = pc->blockidGen;

    FunctionBox *funbox = fn->pn_funbox;
    JS_ASSERT(funbox == pc->sc->asFunctionBox());

    if (!outerpc->topStmt || outerpc->topStmt->type == STMT_BLOCK)
        fn->pn_dflags |= PND_BLOCKCHILD;

    /* Propagate unresolved lexical names up to outerpc->lexdeps. */
    if (pc->lexdeps->count()) {
        for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
            JSAtom *atom = r.front().key();
            Definition *dn = r.front().value().get<FullParseHandler>();
            JS_ASSERT(dn->isPlaceholder());

            if (atom == funName && kind == Expression) {
                if (!ConvertDefinitionToNamedLambdaUse(context, pc, funbox, dn))
                    return false;
                continue;
            }

            Definition *outer_dn = outerpc->decls().lookupFirst(atom);

            /*
             * Make sure to deoptimize lexical dependencies that are polluted
             * by eval and function statements (which both flag the function as
             * having an extensible scope) or any enclosing 'with'.
             */
            if (funbox->hasExtensibleScope() || outerpc->parsingWith)
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
             *
             * The dn == outer_dn case arises with generator expressions
             * (see CompExprTransplanter::transplant, the PN_CODE/PN_NAME
             * case), and nowhere else, currently.
             */
            if (dn != outer_dn) {
                if (ParseNode *pnu = dn->dn_uses) {
                    while (true) {
                        pnu->pn_lexdef = outer_dn;
                        if (!pnu->pn_link)
                            break;
                        pnu = pnu->pn_link;
                    }
                    pnu->pn_link = outer_dn->dn_uses;
                    outer_dn->dn_uses = dn->dn_uses;
                    dn->dn_uses = NULL;
                }

                outer_dn->pn_dflags |= dn->pn_dflags & ~PND_PLACEHOLDER;
            }

            /* Mark the outer dn as escaping. */
            outer_dn->pn_dflags |= PND_CLOSED;
        }
    }

    InternalHandle<Bindings*> bindings =
        InternalHandle<Bindings*>::fromMarkedLocation(&funbox->bindings);
    return pc->generateFunctionBindings(context, bindings);
}

template <>
bool
Parser<SyntaxParseHandler>::leaveFunction(Node fn, HandlePropertyName funName,
                                          ParseContext<SyntaxParseHandler> *outerpc,
                                          FunctionSyntaxKind kind)
{
    outerpc->blockidGen = pc->blockidGen;

    FunctionBox *funbox = pc->sc->asFunctionBox();
    return addFreeVariablesFromLazyFunction(funbox->function(), outerpc);
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
                                bool disallowDuplicateArgs, Node *duplicatedArg)
{
    SharedContext *sc = pc->sc;

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
            if (!js_AtomToPrintableString(context, name, &bytes))
                return false;
            if (!report(ParseStrictError, pc->sc->strict, pn,
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
        JS_ASSERT(handler.getDefinitionKind(prevDecl) == Definition::ARG);
        pc->prepareToAddDuplicateArg(name, prevDecl);
    }

    Node argpn = handler.newName(name, pc);
    if (!argpn)
        return false;

    if (!checkStrictBinding(name, argpn))
        return false;

    handler.addFunctionArgument(funcpn, argpn);
    return pc->define(context, name, argpn, Definition::ARG);
}

#if JS_HAS_DESTRUCTURING
template <typename ParseHandler>
/* static */ bool
Parser<ParseHandler>::bindDestructuringArg(JSContext *cx, BindData<ParseHandler> *data,
                                           HandlePropertyName name, Parser<ParseHandler> *parser)
{
    ParseContext<ParseHandler> *pc = parser->pc;
    JS_ASSERT(pc->sc->isFunctionBox());

    if (pc->decls().lookupFirst(name)) {
        parser->report(ParseError, false, null(), JSMSG_BAD_DUP_ARGS);
        return false;
    }

    if (!parser->checkStrictBinding(name, data->pn))
        return false;

    return pc->define(cx, name, data->pn, Definition::VAR);
}
#endif /* JS_HAS_DESTRUCTURING */

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionArguments(FunctionSyntaxKind kind, Node *listp, Node funcpn,
                                        bool &hasRest)
{
    FunctionBox *funbox = pc->sc->asFunctionBox();

    bool parenFreeArrow = false;
    if (kind == Arrow && tokenStream.peekToken() == TOK_NAME) {
        parenFreeArrow = true;
    } else {
        if (tokenStream.getToken() != TOK_LP) {
            report(ParseError, false, null(),
                   kind == Arrow ? JSMSG_BAD_ARROW_ARGS : JSMSG_PAREN_BEFORE_FORMAL);
            return false;
        }

        // Record the start of function source (for FunctionToString). If we
        // are parenFreeArrow, we will set this below, after consuming the NAME.
        funbox->setStart(tokenStream);
    }

    hasRest = false;

    Node argsbody = handler.newList(PNK_ARGSBODY);
    if (!argsbody)
        return false;
    handler.setFunctionBody(funcpn, argsbody);

    if (parenFreeArrow || !tokenStream.matchToken(TOK_RP)) {
        bool hasDefaults = false;
        Node duplicatedArg = null();
        bool destructuringArg = false;
#if JS_HAS_DESTRUCTURING
        Node list = null();
#endif

        do {
            if (hasRest) {
                report(ParseError, false, null(), JSMSG_PARAMETER_AFTER_REST);
                return false;
            }

            TokenKind tt = tokenStream.getToken();
            JS_ASSERT_IF(parenFreeArrow, tt == TOK_NAME);
            switch (tt) {
#if JS_HAS_DESTRUCTURING
              case TOK_LB:
              case TOK_LC:
              {
                /* See comment below in the TOK_NAME case. */
                if (duplicatedArg) {
                    report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                if (hasDefaults) {
                    report(ParseError, false, null(), JSMSG_NONDEFAULT_FORMAL_AFTER_DEFAULT);
                    return false;
                }

                destructuringArg = true;

                /*
                 * A destructuring formal parameter turns into one or more
                 * local variables initialized from properties of a single
                 * anonymous positional parameter, so here we must tweak our
                 * binder and its data.
                 */
                BindData<ParseHandler> data(context);
                data.pn = ParseHandler::null();
                data.op = JSOP_DEFVAR;
                data.binder = bindDestructuringArg;
                Node lhs = destructuringExpr(&data, tt);
                if (!lhs)
                    return false;

                /*
                 * Synthesize a destructuring assignment from the single
                 * anonymous positional parameter into the destructuring
                 * left-hand-side expression and accumulate it in list.
                 */
                HandlePropertyName name = context->names().empty;
                Node rhs = handler.newName(name, pc);
                if (!rhs)
                    return false;

                if (!pc->define(context, name, rhs, Definition::ARG))
                    return false;

                Node item = handler.newBinary(PNK_ASSIGN, lhs, rhs);
                if (!item)
                    return false;
                if (list) {
                    handler.addList(list, item);
                } else {
                    list = handler.newList(PNK_VAR, item);
                    if (!list)
                        return false;
                    *listp = list;
                }
                break;
              }
#endif /* JS_HAS_DESTRUCTURING */

              case TOK_TRIPLEDOT:
              {
                hasRest = true;
                tt = tokenStream.getToken();
                if (tt != TOK_NAME) {
                    if (tt != TOK_ERROR)
                        report(ParseError, false, null(), JSMSG_NO_REST_NAME);
                    return false;
                }
                /* Fall through */
              }

              case TOK_NAME:
              {
                if (parenFreeArrow)
                    funbox->setStart(tokenStream);

                RootedPropertyName name(context, tokenStream.currentToken().name());
                bool disallowDuplicateArgs = destructuringArg || hasDefaults;
                if (!defineArg(funcpn, name, disallowDuplicateArgs, &duplicatedArg))
                    return false;

                if (tokenStream.matchToken(TOK_ASSIGN)) {
                    // A default argument without parentheses would look like:
                    // a = expr => body, but both operators are right-associative, so
                    // that would have been parsed as a = (expr => body) instead.
                    // Therefore it's impossible to get here with parenFreeArrow.
                    JS_ASSERT(!parenFreeArrow);

                    if (hasRest) {
                        report(ParseError, false, null(), JSMSG_REST_WITH_DEFAULT);
                        return false;
                    }
                    if (duplicatedArg) {
                        report(ParseError, false, duplicatedArg, JSMSG_BAD_DUP_ARGS);
                        return false;
                    }
                    hasDefaults = true;
                    Node def_expr = assignExprWithoutYield(JSMSG_YIELD_IN_DEFAULT);
                    if (!def_expr)
                        return false;
                    handler.setLastFunctionArgumentDefault(funcpn, def_expr);
                    funbox->ndefaults++;
                } else if (!hasRest && hasDefaults) {
                    report(ParseError, false, null(), JSMSG_NONDEFAULT_FORMAL_AFTER_DEFAULT);
                    return false;
                }

                break;
              }

              default:
                report(ParseError, false, null(), JSMSG_MISSING_FORMAL);
                /* FALL THROUGH */
              case TOK_ERROR:
                return false;
            }
        } while (!parenFreeArrow && tokenStream.matchToken(TOK_COMMA));

        if (!parenFreeArrow && tokenStream.getToken() != TOK_RP) {
            report(ParseError, false, null(), JSMSG_PAREN_AFTER_FORMAL);
            return false;
        }
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::checkFunctionDefinition(HandlePropertyName funName,
                                                  ParseNode **pn_, FunctionSyntaxKind kind,
                                                  bool *pbodyProcessed)
{
    ParseNode *&pn = *pn_;
    *pbodyProcessed = false;

    /* Function statements add a binding to the enclosing scope. */
    bool bodyLevel = pc->atBodyLevel();

    if (kind == Statement) {
        /*
         * Handle redeclaration and optimize cases where we can statically bind the
         * function (thereby avoiding JSOP_DEFFUN and dynamic name lookup).
         */
        if (Definition *dn = pc->decls().lookupFirst(funName)) {
            JS_ASSERT(!dn->isUsed());
            JS_ASSERT(dn->isDefn());

            if (context->hasExtraWarningsOption() || dn->kind() == Definition::CONST) {
                JSAutoByteString name;
                ParseReportKind reporter = (dn->kind() != Definition::CONST)
                                           ? ParseExtraWarning
                                           : ParseError;
                if (!js_AtomToPrintableString(context, funName, &name) ||
                    !report(reporter, false, NULL, JSMSG_REDECLARED_VAR,
                            Definition::kindString(dn->kind()), name.ptr()))
                {
                    return false;
                }
            }

            /*
             * Body-level function statements are effectively variable
             * declarations where the initialization is hoisted to the
             * beginning of the block. This means that any other variable
             * declaration with the same name is really just an assignment to
             * the function's binding (which is mutable), so turn any existing
             * declaration into a use.
             */
            if (bodyLevel && !makeDefIntoUse(dn, pn, funName))
                return false;
        } else if (bodyLevel) {
            /*
             * If this function was used before it was defined, claim the
             * pre-created definition node for this function that primaryExpr
             * put in pc->lexdeps on first forward reference, and recycle pn.
             */
            if (Definition *fn = pc->lexdeps.lookupDefn<FullParseHandler>(funName)) {
                JS_ASSERT(fn->isDefn());
                fn->setKind(PNK_FUNCTION);
                fn->setArity(PN_CODE);
                fn->pn_pos.begin = pn->pn_pos.begin;
                fn->pn_pos.end = pn->pn_pos.end;

                fn->pn_body = NULL;
                fn->pn_cookie.makeFree();

                pc->lexdeps->remove(funName);
                handler.freeTree(pn);
                pn = fn;
            }

            if (!pc->define(context, funName, pn, Definition::VAR))
                return false;
        }

        /*
         * As a SpiderMonkey-specific extension, non-body-level function
         * statements (e.g., functions in an "if" or "while" block) are
         * dynamically bound when control flow reaches the statement. The
         * emitter normally emits functions in two passes (see PNK_ARGSBODY).
         */
        if (bodyLevel) {
            JS_ASSERT(pn->functionIsHoisted());
            JS_ASSERT_IF(pc->sc->isFunctionBox(), !pn->pn_cookie.isFree());
            JS_ASSERT_IF(!pc->sc->isFunctionBox(), pn->pn_cookie.isFree());
        } else {
            JS_ASSERT(!pc->sc->strict);
            JS_ASSERT(pn->pn_cookie.isFree());
            if (pc->sc->isFunctionBox()) {
                FunctionBox *funbox = pc->sc->asFunctionBox();
                funbox->setMightAliasLocals();
                funbox->setHasExtensibleScope();
            }
            pn->setOp(JSOP_DEFFUN);

            /*
             * Instead of setting bindingsAccessedDynamically, which would be
             * overly conservative, remember the names of all function
             * statements and mark any bindings with the same as aliased at the
             * end of functionBody.
             */
            if (!pc->funcStmts) {
                pc->funcStmts = context->new_<FuncStmtSet>(context);
                if (!pc->funcStmts || !pc->funcStmts->init())
                    return false;
            }
            if (!pc->funcStmts->put(funName))
                return false;

            /*
             * Due to the implicit declaration mechanism, 'arguments' will not
             * have decls and, even if it did, they will not be noted as closed
             * in the emitter. Thus, in the corner case of function statements
             * overridding arguments, flag the whole scope as dynamic.
             */
            if (funName == context->names().arguments)
                pc->sc->setBindingsAccessedDynamically();
        }

        /* No further binding (in BindNameToSlot) is needed for functions. */
        pn->pn_dflags |= PND_BOUND;
    } else {
        /* A function expression does not introduce any binding. */
        pn->setOp(JSOP_LAMBDA);
    }

    // When a lazily-parsed function is called, we only fully parse (and emit)
    // that function, not any of its nested children. The initial syntax-only
    // parse recorded the free variables of nested functions and their extents,
    // so we can skip over them after accounting for their free variables.
    if (LazyScript *lazyOuter = handler.lazyOuterFunction()) {
        JSFunction *fun = handler.nextLazyInnerFunction();
        FunctionBox *funbox = newFunctionBox(fun, pc, /* strict = */ false);
        if (!funbox)
            return false;
        handler.setFunctionBox(pn, funbox);

        if (!addFreeVariablesFromLazyFunction(fun, pc))
            return false;

        // The position passed to tokenStream.advance() is relative to
        // userbuf.base() while LazyScript::{begin,end} offsets are relative to
        // the outermost script source. N.B: userbuf.base() is initialized
        // (in TokenStream()) to begin() - column() so that column numbers in
        // the lazily parsed script are correct.
        uint32_t userbufBase = lazyOuter->begin() - lazyOuter->column();
        tokenStream.advance(fun->lazyScript()->end() - userbufBase);

        *pbodyProcessed = true;
        return true;
    }

    return true;
}

template <class T, class U>
static inline void
PropagateTransitiveParseFlags(const T *inner, U *outer)
{
   if (inner->bindingsAccessedDynamically())
     outer->setBindingsAccessedDynamically();
   if (inner->hasDebuggerStatement())
     outer->setHasDebuggerStatement();
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::addFreeVariablesFromLazyFunction(JSFunction *fun,
                                                       ParseContext<ParseHandler> *pc)
{
    // Update any definition nodes in this context according to free variables
    // in a lazily parsed inner function.

    LazyScript *lazy = fun->lazyScript();
    HeapPtrAtom *freeVariables = lazy->freeVariables();
    for (size_t i = 0; i < lazy->numFreeVariables(); i++) {
        JSAtom *atom = freeVariables[i];

        // 'arguments' will be implicitly bound within the inner function.
        if (atom == context->names().arguments)
            continue;

        DefinitionNode dn = pc->decls().lookupFirst(atom);

        if (!dn) {
            dn = getOrCreateLexicalDependency(pc, atom);
            if (!dn)
                return false;
        }

        /* Mark the outer dn as escaping. */
        handler.setFlag(handler.getDefinitionNode(dn), PND_CLOSED);
    }

    PropagateTransitiveParseFlags(lazy, pc->sc);
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkFunctionDefinition(HandlePropertyName funName,
                                                    Node *pn, FunctionSyntaxKind kind,
                                                    bool *pbodyProcessed)
{
    *pbodyProcessed = false;

    /* Function statements add a binding to the enclosing scope. */
    bool bodyLevel = pc->atBodyLevel();

    if (kind == Statement) {
        /*
         * Handle redeclaration and optimize cases where we can statically bind the
         * function (thereby avoiding JSOP_DEFFUN and dynamic name lookup).
         */
        if (DefinitionNode dn = pc->decls().lookupFirst(funName)) {
            if (dn == Definition::CONST) {
                JSAutoByteString name;
                if (!js_AtomToPrintableString(context, funName, &name) ||
                    !report(ParseError, false, null(), JSMSG_REDECLARED_VAR,
                            Definition::kindString(dn), name.ptr()))
                {
                    return false;
                }
            }
        } else if (bodyLevel) {
            if (pc->lexdeps.lookupDefn<SyntaxParseHandler>(funName))
                pc->lexdeps->remove(funName);

            if (!pc->define(context, funName, *pn, Definition::VAR))
                return false;
        }

        if (!bodyLevel && funName == context->names().arguments)
            pc->sc->setBindingsAccessedDynamically();
    }

    if (kind == Arrow) {
        /* Arrow functions cannot yet be parsed lazily. */
        return abortIfSyntaxParser();
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionDef(HandlePropertyName funName, const TokenStream::Position &start,
                                  size_t startOffset, FunctionType type, FunctionSyntaxKind kind)
{
    JS_ASSERT_IF(kind == Statement, funName);

    /* Make a TOK_FUNCTION node. */
    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();

    bool bodyProcessed;
    if (!checkFunctionDefinition(funName, &pn, kind, &bodyProcessed))
        return null();

    if (bodyProcessed)
        return pn;

    RootedFunction fun(context, newFunction(pc, funName, kind));
    if (!fun)
        return null();

    // If the outer scope is strict, immediately parse the function in strict
    // mode. Otherwise, we parse it normally. If we see a "use strict"
    // directive, we backup and reparse it as strict.
    handler.setFunctionBody(pn, null());
    bool initiallyStrict = kind == Arrow || pc->sc->strict;
    bool becameStrict;
    if (!functionArgsAndBody(pn, fun, funName, startOffset,
                             type, kind, initiallyStrict, &becameStrict))
    {
        if (initiallyStrict || !becameStrict || tokenStream.hadError())
            return null();

        // Reparse the function in strict mode.
        tokenStream.seek(start);
        if (funName && tokenStream.getToken() == TOK_ERROR)
            return null();
        handler.setFunctionBody(pn, null());
        if (!functionArgsAndBody(pn, fun, funName, startOffset, type, kind, true))
            return null();
    }

    return pn;
}

template <>
bool
Parser<FullParseHandler>::finishFunctionDefinition(ParseNode *pn, FunctionBox *funbox,
                                                   ParseNode *prelude, ParseNode *body)
{
    pn->pn_pos.end = tokenStream.currentToken().pos.end;

#if JS_HAS_DESTRUCTURING
    /*
     * If there were destructuring formal parameters, prepend the initializing
     * comma expression that we synthesized to body. If the body is a return
     * node, we must make a special PNK_SEQ node, to prepend the destructuring
     * code without bracing the decompilation of the function body.
     */
    if (prelude) {
        if (!body->isArity(PN_LIST)) {
            ParseNode *block;

            block = ListNode::create(PNK_SEQ, &handler);
            if (!block)
                return false;
            block->pn_pos = body->pn_pos;
            block->initList(body);

            body = block;
        }

        ParseNode *item = UnaryNode::create(PNK_SEMI, &handler);
        if (!item)
            return false;

        item->pn_pos.begin = item->pn_pos.end = body->pn_pos.begin;
        item->pn_kid = prelude;
        item->pn_next = body->pn_head;
        body->pn_head = item;
        if (body->pn_tail == &body->pn_head)
            body->pn_tail = &item->pn_next;
        ++body->pn_count;
        body->pn_xflags |= PNX_DESTRUCT;
    }
#endif

    pn->pn_funbox = funbox;
    pn->pn_body->append(body);
    pn->pn_body->pn_pos = body->pn_pos;

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::finishFunctionDefinition(Node pn, FunctionBox *funbox,
                                                     Node prelude, Node body)
{
    // The LazyScript for a lazily parsed function needs to be constructed
    // while its ParseContext and associated lexdeps and inner functions are
    // still available.

    if (funbox->inWith)
        return abortIfSyntaxParser();

    size_t numFreeVariables = pc->lexdeps->count();
    size_t numInnerFunctions = pc->innerFunctions.length();

    LazyScript *lazy = LazyScript::Create(context, numFreeVariables, numInnerFunctions, versionNumber(),
                                          funbox->bufStart, funbox->bufEnd,
                                          funbox->startLine, funbox->startColumn);
    if (!lazy)
        return false;

    HeapPtrAtom *freeVariables = lazy->freeVariables();
    size_t i = 0;
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront())
        freeVariables[i++].init(r.front().key());
    JS_ASSERT(i == numFreeVariables);

    HeapPtrFunction *innerFunctions = lazy->innerFunctions();
    for (size_t i = 0; i < numInnerFunctions; i++)
        innerFunctions[i].init(pc->innerFunctions[i]);

    if (pc->sc->strict)
        lazy->setStrict();
    if (funbox->usesArguments && funbox->usesApply)
        lazy->setUsesArgumentsAndApply();
    PropagateTransitiveParseFlags(funbox, lazy);

    funbox->object->toFunction()->initLazyScript(lazy);
    return true;
}

template <>
bool
Parser<FullParseHandler>::functionArgsAndBody(ParseNode *pn, HandleFunction fun,
                                              HandlePropertyName funName,
                                              size_t startOffset, FunctionType type,
                                              FunctionSyntaxKind kind,
                                              bool strict, bool *becameStrict)
{
    if (becameStrict)
        *becameStrict = false;
    ParseContext<FullParseHandler> *outerpc = pc;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox *funbox = newFunctionBox(fun, pc, strict);
    if (!funbox)
        return false;

    // Try a syntax parse for this inner function.
    do {
        Parser<SyntaxParseHandler> *parser = handler.syntaxParser;
        if (!parser)
            break;

        {
            // Move the syntax parser to the current position in the stream.
            TokenStream::Position position(keepAtoms);
            tokenStream.tell(&position);
            parser->tokenStream.seek(position, tokenStream);

            ParseContext<SyntaxParseHandler> funpc(parser, outerpc, funbox,
                                                   outerpc->staticLevel + 1, outerpc->blockidGen);
            if (!funpc.init())
                return false;

            if (!parser->functionArgsAndBodyGeneric(SyntaxParseHandler::NodeGeneric,
                                                    fun, funName, type, kind, strict, becameStrict))
            {
                if (parser->hadAbortedSyntaxParse()) {
                    // Try again with a full parse.
                    parser->clearAbortedSyntaxParse();
                    break;
                }
                return false;
            }

            outerpc->blockidGen = funpc.blockidGen;

            // Advance this parser over tokens processed by the syntax parser.
            parser->tokenStream.tell(&position);
            tokenStream.seek(position, parser->tokenStream);
        }

        pn->pn_funbox = funbox;

        if (!addFreeVariablesFromLazyFunction(fun, pc))
            return false;

        pn->pn_blockid = outerpc->blockid();
        PropagateTransitiveParseFlags(funbox, outerpc->sc);
        return true;
    } while (false);

    // Continue doing a full parse for this inner function.
    ParseContext<FullParseHandler> funpc(this, pc, funbox,
                                         outerpc->staticLevel + 1, outerpc->blockidGen);
    if (!funpc.init())
        return false;

    if (!functionArgsAndBodyGeneric(pn, fun, funName, type, kind, strict, becameStrict))
        return false;

    if (!leaveFunction(pn, funName, outerpc, kind))
        return false;

    pn->pn_blockid = outerpc->blockid();

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
Parser<SyntaxParseHandler>::functionArgsAndBody(Node pn, HandleFunction fun,
                                                HandlePropertyName funName,
                                                size_t startOffset, FunctionType type,
                                                FunctionSyntaxKind kind,
                                                bool strict, bool *becameStrict)
{
    if (becameStrict)
        *becameStrict = false;
    ParseContext<SyntaxParseHandler> *outerpc = pc;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox *funbox = newFunctionBox(fun, pc, strict);
    if (!funbox)
        return false;

    // Initialize early for possible flags mutation via destructuringExpr.
    ParseContext<SyntaxParseHandler> funpc(this, pc, funbox,
                                           outerpc->staticLevel + 1, outerpc->blockidGen);
    if (!funpc.init())
        return false;

    if (!functionArgsAndBodyGeneric(pn, fun, funName, type, kind, strict, becameStrict))
        return false;

    if (!leaveFunction(pn, funName, outerpc, kind))
        return false;

    // This is a lazy function inner to another lazy function. Remember the
    // inner function so that if the outer function is eventually parsed we do
    // not need any further parsing or processing of the inner function.
    JS_ASSERT(fun->lazyScript());
    return outerpc->innerFunctions.append(fun);
}

template <>
ParseNode *
Parser<FullParseHandler>::standaloneLazyFunction(HandleFunction fun, unsigned staticLevel,
                                                 bool strict)
{
    Node pn = handler.newFunctionDefinition();
    if (!pn)
        return null();

    FunctionBox *funbox = newFunctionBox(fun, /* outerpc = */ NULL, strict);
    if (!funbox)
        return null();
    handler.setFunctionBox(pn, funbox);

    ParseContext<FullParseHandler> funpc(this, NULL, funbox, staticLevel, 0);
    if (!funpc.init())
        return null();

    RootedPropertyName funName(context, fun->atom() ? fun->atom()->asPropertyName() : NULL);

    if (!functionArgsAndBodyGeneric(pn, fun, funName, Normal, Statement, strict, NULL))
        return null();

    if (fun->isNamedLambda()) {
        if (AtomDefnPtr p = pc->lexdeps->lookup(funName)) {
            Definition *dn = p.value().get<FullParseHandler>();
            if (!ConvertDefinitionToNamedLambdaUse(context, pc, funbox, dn))
                return NULL;
        }
    }

    InternalHandle<Bindings*> bindings =
        InternalHandle<Bindings*>::fromMarkedLocation(&funbox->bindings);
    if (!pc->generateFunctionBindings(context, bindings))
        return null();

    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionArgsAndBodyGeneric(Node pn, HandleFunction fun,
                                                 HandlePropertyName funName, FunctionType type,
                                                 FunctionSyntaxKind kind,
                                                 bool strict, bool *becameStrict)
{
    // Given a properly initialized parse context, try to parse an actual
    // function without concern for conversion to strict mode, use of lazy
    // parsing and such.

    Node prelude = null();
    bool hasRest;
    if (!functionArguments(kind, &prelude, pn, hasRest))
        return false;

    FunctionBox *funbox = pc->sc->asFunctionBox();

    fun->setArgCount(pc->numArgs());
    if (funbox->ndefaults)
        fun->setHasDefaults();
    if (hasRest)
        fun->setHasRest();

    if (type == Getter && fun->nargs > 0) {
        report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "getter", "no", "s");
        return false;
    }
    if (type == Setter && fun->nargs != 1) {
        report(ParseError, false, null(), JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
        return false;
    }

    if (kind == Arrow && !tokenStream.matchToken(TOK_ARROW)) {
        report(ParseError, false, null(), JSMSG_BAD_ARROW_ARGS);
        return false;
    }

    // Parse the function body.
    Maybe<GenexpGuard<ParseHandler> > yieldGuard;
    if (kind == Arrow)
        yieldGuard.construct(this);

    FunctionBodyType bodyType = StatementListBody;
    if (tokenStream.getToken(TSF_OPERAND) != TOK_LC) {
        tokenStream.ungetToken();
        bodyType = ExpressionBody;
        fun->setIsExprClosure();
    }

    Node body = functionBody(kind, bodyType);
    if (!body) {
        // Notify the caller if this function was discovered to be strict.
        if (becameStrict && pc->funBecameStrict)
            *becameStrict = true;
        return false;
    }

    if (!yieldGuard.empty() && !yieldGuard.ref().checkValidBody(body, JSMSG_YIELD_IN_ARROW))
        return false;

    if (funName && !checkStrictBinding(funName, pn))
        return false;

#if JS_HAS_EXPR_CLOSURES
    if (bodyType == StatementListBody) {
#endif
        if (!tokenStream.matchToken(TOK_RC)) {
            report(ParseError, false, null(), JSMSG_CURLY_AFTER_BODY);
            return false;
        }
        funbox->bufEnd = tokenStream.currentToken().pos.begin + 1;
#if JS_HAS_EXPR_CLOSURES
    } else {
        if (tokenStream.hadError())
            return false;
        funbox->bufEnd = tokenStream.currentToken().pos.end;
        if (kind == Statement && !MatchOrInsertSemicolon(context, &tokenStream))
            return false;
    }
#endif

    return finishFunctionDefinition(pn, funbox, prelude, body);
}

template <>
ParseNode *
Parser<FullParseHandler>::moduleDecl()
{
    JS_ASSERT(tokenStream.currentToken().name() == context->runtime()->atomState.module);
    if (!((pc->sc->isGlobalSharedContext() || pc->sc->isModuleBox()) && pc->atBodyLevel()))
    {
        report(ParseError, false, NULL, JSMSG_MODULE_STATEMENT);
        return NULL;
    }

    ParseNode *pn = CodeNode::create(PNK_MODULE, &handler);
    if (!pn)
        return NULL;
    JS_ALWAYS_TRUE(tokenStream.matchToken(TOK_STRING));
    RootedAtom atom(context, tokenStream.currentToken().atom());
    Module *module = Module::create(context, atom);
    if (!module)
        return NULL;
    ModuleBox *modulebox = newModuleBox(module, pc);
    if (!modulebox)
        return NULL;
    pn->pn_modulebox = modulebox;

    ParseContext<FullParseHandler> modulepc(this, pc, modulebox, pc->staticLevel + 1, pc->blockidGen);
    if (!modulepc.init())
        return NULL;
    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_MODULE);
    pn->pn_body = statements();
    if (!pn->pn_body)
        return NULL;
    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_MODULE);

    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::moduleDecl()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionStmt()
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_FUNCTION);
    RootedPropertyName name(context);
    if (tokenStream.getToken(TSF_KEYWORD_IS_NAME) == TOK_NAME) {
        name = tokenStream.currentToken().name();
    } else {
        /* Unnamed function expressions are forbidden in statement context. */
        report(ParseError, false, null(), JSMSG_UNNAMED_FUNCTION_STMT);
        return null();
    }

    TokenStream::Position start(keepAtoms);
    tokenStream.positionAfterLastFunctionKeyword(start);

    /* We forbid function statements in strict mode code. */
    if (!pc->atBodyLevel() && pc->sc->needStrictChecks() &&
        !report(ParseStrictError, pc->sc->strict, null(), JSMSG_STRICT_FUNCTION_STATEMENT))
        return null();

    return functionDef(name, start, tokenStream.positionToOffset(start), Normal, Statement);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::functionExpr()
{
    RootedPropertyName name(context);
    JS_ASSERT(tokenStream.currentToken().type == TOK_FUNCTION);
    TokenStream::Position start(keepAtoms);
    tokenStream.positionAfterLastFunctionKeyword(start);
    if (tokenStream.getToken(TSF_KEYWORD_IS_NAME) == TOK_NAME)
        name = tokenStream.currentToken().name();
    else
        tokenStream.ungetToken();
    return functionDef(name, start, tokenStream.positionToOffset(start), Normal, Expression);
}

/*
 * Return true if this node, known to be an unparenthesized string literal,
 * could be the string of a directive in a Directive Prologue. Directive
 * strings never contain escape sequences or line continuations.
 * isEscapeFreeStringLiteral, below, checks whether the node itself could be
 * a directive.
 */
static inline bool
IsEscapeFreeStringLiteral(const TokenPos &pos, JSAtom *str)
{
    /*
     * If the string's length in the source code is its length as a value,
     * accounting for the quotes, then it must not contain any escape
     * sequences or line continuations.
     */
    return pos.begin + str->length() + 2 == pos.end;
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
Parser<ParseHandler>::maybeParseDirective(Node pn, bool *cont)
{
    TokenPos directivePos;
    JSAtom *directive = handler.isStringExprStatement(pn, &directivePos);

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

        if (directive == context->runtime()->atomState.useStrict) {
            // We're going to be in strict mode. Note that this scope explicitly
            // had "use strict";
            pc->sc->setExplicitUseStrict();
            if (!pc->sc->strict) {
                if (pc->sc->isFunctionBox()) {
                    // Request that this function be reparsed as strict.
                    pc->funBecameStrict = true;
                    return false;
                } else {
                    // We don't reparse global scopes, so we keep track of the
                    // one possible strict violation that could occur in the
                    // directive prologue -- octal escapes -- and complain now.
                    if (tokenStream.sawOctalEscape()) {
                        report(ParseError, false, null(), JSMSG_DEPRECATED_OCTAL);
                        return false;
                    }
                    pc->sc->strict = true;
                }
            }
        } else if (directive == context->names().useAsm) {
            if (pc->sc->isFunctionBox()) {
                pc->sc->asFunctionBox()->useAsm = true;
                pc->sc->asFunctionBox()->asmStart = handler.getPosition(pn).begin;
                if (!abortIfSyntaxParser())
                    return false;
            } else {
                if (!report(ParseWarning, false, pn, JSMSG_USE_ASM_DIRECTIVE_FAIL))
                    return false;
            }
        }
    }
    return true;
}

template <>
void
Parser<FullParseHandler>::addStatementToList(ParseNode *pn, ParseNode *kid)
{
    JS_ASSERT(pn->isKind(PNK_STATEMENTLIST));

    if (kid->isKind(PNK_FUNCTION)) {
        /*
         * PNX_FUNCDEFS notifies the emitter that the block contains body-
         * level function definitions that should be processed before the
         * rest of nodes.
         */
        if (pc->atBodyLevel()) {
            pn->pn_xflags |= PNX_FUNCDEFS;
        } else {
            /* General deoptimization was done in functionDef. */
            JS_ASSERT_IF(pc->sc->isFunctionBox(), pc->sc->asFunctionBox()->hasExtensibleScope());
        }
    }

    pn->append(kid);
    pn->pn_pos.end = kid->pn_pos.end;
}

template <>
void
Parser<SyntaxParseHandler>::addStatementToList(Node pn, Node kid)
{
}

/*
 * Parse the statements in a block, creating a TOK_LC node that lists the
 * statements' trees.  If called from block-parsing code, the caller must
 * match { before and } after.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statements()
{
    JS_CHECK_RECURSION(context, return null());

    Node pn = handler.newList(PNK_STATEMENTLIST);
    if (!pn)
        return null();
    handler.setBlockId(pn, pc->blockid());

    Node saveBlock = pc->blockNode;
    pc->blockNode = pn;

    bool canHaveDirectives = pc->atBodyLevel();
    for (;;) {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt <= TOK_EOF || tt == TOK_RC) {
            if (tt == TOK_ERROR) {
                if (tokenStream.isEOF())
                    tokenStream.setUnexpectedEOF();
                return null();
            }
            break;
        }
        Node next = statement();
        if (!next) {
            if (tokenStream.isEOF())
                tokenStream.setUnexpectedEOF();
            return null();
        }

        if (canHaveDirectives) {
            if (!maybeParseDirective(next, &canHaveDirectives))
                return null();
        }

        addStatementToList(pn, next);
    }

    /*
     * Handle the case where there was a let declaration under this block.  If
     * it replaced pc->blockNode with a new block node then we must refresh pn
     * and then restore pc->blockNode.
     */
    if (pc->blockNode != pn)
        pn = pc->blockNode;
    pc->blockNode = saveBlock;

    handler.setEndPosition(pn, tokenStream.currentToken().pos.end);
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::condition()
{
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    Node pn = parenExpr();
    if (!pn)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isOperationWithoutParens(pn, PNK_ASSIGN) &&
        !report(ParseExtraWarning, false, null(), JSMSG_EQUAL_AS_ASSIGN))
    {
        return null();
    }
    return pn;
}

static bool
MatchLabel(JSContext *cx, TokenStream *ts, MutableHandlePropertyName label)
{
    TokenKind tt = ts->peekTokenSameLine(TSF_OPERAND);
    if (tt == TOK_ERROR)
        return false;
    if (tt == TOK_NAME) {
        (void) ts->getToken();
        label.set(ts->currentToken().name());
    } else {
        label.set(NULL);
    }
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::reportRedeclaration(Node pn, bool isConst, JSAtom *atom)
{
    JSAutoByteString name;
    if (js_AtomToPrintableString(context, atom, &name))
        report(ParseError, false, pn, JSMSG_REDECLARED_VAR, isConst ? "const" : "variable", name.ptr());
    return false;
}

/*
 * Define a let-variable in a block, let-expression, or comprehension scope. pc
 * must already be in such a scope.
 *
 * Throw a SyntaxError if 'atom' is an invalid name. Otherwise create a
 * property for the new variable on the block object, pc->blockChain;
 * populate data->pn->pn_{op,cookie,defn,dflags}; and stash a pointer to
 * data->pn in a slot of the block object.
 */
template <>
/* static */ bool
Parser<FullParseHandler>::bindLet(JSContext *cx, BindData<FullParseHandler> *data,
                                  HandlePropertyName name, Parser<FullParseHandler> *parser)
{
    ParseContext<FullParseHandler> *pc = parser->pc;
    ParseNode *pn = data->pn;
    if (!parser->checkStrictBinding(name, pn))
        return false;

    Rooted<StaticBlockObject *> blockObj(cx, data->let.blockObj);
    unsigned blockCount = blockObj->slotCount();
    if (blockCount == JS_BIT(16)) {
        parser->report(ParseError, false, pn, data->let.overflow);
        return false;
    }

    /*
     * Assign block-local index to pn->pn_cookie right away, encoding it as an
     * upvar cookie whose skip tells the current static level. The emitter will
     * adjust the node's slot based on its stack depth model -- and, for global
     * and eval code, js::frontend::CompileScript will adjust the slot
     * again to include script->nfixed.
     */
    if (!pn->pn_cookie.set(parser->context, pc->staticLevel, uint16_t(blockCount)))
        return false;

    /*
     * For bindings that are hoisted to the beginning of the block/function,
     * define() right now. Otherwise, delay define until PushLetScope.
     */
    if (data->let.varContext == HoistVars) {
        JS_ASSERT(!pc->atBodyLevel());
        Definition *dn = pc->decls().lookupFirst(name);
        if (dn && dn->pn_blockid == pc->blockid())
            return parser->reportRedeclaration(pn, dn->isConst(), name);
        if (!pc->define(cx, name, pn, Definition::LET))
            return false;
    }

    /*
     * Define the let binding's property before storing pn in the the binding's
     * slot indexed by blockCount off the class-reserved slot base.
     */
    bool redeclared;
    RootedId id(cx, NameToId(name));
    RootedShape shape(cx, StaticBlockObject::addVar(cx, blockObj, id, blockCount, &redeclared));
    if (!shape) {
        if (redeclared)
            parser->reportRedeclaration(pn, false, name);
        return false;
    }

    /* Store pn in the static block object. */
    blockObj->setDefinitionParseNode(blockCount, reinterpret_cast<Definition *>(pn));
    return true;
}

template <>
/* static */ bool
Parser<SyntaxParseHandler>::bindLet(JSContext *cx, BindData<SyntaxParseHandler> *data,
                                    HandlePropertyName name, Parser<SyntaxParseHandler> *parser)
{
    return true;
}

template <typename ParseHandler, class Op>
static inline bool
ForEachLetDef(JSContext *cx, ParseContext<ParseHandler> *pc,
              HandleStaticBlockObject blockObj, Op op)
{
    for (Shape::Range<CanGC> r(cx, blockObj->lastProperty()); !r.empty(); r.popFront()) {
        Shape &shape = r.front();

        /* Beware the destructuring dummy slots. */
        if (JSID_IS_INT(shape.propid()))
            continue;

        if (!op(cx, pc, blockObj, shape, JSID_TO_ATOM(shape.propid())))
            return false;
    }
    return true;
}

template <typename ParseHandler>
struct PopLetDecl {
    bool operator()(JSContext *, ParseContext<ParseHandler> *pc, HandleStaticBlockObject,
                    const Shape &, JSAtom *atom)
    {
        pc->popLetDecl(atom);
        return true;
    }
};

template <typename ParseHandler>
static void
PopStatementPC(JSContext *cx, ParseContext<ParseHandler> *pc)
{
    RootedStaticBlockObject blockObj(cx, pc->topStmt->blockObj);
    JS_ASSERT(!!blockObj == (pc->topStmt->isBlockScope));

    FinishPopStatement(pc);

    if (blockObj) {
        JS_ASSERT(!blockObj->inDictionaryMode());
        ForEachLetDef(cx, pc, blockObj, PopLetDecl<ParseHandler>());
        blockObj->resetPrevBlockChainFromParser();
    }
}

/*
 * The function LexicalLookup searches a static binding for the given name in
 * the stack of statements enclosing the statement currently being parsed. Each
 * statement that introduces a new scope has a corresponding scope object, on
 * which the bindings for that scope are stored. LexicalLookup either returns
 * the innermost statement which has a scope object containing a binding with
 * the given name, or NULL.
 */
template <class ContextT>
typename ContextT::StmtInfo *
LexicalLookup(ContextT *ct, HandleAtom atom, int *slotp, typename ContextT::StmtInfo *stmt)
{
    RootedId id(ct->sc->context, AtomToId(atom));

    if (!stmt)
        stmt = ct->topScopeStmt;
    for (; stmt; stmt = stmt->downScope) {
        /*
         * With-statements introduce dynamic bindings. Since dynamic bindings
         * can potentially override any static bindings introduced by statements
         * further up the stack, we have to abort the search.
         */
        if (stmt->type == STMT_WITH)
            break;

        // Skip statements that do not introduce a new scope
        if (!stmt->isBlockScope)
            continue;

        StaticBlockObject &blockObj = *stmt->blockObj;
        Shape *shape = blockObj.nativeLookup(ct->sc->context, id);
        if (shape) {
            JS_ASSERT(shape->hasShortID());

            if (slotp)
                *slotp = blockObj.stackDepth() + shape->shortid();
            return stmt;
        }
    }

    if (slotp)
        *slotp = -1;
    return stmt;
}

template <typename ParseHandler>
static inline bool
OuterLet(ParseContext<ParseHandler> *pc, StmtInfoPC *stmt, HandleAtom atom)
{
    while (stmt->downScope) {
        stmt = LexicalLookup(pc, atom, NULL, stmt->downScope);
        if (!stmt)
            return false;
        if (stmt->type == STMT_BLOCK)
            return true;
    }
    return false;
}

template <typename ParseHandler>
/* static */ bool
Parser<ParseHandler>::bindVarOrConst(JSContext *cx, BindData<ParseHandler> *data,
                                     HandlePropertyName name, Parser<ParseHandler> *parser)
{
    ParseContext<ParseHandler> *pc = parser->pc;
    Node pn = data->pn;
    bool isConstDecl = data->op == JSOP_DEFCONST;

    /* Default best op for pn is JSOP_NAME; we'll try to improve below. */
    parser->handler.setOp(pn, JSOP_NAME);

    if (!parser->checkStrictBinding(name, pn))
        return false;

    StmtInfoPC *stmt = LexicalLookup(pc, name, NULL, (StmtInfoPC *)NULL);

    if (stmt && stmt->type == STMT_WITH) {
        parser->handler.setFlag(pn, PND_DEOPTIMIZED);
        if (pc->sc->isFunctionBox()) {
            FunctionBox *funbox = pc->sc->asFunctionBox();
            funbox->setMightAliasLocals();

            /*
             * This definition isn't being added to the parse context's
             * declarations, so make sure to indicate the need to deoptimize
             * the script's arguments object.
             */
            HandlePropertyName arguments = cx->names().arguments;
            if (name == arguments) {
                Node pn = parser->handler.newName(arguments, pc);
                if (!pc->define(parser->context, arguments, pn, Definition::VAR))
                    return false;
                funbox->setArgumentsHasLocalBinding();
                funbox->setDefinitelyNeedsArgsObj();
            }
        }
        return true;
    }

    DefinitionList::Range defs = pc->decls().lookupMulti(name);
    JS_ASSERT_IF(stmt, !defs.empty());

    if (defs.empty())
        return pc->define(cx, name, pn, isConstDecl ? Definition::CONST : Definition::VAR);

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
        if (!js_AtomToPrintableString(cx, name, &bytes))
            return false;

        if (isConstDecl) {
            parser->report(ParseError, false, pn, JSMSG_REDECLARED_PARAM, bytes.ptr());
            return false;
        }
        if (!parser->report(ParseExtraWarning, false, pn, JSMSG_VAR_HIDES_ARG, bytes.ptr()))
            return false;
    } else {
        bool error = (isConstDecl ||
                      dn_kind == Definition::CONST ||
                      (dn_kind == Definition::LET &&
                       (stmt->type != STMT_CATCH || OuterLet(pc, stmt, name))));

        if (cx->hasExtraWarningsOption()
            ? data->op != JSOP_DEFVAR || dn_kind != Definition::VAR
            : error)
        {
            JSAutoByteString bytes;
            ParseReportKind reporter = error ? ParseError : ParseExtraWarning;
            if (!js_AtomToPrintableString(cx, name, &bytes) ||
                !parser->report(reporter, false, pn, JSMSG_REDECLARED_VAR,
                                Definition::kindString(dn_kind), bytes.ptr()))
            {
                return false;
            }
        }
    }

    parser->handler.linkUseToDef(pn, dn);
    return true;
}

template <>
bool
Parser<FullParseHandler>::makeSetCall(ParseNode *pn, unsigned msg)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(pn->isOp(JSOP_CALL) || pn->isOp(JSOP_EVAL) ||
              pn->isOp(JSOP_FUNCALL) || pn->isOp(JSOP_FUNAPPLY));
    if (!report(ParseStrictError, pc->sc->strict, pn, msg))
        return false;

    ParseNode *pn2 = pn->pn_head;
    if (pn2->isKind(PNK_FUNCTION) && (pn2->pn_funbox->inGenexpLambda)) {
        report(ParseError, false, pn, msg);
        return false;
    }
    pn->pn_xflags |= PNX_SETCALL;
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteNameUse(HandlePropertyName name, Node pn)
{
    StmtInfoPC *stmt = LexicalLookup(pc, name, NULL, (StmtInfoPC *)NULL);

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

    if (stmt && stmt->type == STMT_WITH)
        handler.setFlag(pn, PND_DEOPTIMIZED);

    return true;
}

#if JS_HAS_DESTRUCTURING

template <>
bool
Parser<FullParseHandler>::bindDestructuringVar(BindData<FullParseHandler> *data, ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_NAME));

    RootedPropertyName name(context, pn->pn_atom->asPropertyName());

    data->pn = pn;
    if (!data->binder(context, data, name, this))
        return false;

    /*
     * Select the appropriate name-setting opcode, respecting eager selection
     * done by the data->binder function.
     */
    if (pn->pn_dflags & PND_BOUND)
        pn->setOp(JSOP_SETLOCAL);
    else if (data->op == JSOP_DEFCONST)
        pn->setOp(JSOP_SETCONST);
    else
        pn->setOp(JSOP_SETNAME);

    if (data->op == JSOP_DEFCONST)
        pn->pn_dflags |= PND_CONST;

    pn->markAsAssigned();
    return true;
}

/*
 * Here, we are destructuring {... P: Q, ...} = R, where P is any id, Q is any
 * LHS expression except a destructuring initialiser, and R is on the stack.
 * Because R is already evaluated, the usual LHS-specialized bytecodes won't
 * work.  After pushing R[P] we need to evaluate Q's "reference base" QB and
 * then push its property name QN.  At this point the stack looks like
 *
 *   [... R, R[P], QB, QN]
 *
 * We need to set QB[QN] = R[P].  This is a job for JSOP_ENUMELEM, which takes
 * its operands with left-hand side above right-hand side:
 *
 *   [rval, lval, xval]
 *
 * and pops all three values, setting lval[xval] = rval.  But we cannot select
 * JSOP_ENUMELEM yet, because the LHS may turn out to be an arg or local var,
 * which can be optimized further.  So we select JSOP_SETNAME.
 */
template <>
bool
Parser<FullParseHandler>::bindDestructuringLHS(ParseNode *pn)
{
    switch (pn->getKind()) {
      case PNK_NAME:
        pn->markAsAssigned();
        /* FALL THROUGH */

      case PNK_DOT:
      case PNK_ELEM:
        /*
         * We may be called on a name node that has already been specialized,
         * in the very weird and ECMA-262-required "for (var [x] = i in o) ..."
         * case. See bug 558633.
         */
        if (!(js_CodeSpec[pn->getOp()].format & JOF_SET))
            pn->setOp(JSOP_SETNAME);
        break;

      case PNK_CALL:
        if (!makeSetCall(pn, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return false;
        break;

      default:
        report(ParseError, false, pn, JSMSG_BAD_LEFTSIDE_OF_ASS);
        return false;
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
 * - declaration-like: |var| and |let| declarations, functions' formal
 *   parameter lists, |catch| clauses, and comprehension tails.  In
 *   these cases, the patterns' property value positions must be
 *   simple names; the destructuring defines them as new variables.
 *
 * In both cases, other code parses the pattern as an arbitrary
 * primaryExpr, and then, here in CheckDestructuring, verify that the
 * tree is a valid destructuring expression.
 *
 * In assignment-like contexts, we parse the pattern with
 * pc->inDeclDestructuring clear, so the lvalue expressions in the
 * pattern are parsed normally.  primaryExpr links variable references
 * into the appropriate use chains; creates placeholder definitions;
 * and so on.  CheckDestructuring is called with |data| NULL (since we
 * won't be binding any new names), and we specialize lvalues as
 * appropriate.
 *
 * In declaration-like contexts, the normal variable reference
 * processing would just be an obstruction, because we're going to
 * define the names that appear in the property value positions as new
 * variables anyway.  In this case, we parse the pattern with
 * pc->inDeclDestructuring set, which directs primaryExpr to leave
 * whatever name nodes it creates unconnected.  Then, here in
 * CheckDestructuring, we require the pattern's property value
 * positions to be simple names, and define them as appropriate to the
 * context.  For these calls, |data| points to the right sort of
 * BindData.
 *
 * The 'toplevel' is a private detail of the recursive strategy used by
 * CheckDestructuring and callers should use the default value.
 */
template <>
bool
Parser<FullParseHandler>::checkDestructuring(BindData<FullParseHandler> *data,
                                             ParseNode *left, bool toplevel)
{
    bool ok;

    if (left->isKind(PNK_ARRAYCOMP)) {
        report(ParseError, false, left, JSMSG_ARRAY_COMP_LEFTSIDE);
        return false;
    }

    Rooted<StaticBlockObject *> blockObj(context);
    blockObj = data && data->binder == bindLet ? data->let.blockObj.get() : NULL;
    uint32_t blockCountBefore = blockObj ? blockObj->slotCount() : 0;

    if (left->isKind(PNK_ARRAY)) {
        for (ParseNode *pn = left->pn_head; pn; pn = pn->pn_next) {
            if (!pn->isKind(PNK_ELISION)) {
                if (pn->isKind(PNK_ARRAY) || pn->isKind(PNK_OBJECT)) {
                    ok = checkDestructuring(data, pn, false);
                } else {
                    if (data) {
                        if (!pn->isKind(PNK_NAME)) {
                            report(ParseError, false, pn, JSMSG_NO_VARIABLE_NAME);
                            return false;
                        }
                        ok = bindDestructuringVar(data, pn);
                    } else {
                        ok = bindDestructuringLHS(pn);
                    }
                }
                if (!ok)
                    return false;
            }
        }
    } else {
        JS_ASSERT(left->isKind(PNK_OBJECT));
        for (ParseNode *pair = left->pn_head; pair; pair = pair->pn_next) {
            JS_ASSERT(pair->isKind(PNK_COLON));
            ParseNode *pn = pair->pn_right;

            if (pn->isKind(PNK_ARRAY) || pn->isKind(PNK_OBJECT)) {
                ok = checkDestructuring(data, pn, false);
            } else if (data) {
                if (!pn->isKind(PNK_NAME)) {
                    report(ParseError, false, pn, JSMSG_NO_VARIABLE_NAME);
                    return false;
                }
                ok = bindDestructuringVar(data, pn);
            } else {
                /*
                 * If right and left point to the same node, then this is
                 * destructuring shorthand ({x} = ...). In that case,
                 * identifierName was not used to parse 'x' so 'x' has not been
                 * officially linked to its def or registered in lexdeps. Do
                 * that now.
                 */
                if (pair->pn_right == pair->pn_left) {
                    RootedPropertyName name(context, pn->pn_atom->asPropertyName());
                    if (!noteNameUse(name, pn))
                        return false;
                }
                ok = bindDestructuringLHS(pn);
            }
            if (!ok)
                return false;
        }
    }

    /*
     * The catch/finally handler implementation in the interpreter assumes
     * that any operation that introduces a new scope (like a "let" or "with"
     * block) increases the stack depth. This way, it is possible to restore
     * the scope chain based on stack depth of the handler alone. "let" with
     * an empty destructuring pattern like in
     *
     *   let [] = 1;
     *
     * would violate this assumption as the there would be no let locals to
     * store on the stack.
     *
     * Furthermore, the decompiler needs an abstract stack location to store
     * the decompilation of each let block/expr initializer. E.g., given:
     *
     *   let (x = 1, [[]] = b, y = 3, {a:[]} = c) { ... }
     *
     * four slots are needed.
     *
     * To satisfy both constraints, we push a dummy slot (and add a
     * corresponding dummy property to the block object) for each initializer
     * that doesn't introduce at least one binding.
     */
    if (toplevel && blockObj && blockCountBefore == blockObj->slotCount()) {
        bool redeclared;
        RootedId id(context, INT_TO_JSID(blockCountBefore));
        if (!StaticBlockObject::addVar(context, blockObj, id, blockCountBefore, &redeclared))
            return false;
        JS_ASSERT(!redeclared);
        JS_ASSERT(blockObj->slotCount() == blockCountBefore + 1);
    }

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkDestructuring(BindData<SyntaxParseHandler> *data,
                                               Node left, bool toplevel)
{
    return abortIfSyntaxParser();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringExpr(BindData<ParseHandler> *data, TokenKind tt)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    pc->inDeclDestructuring = true;
    Node pn = primaryExpr(tt);
    pc->inDeclDestructuring = false;
    if (!pn)
        return null();
    if (!checkDestructuring(data, pn))
        return null();
    return pn;
}

#endif /* JS_HAS_DESTRUCTURING */

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::returnOrYield(bool useAssignExpr)
{
    TokenKind tt = tokenStream.currentToken().type;
    if (!pc->sc->isFunctionBox()) {
        report(ParseError, false, null(), JSMSG_BAD_RETURN_OR_YIELD,
               (tt == TOK_RETURN) ? js_return_str : js_yield_str);
        return null();
    }

    ParseNodeKind kind = (tt == TOK_RETURN) ? PNK_RETURN : PNK_YIELD;
    JSOp op = (tt == TOK_RETURN) ? JSOP_RETURN : JSOP_YIELD;

    Node pn = handler.newUnary(kind, op);
    if (!pn)
        return null();

#if JS_HAS_GENERATORS
    if (tt == TOK_YIELD) {
        if (!abortIfSyntaxParser())
            return null();

        /*
         * If we're within parens, we won't know if this is a generator expression until we see
         * a |for| token, so we have to delay flagging the current function.
         */
        if (pc->parenDepth == 0) {
            pc->sc->asFunctionBox()->setIsGenerator();
        } else {
            pc->yieldCount++;
            pc->yieldOffset = handler.getPosition(pn).begin;
        }
    }
#endif

    /* This is ugly, but we don't want to require a semicolon. */
    TokenKind tt2 = tokenStream.peekTokenSameLine(TSF_OPERAND);
    if (tt2 == TOK_ERROR)
        return null();

    if (tt2 != TOK_EOF && tt2 != TOK_EOL && tt2 != TOK_SEMI && tt2 != TOK_RC
#if JS_HAS_GENERATORS
        && (tt != TOK_YIELD ||
            (tt2 != tt && tt2 != TOK_RB && tt2 != TOK_RP &&
             tt2 != TOK_COLON && tt2 != TOK_COMMA))
#endif
        )
    {
        Node pn2 = useAssignExpr ? assignExpr() : expr();
        if (!pn2)
            return null();
#if JS_HAS_GENERATORS
        if (tt == TOK_RETURN)
#endif
            pc->funHasReturnExpr = true;
        handler.setUnaryKid(pn, pn2);
    } else {
#if JS_HAS_GENERATORS
        if (tt == TOK_RETURN)
#endif
            pc->funHasReturnVoid = true;
    }

    if (pc->funHasReturnExpr && pc->sc->asFunctionBox()->isGenerator()) {
        /* As in Python (see PEP-255), disallow return v; in generators. */
        reportBadReturn(pn, ParseError, JSMSG_BAD_GENERATOR_RETURN,
                        JSMSG_BAD_ANON_GENERATOR_RETURN);
        return null();
    }

    if (context->hasExtraWarningsOption() && pc->funHasReturnExpr && pc->funHasReturnVoid &&
        !reportBadReturn(pn, ParseExtraWarning,
                         JSMSG_NO_RETURN_VALUE, JSMSG_ANON_NO_RETURN_VALUE))
    {
        return null();
    }

    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::pushLexicalScope(HandleStaticBlockObject blockObj, StmtInfoPC *stmt)
{
    JS_ASSERT(blockObj);

    ObjectBox *blockbox = newObjectBox(blockObj);
    if (!blockbox)
        return null();

    PushStatementPC(pc, stmt, STMT_BLOCK);
    blockObj->initPrevBlockChainFromParser(pc->blockChain);
    FinishPushBlockScope(pc, stmt, *blockObj.get());

    Node pn = handler.newLexicalScope(blockbox);
    if (!pn)
        return null();

    if (!GenerateBlockId(pc, stmt->blockid))
        return null();
    handler.setBlockId(pn, stmt->blockid);
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::pushLexicalScope(StmtInfoPC *stmt)
{
    RootedStaticBlockObject blockObj(context, StaticBlockObject::create(context));
    if (!blockObj)
        return null();

    return pushLexicalScope(blockObj, stmt);
}

#if JS_HAS_BLOCK_SCOPE

struct AddLetDecl
{
    uint32_t blockid;

    AddLetDecl(uint32_t blockid) : blockid(blockid) {}

    bool operator()(JSContext *cx, ParseContext<FullParseHandler> *pc,
                    HandleStaticBlockObject blockObj, const Shape &shape, JSAtom *)
    {
        ParseNode *def = (ParseNode *) blockObj->getSlot(shape.slot()).toPrivate();
        def->pn_blockid = blockid;
        RootedPropertyName name(cx, def->name());
        return pc->define(cx, name, def, Definition::LET);
    }
};

template <>
ParseNode *
Parser<FullParseHandler>::pushLetScope(HandleStaticBlockObject blockObj, StmtInfoPC *stmt)
{
    JS_ASSERT(blockObj);
    ParseNode *pn = pushLexicalScope(blockObj, stmt);
    if (!pn)
        return null();

    /* Tell codegen to emit JSOP_ENTERLETx (not JSOP_ENTERBLOCK). */
    pn->pn_dflags |= PND_LET;

    /* Populate the new scope with decls found in the head with updated blockid. */
    if (!ForEachLetDef(context, pc, blockObj, AddLetDecl(stmt->blockid)))
        return null();

    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::pushLetScope(HandleStaticBlockObject blockObj, StmtInfoPC *stmt)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

/*
 * Parse a let block statement or let expression (determined by 'letContext').
 * In both cases, bindings are not hoisted to the top of the enclosing block
 * and thus must be carefully injected between variables() and the let body.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::letBlock(LetContext letContext)
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_LET);

    RootedStaticBlockObject blockObj(context, StaticBlockObject::create(context));
    if (!blockObj)
        return null();

    uint32_t begin = tokenStream.currentToken().pos.begin;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_LET);

    Node vars = variables(PNK_LET, NULL, blockObj, DontHoistVars);
    if (!vars)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_LET);

    StmtInfoPC stmtInfo(context);
    Node block = pushLetScope(blockObj, &stmtInfo);
    if (!block)
        return null();

    Node pnlet = handler.newBinary(PNK_LET, vars, block);
    if (!pnlet)
        return null();
    handler.setBeginPosition(pnlet, begin);

    Node ret;
    if (letContext == LetStatement && !tokenStream.matchToken(TOK_LC, TSF_OPERAND)) {
        /*
         * Strict mode eliminates a grammar ambiguity with unparenthesized
         * LetExpressions in an ExpressionStatement. If followed immediately
         * by an arguments list, it's ambiguous whether the let expression
         * is the callee or the call is inside the let expression body.
         *
         * See bug 569464.
         */
        if (!report(ParseStrictError, pc->sc->strict, pnlet,
                    JSMSG_STRICT_CODE_LET_EXPR_STMT))
        {
            return null();
        }

        /*
         * If this is really an expression in let statement guise, then we
         * need to wrap the TOK_LET node in a TOK_SEMI node so that we pop
         * the return value of the expression.
         */
        Node semi = handler.newUnary(PNK_SEMI, pnlet);

        letContext = LetExpresion;
        ret = semi;
    } else {
        ret = pnlet;
    }

    Node expr;
    if (letContext == LetStatement) {
        expr = statements();
        if (!expr)
            return null();
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_LET);
    } else {
        JS_ASSERT(letContext == LetExpresion);
        expr = assignExpr();
        if (!expr)
            return null();
    }
    handler.setLeaveBlockResult(block, expr, letContext != LetStatement);

    handler.setBeginPosition(ret, vars);
    handler.setEndPosition(ret, expr);

    handler.setBeginPosition(pnlet, vars);
    handler.setEndPosition(pnlet, expr);

    PopStatementPC(context, pc);
    return ret;
}

#endif /* JS_HAS_BLOCK_SCOPE */

template <typename ParseHandler>
static bool
PushBlocklikeStatement(StmtInfoPC *stmt, StmtType type, ParseContext<ParseHandler> *pc)
{
    PushStatementPC(pc, stmt, type);
    return GenerateBlockId(pc, stmt->blockid);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::newBindingNode(PropertyName *name, bool functionScope, VarContext varContext)
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
            JS_ASSERT(handler.getDefinitionKind(lexdep) == Definition::PLACEHOLDER);

            Node pn = handler.getDefinitionNode(lexdep);
            if (handler.dependencyCovered(pn, pc->blockid(), functionScope)) {
                handler.setBlockId(pn, pc->blockid());
                pc->lexdeps->remove(p);
                handler.setPosition(pn, tokenStream.currentToken().pos);
                return pn;
            }
        }
    }

    /* Make a new node for this declarator name (or destructuring pattern). */
    JS_ASSERT(tokenStream.currentToken().type == TOK_NAME);
    return handler.newName(name, pc);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::switchStatement()
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_SWITCH);

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_SWITCH);

    Node discriminant = parenExpr();
    if (!discriminant)
        return null();

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_SWITCH);
    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_SWITCH);

    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_SWITCH);

    if (!GenerateBlockId(pc, pc->topStmt->blockid))
        return null();

    /* The default case has pn_left == NULL */
    Node caseList = handler.newList(PNK_STATEMENTLIST);
    if (!caseList)
        return null();
    handler.setBlockId(caseList, pc->blockid());

    Node saveBlock = pc->blockNode;
    pc->blockNode = caseList;

    bool seenDefault = false;
    TokenKind tt;
    while ((tt = tokenStream.getToken()) != TOK_RC) {
        uint32_t caseBegin = tokenStream.currentToken().pos.begin;

        Node caseExpr;
        switch (tt) {
          case TOK_DEFAULT:
            if (seenDefault) {
                report(ParseError, false, null(), JSMSG_TOO_MANY_DEFAULTS);
                return null();
            }
            seenDefault = true;
            caseExpr = null();
            break;

          case TOK_CASE:
            caseExpr = expr();
            if (!caseExpr)
                return null();
            break;

          case TOK_ERROR:
            return null();

          default:
            report(ParseError, false, null(), JSMSG_BAD_SWITCH);
            return null();
        }

        MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_CASE);

        Node body = handler.newList(PNK_STATEMENTLIST);
        if (!body)
            return null();
        handler.setBlockId(body, pc->blockid());

        while ((tt = tokenStream.peekToken(TSF_OPERAND)) != TOK_RC &&
               tt != TOK_CASE && tt != TOK_DEFAULT) {
            if (tt == TOK_ERROR)
                return null();
            Node stmt = statement();
            if (!stmt)
                return null();
            handler.addList(body, stmt);
        }

        Node casepn = handler.newCaseOrDefault(caseBegin, caseExpr, body);
        if (!casepn)
            return null();
        handler.addList(caseList, casepn);
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

    PopStatementPC(context, pc);

    Node pn = handler.newBinary(PNK_SWITCH, discriminant, caseList);
    if (!pn)
        return null();

    handler.setEndPosition(pn, tokenStream.currentToken().pos.end);
    handler.setEndPosition(caseList, tokenStream.currentToken().pos.end);
    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchInOrOf(bool *isForOfp)
{
    if (tokenStream.matchToken(TOK_IN)) {
        *isForOfp = false;
        return true;
    }
    if (tokenStream.matchToken(TOK_NAME)) {
        if (tokenStream.currentToken().name() == context->names().of) {
            *isForOfp = true;
            return true;
        }
        tokenStream.ungetToken();
    }
    return false;
}

template <>
bool
Parser<FullParseHandler>::isValidForStatementLHS(ParseNode *pn1, JSVersion version,
                                                 bool forDecl, bool forEach, bool forOf)
{
    if (forDecl) {
        if (pn1->pn_count > 1)
            return false;
        if (pn1->isOp(JSOP_DEFCONST))
            return false;
#if JS_HAS_DESTRUCTURING
        // In JS 1.7 only, for (var [K, V] in EXPR) has a special meaning.
        // Hence all other destructuring decls are banned there.
        if (version == JSVERSION_1_7 && !forEach && !forOf) {
            ParseNode *lhs = pn1->pn_head;
            if (lhs->isKind(PNK_ASSIGN))
                lhs = lhs->pn_left;

            if (lhs->isKind(PNK_OBJECT))
                return false;
            if (lhs->isKind(PNK_ARRAY) && lhs->pn_count != 2)
                return false;
        }
#endif
        return true;
    }

    switch (pn1->getKind()) {
      case PNK_NAME:
      case PNK_DOT:
      case PNK_CALL:
      case PNK_ELEM:
        return true;

#if JS_HAS_DESTRUCTURING
      case PNK_ARRAY:
      case PNK_OBJECT:
        // In JS 1.7 only, for ([K, V] in EXPR) has a special meaning.
        // Hence all other destructuring left-hand sides are banned there.
        if (version == JSVERSION_1_7 && !forEach && !forOf)
            return pn1->isKind(PNK_ARRAY) && pn1->pn_count == 2;
        return true;
#endif

      default:
        return false;
    }
}

template <>
ParseNode *
Parser<FullParseHandler>::forStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    StmtInfoPC forStmt(context);
    PushStatementPC(pc, &forStmt, STMT_FOR_LOOP);

    /* A FOR node is binary, left is loop control and right is the body. */
    ParseNode *pn = BinaryNode::create(PNK_FOR, &handler);
    if (!pn)
        return null();

    pn->setOp(JSOP_ITER);
    pn->pn_iflags = 0;

    if (allowsForEachIn() && tokenStream.matchToken(TOK_NAME)) {
        if (tokenStream.currentToken().name() == context->names().each)
            pn->pn_iflags = JSITER_FOREACH;
        else
            tokenStream.ungetToken();
    }

    TokenPos lp_pos = tokenStream.currentToken().pos;
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    /*
     * True if we have 'for (var/let/const ...)', except in the oddball case
     * where 'let' begins a let-expression in 'for (let (...) ...)'.
     */
    bool forDecl = false;

    /* Non-null when forDecl is true for a 'for (let ...)' statement. */
    RootedStaticBlockObject blockObj(context);

    /* Set to 'x' in 'for (x ;... ;...)' or 'for (x in ...)'. */
    ParseNode *pn1;

    {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt == TOK_SEMI) {
            if (pn->pn_iflags & JSITER_FOREACH) {
                report(ParseError, false, null(), JSMSG_BAD_FOR_EACH_LOOP);
                return null();
            }

            pn1 = NULL;
        } else {
            /*
             * Set pn1 to a var list or an initializing expression.
             *
             * Set the parsingForInit flag during parsing of the first clause
             * of the for statement.  This flag will be used by the RelExpr
             * production; if it is set, then the 'in' keyword will not be
             * recognized as an operator, leaving it available to be parsed as
             * part of a for/in loop.
             *
             * A side effect of this restriction is that (unparenthesized)
             * expressions involving an 'in' operator are illegal in the init
             * clause of an ordinary for loop.
             */
            pc->parsingForInit = true;
            if (tt == TOK_VAR || tt == TOK_CONST) {
                forDecl = true;
                tokenStream.consumeKnownToken(tt);
                pn1 = variables(tt == TOK_VAR ? PNK_VAR : PNK_CONST);
            }
#if JS_HAS_BLOCK_SCOPE
            else if (tt == TOK_LET) {
                handler.disableSyntaxParser();
                (void) tokenStream.getToken();
                if (tokenStream.peekToken() == TOK_LP) {
                    pn1 = letBlock(LetExpresion);
                } else {
                    forDecl = true;
                    blockObj = StaticBlockObject::create(context);
                    if (!blockObj)
                        return null();
                    pn1 = variables(PNK_LET, NULL, blockObj, DontHoistVars);
                }
            }
#endif
            else {
                pn1 = expr();
            }
            pc->parsingForInit = false;
            if (!pn1)
                return null();
        }
    }

    JS_ASSERT_IF(forDecl, pn1->isArity(PN_LIST));
    JS_ASSERT(!!blockObj == (forDecl && pn1->isOp(JSOP_NOP)));

    const TokenPos pos = tokenStream.currentToken().pos;

    /* If non-null, the parent that should be returned instead of forHead. */
    ParseNode *forParent = NULL;

    /*
     * We can be sure that it's a for/in loop if there's still an 'in'
     * keyword here, even if JavaScript recognizes 'in' as an operator,
     * as we've excluded 'in' from being parsed in RelExpr by setting
     * pc->parsingForInit.
     */
    ParseNode *forHead;        /* initialized by both branches. */
    StmtInfoPC letStmt(context); /* used if blockObj != NULL. */
    ParseNode *pn2, *pn3;      /* forHead->pn_kid1 and pn_kid2. */
    bool forOf;
    if (pn1 && matchInOrOf(&forOf)) {
        /*
         * Parse the rest of the for/in or for/of head.
         *
         * Here pn1 is everything to the left of 'in' or 'of'. At the end of
         * this block, pn1 is a decl or NULL, pn2 is the assignment target that
         * receives the enumeration value each iteration, and pn3 is the rhs of
         * 'in'.
         */
        forStmt.type = STMT_FOR_IN_LOOP;

        /* Set pn_iflags and rule out invalid combinations. */
        if (forOf && pn->pn_iflags != 0) {
            JS_ASSERT(pn->pn_iflags == JSITER_FOREACH);
            report(ParseError, false, null(), JSMSG_BAD_FOR_EACH_LOOP);
            return null();
        }
        pn->pn_iflags |= (forOf ? JSITER_FOR_OF : JSITER_ENUMERATE);

        /* Check that the left side of the 'in' or 'of' is valid. */
        bool forEach = bool(pn->pn_iflags & JSITER_FOREACH);
        if (!isValidForStatementLHS(pn1, versionNumber(), forDecl, forEach, forOf)) {
            report(ParseError, false, pn1, JSMSG_BAD_FOR_LEFTSIDE);
            return null();
        }

        /*
         * After the following if-else, pn2 will point to the name or
         * destructuring pattern on in's left. pn1 will point to the decl, if
         * any, else NULL. Note that the "declaration with initializer" case
         * rewrites the loop-head, moving the decl and setting pn1 to NULL.
         */
        if (forDecl) {
            pn2 = pn1->pn_head;
            if ((pn2->isKind(PNK_NAME) && pn2->maybeExpr())
#if JS_HAS_DESTRUCTURING
                || pn2->isKind(PNK_ASSIGN)
#endif
                )
            {
                /*
                 * Declaration with initializer.
                 *
                 * Rewrite 'for (<decl> x = i in o)' where <decl> is 'var' or
                 * 'const' to hoist the initializer or the entire decl out of
                 * the loop head.
                 */
#if JS_HAS_BLOCK_SCOPE
                if (blockObj) {
                    report(ParseError, false, pn2, JSMSG_INVALID_FOR_IN_INIT);
                    return null();
                }
#endif /* JS_HAS_BLOCK_SCOPE */

                ParseNode *pnseq = handler.newList(PNK_SEQ, pn1);
                if (!pnseq)
                    return null();

                /*
                 * All of 'var x = i' is hoisted above 'for (x in o)'.
                 *
                 * Request JSOP_POP here since the var is for a simple
                 * name (it is not a destructuring binding's left-hand
                 * side) and it has an initializer.
                 */
                pn1->pn_xflags |= PNX_POPVAR;
                pn1 = NULL;

#if JS_HAS_DESTRUCTURING
                if (pn2->isKind(PNK_ASSIGN)) {
                    pn2 = pn2->pn_left;
                    JS_ASSERT(pn2->isKind(PNK_ARRAY) || pn2->isKind(PNK_OBJECT) ||
                              pn2->isKind(PNK_NAME));
                }
#endif
                pnseq->pn_pos.begin = pn->pn_pos.begin;
                pnseq->append(pn);
                forParent = pnseq;
            }
        } else {
            /* Not a declaration. */
            JS_ASSERT(!blockObj);
            pn2 = pn1;
            pn1 = NULL;

            if (!setAssignmentLhsOps(pn2, JSOP_NOP))
                return null();
        }

        pn3 = expr();
        if (!pn3)
            return null();

        if (blockObj) {
            /*
             * Now that the pn3 has been parsed, push the let scope. To hold
             * the blockObj for the emitter, wrap the TOK_LEXICALSCOPE node
             * created by PushLetScope around the for's initializer. This also
             * serves to indicate the let-decl to the emitter.
             */
            ParseNode *block = pushLetScope(blockObj, &letStmt);
            if (!block)
                return null();
            letStmt.isForLetBlock = true;
            block->pn_expr = pn1;
            block->pn_pos = pn1->pn_pos;
            pn1 = block;
        }

        if (forDecl) {
            /*
             * pn2 is part of a declaration. Make a copy that can be passed to
             * EmitAssignment. Take care to do this after PushLetScope.
             */
            pn2 = cloneLeftHandSide(pn2);
            if (!pn2)
                return null();
        }

        switch (pn2->getKind()) {
          case PNK_NAME:
            /* Beware 'for (arguments in ...)' with or without a 'var'. */
            pn2->markAsAssigned();
            break;

#if JS_HAS_DESTRUCTURING
          case PNK_ASSIGN:
            JS_NOT_REACHED("forStatement TOK_ASSIGN");
            break;

          case PNK_ARRAY:
          case PNK_OBJECT:
            if (versionNumber() == JSVERSION_1_7) {
                /*
                 * Destructuring for-in requires [key, value] enumeration
                 * in JS1.7.
                 */
                JS_ASSERT(pn->isOp(JSOP_ITER));
                if (!(pn->pn_iflags & JSITER_FOREACH) && !forOf)
                    pn->pn_iflags |= JSITER_FOREACH | JSITER_KEYVALUE;
            }
            break;
#endif

          default:;
        }

        forHead = TernaryNode::create(PNK_FORIN, &handler);
        if (!forHead)
            return null();
    } else {
        if (blockObj) {
            /*
             * Desugar 'for (let A; B; C) D' into 'let (A) { for (; B; C) D }'
             * to induce the correct scoping for A.
             */
            ParseNode *block = pushLetScope(blockObj, &letStmt);
            if (!block)
                return null();
            letStmt.isForLetBlock = true;

            ParseNode *let = handler.newBinary(PNK_LET, pn1, block);
            if (!let)
                return null();

            pn1 = NULL;
            block->pn_expr = pn;
            forParent = let;
        }

        if (pn->pn_iflags & JSITER_FOREACH) {
            report(ParseError, false, pn, JSMSG_BAD_FOR_EACH_LOOP);
            return null();
        }
        pn->setOp(JSOP_NOP);

        /* Parse the loop condition or null into pn2. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
        if (tokenStream.peekToken(TSF_OPERAND) == TOK_SEMI) {
            pn2 = NULL;
        } else {
            pn2 = expr();
            if (!pn2)
                return null();
        }

        /* Parse the update expression or null into pn3. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
        if (tokenStream.peekToken(TSF_OPERAND) == TOK_RP) {
            pn3 = NULL;
        } else {
            pn3 = expr();
            if (!pn3)
                return null();
        }

        forHead = TernaryNode::create(PNK_FORHEAD, &handler);
        if (!forHead)
            return null();
    }

    forHead->pn_pos = pos;
    forHead->setOp(JSOP_NOP);
    forHead->pn_kid1 = pn1;
    forHead->pn_kid2 = pn2;
    forHead->pn_kid3 = pn3;
    forHead->pn_pos.begin = lp_pos.begin;
    forHead->pn_pos.end   = tokenStream.currentToken().pos.end;
    pn->pn_left = forHead;

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

    /* Parse the loop body. */
    ParseNode *body = statement();
    if (!body)
        return null();

    /* Record the absolute line number for source note emission. */
    pn->pn_pos.end = body->pn_pos.end;
    pn->pn_right = body;

    if (forParent) {
        forParent->pn_pos.begin = pn->pn_pos.begin;
        forParent->pn_pos.end = pn->pn_pos.end;
    }

#if JS_HAS_BLOCK_SCOPE
    if (blockObj)
        PopStatementPC(context, pc);
#endif
    PopStatementPC(context, pc);
    return forParent ? forParent : pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::forStatement()
{
    /*
     * 'for' statement parsing is fantastically complicated and requires being
     * able to inspect the parse tree for previous parts of the 'for'. Syntax
     * parsing of 'for' statements is thus done separately, and only handles
     * the types of 'for' statements likely to be seen in web content.
     */
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    StmtInfoPC forStmt(context);
    PushStatementPC(pc, &forStmt, STMT_FOR_LOOP);

    /* Don't parse 'for each' loops. */
    if (allowsForEachIn() && tokenStream.peekToken() == TOK_NAME) {
        JS_ALWAYS_FALSE(abortIfSyntaxParser());
        return null();
    }

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

    /* True if we have 'for (var ...)'. */
    bool forDecl = false;
    bool simpleForDecl = true;

    /* Set to 'x' in 'for (x ;... ;...)' or 'for (x in ...)'. */
    Node lhsNode;

    {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt == TOK_SEMI) {
            lhsNode = null();
        } else {
            /* Set lhsNode to a var list or an initializing expression. */
            pc->parsingForInit = true;
            if (tt == TOK_VAR) {
                forDecl = true;
                tokenStream.consumeKnownToken(tt);
                lhsNode = variables(tt == TOK_VAR ? PNK_VAR : PNK_CONST, &simpleForDecl);
            }
#if JS_HAS_BLOCK_SCOPE
            else if (tt == TOK_CONST || tt == TOK_LET) {
                JS_ALWAYS_FALSE(abortIfSyntaxParser());
                return null();
            }
#endif
            else {
                lhsNode = expr();
            }
            if (!lhsNode)
                return null();
            pc->parsingForInit = false;
        }
    }

    /*
     * We can be sure that it's a for/in loop if there's still an 'in'
     * keyword here, even if JavaScript recognizes 'in' as an operator,
     * as we've excluded 'in' from being parsed in RelExpr by setting
     * pc->parsingForInit.
     */
    bool forOf;
    if (lhsNode && matchInOrOf(&forOf)) {
        /* Parse the rest of the for/in or for/of head. */
        forStmt.type = STMT_FOR_IN_LOOP;

        /* Check that the left side of the 'in' or 'of' is valid. */
        if (!forDecl &&
            lhsNode != SyntaxParseHandler::NodeName &&
            lhsNode != SyntaxParseHandler::NodeGetProp &&
            lhsNode != SyntaxParseHandler::NodeLValue)
        {
            JS_ALWAYS_FALSE(abortIfSyntaxParser());
            return null();
        }

        if (!simpleForDecl) {
            JS_ALWAYS_FALSE(abortIfSyntaxParser());
            return null();
        }

        if (!forDecl && !setAssignmentLhsOps(lhsNode, JSOP_NOP))
            return null();

        if (!expr())
            return null();
    } else {
        /* Parse the loop condition or null. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
        if (tokenStream.peekToken(TSF_OPERAND) != TOK_SEMI) {
            if (!expr())
                return null();
        }

        /* Parse the update expression or null. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
        if (tokenStream.peekToken(TSF_OPERAND) != TOK_RP) {
            if (!expr())
                return null();
        }
    }

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

    /* Parse the loop body. */
    if (!statement())
        return null();

    PopStatementPC(context, pc);
    return SyntaxParseHandler::NodeGeneric;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::tryStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_TRY));
    uint32_t begin = tokenStream.currentToken().pos.begin;

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

    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_TRY);
    StmtInfoPC stmtInfo(context);
    if (!PushBlocklikeStatement(&stmtInfo, STMT_TRY, pc))
        return null();
    Node innerBlock = statements();
    if (!innerBlock)
        return null();
    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_TRY);
    PopStatementPC(context, pc);

    bool hasUnconditionalCatch = false;
    Node catchList = null();
    TokenKind tt = tokenStream.getToken();
    if (tt == TOK_CATCH) {
        catchList = handler.newList(PNK_CATCH);
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
            pnblock = pushLexicalScope(&stmtInfo);
            if (!pnblock)
                return null();
            stmtInfo.type = STMT_CATCH;

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
            data.initLet(HoistVars, *pc->blockChain, JSMSG_TOO_MANY_CATCH_VARS);
            JS_ASSERT(data.let.blockObj);

            tt = tokenStream.getToken();
            Node catchName;
            switch (tt) {
#if JS_HAS_DESTRUCTURING
              case TOK_LB:
              case TOK_LC:
                catchName = destructuringExpr(&data, tt);
                if (!catchName)
                    return null();
                break;
#endif

              case TOK_NAME:
              {
                RootedPropertyName label(context, tokenStream.currentToken().name());
                catchName = newBindingNode(label, false);
                if (!catchName)
                    return null();
                data.pn = catchName;
                if (!data.binder(context, &data, label, this))
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
            if (tokenStream.matchToken(TOK_IF)) {
                catchGuard = expr();
                if (!catchGuard)
                    return null();
            }
#endif
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_CATCH);

            MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CATCH);
            Node catchBody = statements();
            if (!catchBody)
                return null();
            MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_CATCH);
            PopStatementPC(context, pc);

            if (!catchGuard)
                hasUnconditionalCatch = true;

            if (!handler.addCatchBlock(catchList, pnblock, catchName, catchGuard, catchBody))
                return null();
            handler.setEndPosition(catchList, tokenStream.currentToken().pos.end);
            handler.setEndPosition(pnblock, tokenStream.currentToken().pos.end);

            tt = tokenStream.getToken(TSF_OPERAND);
        } while (tt == TOK_CATCH);
    }

    Node finallyBlock = null();

    if (tt == TOK_FINALLY) {
        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY);
        if (!PushBlocklikeStatement(&stmtInfo, STMT_FINALLY, pc))
            return null();
        finallyBlock = statements();
        if (!finallyBlock)
            return null();
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_FINALLY);
        PopStatementPC(context, pc);
    } else {
        tokenStream.ungetToken();
    }
    if (!catchList && !finallyBlock) {
        report(ParseError, false, null(), JSMSG_CATCH_OR_FINALLY);
        return null();
    }

    Node pn = handler.newTernary(PNK_TRY, innerBlock, catchList, finallyBlock);
    if (!pn)
        return null();

    handler.setBeginPosition(pn, begin);
    handler.setEndPosition(pn, finallyBlock ? finallyBlock : catchList);
    return pn;
}

template <>
ParseNode *
Parser<FullParseHandler>::withStatement()
{
    if (handler.syntaxParser) {
        handler.disableSyntaxParser();
        abortedSyntaxParse = true;
        return null();
    }

    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_WITH));
    uint32_t begin = tokenStream.currentToken().pos.begin;

    // In most cases, we want the constructs forbidden in strict mode code to be
    // a subset of those that JSOPTION_EXTRA_WARNINGS warns about, and we should
    // use reportStrictModeError.  However, 'with' is the sole instance of a
    // construct that is forbidden in strict mode code, but doesn't even merit a
    // warning under JSOPTION_EXTRA_WARNINGS.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=514576#c1.
    if (pc->sc->strict && !report(ParseStrictError, true, null(), JSMSG_STRICT_CODE_WITH))
        return null();

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_WITH);
    Node objectExpr = parenExpr();
    if (!objectExpr)
        return null();
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_WITH);

    bool oldParsingWith = pc->parsingWith;
    pc->parsingWith = true;

    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_WITH);
    Node innerBlock = statement();
    if (!innerBlock)
        return null();
    PopStatementPC(context, pc);

    pc->sc->setBindingsAccessedDynamically();
    pc->parsingWith = oldParsingWith;

    /*
     * Make sure to deoptimize lexical dependencies inside the |with|
     * to safely optimize binding globals (see bug 561923).
     */
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        DefinitionNode defn = r.front().value().get<FullParseHandler>();
        DefinitionNode lexdep = handler.resolve(defn);
        handler.deoptimizeUsesWithin(lexdep,
                                     TokenPos::make(begin, tokenStream.currentToken().pos.begin));
    }

    Node pn = handler.newBinary(PNK_WITH, objectExpr, innerBlock);
    if (!pn)
        return null();

    handler.setBeginPosition(pn, begin);
    handler.setEndPosition(pn, innerBlock);
    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::withStatement()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return null();
}

#if JS_HAS_BLOCK_SCOPE
template <>
ParseNode *
Parser<FullParseHandler>::letStatement()
{
    handler.disableSyntaxParser();

    ParseNode *pn;
    do {
        /* Check for a let statement or let expression. */
        if (tokenStream.peekToken() == TOK_LP) {
            pn = letBlock(LetStatement);
            if (!pn)
                return null();

            JS_ASSERT(pn->isKind(PNK_LET) || pn->isKind(PNK_SEMI));
            if (pn->isKind(PNK_LET) && pn->pn_expr->getOp() == JSOP_LEAVEBLOCK)
                return pn;

            /* Let expressions require automatic semicolon insertion. */
            JS_ASSERT(pn->isKind(PNK_SEMI) || pn->isOp(JSOP_NOP));
            break;
        }

        /*
         * This is a let declaration. We must be directly under a block per the
         * proposed ES4 specs, but not an implicit block created due to
         * 'for (let ...)'. If we pass this error test, make the enclosing
         * StmtInfoPC be our scope. Further let declarations in this block will
         * find this scope statement and use the same block object.
         *
         * If we are the first let declaration in this block (i.e., when the
         * enclosing maybe-scope StmtInfoPC isn't yet a scope statement) then
         * we also need to set pc->blockNode to be our TOK_LEXICALSCOPE.
         */
        StmtInfoPC *stmt = pc->topStmt;
        if (stmt && (!stmt->maybeScope() || stmt->isForLetBlock)) {
            report(ParseError, false, null(), JSMSG_LET_DECL_NOT_IN_BLOCK);
            return null();
        }

        if (stmt && stmt->isBlockScope) {
            JS_ASSERT(pc->blockChain == stmt->blockObj);
        } else {
            if (pc->atBodyLevel()) {
                /*
                 * ES4 specifies that let at top level and at body-block scope
                 * does not shadow var, so convert back to var.
                 */
                pn = variables(PNK_VAR);
                if (!pn)
                    return null();
                pn->pn_xflags |= PNX_POPVAR;
                break;
            }

            /*
             * Some obvious assertions here, but they may help clarify the
             * situation. This stmt is not yet a scope, so it must not be a
             * catch block (catch is a lexical scope by definition).
             */
            JS_ASSERT(!stmt->isBlockScope);
            JS_ASSERT(stmt != pc->topScopeStmt);
            JS_ASSERT(stmt->type == STMT_BLOCK ||
                      stmt->type == STMT_SWITCH ||
                      stmt->type == STMT_TRY ||
                      stmt->type == STMT_FINALLY);
            JS_ASSERT(!stmt->downScope);

            /* Convert the block statement into a scope statement. */
            StaticBlockObject *blockObj = StaticBlockObject::create(context);
            if (!blockObj)
                return null();

            ObjectBox *blockbox = newObjectBox(blockObj);
            if (!blockbox)
                return null();

            /*
             * Insert stmt on the pc->topScopeStmt/stmtInfo.downScope linked
             * list stack, if it isn't already there.  If it is there, but it
             * lacks the SIF_SCOPE flag, it must be a try, catch, or finally
             * block.
             */
            stmt->isBlockScope = true;
            stmt->downScope = pc->topScopeStmt;
            pc->topScopeStmt = stmt;

            blockObj->initPrevBlockChainFromParser(pc->blockChain);
            pc->blockChain = blockObj;
            stmt->blockObj = blockObj;

#ifdef DEBUG
            ParseNode *tmp = pc->blockNode;
            JS_ASSERT(!tmp || !tmp->isKind(PNK_LEXICALSCOPE));
#endif

            /* Create a new lexical scope node for these statements. */
            ParseNode *pn1 = LexicalScopeNode::create(PNK_LEXICALSCOPE, &handler);
            if (!pn1)
                return null();

            pn1->setOp(JSOP_LEAVEBLOCK);
            pn1->pn_pos = pc->blockNode->pn_pos;
            pn1->pn_objbox = blockbox;
            pn1->pn_expr = pc->blockNode;
            pn1->pn_blockid = pc->blockNode->pn_blockid;
            pc->blockNode = pn1;
        }

        pn = variables(PNK_LET, NULL, pc->blockChain, HoistVars);
        if (!pn)
            return null();
        pn->pn_xflags = PNX_POPVAR;
    } while (0);

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : NULL;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::letStatement()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

#endif // JS_HAS_BLOCK_SCOPE

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::labeledStatement()
{
    uint32_t begin = tokenStream.currentToken().pos.begin;
    RootedPropertyName label(context, tokenStream.currentToken().name());
    for (StmtInfoPC *stmt = pc->topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == STMT_LABEL && stmt->label == label) {
            report(ParseError, false, null(), JSMSG_DUPLICATE_LABEL);
            return null();
        }
    }

    tokenStream.consumeKnownToken(TOK_COLON);

    /* Push a label struct and parse the statement. */
    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_LABEL);
    stmtInfo.label = label;
    Node pn = statement();
    if (!pn)
        return null();

    /* Pop the label, set pn_expr, and return early. */
    PopStatementPC(context, pc);

    return handler.newLabeledStatement(label, pn, begin);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expressionStatement()
{
    tokenStream.ungetToken();
    Node pn2 = expr();
    if (!pn2)
        return null();

    Node pn = handler.newUnary(PNK_SEMI, pn2);

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : null();
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statement()
{
    Node pn;

    JS_CHECK_RECURSION(context, return null());

    switch (tokenStream.getToken(TSF_OPERAND)) {
      case TOK_FUNCTION:
        return functionStmt();

      case TOK_IF:
      {
        uint32_t begin = tokenStream.currentToken().pos.begin;

        /* An IF node has three kids: condition, then, and optional else. */
        Node cond = condition();
        if (!cond)
            return null();

        if (tokenStream.peekToken(TSF_OPERAND) == TOK_SEMI &&
            !report(ParseExtraWarning, false, null(), JSMSG_EMPTY_CONSEQUENT))
        {
            return null();
        }

        StmtInfoPC stmtInfo(context);
        PushStatementPC(pc, &stmtInfo, STMT_IF);
        Node thenBranch = statement();
        if (!thenBranch)
            return null();

        Node elseBranch;
        if (tokenStream.matchToken(TOK_ELSE, TSF_OPERAND)) {
            stmtInfo.type = STMT_ELSE;
            elseBranch = statement();
            if (!elseBranch)
                return null();
        } else {
            elseBranch = null();
        }

        PopStatementPC(context, pc);
        pn = handler.newTernary(PNK_IF, cond, thenBranch, elseBranch);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
        return pn;
      }

      case TOK_SWITCH:
        return switchStatement();

      case TOK_WHILE:
      {
        uint32_t begin = tokenStream.currentToken().pos.begin;
        StmtInfoPC stmtInfo(context);
        PushStatementPC(pc, &stmtInfo, STMT_WHILE_LOOP);
        Node cond = condition();
        if (!cond)
            return null();
        Node body = statement();
        if (!body)
            return null();
        PopStatementPC(context, pc);
        pn = handler.newBinary(PNK_WHILE, cond, body);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
        return pn;
      }

      case TOK_DO:
      {
        uint32_t begin = tokenStream.currentToken().pos.begin;
        StmtInfoPC stmtInfo(context);
        PushStatementPC(pc, &stmtInfo, STMT_DO_LOOP);
        Node body = statement();
        if (!body)
            return null();
        MUST_MATCH_TOKEN(TOK_WHILE, JSMSG_WHILE_AFTER_DO);
        Node cond = condition();
        if (!cond)
            return null();
        PopStatementPC(context, pc);

        pn = handler.newBinary(PNK_DOWHILE, body, cond);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);

        if (versionNumber() != JSVERSION_ECMA_3) {
            /*
             * All legacy and extended versions must do automatic semicolon
             * insertion after do-while.  See the testcase and discussion in
             * http://bugzilla.mozilla.org/show_bug.cgi?id=238945.
             */
            (void) tokenStream.matchToken(TOK_SEMI);
            return pn;
        }
        break;
      }

      case TOK_FOR:
        return forStatement();

      case TOK_TRY:
        return tryStatement();

      case TOK_THROW:
      {
        uint32_t begin = tokenStream.currentToken().pos.begin;

        /* ECMA-262 Edition 3 says 'throw [no LineTerminator here] Expr'. */
        TokenKind tt = tokenStream.peekTokenSameLine(TSF_OPERAND);
        if (tt == TOK_ERROR)
            return null();
        if (tt == TOK_EOF || tt == TOK_EOL || tt == TOK_SEMI || tt == TOK_RC) {
            report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
            return null();
        }

        Node pnexp = expr();
        if (!pnexp)
            return null();

        pn = handler.newUnary(PNK_THROW, pnexp, JSOP_THROW);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
        break;
      }

      /* TOK_CATCH and TOK_FINALLY are both handled in the TOK_TRY case */
      case TOK_CATCH:
        report(ParseError, false, null(), JSMSG_CATCH_WITHOUT_TRY);
        return null();

      case TOK_FINALLY:
        report(ParseError, false, null(), JSMSG_FINALLY_WITHOUT_TRY);
        return null();

      case TOK_BREAK:
      {
        uint32_t begin = tokenStream.currentToken().pos.begin;
        RootedPropertyName label(context);
        if (!MatchLabel(context, &tokenStream, &label))
            return null();
        uint32_t end = tokenStream.currentToken().pos.end;
        pn = handler.newBreak(label, begin, end);
        if (!pn)
            return null();
        StmtInfoPC *stmt = pc->topStmt;
        if (label) {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    report(ParseError, false, null(), JSMSG_LABEL_NOT_FOUND);
                    return null();
                }
                if (stmt->type == STMT_LABEL && stmt->label == label)
                    break;
            }
        } else {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    report(ParseError, false, null(), JSMSG_TOUGH_BREAK);
                    return null();
                }
                if (stmt->isLoop() || stmt->type == STMT_SWITCH)
                    break;
            }
        }
        break;
      }

      case TOK_CONTINUE:
      {
        uint32_t begin = tokenStream.currentToken().pos.begin;
        RootedPropertyName label(context);
        if (!MatchLabel(context, &tokenStream, &label))
            return null();
        uint32_t end = tokenStream.currentToken().pos.end;
        pn = handler.newContinue(label, begin, end);
        if (!pn)
            return null();
        StmtInfoPC *stmt = pc->topStmt;
        if (label) {
            for (StmtInfoPC *stmt2 = NULL; ; stmt = stmt->down) {
                if (!stmt) {
                    report(ParseError, false, null(), JSMSG_LABEL_NOT_FOUND);
                    return null();
                }
                if (stmt->type == STMT_LABEL) {
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
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    report(ParseError, false, null(), JSMSG_BAD_CONTINUE);
                    return null();
                }
                if (stmt->isLoop())
                    break;
            }
        }
        break;
      }

      case TOK_WITH:
        return withStatement();

      case TOK_VAR:
        pn = variables(PNK_VAR);
        if (!pn)
            return null();

        /* Tell js_EmitTree to generate a final POP. */
        handler.setListFlag(pn, PNX_POPVAR);
        break;

      case TOK_CONST:
        if (!abortIfSyntaxParser())
            return null();

        pn = variables(PNK_CONST);
        if (!pn)
            return null();

        /* Tell js_EmitTree to generate a final POP. */
        handler.setListFlag(pn, PNX_POPVAR);
        break;

#if JS_HAS_BLOCK_SCOPE
      case TOK_LET:
        return letStatement();
#endif /* JS_HAS_BLOCK_SCOPE */

      case TOK_RETURN:
        pn = returnOrYield(false);
        if (!pn)
            return null();
        break;

      case TOK_LC:
      {
        StmtInfoPC stmtInfo(context);
        if (!PushBlocklikeStatement(&stmtInfo, STMT_BLOCK, pc))
            return null();

        pn = statements();
        if (!pn)
            return null();

        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_IN_COMPOUND);
        PopStatementPC(context, pc);
        return pn;
      }

      case TOK_SEMI:
        return handler.newUnary(PNK_SEMI);

      case TOK_DEBUGGER:
        pn = handler.newDebuggerStatement(tokenStream.currentToken().pos);
        if (!pn)
            return null();
        pc->sc->setBindingsAccessedDynamically();
        pc->sc->setHasDebuggerStatement();
        break;

      case TOK_ERROR:
        return null();

      case TOK_NAME: {
        if (tokenStream.peekToken() == TOK_COLON)
            return labeledStatement();
        if (tokenStream.currentToken().name() == context->names().module
            && tokenStream.peekTokenSameLine() == TOK_STRING)
        {
            return moduleDecl();
        }
      }
        /* FALL THROUGH */

      default:
        return expressionStatement();
    }

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : null();
}

/*
 * The 'blockObj' parameter is non-null when parsing the 'vars' in a let
 * expression, block statement, non-top-level let declaration in statement
 * context, and the let-initializer of a for-statement.
 */
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::variables(ParseNodeKind kind, bool *psimple,
                                StaticBlockObject *blockObj, VarContext varContext)
{
    /*
     * The four options here are:
     * - PNK_VAR:   We're parsing var declarations.
     * - PNK_CONST: We're parsing const declarations.
     * - PNK_LET:   We are parsing a let declaration.
     * - PNK_CALL:  We are parsing the head of a let block.
     */
    JS_ASSERT(kind == PNK_VAR || kind == PNK_CONST || kind == PNK_LET || kind == PNK_CALL);

    /*
     * The simple flag is set if the declaration has the form 'var x', with
     * only one variable declared and no initializer expression.
     */
    JS_ASSERT_IF(psimple, *psimple);

    JSOp op = blockObj ? JSOP_NOP : kind == PNK_VAR ? JSOP_DEFVAR : JSOP_DEFCONST;

    Node pn = handler.newList(kind, null(), op);
    if (!pn)
        return null();

    /*
     * SpiderMonkey const is really "write once per initialization evaluation"
     * var, whereas let is block scoped. ES-Harmony wants block-scoped const so
     * this code will change soon.
     */
    BindData<ParseHandler> data(context);
    if (blockObj)
        data.initLet(varContext, *blockObj, JSMSG_TOO_MANY_LOCALS);
    else
        data.initVarOrConst(op);

    bool first = true;
    Node pn2;
    do {
        if (psimple && !first)
            *psimple = false;
        first = false;

        TokenKind tt = tokenStream.getToken();
#if JS_HAS_DESTRUCTURING
        if (tt == TOK_LB || tt == TOK_LC) {
            if (psimple)
                *psimple = false;

            pc->inDeclDestructuring = true;
            pn2 = primaryExpr(tt);
            pc->inDeclDestructuring = false;
            if (!pn2)
                return null();

            if (!checkDestructuring(&data, pn2))
                return null();
            bool ignored;
            if (pc->parsingForInit && matchInOrOf(&ignored)) {
                tokenStream.ungetToken();
                handler.addList(pn, pn2);
                continue;
            }

            MUST_MATCH_TOKEN(TOK_ASSIGN, JSMSG_BAD_DESTRUCT_DECL);
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NOP);

            Node init = assignExpr();
            if (!init)
                return null();

            pn2 = handler.newBinaryOrAppend(PNK_ASSIGN, pn2, init, pc);
            if (!pn2)
                return null();
            handler.addList(pn, pn2);
            continue;
        }
#endif /* JS_HAS_DESTRUCTURING */

        if (tt != TOK_NAME) {
            if (tt != TOK_ERROR)
                report(ParseError, false, null(), JSMSG_NO_VARIABLE_NAME);
            return null();
        }

        RootedPropertyName name(context, tokenStream.currentToken().name());
        pn2 = newBindingNode(name, kind == PNK_VAR || kind == PNK_CONST, varContext);
        if (!pn2)
            return null();
        if (data.op == JSOP_DEFCONST)
            handler.setFlag(pn2, PND_CONST);
        data.pn = pn2;
        if (!data.binder(context, &data, name, this))
            return null();
        handler.addList(pn, pn2);

        if (tokenStream.matchToken(TOK_ASSIGN)) {
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NOP);

            if (psimple)
                *psimple = false;

            Node init = assignExpr();
            if (!init)
                return null();

            if (!handler.finishInitializerAssignment(pn2, init, data.op))
                return null();
        }
    } while (tokenStream.matchToken(TOK_COMMA));

    return pn;
}

template <>
ParseNode *
Parser<FullParseHandler>::expr()
{
    ParseNode *pn = assignExpr();
    if (pn && tokenStream.matchToken(TOK_COMMA)) {
        ParseNode *pn2 = ListNode::create(PNK_COMMA, &handler);
        if (!pn2)
            return null();
        pn2->pn_pos.begin = pn->pn_pos.begin;
        pn2->initList(pn);
        pn = pn2;
        do {
#if JS_HAS_GENERATORS
            pn2 = pn->last();
            if (pn2->isKind(PNK_YIELD) && !pn2->isInParens()) {
                report(ParseError, false, pn2, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
                return null();
            }
#endif
            pn2 = assignExpr();
            if (!pn2)
                return null();
            pn->append(pn2);
        } while (tokenStream.matchToken(TOK_COMMA));
        pn->pn_pos.end = pn->last()->pn_pos.end;
    }
    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::expr()
{
    Node pn = assignExpr();
    if (pn && tokenStream.matchToken(TOK_COMMA)) {
        do {
            if (!assignExpr())
                return null();
        } while (tokenStream.matchToken(TOK_COMMA));
        return SyntaxParseHandler::NodeGeneric;
    }
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
    JSOP_MOD
};

static inline JSOp
BinaryOpParseNodeKindToJSOp(ParseNodeKind pnk)
{
    JS_ASSERT(pnk >= PNK_BINOP_FIRST);
    JS_ASSERT(pnk <= PNK_BINOP_LAST);
    return ParseNodeKindToJSOp[pnk - PNK_BINOP_FIRST];
}

static bool
IsBinaryOpToken(TokenKind tok, bool parsingForInit)
{
    return tok == TOK_IN ? !parsingForInit : TokenKindIsBinaryOp(tok);
}

static ParseNodeKind
BinaryOpTokenKindToParseNodeKind(TokenKind tok)
{
    JS_ASSERT(TokenKindIsBinaryOp(tok));
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
    10  /* PNK_MOD */
};

static const int PRECEDENCE_CLASSES = 10;

static int
Precedence(ParseNodeKind pnk) {
    // Everything binds tighter than PNK_LIMIT, because we want to reduce all
    // nodes to a single node when we reach a token that is not another binary
    // operator.
    if (pnk == PNK_LIMIT)
        return 0;

    JS_ASSERT(pnk >= PNK_BINOP_FIRST);
    JS_ASSERT(pnk <= PNK_BINOP_LAST);
    return PrecedenceTable[pnk - PNK_BINOP_FIRST];
}

template <typename ParseHandler>
JS_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::orExpr1()
{
    // Shift-reduce parser for the left-associative binary operator part of
    // the JS syntax.

    // Conceptually there's just one stack, a stack of pairs (lhs, op).
    // It's implemented using two separate arrays, though.
    Node nodeStack[PRECEDENCE_CLASSES];
    ParseNodeKind kindStack[PRECEDENCE_CLASSES];
    int depth = 0;

    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;

    Node pn;
    for (;;) {
        pn = unaryExpr();
        if (!pn)
            return pn;

        // If a binary operator follows, consume it and compute the
        // corresponding operator.
        TokenKind tok = tokenStream.getToken();
        if (tok == TOK_ERROR)
            return null();
        ParseNodeKind pnk;
        if (IsBinaryOpToken(tok, oldParsingForInit)) {
            pnk = BinaryOpTokenKindToParseNodeKind(tok);
        } else {
            tok = TOK_EOF;
            pnk = PNK_LIMIT;
        }

        // If pnk has precedence less than or equal to another operator on the
        // stack, reduce. This combines nodes on the stack until we form the
        // actual lhs of pnk.
        //
        // The >= in this condition works because all the operators in question
        // are left-associative; if any were not, the case where two operators
        // have equal precedence would need to be handled specially, and the
        // stack would need to be a Vector.
        while (depth > 0 && Precedence(kindStack[depth - 1]) >= Precedence(pnk)) {
            depth--;
            ParseNodeKind combiningPnk = kindStack[depth];
            JSOp combiningOp = BinaryOpParseNodeKindToJSOp(combiningPnk);
            pn = handler.newBinaryOrAppend(combiningPnk, nodeStack[depth], pn, pc, combiningOp);
            if (!pn)
                return pn;
        }

        if (pnk == PNK_LIMIT)
            break;

        nodeStack[depth] = pn;
        kindStack[depth] = pnk;
        depth++;
        JS_ASSERT(depth <= PRECEDENCE_CLASSES);
    }

    JS_ASSERT(depth == 0);
    pc->parsingForInit = oldParsingForInit;
    return pn;
}

template <typename ParseHandler>
JS_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::condExpr1()
{
    Node condition = orExpr1();
    if (!condition || !tokenStream.isCurrentTokenType(TOK_HOOK))
        return condition;

    /*
     * Always accept the 'in' operator in the middle clause of a ternary,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;
    Node thenExpr = assignExpr();
    pc->parsingForInit = oldParsingForInit;
    if (!thenExpr)
        return null();

    MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_IN_COND);

    Node elseExpr = assignExpr();
    if (!elseExpr)
        return null();

    tokenStream.getToken(); /* read one token past the end */
    return handler.newConditional(condition, thenExpr, elseExpr);
}

template <>
bool
Parser<FullParseHandler>::setAssignmentLhsOps(ParseNode *pn, JSOp op)
{
    switch (pn->getKind()) {
      case PNK_NAME:
        if (!checkStrictAssignment(pn))
            return false;
        pn->setOp(pn->isOp(JSOP_GETLOCAL) ? JSOP_SETLOCAL : JSOP_SETNAME);
        pn->markAsAssigned();
        break;
      case PNK_DOT:
        pn->setOp(JSOP_SETPROP);
        break;
      case PNK_ELEM:
        pn->setOp(JSOP_SETELEM);
        break;
#if JS_HAS_DESTRUCTURING
      case PNK_ARRAY:
      case PNK_OBJECT:
        if (op != JSOP_NOP) {
            report(ParseError, false, null(), JSMSG_BAD_DESTRUCT_ASS);
            return false;
        }
        if (!checkDestructuring(NULL, pn))
            return false;
        break;
#endif
      case PNK_CALL:
        if (!makeSetCall(pn, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return false;
        break;
      default:
        report(ParseError, false, null(), JSMSG_BAD_LEFTSIDE_OF_ASS);
        return false;
    }
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::setAssignmentLhsOps(Node pn, JSOp op)
{
    /* Full syntax checking of valid assignment LHS terms requires a parse tree. */
    if (pn != SyntaxParseHandler::NodeName &&
        pn != SyntaxParseHandler::NodeGetProp &&
        pn != SyntaxParseHandler::NodeLValue)
    {
        return abortIfSyntaxParser();
    }
    return checkStrictAssignment(pn);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExpr()
{
    JS_CHECK_RECURSION(context, return null());

#if JS_HAS_GENERATORS
    if (tokenStream.matchToken(TOK_YIELD, TSF_OPERAND))
        return returnOrYield(true);
    if (tokenStream.hadError())
        return null();
#endif

    // Save the tokenizer state in case we find an arrow function and have to
    // rewind.
    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    Node lhs = condExpr1();
    if (!lhs)
        return null();

    ParseNodeKind kind;
    switch (tokenStream.currentToken().type) {
      case TOK_ASSIGN:       kind = PNK_ASSIGN;       break;
      case TOK_ADDASSIGN:    kind = PNK_ADDASSIGN;    break;
      case TOK_SUBASSIGN:    kind = PNK_SUBASSIGN;    break;
      case TOK_BITORASSIGN:  kind = PNK_BITORASSIGN;  break;
      case TOK_BITXORASSIGN: kind = PNK_BITXORASSIGN; break;
      case TOK_BITANDASSIGN: kind = PNK_BITANDASSIGN; break;
      case TOK_LSHASSIGN:    kind = PNK_LSHASSIGN;    break;
      case TOK_RSHASSIGN:    kind = PNK_RSHASSIGN;    break;
      case TOK_URSHASSIGN:   kind = PNK_URSHASSIGN;   break;
      case TOK_MULASSIGN:    kind = PNK_MULASSIGN;    break;
      case TOK_DIVASSIGN:    kind = PNK_DIVASSIGN;    break;
      case TOK_MODASSIGN:    kind = PNK_MODASSIGN;    break;

      case TOK_ARROW: {
        tokenStream.seek(start);
        if (!abortIfSyntaxParser())
            return null();

        if (tokenStream.getToken() == TOK_ERROR)
            return null();
        size_t offset = tokenStream.currentToken().pos.begin;
        tokenStream.ungetToken();

        return functionDef(NullPtr(), start, offset, Normal, Arrow);
      }

      default:
        JS_ASSERT(!tokenStream.isCurrentTokenAssignment());
        tokenStream.ungetToken();
        return lhs;
    }

    JSOp op = tokenStream.currentToken().t_op;
    if (!setAssignmentLhsOps(lhs, op))
        return null();

    Node rhs = assignExpr();
    if (!rhs)
        return null();

    return handler.newBinaryOrAppend(kind, lhs, rhs, pc, op);
}

template <> bool
Parser<FullParseHandler>::setLvalKid(ParseNode *pn, ParseNode *kid, const char *name)
{
    if (!kid->isKind(PNK_NAME) &&
        !kid->isKind(PNK_DOT) &&
        (!kid->isKind(PNK_CALL) ||
         (!kid->isOp(JSOP_CALL) && !kid->isOp(JSOP_EVAL) &&
          !kid->isOp(JSOP_FUNCALL) && !kid->isOp(JSOP_FUNAPPLY))) &&
        !kid->isKind(PNK_ELEM))
    {
        report(ParseError, false, null(), JSMSG_BAD_OPERAND, name);
        return false;
    }
    if (!checkStrictAssignment(kid))
        return false;
    pn->pn_kid = kid;
    return true;
}

static const char incop_name_str[][10] = {"increment", "decrement"};

template <>
bool
Parser<FullParseHandler>::setIncOpKid(ParseNode *pn, ParseNode *kid, TokenKind tt, bool preorder)
{
    if (!setLvalKid(pn, kid, incop_name_str[tt == TOK_DEC]))
        return false;

    switch (kid->getKind()) {
      case PNK_NAME:
        kid->markAsAssigned();
        break;

      case PNK_CALL:
        if (!makeSetCall(kid, JSMSG_BAD_INCOP_OPERAND))
            return false;
        break;

      case PNK_DOT:
      case PNK_ELEM:
        break;

      default:
        JS_ASSERT(0);
    }
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::setIncOpKid(Node pn, Node kid, TokenKind tt, bool preorder)
{
    return setAssignmentLhsOps(kid, JSOP_NOP);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryOpExpr(ParseNodeKind kind, JSOp op)
{
    Node kid = unaryExpr();
    if (!kid)
        return null();
    return handler.newUnary(kind, kid, op);
}

template <>
bool
Parser<FullParseHandler>::checkDeleteExpression(ParseNode **pn_)
{
    ParseNode *&pn = *pn_;

    /*
     * Under ECMA3, deleting any unary expression is valid -- it simply
     * returns true. Here we fold constants before checking for a call
     * expression, in order to rule out delete of a generator expression.
     */
    if (foldConstants && !FoldConstants(context, &pn, this))
        return false;
    switch (pn->getKind()) {
      case PNK_CALL:
        if (!(pn->pn_xflags & PNX_SETCALL)) {
            /*
             * Call MakeSetCall to check for errors, but clear PNX_SETCALL
             * because the optimizer will eliminate the useless delete.
             */
            if (!makeSetCall(pn, JSMSG_BAD_DELETE_OPERAND))
                return false;
            pn->pn_xflags &= ~PNX_SETCALL;
        }
        break;
      case PNK_NAME:
        if (!report(ParseStrictError, pc->sc->strict, pn, JSMSG_DEPRECATED_DELETE_OPERAND))
            return null();
        pc->sc->setBindingsAccessedDynamically();
        pn->pn_dflags |= PND_DEOPTIMIZED;
        pn->setOp(JSOP_DELNAME);
        break;
      default:;
    }
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::checkDeleteExpression(Node *pn)
{
    PropertyName *name = handler.isName(*pn);
    if (name)
        return report(ParseStrictError, pc->sc->strict, *pn, JSMSG_DEPRECATED_DELETE_OPERAND);

    // Treat deletion of non-lvalues as ambiguous, so that any error associated
    // with deleting a call expression is reported.
    if (*pn != SyntaxParseHandler::NodeGetProp &&
        *pn != SyntaxParseHandler::NodeLValue &&
        strictMode())
    {
        return abortIfSyntaxParser();
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryExpr()
{
    Node pn, pn2;

    JS_CHECK_RECURSION(context, return null());

    switch (TokenKind tt = tokenStream.getToken(TSF_OPERAND)) {
      case TOK_TYPEOF:
        return unaryOpExpr(PNK_TYPEOF, JSOP_TYPEOF);
      case TOK_VOID:
        return unaryOpExpr(PNK_VOID, JSOP_VOID);
      case TOK_NOT:
        return unaryOpExpr(PNK_NOT, JSOP_NOT);
      case TOK_BITNOT:
        return unaryOpExpr(PNK_BITNOT, JSOP_BITNOT);
      case TOK_PLUS:
        return unaryOpExpr(PNK_POS, JSOP_POS);
      case TOK_MINUS:
        return unaryOpExpr(PNK_NEG, JSOP_NEG);

      case TOK_INC:
      case TOK_DEC:
      {
        uint32_t begin = tokenStream.currentToken().pos.begin;
        TokenKind tt2 = tokenStream.getToken(TSF_OPERAND);
        pn2 = memberExpr(tt2, true);
        if (!pn2)
            return null();
        pn = handler.newUnary((tt == TOK_INC) ? PNK_PREINCREMENT : PNK_PREDECREMENT, pn2);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
        if (!setIncOpKid(pn, pn2, tt, true))
            return null();
        break;
      }

      case TOK_DELETE:
      {
        uint32_t begin = tokenStream.currentToken().pos.begin;
        pn2 = unaryExpr();
        if (!pn2)
            return null();

        if (!checkDeleteExpression(&pn2))
            return null();

        pn = handler.newUnary(PNK_DELETE, pn2);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
        break;
      }
      case TOK_ERROR:
        return null();

      default:
        pn = memberExpr(tt, true);
        if (!pn)
            return null();

        /* Don't look across a newline boundary for a postfix incop. */
        tt = tokenStream.peekTokenSameLine(TSF_OPERAND);
        if (tt == TOK_INC || tt == TOK_DEC) {
            tokenStream.consumeKnownToken(tt);
            pn2 = handler.newUnary((tt == TOK_INC) ? PNK_POSTINCREMENT : PNK_POSTDECREMENT, pn);
            if (!pn2)
                return null();
            if (!setIncOpKid(pn2, pn, tt, false))
                return null();
            pn = pn2;
        }
        break;
    }
    return pn;
}

#if JS_HAS_GENERATORS

/*
 * A dedicated helper for transplanting the comprehension expression E in
 *
 *   [E for (V in I)]   // array comprehension
 *   (E for (V in I))   // generator expression
 *
 * from its initial location in the AST, on the left of the 'for', to its final
 * position on the right. To avoid a separate pass we do this by adjusting the
 * blockids and name binding links that were established when E was parsed.
 *
 * A generator expression desugars like so:
 *
 *   (E for (V in I)) => (function () { for (var V in I) yield E; })()
 *
 * so the transplanter must adjust static level as well as blockid. E's source
 * coordinates in root->pn_pos are critical to deciding which binding links to
 * preserve and which to cut.
 *
 * NB: This is not a general tree transplanter -- it knows in particular that
 * the one or more bindings induced by V have not yet been created.
 */
class CompExprTransplanter
{
    ParseNode       *root;
    Parser<FullParseHandler> *parser;
    ParseContext<FullParseHandler> *outerpc;
    bool            genexp;
    unsigned        adjust;
    HashSet<Definition *> visitedImplicitArguments;

  public:
    CompExprTransplanter(ParseNode *pn, Parser<FullParseHandler> *parser,
                         ParseContext<FullParseHandler> *outerpc,
                         bool ge, unsigned adj)
      : root(pn), parser(parser), outerpc(outerpc), genexp(ge), adjust(adj),
        visitedImplicitArguments(parser->context)
    {}

    bool init() {
        return visitedImplicitArguments.init();
    }

    bool transplant(ParseNode *pn);
};

/*
 * A helper for lazily checking for the presence of illegal |yield| or |arguments|
 * tokens inside of generator expressions. This must be done lazily since we don't
 * know whether we're in a generator expression until we see the "for" token after
 * we've already parsed the body expression.
 *
 * Use in any context which may turn out to be inside a generator expression. This
 * includes parenthesized expressions and argument lists, and it includes the tail
 * of generator expressions.
 *
 * The guard will keep track of any |yield| or |arguments| tokens that occur while
 * parsing the body. As soon as the parser reaches the end of the body expression,
 * call endBody() to reset the context's state, and then immediately call:
 *
 * - checkValidBody() if this *did* turn out to be a generator expression
 * - maybeNoteGenerator() if this *did not* turn out to be a generator expression
 */
template <typename ParseHandler>
class GenexpGuard
{
    Parser<ParseHandler> *parser;
    uint32_t startYieldCount;

    typedef typename ParseHandler::Node Node;

  public:
    explicit GenexpGuard(Parser<ParseHandler> *parser)
      : parser(parser)
    {
        ParseContext<ParseHandler> *pc = parser->pc;
        if (pc->parenDepth == 0) {
            pc->yieldCount = 0;
            pc->yieldOffset = 0;
        }
        startYieldCount = pc->yieldCount;
        pc->parenDepth++;
    }

    void endBody();
    bool checkValidBody(Node pn, unsigned err = JSMSG_BAD_GENEXP_BODY);
    bool maybeNoteGenerator(Node pn);
};

template <typename ParseHandler>
void
GenexpGuard<ParseHandler>::endBody()
{
    parser->pc->parenDepth--;
}

/*
 * Check whether a |yield| or |arguments| token has been encountered in the
 * body expression, and if so, report an error.
 *
 * Call this after endBody() when determining that the body *was* in a
 * generator expression.
 */
template <typename ParseHandler>
bool
GenexpGuard<ParseHandler>::checkValidBody(Node pn, unsigned err)
{
    ParseContext<ParseHandler> *pc = parser->pc;
    if (pc->yieldCount > startYieldCount) {
        uint32_t offset = pc->yieldOffset
                          ? pc->yieldOffset
                          : (pn ? parser->handler.getPosition(pn)
                                : parser->tokenStream.currentToken().pos).begin;

        parser->reportWithOffset(ParseError, false, offset, err, js_yield_str);
        return false;
    }

    return true;
}

/*
 * Check whether a |yield| token has been encountered in the body expression,
 * and if so, note that the current function is a generator function.
 *
 * Call this after endBody() when determining that the body *was not* in a
 * generator expression.
 */
template <typename ParseHandler>
bool
GenexpGuard<ParseHandler>::maybeNoteGenerator(Node pn)
{
    ParseContext<ParseHandler> *pc = parser->pc;
    if (pc->yieldCount > 0) {
        if (!pc->sc->isFunctionBox()) {
            parser->report(ParseError, false, ParseHandler::null(),
                           JSMSG_BAD_RETURN_OR_YIELD, js_yield_str);
            return false;
        }
        pc->sc->asFunctionBox()->setIsGenerator();
        if (pc->funHasReturnExpr) {
            /* At the time we saw the yield, we might not have set isGenerator yet. */
            parser->reportBadReturn(pn, ParseError,
                                    JSMSG_BAD_GENERATOR_RETURN, JSMSG_BAD_ANON_GENERATOR_RETURN);
            return false;
        }
    }
    return true;
}

/*
 * Any definitions nested within the comprehension expression of a generator
 * expression must move "down" one static level, which of course increases the
 * upvar-frame-skip count.
 */
template <typename ParseHandler>
static bool
BumpStaticLevel(ParseNode *pn, ParseContext<ParseHandler> *pc)
{
    if (pn->pn_cookie.isFree())
        return true;

    unsigned level = unsigned(pn->pn_cookie.level()) + 1;
    JS_ASSERT(level >= pc->staticLevel);
    return pn->pn_cookie.set(pc->sc->context, level, pn->pn_cookie.slot());
}

template <typename ParseHandler>
static bool
AdjustBlockId(ParseNode *pn, unsigned adjust, ParseContext<ParseHandler> *pc)
{
    JS_ASSERT(pn->isArity(PN_LIST) || pn->isArity(PN_CODE) || pn->isArity(PN_NAME));
    if (JS_BIT(20) - pn->pn_blockid <= adjust + 1) {
        JS_ReportErrorNumber(pc->sc->context, js_GetErrorMessage, NULL, JSMSG_NEED_DIET, "program");
        return false;
    }
    pn->pn_blockid += adjust;
    if (pn->pn_blockid >= pc->blockidGen)
        pc->blockidGen = pn->pn_blockid + 1;
    return true;
}

bool
CompExprTransplanter::transplant(ParseNode *pn)
{
    ParseContext<FullParseHandler> *pc = parser->pc;

    if (!pn)
        return true;

    switch (pn->getArity()) {
      case PN_LIST:
        for (ParseNode *pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (!transplant(pn2))
                return false;
        }
        if (pn->pn_pos >= root->pn_pos) {
            if (!AdjustBlockId(pn, adjust, pc))
                return false;
        }
        break;

      case PN_TERNARY:
        if (!transplant(pn->pn_kid1) ||
            !transplant(pn->pn_kid2) ||
            !transplant(pn->pn_kid3))
            return false;
        break;

      case PN_BINARY:
        if (!transplant(pn->pn_left))
            return false;

        /* Binary TOK_COLON nodes can have left == right. See bug 492714. */
        if (pn->pn_right != pn->pn_left) {
            if (!transplant(pn->pn_right))
                return false;
        }
        break;

      case PN_UNARY:
        if (!transplant(pn->pn_kid))
            return false;
        break;

      case PN_CODE:
      case PN_NAME:
        if (!transplant(pn->maybeExpr()))
            return false;

        if (pn->isDefn()) {
            if (genexp && !BumpStaticLevel(pn, pc))
                return false;
        } else if (pn->isUsed()) {
            JS_ASSERT(pn->pn_cookie.isFree());

            Definition *dn = pn->pn_lexdef;
            JS_ASSERT(dn->isDefn());

            /*
             * Adjust the definition's block id only if it is a placeholder not
             * to the left of the root node, and if pn is the last use visited
             * in the comprehension expression (to avoid adjusting the blockid
             * multiple times).
             *
             * Non-placeholder definitions within the comprehension expression
             * will be visited further below.
             */
            if (dn->isPlaceholder() && dn->pn_pos >= root->pn_pos && dn->dn_uses == pn) {
                if (genexp && !BumpStaticLevel(dn, pc))
                    return false;
                if (!AdjustBlockId(dn, adjust, pc))
                    return false;
            }

            RootedAtom atom(parser->context, pn->pn_atom);
#ifdef DEBUG
            StmtInfoPC *stmt = LexicalLookup(pc, atom, NULL, (StmtInfoPC *)NULL);
            JS_ASSERT(!stmt || stmt != pc->topStmt);
#endif
            if (genexp && !dn->isOp(JSOP_CALLEE)) {
                JS_ASSERT(!pc->decls().lookupFirst(atom));

                if (dn->pn_pos < root->pn_pos) {
                    /*
                     * The variable originally appeared to be a use of a
                     * definition or placeholder outside the generator, but now
                     * we know it is scoped within the comprehension tail's
                     * clauses. Make it (along with any other uses within the
                     * generator) a use of a new placeholder in the generator's
                     * lexdeps.
                     */
                    Definition *dn2 = parser->handler.newPlaceholder(atom, parser->pc);
                    if (!dn2)
                        return false;
                    dn2->pn_pos = root->pn_pos;

                    /*
                     * Change all uses of |dn| that lie within the generator's
                     * |yield| expression into uses of dn2.
                     */
                    ParseNode **pnup = &dn->dn_uses;
                    ParseNode *pnu;
                    while ((pnu = *pnup) != NULL && pnu->pn_pos >= root->pn_pos) {
                        pnu->pn_lexdef = dn2;
                        dn2->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
                        pnup = &pnu->pn_link;
                    }
                    dn2->dn_uses = dn->dn_uses;
                    dn->dn_uses = *pnup;
                    *pnup = NULL;
                    DefinitionSingle def = DefinitionSingle::new_<FullParseHandler>(dn2);
                    if (!pc->lexdeps->put(atom, def))
                        return false;
                    if (dn->isClosed())
                        dn2->pn_dflags |= PND_CLOSED;
                } else if (dn->isPlaceholder()) {
                    /*
                     * The variable first occurs free in the 'yield' expression;
                     * move the existing placeholder node (and all its uses)
                     * from the parent's lexdeps into the generator's lexdeps.
                     */
                    outerpc->lexdeps->remove(atom);
                    DefinitionSingle def = DefinitionSingle::new_<FullParseHandler>(dn);
                    if (!pc->lexdeps->put(atom, def))
                        return false;
                } else if (dn->isImplicitArguments()) {
                    /*
                     * Implicit 'arguments' Definition nodes (see
                     * PND_IMPLICITARGUMENTS in Parser::functionBody) are only
                     * reachable via the lexdefs of their uses. Unfortunately,
                     * there may be multiple uses, so we need to maintain a set
                     * to only bump the definition once.
                     */
                    if (genexp && !visitedImplicitArguments.has(dn)) {
                        if (!BumpStaticLevel(dn, pc))
                            return false;
                        if (!AdjustBlockId(dn, adjust, pc))
                            return false;
                        if (!visitedImplicitArguments.put(dn))
                            return false;
                    }
                }
            }
        }

        if (pn->pn_pos >= root->pn_pos) {
            if (!AdjustBlockId(pn, adjust, pc))
                return false;
        }
        break;

      case PN_NULLARY:
        /* Nothing. */
        break;
    }
    return true;
}

/*
 * Starting from a |for| keyword after the first array initialiser element or
 * an expression in an open parenthesis, parse the tail of the comprehension
 * or generator expression signified by this |for| keyword in context.
 *
 * Return null on failure, else return the top-most parse node for the array
 * comprehension or generator expression, with a unary node as the body of the
 * (possibly nested) for-loop, initialized by |kind, op, kid|.
 */
template <>
ParseNode *
Parser<FullParseHandler>::comprehensionTail(ParseNode *kid, unsigned blockid, bool isGenexp,
                                            ParseContext<FullParseHandler> *outerpc,
                                            ParseNodeKind kind, JSOp op)
{
    /*
     * If we saw any inner functions while processing the generator expression
     * then they may have upvars referring to the let vars in this generator
     * which were not correctly processed. Bail out and start over without
     * allowing lazy parsing.
     */
    if (handler.syntaxParser) {
        handler.disableSyntaxParser();
        abortedSyntaxParse = true;
        return NULL;
    }

    unsigned adjust;
    ParseNode *pn, *pn2, *pn3, **pnp;
    StmtInfoPC stmtInfo(context);
    BindData<FullParseHandler> data(context);
    TokenKind tt;

    JS_ASSERT(tokenStream.currentToken().type == TOK_FOR);

    if (kind == PNK_SEMI) {
        /*
         * Generator expression desugars to an immediately applied lambda that
         * yields the next value from a for-in loop (possibly nested, and with
         * optional if guard). Make pn be the TOK_LC body node.
         */
        pn = pushLexicalScope(&stmtInfo);
        if (!pn)
            return null();
        adjust = pn->pn_blockid - blockid;
    } else {
        JS_ASSERT(kind == PNK_ARRAYPUSH);

        /*
         * Make a parse-node and literal object representing the block scope of
         * this array comprehension. Our caller in primaryExpr, the TOK_LB case
         * aka the array initialiser case, has passed the blockid to claim for
         * the comprehension's block scope. We allocate that id or one above it
         * here, by calling PushLexicalScope.
         *
         * In the case of a comprehension expression that has nested blocks
         * (e.g., let expressions), we will allocate a higher blockid but then
         * slide all blocks "to the right" to make room for the comprehension's
         * block scope.
         */
        adjust = pc->blockid();
        pn = pushLexicalScope(&stmtInfo);
        if (!pn)
            return null();

        JS_ASSERT(blockid <= pn->pn_blockid);
        JS_ASSERT(blockid < pc->blockidGen);
        JS_ASSERT(pc->bodyid < blockid);
        pn->pn_blockid = stmtInfo.blockid = blockid;
        JS_ASSERT(adjust < blockid);
        adjust = blockid - adjust;
    }

    pnp = &pn->pn_expr;

    CompExprTransplanter transplanter(kid, this, outerpc, kind == PNK_SEMI, adjust);
    if (!transplanter.init())
        return null();

    if (!transplanter.transplant(kid))
        return null();

    JS_ASSERT(pc->blockChain && pc->blockChain == pn->pn_objbox->object);
    data.initLet(HoistVars, *pc->blockChain, JSMSG_ARRAY_INIT_TOO_BIG);

    do {
        /*
         * FOR node is binary, left is loop control and right is body.  Use
         * index to count each block-local let-variable on the left-hand side
         * of the in/of.
         */
        pn2 = BinaryNode::create(PNK_FOR, &handler);
        if (!pn2)
            return null();

        pn2->setOp(JSOP_ITER);
        pn2->pn_iflags = JSITER_ENUMERATE;
        if (allowsForEachIn() && tokenStream.matchToken(TOK_NAME)) {
            if (tokenStream.currentToken().name() == context->names().each)
                pn2->pn_iflags |= JSITER_FOREACH;
            else
                tokenStream.ungetToken();
        }
        MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

        GenexpGuard<FullParseHandler> guard(this);

        RootedPropertyName name(context);
        tt = tokenStream.getToken();
        switch (tt) {
#if JS_HAS_DESTRUCTURING
          case TOK_LB:
          case TOK_LC:
            pc->inDeclDestructuring = true;
            pn3 = primaryExpr(tt);
            pc->inDeclDestructuring = false;
            if (!pn3)
                return null();
            break;
#endif

          case TOK_NAME:
            name = tokenStream.currentToken().name();

            /*
             * Create a name node with pn_op JSOP_NAME.  We can't set pn_op to
             * JSOP_GETLOCAL here, because we don't yet know the block's depth
             * in the operand stack frame.  The code generator computes that,
             * and it tries to bind all names to slots, so we must let it do
             * the deed.
             */
            pn3 = newBindingNode(name, false);
            if (!pn3)
                return null();
            break;

          default:
            report(ParseError, false, null(), JSMSG_NO_VARIABLE_NAME);

          case TOK_ERROR:
            return null();
        }

        bool forOf;
        if (!matchInOrOf(&forOf)) {
            report(ParseError, false, null(), JSMSG_IN_AFTER_FOR_NAME);
            return null();
        }
        if (forOf) {
            if (pn2->pn_iflags != JSITER_ENUMERATE) {
                JS_ASSERT(pn2->pn_iflags == (JSITER_FOREACH | JSITER_ENUMERATE));
                report(ParseError, false, null(), JSMSG_BAD_FOR_EACH_LOOP);
                return null();
            }
            pn2->pn_iflags = JSITER_FOR_OF;
        }

        ParseNode *pn4 = expr();
        if (!pn4)
            return null();
        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

        guard.endBody();

        if (isGenexp) {
            if (!guard.checkValidBody(pn2))
                return null();
        } else {
            if (!guard.maybeNoteGenerator(pn2))
                return null();
        }

        switch (tt) {
#if JS_HAS_DESTRUCTURING
          case TOK_LB:
          case TOK_LC:
            if (!checkDestructuring(&data, pn3))
                return null();

            if (versionNumber() == JSVERSION_1_7 &&
                !(pn2->pn_iflags & JSITER_FOREACH) &&
                !forOf)
            {
                /* Destructuring requires [key, value] enumeration in JS1.7. */
                if (!pn3->isKind(PNK_ARRAY) || pn3->pn_count != 2) {
                    report(ParseError, false, null(), JSMSG_BAD_FOR_LEFTSIDE);
                    return null();
                }

                JS_ASSERT(pn2->isOp(JSOP_ITER));
                JS_ASSERT(pn2->pn_iflags & JSITER_ENUMERATE);
                pn2->pn_iflags |= JSITER_FOREACH | JSITER_KEYVALUE;
            }
            break;
#endif

          case TOK_NAME:
            data.pn = pn3;
            if (!data.binder(context, &data, name, this))
                return null();
            break;

          default:;
        }

        /*
         * Synthesize a declaration. Every definition must appear in the parse
         * tree in order for ComprehensionTranslator to work.
         */
        ParseNode *vars = ListNode::create(PNK_VAR, &handler);
        if (!vars)
            return null();
        vars->setOp(JSOP_NOP);
        vars->pn_pos = pn3->pn_pos;
        vars->makeEmpty();
        vars->append(pn3);

        /* Definitions can't be passed directly to EmitAssignment as lhs. */
        pn3 = cloneLeftHandSide(pn3);
        if (!pn3)
            return null();

        pn2->pn_left = handler.newTernary(PNK_FORIN, vars, pn3, pn4);
        if (!pn2->pn_left)
            return null();
        *pnp = pn2;
        pnp = &pn2->pn_right;
    } while (tokenStream.matchToken(TOK_FOR));

    if (tokenStream.matchToken(TOK_IF)) {
        pn2 = TernaryNode::create(PNK_IF, &handler);
        if (!pn2)
            return null();
        pn2->pn_kid1 = condition();
        if (!pn2->pn_kid1)
            return null();
        *pnp = pn2;
        pnp = &pn2->pn_kid2;
    }

    pn2 = UnaryNode::create(kind, &handler);
    if (!pn2)
        return null();
    pn2->setOp(op);
    pn2->pn_kid = kid;
    *pnp = pn2;

    PopStatementPC(context, pc);
    return pn;
}

template <>
bool
Parser<FullParseHandler>::arrayInitializerComprehensionTail(ParseNode *pn)
{
    /* Relabel pn as an array comprehension node. */
    pn->setKind(PNK_ARRAYCOMP);

    /*
     * Remove the comprehension expression from pn's linked list
     * and save it via pnexp.  We'll re-install it underneath the
     * ARRAYPUSH node after we parse the rest of the comprehension.
     */
    ParseNode *pnexp = pn->last();
    JS_ASSERT(pn->pn_count == 1);
    pn->pn_count = 0;
    pn->pn_tail = &pn->pn_head;
    *pn->pn_tail = NULL;

    ParseNode *pntop = comprehensionTail(pnexp, pn->pn_blockid, false, NULL,
                                         PNK_ARRAYPUSH, JSOP_ARRAYPUSH);
    if (!pntop)
        return false;
    pn->append(pntop);
    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::arrayInitializerComprehensionTail(Node pn)
{
    return abortIfSyntaxParser();
}

#if JS_HAS_GENERATOR_EXPRS

/*
 * Starting from a |for| keyword after an expression, parse the comprehension
 * tail completing this generator expression. Wrap the expression at kid in a
 * generator function that is immediately called to evaluate to the generator
 * iterator that is the value of this generator expression.
 *
 * |kid| must be the expression before the |for| keyword; we return an
 * application of a generator function that includes the |for| loops and
 * |if| guards, with |kid| as the operand of a |yield| expression as the
 * innermost loop body.
 *
 * Note how unlike Python, we do not evaluate the expression to the right of
 * the first |in| in the chain of |for| heads. Instead, a generator expression
 * is merely sugar for a generator function expression and its application.
 */
template <>
ParseNode *
Parser<FullParseHandler>::generatorExpr(ParseNode *kid)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    /* Create a |yield| node for |kid|. */
    ParseNode *pn = UnaryNode::create(PNK_YIELD, &handler);
    if (!pn)
        return null();
    pn->setOp(JSOP_YIELD);
    pn->setInParens(true);
    pn->pn_pos = kid->pn_pos;
    pn->pn_kid = kid;
    pn->pn_hidden = true;

    /* Make a new node for the desugared generator function. */
    ParseNode *genfn = CodeNode::create(PNK_FUNCTION, &handler);
    if (!genfn)
        return null();
    genfn->setOp(JSOP_LAMBDA);
    JS_ASSERT(!genfn->pn_body);
    genfn->pn_dflags = 0;

    {
        ParseContext<FullParseHandler> *outerpc = pc;

        RootedFunction fun(context, newFunction(outerpc, /* atom = */ NullPtr(), Expression));
        if (!fun)
            return null();

        /* Create box for fun->object early to protect against last-ditch GC. */
        FunctionBox *genFunbox = newFunctionBox(fun, outerpc, outerpc->sc->strict);
        if (!genFunbox)
            return null();

        ParseContext<FullParseHandler> genpc(this, outerpc, genFunbox,
                                             outerpc->staticLevel + 1, outerpc->blockidGen);
        if (!genpc.init())
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

        genFunbox->setIsGenerator();
        genFunbox->inGenexpLambda = true;
        genfn->pn_funbox = genFunbox;
        genfn->pn_blockid = genpc.bodyid;

        ParseNode *body = comprehensionTail(pn, outerpc->blockid(), true, outerpc);
        if (!body)
            return null();
        JS_ASSERT(!genfn->pn_body);
        genfn->pn_body = body;
        genfn->pn_pos.begin = body->pn_pos.begin = kid->pn_pos.begin;
        genfn->pn_pos.end = body->pn_pos.end = tokenStream.currentToken().pos.end;

        RootedPropertyName funName(context);
        if (!leaveFunction(genfn, funName, outerpc))
            return null();
    }

    /*
     * Our result is a call expression that invokes the anonymous generator
     * function object.
     */
    ParseNode *result = ListNode::create(PNK_GENEXP, &handler);
    if (!result)
        return null();
    result->setOp(JSOP_CALL);
    result->pn_pos.begin = genfn->pn_pos.begin;
    result->initList(genfn);
    return result;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::generatorExpr(Node kid)
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

static const char js_generator_str[] = "generator";

#endif /* JS_HAS_GENERATOR_EXPRS */
#endif /* JS_HAS_GENERATORS */

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExprWithoutYield(unsigned msg)
{
#ifdef JS_HAS_GENERATORS
    GenexpGuard<ParseHandler> yieldGuard(this);
#endif
    Node res = assignExpr();
    yieldGuard.endBody();
    if (res) {
#ifdef JS_HAS_GENERATORS
        if (!yieldGuard.checkValidBody(res, msg))
            return null();
#endif
    }
    return res;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::argumentList(Node listNode)
{
    if (tokenStream.matchToken(TOK_RP, TSF_OPERAND))
        return true;

    GenexpGuard<ParseHandler> guard(this);
    bool arg0 = true;

    do {
        Node argNode = assignExpr();
        if (!argNode)
            return false;
        if (arg0)
            guard.endBody();

#if JS_HAS_GENERATORS
        if (handler.isOperationWithoutParens(argNode, PNK_YIELD) &&
            tokenStream.peekToken() == TOK_COMMA) {
            report(ParseError, false, argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
            return false;
        }
#endif
#if JS_HAS_GENERATOR_EXPRS
        if (tokenStream.matchToken(TOK_FOR)) {
            if (!guard.checkValidBody(argNode))
                return false;
            argNode = generatorExpr(argNode);
            if (!argNode)
                return false;
            if (!arg0 || tokenStream.peekToken() == TOK_COMMA) {
                report(ParseError, false, argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
                return false;
            }
        } else
#endif
        if (arg0 && !guard.maybeNoteGenerator(argNode))
            return false;

        arg0 = false;

        handler.addList(listNode, argNode);
    } while (tokenStream.matchToken(TOK_COMMA));

    if (tokenStream.getToken() != TOK_RP) {
        report(ParseError, false, null(), JSMSG_PAREN_AFTER_ARGS);
        return false;
    }
    return true;
}

template <>
PropertyName *
Parser<FullParseHandler>::foldPropertyByValue(ParseNode *pn)
{
    /*
     * Optimize property name lookups. If the name is a PropertyName,
     * then make a name-based node so the emitter will use a name-based
     * bytecode. Otherwise make a node using the property expression
     * by value. If the node is a string containing an index, convert
     * it to a number to save work later.
     */

    uint32_t index;
    if (foldConstants) {
        if (pn->isKind(PNK_STRING)) {
            JSAtom *atom = pn->pn_atom;
            if (atom->isIndex(&index)) {
                pn->setKind(PNK_NUMBER);
                pn->setOp(JSOP_DOUBLE);
                pn->pn_dval = index;
            } else {
                return atom->asPropertyName();
            }
        } else if (pn->isKind(PNK_NUMBER)) {
            double number = pn->pn_dval;
            if (number != ToUint32(number)) {
                JSAtom *atom = ToAtom<NoGC>(context, DoubleValue(number));
                if (!atom)
                    return NULL;
                return atom->asPropertyName();
            }
        }
    }

    return NULL;
}

template <>
PropertyName *
Parser<SyntaxParseHandler>::foldPropertyByValue(Node pn)
{
    return NULL;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberExpr(TokenKind tt, bool allowCallSyntax)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    Node lhs;

    JS_CHECK_RECURSION(context, return null());

    /* Check for new expression first. */
    if (tt == TOK_NEW) {
        lhs = handler.newList(PNK_NEW, null(), JSOP_NEW);
        if (!lhs)
            return null();

        tt = tokenStream.getToken(TSF_OPERAND);
        Node ctorExpr = memberExpr(tt, false);
        if (!ctorExpr)
            return null();

        handler.addList(lhs, ctorExpr);

        if (tokenStream.matchToken(TOK_LP) && !argumentList(lhs))
            return null();
    } else {
        lhs = primaryExpr(tt);
        if (!lhs)
            return null();
    }

    while ((tt = tokenStream.getToken()) > TOK_EOF) {
        Node nextMember;
        if (tt == TOK_DOT) {
            tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
            if (tt == TOK_ERROR)
                return null();
            if (tt == TOK_NAME) {
                PropertyName *field = tokenStream.currentToken().name();
                uint32_t end = tokenStream.currentToken().pos.end;
                nextMember = handler.newPropertyAccess(lhs, field, end);
                if (!nextMember)
                    return null();
            } else {
                report(ParseError, false, null(), JSMSG_NAME_AFTER_DOT);
                return null();
            }
        } else if (tt == TOK_LB) {
            Node propExpr = expr();
            if (!propExpr)
                return null();

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);

            /*
             * Do folding so we don't have roundtrip changes for cases like:
             * function (obj) { return obj["a" + "b"] }
             */
            if (foldConstants && !FoldConstants(context, &propExpr, this))
                return null();

            PropertyName *name = foldPropertyByValue(propExpr);

            uint32_t end = tokenStream.currentToken().pos.end;
            if (name)
                nextMember = handler.newPropertyAccess(lhs, name, end);
            else
                nextMember = handler.newPropertyByValue(lhs, propExpr, end);
            if (!nextMember)
                return null();
        } else if (allowCallSyntax && tt == TOK_LP) {
            nextMember = handler.newList(PNK_CALL, null(), JSOP_CALL);
            if (!nextMember)
                return null();

            if (JSAtom *atom = handler.isName(lhs)) {
                if (atom == context->names().eval) {
                    /* Select JSOP_EVAL and flag pc as heavyweight. */
                    handler.setOp(nextMember, JSOP_EVAL);
                    pc->sc->setBindingsAccessedDynamically();

                    /*
                     * In non-strict mode code, direct calls to eval can add
                     * variables to the call object.
                     */
                    if (pc->sc->isFunctionBox() && !pc->sc->strict)
                        pc->sc->asFunctionBox()->setHasExtensibleScope();
                }
            } else if (JSAtom *atom = handler.isGetProp(lhs)) {
                /* Select JSOP_FUNAPPLY given foo.apply(...). */
                if (atom == context->names().apply) {
                    handler.setOp(nextMember, JSOP_FUNAPPLY);
                    if (pc->sc->isFunctionBox())
                        pc->sc->asFunctionBox()->usesApply = true;
                } else if (atom == context->names().call) {
                    handler.setOp(nextMember, JSOP_FUNCALL);
                }
            }

            handler.setBeginPosition(nextMember, lhs);
            handler.addList(nextMember, lhs);

            if (!argumentList(nextMember))
                return null();
        } else {
            tokenStream.ungetToken();
            return lhs;
        }

        lhs = nextMember;
    }
    if (tt == TOK_ERROR)
        return null();
    return lhs;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::bracketedExpr()
{
    /*
     * Always accept the 'in' operator in a parenthesized expression,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;
    Node pn = expr();
    pc->parsingForInit = oldParsingForInit;
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::identifierName()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_NAME));

    RootedPropertyName name(context, tokenStream.currentToken().name());
    Node pn = handler.newName(name, pc);
    if (!pn)
        return null();

    if (!pc->inDeclDestructuring && !noteNameUse(name, pn))
        return null();

    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::atomNode(ParseNodeKind kind, JSOp op)
{
    JSAtom *atom = tokenStream.currentToken().atom();
    Node pn = handler.newAtom(kind, atom, op);
    if (!pn)
        return null();

    // Large strings are fast to parse but slow to compress. Stop compression on
    // them, so we don't wait for a long time for compression to finish at the
    // end of compilation.
    const size_t HUGE_STRING = 50000;
    if (sct && sct->active() && kind == PNK_STRING && atom->length() >= HUGE_STRING)
        sct->abort();

    return pn;
}

template <>
ParseNode *
Parser<FullParseHandler>::newRegExp(const jschar *buf, size_t length, RegExpFlag flags)
{
    ParseNode *pn = NullaryNode::create(PNK_REGEXP, &handler);
    if (!pn)
        return NULL;

    const StableCharPtr chars(buf, length);
    RegExpStatics *res = context->regExpStatics();

    Rooted<RegExpObject*> reobj(context);
    if (context->hasfp())
        reobj = RegExpObject::create(context, res, chars.get(), length, flags, &tokenStream);
    else
        reobj = RegExpObject::createNoStatics(context, chars.get(), length, flags, &tokenStream);

    if (!reobj)
        return NULL;

    pn->pn_objbox = newObjectBox(reobj);
    if (!pn->pn_objbox)
        return NULL;

    pn->setOp(JSOP_REGEXP);
    return pn;
}

template <>
SyntaxParseHandler::Node
Parser<SyntaxParseHandler>::newRegExp(const jschar *buf, size_t length, RegExpFlag flags)
{
    // Create the regexp even when doing a syntax parse, to check the regexp's syntax.
    const StableCharPtr chars(buf, length);
    RegExpStatics *res = context->regExpStatics();

    RegExpObject *reobj;
    if (context->hasfp())
        reobj = RegExpObject::create(context, res, chars.get(), length, flags, &tokenStream);
    else
        reobj = RegExpObject::createNoStatics(context, chars.get(), length, flags, &tokenStream);

    return reobj ? SyntaxParseHandler::NodeGeneric : SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::primaryExpr(TokenKind tt)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    Node pn, pn2, pn3;
    JSOp op;

    JS_CHECK_RECURSION(context, return null());

    switch (tt) {
      case TOK_FUNCTION:
        pn = functionExpr();
        if (!pn)
            return null();
        break;

      case TOK_LB:
      {
        pn = handler.newList(PNK_ARRAY, null(), JSOP_NEWINIT);
        if (!pn)
            return null();
#if JS_HAS_GENERATORS
        handler.setBlockId(pn, pc->blockidGen);
#endif

        if (tokenStream.matchToken(TOK_RB, TSF_OPERAND)) {
            /*
             * Mark empty arrays as non-constant, since we cannot easily
             * determine their type.
             */
            handler.setListFlag(pn, PNX_NONCONST);
        } else {
            bool spread = false, missingTrailingComma = false;
            unsigned index = 0;
            for (; ; index++) {
                if (index == JSObject::NELEMENTS_LIMIT) {
                    report(ParseError, false, null(), JSMSG_ARRAY_INIT_TOO_BIG);
                    return null();
                }

                tt = tokenStream.peekToken(TSF_OPERAND);
                if (tt == TOK_RB)
                    break;

                if (tt == TOK_COMMA) {
                    tokenStream.matchToken(TOK_COMMA);
                    pn2 = handler.newElision();
                    if (!pn2)
                        return null();
                    handler.setListFlag(pn, PNX_SPECIALARRAYINIT | PNX_NONCONST);
                } else if (tt == TOK_TRIPLEDOT) {
                    spread = true;
                    handler.setListFlag(pn, PNX_SPECIALARRAYINIT | PNX_NONCONST);

                    tokenStream.getToken();

                    Node inner = assignExpr();
                    if (!inner)
                        return null();

                    pn2 = handler.newUnary(PNK_SPREAD, inner);
                    if (!pn2)
                        return null();
                } else {
                    pn2 = assignExpr();
                    if (!pn2)
                        return null();
                    if (foldConstants && !FoldConstants(context, &pn2, this))
                        return null();
                    if (!handler.isConstant(pn2))
                        handler.setListFlag(pn, PNX_NONCONST);
                }
                handler.addList(pn, pn2);

                if (tt != TOK_COMMA) {
                    /* If we didn't already match TOK_COMMA in above case. */
                    if (!tokenStream.matchToken(TOK_COMMA)) {
                        missingTrailingComma = true;
                        break;
                    }
                }
            }

#if JS_HAS_GENERATORS
            /*
             * At this point, (index == 0 && missingTrailingComma) implies one
             * element initialiser was parsed.
             *
             * An array comprehension of the form:
             *
             *   [i * j for (i in o) for (j in p) if (i != j)]
             *
             * translates to roughly the following let expression:
             *
             *   let (array = new Array, i, j) {
             *     for (i in o) let {
             *       for (j in p)
             *         if (i != j)
             *           array.push(i * j)
             *     }
             *     array
             *   }
             *
             * where array is a nameless block-local variable. The "roughly"
             * means that an implementation may optimize away the array.push.
             * An array comprehension opens exactly one block scope, no matter
             * how many for heads it contains.
             *
             * Each let () {...} or for (let ...) ... compiles to:
             *
             *   JSOP_ENTERBLOCK <o> ... JSOP_LEAVEBLOCK <n>
             *
             * where <o> is a literal object representing the block scope,
             * with <n> properties, naming each var declared in the block.
             *
             * Each var declaration in a let-block binds a name in <o> at
             * compile time, and allocates a slot on the operand stack at
             * runtime via JSOP_ENTERBLOCK. A block-local var is accessed by
             * the JSOP_GETLOCAL and JSOP_SETLOCAL ops. These ops have an
             * immediate operand, the local slot's stack index from fp->spbase.
             *
             * The array comprehension iteration step, array.push(i * j) in
             * the example above, is done by <i * j>; JSOP_ARRAYPUSH <array>,
             * where <array> is the index of array's stack slot.
             */
            if (index == 0 && !spread && tokenStream.matchToken(TOK_FOR) && missingTrailingComma) {
                if (!arrayInitializerComprehensionTail(pn))
                    return null();
            }
#endif /* JS_HAS_GENERATORS */

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_LIST);
        }
        handler.setEndPosition(pn, tokenStream.currentToken().pos.end);
        return pn;
      }

      case TOK_LC:
      {
        Node pnval;

        /*
         * A map from property names we've seen thus far to a mask of property
         * assignment types, stored and retrieved with ALE_SET_INDEX/ALE_INDEX.
         */
        AtomIndexMap seen;

        enum AssignmentType {
            GET     = 0x1,
            SET     = 0x2,
            VALUE   = 0x4 | GET | SET
        };

        pn = handler.newList(PNK_OBJECT, null(), JSOP_NEWINIT);
        if (!pn)
            return null();

        RootedAtom atom(context);
        Value tmp;
        for (;;) {
            TokenKind ltok = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
            switch (ltok) {
              case TOK_NUMBER:
                tmp = DoubleValue(tokenStream.currentToken().number());
                atom = ToAtom<CanGC>(context, HandleValue::fromMarkedLocation(&tmp));
                if (!atom)
                    return null();
                pn3 = handler.newNumber(tokenStream.currentToken());
                break;
              case TOK_NAME:
                {
                    atom = tokenStream.currentToken().name();
                    if (atom == context->names().get) {
                        op = JSOP_INITPROP_GETTER;
                    } else if (atom == context->names().set) {
                        op = JSOP_INITPROP_SETTER;
                    } else {
                        pn3 = handler.newAtom(PNK_NAME, atom);
                        if (!pn3)
                            return null();
                        break;
                    }

                    tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
                    if (tt == TOK_NAME) {
                        atom = tokenStream.currentToken().name();
                        pn3 = handler.newName(atom->asPropertyName(), pc);
                        if (!pn3)
                            return null();
                    } else if (tt == TOK_STRING) {
                        atom = tokenStream.currentToken().atom();

                        uint32_t index;
                        if (atom->isIndex(&index)) {
                            pn3 = handler.newNumber(index);
                            if (!pn3)
                                return null();
                            tmp = DoubleValue(index);
                            atom = ToAtom<CanGC>(context, HandleValue::fromMarkedLocation(&tmp));
                            if (!atom)
                                return null();
                        } else {
                            pn3 = handler.newName(atom->asPropertyName(), pc, PNK_STRING);
                            if (!pn3)
                                return null();
                        }
                    } else if (tt == TOK_NUMBER) {
                        double number = tokenStream.currentToken().number();
                        tmp = DoubleValue(number);
                        atom = ToAtom<CanGC>(context, HandleValue::fromMarkedLocation(&tmp));
                        if (!atom)
                            return null();
                        pn3 = handler.newNumber(tokenStream.currentToken());
                        if (!pn3)
                            return null();
                    } else {
                        tokenStream.ungetToken();
                        pn3 = handler.newAtom(PNK_NAME, atom);
                        if (!pn3)
                            return null();
                        break;
                    }

                    JS_ASSERT(op == JSOP_INITPROP_GETTER || op == JSOP_INITPROP_SETTER);

                    handler.setListFlag(pn, PNX_NONCONST);

                    /* NB: Getter function in { get x(){} } is unnamed. */
                    Rooted<PropertyName*> funName(context, NULL);
                    TokenStream::Position start(keepAtoms);
                    tokenStream.tell(&start);
                    pn2 = functionDef(funName, start, tokenStream.positionToOffset(start),
                                      op == JSOP_INITPROP_GETTER ? Getter : Setter,
                                      Expression);
                    if (!pn2)
                        return null();
                    pn2 = handler.newBinary(PNK_COLON, pn3, pn2, op);
                    goto skip;
                }
              case TOK_STRING: {
                atom = tokenStream.currentToken().atom();
                uint32_t index;
                if (atom->isIndex(&index)) {
                    pn3 = handler.newNumber(index);
                    if (!pn3)
                        return null();
                } else {
                    pn3 = handler.newAtom(PNK_STRING, atom);
                    if (!pn3)
                        return null();
                }
                break;
              }
              case TOK_RC:
                goto end_obj_init;
              default:
                report(ParseError, false, null(), JSMSG_BAD_PROP_ID);
                return null();
            }

            op = JSOP_INITPROP;
            tt = tokenStream.getToken();
            if (tt == TOK_COLON) {
                pnval = assignExpr();
                if (!pnval)
                    return null();

                if (foldConstants && !FoldConstants(context, &pnval, this))
                    return null();

                /*
                 * Treat initializers which mutate __proto__ as non-constant,
                 * so that we can later assume singleton objects delegate to
                 * the default Object.prototype.
                 */
                if (!handler.isConstant(pnval) || atom == context->names().proto)
                    handler.setListFlag(pn, PNX_NONCONST);
            }
#if JS_HAS_DESTRUCTURING_SHORTHAND
            else if (ltok == TOK_NAME && (tt == TOK_COMMA || tt == TOK_RC)) {
                /*
                 * Support, e.g., |var {x, y} = o| as destructuring shorthand
                 * for |var {x: x, y: y} = o|, per proposed JS2/ES4 for JS1.8.
                 */
                tokenStream.ungetToken();
                if (!tokenStream.checkForKeyword(atom->charsZ(), atom->length(), NULL, NULL))
                    return null();
                handler.setListFlag(pn, PNX_DESTRUCT | PNX_NONCONST);
                PropertyName *name = handler.isName(pn3);
                JS_ASSERT(atom);
                pn3 = handler.newName(name, pc);
                if (!pn3)
                    return null();
                pnval = pn3;
            }
#endif
            else {
                report(ParseError, false, null(), JSMSG_COLON_AFTER_ID);
                return null();
            }

            pn2 = handler.newBinary(PNK_COLON, pn3, pnval, op);
          skip:
            if (!pn2)
                return null();
            handler.addList(pn, pn2);

            /*
             * Check for duplicate property names.  Duplicate data properties
             * only conflict in strict mode.  Duplicate getter or duplicate
             * setter halves always conflict.  A data property conflicts with
             * any part of an accessor property.
             */
            AssignmentType assignType;
            if (op == JSOP_INITPROP) {
                assignType = VALUE;
            } else if (op == JSOP_INITPROP_GETTER) {
                assignType = GET;
            } else if (op == JSOP_INITPROP_SETTER) {
                assignType = SET;
            } else {
                JS_NOT_REACHED("bad opcode in object initializer");
                assignType = VALUE; /* try to error early */
            }

            AtomIndexAddPtr p = seen.lookupForAdd(atom);
            if (p) {
                jsatomid index = p.value();
                AssignmentType oldAssignType = AssignmentType(index);
                if ((oldAssignType & assignType) &&
                    (oldAssignType != VALUE || assignType != VALUE || pc->sc->needStrictChecks()))
                {
                    JSAutoByteString name;
                    if (!js_AtomToPrintableString(context, atom, &name))
                        return null();

                    ParseReportKind reportKind =
                        (oldAssignType == VALUE && assignType == VALUE && !pc->sc->needStrictChecks())
                        ? ParseWarning
                        : (pc->sc->needStrictChecks() ? ParseStrictError : ParseError);
                    if (!report(reportKind, pc->sc->strict, null(),
                                JSMSG_DUPLICATE_PROPERTY, name.ptr()))
                    {
                        return null();
                    }
                }
                p.value() = assignType | oldAssignType;
            } else {
                if (!seen.add(p, atom, assignType))
                    return null();
            }

            tt = tokenStream.getToken();
            if (tt == TOK_RC)
                goto end_obj_init;
            if (tt != TOK_COMMA) {
                report(ParseError, false, null(), JSMSG_CURLY_AFTER_LIST);
                return null();
            }
        }

      end_obj_init:
        handler.setEndPosition(pn, tokenStream.currentToken().pos.end);
        return pn;
      }

#if JS_HAS_BLOCK_SCOPE
      case TOK_LET:
        pn = letBlock(LetExpresion);
        if (!pn)
            return null();
        break;
#endif

      case TOK_LP:
      {
        bool genexp;

        pn = parenExpr(&genexp);
        if (!pn)
            return null();
        pn = handler.setInParens(pn);

        if (!genexp)
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);
        break;
      }

      case TOK_STRING:
        pn = atomNode(PNK_STRING, JSOP_STRING);
        if (!pn)
            return null();
        break;

      case TOK_NAME:
        pn = identifierName();
        break;

      case TOK_REGEXP:
        pn = newRegExp(tokenStream.getTokenbuf().begin(),
                       tokenStream.getTokenbuf().length(),
                       tokenStream.currentToken().regExpFlags());
        break;

      case TOK_NUMBER:
        pn = handler.newNumber(tokenStream.currentToken());
        break;

      case TOK_TRUE:
        return handler.newBooleanLiteral(true, tokenStream.currentToken().pos);
      case TOK_FALSE:
        return handler.newBooleanLiteral(false, tokenStream.currentToken().pos);
      case TOK_THIS:
        return handler.newThisLiteral(tokenStream.currentToken().pos);
      case TOK_NULL:
        return handler.newNullLiteral(tokenStream.currentToken().pos);

      case TOK_RP:
        // Not valid expression syntax, but this is valid in an arrow function
        // with no params: `() => body`.
        if (tokenStream.peekToken() == TOK_ARROW) {
            tokenStream.ungetToken();  // put back right paren

            // Now just return something that will allow parsing to continue.
            // It doesn't matter what; when we reach the =>, we will rewind and
            // reparse the whole arrow function. See Parser::assignExpr.
            return handler.newNullLiteral(tokenStream.currentToken().pos);
        }
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();

      case TOK_TRIPLEDOT:
        // Not valid expression syntax, but this is valid in an arrow function
        // with a rest param: `(a, b, ...rest) => body`.
        if (tokenStream.matchToken(TOK_NAME) &&
            tokenStream.matchToken(TOK_RP) &&
            tokenStream.peekToken() == TOK_ARROW)
        {
            tokenStream.ungetToken();  // put back right paren

            // Return an arbitrary expression node. See case TOK_RP above.
            return handler.newNullLiteral(tokenStream.currentToken().pos);
        }
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();

      case TOK_ERROR:
        /* The scanner or one of its subroutines reported the error. */
        return null();

      default:
        report(ParseError, false, null(), JSMSG_SYNTAX_ERROR);
        return null();
    }
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::parenExpr(bool *genexp)
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_LP);
    uint32_t begin = tokenStream.currentToken().pos.begin;

    if (genexp)
        *genexp = false;

    GenexpGuard<ParseHandler> guard(this);

    Node pn = bracketedExpr();
    if (!pn)
        return null();
    guard.endBody();

#if JS_HAS_GENERATOR_EXPRS
    if (tokenStream.matchToken(TOK_FOR)) {
        if (!guard.checkValidBody(pn))
            return null();
        if (handler.isOperationWithoutParens(pn, PNK_COMMA)) {
            report(ParseError, false, null(),
                   JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
            return null();
        }
        pn = generatorExpr(pn);
        if (!pn)
            return null();
        handler.setBeginPosition(pn, begin);
        if (genexp) {
            if (tokenStream.getToken() != TOK_RP) {
                report(ParseError, false, null(),
                       JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
                return null();
            }
            handler.setEndPosition(pn, tokenStream.currentToken().pos.end);
            *genexp = true;
        }
    } else
#endif /* JS_HAS_GENERATOR_EXPRS */

    if (!guard.maybeNoteGenerator(pn))
        return null();

    return pn;
}

template class Parser<FullParseHandler>;
template class Parser<SyntaxParseHandler>;

} /* namespace frontend */
} /* namespace js */

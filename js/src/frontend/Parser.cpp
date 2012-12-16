/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
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

#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#include "frontend/FoldConstants.h"
#include "frontend/ParseMaps.h"
#include "frontend/Parser.h"
#include "frontend/TokenStream.h"
#include "gc/Marking.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsatominlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseMaps-inl.h"
#include "frontend/ParseNode-inl.h"
#include "frontend/Parser-inl.h"
#include "frontend/SharedContext-inl.h"

#include "vm/NumericConversions.h"
#include "vm/RegExpObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::frontend;

/*
 * Insist that the next token be of type tt, or report errno and return null.
 * NB: this macro uses cx and ts from its lexical environment.
 */
#define MUST_MATCH_TOKEN_WITH_FLAGS(tt, errno, __flags)                                     \
    JS_BEGIN_MACRO                                                                          \
        if (tokenStream.getToken((__flags)) != tt) {                                        \
            reportError(NULL, errno);                                                       \
            return NULL;                                                                    \
        }                                                                                   \
    JS_END_MACRO
#define MUST_MATCH_TOKEN(tt, errno) MUST_MATCH_TOKEN_WITH_FLAGS(tt, errno, 0)

bool
StrictModeGetter::get() const
{
    return parser->pc->sc->strict;
}

bool
frontend::GenerateBlockId(ParseContext *pc, uint32_t &blockid)
{
    if (pc->blockidGen == JS_BIT(20)) {
        JS_ReportErrorNumber(pc->sc->context, js_GetErrorMessage, NULL, JSMSG_NEED_DIET, "program");
        return false;
    }
    JS_ASSERT(pc->blockidGen < JS_BIT(20));
    blockid = pc->blockidGen++;
    return true;
}

static void
PushStatementPC(ParseContext *pc, StmtInfoPC *stmt, StmtType type)
{
    stmt->blockid = pc->blockid();
    PushStatement(pc, stmt, type);
}

// See comment on member function declaration.
bool
ParseContext::define(JSContext *cx, PropertyName *name, ParseNode *pn, Definition::Kind kind)
{
    JS_ASSERT(!pn->isUsed());
    JS_ASSERT_IF(pn->isDefn(), pn->isPlaceholder());

    Definition *prevDef = NULL;
    if (kind == Definition::LET)
        prevDef = decls_.lookupFirst(name);
    else
        JS_ASSERT(!decls_.lookupFirst(name));

    if (!prevDef)
        prevDef = lexdeps.lookupDefn(name);

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
        JS_ASSERT(sc->isFunction);
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
        if (sc->isFunction) {
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
        JS_ASSERT(dn->pn_cookie.level() == staticLevel); /* see BindLet */
        if (!decls_.addShadow(name, dn))
            return false;
        break;

      case Definition::PLACEHOLDER:
      case Definition::NAMED_LAMBDA:
        JS_NOT_REACHED("unexpected kind");
        break;
    }

    return true;
}

void
ParseContext::prepareToAddDuplicateArg(Definition *prevDecl)
{
    JS_ASSERT(prevDecl->kind() == Definition::ARG);
    JS_ASSERT(decls_.lookupFirst(prevDecl->name()) == prevDecl);
    JS_ASSERT(!prevDecl->isClosed());
    decls_.remove(prevDecl->name());
}

void
ParseContext::updateDecl(JSAtom *atom, ParseNode *pn)
{
    Definition *oldDecl = decls_.lookupFirst(atom);

    pn->setDefn(true);
    Definition *newDecl = (Definition *)pn;
    decls_.updateFirst(atom, newDecl);

    if (!sc->isFunction) {
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

void
ParseContext::popLetDecl(JSAtom *atom)
{
    JS_ASSERT(decls_.lookupFirst(atom)->isLet());
    decls_.remove(atom);
}

static void
AppendPackedBindings(const ParseContext *pc, const DeclVector &vec, Binding *dst)
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
          case Definition::LET:
          case Definition::NAMED_LAMBDA:
          case Definition::PLACEHOLDER:
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

bool
ParseContext::generateFunctionBindings(JSContext *cx, InternalHandle<Bindings*> bindings) const
{
    JS_ASSERT(sc->isFunction);

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

    FunctionBox *funbox = sc->asFunbox();
    if (bindings->hasAnyAliasedBindings() || funbox->hasExtensibleScope())
        funbox->function()->setIsHeavyweight();

    return true;
}

Parser::Parser(JSContext *cx, const CompileOptions &options,
               StableCharPtr chars, size_t length, bool foldConstants)
  : AutoGCRooter(cx, PARSER),
    context(cx),
    strictModeGetter(thisForCtor()),
    tokenStream(cx, options, chars, length, &strictModeGetter),
    tempPoolMark(NULL),
    allocator(cx),
    traceListHead(NULL),
    pc(NULL),
    sct(NULL),
    keepAtoms(cx->runtime),
    foldConstants(foldConstants),
    compileAndGo(options.compileAndGo),
    selfHostingMode(options.selfHostingMode)
{
    cx->activeCompilations++;
}

bool
Parser::init()
{
    if (!context->ensureParseMapPool())
        return false;

    tempPoolMark = context->tempLifoAlloc().mark();
    return true;
}

Parser::~Parser()
{
    JSContext *cx = context;
    cx->tempLifoAlloc().release(tempPoolMark);
    cx->activeCompilations--;
}

ObjectBox *
Parser::newObjectBox(JSObject *obj)
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

FunctionBox::FunctionBox(JSContext *cx, ObjectBox* traceListHead, JSFunction *fun,
                         ParseContext *outerpc, bool strict)
  : ObjectBox(fun, traceListHead),
    SharedContext(cx, /* isFunction = */ true, strict),
    bindings(),
    bufStart(0),
    bufEnd(0),
    ndefaults(0),
    inWith(false),                  // initialized below
    inGenexpLambda(false),
    funCxFlags()
{
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

    } else if (!outerpc->sc->isFunction) {
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
        JSObject *scope = outerpc->sc->asGlobal()->scopeChain();
        while (scope) {
            if (scope->isWith())
                inWith = true;
            scope = scope->enclosingScope();
        }
    } else {
        // This is like the above case, but for more deeply nested functions.
        // For example:
        //
        //   with (o) { eval("(function() { (function() { g(); })(); })();"); } }
        //
        // In this case, the inner anonymous function needs to inherit the
        // setting of |inWith| from the outer one.
        FunctionBox *parent = outerpc->sc->asFunbox();
        if (parent && parent->inWith)
            inWith = true;
    }
}

FunctionBox *
Parser::newFunctionBox(JSFunction *fun, ParseContext *outerpc, bool strict)
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

void
Parser::trace(JSTracer *trc)
{
    traceListHead->trace(trc);
}

static bool
GenerateBlockIdForStmtNode(ParseNode *pn, ParseContext *pc)
{
    JS_ASSERT(pc->topStmt);
    JS_ASSERT(pc->topStmt->maybeScope());
    JS_ASSERT(pn->isKind(PNK_STATEMENTLIST) || pn->isKind(PNK_LEXICALSCOPE));
    if (!GenerateBlockId(pc, pc->topStmt->blockid))
        return false;
    pn->pn_blockid = pc->topStmt->blockid;
    return true;
}

/*
 * Parse a top-level JS script.
 */
ParseNode *
Parser::parse(JSObject *chain)
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
    ParseContext globalpc(this, &globalsc, /* staticLevel = */ 0, /* bodyid = */ 0);
    if (!globalpc.init())
        return NULL;

    ParseNode *pn = statements();
    if (pn) {
        if (!tokenStream.matchToken(TOK_EOF)) {
            reportError(NULL, JSMSG_SYNTAX_ERROR);
            pn = NULL;
        } else if (foldConstants) {
            if (!FoldConstants(context, pn, this))
                pn = NULL;
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
#define ENDS_IN_OTHER   0
#define ENDS_IN_RETURN  1
#define ENDS_IN_BREAK   2

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

static bool
ReportBadReturn(JSContext *cx, Parser *parser, ParseNode *pn, Parser::Reporter reporter,
                unsigned errnum, unsigned anonerrnum)
{
    JSAutoByteString name;
    JSAtom *atom = parser->pc->sc->asFunbox()->function()->atom();
    if (atom) {
        if (!js_AtomToPrintableString(cx, atom, &name))
            return false;
    } else {
        errnum = anonerrnum;
    }
    return (parser->*reporter)(pn, errnum, name.ptr());
}

static bool
CheckFinalReturn(JSContext *cx, Parser *parser, ParseNode *pn)
{
    JS_ASSERT(parser->pc->sc->isFunction);
    return HasFinalReturn(pn) == ENDS_IN_RETURN ||
           ReportBadReturn(cx, parser, pn, &Parser::reportStrictWarning,
                           JSMSG_NO_RETURN_VALUE, JSMSG_ANON_NO_RETURN_VALUE);
}

/*
 * Check that it is permitted to assign to lhs.  Strict mode code may not
 * assign to 'eval' or 'arguments'.
 */
static bool
CheckStrictAssignment(JSContext *cx, Parser *parser, ParseNode *lhs)
{
    if (parser->pc->sc->needStrictChecks() && lhs->isKind(PNK_NAME)) {
        JSAtom *atom = lhs->pn_atom;
        if (atom == cx->names().eval || atom == cx->names().arguments) {
            JSAutoByteString name;
            if (!js_AtomToPrintableString(cx, atom, &name) ||
                !parser->reportStrictModeError(lhs, JSMSG_DEPRECATED_ASSIGN, name.ptr()))
            {
                return false;
            }
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
bool
CheckStrictBinding(JSContext *cx, Parser *parser, HandlePropertyName name, ParseNode *pn)
{
    if (!parser->pc->sc->needStrictChecks())
        return true;

    if (name == cx->names().eval ||
        name == cx->names().arguments ||
        FindKeyword(name->charsZ(), name->length()))
    {
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(cx, name, &bytes))
            return false;
        return parser->reportStrictModeError(pn, JSMSG_BAD_BINDING, bytes.ptr());
    }

    return true;
}

ParseNode *
Parser::standaloneFunctionBody(HandleFunction fun, const AutoNameVector &formals, HandleScript script,
                               ParseNode *fn, FunctionBox **funbox, bool strict, bool *becameStrict)
{
    if (becameStrict)
        *becameStrict = false;

    *funbox = newFunctionBox(fun, /* outerpc = */ NULL, strict);
    if (!funbox)
        return NULL;

    ParseContext funpc(this, *funbox, 0, /* staticLevel = */ 0);
    if (!funpc.init())
        return NULL;

    for (unsigned i = 0; i < formals.length(); i++) {
        if (!DefineArg(this, fn, formals[i]))
            return NULL;
    }

    ParseNode *pn = functionBody(StatementListBody);
    if (!pn) {
        if (becameStrict && pc->funBecameStrict)
            *becameStrict = true;
        return NULL;
    }

    if (!tokenStream.matchToken(TOK_EOF)) {
        reportError(NULL, JSMSG_SYNTAX_ERROR);
        return NULL;
    }

    if (!FoldConstants(context, pn, this))
        return NULL;

    InternalHandle<Bindings*> bindings(script, &script->bindings);
    if (!funpc.generateFunctionBindings(context, bindings))
        return NULL;

    return pn;
}

ParseNode *
Parser::functionBody(FunctionBodyType type)
{
    JS_ASSERT(pc->sc->isFunction);
    JS_ASSERT(!pc->funHasReturnExpr && !pc->funHasReturnVoid);

    ParseNode *pn;
    if (type == StatementListBody) {
        pn = statements();
    } else {
        JS_ASSERT(type == ExpressionBody);
        JS_ASSERT(JS_HAS_EXPR_CLOSURES);

        pn = UnaryNode::create(PNK_RETURN, this);
        if (pn) {
            pn->pn_kid = assignExpr();
            if (!pn->pn_kid) {
                pn = NULL;
            } else {
                if (pc->sc->asFunbox()->isGenerator()) {
                    ReportBadReturn(context, this, pn, &Parser::reportError,
                                    JSMSG_BAD_GENERATOR_RETURN,
                                    JSMSG_BAD_ANON_GENERATOR_RETURN);
                    pn = NULL;
                } else {
                    pn->setOp(JSOP_RETURN);
                    pn->pn_pos.end = pn->pn_kid->pn_pos.end;
                }
            }
        }
    }

    if (!pn)
        return NULL;

    /* Check for falling off the end of a function that returns a value. */
    if (context->hasStrictOption() && pc->funHasReturnExpr &&
        !CheckFinalReturn(context, this, pn))
    {
        pn = NULL;
    }

    /* Time to implement the odd semantics of 'arguments'. */
    Handle<PropertyName*> arguments = context->names().arguments;

    /*
     * Non-top-level functions use JSOP_DEFFUN which is a dynamic scope
     * operation which means it aliases any bindings with the same name.
     * Due to the implicit declaration mechanism (below), 'arguments' will not
     * have decls and, even if it did, they will not be noted as closed in the
     * emitter. Thus, in the corner case of function-statement-overridding-
     * arguments, flag the whole scope as dynamic.
     */
    if (FuncStmtSet *set = pc->funcStmts) {
        for (FuncStmtSet::Range r = set->all(); !r.empty(); r.popFront()) {
            PropertyName *name = r.front()->asPropertyName();
            if (name == arguments)
                pc->sc->setBindingsAccessedDynamically();
            else if (Definition *dn = pc->decls().lookupFirst(name))
                dn->pn_dflags |= PND_CLOSED;
        }
    }

    /*
     * As explained by the ContextFlags::funArgumentsHasLocalBinding comment,
     * create a declaration for 'arguments' if there are any unbound uses in
     * the function body.
     */
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        if (r.front().key() == arguments) {
            Definition *dn = r.front().value();
            pc->lexdeps->remove(arguments);
            dn->pn_dflags |= PND_IMPLICITARGUMENTS;
            if (!pc->define(context, arguments, dn, Definition::VAR))
                return NULL;
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
    bool hasRest = pc->sc->asFunbox()->function()->hasRest();
    if (hasRest && argumentsHasLocalBinding) {
        reportError(NULL, JSMSG_ARGUMENTS_AND_REST);
        return NULL;
    }

    /*
     * Even if 'arguments' isn't explicitly mentioned, dynamic name lookup
     * forces an 'arguments' binding. The exception is that functions with rest
     * parameters are free from 'arguments'.
     */
    if (!argumentsHasBinding && pc->sc->bindingsAccessedDynamically() && !hasRest) {
        ParseNode *pn = NameNode::create(PNK_NAME, arguments, this, pc);
        if (!pn)
            return NULL;
        if (!pc->define(context, arguments, pn, Definition::VAR))
            return NULL;
        argumentsHasBinding = true;
        argumentsHasLocalBinding = true;
    }

    /*
     * Now that all possible 'arguments' bindings have been added, note whether
     * 'arguments' has a local binding and whether it unconditionally needs an
     * arguments object. (Also see the flags' comments in ContextFlags.)
     */
    if (argumentsHasLocalBinding) {
        FunctionBox *funbox = pc->sc->asFunbox();
        funbox->setArgumentsHasLocalBinding();

        /* Dynamic scope access destroys all hope of optimization. */
        if (pc->sc->bindingsAccessedDynamically())
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
                    Definition *dn = dr.front();
                    if (dn->kind() == Definition::ARG && dn->isAssigned()) {
                        funbox->setDefinitelyNeedsArgsObj();
                        goto exitLoop;
                    }
                }
            }
          exitLoop: ;
        }
    }

    return pn;
}

// Create a placeholder Definition node for |atom|.
// Nb: unlike most functions that are passed a Parser, this one gets a
// SharedContext passed in separately, because in this case |pc| may not equal
// |parser->pc|.
static Definition *
MakePlaceholder(ParseNode *pn, Parser *parser, ParseContext *pc)
{
    Definition *dn = (Definition *) NameNode::create(PNK_NAME, pn->pn_atom, parser, pc);
    if (!dn)
        return NULL;

    dn->setOp(JSOP_NOP);
    dn->setDefn(true);
    dn->pn_dflags |= PND_PLACEHOLDER;
    return dn;
}

static void
ForgetUse(ParseNode *pn)
{
    if (!pn->isUsed()) {
        JS_ASSERT(!pn->isDefn());
        return;
    }

    ParseNode **pnup = &pn->lexdef()->dn_uses;
    ParseNode *pnu;
    while ((pnu = *pnup) != pn)
        pnup = &pnu->pn_link;
    *pnup = pn->pn_link;
    pn->setUsed(false);
}

static ParseNode *
MakeAssignment(ParseNode *pn, ParseNode *rhs, Parser *parser)
{
    ParseNode *lhs = parser->cloneNode(*pn);
    if (!lhs)
        return NULL;

    if (pn->isUsed()) {
        Definition *dn = pn->pn_lexdef;
        ParseNode **pnup = &dn->dn_uses;

        while (*pnup != pn)
            pnup = &(*pnup)->pn_link;
        *pnup = lhs;
        lhs->pn_link = pn->pn_link;
        pn->pn_link = NULL;
    }

    pn->setKind(PNK_ASSIGN);
    pn->setOp(JSOP_NOP);
    pn->setArity(PN_BINARY);
    pn->setInParens(false);
    pn->setUsed(false);
    pn->setDefn(false);
    pn->pn_left = lhs;
    pn->pn_right = rhs;
    pn->pn_pos.end = rhs->pn_pos.end;
    return lhs;
}

/* See comment for use in Parser::functionDef. */
static bool
MakeDefIntoUse(Definition *dn, ParseNode *pn, JSAtom *atom, Parser *parser)
{
    /* Turn pn into a definition. */
    parser->pc->updateDecl(atom, pn);

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
        parser->prepareNodeForMutation(dn);
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
            ParseNode *lhs = MakeAssignment(dn, rhs, parser);
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
typedef bool
(*Binder)(JSContext *cx, BindData *data, HandlePropertyName name, Parser *parser);

static bool
BindLet(JSContext *cx, BindData *data, HandlePropertyName name, Parser *parser);

static bool
BindVarOrConst(JSContext *cx, BindData *data, HandlePropertyName name, Parser *parser);

struct frontend::BindData {
    BindData(JSContext *cx) : let(cx) {}

    ParseNode       *pn;        /* name node for definition processing and
                                   error source coordinates */
    JSOp            op;         /* prolog bytecode or nop */
    Binder          binder;     /* binder, discriminates u */

    struct LetData {
        LetData(JSContext *cx) : blockObj(cx) {}
        VarContext varContext;
        Rooted<StaticBlockObject*> blockObj;
        unsigned   overflow;
    } let;

    void initLet(VarContext varContext, StaticBlockObject &blockObj, unsigned overflow) {
        this->pn = NULL;
        this->op = JSOP_NOP;
        this->binder = BindLet;
        this->let.varContext = varContext;
        this->let.blockObj = &blockObj;
        this->let.overflow = overflow;
    }

    void initVarOrConst(JSOp op) {
        this->op = op;
        this->binder = BindVarOrConst;
    }
};

JSFunction *
Parser::newFunction(ParseContext *pc, HandleAtom atom, FunctionSyntaxKind kind)
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
    parent = pc->sc->isFunction ? NULL : pc->sc->asGlobal()->scopeChain();

    RootedFunction fun(context);
    JSFunction::Flags flags = (kind == Expression)
                              ? JSFunction::INTERPRETED_LAMBDA
                              : JSFunction::INTERPRETED;
    fun = js_NewFunction(context, NullPtr(), NULL, 0, flags, parent, atom);
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

static bool
DeoptimizeUsesWithin(Definition *dn, const TokenPos &pos)
{
    unsigned ndeoptimized = 0;

    for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
        JS_ASSERT(pnu->isUsed());
        JS_ASSERT(!pnu->isDefn());
        if (pnu->pn_pos.begin >= pos.begin && pnu->pn_pos.end <= pos.end) {
            pnu->pn_dflags |= PND_DEOPTIMIZED;
            ++ndeoptimized;
        }
    }

    return ndeoptimized != 0;
}

/*
 * Beware: this function is called for functions nested in other functions or
 * global scripts but not for functions compiled through the Function
 * constructor or JSAPI. To always execute code when a function has finished
 * parsing, use Parser::functionBody.
 */
static bool
LeaveFunction(ParseNode *fn, Parser *parser, PropertyName *funName = NULL,
              FunctionSyntaxKind kind = Expression)
{
    JSContext *cx = parser->context;
    ParseContext *funpc = parser->pc;
    ParseContext *pc = funpc->parent;
    pc->blockidGen = funpc->blockidGen;

    FunctionBox *funbox = fn->pn_funbox;
    JS_ASSERT(funbox == funpc->sc->asFunbox());

    if (!pc->topStmt || pc->topStmt->type == STMT_BLOCK)
        fn->pn_dflags |= PND_BLOCKCHILD;

    /* Propagate unresolved lexical names up to pc->lexdeps. */
    if (funpc->lexdeps->count()) {
        for (AtomDefnRange r = funpc->lexdeps->all(); !r.empty(); r.popFront()) {
            JSAtom *atom = r.front().key();
            Definition *dn = r.front().value();
            JS_ASSERT(dn->isPlaceholder());

            if (atom == funName && kind == Expression) {
                dn->setOp(JSOP_CALLEE);
                if (!dn->pn_cookie.set(cx, funpc->staticLevel,
                                       UpvarCookie::CALLEE_SLOT))
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
                continue;
            }

            Definition *outer_dn = pc->decls().lookupFirst(atom);

            /*
             * Make sure to deoptimize lexical dependencies that are polluted
             * by eval and function statements (which both flag the function as
             * having an extensible scope) or any enclosing 'with'.
             */
            if (funbox->hasExtensibleScope() || pc->parsingWith)
                DeoptimizeUsesWithin(dn, fn->pn_pos);

            if (!outer_dn) {
                AtomDefnAddPtr p = pc->lexdeps->lookupForAdd(atom);
                if (p) {
                    outer_dn = p.value();
                } else {
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
                    outer_dn = MakePlaceholder(dn, parser, pc);
                    if (!outer_dn || !pc->lexdeps->add(p, atom, outer_dn))
                        return false;
                }
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
             * (see CompExprTransplanter::transplant, the PN_FUNC/PN_NAME
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
    if (!funpc->generateFunctionBindings(cx, bindings))
        return false;

    funpc->lexdeps.releaseMap(cx);
    return true;
}

/*
 * DefineArg is called for both the arguments of a regular function definition
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
bool
frontend::DefineArg(Parser *parser, ParseNode *funcpn, HandlePropertyName name,
                    bool disallowDuplicateArgs, Definition **duplicatedArg)
{
    JSContext *cx = parser->context;
    ParseContext *pc = parser->pc;
    SharedContext *sc = pc->sc;

    /* Handle duplicate argument names. */
    if (Definition *prevDecl = pc->decls().lookupFirst(name)) {
        /*
         * Strict-mode disallows duplicate args. We may not know whether we are
         * in strict mode or not (since the function body hasn't been parsed).
         * In such cases, reportStrictModeError will queue up the potential
         * error and return 'true'.
         */
        if (sc->needStrictChecks()) {
            JSAutoByteString bytes;
            if (!js_AtomToPrintableString(cx, name, &bytes))
                return false;
            if (!parser->reportStrictModeError(prevDecl, JSMSG_DUPLICATE_FORMAL, bytes.ptr()))
                return false;
        }

        if (disallowDuplicateArgs) {
            parser->reportError(prevDecl, JSMSG_BAD_DUP_ARGS);
            return false;
        }

        if (duplicatedArg)
            *duplicatedArg = prevDecl;

        /* ParseContext::define assumes and asserts prevDecl is not in decls. */
        pc->prepareToAddDuplicateArg(prevDecl);
    }

    ParseNode *argpn = NameNode::create(PNK_NAME, name, parser, parser->pc);
    if (!argpn)
        return false;

    if (!CheckStrictBinding(parser->context, parser, name, argpn))
        return false;

    funcpn->pn_body->append(argpn);
    return parser->pc->define(parser->context, name, argpn, Definition::ARG);
}

#if JS_HAS_DESTRUCTURING
static bool
BindDestructuringArg(JSContext *cx, BindData *data, HandlePropertyName name, Parser *parser)
{
    ParseContext *pc = parser->pc;
    JS_ASSERT(pc->sc->isFunction);

    if (pc->decls().lookupFirst(name)) {
        parser->reportError(NULL, JSMSG_BAD_DUP_ARGS);
        return false;
    }

    if (!CheckStrictBinding(cx, parser, name, data->pn))
        return false;

    return pc->define(cx, name, data->pn, Definition::VAR);
}
#endif /* JS_HAS_DESTRUCTURING */

bool
Parser::functionArguments(ParseNode **listp, ParseNode* funcpn, bool &hasRest)
{
    if (tokenStream.getToken() != TOK_LP) {
        reportError(NULL, JSMSG_PAREN_BEFORE_FORMAL);
        return false;
    }

    FunctionBox *funbox = pc->sc->asFunbox();
    funbox->bufStart = tokenStream.offsetOfToken(tokenStream.currentToken());

    hasRest = false;

    ParseNode *argsbody = ListNode::create(PNK_ARGSBODY, this);
    if (!argsbody)
        return false;
    argsbody->setOp(JSOP_NOP);
    argsbody->makeEmpty();

    funcpn->pn_body = argsbody;

    if (!tokenStream.matchToken(TOK_RP)) {
        bool hasDefaults = false;
        Definition *duplicatedArg = NULL;
        bool destructuringArg = false;
#if JS_HAS_DESTRUCTURING
        ParseNode *list = NULL;
#endif

        do {
            if (hasRest) {
                reportError(NULL, JSMSG_PARAMETER_AFTER_REST);
                return false;
            }
            switch (TokenKind tt = tokenStream.getToken()) {
#if JS_HAS_DESTRUCTURING
              case TOK_LB:
              case TOK_LC:
              {
                /* See comment below in the TOK_NAME case. */
                if (duplicatedArg) {
                    reportError(duplicatedArg, JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                if (hasDefaults) {
                    reportError(NULL, JSMSG_NONDEFAULT_FORMAL_AFTER_DEFAULT);
                    return false;
                }

                destructuringArg = true;

                /*
                 * A destructuring formal parameter turns into one or more
                 * local variables initialized from properties of a single
                 * anonymous positional parameter, so here we must tweak our
                 * binder and its data.
                 */
                BindData data(context);
                data.pn = NULL;
                data.op = JSOP_DEFVAR;
                data.binder = BindDestructuringArg;
                ParseNode *lhs = destructuringExpr(&data, tt);
                if (!lhs)
                    return false;

                /*
                 * Synthesize a destructuring assignment from the single
                 * anonymous positional parameter into the destructuring
                 * left-hand-side expression and accumulate it in list.
                 */
                HandlePropertyName name = context->names().empty;
                ParseNode *rhs = NameNode::create(PNK_NAME, name, this, this->pc);
                if (!rhs)
                    return false;

                if (!pc->define(context, name, rhs, Definition::ARG))
                    return false;

                ParseNode *item = new_<BinaryNode>(PNK_ASSIGN, JSOP_NOP, lhs->pn_pos, lhs, rhs);
                if (!item)
                    return false;
                if (list) {
                    list->append(item);
                } else {
                    list = ListNode::create(PNK_VAR, this);
                    if (!list)
                        return false;
                    list->initList(item);
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
                        reportError(NULL, JSMSG_NO_REST_NAME);
                    return false;
                }
                /* Fall through */
              }

              case TOK_NAME:
              {
                RootedPropertyName name(context, tokenStream.currentToken().name());
                bool disallowDuplicateArgs = destructuringArg || hasDefaults;
                if (!DefineArg(this, funcpn, name, disallowDuplicateArgs, &duplicatedArg))
                    return false;

                if (tokenStream.matchToken(TOK_ASSIGN)) {
                    if (hasRest) {
                        reportError(NULL, JSMSG_REST_WITH_DEFAULT);
                        return false;
                    }
                    if (duplicatedArg) {
                        reportError(duplicatedArg, JSMSG_BAD_DUP_ARGS);
                        return false;
                    }
                    hasDefaults = true;
                    ParseNode *def_expr = assignExprWithoutYield(JSMSG_YIELD_IN_DEFAULT);
                    if (!def_expr)
                        return false;
                    ParseNode *arg = funcpn->pn_body->last();
                    arg->pn_dflags |= PND_DEFAULT;
                    arg->pn_expr = def_expr;
                    funbox->ndefaults++;
                } else if (!hasRest && hasDefaults) {
                    reportError(NULL, JSMSG_NONDEFAULT_FORMAL_AFTER_DEFAULT);
                    return false;
                }

                break;
              }

              default:
                reportError(NULL, JSMSG_MISSING_FORMAL);
                /* FALL THROUGH */
              case TOK_ERROR:
                return false;
            }
        } while (tokenStream.matchToken(TOK_COMMA));

        if (tokenStream.getToken() != TOK_RP) {
            reportError(NULL, JSMSG_PAREN_AFTER_FORMAL);
            return false;
        }
    }

    return true;
}

ParseNode *
Parser::functionDef(HandlePropertyName funName, const TokenStream::Position &start,
                    FunctionType type, FunctionSyntaxKind kind)
{
    JS_ASSERT_IF(kind == Statement, funName);

    /* Make a TOK_FUNCTION node. */
    ParseNode *pn = FunctionNode::create(PNK_FUNCTION, this);
    if (!pn)
        return NULL;
    pn->pn_body = NULL;
    pn->pn_funbox = NULL;
    pn->pn_cookie.makeFree();
    pn->pn_dflags = 0;

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

            if (context->hasStrictOption() || dn->kind() == Definition::CONST) {
                JSAutoByteString name;
                Reporter reporter = (dn->kind() != Definition::CONST)
                                    ? &Parser::reportStrictWarning
                                    : &Parser::reportError;
                if (!js_AtomToPrintableString(context, funName, &name) ||
                    !(this->*reporter)(NULL, JSMSG_REDECLARED_VAR, Definition::kindString(dn->kind()),
                                       name.ptr()))
                {
                    return NULL;
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
            if (bodyLevel && !MakeDefIntoUse(dn, pn, funName, this))
                return NULL;
        } else if (bodyLevel) {
            /*
             * If this function was used before it was defined, claim the
             * pre-created definition node for this function that primaryExpr
             * put in pc->lexdeps on first forward reference, and recycle pn.
             */
            if (Definition *fn = pc->lexdeps.lookupDefn(funName)) {
                JS_ASSERT(fn->isDefn());
                fn->setKind(PNK_FUNCTION);
                fn->setArity(PN_FUNC);
                fn->pn_pos.begin = pn->pn_pos.begin;
                fn->pn_pos.end = pn->pn_pos.end;

                fn->pn_body = NULL;
                fn->pn_cookie.makeFree();

                pc->lexdeps->remove(funName);
                freeTree(pn);
                pn = fn;
            }

            if (!pc->define(context, funName, pn, Definition::VAR))
                return NULL;
        }

        /*
         * As a SpiderMonkey-specific extension, non-body-level function
         * statements (e.g., functions in an "if" or "while" block) are
         * dynamically bound when control flow reaches the statement. The
         * emitter normally emits functions in two passes (see PNK_ARGSBODY).
         * To distinguish
         */
        if (bodyLevel) {
            JS_ASSERT(pn->functionIsHoisted());
            JS_ASSERT_IF(pc->sc->isFunction, !pn->pn_cookie.isFree());
            JS_ASSERT_IF(!pc->sc->isFunction, pn->pn_cookie.isFree());
        } else {
            JS_ASSERT(!pc->sc->strict);
            JS_ASSERT(pn->pn_cookie.isFree());
            if (pc->sc->isFunction) {
                FunctionBox *funbox = pc->sc->asFunbox();
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
                    return NULL;
            }
            if (!pc->funcStmts->put(funName))
                return NULL;
        }

        /* No further binding (in BindNameToSlot) is needed for functions. */
        pn->pn_dflags |= PND_BOUND;
    } else {
        /* A function expression does not introduce any binding. */
        pn->setOp(JSOP_LAMBDA);
    }

    RootedFunction fun(context, newFunction(pc, funName, kind));
    if (!fun)
        return NULL;

    // If the outer scope is strict, immediately parse the function in strict
    // mode. Otherwise, we parse it normally. If we see a "use strict"
    // directive, we backup and reparse it as strict.
    pn->pn_body = NULL;
    bool initiallyStrict = pc->sc->strict;
    bool becameStrict;
    if (!functionArgsAndBody(pn, fun, funName, type, kind, initiallyStrict, &becameStrict)) {
        if (initiallyStrict || !becameStrict || tokenStream.hadError())
            return NULL;

        // Reparse the function in strict mode.
        tokenStream.seek(start);
        if (funName && tokenStream.getToken() == TOK_ERROR)
            return NULL;
        pn->pn_body = NULL;
        if (!functionArgsAndBody(pn, fun, funName, type, kind, true))
            return NULL;
    }

    return pn;
}

bool
Parser::functionArgsAndBody(ParseNode *pn, HandleFunction fun, HandlePropertyName funName, FunctionType type,
                            FunctionSyntaxKind kind, bool strict, bool *becameStrict)
{
    if (becameStrict)
        *becameStrict = false;
    ParseContext *outerpc = pc;

    // Create box for fun->object early to protect against last-ditch GC.
    FunctionBox *funbox = newFunctionBox(fun, pc, strict);
    if (!funbox)
        return false;

    /* Initialize early for possible flags mutation via destructuringExpr. */
    ParseContext funpc(this, funbox, outerpc->staticLevel + 1, outerpc->blockidGen);
    if (!funpc.init())
        return false;

    /* Now parse formal argument list and compute fun->nargs. */
    ParseNode *prelude = NULL;
    bool hasRest;
    tokenStream.incBanXML();
    if (!functionArguments(&prelude, pn, hasRest))
        return false;
    tokenStream.decBanXML();

    fun->setArgCount(funpc.numArgs());
    if (funbox->ndefaults)
        fun->setHasDefaults();
    if (hasRest)
        fun->setHasRest();

    if (type == Getter && fun->nargs > 0) {
        reportError(NULL, JSMSG_ACCESSOR_WRONG_ARGS, "getter", "no", "s");
        return false;
    }
    if (type == Setter && fun->nargs != 1) {
        reportError(NULL, JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
        return false;
    }

    FunctionBodyType bodyType = StatementListBody;
#if JS_HAS_EXPR_CLOSURES
    if (tokenStream.getToken(TSF_OPERAND) != TOK_LC) {
        tokenStream.ungetToken();
        bodyType = ExpressionBody;
        fun->setIsExprClosure();
    }
#else
    if (!tokenStream.matchToken(TOK_LC)) {
        reportError(NULL, JSMSG_CURLY_BEFORE_BODY);
        return false;
    }
#endif

    ParseNode *body = functionBody(bodyType);
    if (!body) {
        // Notify the caller if this function was discovered to be strict.
        if (becameStrict && pc->funBecameStrict)
            *becameStrict = true;
        return false;
    }

    if (funName && !CheckStrictBinding(context, this, funName, pn))
        return false;

#if JS_HAS_EXPR_CLOSURES
    if (bodyType == StatementListBody) {
#endif
        if (!tokenStream.matchToken(TOK_RC)) {
            reportError(NULL, JSMSG_CURLY_AFTER_BODY);
            return false;
        }
        funbox->bufEnd = tokenStream.offsetOfToken(tokenStream.currentToken()) + 1;
#if JS_HAS_EXPR_CLOSURES
    } else {
        // We shouldn't call endOffset if the tokenizer got an error.
        if (tokenStream.hadError())
            return false;
        funbox->bufEnd = tokenStream.endOffset(tokenStream.currentToken());
        if (kind == Statement && !MatchOrInsertSemicolon(context, &tokenStream))
            return false;
    }
#endif
    pn->pn_pos.end = tokenStream.currentToken().pos.end;

    /*
     * Fruit of the poisonous tree: if a closure contains a dynamic name access
     * (eval, with, etc), we consider the parent to do the same. The reason is
     * that the deoptimizing effects of dynamic name access apply equally to
     * parents: any local can be read at runtime.
     */
    if (funbox->bindingsAccessedDynamically())
        outerpc->sc->setBindingsAccessedDynamically();

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

            block = ListNode::create(PNK_SEQ, this);
            if (!block)
                return false;
            block->pn_pos = body->pn_pos;
            block->initList(body);

            body = block;
        }

        ParseNode *item = UnaryNode::create(PNK_SEMI, this);
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

    /*
     * If any nested function scope does a dynamic scope access, all enclosing
     * scopes may be accessed dynamically.
     */
    if (funbox->bindingsAccessedDynamically())
        outerpc->sc->setBindingsAccessedDynamically();


    pn->pn_funbox = funbox;
    pn->pn_body->append(body);
    pn->pn_body->pn_pos = body->pn_pos;
    pn->pn_blockid = outerpc->blockid();

    if (!LeaveFunction(pn, this, funName, kind))
        return false;

    return true;
}

ParseNode *
Parser::functionStmt()
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_FUNCTION);
    RootedPropertyName name(context);
    if (tokenStream.getToken(TSF_KEYWORD_IS_NAME) == TOK_NAME) {
        name = tokenStream.currentToken().name();
    } else {
        /* Unnamed function expressions are forbidden in statement context. */
        reportError(NULL, JSMSG_UNNAMED_FUNCTION_STMT);
        return NULL;
    }

    TokenStream::Position start;
    tokenStream.positionAfterLastFunctionKeyword(start);

    /* We forbid function statements in strict mode code. */
    if (!pc->atBodyLevel() && pc->sc->needStrictChecks() &&
        !reportStrictModeError(NULL, JSMSG_STRICT_FUNCTION_STATEMENT))
        return NULL;

    return functionDef(name, start, Normal, Statement);
}

ParseNode *
Parser::functionExpr()
{
    RootedPropertyName name(context);
    JS_ASSERT(tokenStream.currentToken().type == TOK_FUNCTION);
    TokenStream::Position start;
    tokenStream.positionAfterLastFunctionKeyword(start);
    if (tokenStream.getToken(TSF_KEYWORD_IS_NAME) == TOK_NAME)
        name = tokenStream.currentToken().name();
    else
        tokenStream.ungetToken();
    return functionDef(name, start, Normal, Expression);
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
bool
Parser::maybeParseDirective(ParseNode *pn, bool *cont)
{
    *cont = pn->isStringExprStatement();
    if (!*cont)
        return true;

    ParseNode *string = pn->pn_kid;
    if (string->isEscapeFreeStringLiteral()) {
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
        pn->pn_prologue = true;

        JSAtom *directive = string->pn_atom;
        if (directive == context->runtime->atomState.useStrict) {
            // We're going to be in strict mode. Note that this scope explicitly
            // had "use strict";
            pc->sc->setExplicitUseStrict();
            if (!pc->sc->strict) {
                if (pc->sc->isFunction) {
                    // Request that this function be reparsed as strict.
                    pc->funBecameStrict = true;
                    return false;
                } else {
                    // We don't reparse global scopes, so we keep track of the
                    // one possible strict violation that could occur in the
                    // directive prologue -- octal escapes -- and complain now.
                    if (tokenStream.sawOctalEscape()) {
                        reportError(NULL, JSMSG_DEPRECATED_OCTAL);
                        return false;
                    }
                    pc->sc->strict = true;
                }
            }
        }
    }
    return true;
}

/*
 * Parse the statements in a block, creating a TOK_LC node that lists the
 * statements' trees.  If called from block-parsing code, the caller must
 * match { before and } after.
 */
ParseNode *
Parser::statements(bool *hasFunctionStmt)
{
    JS_CHECK_RECURSION(context, return NULL);
    if (hasFunctionStmt)
        *hasFunctionStmt = false;

    ParseNode *pn = ListNode::create(PNK_STATEMENTLIST, this);
    if (!pn)
        return NULL;
    pn->makeEmpty();
    pn->pn_blockid = pc->blockid();
    ParseNode *saveBlock = pc->blockNode;
    pc->blockNode = pn;

    bool canHaveDirectives = pc->atBodyLevel();
    for (;;) {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt <= TOK_EOF || tt == TOK_RC) {
            if (tt == TOK_ERROR) {
                if (tokenStream.isEOF())
                    tokenStream.setUnexpectedEOF();
                return NULL;
            }
            break;
        }
        ParseNode *next = statement();
        if (!next) {
            if (tokenStream.isEOF())
                tokenStream.setUnexpectedEOF();
            return NULL;
        }

        if (canHaveDirectives) {
            if (!maybeParseDirective(next, &canHaveDirectives))
                return NULL;
        }

        if (next->isKind(PNK_FUNCTION)) {
            /*
             * PNX_FUNCDEFS notifies the emitter that the block contains body-
             * level function definitions that should be processed before the
             * rest of nodes.
             *
             * |hasFunctionStmt| is for the TOK_LC case in Statement. It
             * is relevant only for function definitions not at body-level,
             * which we call function statements.
             */
            if (pc->atBodyLevel()) {
                pn->pn_xflags |= PNX_FUNCDEFS;
            } else {
                /*
                 * General deoptimization was done in functionDef, here we just
                 * need to tell TOK_LC in Parser::statement to add braces.
                 */
                JS_ASSERT_IF(pc->sc->isFunction, pc->sc->asFunbox()->hasExtensibleScope());
                if (hasFunctionStmt)
                    *hasFunctionStmt = true;
            }
        }
        pn->append(next);
    }

    /*
     * Handle the case where there was a let declaration under this block.  If
     * it replaced pc->blockNode with a new block node then we must refresh pn
     * and then restore pc->blockNode.
     */
    if (pc->blockNode != pn)
        pn = pc->blockNode;
    pc->blockNode = saveBlock;

    pn->pn_pos.end = tokenStream.currentToken().pos.end;
    JS_ASSERT(pn->pn_pos.begin <= pn->pn_pos.end);
    return pn;
}

ParseNode *
Parser::condition()
{
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_COND);
    ParseNode *pn = parenExpr();
    if (!pn)
        return NULL;
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_COND);

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    JS_ASSERT_IF(pn->isKind(PNK_ASSIGN), pn->isOp(JSOP_NOP));
    if (pn->isKind(PNK_ASSIGN) &&
        !pn->isInParens() &&
        !reportStrictWarning(NULL, JSMSG_EQUAL_AS_ASSIGN))
    {
        return NULL;
    }
    return pn;
}

static bool
MatchLabel(JSContext *cx, TokenStream *ts, PropertyName **label)
{
    TokenKind tt = ts->peekTokenSameLine(TSF_OPERAND);
    if (tt == TOK_ERROR)
        return false;
    if (tt == TOK_NAME) {
        (void) ts->getToken();
        *label = ts->currentToken().name();
    } else {
        *label = NULL;
    }
    return true;
}

static bool
ReportRedeclaration(JSContext *cx, Parser *parser, ParseNode *pn, bool isConst, JSAtom *atom)
{
    JSAutoByteString name;
    if (js_AtomToPrintableString(cx, atom, &name))
        parser->reportError(pn, JSMSG_REDECLARED_VAR, isConst ? "const" : "variable", name.ptr());
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
static bool
BindLet(JSContext *cx, BindData *data, HandlePropertyName name, Parser *parser)
{
    ParseContext *pc = parser->pc;
    ParseNode *pn = data->pn;
    if (!CheckStrictBinding(cx, parser, name, pn))
        return false;

    Rooted<StaticBlockObject *> blockObj(cx, data->let.blockObj);
    unsigned blockCount = blockObj->slotCount();
    if (blockCount == JS_BIT(16)) {
        parser->reportError(pn, data->let.overflow);
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
            return ReportRedeclaration(cx, parser, pn, dn->isConst(), name);
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
            ReportRedeclaration(cx, parser, pn, false, name);
        return false;
    }

    /* Store pn in the static block object. */
    blockObj->setDefinitionParseNode(blockCount, reinterpret_cast<Definition *>(pn));
    return true;
}

template <class Op>
static inline bool
ForEachLetDef(JSContext *cx, ParseContext *pc, StaticBlockObject &blockObj, Op op)
{
    for (Shape::Range r = blockObj.lastProperty()->all(); !r.empty(); r.popFront()) {
        Shape &shape = r.front();

        /* Beware the destructuring dummy slots. */
        if (JSID_IS_INT(shape.propid()))
            continue;

        if (!op(cx, pc, blockObj, shape, JSID_TO_ATOM(shape.propid())))
            return false;
    }
    return true;
}

struct PopLetDecl {
    bool operator()(JSContext *, ParseContext *pc, StaticBlockObject &, const Shape &, JSAtom *atom) {
        pc->popLetDecl(atom);
        return true;
    }
};

static void
PopStatementPC(JSContext *cx, ParseContext *pc)
{
    StaticBlockObject *blockObj = pc->topStmt->blockObj;
    JS_ASSERT(!!blockObj == (pc->topStmt->isBlockScope));

    FinishPopStatement(pc);

    if (blockObj) {
        JS_ASSERT(!blockObj->inDictionaryMode());
        ForEachLetDef(cx, pc, *blockObj, PopLetDecl());
        blockObj->resetPrevBlockChainFromParser();
    }
}

static inline bool
OuterLet(ParseContext *pc, StmtInfoPC *stmt, HandleAtom atom)
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

static bool
BindVarOrConst(JSContext *cx, BindData *data, HandlePropertyName name, Parser *parser)
{
    ParseContext *pc = parser->pc;
    ParseNode *pn = data->pn;
    bool isConstDecl = data->op == JSOP_DEFCONST;

    /* Default best op for pn is JSOP_NAME; we'll try to improve below. */
    pn->setOp(JSOP_NAME);

    if (!CheckStrictBinding(cx, parser, name, pn))
        return false;

    StmtInfoPC *stmt = LexicalLookup(pc, name, NULL, (StmtInfoPC *)NULL);

    if (stmt && stmt->type == STMT_WITH) {
        pn->pn_dflags |= PND_DEOPTIMIZED;
        if (pc->sc->isFunction)
            pc->sc->asFunbox()->setMightAliasLocals();
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
    Definition *dn = defs.front();
    Definition::Kind dn_kind = dn->kind();
    if (dn_kind == Definition::ARG) {
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(cx, name, &bytes))
            return false;

        if (isConstDecl) {
            parser->reportError(pn, JSMSG_REDECLARED_PARAM, bytes.ptr());
            return false;
        }
        if (!parser->reportStrictWarning(pn, JSMSG_VAR_HIDES_ARG, bytes.ptr()))
            return false;
    } else {
        bool error = (isConstDecl ||
                      dn_kind == Definition::CONST ||
                      (dn_kind == Definition::LET &&
                       (stmt->type != STMT_CATCH || OuterLet(pc, stmt, name))));

        if (cx->hasStrictOption()
            ? data->op != JSOP_DEFVAR || dn_kind != Definition::VAR
            : error)
        {
            JSAutoByteString bytes;
            Parser::Reporter reporter =
                error ? &Parser::reportError : &Parser::reportStrictWarning;
            if (!js_AtomToPrintableString(cx, name, &bytes) ||
                !(parser->*reporter)(pn, JSMSG_REDECLARED_VAR,
                                     Definition::kindString(dn_kind), bytes.ptr()))
            {
                return false;
            }
        }
    }

    LinkUseToDef(pn, dn);
    return true;
}

static bool
MakeSetCall(JSContext *cx, ParseNode *pn, Parser *parser, unsigned msg)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(pn->isOp(JSOP_CALL) || pn->isOp(JSOP_EVAL) ||
              pn->isOp(JSOP_FUNCALL) || pn->isOp(JSOP_FUNAPPLY));
    if (!parser->reportStrictModeError(pn, msg))
        return false;

    ParseNode *pn2 = pn->pn_head;
    if (pn2->isKind(PNK_FUNCTION) && (pn2->pn_funbox->inGenexpLambda)) {
        parser->reportError(pn, msg);
        return false;
    }
    pn->pn_xflags |= PNX_SETCALL;
    return true;
}

static void
NoteLValue(ParseNode *pn)
{
    if (pn->isUsed())
        pn->pn_lexdef->pn_dflags |= PND_ASSIGNED;

    pn->pn_dflags |= PND_ASSIGNED;
}

static bool
NoteNameUse(ParseNode *pn, Parser *parser)
{
    RootedPropertyName name(parser->context, pn->pn_atom->asPropertyName());
    StmtInfoPC *stmt = LexicalLookup(parser->pc, name, NULL, (StmtInfoPC *)NULL);

    DefinitionList::Range defs = parser->pc->decls().lookupMulti(name);

    Definition *dn;
    if (!defs.empty()) {
        dn = defs.front();
    } else {
        if (AtomDefnAddPtr p = parser->pc->lexdeps->lookupForAdd(name)) {
            dn = p.value();
        } else {
            /*
             * No definition before this use in any lexical scope.
             * Create a placeholder definition node to either:
             * - Be adopted when we parse the real defining
             *   declaration, or
             * - Be left as a free variable definition if we never
             *   see the real definition.
             */
            dn = MakePlaceholder(pn, parser, parser->pc);
            if (!dn || !parser->pc->lexdeps->add(p, name, dn))
                return false;
        }
    }

    JS_ASSERT(dn->isDefn());
    LinkUseToDef(pn, dn);

    if (stmt && stmt->type == STMT_WITH)
        pn->pn_dflags |= PND_DEOPTIMIZED;

    return true;
}

#if JS_HAS_DESTRUCTURING

static bool
BindDestructuringVar(JSContext *cx, BindData *data, ParseNode *pn, Parser *parser)
{
    JS_ASSERT(pn->isKind(PNK_NAME));

    RootedPropertyName name(cx, pn->pn_atom->asPropertyName());

    data->pn = pn;
    if (!data->binder(cx, data, name, parser))
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

    NoteLValue(pn);
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
static bool
BindDestructuringLHS(JSContext *cx, ParseNode *pn, Parser *parser)
{
    switch (pn->getKind()) {
      case PNK_NAME:
        NoteLValue(pn);
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
        if (!MakeSetCall(cx, pn, parser, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return false;
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(pn->isOp(JSOP_XMLNAME));
        pn->setOp(JSOP_BINDXMLNAME);
        break;
#endif

      default:
        parser->reportError(pn, JSMSG_BAD_LEFTSIDE_OF_ASS);
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
static bool
CheckDestructuring(JSContext *cx, BindData *data, ParseNode *left, Parser *parser,
                   bool toplevel = true)
{
    bool ok;

    if (left->isKind(PNK_ARRAYCOMP)) {
        parser->reportError(left, JSMSG_ARRAY_COMP_LEFTSIDE);
        return false;
    }

    Rooted<StaticBlockObject *> blockObj(cx);
    blockObj = data && data->binder == BindLet ? data->let.blockObj.get() : NULL;
    uint32_t blockCountBefore = blockObj ? blockObj->slotCount() : 0;

    if (left->isKind(PNK_ARRAY)) {
        for (ParseNode *pn = left->pn_head; pn; pn = pn->pn_next) {
            /* Nullary comma is an elision; binary comma is an expression.*/
            if (!pn->isArrayHole()) {
                if (pn->isKind(PNK_ARRAY) || pn->isKind(PNK_OBJECT)) {
                    ok = CheckDestructuring(cx, data, pn, parser, false);
                } else {
                    if (data) {
                        if (!pn->isKind(PNK_NAME)) {
                            parser->reportError(pn, JSMSG_NO_VARIABLE_NAME);
                            return false;
                        }
                        ok = BindDestructuringVar(cx, data, pn, parser);
                    } else {
                        ok = BindDestructuringLHS(cx, pn, parser);
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
                ok = CheckDestructuring(cx, data, pn, parser, false);
            } else if (data) {
                if (!pn->isKind(PNK_NAME)) {
                    parser->reportError(pn, JSMSG_NO_VARIABLE_NAME);
                    return false;
                }
                ok = BindDestructuringVar(cx, data, pn, parser);
            } else {
                /*
                 * If right and left point to the same node, then this is
                 * destructuring shorthand ({x} = ...). In that case,
                 * identifierName was not used to parse 'x' so 'x' has not been
                 * officially linked to its def or registered in lexdeps. Do
                 * that now.
                 */
                if (pair->pn_right == pair->pn_left && !NoteNameUse(pn, parser))
                    return false;
                ok = BindDestructuringLHS(cx, pn, parser);
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
        RootedId id(cx, INT_TO_JSID(blockCountBefore));
        if (!StaticBlockObject::addVar(cx, blockObj, id, blockCountBefore, &redeclared))
            return false;
        JS_ASSERT(!redeclared);
        JS_ASSERT(blockObj->slotCount() == blockCountBefore + 1);
    }

    return true;
}

ParseNode *
Parser::destructuringExpr(BindData *data, TokenKind tt)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    pc->inDeclDestructuring = true;
    ParseNode *pn = primaryExpr(tt, false);
    pc->inDeclDestructuring = false;
    if (!pn)
        return NULL;
    if (!CheckDestructuring(context, data, pn, this))
        return NULL;
    return pn;
}

#endif /* JS_HAS_DESTRUCTURING */

ParseNode *
Parser::returnOrYield(bool useAssignExpr)
{
    TokenKind tt = tokenStream.currentToken().type;
    if (!pc->sc->isFunction) {
        reportError(NULL, JSMSG_BAD_RETURN_OR_YIELD,
                    (tt == TOK_RETURN) ? js_return_str : js_yield_str);
        return NULL;
    }

    ParseNode *pn = UnaryNode::create((tt == TOK_RETURN) ? PNK_RETURN : PNK_YIELD, this);
    if (!pn)
        return NULL;

#if JS_HAS_GENERATORS
    if (tt == TOK_YIELD) {
        /*
         * If we're within parens, we won't know if this is a generator expression until we see
         * a |for| token, so we have to delay flagging the current function.
         */
        if (pc->parenDepth == 0) {
            pc->sc->asFunbox()->setIsGenerator();
        } else {
            pc->yieldCount++;
            pc->yieldNode = pn;
        }
    }
#endif

    /* This is ugly, but we don't want to require a semicolon. */
    TokenKind tt2 = tokenStream.peekTokenSameLine(TSF_OPERAND);
    if (tt2 == TOK_ERROR)
        return NULL;

    if (tt2 != TOK_EOF && tt2 != TOK_EOL && tt2 != TOK_SEMI && tt2 != TOK_RC
#if JS_HAS_GENERATORS
        && (tt != TOK_YIELD ||
            (tt2 != tt && tt2 != TOK_RB && tt2 != TOK_RP &&
             tt2 != TOK_COLON && tt2 != TOK_COMMA))
#endif
        )
    {
        ParseNode *pn2 = useAssignExpr ? assignExpr() : expr();
        if (!pn2)
            return NULL;
#if JS_HAS_GENERATORS
        if (tt == TOK_RETURN)
#endif
            pc->funHasReturnExpr = true;
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->pn_kid = pn2;
    } else {
#if JS_HAS_GENERATORS
        if (tt == TOK_RETURN)
#endif
            pc->funHasReturnVoid = true;
    }

    if (pc->funHasReturnExpr && pc->sc->asFunbox()->isGenerator()) {
        /* As in Python (see PEP-255), disallow return v; in generators. */
        ReportBadReturn(context, this, pn, &Parser::reportError, JSMSG_BAD_GENERATOR_RETURN,
                        JSMSG_BAD_ANON_GENERATOR_RETURN);
        return NULL;
    }

    if (context->hasStrictOption() && pc->funHasReturnExpr && pc->funHasReturnVoid &&
        !ReportBadReturn(context, this, pn, &Parser::reportStrictWarning,
                         JSMSG_NO_RETURN_VALUE, JSMSG_ANON_NO_RETURN_VALUE))
    {
        return NULL;
    }

    return pn;
}

static ParseNode *
PushLexicalScope(JSContext *cx, Parser *parser, StaticBlockObject &blockObj, StmtInfoPC *stmt)
{
    ParseNode *pn = LexicalScopeNode::create(PNK_LEXICALSCOPE, parser);
    if (!pn)
        return NULL;

    ObjectBox *blockbox = parser->newObjectBox(&blockObj);
    if (!blockbox)
        return NULL;

    ParseContext *pc = parser->pc;

    PushStatementPC(pc, stmt, STMT_BLOCK);
    blockObj.initPrevBlockChainFromParser(pc->blockChain);
    FinishPushBlockScope(pc, stmt, blockObj);

    pn->setOp(JSOP_LEAVEBLOCK);
    pn->pn_objbox = blockbox;
    pn->pn_cookie.makeFree();
    pn->pn_dflags = 0;
    if (!GenerateBlockId(pc, stmt->blockid))
        return NULL;
    pn->pn_blockid = stmt->blockid;
    return pn;
}

static ParseNode *
PushLexicalScope(JSContext *cx, Parser *parser, StmtInfoPC *stmt)
{
    StaticBlockObject *blockObj = StaticBlockObject::create(cx);
    if (!blockObj)
        return NULL;

    return PushLexicalScope(cx, parser, *blockObj, stmt);
}

#if JS_HAS_BLOCK_SCOPE

struct AddLetDecl
{
    uint32_t blockid;

    AddLetDecl(uint32_t blockid) : blockid(blockid) {}

    bool operator()(JSContext *cx, ParseContext *pc, StaticBlockObject &blockObj, const Shape &shape, JSAtom *)
    {
        ParseNode *def = (ParseNode *) blockObj.getSlot(shape.slot()).toPrivate();
        def->pn_blockid = blockid;
        return pc->define(cx, def->name(), def, Definition::LET);
    }
};

static ParseNode *
PushLetScope(JSContext *cx, Parser *parser, StaticBlockObject &blockObj, StmtInfoPC *stmt)
{
    ParseNode *pn = PushLexicalScope(cx, parser, blockObj, stmt);
    if (!pn)
        return NULL;

    /* Tell codegen to emit JSOP_ENTERLETx (not JSOP_ENTERBLOCK). */
    pn->pn_dflags |= PND_LET;

    /* Populate the new scope with decls found in the head with updated blockid. */
    if (!ForEachLetDef(cx, parser->pc, blockObj, AddLetDecl(stmt->blockid)))
        return NULL;

    return pn;
}

/*
 * Parse a let block statement or let expression (determined by 'letContext').
 * In both cases, bindings are not hoisted to the top of the enclosing block
 * and thus must be carefully injected between variables() and the let body.
 */
ParseNode *
Parser::letBlock(LetContext letContext)
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_LET);

    ParseNode *pnlet = BinaryNode::create(PNK_LET, this);
    if (!pnlet)
        return NULL;

    Rooted<StaticBlockObject*> blockObj(context, StaticBlockObject::create(context));
    if (!blockObj)
        return NULL;

    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_LET);

    ParseNode *vars = variables(PNK_LET, blockObj, DontHoistVars);
    if (!vars)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_LET);

    StmtInfoPC stmtInfo(context);
    ParseNode *block = PushLetScope(context, this, *blockObj, &stmtInfo);
    if (!block)
        return NULL;

    pnlet->pn_left = vars;
    pnlet->pn_right = block;

    ParseNode *ret;
    if (letContext == LetStatement && !tokenStream.matchToken(TOK_LC, TSF_OPERAND)) {
        /*
         * Strict mode eliminates a grammar ambiguity with unparenthesized
         * LetExpressions in an ExpressionStatement. If followed immediately
         * by an arguments list, it's ambiguous whether the let expression
         * is the callee or the call is inside the let expression body.
         *
         * See bug 569464.
         */
        if (!reportStrictModeError(pnlet, JSMSG_STRICT_CODE_LET_EXPR_STMT))
            return NULL;

        /*
         * If this is really an expression in let statement guise, then we
         * need to wrap the TOK_LET node in a TOK_SEMI node so that we pop
         * the return value of the expression.
         */
        ParseNode *semi = UnaryNode::create(PNK_SEMI, this);
        if (!semi)
            return NULL;

        semi->pn_kid = pnlet;
        semi->pn_pos = pnlet->pn_pos;

        letContext = LetExpresion;
        ret = semi;
    } else {
        ret = pnlet;
    }

    if (letContext == LetStatement) {
        JS_ASSERT(block->getOp() == JSOP_LEAVEBLOCK);
        block->pn_expr = statements();
        if (!block->pn_expr)
            return NULL;
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_LET);
    } else {
        JS_ASSERT(letContext == LetExpresion);
        block->setOp(JSOP_LEAVEBLOCKEXPR);
        block->pn_expr = assignExpr();
        if (!block->pn_expr)
            return NULL;
    }

    ret->pn_pos.begin = pnlet->pn_pos.begin = pnlet->pn_left->pn_pos.begin;
    ret->pn_pos.end = pnlet->pn_pos.end = pnlet->pn_right->pn_pos.end;

    PopStatementPC(context, pc);
    return ret;
}

#endif /* JS_HAS_BLOCK_SCOPE */

static bool
PushBlocklikeStatement(StmtInfoPC *stmt, StmtType type, ParseContext *pc)
{
    PushStatementPC(pc, stmt, type);
    return GenerateBlockId(pc, stmt->blockid);
}

static ParseNode *
NewBindingNode(JSAtom *atom, Parser *parser, VarContext varContext = HoistVars)
{
    ParseContext *pc = parser->pc;

    /*
     * If this name is being injected into an existing block/function, see if
     * it has already been declared or if it resolves an outstanding lexdep.
     * Otherwise, this is a let block/expr that introduces a new scope and thus
     * shadows existing decls and doesn't resolve existing lexdeps. Duplicate
     * names are caught by BindLet.
     */
    if (varContext == HoistVars) {
        if (AtomDefnPtr p = pc->lexdeps->lookup(atom)) {
            ParseNode *lexdep = p.value();
            JS_ASSERT(lexdep->isPlaceholder());
            if (lexdep->pn_blockid >= pc->blockid()) {
                lexdep->pn_blockid = pc->blockid();
                pc->lexdeps->remove(p);
                lexdep->pn_pos = parser->tokenStream.currentToken().pos;
                return lexdep;
            }
        }
    }

    /* Make a new node for this declarator name (or destructuring pattern). */
    JS_ASSERT(parser->tokenStream.currentToken().type == TOK_NAME);
    return NameNode::create(PNK_NAME, atom, parser, parser->pc);
}

ParseNode *
Parser::switchStatement()
{
    JS_ASSERT(tokenStream.currentToken().type == TOK_SWITCH);
    ParseNode *pn = BinaryNode::create(PNK_SWITCH, this);
    if (!pn)
        return NULL;
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_SWITCH);

    /* pn1 points to the switch's discriminant. */
    ParseNode *pn1 = parenExpr();
    if (!pn1)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_SWITCH);
    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_SWITCH);

    /*
     * NB: we must push stmtInfo before calling GenerateBlockIdForStmtNode
     * because that function states pc->topStmt->blockid.
     */
    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_SWITCH);

    /* pn2 is a list of case nodes. The default case has pn_left == NULL */
    ParseNode *pn2 = ListNode::create(PNK_STATEMENTLIST, this);
    if (!pn2)
        return NULL;
    pn2->makeEmpty();
    if (!GenerateBlockIdForStmtNode(pn2, pc))
        return NULL;
    ParseNode *saveBlock = pc->blockNode;
    pc->blockNode = pn2;

    bool seenDefault = false;
    TokenKind tt;
    while ((tt = tokenStream.getToken()) != TOK_RC) {
        ParseNode *pn3;
        switch (tt) {
          case TOK_DEFAULT:
            if (seenDefault) {
                reportError(NULL, JSMSG_TOO_MANY_DEFAULTS);
                return NULL;
            }
            seenDefault = true;
            pn3 = BinaryNode::create(PNK_DEFAULT, this);
            if (!pn3)
                return NULL;
            break;

          case TOK_CASE:
          {
            pn3 = BinaryNode::create(PNK_CASE, this);
            if (!pn3)
                return NULL;
            pn3->pn_left = expr();
            if (!pn3->pn_left)
                return NULL;
            break;
          }

          case TOK_ERROR:
            return NULL;

          default:
            reportError(NULL, JSMSG_BAD_SWITCH);
            return NULL;
        }

        pn2->append(pn3);
        if (pn2->pn_count == JS_BIT(16)) {
            reportError(NULL, JSMSG_TOO_MANY_CASES);
            return NULL;
        }

        MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_AFTER_CASE);

        ParseNode *pn4 = ListNode::create(PNK_STATEMENTLIST, this);
        if (!pn4)
            return NULL;
        pn4->makeEmpty();
        while ((tt = tokenStream.peekToken(TSF_OPERAND)) != TOK_RC &&
               tt != TOK_CASE && tt != TOK_DEFAULT) {
            if (tt == TOK_ERROR)
                return NULL;
            ParseNode *pn5 = statement();
            if (!pn5)
                return NULL;
            pn4->pn_pos.end = pn5->pn_pos.end;
            pn4->append(pn5);
        }

        /* Fix the PN_LIST so it doesn't begin at the TOK_COLON. */
        if (pn4->pn_head)
            pn4->pn_pos.begin = pn4->pn_head->pn_pos.begin;
        pn3->pn_pos.end = pn4->pn_pos.end;
        pn3->pn_right = pn4;
    }

    /*
     * Handle the case where there was a let declaration in any case in
     * the switch body, but not within an inner block.  If it replaced
     * pc->blockNode with a new block node then we must refresh pn2 and
     * then restore pc->blockNode.
     */
    if (pc->blockNode != pn2)
        pn2 = pc->blockNode;
    pc->blockNode = saveBlock;
    PopStatementPC(context, pc);

    pn->pn_pos.end = pn2->pn_pos.end = tokenStream.currentToken().pos.end;
    pn->pn_left = pn1;
    pn->pn_right = pn2;
    return pn;
}

bool
Parser::matchInOrOf(bool *isForOfp)
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

static bool
IsValidForStatementLHS(ParseNode *pn1, JSVersion version, bool forDecl, bool forEach, bool forOf)
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
      case PNK_XMLUNARY:
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

ParseNode *
Parser::forStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    /* A FOR node is binary, left is loop control and right is the body. */
    ParseNode *pn = BinaryNode::create(PNK_FOR, this);
    if (!pn)
        return NULL;

    StmtInfoPC forStmt(context);
    PushStatementPC(pc, &forStmt, STMT_FOR_LOOP);

    pn->setOp(JSOP_ITER);
    pn->pn_iflags = 0;
    if (tokenStream.matchToken(TOK_NAME)) {
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
    Rooted<StaticBlockObject*> blockObj(context);

    /* Set to 'x' in 'for (x ;... ;...)' or 'for (x in ...)'. */
    ParseNode *pn1;

    {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt == TOK_SEMI) {
            if (pn->pn_iflags & JSITER_FOREACH) {
                reportError(pn, JSMSG_BAD_FOR_EACH_LOOP);
                return NULL;
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
                (void) tokenStream.getToken();
                if (tokenStream.peekToken() == TOK_LP) {
                    pn1 = letBlock(LetExpresion);
                } else {
                    forDecl = true;
                    blockObj = StaticBlockObject::create(context);
                    if (!blockObj)
                        return NULL;
                    pn1 = variables(PNK_LET, blockObj, DontHoistVars);
                }
            }
#endif
            else {
                pn1 = expr();
            }
            pc->parsingForInit = false;
            if (!pn1)
                return NULL;
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
            reportError(NULL, JSMSG_BAD_FOR_EACH_LOOP);
            return NULL;
        }
        pn->pn_iflags |= (forOf ? JSITER_FOR_OF : JSITER_ENUMERATE);

        /* Check that the left side of the 'in' or 'of' is valid. */
        bool forEach = bool(pn->pn_iflags & JSITER_FOREACH);
        if (!IsValidForStatementLHS(pn1, versionNumber(), forDecl, forEach, forOf)) {
            reportError(pn1, JSMSG_BAD_FOR_LEFTSIDE);
            return NULL;
        }

        /*
         * After the following if-else, pn2 will point to the name or
         * destructuring pattern on in's left. pn1 will point to the decl, if
         * any, else NULL. Note that the "declaration with initializer" case
         * rewrites the loop-head, moving the decl and setting pn1 to NULL.
         */
        pn2 = NULL;
        if (forDecl) {
            /* Tell EmitVariables that pn1 is part of a for/in. */
            pn1->pn_xflags |= PNX_FORINVAR;

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
                    reportError(pn2, JSMSG_INVALID_FOR_IN_INIT);
                    return NULL;
                }
#endif /* JS_HAS_BLOCK_SCOPE */

                ParseNode *pnseq = ListNode::create(PNK_SEQ, this);
                if (!pnseq)
                    return NULL;

                /*
                 * All of 'var x = i' is hoisted above 'for (x in o)',
                 * so clear PNX_FORINVAR.
                 *
                 * Request JSOP_POP here since the var is for a simple
                 * name (it is not a destructuring binding's left-hand
                 * side) and it has an initializer.
                 */
                pn1->pn_xflags &= ~PNX_FORINVAR;
                pn1->pn_xflags |= PNX_POPVAR;
                pnseq->initList(pn1);
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
                return NULL;
        }

        pn3 = expr();
        if (!pn3)
            return NULL;

        if (blockObj) {
            /*
             * Now that the pn3 has been parsed, push the let scope. To hold
             * the blockObj for the emitter, wrap the TOK_LEXICALSCOPE node
             * created by PushLetScope around the for's initializer. This also
             * serves to indicate the let-decl to the emitter.
             */
            ParseNode *block = PushLetScope(context, this, *blockObj, &letStmt);
            if (!block)
                return NULL;
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
            pn2 = CloneLeftHandSide(pn2, this);
            if (!pn2)
                return NULL;
        }

        switch (pn2->getKind()) {
          case PNK_NAME:
            /* Beware 'for (arguments in ...)' with or without a 'var'. */
            NoteLValue(pn2);
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

        forHead = TernaryNode::create(PNK_FORIN, this);
        if (!forHead)
            return NULL;
    } else {
        if (blockObj) {
            /*
             * Desugar 'for (let A; B; C) D' into 'let (A) { for (; B; C) D }'
             * to induce the correct scoping for A.
             */
            ParseNode *block = PushLetScope(context, this, *blockObj, &letStmt);
            if (!block)
                return NULL;
            letStmt.isForLetBlock = true;

            ParseNode *let = new_<BinaryNode>(PNK_LET, JSOP_NOP, pos, pn1, block);
            if (!let)
                return NULL;

            pn1 = NULL;
            block->pn_expr = pn;
            forParent = let;
        }

        if (pn->pn_iflags & JSITER_FOREACH) {
            reportError(pn, JSMSG_BAD_FOR_EACH_LOOP);
            return NULL;
        }
        pn->setOp(JSOP_NOP);

        /* Parse the loop condition or null into pn2. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_INIT);
        if (tokenStream.peekToken(TSF_OPERAND) == TOK_SEMI) {
            pn2 = NULL;
        } else {
            pn2 = expr();
            if (!pn2)
                return NULL;
        }

        /* Parse the update expression or null into pn3. */
        MUST_MATCH_TOKEN(TOK_SEMI, JSMSG_SEMI_AFTER_FOR_COND);
        if (tokenStream.peekToken(TSF_OPERAND) == TOK_RP) {
            pn3 = NULL;
        } else {
            pn3 = expr();
            if (!pn3)
                return NULL;
        }

        forHead = TernaryNode::create(PNK_FORHEAD, this);
        if (!forHead)
            return NULL;
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
        return NULL;

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

ParseNode *
Parser::tryStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_TRY));

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
    ParseNode *pn = TernaryNode::create(PNK_TRY, this);
    if (!pn)
        return NULL;
    pn->setOp(JSOP_NOP);

    MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_TRY);
    StmtInfoPC stmtInfo(context);
    if (!PushBlocklikeStatement(&stmtInfo, STMT_TRY, pc))
        return NULL;
    pn->pn_kid1 = statements();
    if (!pn->pn_kid1)
        return NULL;
    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_TRY);
    PopStatementPC(context, pc);

    ParseNode *lastCatch;
    ParseNode *catchList = NULL;
    TokenKind tt = tokenStream.getToken();
    if (tt == TOK_CATCH) {
        catchList = ListNode::create(PNK_CATCHLIST, this);
        if (!catchList)
            return NULL;
        catchList->makeEmpty();
        lastCatch = NULL;

        do {
            ParseNode *pnblock;
            BindData data(context);

            /* Check for another catch after unconditional catch. */
            if (lastCatch && !lastCatch->pn_kid2) {
                reportError(NULL, JSMSG_CATCH_AFTER_GENERAL);
                return NULL;
            }

            /*
             * Create a lexical scope node around the whole catch clause,
             * including the head.
             */
            pnblock = PushLexicalScope(context, this, &stmtInfo);
            if (!pnblock)
                return NULL;
            stmtInfo.type = STMT_CATCH;

            /*
             * Legal catch forms are:
             *   catch (lhs)
             *   catch (lhs if <boolean_expression>)
             * where lhs is a name or a destructuring left-hand side.
             * (the latter is legal only #ifdef JS_HAS_CATCH_GUARD)
             */
            ParseNode *pn2 = TernaryNode::create(PNK_CATCH, this);
            if (!pn2)
                return NULL;
            pnblock->pn_expr = pn2;
            MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_CATCH);

            /*
             * Contrary to ECMA Ed. 3, the catch variable is lexically
             * scoped, not a property of a new Object instance.  This is
             * an intentional change that anticipates ECMA Ed. 4.
             */
            data.initLet(HoistVars, *pc->blockChain, JSMSG_TOO_MANY_CATCH_VARS);
            JS_ASSERT(data.let.blockObj && data.let.blockObj == pnblock->pn_objbox->object);

            tt = tokenStream.getToken();
            ParseNode *pn3;
            switch (tt) {
#if JS_HAS_DESTRUCTURING
              case TOK_LB:
              case TOK_LC:
                pn3 = destructuringExpr(&data, tt);
                if (!pn3)
                    return NULL;
                break;
#endif

              case TOK_NAME:
              {
                RootedPropertyName label(context, tokenStream.currentToken().name());
                pn3 = NewBindingNode(label, this);
                if (!pn3)
                    return NULL;
                data.pn = pn3;
                if (!data.binder(context, &data, label, this))
                    return NULL;
                break;
              }

              default:
                reportError(NULL, JSMSG_CATCH_IDENTIFIER);
                return NULL;
            }

            pn2->pn_kid1 = pn3;
#if JS_HAS_CATCH_GUARD
            /*
             * We use 'catch (x if x === 5)' (not 'catch (x : x === 5)')
             * to avoid conflicting with the JS2/ECMAv4 type annotation
             * catchguard syntax.
             */
            if (tokenStream.matchToken(TOK_IF)) {
                pn2->pn_kid2 = expr();
                if (!pn2->pn_kid2)
                    return NULL;
            }
#endif
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_CATCH);

            MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_CATCH);
            pn2->pn_kid3 = statements();
            if (!pn2->pn_kid3)
                return NULL;
            MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_CATCH);
            PopStatementPC(context, pc);

            pnblock->pn_pos.end = pn2->pn_pos.end = tokenStream.currentToken().pos.end;

            catchList->append(pnblock);
            lastCatch = pn2;
            tt = tokenStream.getToken(TSF_OPERAND);
        } while (tt == TOK_CATCH);
    }
    pn->pn_kid2 = catchList;

    if (tt == TOK_FINALLY) {
        MUST_MATCH_TOKEN(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY);
        if (!PushBlocklikeStatement(&stmtInfo, STMT_FINALLY, pc))
            return NULL;
        pn->pn_kid3 = statements();
        if (!pn->pn_kid3)
            return NULL;
        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_AFTER_FINALLY);
        PopStatementPC(context, pc);
    } else {
        tokenStream.ungetToken();
    }
    if (!catchList && !pn->pn_kid3) {
        reportError(NULL, JSMSG_CATCH_OR_FINALLY);
        return NULL;
    }
    pn->pn_pos.end = (pn->pn_kid3 ? pn->pn_kid3 : catchList)->pn_pos.end;
    return pn;
}

ParseNode *
Parser::withStatement()
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_WITH));

    // In most cases, we want the constructs forbidden in strict mode code to be
    // a subset of those that JSOPTION_STRICT warns about, and we should use
    // reportStrictModeError.  However, 'with' is the sole instance of a
    // construct that is forbidden in strict mode code, but doesn't even merit a
    // warning under JSOPTION_STRICT.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=514576#c1.
    if (pc->sc->strict && !reportStrictModeError(NULL, JSMSG_STRICT_CODE_WITH))
        return NULL;

    ParseNode *pn = BinaryNode::create(PNK_WITH, this);
    if (!pn)
        return NULL;
    MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_BEFORE_WITH);
    ParseNode *pn2 = parenExpr();
    if (!pn2)
        return NULL;
    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_WITH);
    pn->pn_left = pn2;

    bool oldParsingWith = pc->parsingWith;
    pc->parsingWith = true;

    StmtInfoPC stmtInfo(context);
    PushStatementPC(pc, &stmtInfo, STMT_WITH);
    pn2 = statement();
    if (!pn2)
        return NULL;
    PopStatementPC(context, pc);

    pn->pn_pos.end = pn2->pn_pos.end;
    pn->pn_right = pn2;

    pc->sc->setBindingsAccessedDynamically();
    pc->parsingWith = oldParsingWith;

    /*
     * Make sure to deoptimize lexical dependencies inside the |with|
     * to safely optimize binding globals (see bug 561923).
     */
    for (AtomDefnRange r = pc->lexdeps->all(); !r.empty(); r.popFront()) {
        Definition *defn = r.front().value();
        Definition *lexdep = defn->resolve();
        DeoptimizeUsesWithin(lexdep, pn->pn_pos);
    }

    return pn;
}

#if JS_HAS_BLOCK_SCOPE
ParseNode *
Parser::letStatement()
{
    ParseNode *pn;
    do {
        /* Check for a let statement or let expression. */
        if (tokenStream.peekToken() == TOK_LP) {
            pn = letBlock(LetStatement);
            if (!pn)
                return NULL;

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
            reportError(NULL, JSMSG_LET_DECL_NOT_IN_BLOCK);
            return NULL;
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
                    return NULL;
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
                return NULL;

            ObjectBox *blockbox = newObjectBox(blockObj);
            if (!blockbox)
                return NULL;

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
            ParseNode *pn1 = LexicalScopeNode::create(PNK_LEXICALSCOPE, this);
            if (!pn1)
                return NULL;

            pn1->setOp(JSOP_LEAVEBLOCK);
            pn1->pn_pos = pc->blockNode->pn_pos;
            pn1->pn_objbox = blockbox;
            pn1->pn_expr = pc->blockNode;
            pn1->pn_blockid = pc->blockNode->pn_blockid;
            pc->blockNode = pn1;
        }

        pn = variables(PNK_LET, pc->blockChain, HoistVars);
        if (!pn)
            return NULL;
        pn->pn_xflags = PNX_POPVAR;
    } while (0);

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : NULL;
}
#endif

ParseNode *
Parser::expressionStatement()
{
    tokenStream.ungetToken();
    ParseNode *pn2 = expr();
    if (!pn2)
        return NULL;

    if (tokenStream.peekToken() == TOK_COLON) {
        if (!pn2->isKind(PNK_NAME)) {
            reportError(NULL, JSMSG_BAD_LABEL);
            return NULL;
        }
        JSAtom *label = pn2->pn_atom;
        for (StmtInfoPC *stmt = pc->topStmt; stmt; stmt = stmt->down) {
            if (stmt->type == STMT_LABEL && stmt->label == label) {
                reportError(NULL, JSMSG_DUPLICATE_LABEL);
                return NULL;
            }
        }
        ForgetUse(pn2);

        (void) tokenStream.getToken();

        /* Push a label struct and parse the statement. */
        StmtInfoPC stmtInfo(context);
        PushStatementPC(pc, &stmtInfo, STMT_LABEL);
        stmtInfo.label = label;
        ParseNode *pn = statement();
        if (!pn)
            return NULL;

        /* Normalize empty statement to empty block for the decompiler. */
        if (pn->isKind(PNK_SEMI) && !pn->pn_kid) {
            pn->setKind(PNK_STATEMENTLIST);
            pn->setArity(PN_LIST);
            pn->makeEmpty();
        }

        /* Pop the label, set pn_expr, and return early. */
        PopStatementPC(context, pc);
        pn2->setKind(PNK_COLON);
        pn2->pn_pos.end = pn->pn_pos.end;
        pn2->pn_expr = pn;
        return pn2;
    }

    ParseNode *pn = UnaryNode::create(PNK_SEMI, this);
    if (!pn)
        return NULL;
    pn->pn_pos = pn2->pn_pos;
    pn->pn_kid = pn2;

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : NULL;
}

ParseNode *
Parser::statement()
{
    ParseNode *pn;

    JS_CHECK_RECURSION(context, return NULL);

    switch (tokenStream.getToken(TSF_OPERAND)) {
      case TOK_FUNCTION:
      {
#if JS_HAS_XML_SUPPORT
        if (allowsXML()) {
            TokenKind tt = tokenStream.peekToken(TSF_KEYWORD_IS_NAME);
            if (tt == TOK_DBLCOLON)
                return expressionStatement();
        }
#endif
        return functionStmt();
      }

      case TOK_IF:
      {
        /* An IF node has three kids: condition, then, and optional else. */
        pn = TernaryNode::create(PNK_IF, this);
        if (!pn)
            return NULL;
        ParseNode *pn1 = condition();
        if (!pn1)
            return NULL;

        StmtInfoPC stmtInfo(context);
        PushStatementPC(pc, &stmtInfo, STMT_IF);
        ParseNode *pn2 = statement();
        if (!pn2)
            return NULL;

        if (pn2->isKind(PNK_SEMI) &&
            !pn2->pn_kid &&
            !reportStrictWarning(NULL, JSMSG_EMPTY_CONSEQUENT))
        {
            return NULL;
        }

        ParseNode *pn3;
        if (tokenStream.matchToken(TOK_ELSE, TSF_OPERAND)) {
            stmtInfo.type = STMT_ELSE;
            pn3 = statement();
            if (!pn3)
                return NULL;
            pn->pn_pos.end = pn3->pn_pos.end;
        } else {
            pn3 = NULL;
            pn->pn_pos.end = pn2->pn_pos.end;
        }
        PopStatementPC(context, pc);
        pn->pn_kid1 = pn1;
        pn->pn_kid2 = pn2;
        pn->pn_kid3 = pn3;
        return pn;
      }

      case TOK_SWITCH:
        return switchStatement();

      case TOK_WHILE:
      {
        pn = BinaryNode::create(PNK_WHILE, this);
        if (!pn)
            return NULL;
        StmtInfoPC stmtInfo(context);
        PushStatementPC(pc, &stmtInfo, STMT_WHILE_LOOP);
        ParseNode *pn2 = condition();
        if (!pn2)
            return NULL;
        pn->pn_left = pn2;
        ParseNode *pn3 = statement();
        if (!pn3)
            return NULL;
        PopStatementPC(context, pc);
        pn->pn_pos.end = pn3->pn_pos.end;
        pn->pn_right = pn3;
        return pn;
      }

      case TOK_DO:
      {
        pn = BinaryNode::create(PNK_DOWHILE, this);
        if (!pn)
            return NULL;
        StmtInfoPC stmtInfo(context);
        PushStatementPC(pc, &stmtInfo, STMT_DO_LOOP);
        ParseNode *pn2 = statement();
        if (!pn2)
            return NULL;
        pn->pn_left = pn2;
        MUST_MATCH_TOKEN(TOK_WHILE, JSMSG_WHILE_AFTER_DO);
        ParseNode *pn3 = condition();
        if (!pn3)
            return NULL;
        PopStatementPC(context, pc);
        pn->pn_pos.end = pn3->pn_pos.end;
        pn->pn_right = pn3;
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
        pn = UnaryNode::create(PNK_THROW, this);
        if (!pn)
            return NULL;

        /* ECMA-262 Edition 3 says 'throw [no LineTerminator here] Expr'. */
        TokenKind tt = tokenStream.peekTokenSameLine(TSF_OPERAND);
        if (tt == TOK_ERROR)
            return NULL;
        if (tt == TOK_EOF || tt == TOK_EOL || tt == TOK_SEMI || tt == TOK_RC) {
            reportError(NULL, JSMSG_SYNTAX_ERROR);
            return NULL;
        }

        ParseNode *pn2 = expr();
        if (!pn2)
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->setOp(JSOP_THROW);
        pn->pn_kid = pn2;
        break;
      }

      /* TOK_CATCH and TOK_FINALLY are both handled in the TOK_TRY case */
      case TOK_CATCH:
        reportError(NULL, JSMSG_CATCH_WITHOUT_TRY);
        return NULL;

      case TOK_FINALLY:
        reportError(NULL, JSMSG_FINALLY_WITHOUT_TRY);
        return NULL;

      case TOK_BREAK:
      {
        TokenPtr begin = tokenStream.currentToken().pos.begin;
        PropertyName *label;
        if (!MatchLabel(context, &tokenStream, &label))
            return NULL;
        TokenPtr end = tokenStream.currentToken().pos.end;
        pn = new_<BreakStatement>(label, begin, end);
        if (!pn)
            return NULL;
        StmtInfoPC *stmt = pc->topStmt;
        if (label) {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    reportError(NULL, JSMSG_LABEL_NOT_FOUND);
                    return NULL;
                }
                if (stmt->type == STMT_LABEL && stmt->label == label)
                    break;
            }
        } else {
            for (; ; stmt = stmt->down) {
                if (!stmt) {
                    reportError(NULL, JSMSG_TOUGH_BREAK);
                    return NULL;
                }
                if (stmt->isLoop() || stmt->type == STMT_SWITCH)
                    break;
            }
        }
        break;
      }

      case TOK_CONTINUE:
      {
        TokenPtr begin = tokenStream.currentToken().pos.begin;
        PropertyName *label;
        if (!MatchLabel(context, &tokenStream, &label))
            return NULL;
        TokenPtr end = tokenStream.currentToken().pos.begin;
        pn = new_<ContinueStatement>(label, begin, end);
        if (!pn)
            return NULL;
        StmtInfoPC *stmt = pc->topStmt;
        if (label) {
            for (StmtInfoPC *stmt2 = NULL; ; stmt = stmt->down) {
                if (!stmt) {
                    reportError(NULL, JSMSG_LABEL_NOT_FOUND);
                    return NULL;
                }
                if (stmt->type == STMT_LABEL) {
                    if (stmt->label == label) {
                        if (!stmt2 || !stmt2->isLoop()) {
                            reportError(NULL, JSMSG_BAD_CONTINUE);
                            return NULL;
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
                    reportError(NULL, JSMSG_BAD_CONTINUE);
                    return NULL;
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
            return NULL;

        /* Tell js_EmitTree to generate a final POP. */
        pn->pn_xflags |= PNX_POPVAR;
        break;

      case TOK_CONST:
        pn = variables(PNK_CONST);
        if (!pn)
            return NULL;

        /* Tell js_EmitTree to generate a final POP. */
        pn->pn_xflags |= PNX_POPVAR;
        break;

#if JS_HAS_BLOCK_SCOPE
      case TOK_LET:
        return letStatement();
#endif /* JS_HAS_BLOCK_SCOPE */

      case TOK_RETURN:
        pn = returnOrYield(false);
        if (!pn)
            return NULL;
        break;

      case TOK_LC:
      {
        StmtInfoPC stmtInfo(context);
        if (!PushBlocklikeStatement(&stmtInfo, STMT_BLOCK, pc))
            return NULL;
        bool hasFunctionStmt;
        pn = statements(&hasFunctionStmt);
        if (!pn)
            return NULL;

        MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_IN_COMPOUND);
        PopStatementPC(context, pc);

        /*
         * If we contain a function statement and our container is top-level
         * or another block, flag pn to preserve braces when decompiling.
         */
        if (hasFunctionStmt && (!pc->topStmt || pc->topStmt->type == STMT_BLOCK))
            pn->pn_xflags |= PNX_NEEDBRACES;

        return pn;
      }

      case TOK_SEMI:
        pn = UnaryNode::create(PNK_SEMI, this);
        if (!pn)
            return NULL;
        return pn;

      case TOK_DEBUGGER:
        pn = new_<DebuggerStatement>(tokenStream.currentToken().pos);
        if (!pn)
            return NULL;
        pc->sc->setBindingsAccessedDynamically();
        break;

#if JS_HAS_XML_SUPPORT
      case TOK_DEFAULT:
      {
        if (!allowsXML())
            return expressionStatement();

        pn = UnaryNode::create(PNK_DEFXMLNS, this);
        if (!pn)
            return NULL;
        if (!tokenStream.matchToken(TOK_NAME) ||
            tokenStream.currentToken().name() != context->names().xml ||
            !tokenStream.matchToken(TOK_NAME) ||
            tokenStream.currentToken().name() != context->names().namespace_ ||
            !tokenStream.matchToken(TOK_ASSIGN))
        {
            reportError(NULL, JSMSG_BAD_DEFAULT_XML_NAMESPACE);
            return NULL;
        }

        JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NOP);

        /* Is this an E4X dagger I see before me? */
        pc->sc->setBindingsAccessedDynamically();
        ParseNode *pn2 = expr();
        if (!pn2)
            return NULL;
        pn->setOp(JSOP_DEFXMLNS);
        pn->pn_pos.end = pn2->pn_pos.end;
        pn->pn_kid = pn2;
        break;
      }
#endif

      case TOK_ERROR:
        return NULL;

      default:
        return expressionStatement();
    }

    /* Check termination of this primitive statement. */
    return MatchOrInsertSemicolon(context, &tokenStream) ? pn : NULL;
}

/*
 * The 'blockObj' parameter is non-null when parsing the 'vars' in a let
 * expression, block statement, non-top-level let declaration in statement
 * context, and the let-initializer of a for-statement.
 */
ParseNode *
Parser::variables(ParseNodeKind kind, StaticBlockObject *blockObj, VarContext varContext)
{
    /*
     * The four options here are:
     * - PNK_VAR:   We're parsing var declarations.
     * - PNK_CONST: We're parsing const declarations.
     * - PNK_LET:   We are parsing a let declaration.
     * - PNK_CALL:  We are parsing the head of a let block.
     */
    JS_ASSERT(kind == PNK_VAR || kind == PNK_CONST || kind == PNK_LET || kind == PNK_CALL);

    ParseNode *pn = ListNode::create(kind, this);
    if (!pn)
        return NULL;

    pn->setOp(blockObj ? JSOP_NOP : kind == PNK_VAR ? JSOP_DEFVAR : JSOP_DEFCONST);
    pn->makeEmpty();

    /*
     * SpiderMonkey const is really "write once per initialization evaluation"
     * var, whereas let is block scoped. ES-Harmony wants block-scoped const so
     * this code will change soon.
     */
    BindData data(context);
    if (blockObj)
        data.initLet(varContext, *blockObj, JSMSG_TOO_MANY_LOCALS);
    else
        data.initVarOrConst(pn->getOp());

    ParseNode *pn2;
    do {
        TokenKind tt = tokenStream.getToken();
#if JS_HAS_DESTRUCTURING
        if (tt == TOK_LB || tt == TOK_LC) {
            pc->inDeclDestructuring = true;
            pn2 = primaryExpr(tt, false);
            pc->inDeclDestructuring = false;
            if (!pn2)
                return NULL;

            if (!CheckDestructuring(context, &data, pn2, this))
                return NULL;
            bool ignored;
            if (pc->parsingForInit && matchInOrOf(&ignored)) {
                tokenStream.ungetToken();
                pn->append(pn2);
                continue;
            }

            MUST_MATCH_TOKEN(TOK_ASSIGN, JSMSG_BAD_DESTRUCT_DECL);
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NOP);

            ParseNode *init = assignExpr();
            if (!init)
                return NULL;

            pn2 = ParseNode::newBinaryOrAppend(PNK_ASSIGN, JSOP_NOP, pn2, init, this);
            if (!pn2)
                return NULL;
            pn->append(pn2);
            continue;
        }
#endif /* JS_HAS_DESTRUCTURING */

        if (tt != TOK_NAME) {
            if (tt != TOK_ERROR)
                reportError(NULL, JSMSG_NO_VARIABLE_NAME);
            return NULL;
        }

        RootedPropertyName name(context, tokenStream.currentToken().name());
        pn2 = NewBindingNode(name, this, varContext);
        if (!pn2)
            return NULL;
        if (data.op == JSOP_DEFCONST)
            pn2->pn_dflags |= PND_CONST;
        data.pn = pn2;
        if (!data.binder(context, &data, name, this))
            return NULL;
        pn->append(pn2);

        if (tokenStream.matchToken(TOK_ASSIGN)) {
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NOP);

            ParseNode *init = assignExpr();
            if (!init)
                return NULL;

            if (pn2->isUsed()) {
                pn2 = MakeAssignment(pn2, init, this);
                if (!pn2)
                    return NULL;
            } else {
                pn2->pn_expr = init;
            }

            pn2->setOp((pn2->pn_dflags & PND_BOUND)
                       ? JSOP_SETLOCAL
                       : (data.op == JSOP_DEFCONST)
                       ? JSOP_SETCONST
                       : JSOP_SETNAME);

            NoteLValue(pn2);

            /* The declarator's position must include the initializer. */
            pn2->pn_pos.end = init->pn_pos.end;
        }
    } while (tokenStream.matchToken(TOK_COMMA));

    pn->pn_pos.end = pn->last()->pn_pos.end;
    return pn;
}

ParseNode *
Parser::expr()
{
    ParseNode *pn = assignExpr();
    if (pn && tokenStream.matchToken(TOK_COMMA)) {
        ParseNode *pn2 = ListNode::create(PNK_COMMA, this);
        if (!pn2)
            return NULL;
        pn2->pn_pos.begin = pn->pn_pos.begin;
        pn2->initList(pn);
        pn = pn2;
        do {
#if JS_HAS_GENERATORS
            pn2 = pn->last();
            if (pn2->isKind(PNK_YIELD) && !pn2->isInParens()) {
                reportError(pn2, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
                return NULL;
            }
#endif
            pn2 = assignExpr();
            if (!pn2)
                return NULL;
            pn->append(pn2);
        } while (tokenStream.matchToken(TOK_COMMA));
        pn->pn_pos.end = pn->last()->pn_pos.end;
    }
    return pn;
}

/*
 * For a number of the expression parsers we define an always-inlined version
 * and a never-inlined version (which just calls the always-inlined version).
 * Using the always-inlined version in the hot call-sites givs a ~5% parsing
 * speedup.  These macros help avoid some boilerplate code.
 */
#define BEGIN_EXPR_PARSER(name)                                               \
    JS_ALWAYS_INLINE ParseNode *                                              \
    Parser::name##i()

#define END_EXPR_PARSER(name)                                                 \
    JS_NEVER_INLINE ParseNode *                                               \
    Parser::name##n() {                                                       \
        return name##i();                                                     \
    }

BEGIN_EXPR_PARSER(mulExpr1)
{
    ParseNode *pn = unaryExpr();

    /*
     * Note: unlike addExpr1() et al, we use getToken() here instead of
     * isCurrentTokenType() because unaryExpr() doesn't leave the TokenStream
     * state one past the end of the unary expression.
     */
    TokenKind tt;
    while (pn && ((tt = tokenStream.getToken()) == TOK_STAR || tt == TOK_DIV || tt == TOK_MOD)) {
        ParseNodeKind kind = (tt == TOK_STAR)
                             ? PNK_STAR
                             : (tt == TOK_DIV)
                             ? PNK_DIV
                             : PNK_MOD;
        JSOp op = tokenStream.currentToken().t_op;
        pn = ParseNode::newBinaryOrAppend(kind, op, pn, unaryExpr(), this);
    }
    return pn;
}
END_EXPR_PARSER(mulExpr1)

BEGIN_EXPR_PARSER(addExpr1)
{
    ParseNode *pn = mulExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_PLUS, TOK_MINUS)) {
        TokenKind tt = tokenStream.currentToken().type;
        JSOp op = (tt == TOK_PLUS) ? JSOP_ADD : JSOP_SUB;
        ParseNodeKind kind = (tt == TOK_PLUS) ? PNK_ADD : PNK_SUB;
        pn = ParseNode::newBinaryOrAppend(kind, op, pn, mulExpr1n(), this);
    }
    return pn;
}
END_EXPR_PARSER(addExpr1)

inline ParseNodeKind
ShiftTokenToParseNodeKind(const Token &token)
{
    switch (token.type) {
      case TOK_LSH:
        return PNK_LSH;
      case TOK_RSH:
        return PNK_RSH;
      default:
        JS_ASSERT(token.type == TOK_URSH);
        return PNK_URSH;
    }
}

BEGIN_EXPR_PARSER(shiftExpr1)
{
    ParseNode *left = addExpr1i();
    while (left && tokenStream.isCurrentTokenShift()) {
        ParseNodeKind kind = ShiftTokenToParseNodeKind(tokenStream.currentToken());
        JSOp op = tokenStream.currentToken().t_op;
        ParseNode *right = addExpr1n();
        if (!right)
            return NULL;
        left = new_<BinaryNode>(kind, op, left, right);
    }
    return left;
}
END_EXPR_PARSER(shiftExpr1)

inline ParseNodeKind
RelationalTokenToParseNodeKind(const Token &token)
{
    switch (token.type) {
      case TOK_IN:
        return PNK_IN;
      case TOK_INSTANCEOF:
        return PNK_INSTANCEOF;
      case TOK_LT:
        return PNK_LT;
      case TOK_LE:
        return PNK_LE;
      case TOK_GT:
        return PNK_GT;
      default:
        JS_ASSERT(token.type == TOK_GE);
        return PNK_GE;
    }
}

BEGIN_EXPR_PARSER(relExpr1)
{
    /*
     * Uses of the in operator in shiftExprs are always unambiguous,
     * so unset the flag that prohibits recognizing it.
     */
    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;

    ParseNode *pn = shiftExpr1i();
    while (pn &&
           (tokenStream.isCurrentTokenRelational() ||
            /*
             * Recognize the 'in' token as an operator only if we're not
             * currently in the init expr of a for loop.
             */
            (oldParsingForInit == 0 && tokenStream.isCurrentTokenType(TOK_IN)) ||
            tokenStream.isCurrentTokenType(TOK_INSTANCEOF))) {
        ParseNodeKind kind = RelationalTokenToParseNodeKind(tokenStream.currentToken());
        JSOp op = tokenStream.currentToken().t_op;
        pn = ParseNode::newBinaryOrAppend(kind, op, pn, shiftExpr1n(), this);
    }
    /* Restore previous state of parsingForInit flag. */
    pc->parsingForInit |= oldParsingForInit;

    return pn;
}
END_EXPR_PARSER(relExpr1)

inline ParseNodeKind
EqualityTokenToParseNodeKind(const Token &token)
{
    switch (token.type) {
      case TOK_STRICTEQ:
        return PNK_STRICTEQ;
      case TOK_EQ:
        return PNK_EQ;
      case TOK_STRICTNE:
        return PNK_STRICTNE;
      default:
        JS_ASSERT(token.type == TOK_NE);
        return PNK_NE;
    }
}

BEGIN_EXPR_PARSER(eqExpr1)
{
    ParseNode *left = relExpr1i();
    while (left && tokenStream.isCurrentTokenEquality()) {
        ParseNodeKind kind = EqualityTokenToParseNodeKind(tokenStream.currentToken());
        JSOp op = tokenStream.currentToken().t_op;
        ParseNode *right = relExpr1n();
        if (!right)
            return NULL;
        left = new_<BinaryNode>(kind, op, left, right);
    }
    return left;
}
END_EXPR_PARSER(eqExpr1)

BEGIN_EXPR_PARSER(bitAndExpr1)
{
    ParseNode *pn = eqExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_BITAND))
        pn = ParseNode::newBinaryOrAppend(PNK_BITAND, JSOP_BITAND, pn, eqExpr1n(), this);
    return pn;
}
END_EXPR_PARSER(bitAndExpr1)

BEGIN_EXPR_PARSER(bitXorExpr1)
{
    ParseNode *pn = bitAndExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_BITXOR))
        pn = ParseNode::newBinaryOrAppend(PNK_BITXOR, JSOP_BITXOR, pn, bitAndExpr1n(), this);
    return pn;
}
END_EXPR_PARSER(bitXorExpr1)

BEGIN_EXPR_PARSER(bitOrExpr1)
{
    ParseNode *pn = bitXorExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_BITOR))
        pn = ParseNode::newBinaryOrAppend(PNK_BITOR, JSOP_BITOR, pn, bitXorExpr1n(), this);
    return pn;
}
END_EXPR_PARSER(bitOrExpr1)

BEGIN_EXPR_PARSER(andExpr1)
{
    ParseNode *pn = bitOrExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_AND))
        pn = ParseNode::newBinaryOrAppend(PNK_AND, JSOP_AND, pn, bitOrExpr1n(), this);
    return pn;
}
END_EXPR_PARSER(andExpr1)

JS_ALWAYS_INLINE ParseNode *
Parser::orExpr1()
{
    ParseNode *pn = andExpr1i();
    while (pn && tokenStream.isCurrentTokenType(TOK_OR))
        pn = ParseNode::newBinaryOrAppend(PNK_OR, JSOP_OR, pn, andExpr1n(), this);
    return pn;
}

JS_ALWAYS_INLINE ParseNode *
Parser::condExpr1()
{
    ParseNode *condition = orExpr1();
    if (!condition || !tokenStream.isCurrentTokenType(TOK_HOOK))
        return condition;

    /*
     * Always accept the 'in' operator in the middle clause of a ternary,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;
    ParseNode *thenExpr = assignExpr();
    pc->parsingForInit = oldParsingForInit;
    if (!thenExpr)
        return NULL;

    MUST_MATCH_TOKEN(TOK_COLON, JSMSG_COLON_IN_COND);

    ParseNode *elseExpr = assignExpr();
    if (!elseExpr)
        return NULL;

    tokenStream.getToken(); /* read one token past the end */
    return new_<ConditionalExpression>(condition, thenExpr, elseExpr);
}

bool
Parser::setAssignmentLhsOps(ParseNode *pn, JSOp op)
{
    switch (pn->getKind()) {
      case PNK_NAME:
        if (!CheckStrictAssignment(context, this, pn))
            return false;
        pn->setOp(pn->isOp(JSOP_GETLOCAL) ? JSOP_SETLOCAL : JSOP_SETNAME);
        NoteLValue(pn);
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
            reportError(NULL, JSMSG_BAD_DESTRUCT_ASS);
            return false;
        }
        if (!CheckDestructuring(context, NULL, pn, this))
            return false;
        break;
#endif
      case PNK_CALL:
        if (!MakeSetCall(context, pn, this, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return false;
        break;
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(pn->isOp(JSOP_XMLNAME));
        pn->setOp(JSOP_SETXMLNAME);
        break;
#endif
      default:
        reportError(NULL, JSMSG_BAD_LEFTSIDE_OF_ASS);
        return false;
    }
    return true;
}

ParseNode *
Parser::assignExpr()
{
    JS_CHECK_RECURSION(context, return NULL);

#if JS_HAS_GENERATORS
    if (tokenStream.matchToken(TOK_YIELD, TSF_OPERAND))
        return returnOrYield(true);
#endif

    ParseNode *lhs = condExpr1();
    if (!lhs)
        return NULL;

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
      default:
        JS_ASSERT(!tokenStream.isCurrentTokenAssignment());
        tokenStream.ungetToken();
        return lhs;
    }

    JSOp op = tokenStream.currentToken().t_op;
    if (!setAssignmentLhsOps(lhs, op))
        return NULL;

    ParseNode *rhs = assignExpr();
    if (!rhs)
        return NULL;

    return ParseNode::newBinaryOrAppend(kind, op, lhs, rhs, this);
}

static bool
SetLvalKid(JSContext *cx, Parser *parser, ParseNode *pn, ParseNode *kid,
           const char *name)
{
    if (!kid->isKind(PNK_NAME) &&
        !kid->isKind(PNK_DOT) &&
        (!kid->isKind(PNK_CALL) ||
         (!kid->isOp(JSOP_CALL) && !kid->isOp(JSOP_EVAL) &&
          !kid->isOp(JSOP_FUNCALL) && !kid->isOp(JSOP_FUNAPPLY))) &&
#if JS_HAS_XML_SUPPORT
        !kid->isKind(PNK_XMLUNARY) &&
#endif
        !kid->isKind(PNK_ELEM))
    {
        parser->reportError(NULL, JSMSG_BAD_OPERAND, name);
        return false;
    }
    if (!CheckStrictAssignment(cx, parser, kid))
        return false;
    pn->pn_kid = kid;
    return true;
}

static const char incop_name_str[][10] = {"increment", "decrement"};

static bool
SetIncOpKid(JSContext *cx, Parser *parser, ParseNode *pn, ParseNode *kid,
            TokenKind tt, bool preorder)
{
    JSOp op;

    if (!SetLvalKid(cx, parser, pn, kid, incop_name_str[tt == TOK_DEC]))
        return false;
    switch (kid->getKind()) {
      case PNK_NAME:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCNAME : JSOP_NAMEINC)
             : (preorder ? JSOP_DECNAME : JSOP_NAMEDEC);
        NoteLValue(kid);
        break;

      case PNK_DOT:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCPROP : JSOP_PROPINC)
             : (preorder ? JSOP_DECPROP : JSOP_PROPDEC);
        break;

      case PNK_CALL:
        if (!MakeSetCall(cx, kid, parser, JSMSG_BAD_INCOP_OPERAND))
            return false;
        /* FALL THROUGH */
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        if (kid->isOp(JSOP_XMLNAME))
            kid->setOp(JSOP_SETXMLNAME);
        /* FALL THROUGH */
#endif
      case PNK_ELEM:
        op = (tt == TOK_INC)
             ? (preorder ? JSOP_INCELEM : JSOP_ELEMINC)
             : (preorder ? JSOP_DECELEM : JSOP_ELEMDEC);
        break;

      default:
        JS_ASSERT(0);
        op = JSOP_NOP;
    }
    pn->setOp(op);
    return true;
}

ParseNode *
Parser::unaryOpExpr(ParseNodeKind kind, JSOp op)
{
    TokenPtr begin = tokenStream.currentToken().pos.begin;
    ParseNode *kid = unaryExpr();
    if (!kid)
        return NULL;
    return new_<UnaryNode>(kind, op, TokenPos::make(begin, kid->pn_pos.end), kid);
}

ParseNode *
Parser::unaryExpr()
{
    ParseNode *pn, *pn2;

    JS_CHECK_RECURSION(context, return NULL);

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
        pn = UnaryNode::create((tt == TOK_INC) ? PNK_PREINCREMENT : PNK_PREDECREMENT, this);
        if (!pn)
            return NULL;
        pn2 = memberExpr(true);
        if (!pn2)
            return NULL;
        if (!SetIncOpKid(context, this, pn, pn2, tt, true))
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;
        break;

      case TOK_DELETE:
      {
        pn = UnaryNode::create(PNK_DELETE, this);
        if (!pn)
            return NULL;
        pn2 = unaryExpr();
        if (!pn2)
            return NULL;
        pn->pn_pos.end = pn2->pn_pos.end;

        /*
         * Under ECMA3, deleting any unary expression is valid -- it simply
         * returns true. Here we fold constants before checking for a call
         * expression, in order to rule out delete of a generator expression.
         */
        if (foldConstants && !FoldConstants(context, pn2, this))
            return NULL;
        switch (pn2->getKind()) {
          case PNK_CALL:
            if (!(pn2->pn_xflags & PNX_SETCALL)) {
                /*
                 * Call MakeSetCall to check for errors, but clear PNX_SETCALL
                 * because the optimizer will eliminate the useless delete.
                 */
                if (!MakeSetCall(context, pn2, this, JSMSG_BAD_DELETE_OPERAND))
                    return NULL;
                pn2->pn_xflags &= ~PNX_SETCALL;
            }
            break;
          case PNK_NAME:
            if (!reportStrictModeError(pn, JSMSG_DEPRECATED_DELETE_OPERAND))
                return NULL;
            pc->sc->setBindingsAccessedDynamically();
            pn2->pn_dflags |= PND_DEOPTIMIZED;
            pn2->setOp(JSOP_DELNAME);
            break;
          default:;
        }
        pn->pn_kid = pn2;
        break;
      }
      case TOK_ERROR:
        return NULL;

      default:
        tokenStream.ungetToken();
        pn = memberExpr(true);
        if (!pn)
            return NULL;

        /* Don't look across a newline boundary for a postfix incop. */
        if (tokenStream.onCurrentLine(pn->pn_pos)) {
            tt = tokenStream.peekTokenSameLine(TSF_OPERAND);
            if (tt == TOK_INC || tt == TOK_DEC) {
                tokenStream.consumeKnownToken(tt);
                pn2 = UnaryNode::create((tt == TOK_INC) ? PNK_POSTINCREMENT : PNK_POSTDECREMENT, this);
                if (!pn2)
                    return NULL;
                if (!SetIncOpKid(context, this, pn2, pn, tt, false))
                    return NULL;
                pn2->pn_pos.begin = pn->pn_pos.begin;
                pn = pn2;
            }
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
class CompExprTransplanter {
    ParseNode       *root;
    Parser          *parser;
    bool            genexp;
    unsigned        adjust;
    HashSet<Definition *> visitedImplicitArguments;

  public:
    CompExprTransplanter(ParseNode *pn, Parser *parser, bool ge, unsigned adj)
      : root(pn), parser(parser), genexp(ge), adjust(adj),
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
class GenexpGuard {
    Parser          *parser;
    uint32_t        startYieldCount;

  public:
    explicit GenexpGuard(Parser *parser)
      : parser(parser)
    {
        ParseContext *pc = parser->pc;
        if (pc->parenDepth == 0) {
            pc->yieldCount = 0;
            pc->yieldNode = NULL;
        }
        startYieldCount = pc->yieldCount;
        pc->parenDepth++;
    }

    void endBody();
    bool checkValidBody(ParseNode *pn, unsigned err);
    bool maybeNoteGenerator(ParseNode *pn);
};

void
GenexpGuard::endBody()
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
bool
GenexpGuard::checkValidBody(ParseNode *pn, unsigned err = JSMSG_BAD_GENEXP_BODY)
{
    ParseContext *pc = parser->pc;
    if (pc->yieldCount > startYieldCount) {
        ParseNode *errorNode = pc->yieldNode;
        if (!errorNode)
            errorNode = pn;
        parser->reportError(errorNode, err, js_yield_str);
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
bool
GenexpGuard::maybeNoteGenerator(ParseNode *pn)
{
    ParseContext *pc = parser->pc;
    if (pc->yieldCount > 0) {
        if (!pc->sc->isFunction) {
            parser->reportError(NULL, JSMSG_BAD_RETURN_OR_YIELD, js_yield_str);
            return false;
        }
        pc->sc->asFunbox()->setIsGenerator();
        if (pc->funHasReturnExpr) {
            /* At the time we saw the yield, we might not have set isGenerator yet. */
            ReportBadReturn(pc->sc->context, parser, pn, &Parser::reportError,
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
static bool
BumpStaticLevel(ParseNode *pn, ParseContext *pc)
{
    if (pn->pn_cookie.isFree())
        return true;

    unsigned level = unsigned(pn->pn_cookie.level()) + 1;
    JS_ASSERT(level >= pc->staticLevel);
    return pn->pn_cookie.set(pc->sc->context, level, pn->pn_cookie.slot());
}

static bool
AdjustBlockId(ParseNode *pn, unsigned adjust, ParseContext *pc)
{
    JS_ASSERT(pn->isArity(PN_LIST) || pn->isArity(PN_FUNC) || pn->isArity(PN_NAME));
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
    ParseContext *pc = parser->pc;

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

      case PN_FUNC:
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
                    Definition *dn2 = MakePlaceholder(pn, parser, parser->pc);
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
                    if (!pc->lexdeps->put(atom, dn2))
                        return false;
                    if (dn->isClosed())
                        dn2->pn_dflags |= PND_CLOSED;
                } else if (dn->isPlaceholder()) {
                    /*
                     * The variable first occurs free in the 'yield' expression;
                     * move the existing placeholder node (and all its uses)
                     * from the parent's lexdeps into the generator's lexdeps.
                     */
                    pc->parent->lexdeps->remove(atom);
                    if (!pc->lexdeps->put(atom, dn))
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
ParseNode *
Parser::comprehensionTail(ParseNode *kid, unsigned blockid, bool isGenexp,
                          ParseNodeKind kind, JSOp op)
{
    unsigned adjust;
    ParseNode *pn, *pn2, *pn3, **pnp;
    StmtInfoPC stmtInfo(context);
    BindData data(context);
    TokenKind tt;

    JS_ASSERT(tokenStream.currentToken().type == TOK_FOR);

    if (kind == PNK_SEMI) {
        /*
         * Generator expression desugars to an immediately applied lambda that
         * yields the next value from a for-in loop (possibly nested, and with
         * optional if guard). Make pn be the TOK_LC body node.
         */
        pn = PushLexicalScope(context, this, &stmtInfo);
        if (!pn)
            return NULL;
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
        pn = PushLexicalScope(context, this, &stmtInfo);
        if (!pn)
            return NULL;

        JS_ASSERT(blockid <= pn->pn_blockid);
        JS_ASSERT(blockid < pc->blockidGen);
        JS_ASSERT(pc->bodyid < blockid);
        pn->pn_blockid = stmtInfo.blockid = blockid;
        JS_ASSERT(adjust < blockid);
        adjust = blockid - adjust;
    }

    pnp = &pn->pn_expr;

    CompExprTransplanter transplanter(kid, this, kind == PNK_SEMI, adjust);
    if (!transplanter.init())
        return NULL;

    if (!transplanter.transplant(kid))
        return NULL;

    JS_ASSERT(pc->blockChain && pc->blockChain == pn->pn_objbox->object);
    data.initLet(HoistVars, *pc->blockChain, JSMSG_ARRAY_INIT_TOO_BIG);

    do {
        /*
         * FOR node is binary, left is loop control and right is body.  Use
         * index to count each block-local let-variable on the left-hand side
         * of the in/of.
         */
        pn2 = BinaryNode::create(PNK_FOR, this);
        if (!pn2)
            return NULL;

        pn2->setOp(JSOP_ITER);
        pn2->pn_iflags = JSITER_ENUMERATE;
        if (tokenStream.matchToken(TOK_NAME)) {
            if (tokenStream.currentToken().name() == context->names().each)
                pn2->pn_iflags |= JSITER_FOREACH;
            else
                tokenStream.ungetToken();
        }
        MUST_MATCH_TOKEN(TOK_LP, JSMSG_PAREN_AFTER_FOR);

        GenexpGuard guard(this);

        RootedPropertyName name(context);
        tt = tokenStream.getToken();
        switch (tt) {
#if JS_HAS_DESTRUCTURING
          case TOK_LB:
          case TOK_LC:
            pc->inDeclDestructuring = true;
            pn3 = primaryExpr(tt, false);
            pc->inDeclDestructuring = false;
            if (!pn3)
                return NULL;
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
            pn3 = NewBindingNode(name, this);
            if (!pn3)
                return NULL;
            break;

          default:
            reportError(NULL, JSMSG_NO_VARIABLE_NAME);

          case TOK_ERROR:
            return NULL;
        }

        bool forOf;
        if (!matchInOrOf(&forOf)) {
            reportError(NULL, JSMSG_IN_AFTER_FOR_NAME);
            return NULL;
        }
        if (forOf) {
            if (pn2->pn_iflags != JSITER_ENUMERATE) {
                JS_ASSERT(pn2->pn_iflags == (JSITER_FOREACH | JSITER_ENUMERATE));
                reportError(NULL, JSMSG_BAD_FOR_EACH_LOOP);
                return NULL;
            }
            pn2->pn_iflags = JSITER_FOR_OF;
        }

        ParseNode *pn4 = expr();
        if (!pn4)
            return NULL;
        MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_AFTER_FOR_CTRL);

        guard.endBody();

        if (isGenexp) {
            if (!guard.checkValidBody(pn2))
                return NULL;
        } else {
            if (!guard.maybeNoteGenerator(pn2))
                return NULL;
        }

        switch (tt) {
#if JS_HAS_DESTRUCTURING
          case TOK_LB:
          case TOK_LC:
            if (!CheckDestructuring(context, &data, pn3, this))
                return NULL;

            if (versionNumber() == JSVERSION_1_7 &&
                !(pn2->pn_iflags & JSITER_FOREACH) &&
                !forOf)
            {
                /* Destructuring requires [key, value] enumeration in JS1.7. */
                if (!pn3->isKind(PNK_ARRAY) || pn3->pn_count != 2) {
                    reportError(NULL, JSMSG_BAD_FOR_LEFTSIDE);
                    return NULL;
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
                return NULL;
            break;

          default:;
        }

        /*
         * Synthesize a declaration. Every definition must appear in the parse
         * tree in order for ComprehensionTranslator to work.
         */
        ParseNode *vars = ListNode::create(PNK_VAR, this);
        if (!vars)
            return NULL;
        vars->setOp(JSOP_NOP);
        vars->pn_pos = pn3->pn_pos;
        vars->makeEmpty();
        vars->append(pn3);
        vars->pn_xflags |= PNX_FORINVAR;

        /* Definitions can't be passed directly to EmitAssignment as lhs. */
        pn3 = CloneLeftHandSide(pn3, this);
        if (!pn3)
            return NULL;

        pn2->pn_left = new_<TernaryNode>(PNK_FORIN, JSOP_NOP, vars, pn3, pn4);
        if (!pn2->pn_left)
            return NULL;
        *pnp = pn2;
        pnp = &pn2->pn_right;
    } while (tokenStream.matchToken(TOK_FOR));

    if (tokenStream.matchToken(TOK_IF)) {
        pn2 = TernaryNode::create(PNK_IF, this);
        if (!pn2)
            return NULL;
        pn2->pn_kid1 = condition();
        if (!pn2->pn_kid1)
            return NULL;
        *pnp = pn2;
        pnp = &pn2->pn_kid2;
    }

    pn2 = UnaryNode::create(kind, this);
    if (!pn2)
        return NULL;
    pn2->setOp(op);
    pn2->pn_kid = kid;
    *pnp = pn2;

    PopStatementPC(context, pc);
    return pn;
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
ParseNode *
Parser::generatorExpr(ParseNode *kid)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    /* Create a |yield| node for |kid|. */
    ParseNode *pn = UnaryNode::create(PNK_YIELD, this);
    if (!pn)
        return NULL;
    pn->setOp(JSOP_YIELD);
    pn->setInParens(true);
    pn->pn_pos = kid->pn_pos;
    pn->pn_kid = kid;
    pn->pn_hidden = true;

    /* Make a new node for the desugared generator function. */
    ParseNode *genfn = FunctionNode::create(PNK_FUNCTION, this);
    if (!genfn)
        return NULL;
    genfn->setOp(JSOP_LAMBDA);
    JS_ASSERT(!genfn->pn_body);
    genfn->pn_dflags = 0;

    {
        ParseContext *outerpc = pc;

        RootedFunction fun(context, newFunction(outerpc, /* atom = */ NullPtr(), Expression));
        if (!fun)
            return NULL;

        /* Create box for fun->object early to protect against last-ditch GC. */
        FunctionBox *genFunbox = newFunctionBox(fun, outerpc, outerpc->sc->strict);
        if (!genFunbox)
            return NULL;

        ParseContext genpc(this, genFunbox, outerpc->staticLevel + 1, outerpc->blockidGen);
        if (!genpc.init())
            return NULL;

        /*
         * We assume conservatively that any deoptimization flags in pc->sc
         * come from the kid. So we propagate these flags into genfn. For code
         * simplicity we also do not detect if the flags were only set in the
         * kid and could be removed from pc->sc.
         */
        genFunbox->anyCxFlags = outerpc->sc->anyCxFlags;
        if (outerpc->sc->isFunction)
            genFunbox->funCxFlags = outerpc->sc->asFunbox()->funCxFlags;

        genFunbox->setIsGenerator();
        genFunbox->inGenexpLambda = true;
        genfn->pn_funbox = genFunbox;
        genfn->pn_blockid = genpc.bodyid;

        ParseNode *body = comprehensionTail(pn, outerpc->blockid(), true);
        if (!body)
            return NULL;
        JS_ASSERT(!genfn->pn_body);
        genfn->pn_body = body;
        genfn->pn_pos.begin = body->pn_pos.begin = kid->pn_pos.begin;
        genfn->pn_pos.end = body->pn_pos.end = tokenStream.currentToken().pos.end;

        if (AtomDefnPtr p = genpc.lexdeps->lookup(context->names().arguments)) {
            Definition *dn = p.value();
            ParseNode *errorNode = dn->dn_uses ? dn->dn_uses : body;
            reportError(errorNode, JSMSG_BAD_GENEXP_BODY, js_arguments_str);
            return NULL;
        }

        if (!LeaveFunction(genfn, this))
            return NULL;
    }

    /*
     * Our result is a call expression that invokes the anonymous generator
     * function object.
     */
    ParseNode *result = ListNode::create(PNK_CALL, this);
    if (!result)
        return NULL;
    result->setOp(JSOP_CALL);
    result->pn_pos.begin = genfn->pn_pos.begin;
    result->initList(genfn);
    return result;
}

static const char js_generator_str[] = "generator";

#endif /* JS_HAS_GENERATOR_EXPRS */
#endif /* JS_HAS_GENERATORS */

ParseNode *
Parser::assignExprWithoutYield(unsigned msg)
{
#ifdef JS_HAS_GENERATORS
    GenexpGuard yieldGuard(this);
#endif
    ParseNode *res = assignExpr();
    yieldGuard.endBody();
    if (res) {
#ifdef JS_HAS_GENERATORS
        if (!yieldGuard.checkValidBody(res, msg)) {
            freeTree(res);
            res = NULL;
        }
#endif
    }
    return res;
}

bool
Parser::argumentList(ParseNode *listNode)
{
    if (tokenStream.matchToken(TOK_RP, TSF_OPERAND))
        return true;

    GenexpGuard guard(this);
    bool arg0 = true;

    do {
        ParseNode *argNode = assignExpr();
        if (!argNode)
            return false;
        if (arg0)
            guard.endBody();

#if JS_HAS_GENERATORS
        if (argNode->isKind(PNK_YIELD) &&
            !argNode->isInParens() &&
            tokenStream.peekToken() == TOK_COMMA) {
            reportError(argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_yield_str);
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
            if (listNode->pn_count > 1 ||
                tokenStream.peekToken() == TOK_COMMA) {
                reportError(argNode, JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
                return false;
            }
        } else
#endif
        if (arg0 && !guard.maybeNoteGenerator(argNode))
            return false;

        arg0 = false;

        listNode->append(argNode);
    } while (tokenStream.matchToken(TOK_COMMA));

    if (tokenStream.getToken() != TOK_RP) {
        reportError(NULL, JSMSG_PAREN_AFTER_ARGS);
        return false;
    }
    return true;
}

ParseNode *
Parser::memberExpr(bool allowCallSyntax)
{
    ParseNode *lhs;

    JS_CHECK_RECURSION(context, return NULL);

    /* Check for new expression first. */
    TokenKind tt = tokenStream.getToken(TSF_OPERAND);
    if (tt == TOK_NEW) {
        lhs = ListNode::create(PNK_NEW, this);
        if (!lhs)
            return NULL;
        ParseNode *ctorExpr = memberExpr(false);
        if (!ctorExpr)
            return NULL;
        lhs->setOp(JSOP_NEW);
        lhs->initList(ctorExpr);
        lhs->pn_pos.begin = ctorExpr->pn_pos.begin;

        if (tokenStream.matchToken(TOK_LP) && !argumentList(lhs))
            return NULL;
        if (lhs->pn_count > ARGC_LIMIT) {
            JS_ReportErrorNumber(context, js_GetErrorMessage, NULL,
                                 JSMSG_TOO_MANY_CON_ARGS);
            return NULL;
        }
        lhs->pn_pos.end = lhs->last()->pn_pos.end;
    } else {
        lhs = primaryExpr(tt, false);
        if (!lhs)
            return NULL;

        if (lhs->isXMLNameOp()) {
            lhs = new_<UnaryNode>(PNK_XMLUNARY, JSOP_XMLNAME, lhs->pn_pos, lhs);
            if (!lhs)
                return NULL;
        }
    }

    while ((tt = tokenStream.getToken()) > TOK_EOF) {
        ParseNode *nextMember;
        if (tt == TOK_DOT) {
            tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
            if (tt == TOK_ERROR)
                return NULL;
            if (tt == TOK_NAME) {
#if JS_HAS_XML_SUPPORT
                if (allowsXML() && tokenStream.peekToken() == TOK_DBLCOLON) {
                    ParseNode *propertyId = propertyQualifiedIdentifier();
                    if (!propertyId)
                        return NULL;

                    nextMember = new_<XMLDoubleColonProperty>(lhs, propertyId,
                                                              lhs->pn_pos.begin,
                                                              tokenStream.currentToken().pos.end);
                    if (!nextMember)
                        return NULL;
                } else
#endif
                {
                    PropertyName *field = tokenStream.currentToken().name();
                    nextMember = new_<PropertyAccess>(lhs, field,
                                                      lhs->pn_pos.begin,
                                                      tokenStream.currentToken().pos.end);
                    if (!nextMember)
                        return NULL;
                }
            }
#if JS_HAS_XML_SUPPORT
            else if (allowsXML()) {
                TokenPtr begin = lhs->pn_pos.begin;
                if (tt == TOK_LP) {
                    /* Filters are effectively 'with', so deoptimize names. */
                    pc->sc->setBindingsAccessedDynamically();

                    StmtInfoPC stmtInfo(context);
                    bool oldParsingWith = pc->parsingWith;
                    pc->parsingWith = true;
                    PushStatementPC(pc, &stmtInfo, STMT_WITH);

                    ParseNode *filter = bracketedExpr();
                    if (!filter)
                        return NULL;
                    filter->setInParens(true);
                    MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);

                    pc->parsingWith = oldParsingWith;
                    PopStatementPC(context, pc);

                    nextMember =
                        new_<XMLFilterExpression>(lhs, filter,
                                                  begin, tokenStream.currentToken().pos.end);
                    if (!nextMember)
                        return NULL;
                } else if (tt == TOK_AT || tt == TOK_STAR) {
                    ParseNode *propertyId = starOrAtPropertyIdentifier(tt);
                    if (!propertyId)
                        return NULL;
                    nextMember = new_<XMLProperty>(lhs, propertyId,
                                                   begin, tokenStream.currentToken().pos.end);
                    if (!nextMember)
                        return NULL;
                } else {
                    reportError(NULL, JSMSG_NAME_AFTER_DOT);
                    return NULL;
                }
            }
#endif
            else {
                reportError(NULL, JSMSG_NAME_AFTER_DOT);
                return NULL;
            }
        }
#if JS_HAS_XML_SUPPORT
        else if (tt == TOK_DBLDOT) {
            if (!allowsXML()) {
                reportError(NULL, JSMSG_NAME_AFTER_DOT);
                return NULL;
            }

            nextMember = BinaryNode::create(PNK_DBLDOT, this);
            if (!nextMember)
                return NULL;
            tt = tokenStream.getToken(TSF_OPERAND | TSF_KEYWORD_IS_NAME);
            ParseNode *pn3 = primaryExpr(tt, true);
            if (!pn3)
                return NULL;
            if (pn3->isKind(PNK_NAME) && !pn3->isInParens()) {
                pn3->setKind(PNK_STRING);
                pn3->setArity(PN_NULLARY);
                pn3->setOp(JSOP_QNAMEPART);
            } else if (!pn3->isXMLPropertyIdentifier()) {
                reportError(NULL, JSMSG_NAME_AFTER_DOT);
                return NULL;
            }
            nextMember->setOp(JSOP_DESCENDANTS);
            nextMember->pn_left = lhs;
            nextMember->pn_right = pn3;
            nextMember->pn_pos.begin = lhs->pn_pos.begin;
            nextMember->pn_pos.end = tokenStream.currentToken().pos.end;
        }
#endif
        else if (tt == TOK_LB) {
            ParseNode *propExpr = expr();
            if (!propExpr)
                return NULL;

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_IN_INDEX);
            TokenPtr begin = lhs->pn_pos.begin, end = tokenStream.currentToken().pos.end;

            /*
             * Do folding so we don't have roundtrip changes for cases like:
             * function (obj) { return obj["a" + "b"] }
             */
            if (foldConstants && !FoldConstants(context, propExpr, this))
                return NULL;

            /*
             * Optimize property name lookups. If the name is a PropertyName,
             * then make a name-based node so the emitter will use a name-based
             * bytecode. Otherwise make a node using the property expression
             * by value. If the node is a string containing an index, convert
             * it to a number to save work later.
             */
            uint32_t index;
            PropertyName *name = NULL;
            if (foldConstants) {
                if (propExpr->isKind(PNK_STRING)) {
                    JSAtom *atom = propExpr->pn_atom;
                    if (atom->isIndex(&index)) {
                        propExpr->setKind(PNK_NUMBER);
                        propExpr->setOp(JSOP_DOUBLE);
                        propExpr->pn_dval = index;
                    } else {
                        name = atom->asPropertyName();
                    }
                } else if (propExpr->isKind(PNK_NUMBER)) {
                    double number = propExpr->pn_dval;
                    if (number != ToUint32(number)) {
                        JSAtom *atom = ToAtom(context, DoubleValue(number));
                        if (!atom)
                            return NULL;
                        name = atom->asPropertyName();
                    }
                }
            }

            if (name)
                nextMember = new_<PropertyAccess>(lhs, name, begin, end);
            else
                nextMember = new_<PropertyByValue>(lhs, propExpr, begin, end);
            if (!nextMember)
                return NULL;
        } else if (allowCallSyntax && tt == TOK_LP) {
            nextMember = ListNode::create(PNK_CALL, this);
            if (!nextMember)
                return NULL;
            nextMember->setOp(JSOP_CALL);

            if (lhs->isOp(JSOP_NAME)) {
                if (lhs->pn_atom == context->names().eval) {
                    /* Select JSOP_EVAL and flag pc as heavyweight. */
                    nextMember->setOp(JSOP_EVAL);
                    pc->sc->setBindingsAccessedDynamically();

                    /*
                     * In non-strict mode code, direct calls to eval can add
                     * variables to the call object.
                     */
                    if (pc->sc->isFunction && !pc->sc->strict)
                        pc->sc->asFunbox()->setHasExtensibleScope();
                }
            } else if (lhs->isOp(JSOP_GETPROP)) {
                /* Select JSOP_FUNAPPLY given foo.apply(...). */
                if (lhs->pn_atom == context->names().apply)
                    nextMember->setOp(JSOP_FUNAPPLY);
                else if (lhs->pn_atom == context->names().call)
                    nextMember->setOp(JSOP_FUNCALL);
            }

            nextMember->initList(lhs);
            nextMember->pn_pos.begin = lhs->pn_pos.begin;

            if (!argumentList(nextMember))
                return NULL;
            if (nextMember->pn_count > ARGC_LIMIT) {
                JS_ReportErrorNumber(context, js_GetErrorMessage, NULL,
                                     JSMSG_TOO_MANY_FUN_ARGS);
                return NULL;
            }
            nextMember->pn_pos.end = tokenStream.currentToken().pos.end;
        } else {
            tokenStream.ungetToken();
            return lhs;
        }

        lhs = nextMember;
    }
    if (tt == TOK_ERROR)
        return NULL;
    return lhs;
}

ParseNode *
Parser::bracketedExpr()
{
    /*
     * Always accept the 'in' operator in a parenthesized expression,
     * where it's unambiguous, even if we might be parsing the init of a
     * for statement.
     */
    bool oldParsingForInit = pc->parsingForInit;
    pc->parsingForInit = false;
    ParseNode *pn = expr();
    pc->parsingForInit = oldParsingForInit;
    return pn;
}

#if JS_HAS_XML_SUPPORT

ParseNode *
Parser::endBracketedExpr()
{
    JS_ASSERT(allowsXML());

    ParseNode *pn = bracketedExpr();
    if (!pn)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_ATTR_EXPR);
    return pn;
}

/*
 * From the ECMA-357 grammar in 11.1.1 and 11.1.2:
 *
 *      AttributeIdentifier:
 *              @ PropertySelector
 *              @ QualifiedIdentifier
 *              @ [ Expression ]
 *
 *      PropertySelector:
 *              Identifier
 *              *
 *
 *      QualifiedIdentifier:
 *              PropertySelector :: PropertySelector
 *              PropertySelector :: [ Expression ]
 *
 * We adapt AttributeIdentifier and QualifiedIdentier to be LL(1), like so:
 *
 *      AttributeIdentifier:
 *              @ QualifiedIdentifier
 *              @ [ Expression ]
 *
 *      PropertySelector:
 *              Identifier
 *              *
 *
 *      QualifiedIdentifier:
 *              PropertySelector :: PropertySelector
 *              PropertySelector :: [ Expression ]
 *              PropertySelector
 *
 * As PrimaryExpression: Identifier is in ECMA-262 and we want the semantics
 * for that rule to result in a name node, but ECMA-357 extends the grammar
 * to include PrimaryExpression: QualifiedIdentifier, we must factor further:
 *
 *      QualifiedIdentifier:
 *              PropertySelector QualifiedSuffix
 *
 *      QualifiedSuffix:
 *              :: PropertySelector
 *              :: [ Expression ]
 *              /nothing/
 *
 * And use this production instead of PrimaryExpression: QualifiedIdentifier:
 *
 *      PrimaryExpression:
 *              Identifier QualifiedSuffix
 *
 * We hoist the :: match into callers of QualifiedSuffix, in order to tweak
 * PropertySelector vs. Identifier pn_arity, pn_op, and other members.
 */
ParseNode *
Parser::propertySelector()
{
    JS_ASSERT(allowsXML());

    ParseNode *selector;
    if (tokenStream.isCurrentTokenType(TOK_STAR)) {
        selector = NullaryNode::create(PNK_ANYNAME, this);
        if (!selector)
            return NULL;
        selector->setOp(JSOP_ANYNAME);
        selector->pn_atom = context->names().star;
    } else {
        JS_ASSERT(tokenStream.isCurrentTokenType(TOK_NAME));
        selector = NullaryNode::create(PNK_NAME, this);
        if (!selector)
            return NULL;
        selector->setOp(JSOP_QNAMEPART);
        selector->setArity(PN_NAME);
        selector->pn_atom = tokenStream.currentToken().name();
        selector->pn_cookie.makeFree();
    }
    return selector;
}

ParseNode *
Parser::qualifiedSuffix(ParseNode *pn)
{
    JS_ASSERT(allowsXML());

    JS_ASSERT(tokenStream.currentToken().type == TOK_DBLCOLON);
    ParseNode *pn2 = NameNode::create(PNK_DBLCOLON, NULL, this, this->pc);
    if (!pn2)
        return NULL;

    pc->sc->setBindingsAccessedDynamically();

    /* Left operand of :: must be evaluated if it is an identifier. */
    if (pn->isOp(JSOP_QNAMEPART))
        pn->setOp(JSOP_NAME);

    TokenKind tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
    if (tt == TOK_STAR || tt == TOK_NAME) {
        /* Inline and specialize propertySelector for JSOP_QNAMECONST. */
        pn2->setOp(JSOP_QNAMECONST);
        pn2->pn_pos.begin = pn->pn_pos.begin;
        pn2->pn_atom = (tt == TOK_STAR)
                       ? context->names().star
                       : tokenStream.currentToken().name();
        pn2->pn_expr = pn;
        pn2->pn_cookie.makeFree();
        return pn2;
    }

    if (tt != TOK_LB) {
        reportError(NULL, JSMSG_SYNTAX_ERROR);
        return NULL;
    }
    ParseNode *pn3 = endBracketedExpr();
    if (!pn3)
        return NULL;

    pn2->setOp(JSOP_QNAME);
    pn2->setArity(PN_BINARY);
    pn2->pn_pos.begin = pn->pn_pos.begin;
    pn2->pn_pos.end = pn3->pn_pos.end;
    pn2->pn_left = pn;
    pn2->pn_right = pn3;
    return pn2;
}

ParseNode *
Parser::qualifiedIdentifier()
{
    JS_ASSERT(allowsXML());

    ParseNode *pn = propertySelector();
    if (!pn)
        return NULL;
    if (tokenStream.matchToken(TOK_DBLCOLON)) {
        /* Hack for bug 496316. Slowing down E4X won't make it go away, alas. */
        pc->sc->setBindingsAccessedDynamically();
        pn = qualifiedSuffix(pn);
    }
    return pn;
}

ParseNode *
Parser::attributeIdentifier()
{
    JS_ASSERT(allowsXML());

    JS_ASSERT(tokenStream.currentToken().type == TOK_AT);
    ParseNode *pn = UnaryNode::create(PNK_AT, this);
    if (!pn)
        return NULL;
    pn->setOp(JSOP_TOATTRNAME);

    ParseNode *pn2;
    TokenKind tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
    if (tt == TOK_STAR || tt == TOK_NAME) {
        pn2 = qualifiedIdentifier();
    } else if (tt == TOK_LB) {
        pn2 = endBracketedExpr();
    } else {
        reportError(NULL, JSMSG_SYNTAX_ERROR);
        return NULL;
    }
    if (!pn2)
        return NULL;
    pn->pn_kid = pn2;
    pn->pn_pos.end = pn2->pn_pos.end;
    return pn;
}

/*
 * Make a TOK_LC unary node whose pn_kid is an expression.
 */
ParseNode *
Parser::xmlExpr(bool inTag)
{
    JS_ASSERT(allowsXML());

    JS_ASSERT(tokenStream.currentToken().type == TOK_LC);
    ParseNode *pn = UnaryNode::create(PNK_XMLCURLYEXPR, this);
    if (!pn)
        return NULL;

    /*
     * Turn off XML tag mode. We save the old value of the flag because it may
     * already be off: XMLExpr is called both from within a tag, and from
     * within text contained in an element, but outside of any start, end, or
     * point tag.
     */
    bool oldflag = tokenStream.isXMLTagMode();
    tokenStream.setXMLTagMode(false);
    ParseNode *pn2 = expr();
    if (!pn2)
        return NULL;

    MUST_MATCH_TOKEN(TOK_RC, JSMSG_CURLY_IN_XML_EXPR);
    tokenStream.setXMLTagMode(oldflag);
    pn->pn_kid = pn2;
    pn->setOp(inTag ? JSOP_XMLTAGEXPR : JSOP_XMLELTEXPR);
    pn->pn_pos.end = pn2->pn_pos.end;
    return pn;
}

/*
 * Parse the productions:
 *
 *      XMLNameExpr:
 *              XMLName XMLNameExpr?
 *              { Expr } XMLNameExpr?
 *
 * Return a PN_LIST, PN_UNARY, or PN_NULLARY according as XMLNameExpr produces
 * a list of names and/or expressions, a single expression, or a single name.
 * If PN_LIST or PN_NULLARY, getKind() will be PNK_XMLNAME.  Otherwise if
 * PN_UNARY, getKind() will be PNK_XMLCURLYEXPR.
 */
ParseNode *
Parser::xmlNameExpr()
{
    JS_ASSERT(allowsXML());

    ParseNode *pn, *pn2, *list;
    TokenKind tt;

    pn = list = NULL;
    do {
        tt = tokenStream.currentToken().type;
        if (tt == TOK_LC) {
            pn2 = xmlExpr(true);
            if (!pn2)
                return NULL;
        } else {
            JS_ASSERT(tt == TOK_XMLNAME);
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_STRING);
            pn2 = atomNode(PNK_XMLNAME, JSOP_STRING);
            if (!pn2)
                return NULL;
        }

        if (!pn) {
            pn = pn2;
        } else {
            if (!list) {
                list = ListNode::create(PNK_XMLNAME, this);
                if (!list)
                    return NULL;
                list->pn_pos.begin = pn->pn_pos.begin;
                list->initList(pn);
                list->pn_xflags = PNX_CANTFOLD;
                pn = list;
            }
            pn->pn_pos.end = pn2->pn_pos.end;
            pn->append(pn2);
        }
    } while ((tt = tokenStream.getToken()) == TOK_XMLNAME || tt == TOK_LC);

    tokenStream.ungetToken();
    return pn;
}

/*
 * Macro to test whether an XMLNameExpr or XMLTagContent node can be folded
 * at compile time into a JSXML tree.
 */
#define XML_FOLDABLE(pn)        ((pn)->isArity(PN_LIST)                     \
                                 ? ((pn)->pn_xflags & PNX_CANTFOLD) == 0    \
                                 : !(pn)->isKind(PNK_XMLCURLYEXPR))

/*
 * Parse the productions:
 *
 *      XMLTagContent:
 *              XMLNameExpr
 *              XMLTagContent S XMLNameExpr S? = S? XMLAttr
 *              XMLTagContent S XMLNameExpr S? = S? { Expr }
 *
 * Return a PN_LIST, PN_UNARY, or PN_NULLARY according to how XMLTagContent
 * produces a list of name and attribute values and/or braced expressions, a
 * single expression, or a single name.
 *
 * If PN_LIST or PN_NULLARY, getKind() will be PNK_XMLNAME for the case where
 * XMLTagContent: XMLNameExpr.  If getKind() is not PNK_XMLNAME but getArity()
 * is PN_LIST, getKind() will be tagkind.  If PN_UNARY, getKind() will be
 * PNK_XMLCURLYEXPR and we parsed exactly one expression.
 */
ParseNode *
Parser::xmlTagContent(ParseNodeKind tagkind, JSAtom **namep)
{
    JS_ASSERT(allowsXML());

    ParseNode *pn, *pn2, *list;
    TokenKind tt;

    pn = xmlNameExpr();
    if (!pn)
        return NULL;
    *namep = (pn->isArity(PN_NULLARY)) ? pn->pn_atom : NULL;
    list = NULL;

    while (tokenStream.matchToken(TOK_XMLSPACE)) {
        tt = tokenStream.getToken();
        if (tt != TOK_XMLNAME && tt != TOK_LC) {
            tokenStream.ungetToken();
            break;
        }

        pn2 = xmlNameExpr();
        if (!pn2)
            return NULL;
        if (!list) {
            list = ListNode::create(tagkind, this);
            if (!list)
                return NULL;
            list->pn_pos.begin = pn->pn_pos.begin;
            list->initList(pn);
            pn = list;
        }
        pn->append(pn2);
        if (!XML_FOLDABLE(pn2))
            pn->pn_xflags |= PNX_CANTFOLD;

        tokenStream.matchToken(TOK_XMLSPACE);
        MUST_MATCH_TOKEN(TOK_ASSIGN, JSMSG_NO_ASSIGN_IN_XML_ATTR);
        tokenStream.matchToken(TOK_XMLSPACE);

        tt = tokenStream.getToken();
        if (tt == TOK_XMLATTR) {
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_STRING);
            pn2 = atomNode(PNK_XMLATTR, JSOP_STRING);
        } else if (tt == TOK_LC) {
            pn2 = xmlExpr(true);
            pn->pn_xflags |= PNX_CANTFOLD;
        } else {
            reportError(NULL, JSMSG_BAD_XML_ATTR_VALUE);
            return NULL;
        }
        if (!pn2)
            return NULL;
        pn->append(pn2);
    }

    return pn;
}

#define XML_CHECK_FOR_ERROR_AND_EOF(tt,result)                                              \
    JS_BEGIN_MACRO                                                                          \
        if ((tt) <= TOK_EOF) {                                                              \
            if ((tt) == TOK_EOF) {                                                          \
                reportError(NULL, JSMSG_END_OF_XML_SOURCE);                                 \
            }                                                                               \
            return result;                                                                  \
        }                                                                                   \
    JS_END_MACRO

/*
 * Consume XML element tag content, including the TOK_XMLETAGO (</) sequence
 * that opens the end tag for the container.
 */
bool
Parser::xmlElementContent(ParseNode *pn)
{
    JS_ASSERT(allowsXML());

    tokenStream.setXMLTagMode(false);
    for (;;) {
        TokenKind tt = tokenStream.getToken(TSF_XMLTEXTMODE);
        XML_CHECK_FOR_ERROR_AND_EOF(tt, false);

        JS_ASSERT(tt == TOK_XMLSPACE || tt == TOK_XMLTEXT);
        JSAtom *textAtom = tokenStream.currentToken().atom();
        if (textAtom) {
            /* Non-zero-length XML text scanned. */
            JS_ASSERT(tokenStream.currentToken().t_op == JSOP_STRING);
            ParseNode *pn2 = atomNode(tt == TOK_XMLSPACE ? PNK_XMLSPACE : PNK_XMLTEXT,
                                      JSOP_STRING);
            if (!pn2)
                return false;
            pn->append(pn2);
        }

        tt = tokenStream.getToken(TSF_OPERAND);
        XML_CHECK_FOR_ERROR_AND_EOF(tt, false);
        if (tt == TOK_XMLETAGO)
            break;

        ParseNode *pn2;
        if (tt == TOK_LC) {
            pn2 = xmlExpr(false);
            if (!pn2)
                return false;
            pn->pn_xflags |= PNX_CANTFOLD;
        } else if (tt == TOK_XMLSTAGO) {
            pn2 = xmlElementOrList(false);
            if (!pn2)
                return false;
            pn2->pn_xflags &= ~PNX_XMLROOT;
            pn->pn_xflags |= pn2->pn_xflags;
        } else if (tt == TOK_XMLPI) {
            const Token &tok = tokenStream.currentToken();
            pn2 = new_<XMLProcessingInstruction>(tok.xmlPITarget(), tok.xmlPIData(), tok.pos);
            if (!pn2)
                return false;
        } else {
            JS_ASSERT(tt == TOK_XMLCDATA || tt == TOK_XMLCOMMENT);
            pn2 = atomNode(tt == TOK_XMLCDATA ? PNK_XMLCDATA : PNK_XMLCOMMENT,
                           tokenStream.currentToken().t_op);
            if (!pn2)
                return false;
        }
        pn->append(pn2);
    }
    tokenStream.setXMLTagMode(true);

    JS_ASSERT(tokenStream.currentToken().type == TOK_XMLETAGO);
    return true;
}

/*
 * Return a PN_LIST node containing an XML or XMLList Initialiser.
 */
ParseNode *
Parser::xmlElementOrList(bool allowList)
{
    JS_ASSERT(allowsXML());

    ParseNode *pn, *pn2, *list;
    TokenKind tt;
    RootedAtom startAtom(context), endAtom(context);

    JS_CHECK_RECURSION(context, return NULL);

    JS_ASSERT(tokenStream.currentToken().type == TOK_XMLSTAGO);
    pn = ListNode::create(PNK_XMLSTAGO, this);
    if (!pn)
        return NULL;

    tokenStream.setXMLTagMode(true);
    tt = tokenStream.getToken();
    if (tt == TOK_ERROR)
        return NULL;

    if (tt == TOK_XMLNAME || tt == TOK_LC) {
        /*
         * XMLElement.  Append the tag and its contents, if any, to pn.
         */
        pn2 = xmlTagContent(PNK_XMLSTAGO, startAtom.address());
        if (!pn2)
            return NULL;
        tokenStream.matchToken(TOK_XMLSPACE);

        tt = tokenStream.getToken();
        if (tt == TOK_XMLPTAGC) {
            /* Point tag (/>): recycle pn if pn2 is a list of tag contents. */
            if (pn2->isKind(PNK_XMLSTAGO)) {
                pn->makeEmpty();
                freeTree(pn);
                pn = pn2;
            } else {
                JS_ASSERT(pn2->isKind(PNK_XMLNAME) || pn2->isKind(PNK_XMLCURLYEXPR));
                pn->initList(pn2);
                if (!XML_FOLDABLE(pn2))
                    pn->pn_xflags |= PNX_CANTFOLD;
            }
            pn->setKind(PNK_XMLPTAGC);
            pn->pn_xflags |= PNX_XMLROOT;
        } else {
            /* We had better have a tag-close (>) at this point. */
            if (tt != TOK_XMLTAGC) {
                reportError(NULL, JSMSG_BAD_XML_TAG_SYNTAX);
                return NULL;
            }
            pn2->pn_pos.end = tokenStream.currentToken().pos.end;

            /* Make sure pn2 is a TOK_XMLSTAGO list containing tag contents. */
            if (!pn2->isKind(PNK_XMLSTAGO)) {
                pn->initList(pn2);
                if (!XML_FOLDABLE(pn2))
                    pn->pn_xflags |= PNX_CANTFOLD;
                pn2 = pn;
                pn = ListNode::create(PNK_XMLTAGC, this);
                if (!pn)
                    return NULL;
                pn->pn_pos = pn2->pn_pos;
            }

            /* Now make pn a nominal-root TOK_XMLELEM list containing pn2. */
            pn->setKind(PNK_XMLELEM);
            pn->pn_pos.begin = pn2->pn_pos.begin;
            pn->initList(pn2);
            if (!XML_FOLDABLE(pn2))
                pn->pn_xflags |= PNX_CANTFOLD;
            pn->pn_xflags |= PNX_XMLROOT;

            /* Get element contents and delimiting end-tag-open sequence. */
            if (!xmlElementContent(pn))
                return NULL;

            tt = tokenStream.getToken();
            XML_CHECK_FOR_ERROR_AND_EOF(tt, NULL);
            if (tt != TOK_XMLNAME && tt != TOK_LC) {
                reportError(NULL, JSMSG_BAD_XML_TAG_SYNTAX);
                return NULL;
            }

            /* Parse end tag; check mismatch at compile-time if we can. */
            pn2 = xmlTagContent(PNK_XMLETAGO, endAtom.address());
            if (!pn2)
                return NULL;
            if (pn2->isKind(PNK_XMLETAGO)) {
                /* Oops, end tag has attributes! */
                reportError(NULL, JSMSG_BAD_XML_TAG_SYNTAX);
                return NULL;
            }
            if (endAtom && startAtom && endAtom != startAtom) {
                /* End vs. start tag name mismatch: point to the tag name. */
                reportUcError(pn2, JSMSG_XML_TAG_NAME_MISMATCH, startAtom->chars());
                return NULL;
            }

            /* Make a TOK_XMLETAGO list with pn2 as its single child. */
            JS_ASSERT(pn2->isKind(PNK_XMLNAME) || pn2->isKind(PNK_XMLCURLYEXPR));
            list = ListNode::create(PNK_XMLETAGO, this);
            if (!list)
                return NULL;
            list->initList(pn2);
            pn->append(list);
            if (!XML_FOLDABLE(pn2)) {
                list->pn_xflags |= PNX_CANTFOLD;
                pn->pn_xflags |= PNX_CANTFOLD;
            }

            tokenStream.matchToken(TOK_XMLSPACE);
            MUST_MATCH_TOKEN(TOK_XMLTAGC, JSMSG_BAD_XML_TAG_SYNTAX);
        }

        /* Set pn_op now that pn has been updated to its final value. */
        pn->setOp(JSOP_TOXML);
    } else if (allowList && tt == TOK_XMLTAGC) {
        /* XMLList Initialiser. */
        pn->setKind(PNK_XMLLIST);
        pn->setOp(JSOP_TOXMLLIST);
        pn->makeEmpty();
        pn->pn_xflags |= PNX_XMLROOT;
        if (!xmlElementContent(pn))
            return NULL;

        MUST_MATCH_TOKEN(TOK_XMLTAGC, JSMSG_BAD_XML_LIST_SYNTAX);
    } else {
        reportError(NULL, JSMSG_BAD_XML_NAME_SYNTAX);
        return NULL;
    }
    tokenStream.setXMLTagMode(false);

    pn->pn_pos.end = tokenStream.currentToken().pos.end;
    return pn;
}

ParseNode *
Parser::xmlElementOrListRoot(bool allowList)
{
    JS_ASSERT(allowsXML());

    /*
     * Turn on "moar XML" so that comments and CDATA literals are recognized,
     * instead of <! followed by -- starting an HTML comment to end of line
     * (used in script tags to hide content from old browsers that don't
     * recognize <script>).
     */
    bool hadMoarXML = tokenStream.hasMoarXML();
    tokenStream.setMoarXML(true);
    ParseNode *pn = xmlElementOrList(allowList);
    tokenStream.setMoarXML(hadMoarXML);
    return pn;
}

ParseNode *
Parser::parseXMLText(JSObject *chain, bool allowList)
{
    /*
     * Push a compiler frame if we have no frames, or if the top frame is a
     * lightweight function activation, or if its scope chain doesn't match
     * the one passed to us.
     */
    GlobalSharedContext xmlsc(context, chain, false);
    ParseContext xmlpc(this, &xmlsc, /* staticLevel = */ 0, /* bodyid = */ 0);
    if (!xmlpc.init())
        return NULL;

    /* Set XML-only mode to turn off special treatment of {expr} in XML. */
    tokenStream.setXMLOnlyMode();
    TokenKind tt = tokenStream.getToken(TSF_OPERAND);

    ParseNode *pn;
    if (tt != TOK_XMLSTAGO) {
        reportError(NULL, JSMSG_BAD_XML_MARKUP);
        pn = NULL;
    } else {
        pn = xmlElementOrListRoot(allowList);
    }
    tokenStream.setXMLOnlyMode(false);

    return pn;
}

#endif /* JS_HAS_XMLSUPPORT */

bool
Parser::checkForFunctionNode(PropertyName *name, ParseNode *node)
{
    /*
     * In |a.ns::name|, |ns| refers to an in-scope variable, so |ns| can't be a
     * keyword.  (Exception: |function::name| is the actual name property, not
     * what E4X would expose.)  We parsed |ns| accepting a keyword as a name,
     * so we must implement the keyword restriction manually in this case.
     */
    if (const KeywordInfo *ki = FindKeyword(name->charsZ(), name->length())) {
        if (ki->tokentype != TOK_FUNCTION) {
            reportError(NULL, JSMSG_KEYWORD_NOT_NS);
            return false;
        }

        node->setArity(PN_NULLARY);
        node->setKind(PNK_FUNCTIONNS);
    }

    return true;
}

#if JS_HAS_XML_SUPPORT
ParseNode *
Parser::propertyQualifiedIdentifier()
{
    JS_ASSERT(allowsXML());
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_NAME));
    JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NAME);
    JS_ASSERT(tokenStream.peekToken() == TOK_DBLCOLON);

    /* Deoptimize QualifiedIdentifier properties to avoid tricky analysis. */
    pc->sc->setBindingsAccessedDynamically();

    PropertyName *name = tokenStream.currentToken().name();
    ParseNode *node = NameNode::create(PNK_NAME, name, this, this->pc);
    if (!node)
        return NULL;
    node->setOp(JSOP_NAME);
    node->pn_dflags |= PND_DEOPTIMIZED;

    if (!checkForFunctionNode(name, node))
        return NULL;

    tokenStream.consumeKnownToken(TOK_DBLCOLON);
    return qualifiedSuffix(node);
}
#endif

ParseNode *
Parser::identifierName(bool afterDoubleDot)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(TOK_NAME));

    PropertyName *name = tokenStream.currentToken().name();
    ParseNode *node = NameNode::create(PNK_NAME, name, this, this->pc);
    if (!node)
        return NULL;
    JS_ASSERT(tokenStream.currentToken().t_op == JSOP_NAME);
    node->setOp(JSOP_NAME);

    if ((!afterDoubleDot
#if JS_HAS_XML_SUPPORT
                || (allowsXML() && tokenStream.peekToken() == TOK_DBLCOLON)
#endif
               ) && !pc->inDeclDestructuring)
    {
        if (!NoteNameUse(node, this))
            return NULL;
    }

#if JS_HAS_XML_SUPPORT
    if (allowsXML() && tokenStream.matchToken(TOK_DBLCOLON)) {
        if (afterDoubleDot) {
            if (!checkForFunctionNode(name, node))
                return NULL;
        }
        node = qualifiedSuffix(node);
        if (!node)
            return NULL;
    }
#endif

    return node;
}

#if JS_HAS_XML_SUPPORT
ParseNode *
Parser::starOrAtPropertyIdentifier(TokenKind tt)
{
    JS_ASSERT(tt == TOK_AT || tt == TOK_STAR);
    if (allowsXML())
        return (tt == TOK_AT) ? attributeIdentifier() : qualifiedIdentifier();
    reportError(NULL, JSMSG_SYNTAX_ERROR);
    return NULL;
}
#endif

ParseNode *
Parser::atomNode(ParseNodeKind kind, JSOp op)
{
    ParseNode *node = NullaryNode::create(kind, this);
    if (!node)
        return NULL;
    node->setOp(op);
    const Token &tok = tokenStream.currentToken();
    node->pn_atom = tok.atom();

    // Large strings are fast to parse but slow to compress. Stop compression on
    // them, so we don't wait for a long time for compression to finish at the
    // end of compilation.
    const size_t HUGE_STRING = 50000;
    if (sct && sct->active() && kind == PNK_STRING && node->pn_atom->length() >= HUGE_STRING)
        sct->abort();

    return node;
}

ParseNode *
Parser::primaryExpr(TokenKind tt, bool afterDoubleDot)
{
    JS_ASSERT(tokenStream.isCurrentTokenType(tt));

    ParseNode *pn, *pn2, *pn3;
    JSOp op;

    JS_CHECK_RECURSION(context, return NULL);

    switch (tt) {
      case TOK_FUNCTION:
#if JS_HAS_XML_SUPPORT
        if (allowsXML() && tokenStream.matchToken(TOK_DBLCOLON, TSF_KEYWORD_IS_NAME)) {
            pn2 = NullaryNode::create(PNK_FUNCTIONNS, this);
            if (!pn2)
                return NULL;
            pn = qualifiedSuffix(pn2);
            if (!pn)
                return NULL;
            break;
        }
#endif
        pn = functionExpr();
        if (!pn)
            return NULL;
        break;

      case TOK_LB:
      {
        pn = ListNode::create(PNK_ARRAY, this);
        if (!pn)
            return NULL;
        pn->setOp(JSOP_NEWINIT);
        pn->makeEmpty();

#if JS_HAS_GENERATORS
        pn->pn_blockid = pc->blockidGen;
#endif
        if (tokenStream.matchToken(TOK_RB, TSF_OPERAND)) {
            /*
             * Mark empty arrays as non-constant, since we cannot easily
             * determine their type.
             */
            pn->pn_xflags |= PNX_NONCONST;
        } else {
            bool spread = false;
            unsigned index = 0;
            for (; ; index++) {
                if (index == StackSpace::ARGS_LENGTH_MAX) {
                    reportError(NULL, JSMSG_ARRAY_INIT_TOO_BIG);
                    return NULL;
                }

                tt = tokenStream.peekToken(TSF_OPERAND);
                if (tt == TOK_RB) {
                    pn->pn_xflags |= PNX_ENDCOMMA;
                    break;
                }

                if (tt == TOK_COMMA) {
                    /* So CURRENT_TOKEN gets TOK_COMMA and not TOK_LB. */
                    tokenStream.matchToken(TOK_COMMA);
                    pn2 = NullaryNode::create(PNK_COMMA, this);
                    pn->pn_xflags |= PNX_HOLEY | PNX_NONCONST;
                } else {
                    ParseNode *spreadNode = NULL;
                    if (tt == TOK_TRIPLEDOT) {
                        spread = true;
                        spreadNode = UnaryNode::create(PNK_SPREAD, this);
                        if (!spreadNode)
                            return NULL;
                        tokenStream.getToken();
                    }
                    pn2 = assignExpr();
                    if (pn2) {
                        if (foldConstants && !FoldConstants(context, pn2, this))
                            return NULL;
                        if (!pn2->isConstant() || spreadNode)
                            pn->pn_xflags |= PNX_NONCONST;
                        if (spreadNode) {
                            spreadNode->pn_kid = pn2;
                            pn2 = spreadNode;
                        }
                    }
                }
                if (!pn2)
                    return NULL;
                pn->append(pn2);

                if (tt != TOK_COMMA) {
                    /* If we didn't already match TOK_COMMA in above case. */
                    if (!tokenStream.matchToken(TOK_COMMA))
                        break;
                }
            }

#if JS_HAS_GENERATORS
            /*
             * At this point, (index == 0 && pn->pn_count != 0) implies one
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
            if (index == 0 && !spread && pn->pn_count != 0 && tokenStream.matchToken(TOK_FOR)) {
                ParseNode *pnexp, *pntop;

                /* Relabel pn as an array comprehension node. */
                pn->setKind(PNK_ARRAYCOMP);

                /*
                 * Remove the comprehension expression from pn's linked list
                 * and save it via pnexp.  We'll re-install it underneath the
                 * ARRAYPUSH node after we parse the rest of the comprehension.
                 */
                pnexp = pn->last();
                JS_ASSERT(pn->pn_count == 1);
                pn->pn_count = 0;
                pn->pn_tail = &pn->pn_head;
                *pn->pn_tail = NULL;

                pntop = comprehensionTail(pnexp, pn->pn_blockid, false,
                                          PNK_ARRAYPUSH, JSOP_ARRAYPUSH);
                if (!pntop)
                    return NULL;
                pn->append(pntop);
            }
#endif /* JS_HAS_GENERATORS */

            MUST_MATCH_TOKEN(TOK_RB, JSMSG_BRACKET_AFTER_LIST);
        }
        pn->pn_pos.end = tokenStream.currentToken().pos.end;
        return pn;
      }

      case TOK_LC:
      {
        ParseNode *pnval;

        /*
         * A map from property names we've seen thus far to a mask of property
         * assignment types, stored and retrieved with ALE_SET_INDEX/ALE_INDEX.
         */
        AtomIndexMap seen(context);

        enum AssignmentType {
            GET     = 0x1,
            SET     = 0x2,
            VALUE   = 0x4 | GET | SET
        };

        pn = ListNode::create(PNK_OBJECT, this);
        if (!pn)
            return NULL;
        pn->setOp(JSOP_NEWINIT);
        pn->makeEmpty();

        for (;;) {
            JSAtom *atom;
            TokenKind ltok = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
            TokenPtr begin = tokenStream.currentToken().pos.begin;
            switch (ltok) {
              case TOK_NUMBER:
                pn3 = NullaryNode::create(PNK_NUMBER, this);
                if (!pn3)
                    return NULL;
                pn3->pn_dval = tokenStream.currentToken().number();
                atom = ToAtom(context, DoubleValue(pn3->pn_dval));
                if (!atom)
                    return NULL;
                break;
              case TOK_NAME:
                {
                    atom = tokenStream.currentToken().name();
                    if (atom == context->names().get) {
                        op = JSOP_GETTER;
                    } else if (atom == context->names().set) {
                        op = JSOP_SETTER;
                    } else {
                        pn3 = NullaryNode::create(PNK_NAME, this);
                        if (!pn3)
                            return NULL;
                        pn3->pn_atom = atom;
                        break;
                    }

                    tt = tokenStream.getToken(TSF_KEYWORD_IS_NAME);
                    if (tt == TOK_NAME) {
                        atom = tokenStream.currentToken().name();
                        pn3 = NameNode::create(PNK_NAME, atom, this, this->pc);
                        if (!pn3)
                            return NULL;
                    } else if (tt == TOK_STRING) {
                        atom = tokenStream.currentToken().atom();

                        uint32_t index;
                        if (atom->isIndex(&index)) {
                            pn3 = NullaryNode::create(PNK_NUMBER, this);
                            if (!pn3)
                                return NULL;
                            pn3->pn_dval = index;
                            atom = ToAtom(context, DoubleValue(pn3->pn_dval));
                            if (!atom)
                                return NULL;
                        } else {
                            pn3 = NameNode::create(PNK_STRING, atom, this, this->pc);
                            if (!pn3)
                                return NULL;
                        }
                    } else if (tt == TOK_NUMBER) {
                        pn3 = NullaryNode::create(PNK_NUMBER, this);
                        if (!pn3)
                            return NULL;
                        pn3->pn_dval = tokenStream.currentToken().number();
                        atom = ToAtom(context, DoubleValue(pn3->pn_dval));
                        if (!atom)
                            return NULL;
                    } else {
                        tokenStream.ungetToken();
                        pn3 = NullaryNode::create(PNK_NAME, this);
                        if (!pn3)
                            return NULL;
                        pn3->pn_atom = atom;
                        break;
                    }

                    pn->pn_xflags |= PNX_NONCONST;

                    /* NB: Getter function in { get x(){} } is unnamed. */
                    Rooted<PropertyName*> funName(context, NULL);
                    TokenStream::Position start;
                    tokenStream.tell(&start);
                    pn2 = functionDef(funName, start, op == JSOP_GETTER ? Getter : Setter,
                                      Expression);
                    if (!pn2)
                        return NULL;
                    TokenPos pos = {begin, pn2->pn_pos.end};
                    pn2 = new_<BinaryNode>(PNK_COLON, op, pos, pn3, pn2);
                    goto skip;
                }
              case TOK_STRING: {
                atom = tokenStream.currentToken().atom();
                uint32_t index;
                if (atom->isIndex(&index)) {
                    pn3 = NullaryNode::create(PNK_NUMBER, this);
                    if (!pn3)
                        return NULL;
                    pn3->pn_dval = index;
                } else {
                    pn3 = NullaryNode::create(PNK_STRING, this);
                    if (!pn3)
                        return NULL;
                    pn3->pn_atom = atom;
                }
                break;
              }
              case TOK_RC:
                goto end_obj_init;
              default:
                reportError(NULL, JSMSG_BAD_PROP_ID);
                return NULL;
            }

            op = JSOP_INITPROP;
            tt = tokenStream.getToken();
            if (tt == TOK_COLON) {
                pnval = assignExpr();
                if (!pnval)
                    return NULL;

                if (foldConstants && !FoldConstants(context, pnval, this))
                    return NULL;

                /*
                 * Treat initializers which mutate __proto__ as non-constant,
                 * so that we can later assume singleton objects delegate to
                 * the default Object.prototype.
                 */
                if (!pnval->isConstant() || atom == context->names().proto)
                    pn->pn_xflags |= PNX_NONCONST;
            }
#if JS_HAS_DESTRUCTURING_SHORTHAND
            else if (ltok == TOK_NAME && (tt == TOK_COMMA || tt == TOK_RC)) {
                /*
                 * Support, e.g., |var {x, y} = o| as destructuring shorthand
                 * for |var {x: x, y: y} = o|, per proposed JS2/ES4 for JS1.8.
                 */
                tokenStream.ungetToken();
                if (!tokenStream.checkForKeyword(atom->charsZ(), atom->length(), NULL, NULL))
                    return NULL;
                pn->pn_xflags |= PNX_DESTRUCT | PNX_NONCONST;
                pnval = pn3;
                JS_ASSERT(pnval->isKind(PNK_NAME));
                pnval->setArity(PN_NAME);
                ((NameNode *)pnval)->initCommon(pc);
            }
#endif
            else {
                reportError(NULL, JSMSG_COLON_AFTER_ID);
                return NULL;
            }

            {
                TokenPos pos = {begin, pnval->pn_pos.end};
                pn2 = new_<BinaryNode>(PNK_COLON, op, pos, pn3, pnval);
            }
          skip:
            if (!pn2)
                return NULL;
            pn->append(pn2);

            /*
             * Check for duplicate property names.  Duplicate data properties
             * only conflict in strict mode.  Duplicate getter or duplicate
             * setter halves always conflict.  A data property conflicts with
             * any part of an accessor property.
             */
            AssignmentType assignType;
            if (op == JSOP_INITPROP) {
                assignType = VALUE;
            } else if (op == JSOP_GETTER) {
                assignType = GET;
            } else if (op == JSOP_SETTER) {
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
                        return NULL;

                    Reporter reporter =
                        (oldAssignType == VALUE && assignType == VALUE && !pc->sc->needStrictChecks())
                        ? &Parser::reportWarning
                        : (pc->sc->needStrictChecks() ? &Parser::reportStrictModeError : &Parser::reportError);
                    if (!(this->*reporter)(NULL, JSMSG_DUPLICATE_PROPERTY, name.ptr()))
                        return NULL;
                }
                p.value() = assignType | oldAssignType;
            } else {
                if (!seen.add(p, atom, assignType))
                    return NULL;
            }

            tt = tokenStream.getToken();
            if (tt == TOK_RC)
                goto end_obj_init;
            if (tt != TOK_COMMA) {
                reportError(NULL, JSMSG_CURLY_AFTER_LIST);
                return NULL;
            }
        }

      end_obj_init:
        pn->pn_pos.end = tokenStream.currentToken().pos.end;
        return pn;
      }

#if JS_HAS_BLOCK_SCOPE
      case TOK_LET:
        pn = letBlock(LetExpresion);
        if (!pn)
            return NULL;
        break;
#endif

      case TOK_LP:
      {
        bool genexp;

        pn = parenExpr(&genexp);
        if (!pn)
            return NULL;
        pn->setInParens(true);
        if (!genexp)
            MUST_MATCH_TOKEN(TOK_RP, JSMSG_PAREN_IN_PAREN);
        break;
      }

      case TOK_STRING:
        pn = atomNode(PNK_STRING, JSOP_STRING);
        if (!pn)
            return NULL;
        break;

#if JS_HAS_XML_SUPPORT
      case TOK_AT:
      case TOK_STAR:
        if (!allowsXML())
            goto syntaxerror;
        pn = starOrAtPropertyIdentifier(tt);
        break;

      case TOK_XMLSTAGO:
        if (!allowsXML())
            goto syntaxerror;
        pn = xmlElementOrListRoot(true);
        if (!pn)
            return NULL;
        break;

      case TOK_XMLCDATA:
        if (!allowsXML())
            goto syntaxerror;
        pn = atomNode(PNK_XMLCDATA, JSOP_XMLCDATA);
        if (!pn)
            return NULL;
        break;

      case TOK_XMLCOMMENT:
        if (!allowsXML())
            goto syntaxerror;
        pn = atomNode(PNK_XMLCOMMENT, JSOP_XMLCOMMENT);
        if (!pn)
            return NULL;
        break;

      case TOK_XMLPI: {
        if (!allowsXML())
            goto syntaxerror;
        const Token &tok = tokenStream.currentToken();
        pn = new_<XMLProcessingInstruction>(tok.xmlPITarget(), tok.xmlPIData(), tok.pos);
        if (!pn)
            return NULL;
        break;
      }
#endif

      case TOK_NAME:
        pn = identifierName(afterDoubleDot);
        break;

      case TOK_REGEXP:
      {
        pn = NullaryNode::create(PNK_REGEXP, this);
        if (!pn)
            return NULL;

        size_t length = tokenStream.getTokenbuf().length();
        const StableCharPtr chars(tokenStream.getTokenbuf().begin(), length);
        RegExpFlag flags = tokenStream.currentToken().regExpFlags();
        RegExpStatics *res = context->regExpStatics();

        Rooted<RegExpObject*> reobj(context);
        if (context->hasfp())
            reobj = RegExpObject::create(context, res, chars, length, flags, &tokenStream);
        else
            reobj = RegExpObject::createNoStatics(context, chars, length, flags, &tokenStream);

        if (!reobj)
            return NULL;

        if (!compileAndGo) {
            if (!JSObject::clearParent(context, reobj))
                return NULL;
            if (!JSObject::clearType(context, reobj))
                return NULL;
        }

        pn->pn_objbox = newObjectBox(reobj);
        if (!pn->pn_objbox)
            return NULL;

        pn->setOp(JSOP_REGEXP);
        break;
      }

      case TOK_NUMBER:
        pn = NullaryNode::create(PNK_NUMBER, this);
        if (!pn)
            return NULL;
        pn->setOp(JSOP_DOUBLE);
        pn->pn_dval = tokenStream.currentToken().number();
        break;

      case TOK_TRUE:
        return new_<BooleanLiteral>(true, tokenStream.currentToken().pos);
      case TOK_FALSE:
        return new_<BooleanLiteral>(false, tokenStream.currentToken().pos);
      case TOK_THIS:
        return new_<ThisLiteral>(tokenStream.currentToken().pos);
      case TOK_NULL:
        return new_<NullLiteral>(tokenStream.currentToken().pos);

      case TOK_ERROR:
        /* The scanner or one of its subroutines reported the error. */
        return NULL;

    syntaxerror:
      default:
        reportError(NULL, JSMSG_SYNTAX_ERROR);
        return NULL;
    }
    return pn;
}

ParseNode *
Parser::parenExpr(bool *genexp)
{
    TokenPtr begin;
    ParseNode *pn;

    JS_ASSERT(tokenStream.currentToken().type == TOK_LP);
    begin = tokenStream.currentToken().pos.begin;

    if (genexp)
        *genexp = false;

    GenexpGuard guard(this);

    pn = bracketedExpr();
    if (!pn)
        return NULL;
    guard.endBody();

#if JS_HAS_GENERATOR_EXPRS
    if (tokenStream.matchToken(TOK_FOR)) {
        if (!guard.checkValidBody(pn))
            return NULL;
        JS_ASSERT(!pn->isKind(PNK_YIELD));
        if (pn->isKind(PNK_COMMA) && !pn->isInParens()) {
            reportError(pn->last(), JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
            return NULL;
        }
        pn = generatorExpr(pn);
        if (!pn)
            return NULL;
        pn->pn_pos.begin = begin;
        if (genexp) {
            if (tokenStream.getToken() != TOK_RP) {
                reportError(NULL, JSMSG_BAD_GENERATOR_SYNTAX, js_generator_str);
                return NULL;
            }
            pn->pn_pos.end = tokenStream.currentToken().pos.end;
            *genexp = true;
        }
    } else
#endif /* JS_HAS_GENERATOR_EXPRS */

    if (!guard.maybeNoteGenerator(pn))
        return NULL;

    return pn;
}


/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Parser_h__
#define Parser_h__

/*
 * JS parser definitions.
 */
#include "jsversion.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsatom.h"
#include "jsscript.h"
#include "jswin.h"

#include "frontend/ParseMaps.h"
#include "frontend/ParseNode.h"
#include "frontend/TreeContext.h"

typedef struct BindData BindData;

namespace js {

class StaticBlockObject;

enum FunctionSyntaxKind { Expression, Statement };
enum LetContext { LetExpresion, LetStatement };
enum VarContext { HoistVars, DontHoistVars };

struct Parser : private AutoGCRooter
{
    JSContext           *const context; /* FIXME Bug 551291: use AutoGCRooter::context? */
    StrictModeGetter    strictModeGetter; /* used by tokenStream to test for strict mode */
    TokenStream         tokenStream;
    void                *tempPoolMark;  /* initial JSContext.tempLifoAlloc mark */
    JSPrincipals        *principals;    /* principals associated with source */
    JSPrincipals        *originPrincipals;   /* see jsapi.h 'originPrincipals' comment */
    StackFrame          *const callerFrame;  /* scripted caller frame for eval and dbgapi */
    ParseNodeAllocator  allocator;
    ObjectBox           *traceListHead; /* list of parsed object for GC tracing */

    TreeContext         *tc;            /* innermost tree context (stack-allocated) */

    /* Root atoms and objects allocated for the parsed tree. */
    AutoKeepAtoms       keepAtoms;

    /* Perform constant-folding; must be true when interfacing with the emitter. */
    const bool          foldConstants:1;

    /* Script can optimize name references based on scope chain. */
    const bool          compileAndGo:1;

    Parser(JSContext *cx, JSPrincipals *prin, JSPrincipals *originPrin,
           const jschar *chars, size_t length, const char *fn, unsigned ln, JSVersion version,
           StackFrame *cfp, bool foldConstants, bool compileAndGo);
    ~Parser();

    friend void AutoGCRooter::trace(JSTracer *trc);
    friend struct TreeContext;

    /*
     * Initialize a parser. The compiler owns the arena pool "tops-of-stack"
     * space above the current JSContext.tempLifoAlloc mark. This means you
     * cannot allocate from tempLifoAlloc and save the pointer beyond the next
     * Parser destructor invocation.
     */
    bool init();

    void setPrincipals(JSPrincipals *prin, JSPrincipals *originPrin);

    const char *getFilename() const { return tokenStream.getFilename(); }
    JSVersion versionWithFlags() const { return tokenStream.versionWithFlags(); }
    JSVersion versionNumber() const { return tokenStream.versionNumber(); }

    /*
     * Parse a top-level JS script.
     */
    ParseNode *parse(JSObject *chain);

#if JS_HAS_XML_SUPPORT
    ParseNode *parseXMLText(JSObject *chain, bool allowList);
#endif

    /*
     * Allocate a new parsed object or function container from
     * cx->tempLifoAlloc.
     */
    ObjectBox *newObjectBox(JSObject *obj);

    FunctionBox *newFunctionBox(JSObject *obj, ParseNode *fn, TreeContext *tc);

    /*
     * Create a new function object given tree context (tc) and a name (which
     * is optional if this is a function expression).
     */
    JSFunction *newFunction(TreeContext *tc, JSAtom *atom, FunctionSyntaxKind kind);

    void trace(JSTracer *trc);

    /*
     * Report a parse (compile) error.
     */
    inline bool reportErrorNumber(ParseNode *pn, unsigned flags, unsigned errorNumber, ...);

  private:
    ParseNode *allocParseNode(size_t size) {
        JS_ASSERT(size == sizeof(ParseNode));
        return static_cast<ParseNode *>(allocator.allocNode());
    }

    /*
     * Create a parse node with the given kind and op using the current token's
     * atom.
     */
    ParseNode *atomNode(ParseNodeKind kind, JSOp op);

  public:
    ParseNode *freeTree(ParseNode *pn) { return allocator.freeTree(pn); }
    void prepareNodeForMutation(ParseNode *pn) { return allocator.prepareNodeForMutation(pn); }

    /* new_ methods for creating parse nodes. These report OOM on context. */
    JS_DECLARE_NEW_METHODS(allocParseNode, inline)

    ParseNode *cloneNode(const ParseNode &other) {
        ParseNode *node = allocParseNode(sizeof(ParseNode));
        if (!node)
            return NULL;
        PodAssign(node, &other);
        return node;
    }

    /* Public entry points for parsing. */
    ParseNode *statement();
    bool recognizeDirectivePrologue(ParseNode *pn, bool *isDirectivePrologueMember);

    /*
     * Parse a function body.  Pass StatementListBody if the body is a list of
     * statements; pass ExpressionBody if the body is a single expression.
     */
    enum FunctionBodyType { StatementListBody, ExpressionBody };
    ParseNode *functionBody(FunctionBodyType type);

    bool checkForArgumentsAndRest();

  private:
    /*
     * JS parsers, from lowest to highest precedence.
     *
     * Each parser must be called during the dynamic scope of a TreeContext
     * object, pointed to by this->tc.
     *
     * Each returns a parse node tree or null on error.
     *
     * Parsers whose name has a '1' suffix leave the TokenStream state
     * pointing to the token one past the end of the parsed fragment.  For a
     * number of the parsers this is convenient and avoids a lot of
     * unnecessary ungetting and regetting of tokens.
     *
     * Some parsers have two versions:  an always-inlined version (with an 'i'
     * suffix) and a never-inlined version (with an 'n' suffix).
     */
    ParseNode *functionStmt();
    ParseNode *functionExpr();
    ParseNode *statements(bool *hasFunctionStmt = NULL);

    ParseNode *switchStatement();
    ParseNode *forStatement();
    ParseNode *tryStatement();
    ParseNode *withStatement();
#if JS_HAS_BLOCK_SCOPE
    ParseNode *letStatement();
#endif
    ParseNode *expressionStatement();
    ParseNode *variables(ParseNodeKind kind, StaticBlockObject *blockObj = NULL,
                         VarContext varContext = HoistVars);
    ParseNode *expr();
    ParseNode *assignExpr();
    ParseNode *assignExprWithoutYield(unsigned err);
    ParseNode *condExpr1();
    ParseNode *orExpr1();
    ParseNode *andExpr1i();
    ParseNode *andExpr1n();
    ParseNode *bitOrExpr1i();
    ParseNode *bitOrExpr1n();
    ParseNode *bitXorExpr1i();
    ParseNode *bitXorExpr1n();
    ParseNode *bitAndExpr1i();
    ParseNode *bitAndExpr1n();
    ParseNode *eqExpr1i();
    ParseNode *eqExpr1n();
    ParseNode *relExpr1i();
    ParseNode *relExpr1n();
    ParseNode *shiftExpr1i();
    ParseNode *shiftExpr1n();
    ParseNode *addExpr1i();
    ParseNode *addExpr1n();
    ParseNode *mulExpr1i();
    ParseNode *mulExpr1n();
    ParseNode *unaryExpr();
    ParseNode *memberExpr(JSBool allowCallSyntax);
    ParseNode *primaryExpr(TokenKind tt, bool afterDoubleDot);
    ParseNode *parenExpr(JSBool *genexp = NULL);

    /*
     * Additional JS parsers.
     */
    enum FunctionType { Getter, Setter, Normal };
    bool functionArguments(ParseNode **list, bool &hasDefaults, bool &hasRest);

    ParseNode *functionDef(HandlePropertyName name, FunctionType type, FunctionSyntaxKind kind);

    ParseNode *unaryOpExpr(ParseNodeKind kind, JSOp op);

    ParseNode *condition();
    ParseNode *comprehensionTail(ParseNode *kid, unsigned blockid, bool isGenexp,
                                 ParseNodeKind kind = PNK_SEMI, JSOp op = JSOP_NOP);
    ParseNode *generatorExpr(ParseNode *kid);
    JSBool argumentList(ParseNode *listNode);
    ParseNode *bracketedExpr();
    ParseNode *letBlock(LetContext letContext);
    ParseNode *returnOrYield(bool useAssignExpr);
    ParseNode *destructuringExpr(BindData *data, TokenKind tt);

    bool checkForFunctionNode(PropertyName *name, ParseNode *node);

    ParseNode *identifierName(bool afterDoubleDot);

#if JS_HAS_XML_SUPPORT
    // True if E4X syntax is allowed in the current syntactic context.
    bool allowsXML() const { return !tc->sc->inStrictMode() && tokenStream.allowsXML(); }

    ParseNode *endBracketedExpr();

    ParseNode *propertySelector();
    ParseNode *qualifiedSuffix(ParseNode *pn);
    ParseNode *qualifiedIdentifier();
    ParseNode *attributeIdentifier();
    ParseNode *xmlExpr(JSBool inTag);
    ParseNode *xmlNameExpr();
    ParseNode *xmlTagContent(ParseNodeKind tagkind, JSAtom **namep);
    JSBool xmlElementContent(ParseNode *pn);
    ParseNode *xmlElementOrList(JSBool allowList);
    ParseNode *xmlElementOrListRoot(JSBool allowList);

    ParseNode *starOrAtPropertyIdentifier(TokenKind tt);
    ParseNode *propertyQualifiedIdentifier();
#endif /* JS_HAS_XML_SUPPORT */

    bool setAssignmentLhsOps(ParseNode *pn, JSOp op);
    bool matchInOrOf(bool *isForOfp);
};

inline bool
Parser::reportErrorNumber(ParseNode *pn, unsigned flags, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream.reportCompileErrorNumberVA(pn, flags, errorNumber, args);
    va_end(args);
    return result;
}

bool
DefineArg(ParseNode *pn, JSAtom *atom, unsigned i, Parser *parser);

} /* namespace js */

/*
 * Convenience macro to access Parser.tokenStream as a pointer.
 */
#define TS(p) (&(p)->tokenStream)

#endif /* Parser_h__ */

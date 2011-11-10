/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#define NUM_TEMP_FREELISTS      6U      /* 32 to 2048 byte size classes (32 bit) */

typedef struct BindData BindData;

namespace js {

enum FunctionSyntaxKind { Expression, Statement };

struct Parser : private AutoGCRooter
{
    JSContext           *const context; /* FIXME Bug 551291: use AutoGCRooter::context? */
    void                *tempFreeList[NUM_TEMP_FREELISTS];
    TokenStream         tokenStream;
    void                *tempPoolMark;  /* initial JSContext.tempLifoAlloc mark */
    JSPrincipals        *principals;    /* principals associated with source */
    StackFrame          *const callerFrame;  /* scripted caller frame for eval and dbgapi */
    JSObject            *const callerVarObj; /* callerFrame's varObj */
    ParseNodeAllocator  allocator;
    uint32              functionCount;  /* number of functions in current unit */
    ObjectBox           *traceListHead; /* list of parsed object for GC tracing */
    TreeContext         *tc;            /* innermost tree context (stack-allocated) */

    /* Root atoms and objects allocated for the parsed tree. */
    AutoKeepAtoms       keepAtoms;

    /* Perform constant-folding; must be true when interfacing with the emitter. */
    bool                foldConstants;

    Parser(JSContext *cx, JSPrincipals *prin = NULL, StackFrame *cfp = NULL, bool fold = true);
    ~Parser();

    friend void AutoGCRooter::trace(JSTracer *trc);
    friend struct TreeContext;

    /*
     * Initialize a parser. Parameters are passed on to init tokenStream. The
     * compiler owns the arena pool "tops-of-stack" space above the current
     * JSContext.tempLifoAlloc mark. This means you cannot allocate from
     * tempLifoAlloc and save the pointer beyond the next Parser destructor
     * invocation.
     */
    bool init(const jschar *base, size_t length, const char *filename, uintN lineno,
              JSVersion version);

    void setPrincipals(JSPrincipals *prin);

    const char *getFilename() const { return tokenStream.getFilename(); }
    JSVersion versionWithFlags() const { return tokenStream.versionWithFlags(); }
    JSVersion versionNumber() const { return tokenStream.versionNumber(); }
    bool hasXML() const { return tokenStream.hasXML(); }

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
    inline bool reportErrorNumber(ParseNode *pn, uintN flags, uintN errorNumber, ...);

  private:
    void *allocParseNode(size_t size) {
        JS_ASSERT(size == sizeof(ParseNode));
        return allocator.allocNode();
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

    /* Public entry points for parsing. */
    ParseNode *statement();
    bool recognizeDirectivePrologue(ParseNode *pn, bool *isDirectivePrologueMember);
    ParseNode *functionBody();

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
    ParseNode *statements();

    ParseNode *switchStatement();
    ParseNode *forStatement();
    ParseNode *tryStatement();
    ParseNode *withStatement();
#if JS_HAS_BLOCK_SCOPE
    ParseNode *letStatement();
#endif
    ParseNode *expressionStatement();
    ParseNode *variables(bool inLetHead);
    ParseNode *expr();
    ParseNode *assignExpr();
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
    ParseNode *primaryExpr(TokenKind tt, JSBool afterDot);
    ParseNode *parenExpr(JSBool *genexp = NULL);

    /*
     * Additional JS parsers.
     */
    enum FunctionType { Getter, Setter, Normal };
    bool functionArguments(TreeContext &funtc, FunctionBox *funbox, ParseNode **list);

    ParseNode *functionDef(PropertyName *name, FunctionType type, FunctionSyntaxKind kind);

    ParseNode *unaryOpExpr(ParseNodeKind kind, JSOp op);

    ParseNode *condition();
    ParseNode *comprehensionTail(ParseNode *kid, uintN blockid, bool isGenexp,
                                 ParseNodeKind kind = PNK_SEMI, JSOp op = JSOP_NOP);
    ParseNode *generatorExpr(ParseNode *kid);
    JSBool argumentList(ParseNode *listNode);
    ParseNode *bracketedExpr();
    ParseNode *letBlock(JSBool statement);
    ParseNode *returnOrYield(bool useAssignExpr);
    ParseNode *destructuringExpr(BindData *data, TokenKind tt);

#if JS_HAS_XML_SUPPORT
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
#endif /* JS_HAS_XML_SUPPORT */

    bool setAssignmentLhsOps(ParseNode *pn, JSOp op);
};

inline bool
Parser::reportErrorNumber(ParseNode *pn, uintN flags, uintN errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream.reportCompileErrorNumberVA(pn, flags, errorNumber, args);
    va_end(args);
    return result;
}

bool
CheckStrictParameters(JSContext *cx, TreeContext *tc);

bool
DefineArg(ParseNode *pn, JSAtom *atom, uintN i, TreeContext *tc);

} /* namespace js */

/*
 * Convenience macro to access Parser.tokenStream as a pointer.
 */
#define TS(p) (&(p)->tokenStream)

#endif /* Parser_h__ */

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

JS_BEGIN_EXTERN_C

namespace js {

struct GlobalScope {
    GlobalScope(JSContext *cx, JSObject *globalObj, JSCodeGenerator *cg)
      : globalObj(globalObj), cg(cg), defs(cx), names(cx)
    { }

    struct GlobalDef {
        JSAtom        *atom;        // If non-NULL, specifies the property name to add.
        JSFunctionBox *funbox;      // If non-NULL, function value for the property.
                                    // This value is only set/used if atom is non-NULL.
        uint32        knownSlot;    // If atom is NULL, this is the known shape slot.

        GlobalDef() { }
        GlobalDef(uint32 knownSlot)
          : atom(NULL), knownSlot(knownSlot)
        { }
        GlobalDef(JSAtom *atom, JSFunctionBox *box) :
          atom(atom), funbox(box)
        { }
    };

    JSObject        *globalObj;
    JSCodeGenerator *cg;

    /*
     * This is the table of global names encountered during parsing. Each
     * global name appears in the list only once, and the |names| table
     * maps back into |defs| for fast lookup.
     *
     * A definition may either specify an existing global property, or a new
     * one that must be added after compilation succeeds.
     */
    Vector<GlobalDef, 16> defs;
    AtomIndexMap      names;
};

} /* namespace js */

#define NUM_TEMP_FREELISTS      6U      /* 32 to 2048 byte size classes (32 bit) */

typedef struct BindData BindData;

namespace js {

enum FunctionSyntaxKind { Expression, Statement };

struct Parser : private js::AutoGCRooter
{
    JSContext           *const context; /* FIXME Bug 551291: use AutoGCRooter::context? */
    void                *tempFreeList[NUM_TEMP_FREELISTS];
    TokenStream         tokenStream;
    void                *tempPoolMark;  /* initial JSContext.tempPool mark */
    JSPrincipals        *principals;    /* principals associated with source */
    StackFrame          *const callerFrame;  /* scripted caller frame for eval and dbgapi */
    JSObject            *const callerVarObj; /* callerFrame's varObj */
    JSParseNode         *nodeList;      /* list of recyclable parse-node structs */
    uint32              functionCount;  /* number of functions in current unit */
    JSObjectBox         *traceListHead; /* list of parsed object for GC tracing */
    JSTreeContext       *tc;            /* innermost tree context (stack-allocated) */

    /* Root atoms and objects allocated for the parsed tree. */
    js::AutoKeepAtoms   keepAtoms;

    /* Perform constant-folding; must be true when interfacing with the emitter. */
    bool                foldConstants;

    Parser(JSContext *cx, JSPrincipals *prin = NULL, StackFrame *cfp = NULL, bool fold = true);
    ~Parser();

    friend void js::AutoGCRooter::trace(JSTracer *trc);
    friend struct ::JSTreeContext;
    friend struct Compiler;

    /*
     * Initialize a parser. Parameters are passed on to init tokenStream.
     * The compiler owns the arena pool "tops-of-stack" space above the current
     * JSContext.tempPool mark. This means you cannot allocate from tempPool
     * and save the pointer beyond the next Parser destructor invocation.
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
    JSParseNode *parse(JSObject *chain);

#if JS_HAS_XML_SUPPORT
    JSParseNode *parseXMLText(JSObject *chain, bool allowList);
#endif

    /*
     * Allocate a new parsed object or function container from cx->tempPool.
     */
    JSObjectBox *newObjectBox(JSObject *obj);

    JSFunctionBox *newFunctionBox(JSObject *obj, JSParseNode *fn, JSTreeContext *tc);

    /*
     * Create a new function object given tree context (tc) and a name (which
     * is optional if this is a function expression).
     */
    JSFunction *newFunction(JSTreeContext *tc, JSAtom *atom, FunctionSyntaxKind kind);

    /*
     * Analyze the tree of functions nested within a single compilation unit,
     * starting at funbox, recursively walking its kids, then following its
     * siblings, their kids, etc.
     */
    bool analyzeFunctions(JSTreeContext *tc);
    void cleanFunctionList(JSFunctionBox **funbox);
    bool markFunArgs(JSFunctionBox *funbox);
    void markExtensibleScopeDescendants(JSFunctionBox *funbox, bool hasExtensibleParent);
    void setFunctionKinds(JSFunctionBox *funbox, uint32 *tcflags);

    void trace(JSTracer *trc);

    /*
     * Report a parse (compile) error.
     */
    inline bool reportErrorNumber(JSParseNode *pn, uintN flags, uintN errorNumber, ...);

private:
    /*
     * JS parsers, from lowest to highest precedence.
     *
     * Each parser must be called during the dynamic scope of a JSTreeContext
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
    JSParseNode *functionStmt();
    JSParseNode *functionExpr();
    JSParseNode *statements();
    JSParseNode *statement();
    JSParseNode *switchStatement();
    JSParseNode *forStatement();
    JSParseNode *tryStatement();
    JSParseNode *withStatement();
#if JS_HAS_BLOCK_SCOPE
    JSParseNode *letStatement();
#endif
    JSParseNode *expressionStatement();
    JSParseNode *variables(bool inLetHead);
    JSParseNode *expr();
    JSParseNode *assignExpr();
    JSParseNode *condExpr1();
    JSParseNode *orExpr1();
    JSParseNode *andExpr1i();
    JSParseNode *andExpr1n();
    JSParseNode *bitOrExpr1i();
    JSParseNode *bitOrExpr1n();
    JSParseNode *bitXorExpr1i();
    JSParseNode *bitXorExpr1n();
    JSParseNode *bitAndExpr1i();
    JSParseNode *bitAndExpr1n();
    JSParseNode *eqExpr1i();
    JSParseNode *eqExpr1n();
    JSParseNode *relExpr1i();
    JSParseNode *relExpr1n();
    JSParseNode *shiftExpr1i();
    JSParseNode *shiftExpr1n();
    JSParseNode *addExpr1i();
    JSParseNode *addExpr1n();
    JSParseNode *mulExpr1i();
    JSParseNode *mulExpr1n();
    JSParseNode *unaryExpr();
    JSParseNode *memberExpr(JSBool allowCallSyntax);
    JSParseNode *primaryExpr(js::TokenKind tt, JSBool afterDot);
    JSParseNode *parenExpr(JSBool *genexp = NULL);

    /*
     * Additional JS parsers.
     */
    bool recognizeDirectivePrologue(JSParseNode *pn, bool *isDirectivePrologueMember);

    enum FunctionType { Getter, Setter, Normal };
    bool functionArguments(JSTreeContext &funtc, JSFunctionBox *funbox, JSParseNode **list);
    JSParseNode *functionBody();
    JSParseNode *functionDef(PropertyName *name, FunctionType type, FunctionSyntaxKind kind);

    JSParseNode *condition();
    JSParseNode *comprehensionTail(JSParseNode *kid, uintN blockid, bool isGenexp,
                                   js::TokenKind type = js::TOK_SEMI, JSOp op = JSOP_NOP);
    JSParseNode *generatorExpr(JSParseNode *kid);
    JSBool argumentList(JSParseNode *listNode);
    JSParseNode *bracketedExpr();
    JSParseNode *letBlock(JSBool statement);
    JSParseNode *returnOrYield(bool useAssignExpr);
    JSParseNode *destructuringExpr(BindData *data, js::TokenKind tt);

#if JS_HAS_XML_SUPPORT
    JSParseNode *endBracketedExpr();

    JSParseNode *propertySelector();
    JSParseNode *qualifiedSuffix(JSParseNode *pn);
    JSParseNode *qualifiedIdentifier();
    JSParseNode *attributeIdentifier();
    JSParseNode *xmlExpr(JSBool inTag);
    JSParseNode *xmlAtomNode();
    JSParseNode *xmlNameExpr();
    JSParseNode *xmlTagContent(js::TokenKind tagtype, JSAtom **namep);
    JSBool xmlElementContent(JSParseNode *pn);
    JSParseNode *xmlElementOrList(JSBool allowList);
    JSParseNode *xmlElementOrListRoot(JSBool allowList);
#endif /* JS_HAS_XML_SUPPORT */

    bool setAssignmentLhsOps(JSParseNode *pn, JSOp op);
};

inline bool
Parser::reportErrorNumber(JSParseNode *pn, uintN flags, uintN errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream.reportCompileErrorNumberVA(pn, flags, errorNumber, args);
    va_end(args);
    return result;
}

bool
CheckStrictParameters(JSContext *cx, JSTreeContext *tc);

bool
DefineArg(JSParseNode *pn, JSAtom *atom, uintN i, JSTreeContext *tc);

} /* namespace js */

/*
 * Convenience macro to access Parser.tokenStream as a pointer.
 */
#define TS(p) (&(p)->tokenStream)

JS_END_EXTERN_C

#endif /* Parser_h__ */

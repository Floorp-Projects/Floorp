/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FullParseHandler_h
#define frontend_FullParseHandler_h

#include "mozilla/PodOperations.h"

#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"

namespace js {
namespace frontend {

template <typename ParseHandler>
class Parser;

class SyntaxParseHandler;

// Parse handler used when generating a full parse tree for all code which the
// parser encounters.
class FullParseHandler
{
    ParseNodeAllocator allocator;
    TokenStream &tokenStream;
    bool foldConstants;

    ParseNode *allocParseNode(size_t size) {
        JS_ASSERT(size == sizeof(ParseNode));
        return static_cast<ParseNode *>(allocator.allocNode());
    }

    ParseNode *cloneNode(const ParseNode &other) {
        ParseNode *node = allocParseNode(sizeof(ParseNode));
        if (!node)
            return nullptr;
        mozilla::PodAssign(node, &other);
        return node;
    }

    /*
     * If this is a full parse to construct the bytecode for a function that
     * was previously lazily parsed, that lazy function and the current index
     * into its inner functions. We do not want to reparse the inner functions.
     */
    LazyScript * const lazyOuterFunction_;
    size_t lazyInnerFunctionIndex;

    const TokenPos &pos() {
        return tokenStream.currentToken().pos;
    }

  public:

    /*
     * If non-nullptr, points to a syntax parser which can be used for inner
     * functions. Cleared if language features not handled by the syntax parser
     * are encountered, in which case all future activity will use the full
     * parser.
     */
    Parser<SyntaxParseHandler> *syntaxParser;

    /* new_ methods for creating parse nodes. These report OOM on context. */
    JS_DECLARE_NEW_METHODS(new_, allocParseNode, inline)

    typedef ParseNode *Node;
    typedef Definition *DefinitionNode;

    FullParseHandler(ExclusiveContext *cx, LifoAlloc &alloc,
                     TokenStream &tokenStream, bool foldConstants,
                     Parser<SyntaxParseHandler> *syntaxParser, LazyScript *lazyOuterFunction)
      : allocator(cx, alloc),
        tokenStream(tokenStream),
        foldConstants(foldConstants),
        lazyOuterFunction_(lazyOuterFunction),
        lazyInnerFunctionIndex(0),
        syntaxParser(syntaxParser)
    {}

    static ParseNode *null() { return nullptr; }

    ParseNode *freeTree(ParseNode *pn) { return allocator.freeTree(pn); }
    void prepareNodeForMutation(ParseNode *pn) { return allocator.prepareNodeForMutation(pn); }
    const Token &currentToken() { return tokenStream.currentToken(); }

    ParseNode *newName(PropertyName *name, uint32_t blockid, const TokenPos &pos) {
        return new_<NameNode>(PNK_NAME, JSOP_NAME, name, blockid, pos);
    }

    ParseNode *newComputedName(ParseNode *expr, uint32_t begin, uint32_t end) {
        TokenPos pos(begin, end);
        return new_<UnaryNode>(PNK_COMPUTED_NAME, JSOP_NOP, pos, expr);
    }

    Definition *newPlaceholder(JSAtom *atom, uint32_t blockid, const TokenPos &pos) {
        Definition *dn =
            (Definition *) new_<NameNode>(PNK_NAME, JSOP_NOP, atom, blockid, pos);
        if (!dn)
            return nullptr;
        dn->setDefn(true);
        dn->pn_dflags |= PND_PLACEHOLDER;
        return dn;
    }

    ParseNode *newIdentifier(JSAtom *atom, const TokenPos &pos) {
        return new_<NullaryNode>(PNK_NAME, JSOP_NOP, pos, atom);
    }

    ParseNode *newNumber(double value, DecimalPoint decimalPoint, const TokenPos &pos) {
        ParseNode *pn = new_<NullaryNode>(PNK_NUMBER, pos);
        if (!pn)
            return nullptr;
        pn->initNumber(value, decimalPoint);
        return pn;
    }

    ParseNode *newBooleanLiteral(bool cond, const TokenPos &pos) {
        return new_<BooleanLiteral>(cond, pos);
    }

    ParseNode *newStringLiteral(JSAtom *atom, const TokenPos &pos) {
        return new_<NullaryNode>(PNK_STRING, JSOP_NOP, pos, atom);
    }

    ParseNode *newTemplateStringLiteral(JSAtom *atom, const TokenPos &pos) {
        return new_<NullaryNode>(PNK_TEMPLATE_STRING, JSOP_NOP, pos, atom);
    }

    ParseNode *newCallSiteObject(uint32_t begin, unsigned blockidGen) {
        ParseNode *callSite = new_<CallSiteNode>(begin);
        if (!callSite)
            return null();

        Node propExpr = newArrayLiteral(getPosition(callSite).begin, blockidGen);
        if (!propExpr)
            return null();

        if (!addArrayElement(callSite, propExpr))
            return null();

        return callSite;
    }

    bool addToCallSiteObject(ParseNode *callSiteObj, ParseNode *rawNode, ParseNode *cookedNode) {
        MOZ_ASSERT(callSiteObj->isKind(PNK_CALLSITEOBJ));

        if (!addArrayElement(callSiteObj, cookedNode))
            return false;
        if (!addArrayElement(callSiteObj->pn_head, rawNode))
            return false;

        /*
         * We don't know when the last noSubstTemplate will come in, and we
         * don't want to deal with this outside this method
         */
        setEndPosition(callSiteObj, callSiteObj->pn_head);
        return true;
    }

    ParseNode *newThisLiteral(const TokenPos &pos) {
        return new_<ThisLiteral>(pos);
    }

    ParseNode *newNullLiteral(const TokenPos &pos) {
        return new_<NullLiteral>(pos);
    }

    // The Boxer object here is any object that can allocate ObjectBoxes.
    // Specifically, a Boxer has a .newObjectBox(T) method that accepts a
    // Rooted<RegExpObject*> argument and returns an ObjectBox*.
    template <class Boxer>
    ParseNode *newRegExp(HandleObject reobj, const TokenPos &pos, Boxer &boxer) {
        ObjectBox *objbox = boxer.newObjectBox(reobj);
        if (!objbox)
            return null();
        return new_<RegExpLiteral>(objbox, pos);
    }

    ParseNode *newConditional(ParseNode *cond, ParseNode *thenExpr, ParseNode *elseExpr) {
        return new_<ConditionalExpression>(cond, thenExpr, elseExpr);
    }

    void markAsSetCall(ParseNode *pn) {
        pn->pn_xflags |= PNX_SETCALL;
    }

    ParseNode *newDelete(uint32_t begin, ParseNode *expr) {
        if (expr->getKind() == PNK_NAME) {
            expr->pn_dflags |= PND_DEOPTIMIZED;
            expr->setOp(JSOP_DELNAME);
        }
        return newUnary(PNK_DELETE, JSOP_NOP, begin, expr);
    }

    ParseNode *newNullary(ParseNodeKind kind, JSOp op, const TokenPos &pos) {
        return new_<NullaryNode>(kind, op, pos);
    }

    ParseNode *newUnary(ParseNodeKind kind, JSOp op, uint32_t begin, ParseNode *kid) {
        TokenPos pos(begin, kid ? kid->pn_pos.end : begin + 1);
        return new_<UnaryNode>(kind, op, pos, kid);
    }

    ParseNode *newBinary(ParseNodeKind kind, JSOp op = JSOP_NOP) {
        return new_<BinaryNode>(kind, op, pos(), (ParseNode *) nullptr, (ParseNode *) nullptr);
    }
    ParseNode *newBinary(ParseNodeKind kind, ParseNode *left,
                         JSOp op = JSOP_NOP) {
        return new_<BinaryNode>(kind, op, left->pn_pos, left, (ParseNode *) nullptr);
    }
    ParseNode *newBinary(ParseNodeKind kind, ParseNode *left, ParseNode *right,
                         JSOp op = JSOP_NOP) {
        TokenPos pos(left->pn_pos.begin, right->pn_pos.end);
        return new_<BinaryNode>(kind, op, pos, left, right);
    }
    ParseNode *newBinaryOrAppend(ParseNodeKind kind, ParseNode *left, ParseNode *right,
                                 ParseContext<FullParseHandler> *pc, JSOp op = JSOP_NOP)
    {
        return ParseNode::newBinaryOrAppend(kind, op, left, right, this, pc, foldConstants);
    }

    ParseNode *newTernary(ParseNodeKind kind,
                          ParseNode *first, ParseNode *second, ParseNode *third,
                          JSOp op = JSOP_NOP)
    {
        return new_<TernaryNode>(kind, op, first, second, third);
    }

    // Expressions

    ParseNode *newArrayComprehension(ParseNode *body, unsigned blockid, const TokenPos &pos) {
        JS_ASSERT(pos.begin <= body->pn_pos.begin);
        JS_ASSERT(body->pn_pos.end <= pos.end);
        ParseNode *pn = new_<ListNode>(PNK_ARRAYCOMP, pos);
        if (!pn)
            return nullptr;
        pn->pn_blockid = blockid;
        pn->append(body);
        return pn;
    }

    ParseNode *newArrayLiteral(uint32_t begin, unsigned blockid) {
        ParseNode *literal = new_<ListNode>(PNK_ARRAY, TokenPos(begin, begin + 1));
        // Later in this stack: remove dependency on this opcode.
        if (literal) {
            literal->setOp(JSOP_NEWINIT);
            literal->pn_blockid = blockid;
        }
        return literal;
    }

    bool addElision(ParseNode *literal, const TokenPos &pos) {
        ParseNode *elision = new_<NullaryNode>(PNK_ELISION, pos);
        if (!elision)
            return false;
        literal->append(elision);
        literal->pn_xflags |= PNX_SPECIALARRAYINIT | PNX_NONCONST;
        return true;
    }

    bool addSpreadElement(ParseNode *literal, uint32_t begin, ParseNode *inner) {
        TokenPos pos(begin, inner->pn_pos.end);
        ParseNode *spread = new_<UnaryNode>(PNK_SPREAD, JSOP_NOP, pos, inner);
        if (!spread)
            return null();
        literal->append(spread);
        literal->pn_xflags |= PNX_SPECIALARRAYINIT | PNX_NONCONST;
        return true;
    }

    bool addArrayElement(ParseNode *literal, ParseNode *element) {
        if (!element->isConstant())
            literal->pn_xflags |= PNX_NONCONST;
        literal->append(element);
        return true;
    }

    ParseNode *newObjectLiteral(uint32_t begin) {
        ParseNode *literal = new_<ListNode>(PNK_OBJECT, TokenPos(begin, begin + 1));
        // Later in this stack: remove dependency on this opcode.
        if (literal)
            literal->setOp(JSOP_NEWINIT);
        return literal;
    }

    bool addPropertyDefinition(ParseNode *literal, ParseNode *name, ParseNode *expr,
                               bool isShorthand = false) {
        JS_ASSERT(literal->isArity(PN_LIST));
        ParseNode *propdef = newBinary(isShorthand ? PNK_SHORTHAND : PNK_COLON, name, expr,
                                       JSOP_INITPROP);
        if (isShorthand)
            literal->pn_xflags |= PNX_NONCONST;
        if (!propdef)
            return false;
        literal->append(propdef);
        return true;
    }

    bool addAccessorPropertyDefinition(ParseNode *literal, ParseNode *name, ParseNode *fn, JSOp op)
    {
        JS_ASSERT(literal->isArity(PN_LIST));
        literal->pn_xflags |= PNX_NONCONST;

        ParseNode *propdef = newBinary(PNK_COLON, name, fn, op);
        if (!propdef)
            return false;
        literal->append(propdef);
        return true;
    }

    // Statements

    ParseNode *newStatementList(unsigned blockid, const TokenPos &pos) {
        ParseNode *pn = new_<ListNode>(PNK_STATEMENTLIST, pos);
        if (pn)
            pn->pn_blockid = blockid;
        return pn;
    }

    template <typename PC>
    void addStatementToList(ParseNode *list, ParseNode *stmt, PC *pc) {
        JS_ASSERT(list->isKind(PNK_STATEMENTLIST));

        if (stmt->isKind(PNK_FUNCTION)) {
            if (pc->atBodyLevel()) {
                // PNX_FUNCDEFS notifies the emitter that the block contains
                // body-level function definitions that should be processed
                // before the rest of nodes.
                list->pn_xflags |= PNX_FUNCDEFS;
            } else {
                // General deoptimization was done in Parser::functionDef.
                JS_ASSERT_IF(pc->sc->isFunctionBox(),
                             pc->sc->asFunctionBox()->hasExtensibleScope());
            }
        }

        list->append(stmt);
    }

    ParseNode *newEmptyStatement(const TokenPos &pos) {
        return new_<UnaryNode>(PNK_SEMI, JSOP_NOP, pos, (ParseNode *) nullptr);
    }

    ParseNode *newImportDeclaration(ParseNode *importSpecSet,
                                    ParseNode *moduleSpec, const TokenPos &pos)
    {
        ParseNode *pn = new_<BinaryNode>(PNK_IMPORT, JSOP_NOP, pos,
                                         importSpecSet, moduleSpec);
        if (!pn)
            return null();
        return pn;
    }

    ParseNode *newExportDeclaration(ParseNode *kid, const TokenPos &pos) {
        return new_<UnaryNode>(PNK_EXPORT, JSOP_NOP, pos, kid);
    }

    ParseNode *newExportFromDeclaration(uint32_t begin, ParseNode *exportSpecSet,
                                        ParseNode *moduleSpec)
    {
        ParseNode *pn = new_<BinaryNode>(PNK_EXPORT_FROM, JSOP_NOP, exportSpecSet, moduleSpec);
        if (!pn)
            return null();
        pn->pn_pos.begin = begin;
        return pn;
    }

    ParseNode *newExprStatement(ParseNode *expr, uint32_t end) {
        JS_ASSERT(expr->pn_pos.end <= end);
        return new_<UnaryNode>(PNK_SEMI, JSOP_NOP, TokenPos(expr->pn_pos.begin, end), expr);
    }

    ParseNode *newIfStatement(uint32_t begin, ParseNode *cond, ParseNode *thenBranch,
                              ParseNode *elseBranch)
    {
        ParseNode *pn = new_<TernaryNode>(PNK_IF, JSOP_NOP, cond, thenBranch, elseBranch);
        if (!pn)
            return null();
        pn->pn_pos.begin = begin;
        return pn;
    }

    ParseNode *newDoWhileStatement(ParseNode *body, ParseNode *cond, const TokenPos &pos) {
        return new_<BinaryNode>(PNK_DOWHILE, JSOP_NOP, pos, body, cond);
    }

    ParseNode *newWhileStatement(uint32_t begin, ParseNode *cond, ParseNode *body) {
        TokenPos pos(begin, body->pn_pos.end);
        return new_<BinaryNode>(PNK_WHILE, JSOP_NOP, pos, cond, body);
    }

    ParseNode *newForStatement(uint32_t begin, ParseNode *forHead, ParseNode *body,
                               unsigned iflags)
    {
        /* A FOR node is binary, left is loop control and right is the body. */
        JSOp op = forHead->isKind(PNK_FORIN) ? JSOP_ITER : JSOP_NOP;
        BinaryNode *pn = new_<BinaryNode>(PNK_FOR, op, TokenPos(begin, body->pn_pos.end),
                                          forHead, body);
        if (!pn)
            return null();
        pn->pn_iflags = iflags;
        return pn;
    }

    ParseNode *newForHead(ParseNodeKind kind, ParseNode *pn1, ParseNode *pn2, ParseNode *pn3,
                          const TokenPos &pos)
    {
        JS_ASSERT(kind == PNK_FORIN || kind == PNK_FOROF || kind == PNK_FORHEAD);
        return new_<TernaryNode>(kind, JSOP_NOP, pn1, pn2, pn3, pos);
    }

    ParseNode *newSwitchStatement(uint32_t begin, ParseNode *discriminant, ParseNode *caseList) {
        TokenPos pos(begin, caseList->pn_pos.end);
        return new_<BinaryNode>(PNK_SWITCH, JSOP_NOP, pos, discriminant, caseList);
    }

    ParseNode *newCaseOrDefault(uint32_t begin, ParseNode *expr, ParseNode *body) {
        TokenPos pos(begin, body->pn_pos.end);
        return new_<BinaryNode>(expr ? PNK_CASE : PNK_DEFAULT, JSOP_NOP, pos, expr, body);
    }

    ParseNode *newContinueStatement(PropertyName *label, const TokenPos &pos) {
        return new_<ContinueStatement>(label, pos);
    }

    ParseNode *newBreakStatement(PropertyName *label, const TokenPos &pos) {
        return new_<BreakStatement>(label, pos);
    }

    ParseNode *newReturnStatement(ParseNode *expr, const TokenPos &pos) {
        JS_ASSERT_IF(expr, pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(PNK_RETURN, JSOP_RETURN, pos, expr);
    }

    ParseNode *newWithStatement(uint32_t begin, ParseNode *expr, ParseNode *body,
                                ObjectBox *staticWith) {
        return new_<BinaryObjNode>(PNK_WITH, JSOP_NOP, TokenPos(begin, body->pn_pos.end),
                                   expr, body, staticWith);
    }

    ParseNode *newLabeledStatement(PropertyName *label, ParseNode *stmt, uint32_t begin) {
        return new_<LabeledStatement>(label, stmt, begin);
    }

    ParseNode *newThrowStatement(ParseNode *expr, const TokenPos &pos) {
        JS_ASSERT(pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(PNK_THROW, JSOP_THROW, pos, expr);
    }

    ParseNode *newTryStatement(uint32_t begin, ParseNode *body, ParseNode *catchList,
                               ParseNode *finallyBlock) {
        TokenPos pos(begin, (finallyBlock ? finallyBlock : catchList)->pn_pos.end);
        return new_<TernaryNode>(PNK_TRY, JSOP_NOP, body, catchList, finallyBlock, pos);
    }

    ParseNode *newDebuggerStatement(const TokenPos &pos) {
        return new_<DebuggerStatement>(pos);
    }

    ParseNode *newPropertyAccess(ParseNode *pn, PropertyName *name, uint32_t end) {
        return new_<PropertyAccess>(pn, name, pn->pn_pos.begin, end);
    }

    ParseNode *newPropertyByValue(ParseNode *lhs, ParseNode *index, uint32_t end) {
        return new_<PropertyByValue>(lhs, index, lhs->pn_pos.begin, end);
    }

    inline bool addCatchBlock(ParseNode *catchList, ParseNode *letBlock,
                              ParseNode *catchName, ParseNode *catchGuard, ParseNode *catchBody);

    inline void setLastFunctionArgumentDefault(ParseNode *funcpn, ParseNode *pn);
    inline ParseNode *newFunctionDefinition();
    void setFunctionBody(ParseNode *pn, ParseNode *kid) {
        pn->pn_body = kid;
    }
    void setFunctionBox(ParseNode *pn, FunctionBox *funbox) {
        JS_ASSERT(pn->isKind(PNK_FUNCTION));
        pn->pn_funbox = funbox;
    }
    void addFunctionArgument(ParseNode *pn, ParseNode *argpn) {
        pn->pn_body->append(argpn);
    }

    inline ParseNode *newLexicalScope(ObjectBox *blockbox);
    inline void setLexicalScopeBody(ParseNode *block, ParseNode *body);

    bool isOperationWithoutParens(ParseNode *pn, ParseNodeKind kind) {
        return pn->isKind(kind) && !pn->isInParens();
    }

    inline bool finishInitializerAssignment(ParseNode *pn, ParseNode *init, JSOp op);

    void setBeginPosition(ParseNode *pn, ParseNode *oth) {
        setBeginPosition(pn, oth->pn_pos.begin);
    }
    void setBeginPosition(ParseNode *pn, uint32_t begin) {
        pn->pn_pos.begin = begin;
        JS_ASSERT(pn->pn_pos.begin <= pn->pn_pos.end);
    }

    void setEndPosition(ParseNode *pn, ParseNode *oth) {
        setEndPosition(pn, oth->pn_pos.end);
    }
    void setEndPosition(ParseNode *pn, uint32_t end) {
        pn->pn_pos.end = end;
        JS_ASSERT(pn->pn_pos.begin <= pn->pn_pos.end);
    }

    void setPosition(ParseNode *pn, const TokenPos &pos) {
        pn->pn_pos = pos;
    }
    TokenPos getPosition(ParseNode *pn) {
        return pn->pn_pos;
    }

    ParseNode *newList(ParseNodeKind kind, ParseNode *kid = nullptr, JSOp op = JSOP_NOP) {
        ParseNode *pn = ListNode::create(kind, this);
        if (!pn)
            return nullptr;
        pn->setOp(op);
        pn->makeEmpty();
        if (kid) {
            pn->pn_pos.begin = kid->pn_pos.begin;
            pn->append(kid);
        }
        return pn;
    }
    void addList(ParseNode *pn, ParseNode *kid) {
        pn->append(kid);
    }

    bool isUnparenthesizedYield(ParseNode *pn) {
        return pn->isKind(PNK_YIELD) && !pn->isInParens();
    }

    void setOp(ParseNode *pn, JSOp op) {
        pn->setOp(op);
    }
    void setBlockId(ParseNode *pn, unsigned blockid) {
        pn->pn_blockid = blockid;
    }
    void setFlag(ParseNode *pn, unsigned flag) {
        pn->pn_dflags |= flag;
    }
    void setListFlag(ParseNode *pn, unsigned flag) {
        JS_ASSERT(pn->isArity(PN_LIST));
        pn->pn_xflags |= flag;
    }
    ParseNode *setInParens(ParseNode *pn) {
        pn->setInParens(true);
        return pn;
    }
    void setPrologue(ParseNode *pn) {
        pn->pn_prologue = true;
    }

    bool isConstant(ParseNode *pn) {
        return pn->isConstant();
    }
    PropertyName *isName(ParseNode *pn) {
        return pn->isKind(PNK_NAME) ? pn->pn_atom->asPropertyName() : nullptr;
    }
    bool isCall(ParseNode *pn) {
        return pn->isKind(PNK_CALL);
    }
    PropertyName *isGetProp(ParseNode *pn) {
        return pn->is<PropertyAccess>() ? &pn->as<PropertyAccess>().name() : nullptr;
    }
    JSAtom *isStringExprStatement(ParseNode *pn, TokenPos *pos) {
        if (JSAtom *atom = pn->isStringExprStatement()) {
            *pos = pn->pn_kid->pn_pos;
            return atom;
        }
        return nullptr;
    }

    inline ParseNode *makeAssignment(ParseNode *pn, ParseNode *rhs);

    static Definition *getDefinitionNode(Definition *dn) {
        return dn;
    }
    static Definition::Kind getDefinitionKind(Definition *dn) {
        return dn->kind();
    }
    void linkUseToDef(ParseNode *pn, Definition *dn)
    {
        JS_ASSERT(!pn->isUsed());
        JS_ASSERT(!pn->isDefn());
        JS_ASSERT(pn != dn->dn_uses);
        JS_ASSERT(dn->isDefn());
        pn->pn_link = dn->dn_uses;
        dn->dn_uses = pn;
        dn->pn_dflags |= pn->pn_dflags & PND_USE2DEF_FLAGS;
        pn->setUsed(true);
        pn->pn_lexdef = dn;
    }
    Definition *resolve(Definition *dn) {
        return dn->resolve();
    }
    void deoptimizeUsesWithin(Definition *dn, const TokenPos &pos)
    {
        for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
            JS_ASSERT(pnu->isUsed());
            JS_ASSERT(!pnu->isDefn());
            if (pnu->pn_pos.begin >= pos.begin && pnu->pn_pos.end <= pos.end)
                pnu->pn_dflags |= PND_DEOPTIMIZED;
        }
    }
    bool dependencyCovered(ParseNode *pn, unsigned blockid, bool functionScope) {
        return pn->pn_blockid >= blockid;
    }

    static uintptr_t definitionToBits(Definition *dn) {
        return uintptr_t(dn);
    }
    static Definition *definitionFromBits(uintptr_t bits) {
        return (Definition *) bits;
    }
    static Definition *nullDefinition() {
        return nullptr;
    }
    void disableSyntaxParser() {
        syntaxParser = nullptr;
    }

    LazyScript *lazyOuterFunction() {
        return lazyOuterFunction_;
    }
    JSFunction *nextLazyInnerFunction() {
        JS_ASSERT(lazyInnerFunctionIndex < lazyOuterFunction()->numInnerFunctions());
        return lazyOuterFunction()->innerFunctions()[lazyInnerFunctionIndex++];
    }
};

inline bool
FullParseHandler::addCatchBlock(ParseNode *catchList, ParseNode *letBlock,
                                ParseNode *catchName, ParseNode *catchGuard, ParseNode *catchBody)
{
    ParseNode *catchpn = newTernary(PNK_CATCH, catchName, catchGuard, catchBody);
    if (!catchpn)
        return false;

    catchList->append(letBlock);
    letBlock->pn_expr = catchpn;
    return true;
}

inline void
FullParseHandler::setLastFunctionArgumentDefault(ParseNode *funcpn, ParseNode *defaultValue)
{
    ParseNode *arg = funcpn->pn_body->last();
    arg->pn_dflags |= PND_DEFAULT;
    arg->pn_expr = defaultValue;
}

inline ParseNode *
FullParseHandler::newFunctionDefinition()
{
    ParseNode *pn = CodeNode::create(PNK_FUNCTION, this);
    if (!pn)
        return nullptr;
    pn->pn_body = nullptr;
    pn->pn_funbox = nullptr;
    pn->pn_cookie.makeFree();
    pn->pn_dflags = 0;
    return pn;
}

inline ParseNode *
FullParseHandler::newLexicalScope(ObjectBox *blockbox)
{
    ParseNode *pn = LexicalScopeNode::create(PNK_LEXICALSCOPE, this);
    if (!pn)
        return nullptr;

    pn->pn_objbox = blockbox;
    pn->pn_cookie.makeFree();
    pn->pn_dflags = 0;
    return pn;
}

inline void
FullParseHandler::setLexicalScopeBody(ParseNode *block, ParseNode *kid)
{
    block->pn_expr = kid;
}

inline bool
FullParseHandler::finishInitializerAssignment(ParseNode *pn, ParseNode *init, JSOp op)
{
    if (pn->isUsed()) {
        pn = makeAssignment(pn, init);
        if (!pn)
            return false;
    } else {
        pn->pn_expr = init;
    }

    pn->setOp((pn->pn_dflags & PND_BOUND)
              ? JSOP_SETLOCAL
              : (op == JSOP_DEFCONST)
              ? JSOP_SETCONST
              : JSOP_SETNAME);

    pn->markAsAssigned();

    /* The declarator's position must include the initializer. */
    pn->pn_pos.end = init->pn_pos.end;
    return true;
}

inline ParseNode *
FullParseHandler::makeAssignment(ParseNode *pn, ParseNode *rhs)
{
    ParseNode *lhs = cloneNode(*pn);
    if (!lhs)
        return nullptr;

    if (pn->isUsed()) {
        Definition *dn = pn->pn_lexdef;
        ParseNode **pnup = &dn->dn_uses;

        while (*pnup != pn)
            pnup = &(*pnup)->pn_link;
        *pnup = lhs;
        lhs->pn_link = pn->pn_link;
        pn->pn_link = nullptr;
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

} // frontend
} // js

#endif /* frontend_FullParseHandler_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FullParseHandler_h__
#define FullParseHandler_h__

#include "mozilla/PodOperations.h"

#include "ParseNode.h"
#include "SharedContext.h"

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
            return NULL;
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

  public:

    /*
     * If non-NULL, points to a syntax parser which can be used for inner
     * functions. Cleared if language features not handled by the syntax parser
     * are encountered, in which case all future activity will use the full
     * parser.
     */
    Parser<SyntaxParseHandler> *syntaxParser;

    /* new_ methods for creating parse nodes. These report OOM on context. */
    JS_DECLARE_NEW_METHODS(new_, allocParseNode, inline)

    typedef ParseNode *Node;
    typedef Definition *DefinitionNode;

    FullParseHandler(JSContext *cx, TokenStream &tokenStream, bool foldConstants,
                     Parser<SyntaxParseHandler> *syntaxParser, LazyScript *lazyOuterFunction)
      : allocator(cx),
        tokenStream(tokenStream),
        foldConstants(foldConstants),
        lazyOuterFunction_(lazyOuterFunction),
        lazyInnerFunctionIndex(0),
        syntaxParser(syntaxParser)
    {}

    static ParseNode *null() { return NULL; }

    ParseNode *freeTree(ParseNode *pn) { return allocator.freeTree(pn); }
    void prepareNodeForMutation(ParseNode *pn) { return allocator.prepareNodeForMutation(pn); }
    const Token &currentToken() { return tokenStream.currentToken(); }

    ParseNode *newName(PropertyName *name, ParseContext<FullParseHandler> *pc,
                       ParseNodeKind kind = PNK_NAME) {
        ParseNode *pn = NameNode::create(kind, name, this, pc);
        if (!pn)
            return NULL;
        pn->setOp(JSOP_NAME);
        return pn;
    }
    Definition *newPlaceholder(JSAtom *atom, ParseContext<FullParseHandler> *pc) {
        Definition *dn = (Definition *) NameNode::create(PNK_NAME, atom, this, pc);
        if (!dn)
            return NULL;

        dn->setOp(JSOP_NOP);
        dn->setDefn(true);
        dn->pn_dflags |= PND_PLACEHOLDER;
        return dn;
    }
    ParseNode *newAtom(ParseNodeKind kind, JSAtom *atom, JSOp op = JSOP_NOP) {
        ParseNode *pn = NullaryNode::create(kind, this);
        if (!pn)
            return NULL;
        pn->setOp(op);
        pn->pn_atom = atom;
        return pn;
    }
    ParseNode *newNumber(double value, DecimalPoint decimalPoint = NoDecimal) {
        ParseNode *pn = NullaryNode::create(PNK_NUMBER, this);
        if (!pn)
            return NULL;
        pn->initNumber(value, decimalPoint);
        return pn;
    }
    ParseNode *newNumber(const Token &tok) {
        return newNumber(tok.number(), tok.decimalPoint());
    }
    ParseNode *newBooleanLiteral(bool cond, const TokenPos &pos) {
        return new_<BooleanLiteral>(cond, pos);
    }
    ParseNode *newThisLiteral(const TokenPos &pos) {
        return new_<ThisLiteral>(pos);
    }
    ParseNode *newNullLiteral(const TokenPos &pos) {
        return new_<NullLiteral>(pos);
    }
    ParseNode *newConditional(ParseNode *cond, ParseNode *thenExpr, ParseNode *elseExpr) {
        return new_<ConditionalExpression>(cond, thenExpr, elseExpr);
    }

    ParseNode *newNullary(ParseNodeKind kind) {
        return NullaryNode::create(kind, this);
    }

    ParseNode *newUnary(ParseNodeKind kind, ParseNode *kid, JSOp op = JSOP_NOP) {
        return new_<UnaryNode>(kind, op, kid->pn_pos, kid);
    }
    ParseNode *newUnary(ParseNodeKind kind, JSOp op = JSOP_NOP) {
        return new_<UnaryNode>(kind, op, tokenStream.currentToken().pos, (ParseNode *) NULL);
    }
    void setUnaryKid(ParseNode *pn, ParseNode *kid) {
        pn->pn_kid = kid;
        pn->pn_pos.end = kid->pn_pos.end;
    }

    ParseNode *newBinary(ParseNodeKind kind, JSOp op = JSOP_NOP) {
        return new_<BinaryNode>(kind, op, tokenStream.currentToken().pos,
                                (ParseNode *) NULL, (ParseNode *) NULL);
    }
    ParseNode *newBinary(ParseNodeKind kind, ParseNode *left,
                         JSOp op = JSOP_NOP) {
        return new_<BinaryNode>(kind, op, left->pn_pos, left, (ParseNode *) NULL);
    }
    ParseNode *newBinary(ParseNodeKind kind, ParseNode *left, ParseNode *right,
                         JSOp op = JSOP_NOP) {
        TokenPos pos = TokenPos::make(left->pn_pos.begin, right->pn_pos.end);
        return new_<BinaryNode>(kind, op, pos, left, right);
    }
    ParseNode *newBinaryOrAppend(ParseNodeKind kind, ParseNode *left, ParseNode *right,
                                 ParseContext<FullParseHandler> *pc, JSOp op = JSOP_NOP) {
        return ParseNode::newBinaryOrAppend(kind, op, left, right, this, pc, foldConstants);
    }
    void setBinaryRHS(ParseNode *pn, ParseNode *rhs) {
        JS_ASSERT(pn->isArity(PN_BINARY));
        pn->pn_right = rhs;
        pn->pn_pos.end = rhs->pn_pos.end;
    }

    ParseNode *newTernary(ParseNodeKind kind,
                          ParseNode *first, ParseNode *second, ParseNode *third,
                          JSOp op = JSOP_NOP) {
        return new_<TernaryNode>(kind, op, first, second, third);
    }

    ParseNode *newBreak(PropertyName *label, uint32_t begin, uint32_t end) {
        return new_<BreakStatement>(label, begin, end);
    }
    ParseNode *newContinue(PropertyName *label, uint32_t begin, uint32_t end) {
        return new_<ContinueStatement>(label, begin, end);
    }
    ParseNode *newDebuggerStatement(const TokenPos &pos) {
        return new_<DebuggerStatement>(pos);
    }
    ParseNode *newPropertyAccess(ParseNode *pn, PropertyName *name, uint32_t end) {
        return new_<PropertyAccess>(pn, name, pn->pn_pos.begin, end);
    }
    ParseNode *newPropertyByValue(ParseNode *pn, ParseNode *kid, uint32_t end) {
        return new_<PropertyByValue>(pn, kid, pn->pn_pos.begin, end);
    }

    inline bool addCatchBlock(ParseNode *catchList, ParseNode *letBlock,
                              ParseNode *catchName, ParseNode *catchGuard, ParseNode *catchBody);

    inline void morphNameIntoLabel(ParseNode *name, ParseNode *statement);

    inline void setLeaveBlockResult(ParseNode *block, ParseNode *kid, bool leaveBlockExpr);

    inline void setLastFunctionArgumentDefault(ParseNode *funcpn, ParseNode *pn);
    inline ParseNode *newFunctionDefinition();
    void setFunctionBody(ParseNode *pn, ParseNode *kid) {
        pn->pn_body = kid;
    }
    void setFunctionBox(ParseNode *pn, FunctionBox *funbox) {
        pn->pn_funbox = funbox;
    }
    void addFunctionArgument(ParseNode *pn, ParseNode *argpn) {
        pn->pn_body->append(argpn);
    }
    inline ParseNode *newLexicalScope(ObjectBox *blockbox);
    bool isOperationWithoutParens(ParseNode *pn, ParseNodeKind kind) {
        return pn->isKind(kind) && !pn->isInParens();
    }

    inline void noteLValue(ParseNode *pn);
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

    ParseNode *newList(ParseNodeKind kind, ParseNode *kid = NULL, JSOp op = JSOP_NOP) {
        ParseNode *pn = ListNode::create(kind, this);
        if (!pn)
            return NULL;
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
        return pn->isKind(PNK_NAME) ? pn->pn_atom->asPropertyName() : NULL;
    }
    PropertyName *isGetProp(ParseNode *pn) {
        return pn->isOp(JSOP_GETPROP) ? pn->pn_atom->asPropertyName() : NULL;
    }
    JSAtom *isStringExprStatement(ParseNode *pn, TokenPos *pos) {
        if (JSAtom *atom = pn->isStringExprStatement()) {
            *pos = pn->pn_pos;
            return atom;
        }
        return NULL;
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
        return NULL;
    }
    void disableSyntaxParser() {
        syntaxParser = NULL;
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
FullParseHandler::morphNameIntoLabel(ParseNode *name, ParseNode *statement)
{
    name->setKind(PNK_COLON);
    name->pn_pos.end = statement->pn_pos.end;
    name->pn_expr = statement;
}

inline void
FullParseHandler::setLeaveBlockResult(ParseNode *block, ParseNode *kid, bool leaveBlockExpr)
{
    JS_ASSERT(block->isOp(JSOP_LEAVEBLOCK));
    if (leaveBlockExpr)
        block->setOp(JSOP_LEAVEBLOCKEXPR);
    block->pn_expr = kid;
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
        return NULL;
    pn->pn_body = NULL;
    pn->pn_funbox = NULL;
    pn->pn_cookie.makeFree();
    pn->pn_dflags = 0;
    return pn;
}

inline ParseNode *
FullParseHandler::newLexicalScope(ObjectBox *blockbox)
{
    ParseNode *pn = LexicalScopeNode::create(PNK_LEXICALSCOPE, this);
    if (!pn)
        return NULL;

    pn->setOp(JSOP_LEAVEBLOCK);
    pn->pn_objbox = blockbox;
    pn->pn_cookie.makeFree();
    pn->pn_dflags = 0;
    return pn;
}

inline void
FullParseHandler::noteLValue(ParseNode *pn)
{
    if (pn->isUsed())
        pn->pn_lexdef->pn_dflags |= PND_ASSIGNED;

    pn->pn_dflags |= PND_ASSIGNED;
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

    noteLValue(pn);

    /* The declarator's position must include the initializer. */
    pn->pn_pos.end = init->pn_pos.end;
    return true;
}

inline ParseNode *
FullParseHandler::makeAssignment(ParseNode *pn, ParseNode *rhs)
{
    ParseNode *lhs = cloneNode(*pn);
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

} // frontend
} // js

#endif /* FullParseHandler_h__ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FullParseHandler_h
#define frontend_FullParseHandler_h

#include "mozilla/Attributes.h"
#include "mozilla/PodOperations.h"

#include <string.h>

#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"

namespace js {

class RegExpObject;

namespace frontend {

// Parse handler used when generating a full parse tree for all code which the
// parser encounters.
class FullParseHandler
{
    ParseNodeAllocator allocator;

    ParseNode* allocParseNode(size_t size) {
        MOZ_ASSERT(size == sizeof(ParseNode));
        return static_cast<ParseNode*>(allocator.allocNode());
    }

    /*
     * If this is a full parse to construct the bytecode for a function that
     * was previously lazily parsed, that lazy function and the current index
     * into its inner functions. We do not want to reparse the inner functions.
     */
    const Rooted<LazyScript*> lazyOuterFunction_;
    size_t lazyInnerFunctionIndex;
    size_t lazyClosedOverBindingIndex;

  public:
    /* new_ methods for creating parse nodes. These report OOM on context. */
    JS_DECLARE_NEW_METHODS(new_, allocParseNode, inline)

    typedef ParseNode* Node;

    bool isPropertyAccess(ParseNode* node) {
        return node->isKind(PNK_DOT) || node->isKind(PNK_ELEM);
    }

    bool isFunctionCall(ParseNode* node) {
        // Note: super() is a special form, *not* a function call.
        return node->isKind(PNK_CALL);
    }

    static bool isUnparenthesizedDestructuringPattern(ParseNode* node) {
        return !node->isInParens() && (node->isKind(PNK_OBJECT) || node->isKind(PNK_ARRAY));
    }

    static bool isParenthesizedDestructuringPattern(ParseNode* node) {
        // Technically this isn't a destructuring pattern at all -- the grammar
        // doesn't treat it as such.  But we need to know when this happens to
        // consider it a SyntaxError rather than an invalid-left-hand-side
        // ReferenceError.
        return node->isInParens() && (node->isKind(PNK_OBJECT) || node->isKind(PNK_ARRAY));
    }

    FullParseHandler(JSContext* cx, LifoAlloc& alloc, LazyScript* lazyOuterFunction)
      : allocator(cx, alloc),
        lazyOuterFunction_(cx, lazyOuterFunction),
        lazyInnerFunctionIndex(0),
        lazyClosedOverBindingIndex(0)
    {}

    static ParseNode* null() { return nullptr; }

    ParseNode* freeTree(ParseNode* pn) { return allocator.freeTree(pn); }
    void prepareNodeForMutation(ParseNode* pn) { return allocator.prepareNodeForMutation(pn); }

    ParseNode* newName(PropertyName* name, const TokenPos& pos, JSContext* cx)
    {
        return new_<NameNode>(PNK_NAME, JSOP_GETNAME, name, pos);
    }

    ParseNode* newComputedName(ParseNode* expr, uint32_t begin, uint32_t end) {
        TokenPos pos(begin, end);
        return new_<UnaryNode>(PNK_COMPUTED_NAME, pos, expr);
    }

    ParseNode* newObjectLiteralPropertyName(JSAtom* atom, const TokenPos& pos) {
        return new_<NullaryNode>(PNK_OBJECT_PROPERTY_NAME, JSOP_NOP, pos, atom);
    }

    ParseNode* newNumber(double value, DecimalPoint decimalPoint, const TokenPos& pos) {
        ParseNode* pn = new_<NullaryNode>(PNK_NUMBER, pos);
        if (!pn)
            return nullptr;
        pn->initNumber(value, decimalPoint);
        return pn;
    }

    ParseNode* newBooleanLiteral(bool cond, const TokenPos& pos) {
        return new_<BooleanLiteral>(cond, pos);
    }

    ParseNode* newStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return new_<NullaryNode>(PNK_STRING, JSOP_NOP, pos, atom);
    }

    ParseNode* newTemplateStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return new_<NullaryNode>(PNK_TEMPLATE_STRING, JSOP_NOP, pos, atom);
    }

    ParseNode* newCallSiteObject(uint32_t begin) {
        ParseNode* callSite = new_<CallSiteNode>(begin);
        if (!callSite)
            return null();

        Node propExpr = newArrayLiteral(callSite->pn_pos.begin);
        if (!propExpr)
            return null();

        addArrayElement(callSite, propExpr);

        return callSite;
    }

    void addToCallSiteObject(ParseNode* callSiteObj, ParseNode* rawNode, ParseNode* cookedNode) {
        MOZ_ASSERT(callSiteObj->isKind(PNK_CALLSITEOBJ));

        addArrayElement(callSiteObj, cookedNode);
        addArrayElement(callSiteObj->pn_head, rawNode);

        /*
         * We don't know when the last noSubstTemplate will come in, and we
         * don't want to deal with this outside this method
         */
        setEndPosition(callSiteObj, callSiteObj->pn_head);
    }

    ParseNode* newThisLiteral(const TokenPos& pos, ParseNode* thisName) {
        return new_<ThisLiteral>(pos, thisName);
    }

    ParseNode* newNullLiteral(const TokenPos& pos) {
        return new_<NullLiteral>(pos);
    }

    ParseNode* newRawUndefinedLiteral(const TokenPos& pos) {
        return new_<RawUndefinedLiteral>(pos);
    }

    // The Boxer object here is any object that can allocate ObjectBoxes.
    // Specifically, a Boxer has a .newObjectBox(T) method that accepts a
    // Rooted<RegExpObject*> argument and returns an ObjectBox*.
    template <class Boxer>
    ParseNode* newRegExp(RegExpObject* reobj, const TokenPos& pos, Boxer& boxer) {
        ObjectBox* objbox = boxer.newObjectBox(reobj);
        if (!objbox)
            return null();
        return new_<RegExpLiteral>(objbox, pos);
    }

    ParseNode* newConditional(ParseNode* cond, ParseNode* thenExpr, ParseNode* elseExpr) {
        return new_<ConditionalExpression>(cond, thenExpr, elseExpr);
    }

    ParseNode* newDelete(uint32_t begin, ParseNode* expr) {
        if (expr->isKind(PNK_NAME)) {
            expr->setOp(JSOP_DELNAME);
            return newUnary(PNK_DELETENAME, begin, expr);
        }

        if (expr->isKind(PNK_DOT))
            return newUnary(PNK_DELETEPROP, begin, expr);

        if (expr->isKind(PNK_ELEM))
            return newUnary(PNK_DELETEELEM, begin, expr);

        return newUnary(PNK_DELETEEXPR, begin, expr);
    }

    ParseNode* newTypeof(uint32_t begin, ParseNode* kid) {
        return newUnary(kid->isKind(PNK_NAME) ? PNK_TYPEOFNAME : PNK_TYPEOFEXPR, begin, kid);
    }

    ParseNode* newUnary(ParseNodeKind kind, uint32_t begin, ParseNode* kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(kind, pos, kid);
    }

    ParseNode* newUpdate(ParseNodeKind kind, uint32_t begin, ParseNode* kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(kind, pos, kid);
    }

    ParseNode* newSpread(uint32_t begin, ParseNode* kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(PNK_SPREAD, pos, kid);
    }

  private:
    ParseNode* newBinary(ParseNodeKind kind, ParseNode* left, ParseNode* right,
                         JSOp op = JSOP_NOP)
    {
        TokenPos pos(left->pn_pos.begin, right->pn_pos.end);
        return new_<BinaryNode>(kind, op, pos, left, right);
    }

  public:
    ParseNode* appendOrCreateList(ParseNodeKind kind, ParseNode* left, ParseNode* right,
                                  ParseContext* pc)
    {
        return ParseNode::appendOrCreateList(kind, left, right, this, pc);
    }

    // Expressions

    ParseNode* newArrayLiteral(uint32_t begin) {
        return new_<ListNode>(PNK_ARRAY, TokenPos(begin, begin + 1));
    }

    MOZ_MUST_USE bool addElision(ParseNode* literal, const TokenPos& pos) {
        MOZ_ASSERT(literal->isKind(PNK_ARRAY));
        MOZ_ASSERT(literal->isArity(PN_LIST));

        ParseNode* elision = new_<NullaryNode>(PNK_ELISION, pos);
        if (!elision)
            return false;

        literal->append(elision);
        literal->pn_xflags |= PNX_ARRAYHOLESPREAD | PNX_NONCONST;
        return true;
    }

    MOZ_MUST_USE bool addSpreadElement(ParseNode* literal, uint32_t begin, ParseNode* inner) {
        MOZ_ASSERT(literal->isKind(PNK_ARRAY));
        MOZ_ASSERT(literal->isArity(PN_LIST));

        ParseNode* spread = newSpread(begin, inner);
        if (!spread)
            return false;

        literal->append(spread);
        literal->pn_xflags |= PNX_ARRAYHOLESPREAD | PNX_NONCONST;
        return true;
    }

    void addArrayElement(ParseNode* literal, ParseNode* element) {
        MOZ_ASSERT(literal->isArity(PN_LIST));

        if (!element->isConstant())
            literal->pn_xflags |= PNX_NONCONST;

        literal->append(element);
    }

    ParseNode* newCall(const TokenPos& pos) {
        return new_<ListNode>(PNK_CALL, JSOP_CALL, pos);
    }

    ParseNode* newSuperCall(ParseNode* callee) {
        return new_<ListNode>(PNK_SUPERCALL, JSOP_SUPERCALL, callee);
    }

    ParseNode* newTaggedTemplate(const TokenPos& pos) {
        return new_<ListNode>(PNK_TAGGED_TEMPLATE, JSOP_CALL, pos);
    }

    ParseNode* newObjectLiteral(uint32_t begin) {
        return new_<ListNode>(PNK_OBJECT, TokenPos(begin, begin + 1));
    }

    ParseNode* newClass(ParseNode* name, ParseNode* heritage, ParseNode* methodBlock,
                        const TokenPos& pos)
    {
        return new_<ClassNode>(name, heritage, methodBlock, pos);
    }
    ParseNode* newClassMethodList(uint32_t begin) {
        return new_<ListNode>(PNK_CLASSMETHODLIST, TokenPos(begin, begin + 1));
    }
    ParseNode* newClassNames(ParseNode* outer, ParseNode* inner, const TokenPos& pos) {
        return new_<ClassNames>(outer, inner, pos);
    }
    ParseNode* newNewTarget(ParseNode* newHolder, ParseNode* targetHolder) {
        return new_<BinaryNode>(PNK_NEWTARGET, JSOP_NOP, newHolder, targetHolder);
    }
    ParseNode* newPosHolder(const TokenPos& pos) {
        return new_<NullaryNode>(PNK_POSHOLDER, pos);
    }
    ParseNode* newSuperBase(ParseNode* thisName, const TokenPos& pos) {
        return new_<UnaryNode>(PNK_SUPERBASE, pos, thisName);
    }

    MOZ_MUST_USE bool addPrototypeMutation(ParseNode* literal, uint32_t begin, ParseNode* expr) {
        MOZ_ASSERT(literal->isKind(PNK_OBJECT));
        MOZ_ASSERT(literal->isArity(PN_LIST));

        // Object literals with mutated [[Prototype]] are non-constant so that
        // singleton objects will have Object.prototype as their [[Prototype]].
        setListFlag(literal, PNX_NONCONST);

        ParseNode* mutation = newUnary(PNK_MUTATEPROTO, begin, expr);
        if (!mutation)
            return false;
        literal->append(mutation);
        return true;
    }

    MOZ_MUST_USE bool addPropertyDefinition(ParseNode* literal, ParseNode* key, ParseNode* val) {
        MOZ_ASSERT(literal->isKind(PNK_OBJECT));
        MOZ_ASSERT(literal->isArity(PN_LIST));
        MOZ_ASSERT(key->isKind(PNK_NUMBER) ||
                   key->isKind(PNK_OBJECT_PROPERTY_NAME) ||
                   key->isKind(PNK_STRING) ||
                   key->isKind(PNK_COMPUTED_NAME));

        ParseNode* propdef = newBinary(PNK_COLON, key, val, JSOP_INITPROP);
        if (!propdef)
            return false;
        literal->append(propdef);
        return true;
    }

    MOZ_MUST_USE bool addShorthand(ParseNode* literal, ParseNode* name, ParseNode* expr) {
        MOZ_ASSERT(literal->isKind(PNK_OBJECT));
        MOZ_ASSERT(literal->isArity(PN_LIST));
        MOZ_ASSERT(name->isKind(PNK_OBJECT_PROPERTY_NAME));
        MOZ_ASSERT(expr->isKind(PNK_NAME));
        MOZ_ASSERT(name->pn_atom == expr->pn_atom);

        setListFlag(literal, PNX_NONCONST);
        ParseNode* propdef = newBinary(PNK_SHORTHAND, name, expr, JSOP_INITPROP);
        if (!propdef)
            return false;
        literal->append(propdef);
        return true;
    }

    MOZ_MUST_USE bool addSpreadProperty(ParseNode* literal, uint32_t begin, ParseNode* inner) {
        MOZ_ASSERT(literal->isKind(PNK_OBJECT));
        MOZ_ASSERT(literal->isArity(PN_LIST));

        setListFlag(literal, PNX_NONCONST);
        ParseNode* spread = newSpread(begin, inner);
        if (!spread)
            return false;
        literal->append(spread);
        return true;
    }

    MOZ_MUST_USE bool addObjectMethodDefinition(ParseNode* literal, ParseNode* key, ParseNode* fn,
                                                AccessorType atype)
    {
        MOZ_ASSERT(literal->isKind(PNK_OBJECT));
        MOZ_ASSERT(literal->isArity(PN_LIST));
        MOZ_ASSERT(key->isKind(PNK_NUMBER) ||
                   key->isKind(PNK_OBJECT_PROPERTY_NAME) ||
                   key->isKind(PNK_STRING) ||
                   key->isKind(PNK_COMPUTED_NAME));

        ParseNode* propdef = newBinary(PNK_COLON, key, fn, AccessorTypeToJSOp(atype));
        if (!propdef)
            return false;

        literal->append(propdef);
        literal->pn_xflags |= PNX_NONCONST;
        return true;
    }

    MOZ_MUST_USE bool addClassMethodDefinition(ParseNode* methodList, ParseNode* key, ParseNode* fn,
                                               AccessorType atype, bool isStatic)
    {
        MOZ_ASSERT(methodList->isKind(PNK_CLASSMETHODLIST));
        MOZ_ASSERT(key->isKind(PNK_NUMBER) ||
                   key->isKind(PNK_OBJECT_PROPERTY_NAME) ||
                   key->isKind(PNK_STRING) ||
                   key->isKind(PNK_COMPUTED_NAME));

        ParseNode* classMethod = new_<ClassMethod>(key, fn, AccessorTypeToJSOp(atype), isStatic);
        if (!classMethod)
            return false;
        methodList->append(classMethod);
        return true;
    }

    ParseNode* newInitialYieldExpression(uint32_t begin, ParseNode* gen) {
        TokenPos pos(begin, begin + 1);
        return new_<UnaryNode>(PNK_INITIALYIELD, pos, gen);
    }

    ParseNode* newYieldExpression(uint32_t begin, ParseNode* value) {
        TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
        return new_<UnaryNode>(PNK_YIELD, pos, value);
    }

    ParseNode* newYieldStarExpression(uint32_t begin, ParseNode* value) {
        TokenPos pos(begin, value->pn_pos.end);
        return new_<UnaryNode>(PNK_YIELD_STAR, pos, value);
    }

    ParseNode* newAwaitExpression(uint32_t begin, ParseNode* value) {
        TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
        return new_<UnaryNode>(PNK_AWAIT, pos, value);
    }

    // Statements

    ParseNode* newStatementList(const TokenPos& pos) {
        return new_<ListNode>(PNK_STATEMENTLIST, pos);
    }

    MOZ_MUST_USE bool isFunctionStmt(ParseNode* stmt) {
        while (stmt->isKind(PNK_LABEL))
            stmt = stmt->as<LabeledStatement>().statement();
        return stmt->isKind(PNK_FUNCTION);
    }

    void addStatementToList(ParseNode* list, ParseNode* stmt) {
        MOZ_ASSERT(list->isKind(PNK_STATEMENTLIST));

        list->append(stmt);

        if (isFunctionStmt(stmt)) {
            // PNX_FUNCDEFS notifies the emitter that the block contains
            // body-level function definitions that should be processed
            // before the rest of nodes.
            list->pn_xflags |= PNX_FUNCDEFS;
        }
    }

    void setListEndPosition(ParseNode* list, const TokenPos& pos) {
        MOZ_ASSERT(list->isKind(PNK_STATEMENTLIST));
        list->pn_pos.end = pos.end;
    }

    void addCaseStatementToList(ParseNode* list, ParseNode* casepn) {
        MOZ_ASSERT(list->isKind(PNK_STATEMENTLIST));
        MOZ_ASSERT(casepn->isKind(PNK_CASE));
        MOZ_ASSERT(casepn->pn_right->isKind(PNK_STATEMENTLIST));

        list->append(casepn);

        if (casepn->pn_right->pn_xflags & PNX_FUNCDEFS)
            list->pn_xflags |= PNX_FUNCDEFS;
    }

    MOZ_MUST_USE bool prependInitialYield(ParseNode* stmtList, ParseNode* genName) {
        MOZ_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));
        MOZ_ASSERT(stmtList->isArity(PN_LIST));

        TokenPos yieldPos(stmtList->pn_pos.begin, stmtList->pn_pos.begin + 1);
        ParseNode* makeGen = new_<NullaryNode>(PNK_GENERATOR, yieldPos);
        if (!makeGen)
            return false;

        MOZ_ASSERT(genName->getOp() == JSOP_GETNAME);
        genName->setOp(JSOP_SETNAME);

        ParseNode* genInit = newBinary(PNK_ASSIGN, genName, makeGen);
        if (!genInit)
            return false;

        ParseNode* initialYield = newInitialYieldExpression(yieldPos.begin, genInit);
        if (!initialYield)
            return false;

        stmtList->prepend(initialYield);
        return true;
    }

    ParseNode* newSetThis(ParseNode* thisName, ParseNode* val) {
        MOZ_ASSERT(thisName->getOp() == JSOP_GETNAME);
        thisName->setOp(JSOP_SETNAME);
        return newBinary(PNK_SETTHIS, thisName, val);
    }

    ParseNode* newEmptyStatement(const TokenPos& pos) {
        return new_<UnaryNode>(PNK_SEMI, pos, nullptr);
    }

    ParseNode* newImportDeclaration(ParseNode* importSpecSet,
                                    ParseNode* moduleSpec, const TokenPos& pos)
    {
        ParseNode* pn = new_<BinaryNode>(PNK_IMPORT, JSOP_NOP, pos,
                                         importSpecSet, moduleSpec);
        if (!pn)
            return null();
        return pn;
    }

    ParseNode* newImportSpec(ParseNode* importNameNode, ParseNode* bindingName) {
        return newBinary(PNK_IMPORT_SPEC, importNameNode, bindingName);
    }

    ParseNode* newExportDeclaration(ParseNode* kid, const TokenPos& pos) {
        return new_<UnaryNode>(PNK_EXPORT, pos, kid);
    }

    ParseNode* newExportFromDeclaration(uint32_t begin, ParseNode* exportSpecSet,
                                        ParseNode* moduleSpec)
    {
        ParseNode* pn = new_<BinaryNode>(PNK_EXPORT_FROM, JSOP_NOP, exportSpecSet, moduleSpec);
        if (!pn)
            return null();
        pn->pn_pos.begin = begin;
        return pn;
    }

    ParseNode* newExportDefaultDeclaration(ParseNode* kid, ParseNode* maybeBinding,
                                           const TokenPos& pos) {
        return new_<BinaryNode>(PNK_EXPORT_DEFAULT, JSOP_NOP, pos, kid, maybeBinding);
    }

    ParseNode* newExportSpec(ParseNode* bindingName, ParseNode* exportName) {
        return newBinary(PNK_EXPORT_SPEC, bindingName, exportName);
    }

    ParseNode* newExportBatchSpec(const TokenPos& pos) {
        return new_<NullaryNode>(PNK_EXPORT_BATCH_SPEC, JSOP_NOP, pos);
    }

    ParseNode* newExprStatement(ParseNode* expr, uint32_t end) {
        MOZ_ASSERT(expr->pn_pos.end <= end);
        return new_<UnaryNode>(PNK_SEMI, TokenPos(expr->pn_pos.begin, end), expr);
    }

    ParseNode* newIfStatement(uint32_t begin, ParseNode* cond, ParseNode* thenBranch,
                              ParseNode* elseBranch)
    {
        ParseNode* pn = new_<TernaryNode>(PNK_IF, cond, thenBranch, elseBranch);
        if (!pn)
            return null();
        pn->pn_pos.begin = begin;
        return pn;
    }

    ParseNode* newDoWhileStatement(ParseNode* body, ParseNode* cond, const TokenPos& pos) {
        return new_<BinaryNode>(PNK_DOWHILE, JSOP_NOP, pos, body, cond);
    }

    ParseNode* newWhileStatement(uint32_t begin, ParseNode* cond, ParseNode* body) {
        TokenPos pos(begin, body->pn_pos.end);
        return new_<BinaryNode>(PNK_WHILE, JSOP_NOP, pos, cond, body);
    }

    ParseNode* newForStatement(uint32_t begin, ParseNode* forHead, ParseNode* body,
                               unsigned iflags)
    {
        /* A FOR node is binary, left is loop control and right is the body. */
        JSOp op = forHead->isKind(PNK_FORIN) ? JSOP_ITER : JSOP_NOP;
        BinaryNode* pn = new_<BinaryNode>(PNK_FOR, op, TokenPos(begin, body->pn_pos.end),
                                          forHead, body);
        if (!pn)
            return null();
        pn->pn_iflags = iflags;
        return pn;
    }

    ParseNode* newForHead(ParseNode* init, ParseNode* test, ParseNode* update,
                          const TokenPos& pos)
    {
        return new_<TernaryNode>(PNK_FORHEAD, init, test, update, pos);
    }

    ParseNode* newForInOrOfHead(ParseNodeKind kind, ParseNode* target, ParseNode* iteratedExpr,
                                const TokenPos& pos)
    {
        MOZ_ASSERT(kind == PNK_FORIN || kind == PNK_FOROF);
        return new_<TernaryNode>(kind, target, nullptr, iteratedExpr, pos);
    }

    ParseNode* newSwitchStatement(uint32_t begin, ParseNode* discriminant, ParseNode* caseList) {
        TokenPos pos(begin, caseList->pn_pos.end);
        return new_<BinaryNode>(PNK_SWITCH, JSOP_NOP, pos, discriminant, caseList);
    }

    ParseNode* newCaseOrDefault(uint32_t begin, ParseNode* expr, ParseNode* body) {
        return new_<CaseClause>(expr, body, begin);
    }

    ParseNode* newContinueStatement(PropertyName* label, const TokenPos& pos) {
        return new_<ContinueStatement>(label, pos);
    }

    ParseNode* newBreakStatement(PropertyName* label, const TokenPos& pos) {
        return new_<BreakStatement>(label, pos);
    }

    ParseNode* newReturnStatement(ParseNode* expr, const TokenPos& pos) {
        MOZ_ASSERT_IF(expr, pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(PNK_RETURN, pos, expr);
    }

    ParseNode* newExpressionBody(ParseNode* expr) {
        return new_<UnaryNode>(PNK_RETURN, expr->pn_pos, expr);
    }

    ParseNode* newWithStatement(uint32_t begin, ParseNode* expr, ParseNode* body) {
        return new_<BinaryNode>(PNK_WITH, JSOP_NOP, TokenPos(begin, body->pn_pos.end),
                                expr, body);
    }

    ParseNode* newLabeledStatement(PropertyName* label, ParseNode* stmt, uint32_t begin) {
        return new_<LabeledStatement>(label, stmt, begin);
    }

    ParseNode* newThrowStatement(ParseNode* expr, const TokenPos& pos) {
        MOZ_ASSERT(pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(PNK_THROW, pos, expr);
    }

    ParseNode* newTryStatement(uint32_t begin, ParseNode* body, ParseNode* catchScope,
                               ParseNode* finallyBlock) {
        TokenPos pos(begin, (finallyBlock ? finallyBlock : catchScope)->pn_pos.end);
        return new_<TernaryNode>(PNK_TRY, body, catchScope, finallyBlock, pos);
    }

    ParseNode* newDebuggerStatement(const TokenPos& pos) {
        return new_<DebuggerStatement>(pos);
    }

    ParseNode* newPropertyAccess(ParseNode* pn, PropertyName* name, uint32_t end) {
        return new_<PropertyAccess>(pn, name, pn->pn_pos.begin, end);
    }

    ParseNode* newPropertyByValue(ParseNode* lhs, ParseNode* index, uint32_t end) {
        return new_<PropertyByValue>(lhs, index, lhs->pn_pos.begin, end);
    }

    bool setupCatchScope(ParseNode* lexicalScope, ParseNode* catchName, ParseNode* catchBody) {
        ParseNode* catchpn;
        if (catchName) {
            catchpn = new_<BinaryNode>(PNK_CATCH, JSOP_NOP, catchName, catchBody);
        } else {
            catchpn = new_<BinaryNode>(PNK_CATCH, JSOP_NOP, catchBody->pn_pos, catchName,
                                       catchBody);
        }
        if (!catchpn)
            return false;
        lexicalScope->setScopeBody(catchpn);
        return true;
    }

    inline MOZ_MUST_USE bool setLastFunctionFormalParameterDefault(ParseNode* funcpn,
                                                                   ParseNode* pn);

    void checkAndSetIsDirectRHSAnonFunction(ParseNode* pn) {
        if (IsAnonymousFunctionDefinition(pn))
            pn->setDirectRHSAnonFunction(true);
    }

    ParseNode* newFunctionStatement(const TokenPos& pos) {
        return new_<CodeNode>(PNK_FUNCTION, JSOP_NOP, pos);
    }

    ParseNode* newFunctionExpression(const TokenPos& pos) {
        return new_<CodeNode>(PNK_FUNCTION, JSOP_LAMBDA, pos);
    }

    ParseNode* newArrowFunction(const TokenPos& pos) {
        return new_<CodeNode>(PNK_FUNCTION, JSOP_LAMBDA_ARROW, pos);
    }

    bool isExpressionClosure(ParseNode* node) const {
        return node->isKind(PNK_FUNCTION) &&
               node->pn_funbox->isExprBody() &&
               !node->pn_funbox->isArrow();
    }

    void setFunctionFormalParametersAndBody(ParseNode* funcNode, ParseNode* kid) {
        MOZ_ASSERT_IF(kid, kid->isKind(PNK_PARAMSBODY));
        funcNode->pn_body = kid;
    }
    void setFunctionBox(ParseNode* pn, FunctionBox* funbox) {
        MOZ_ASSERT(pn->isKind(PNK_FUNCTION));
        pn->pn_funbox = funbox;
        funbox->functionNode = pn;
    }
    void addFunctionFormalParameter(ParseNode* pn, ParseNode* argpn) {
        pn->pn_body->append(argpn);
    }
    void setFunctionBody(ParseNode* fn, ParseNode* body) {
        MOZ_ASSERT(fn->pn_body->isKind(PNK_PARAMSBODY));
        fn->pn_body->append(body);
    }

    ParseNode* newModule(const TokenPos& pos) {
        return new_<CodeNode>(PNK_MODULE, JSOP_NOP, pos);
    }

    ParseNode* newLexicalScope(LexicalScope::Data* bindings, ParseNode* body) {
        return new_<LexicalScopeNode>(bindings, body);
    }

    Node newNewExpression(uint32_t begin, ParseNode* ctor) {
        ParseNode* newExpr = new_<ListNode>(PNK_NEW, JSOP_NEW, TokenPos(begin, begin + 1));
        if (!newExpr)
            return nullptr;

        addList(newExpr, ctor);
        return newExpr;
    }

    ParseNode* newAssignment(ParseNodeKind kind, ParseNode* lhs, ParseNode* rhs) {
        return newBinary(kind, lhs, rhs);
    }

    bool isUnparenthesizedAssignment(Node node) {
        if (node->isKind(PNK_ASSIGN) && !node->isInParens()) {
            // PNK_ASSIGN is also (mis)used for things like |var name = expr;|.
            // But this method is only called on actual expressions, so we can
            // just assert the node's op is the one used for plain assignment.
            MOZ_ASSERT(node->isOp(JSOP_NOP));
            return true;
        }

        return false;
    }

    bool isUnparenthesizedUnaryExpression(ParseNode* node) {
        if (!node->isInParens()) {
            ParseNodeKind kind = node->getKind();
            return kind == PNK_VOID || kind == PNK_NOT || kind == PNK_BITNOT || kind == PNK_POS ||
                   kind == PNK_NEG || IsTypeofKind(kind) || IsDeleteKind(kind);
        }
        return false;
    }

    bool isReturnStatement(ParseNode* node) {
        return node->isKind(PNK_RETURN);
    }

    bool isStatementPermittedAfterReturnStatement(ParseNode *node) {
        ParseNodeKind kind = node->getKind();
        return kind == PNK_FUNCTION || kind == PNK_VAR || kind == PNK_BREAK || kind == PNK_THROW ||
               (kind == PNK_SEMI && !node->pn_kid);
    }

    bool isSuperBase(ParseNode* node) {
        return node->isKind(PNK_SUPERBASE);
    }

    inline MOZ_MUST_USE bool finishInitializerAssignment(ParseNode* pn, ParseNode* init);

    void setBeginPosition(ParseNode* pn, ParseNode* oth) {
        setBeginPosition(pn, oth->pn_pos.begin);
    }
    void setBeginPosition(ParseNode* pn, uint32_t begin) {
        pn->pn_pos.begin = begin;
        MOZ_ASSERT(pn->pn_pos.begin <= pn->pn_pos.end);
    }

    void setEndPosition(ParseNode* pn, ParseNode* oth) {
        setEndPosition(pn, oth->pn_pos.end);
    }
    void setEndPosition(ParseNode* pn, uint32_t end) {
        pn->pn_pos.end = end;
        MOZ_ASSERT(pn->pn_pos.begin <= pn->pn_pos.end);
    }

    uint32_t getFunctionNameOffset(ParseNode* func, TokenStreamAnyChars& ts) {
        return func->pn_pos.begin;
    }

    bool isDeclarationKind(ParseNodeKind kind) {
        return kind == PNK_VAR || kind == PNK_LET || kind == PNK_CONST;
    }

    ParseNode* newList(ParseNodeKind kind, const TokenPos& pos) {
        MOZ_ASSERT(!isDeclarationKind(kind));
        return new_<ListNode>(kind, JSOP_NOP, pos);
    }

  private:
    template<typename T>
    ParseNode* newList(ParseNodeKind kind, const T& begin) = delete;

  public:
    ParseNode* newList(ParseNodeKind kind, ParseNode* kid) {
        MOZ_ASSERT(!isDeclarationKind(kind));
        return new_<ListNode>(kind, JSOP_NOP, kid);
    }

    ParseNode* newDeclarationList(ParseNodeKind kind, const TokenPos& pos) {
        MOZ_ASSERT(isDeclarationKind(kind));
        return new_<ListNode>(kind, JSOP_NOP, pos);
    }

    bool isDeclarationList(ParseNode* node) {
        return isDeclarationKind(node->getKind());
    }

    ParseNode* singleBindingFromDeclaration(ParseNode* decl) {
        MOZ_ASSERT(isDeclarationList(decl));
        MOZ_ASSERT(decl->pn_count == 1);
        return decl->pn_head;
    }

    ParseNode* newCommaExpressionList(ParseNode* kid) {
        return new_<ListNode>(PNK_COMMA, JSOP_NOP, kid);
    }

    void addList(ParseNode* list, ParseNode* kid) {
        list->append(kid);
    }

    void setOp(ParseNode* pn, JSOp op) {
        pn->setOp(op);
    }
    void setListFlag(ParseNode* pn, unsigned flag) {
        MOZ_ASSERT(pn->isArity(PN_LIST));
        pn->pn_xflags |= flag;
    }
    MOZ_MUST_USE ParseNode* parenthesize(ParseNode* pn) {
        pn->setInParens(true);
        return pn;
    }
    MOZ_MUST_USE ParseNode* setLikelyIIFE(ParseNode* pn) {
        return parenthesize(pn);
    }
    void setInDirectivePrologue(ParseNode* pn) {
        pn->pn_prologue = true;
    }

    bool isConstant(ParseNode* pn) {
        return pn->isConstant();
    }

    bool isName(ParseNode* node) {
        return node->isKind(PNK_NAME);
    }

    bool isArgumentsName(ParseNode* node, JSContext* cx) {
        return node->isKind(PNK_NAME) && node->pn_atom == cx->names().arguments;
    }

    bool isEvalName(ParseNode* node, JSContext* cx) {
        return node->isKind(PNK_NAME) && node->pn_atom == cx->names().eval;
    }

    bool isAsyncKeyword(ParseNode* node, JSContext* cx) {
        return node->isKind(PNK_NAME) &&
               node->pn_pos.begin + strlen("async") == node->pn_pos.end &&
               node->pn_atom == cx->names().async;
    }

    PropertyName* maybeDottedProperty(ParseNode* pn) {
        return pn->is<PropertyAccess>() ? &pn->as<PropertyAccess>().name() : nullptr;
    }
    JSAtom* isStringExprStatement(ParseNode* pn, TokenPos* pos) {
        if (JSAtom* atom = pn->isStringExprStatement()) {
            *pos = pn->pn_kid->pn_pos;
            return atom;
        }
        return nullptr;
    }

    void adjustGetToSet(ParseNode* node) {
        node->setOp(node->isOp(JSOP_GETLOCAL) ? JSOP_SETLOCAL : JSOP_SETNAME);
    }

    bool canSkipLazyInnerFunctions() {
        return !!lazyOuterFunction_;
    }
    bool canSkipLazyClosedOverBindings() {
        return !!lazyOuterFunction_;
    }
    JSFunction* nextLazyInnerFunction() {
        MOZ_ASSERT(lazyInnerFunctionIndex < lazyOuterFunction_->numInnerFunctions());
        return lazyOuterFunction_->innerFunctions()[lazyInnerFunctionIndex++];
    }
    JSAtom* nextLazyClosedOverBinding() {
        MOZ_ASSERT(lazyClosedOverBindingIndex < lazyOuterFunction_->numClosedOverBindings());
        return lazyOuterFunction_->closedOverBindings()[lazyClosedOverBindingIndex++];
    }
};

inline bool
FullParseHandler::setLastFunctionFormalParameterDefault(ParseNode* funcpn,
                                                        ParseNode* defaultValue)
{
    MOZ_ASSERT(funcpn->isKind(PNK_FUNCTION));
    MOZ_ASSERT(funcpn->isArity(PN_CODE));

    ParseNode* arg = funcpn->pn_body->last();
    ParseNode* pn = newBinary(PNK_ASSIGN, arg, defaultValue);
    if (!pn)
        return false;

    checkAndSetIsDirectRHSAnonFunction(defaultValue);

    funcpn->pn_body->pn_pos.end = pn->pn_pos.end;
    ParseNode* pnchild = funcpn->pn_body->pn_head;
    ParseNode* pnlast = funcpn->pn_body->last();
    MOZ_ASSERT(pnchild);
    if (pnchild == pnlast) {
        funcpn->pn_body->pn_head = pn;
    } else {
        while (pnchild->pn_next != pnlast) {
            MOZ_ASSERT(pnchild->pn_next);
            pnchild = pnchild->pn_next;
        }
        pnchild->pn_next = pn;
    }
    funcpn->pn_body->pn_tail = &pn->pn_next;

    return true;
}

inline bool
FullParseHandler::finishInitializerAssignment(ParseNode* pn, ParseNode* init)
{
    pn->pn_expr = init;
    pn->setOp(JSOP_SETNAME);

    /* The declarator's position must include the initializer. */
    pn->pn_pos.end = init->pn_pos.end;
    return true;
}

} // namespace frontend
} // namespace js

#endif /* frontend_FullParseHandler_h */

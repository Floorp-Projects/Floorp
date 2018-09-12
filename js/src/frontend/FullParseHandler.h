/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FullParseHandler_h
#define frontend_FullParseHandler_h

#include "mozilla/Attributes.h"
#include "mozilla/PodOperations.h"

#include <cstddef> // std::nullptr_t
#include <string.h>

#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"
#include "vm/JSContext.h"

namespace js {

class RegExpObject;

namespace frontend {

enum class SourceKind {
    // We are parsing from a text source (Parser.h)
    Text,
    // We are parsing from a binary source (BinSource.h)
    Binary,
};

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

    const SourceKind sourceKind_;

  public:
    /* new_ methods for creating parse nodes. These report OOM on context. */
    JS_DECLARE_NEW_METHODS(new_, allocParseNode, inline)

    // FIXME: Use ListNode instead of ListNodeType as an alias (bug 1489008).
    using Node = ParseNode*;

#define DECLARE_TYPE(typeName, longTypeName, asMethodName) \
    using longTypeName = typeName*;
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_TYPE)
#undef DECLARE_TYPE

    using NullNode = std::nullptr_t;

    bool isPropertyAccess(ParseNode* node) {
        return node->isKind(ParseNodeKind::Dot) || node->isKind(ParseNodeKind::Elem);
    }

    bool isFunctionCall(ParseNode* node) {
        // Note: super() is a special form, *not* a function call.
        return node->isKind(ParseNodeKind::Call);
    }

    static bool isUnparenthesizedDestructuringPattern(ParseNode* node) {
        return !node->isInParens() && (node->isKind(ParseNodeKind::Object) ||
                                       node->isKind(ParseNodeKind::Array));
    }

    static bool isParenthesizedDestructuringPattern(ParseNode* node) {
        // Technically this isn't a destructuring pattern at all -- the grammar
        // doesn't treat it as such.  But we need to know when this happens to
        // consider it a SyntaxError rather than an invalid-left-hand-side
        // ReferenceError.
        return node->isInParens() && (node->isKind(ParseNodeKind::Object) ||
                                      node->isKind(ParseNodeKind::Array));
    }

    FullParseHandler(JSContext* cx, LifoAlloc& alloc, LazyScript* lazyOuterFunction,
                     SourceKind kind = SourceKind::Text)
      : allocator(cx, alloc),
        lazyOuterFunction_(cx, lazyOuterFunction),
        lazyInnerFunctionIndex(0),
        lazyClosedOverBindingIndex(0),
        sourceKind_(SourceKind::Text)
    {}

    static NullNode null() { return NullNode(); }

#define DECLARE_AS(typeName, longTypeName, asMethodName) \
    static longTypeName asMethodName(Node node) { \
        return &node->as<typeName>(); \
    }
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_AS)
#undef DECLARE_AS

    // The FullParseHandler may be used to create nodes for text sources
    // (from Parser.h) or for binary sources (from BinSource.h). In the latter
    // case, some common assumptions on offsets are incorrect, e.g. in `a + b`,
    // `a`, `b` and `+` may be stored in any order. We use `sourceKind()`
    // to determine whether we need to check these assumptions.
    SourceKind sourceKind() const { return sourceKind_; }

    NameNodeType newName(PropertyName* name, const TokenPos& pos, JSContext* cx) {
        return new_<NameNode>(ParseNodeKind::Name, JSOP_GETNAME, name, pos);
    }

    UnaryNodeType newComputedName(Node expr, uint32_t begin, uint32_t end) {
        TokenPos pos(begin, end);
        return new_<UnaryNode>(ParseNodeKind::ComputedName, pos, expr);
    }

    NameNodeType newObjectLiteralPropertyName(JSAtom* atom, const TokenPos& pos) {
        return new_<NameNode>(ParseNodeKind::ObjectPropertyName, JSOP_NOP, atom, pos);
    }

    NumericLiteralType newNumber(double value, DecimalPoint decimalPoint, const TokenPos& pos) {
        return new_<NumericLiteral>(value, decimalPoint, pos);
    }

    BooleanLiteralType newBooleanLiteral(bool cond, const TokenPos& pos) {
        return new_<BooleanLiteral>(cond, pos);
    }

    NameNodeType newStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return new_<NameNode>(ParseNodeKind::String, JSOP_NOP, atom, pos);
    }

    NameNodeType newTemplateStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return new_<NameNode>(ParseNodeKind::TemplateString, JSOP_NOP, atom, pos);
    }

    CallSiteNodeType newCallSiteObject(uint32_t begin) {
        CallSiteNode* callSiteObj = new_<CallSiteNode>(begin);
        if (!callSiteObj) {
            return null();
        }

        ListNode* rawNodes = newArrayLiteral(callSiteObj->pn_pos.begin);
        if (!rawNodes) {
            return null();
        }

        addArrayElement(callSiteObj, rawNodes);

        return callSiteObj;
    }

    void addToCallSiteObject(CallSiteNodeType callSiteObj, ParseNode* rawNode,
                             ParseNode* cookedNode) {
        MOZ_ASSERT(callSiteObj->isKind(ParseNodeKind::CallSiteObj));

        addArrayElement(callSiteObj, cookedNode);
        addArrayElement(callSiteObj->rawNodes(), rawNode);

        /*
         * We don't know when the last noSubstTemplate will come in, and we
         * don't want to deal with this outside this method
         */
        setEndPosition(callSiteObj, callSiteObj->rawNodes());
    }

    ThisLiteralType newThisLiteral(const TokenPos& pos, Node thisName) {
        return new_<ThisLiteral>(pos, thisName);
    }

    NullLiteralType newNullLiteral(const TokenPos& pos) {
        return new_<NullLiteral>(pos);
    }

    RawUndefinedLiteralType newRawUndefinedLiteral(const TokenPos& pos) {
        return new_<RawUndefinedLiteral>(pos);
    }

    // The Boxer object here is any object that can allocate ObjectBoxes.
    // Specifically, a Boxer has a .newObjectBox(T) method that accepts a
    // Rooted<RegExpObject*> argument and returns an ObjectBox*.
    template <class Boxer>
    RegExpLiteralType newRegExp(RegExpObject* reobj, const TokenPos& pos, Boxer& boxer) {
        ObjectBox* objbox = boxer.newObjectBox(reobj);
        if (!objbox) {
            return null();
        }
        return new_<RegExpLiteral>(objbox, pos);
    }

    ConditionalExpressionType newConditional(Node cond, Node thenExpr, Node elseExpr) {
        return new_<ConditionalExpression>(cond, thenExpr, elseExpr);
    }

    UnaryNodeType newDelete(uint32_t begin, Node expr) {
        if (expr->isKind(ParseNodeKind::Name)) {
            expr->setOp(JSOP_DELNAME);
            return newUnary(ParseNodeKind::DeleteName, begin, expr);
        }

        if (expr->isKind(ParseNodeKind::Dot)) {
            return newUnary(ParseNodeKind::DeleteProp, begin, expr);
        }

        if (expr->isKind(ParseNodeKind::Elem)) {
            return newUnary(ParseNodeKind::DeleteElem, begin, expr);
        }

        return newUnary(ParseNodeKind::DeleteExpr, begin, expr);
    }

    UnaryNodeType newTypeof(uint32_t begin, Node kid) {
        ParseNodeKind pnk = kid->isKind(ParseNodeKind::Name)
                            ? ParseNodeKind::TypeOfName
                            : ParseNodeKind::TypeOfExpr;
        return newUnary(pnk, begin, kid);
    }

    UnaryNodeType newUnary(ParseNodeKind kind, uint32_t begin, Node kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(kind, pos, kid);
    }

    UnaryNodeType newUpdate(ParseNodeKind kind, uint32_t begin, Node kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(kind, pos, kid);
    }

    UnaryNodeType newSpread(uint32_t begin, Node kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(ParseNodeKind::Spread, pos, kid);
    }

  private:
    BinaryNodeType newBinary(ParseNodeKind kind, Node left, Node right, JSOp op = JSOP_NOP) {
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

    ListNodeType newArrayLiteral(uint32_t begin) {
        return new_<ListNode>(ParseNodeKind::Array, TokenPos(begin, begin + 1));
    }

    MOZ_MUST_USE bool addElision(ListNodeType literal, const TokenPos& pos) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Array));

        NullaryNode* elision = new_<NullaryNode>(ParseNodeKind::Elision, pos);
        if (!elision) {
            return false;
        }
        addList(/* list = */ literal, /* child = */ elision);
        literal->setHasArrayHoleOrSpread();
        literal->setHasNonConstInitializer();
        return true;
    }

    MOZ_MUST_USE bool addSpreadElement(ListNodeType literal, uint32_t begin, Node inner) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Array));

        ParseNode* spread = newSpread(begin, inner);
        if (!spread) {
            return false;
        }
        addList(/* list = */ literal, /* child = */ spread);
        literal->setHasArrayHoleOrSpread();
        literal->setHasNonConstInitializer();
        return true;
    }

    void addArrayElement(ListNodeType literal, Node element) {
        if (!element->isConstant()) {
            literal->setHasNonConstInitializer();
        }
        addList(/* list = */ literal, /* child = */ element);
    }

    BinaryNodeType newCall(Node callee, Node args) {
        return new_<BinaryNode>(ParseNodeKind::Call, JSOP_CALL, callee, args);
    }

    ListNodeType newArguments(const TokenPos& pos) {
        return new_<ListNode>(ParseNodeKind::Arguments, JSOP_NOP, pos);
    }

    BinaryNodeType newSuperCall(Node callee, Node args) {
        return new_<BinaryNode>(ParseNodeKind::SuperCall, JSOP_SUPERCALL, callee, args);
    }

    BinaryNodeType newTaggedTemplate(Node tag, Node args) {
        return new_<BinaryNode>(ParseNodeKind::TaggedTemplate, JSOP_CALL, tag, args);
    }

    ListNodeType newObjectLiteral(uint32_t begin) {
        return new_<ListNode>(ParseNodeKind::Object, TokenPos(begin, begin + 1));
    }

    ClassNodeType newClass(Node name, Node heritage, Node methodBlock, const TokenPos& pos) {
        return new_<ClassNode>(name, heritage, methodBlock, pos);
    }
    ListNodeType newClassMethodList(uint32_t begin) {
        return new_<ListNode>(ParseNodeKind::ClassMethodList, TokenPos(begin, begin + 1));
    }
    ClassNamesType newClassNames(Node outer, Node inner, const TokenPos& pos) {
        return new_<ClassNames>(outer, inner, pos);
    }
    BinaryNodeType newNewTarget(NullaryNodeType newHolder, NullaryNodeType targetHolder) {
        return new_<BinaryNode>(ParseNodeKind::NewTarget, JSOP_NOP, newHolder, targetHolder);
    }
    NullaryNodeType newPosHolder(const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::PosHolder, pos);
    }
    UnaryNodeType newSuperBase(Node thisName, const TokenPos& pos) {
        return new_<UnaryNode>(ParseNodeKind::SuperBase, pos, thisName);
    }
    MOZ_MUST_USE bool addPrototypeMutation(ListNodeType literal, uint32_t begin, Node expr) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Object));

        // Object literals with mutated [[Prototype]] are non-constant so that
        // singleton objects will have Object.prototype as their [[Prototype]].
        literal->setHasNonConstInitializer();

        UnaryNode* mutation = newUnary(ParseNodeKind::MutateProto, begin, expr);
        if (!mutation) {
            return false;
        }
        addList(/* list = */ literal, /* child = */ mutation);
        return true;
    }

    BinaryNodeType newPropertyDefinition(Node key, Node val) {
        MOZ_ASSERT(isUsableAsObjectPropertyName(key));
        checkAndSetIsDirectRHSAnonFunction(val);
        return newBinary(ParseNodeKind::Colon, key, val, JSOP_INITPROP);
    }

    void addPropertyDefinition(ListNodeType literal, BinaryNodeType propdef) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Object));
        MOZ_ASSERT(propdef->isKind(ParseNodeKind::Colon));

        if (!propdef->right()->isConstant()) {
            literal->setHasNonConstInitializer();
        }

        addList(/* list = */ literal, /* child = */ propdef);
    }

    MOZ_MUST_USE bool addPropertyDefinition(ListNodeType literal, Node key, Node val) {
        BinaryNode* propdef = newPropertyDefinition(key, val);
        if (!propdef) {
            return false;
        }
        addPropertyDefinition(literal, propdef);
        return true;
    }

    MOZ_MUST_USE bool addShorthand(ListNodeType literal, NameNodeType name, NameNodeType expr) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Object));
        MOZ_ASSERT(name->isKind(ParseNodeKind::ObjectPropertyName));
        MOZ_ASSERT(expr->isKind(ParseNodeKind::Name));
        MOZ_ASSERT(name->atom() == expr->atom());

        literal->setHasNonConstInitializer();
        BinaryNode* propdef = newBinary(ParseNodeKind::Shorthand, name, expr, JSOP_INITPROP);
        if (!propdef) {
            return false;
        }
        addList(/* list = */ literal, /* child = */ propdef);
        return true;
    }

    MOZ_MUST_USE bool addSpreadProperty(ListNodeType literal, uint32_t begin, Node inner) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Object));

        literal->setHasNonConstInitializer();
        ParseNode* spread = newSpread(begin, inner);
        if (!spread) {
            return false;
        }
        addList(/* list = */ literal, /* child = */ spread);
        return true;
    }

    MOZ_MUST_USE bool addObjectMethodDefinition(ListNodeType literal, Node key,
                                                CodeNodeType funNode, AccessorType atype)
    {
        literal->setHasNonConstInitializer();

        checkAndSetIsDirectRHSAnonFunction(funNode);

        ParseNode* propdef = newObjectMethodOrPropertyDefinition(key, funNode, atype);
        if (!propdef) {
            return false;
        }

        addList(/* list = */ literal, /* child = */ propdef);
        return true;
    }

    MOZ_MUST_USE bool addClassMethodDefinition(ListNodeType methodList, Node key,
                                               CodeNodeType funNode, AccessorType atype,
                                               bool isStatic)
    {
        MOZ_ASSERT(methodList->isKind(ParseNodeKind::ClassMethodList));
        MOZ_ASSERT(isUsableAsObjectPropertyName(key));

        checkAndSetIsDirectRHSAnonFunction(funNode);

        ClassMethod* classMethod = new_<ClassMethod>(key, funNode, AccessorTypeToJSOp(atype),
                                                     isStatic);
        if (!classMethod) {
            return false;
        }
        addList(/* list = */ methodList, /* child = */ classMethod);
        return true;
    }

    UnaryNodeType newInitialYieldExpression(uint32_t begin, Node gen) {
        TokenPos pos(begin, begin + 1);
        return new_<UnaryNode>(ParseNodeKind::InitialYield, pos, gen);
    }

    UnaryNodeType newYieldExpression(uint32_t begin, Node value) {
        TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
        return new_<UnaryNode>(ParseNodeKind::Yield, pos, value);
    }

    UnaryNodeType newYieldStarExpression(uint32_t begin, Node value) {
        TokenPos pos(begin, value->pn_pos.end);
        return new_<UnaryNode>(ParseNodeKind::YieldStar, pos, value);
    }

    UnaryNodeType newAwaitExpression(uint32_t begin, Node value) {
        TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
        return new_<UnaryNode>(ParseNodeKind::Await, pos, value);
    }

    // Statements

    ListNodeType newStatementList(const TokenPos& pos) {
        return new_<ListNode>(ParseNodeKind::StatementList, pos);
    }

    MOZ_MUST_USE bool isFunctionStmt(ParseNode* stmt) {
        while (stmt->isKind(ParseNodeKind::Label)) {
            stmt = stmt->as<LabeledStatement>().statement();
        }
        return stmt->isKind(ParseNodeKind::Function);
    }

    void addStatementToList(ListNodeType list, Node stmt) {
        MOZ_ASSERT(list->isKind(ParseNodeKind::StatementList));

        addList(/* list = */ list, /* child = */ stmt);

        if (isFunctionStmt(stmt)) {
            // Notify the emitter that the block contains body-level function
            // definitions that should be processed before the rest of nodes.
            list->setHasTopLevelFunctionDeclarations();
        }
    }

    void setListEndPosition(ListNodeType list, const TokenPos& pos) {
        MOZ_ASSERT(list->isKind(ParseNodeKind::StatementList));
        list->pn_pos.end = pos.end;
    }

    void addCaseStatementToList(ListNodeType list, CaseClauseType caseClause) {
        MOZ_ASSERT(list->isKind(ParseNodeKind::StatementList));

        addList(/* list = */ list, /* child = */ caseClause);

        if (caseClause->statementList()->hasTopLevelFunctionDeclarations()) {
            list->setHasTopLevelFunctionDeclarations();
        }
    }

    MOZ_MUST_USE bool prependInitialYield(ListNodeType stmtList, Node genName) {
        MOZ_ASSERT(stmtList->isKind(ParseNodeKind::StatementList));

        TokenPos yieldPos(stmtList->pn_pos.begin, stmtList->pn_pos.begin + 1);
        NullaryNode* makeGen = new_<NullaryNode>(ParseNodeKind::Generator, yieldPos);
        if (!makeGen) {
            return false;
        }

        MOZ_ASSERT(genName->getOp() == JSOP_GETNAME);
        genName->setOp(JSOP_SETNAME);
        ParseNode* genInit = newAssignment(ParseNodeKind::Assign, /* lhs = */ genName,
                                           /* rhs = */ makeGen);
        if (!genInit) {
            return false;
        }

        UnaryNode* initialYield = newInitialYieldExpression(yieldPos.begin, genInit);
        if (!initialYield) {
            return false;
        }

        stmtList->prepend(initialYield);
        return true;
    }

    BinaryNodeType newSetThis(Node thisName, Node value) {
        MOZ_ASSERT(thisName->getOp() == JSOP_GETNAME);
        thisName->setOp(JSOP_SETNAME);
        return newBinary(ParseNodeKind::SetThis, thisName, value);
    }

    NullaryNodeType newEmptyStatement(const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::EmptyStatement, pos);
    }

    BinaryNodeType newImportDeclaration(Node importSpecSet, Node moduleSpec, const TokenPos& pos) {
        return new_<BinaryNode>(ParseNodeKind::Import, JSOP_NOP, pos,
                                importSpecSet, moduleSpec);
    }

    BinaryNodeType newImportSpec(Node importNameNode, Node bindingName) {
        return newBinary(ParseNodeKind::ImportSpec, importNameNode, bindingName);
    }

    UnaryNodeType newExportDeclaration(Node kid, const TokenPos& pos) {
        return new_<UnaryNode>(ParseNodeKind::Export, pos, kid);
    }

    BinaryNodeType newExportFromDeclaration(uint32_t begin, Node exportSpecSet, Node moduleSpec) {
        BinaryNode* decl = new_<BinaryNode>(ParseNodeKind::ExportFrom, JSOP_NOP, exportSpecSet,
                                            moduleSpec);
        if (!decl) {
            return nullptr;
        }
        decl->pn_pos.begin = begin;
        return decl;
    }

    BinaryNodeType newExportDefaultDeclaration(Node kid, Node maybeBinding, const TokenPos& pos) {
        if (maybeBinding) {
            MOZ_ASSERT(maybeBinding->isKind(ParseNodeKind::Name));
            MOZ_ASSERT(!maybeBinding->isInParens());

            checkAndSetIsDirectRHSAnonFunction(kid);
        }

        return new_<BinaryNode>(ParseNodeKind::ExportDefault, JSOP_NOP, pos, kid, maybeBinding);
    }

    BinaryNodeType newExportSpec(Node bindingName, Node exportName) {
        return newBinary(ParseNodeKind::ExportSpec, bindingName, exportName);
    }

    NullaryNodeType newExportBatchSpec(const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::ExportBatchSpec, JSOP_NOP, pos);
    }

    BinaryNodeType newImportMeta(NullaryNodeType importHolder, NullaryNodeType metaHolder) {
        return new_<BinaryNode>(ParseNodeKind::ImportMeta, JSOP_NOP, importHolder, metaHolder);
    }

    BinaryNodeType newCallImport(NullaryNodeType importHolder, Node singleArg) {
        return new_<BinaryNode>(ParseNodeKind::CallImport, JSOP_NOP, importHolder, singleArg);
    }

    UnaryNodeType newExprStatement(Node expr, uint32_t end) {
        MOZ_ASSERT(expr->pn_pos.end <= end);
        return new_<UnaryNode>(ParseNodeKind::ExpressionStatement,
                               TokenPos(expr->pn_pos.begin, end), expr);
    }

    TernaryNodeType newIfStatement(uint32_t begin, Node cond, Node thenBranch, Node elseBranch) {
        TernaryNode* node = new_<TernaryNode>(ParseNodeKind::If, cond, thenBranch, elseBranch);
        if (!node) {
            return nullptr;
        }
        node->pn_pos.begin = begin;
        return node;
    }

    BinaryNodeType newDoWhileStatement(Node body, Node cond, const TokenPos& pos) {
        return new_<BinaryNode>(ParseNodeKind::DoWhile, JSOP_NOP, pos, body, cond);
    }

    BinaryNodeType newWhileStatement(uint32_t begin, Node cond, Node body) {
        TokenPos pos(begin, body->pn_pos.end);
        return new_<BinaryNode>(ParseNodeKind::While, JSOP_NOP, pos, cond, body);
    }

    ForNodeType newForStatement(uint32_t begin, TernaryNodeType forHead, Node body,
                                unsigned iflags)
    {
        return new_<ForNode>(TokenPos(begin, body->pn_pos.end), forHead, body, iflags);
    }

    TernaryNodeType newForHead(Node init, Node test, Node update, const TokenPos& pos) {
        return new_<TernaryNode>(ParseNodeKind::ForHead, init, test, update, pos);
    }

    TernaryNodeType newForInOrOfHead(ParseNodeKind kind, Node target, Node iteratedExpr,
                                     const TokenPos& pos)
    {
        MOZ_ASSERT(kind == ParseNodeKind::ForIn || kind == ParseNodeKind::ForOf);
        return new_<TernaryNode>(kind, target, nullptr, iteratedExpr, pos);
    }

    SwitchStatementType newSwitchStatement(uint32_t begin, Node discriminant,
                                           LexicalScopeNodeType lexicalForCaseList,
                                           bool hasDefault)
    {
        return new_<SwitchStatement>(begin, discriminant, lexicalForCaseList, hasDefault);
    }

    CaseClauseType newCaseOrDefault(uint32_t begin, Node expr, Node body) {
        return new_<CaseClause>(expr, body, begin);
    }

    ContinueStatementType newContinueStatement(PropertyName* label, const TokenPos& pos) {
        return new_<ContinueStatement>(label, pos);
    }

    BreakStatementType newBreakStatement(PropertyName* label, const TokenPos& pos) {
        return new_<BreakStatement>(label, pos);
    }

    UnaryNodeType newReturnStatement(Node expr, const TokenPos& pos) {
        MOZ_ASSERT_IF(expr, pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(ParseNodeKind::Return, pos, expr);
    }

    UnaryNodeType newExpressionBody(Node expr) {
        return new_<UnaryNode>(ParseNodeKind::Return, expr->pn_pos, expr);
    }

    BinaryNodeType newWithStatement(uint32_t begin, Node expr, Node body) {
        return new_<BinaryNode>(ParseNodeKind::With, JSOP_NOP, TokenPos(begin, body->pn_pos.end),
                                expr, body);
    }

    LabeledStatementType newLabeledStatement(PropertyName* label, Node stmt, uint32_t begin) {
        return new_<LabeledStatement>(label, stmt, begin);
    }

    UnaryNodeType newThrowStatement(Node expr, const TokenPos& pos) {
        MOZ_ASSERT(pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(ParseNodeKind::Throw, pos, expr);
    }

    TernaryNodeType newTryStatement(uint32_t begin, Node body, LexicalScopeNodeType catchScope,
                                    Node finallyBlock)
    {
        return new_<TryNode>(begin, body, catchScope, finallyBlock);
    }

    DebuggerStatementType newDebuggerStatement(const TokenPos& pos) {
        return new_<DebuggerStatement>(pos);
    }

    NameNodeType newPropertyName(PropertyName* name, const TokenPos& pos) {
        return new_<NameNode>(ParseNodeKind::PropertyName, JSOP_NOP, name, pos);
    }

    PropertyAccessType newPropertyAccess(Node expr, NameNodeType key) {
        return new_<PropertyAccess>(expr, key, expr->pn_pos.begin, key->pn_pos.end);
    }

    PropertyByValueType newPropertyByValue(Node lhs, Node index, uint32_t end) {
        return new_<PropertyByValue>(lhs, index, lhs->pn_pos.begin, end);
    }

    bool setupCatchScope(LexicalScopeNodeType lexicalScope, Node catchName, Node catchBody) {
        BinaryNode* catchClause;
        if (catchName) {
            catchClause = new_<BinaryNode>(ParseNodeKind::Catch, JSOP_NOP, catchName, catchBody);
        } else {
            catchClause = new_<BinaryNode>(ParseNodeKind::Catch, JSOP_NOP, catchBody->pn_pos,
                                       catchName, catchBody);
        }
        if (!catchClause) {
            return false;
        }
        lexicalScope->setScopeBody(catchClause);
        return true;
    }

    inline MOZ_MUST_USE bool setLastFunctionFormalParameterDefault(CodeNodeType funNode,
                                                                   Node defaultValue);

  private:
    void checkAndSetIsDirectRHSAnonFunction(ParseNode* pn) {
        if (IsAnonymousFunctionDefinition(pn)) {
            pn->setDirectRHSAnonFunction(true);
        }
    }

  public:
    CodeNodeType newFunctionStatement(const TokenPos& pos) {
        return new_<CodeNode>(ParseNodeKind::Function, JSOP_NOP, pos);
    }

    CodeNodeType newFunctionExpression(const TokenPos& pos) {
        return new_<CodeNode>(ParseNodeKind::Function, JSOP_LAMBDA, pos);
    }

    CodeNodeType newArrowFunction(const TokenPos& pos) {
        return new_<CodeNode>(ParseNodeKind::Function, JSOP_LAMBDA_ARROW, pos);
    }

    BinaryNodeType newObjectMethodOrPropertyDefinition(Node key, Node value, AccessorType atype) {
        MOZ_ASSERT(isUsableAsObjectPropertyName(key));

        return newBinary(ParseNodeKind::Colon, key, value, AccessorTypeToJSOp(atype));
    }

    void setFunctionFormalParametersAndBody(CodeNodeType funNode, ListNodeType paramsBody) {
        MOZ_ASSERT_IF(paramsBody, paramsBody->isKind(ParseNodeKind::ParamsBody));
        funNode->setBody(paramsBody);
    }
    void setFunctionBox(CodeNodeType funNode, FunctionBox* funbox) {
        MOZ_ASSERT(funNode->isKind(ParseNodeKind::Function));
        funNode->setFunbox(funbox);
        funbox->functionNode = funNode;
    }
    void addFunctionFormalParameter(CodeNodeType funNode, Node argpn) {
        addList(/* list = */ funNode->body(), /* child = */ argpn);
    }
    void setFunctionBody(CodeNodeType funNode, LexicalScopeNodeType body) {
        MOZ_ASSERT(funNode->body()->isKind(ParseNodeKind::ParamsBody));
        addList(/* list = */ funNode->body(), /* child = */ body);
    }

    CodeNodeType newModule(const TokenPos& pos) {
        return new_<CodeNode>(ParseNodeKind::Module, JSOP_NOP, pos);
    }

    LexicalScopeNodeType newLexicalScope(LexicalScope::Data* bindings, Node body) {
        return new_<LexicalScopeNode>(bindings, body);
    }

    BinaryNodeType newNewExpression(uint32_t begin, Node ctor, Node args) {
        return new_<BinaryNode>(ParseNodeKind::New, JSOP_NEW, TokenPos(begin, args->pn_pos.end),
                                ctor, args);
    }

    AssignmentNodeType newAssignment(ParseNodeKind kind, Node lhs, Node rhs) {
        if (kind == ParseNodeKind::Assign && lhs->isKind(ParseNodeKind::Name) &&
            !lhs->isInParens())
        {
            checkAndSetIsDirectRHSAnonFunction(rhs);
        }

        return new_<AssignmentNode>(kind, JSOP_NOP, lhs, rhs);
    }

    bool isUnparenthesizedAssignment(Node node) {
        if (node->isKind(ParseNodeKind::Assign) && !node->isInParens()) {
            // ParseNodeKind::Assign is also (mis)used for things like
            // |var name = expr;|. But this method is only called on actual
            // expressions, so we can just assert the node's op is the one used
            // for plain assignment.
            MOZ_ASSERT(node->isOp(JSOP_NOP));
            return true;
        }

        return false;
    }

    bool isUnparenthesizedUnaryExpression(ParseNode* node) {
        if (!node->isInParens()) {
            ParseNodeKind kind = node->getKind();
            return kind == ParseNodeKind::Void ||
                   kind == ParseNodeKind::Not ||
                   kind == ParseNodeKind::BitNot ||
                   kind == ParseNodeKind::Pos ||
                   kind == ParseNodeKind::Neg ||
                   IsTypeofKind(kind) ||
                   IsDeleteKind(kind);
        }
        return false;
    }

    bool isReturnStatement(ParseNode* node) {
        return node->isKind(ParseNodeKind::Return);
    }

    bool isStatementPermittedAfterReturnStatement(ParseNode *node) {
        ParseNodeKind kind = node->getKind();
        return kind == ParseNodeKind::Function ||
               kind == ParseNodeKind::Var ||
               kind == ParseNodeKind::Break ||
               kind == ParseNodeKind::Throw ||
               kind == ParseNodeKind::EmptyStatement;
    }

    bool isSuperBase(ParseNode* node) {
        return node->isKind(ParseNodeKind::SuperBase);
    }

    bool isUsableAsObjectPropertyName(ParseNode* node) {
        return node->isKind(ParseNodeKind::Number)
            || node->isKind(ParseNodeKind::ObjectPropertyName)
            || node->isKind(ParseNodeKind::String)
            || node->isKind(ParseNodeKind::ComputedName);
    }

    inline MOZ_MUST_USE bool finishInitializerAssignment(NameNodeType nameNode, Node init);

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
        return kind == ParseNodeKind::Var ||
               kind == ParseNodeKind::Let ||
               kind == ParseNodeKind::Const;
    }

    ListNodeType newList(ParseNodeKind kind, const TokenPos& pos) {
        MOZ_ASSERT(!isDeclarationKind(kind));
        return new_<ListNode>(kind, JSOP_NOP, pos);
    }

  public:
    ListNodeType newList(ParseNodeKind kind, Node kid) {
        MOZ_ASSERT(!isDeclarationKind(kind));
        return new_<ListNode>(kind, JSOP_NOP, kid);
    }

    ListNodeType newDeclarationList(ParseNodeKind kind, const TokenPos& pos) {
        MOZ_ASSERT(isDeclarationKind(kind));
        return new_<ListNode>(kind, JSOP_NOP, pos);
    }

    bool isDeclarationList(ParseNode* node) {
        return isDeclarationKind(node->getKind());
    }

    Node singleBindingFromDeclaration(ListNodeType decl) {
        MOZ_ASSERT(isDeclarationList(decl));
        MOZ_ASSERT(decl->count() == 1);
        return decl->head();
    }

    ListNodeType newCommaExpressionList(Node kid) {
        return new_<ListNode>(ParseNodeKind::Comma, JSOP_NOP, kid);
    }

    void addList(ListNodeType list, Node kid) {
        if (sourceKind_ == SourceKind::Text) {
            list->append(kid);
        } else {
            list->appendWithoutOrderAssumption(kid);
        }
    }

    void setOp(ParseNode* pn, JSOp op) {
        pn->setOp(op);
    }
    void setListHasNonConstInitializer(ListNodeType literal) {
        literal->setHasNonConstInitializer();
    }
    template <typename NodeType>
    MOZ_MUST_USE NodeType parenthesize(NodeType node) {
        node->setInParens(true);
        return node;
    }
    template <typename NodeType>
    MOZ_MUST_USE NodeType setLikelyIIFE(NodeType node) {
        return parenthesize(node);
    }
    void setInDirectivePrologue(UnaryNodeType exprStmt) {
        exprStmt->setIsDirectivePrologueMember();
    }

    bool isName(ParseNode* node) {
        return node->isKind(ParseNodeKind::Name);
    }

    bool isArgumentsName(ParseNode* node, JSContext* cx) {
        return node->isKind(ParseNodeKind::Name) &&
               node->as<NameNode>().atom() == cx->names().arguments;
    }

    bool isEvalName(ParseNode* node, JSContext* cx) {
        return node->isKind(ParseNodeKind::Name) &&
               node->as<NameNode>().atom() == cx->names().eval;
    }

    bool isAsyncKeyword(ParseNode* node, JSContext* cx) {
        return node->isKind(ParseNodeKind::Name) &&
               node->pn_pos.begin + strlen("async") == node->pn_pos.end &&
               node->as<NameNode>().atom() == cx->names().async;
    }

    PropertyName* maybeDottedProperty(ParseNode* pn) {
        return pn->is<PropertyAccess>() ? &pn->as<PropertyAccess>().name() : nullptr;
    }
    JSAtom* isStringExprStatement(ParseNode* pn, TokenPos* pos) {
        if (pn->is<UnaryNode>()) {
            UnaryNode* unary = &pn->as<UnaryNode>();
            if (JSAtom* atom = unary->isStringExprStatement()) {
                *pos = unary->kid()->pn_pos;
                return atom;
            }
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
FullParseHandler::setLastFunctionFormalParameterDefault(CodeNodeType funNode, Node defaultValue)
{
    MOZ_ASSERT(funNode->isKind(ParseNodeKind::Function));
    ListNode* body = funNode->body();
    ParseNode* arg = body->last();
    ParseNode* pn = newAssignment(ParseNodeKind::Assign, arg, defaultValue);
    if (!pn) {
        return false;
    }

    body->replaceLast(pn);
    return true;
}

inline bool
FullParseHandler::finishInitializerAssignment(NameNodeType nameNode, Node init)
{
    MOZ_ASSERT(nameNode->isKind(ParseNodeKind::Name));
    MOZ_ASSERT(!nameNode->isInParens());

    checkAndSetIsDirectRHSAnonFunction(init);

    nameNode->setInitializer(init);
    nameNode->setOp(JSOP_SETNAME);

    /* The declarator's position must include the initializer. */
    nameNode->pn_pos.end = init->pn_pos.end;
    return true;
}

} // namespace frontend
} // namespace js

#endif /* frontend_FullParseHandler_h */

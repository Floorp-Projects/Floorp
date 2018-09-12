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

    ParseNode* newName(PropertyName* name, const TokenPos& pos, JSContext* cx)
    {
        return new_<NameNode>(ParseNodeKind::Name, JSOP_GETNAME, name, pos);
    }

    ParseNode* newComputedName(ParseNode* expr, uint32_t begin, uint32_t end) {
        TokenPos pos(begin, end);
        return new_<UnaryNode>(ParseNodeKind::ComputedName, pos, expr);
    }

    ParseNode* newObjectLiteralPropertyName(JSAtom* atom, const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::ObjectPropertyName, JSOP_NOP, pos, atom);
    }

    ParseNode* newNumber(double value, DecimalPoint decimalPoint, const TokenPos& pos) {
        ParseNode* pn = new_<NullaryNode>(ParseNodeKind::Number, pos);
        if (!pn) {
            return nullptr;
        }
        pn->initNumber(value, decimalPoint);
        return pn;
    }

    ParseNode* newBooleanLiteral(bool cond, const TokenPos& pos) {
        return new_<BooleanLiteral>(cond, pos);
    }

    ParseNode* newStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::String, JSOP_NOP, pos, atom);
    }

    ParseNode* newTemplateStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::TemplateString, JSOP_NOP, pos, atom);
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
        if (!objbox) {
            return null();
        }
        return new_<RegExpLiteral>(objbox, pos);
    }

    ConditionalExpressionType newConditional(Node cond, Node thenExpr, Node elseExpr) {
        return new_<ConditionalExpression>(cond, thenExpr, elseExpr);
    }

    ParseNode* newDelete(uint32_t begin, ParseNode* expr) {
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

    ParseNode* newTypeof(uint32_t begin, ParseNode* kid) {
        ParseNodeKind pnk = kid->isKind(ParseNodeKind::Name)
                            ? ParseNodeKind::TypeOfName
                            : ParseNodeKind::TypeOfExpr;
        return newUnary(pnk, begin, kid);
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
        return new_<UnaryNode>(ParseNodeKind::Spread, pos, kid);
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

    ListNodeType newArrayLiteral(uint32_t begin) {
        return new_<ListNode>(ParseNodeKind::Array, TokenPos(begin, begin + 1));
    }

    MOZ_MUST_USE bool addElision(ListNodeType literal, const TokenPos& pos) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Array));

        ParseNode* elision = new_<NullaryNode>(ParseNodeKind::Elision, pos);
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

    ParseNode* newCall(ParseNode* callee, ParseNode* args) {
        return new_<BinaryNode>(ParseNodeKind::Call, JSOP_CALL, callee, args);
    }

    ListNodeType newArguments(const TokenPos& pos) {
        return new_<ListNode>(ParseNodeKind::Arguments, JSOP_NOP, pos);
    }

    ParseNode* newSuperCall(ParseNode* callee, ParseNode* args) {
        return new_<BinaryNode>(ParseNodeKind::SuperCall, JSOP_SUPERCALL, callee, args);
    }

    ParseNode* newTaggedTemplate(ParseNode* tag, ParseNode* args) {
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
    ParseNode* newClassNames(ParseNode* outer, ParseNode* inner, const TokenPos& pos) {
        return new_<ClassNames>(outer, inner, pos);
    }
    ParseNode* newNewTarget(ParseNode* newHolder, ParseNode* targetHolder) {
        return new_<BinaryNode>(ParseNodeKind::NewTarget, JSOP_NOP, newHolder, targetHolder);
    }
    ParseNode* newPosHolder(const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::PosHolder, pos);
    }
    ParseNode* newSuperBase(ParseNode* thisName, const TokenPos& pos) {
        return new_<UnaryNode>(ParseNodeKind::SuperBase, pos, thisName);
    }
    MOZ_MUST_USE bool addPrototypeMutation(ListNodeType literal, uint32_t begin, Node expr) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Object));

        // Object literals with mutated [[Prototype]] are non-constant so that
        // singleton objects will have Object.prototype as their [[Prototype]].
        literal->setHasNonConstInitializer();

        ParseNode* mutation = newUnary(ParseNodeKind::MutateProto, begin, expr);
        if (!mutation) {
            return false;
        }
        addList(/* list = */ literal, /* child = */ mutation);
        return true;
    }

    ParseNode* newPropertyDefinition(ParseNode* key, ParseNode* val) {
        MOZ_ASSERT(isUsableAsObjectPropertyName(key));
        checkAndSetIsDirectRHSAnonFunction(val);
        return newBinary(ParseNodeKind::Colon, key, val, JSOP_INITPROP);
    }

    void addPropertyDefinition(ListNodeType literal, Node propdef) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Object));
        MOZ_ASSERT(propdef->isKind(ParseNodeKind::Colon));

        if (!propdef->pn_right->isConstant()) {
            literal->setHasNonConstInitializer();
        }

        addList(/* list = */ literal, /* child = */ propdef);
    }

    MOZ_MUST_USE bool addPropertyDefinition(ListNodeType literal, Node key, Node val) {
        ParseNode* propdef = newPropertyDefinition(key, val);
        if (!propdef) {
            return false;
        }
        addPropertyDefinition(literal, propdef);
        return true;
    }

    MOZ_MUST_USE bool addShorthand(ListNodeType literal, Node name, Node expr) {
        MOZ_ASSERT(literal->isKind(ParseNodeKind::Object));
        MOZ_ASSERT(name->isKind(ParseNodeKind::ObjectPropertyName));
        MOZ_ASSERT(expr->isKind(ParseNodeKind::Name));
        MOZ_ASSERT(name->pn_atom == expr->pn_atom);

        literal->setHasNonConstInitializer();
        ParseNode* propdef = newBinary(ParseNodeKind::Shorthand, name, expr, JSOP_INITPROP);
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
                                                Node fn, AccessorType atype)
    {
        literal->setHasNonConstInitializer();

        checkAndSetIsDirectRHSAnonFunction(fn);

        ParseNode* propdef = newObjectMethodOrPropertyDefinition(key, fn, atype);
        if (!propdef) {
            return false;
        }

        addList(/* list = */ literal, /* child = */ propdef);
        return true;
    }

    MOZ_MUST_USE bool addClassMethodDefinition(ListNodeType methodList, Node key,
                                               Node fn, AccessorType atype, bool isStatic)
    {
        MOZ_ASSERT(methodList->isKind(ParseNodeKind::ClassMethodList));
        MOZ_ASSERT(isUsableAsObjectPropertyName(key));

        checkAndSetIsDirectRHSAnonFunction(fn);

        ParseNode* classMethod = new_<ClassMethod>(key, fn, AccessorTypeToJSOp(atype), isStatic);
        if (!classMethod) {
            return false;
        }
        addList(/* list = */ methodList, /* child = */ classMethod);
        return true;
    }

    ParseNode* newInitialYieldExpression(uint32_t begin, ParseNode* gen) {
        TokenPos pos(begin, begin + 1);
        return new_<UnaryNode>(ParseNodeKind::InitialYield, pos, gen);
    }

    ParseNode* newYieldExpression(uint32_t begin, ParseNode* value) {
        TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
        return new_<UnaryNode>(ParseNodeKind::Yield, pos, value);
    }

    ParseNode* newYieldStarExpression(uint32_t begin, ParseNode* value) {
        TokenPos pos(begin, value->pn_pos.end);
        return new_<UnaryNode>(ParseNodeKind::YieldStar, pos, value);
    }

    ParseNode* newAwaitExpression(uint32_t begin, ParseNode* value) {
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

    void addCaseStatementToList(ListNodeType list, Node caseClause) {
        MOZ_ASSERT(list->isKind(ParseNodeKind::StatementList));
        MOZ_ASSERT(caseClause->isKind(ParseNodeKind::Case));
        MOZ_ASSERT(caseClause->pn_right->isKind(ParseNodeKind::StatementList));

        addList(/* list = */ list, /* child = */ caseClause);

        if (caseClause->pn_right->as<ListNode>().hasTopLevelFunctionDeclarations()) {
            list->setHasTopLevelFunctionDeclarations();
        }
    }

    MOZ_MUST_USE bool prependInitialYield(ListNodeType stmtList, Node genName) {
        MOZ_ASSERT(stmtList->isKind(ParseNodeKind::StatementList));

        TokenPos yieldPos(stmtList->pn_pos.begin, stmtList->pn_pos.begin + 1);
        ParseNode* makeGen = new_<NullaryNode>(ParseNodeKind::Generator, yieldPos);
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

        ParseNode* initialYield = newInitialYieldExpression(yieldPos.begin, genInit);
        if (!initialYield) {
            return false;
        }

        stmtList->prepend(initialYield);
        return true;
    }

    ParseNode* newSetThis(ParseNode* thisName, ParseNode* val) {
        MOZ_ASSERT(thisName->getOp() == JSOP_GETNAME);
        thisName->setOp(JSOP_SETNAME);
        return newBinary(ParseNodeKind::SetThis, thisName, val);
    }

    ParseNode* newEmptyStatement(const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::EmptyStatement, pos);
    }

    ParseNode* newImportDeclaration(ParseNode* importSpecSet,
                                    ParseNode* moduleSpec, const TokenPos& pos)
    {
        ParseNode* pn = new_<BinaryNode>(ParseNodeKind::Import, JSOP_NOP, pos,
                                         importSpecSet, moduleSpec);
        if (!pn) {
            return null();
        }
        return pn;
    }

    ParseNode* newImportSpec(ParseNode* importNameNode, ParseNode* bindingName) {
        return newBinary(ParseNodeKind::ImportSpec, importNameNode, bindingName);
    }

    ParseNode* newExportDeclaration(ParseNode* kid, const TokenPos& pos) {
        return new_<UnaryNode>(ParseNodeKind::Export, pos, kid);
    }

    ParseNode* newExportFromDeclaration(uint32_t begin, ParseNode* exportSpecSet,
                                        ParseNode* moduleSpec)
    {
        ParseNode* pn = new_<BinaryNode>(ParseNodeKind::ExportFrom, JSOP_NOP, exportSpecSet,
                                         moduleSpec);
        if (!pn) {
            return null();
        }
        pn->pn_pos.begin = begin;
        return pn;
    }

    ParseNode* newExportDefaultDeclaration(ParseNode* kid, ParseNode* maybeBinding,
                                           const TokenPos& pos) {
        if (maybeBinding) {
            MOZ_ASSERT(maybeBinding->isKind(ParseNodeKind::Name));
            MOZ_ASSERT(!maybeBinding->isInParens());

            checkAndSetIsDirectRHSAnonFunction(kid);
        }

        return new_<BinaryNode>(ParseNodeKind::ExportDefault, JSOP_NOP, pos, kid, maybeBinding);
    }

    ParseNode* newExportSpec(ParseNode* bindingName, ParseNode* exportName) {
        return newBinary(ParseNodeKind::ExportSpec, bindingName, exportName);
    }

    ParseNode* newExportBatchSpec(const TokenPos& pos) {
        return new_<NullaryNode>(ParseNodeKind::ExportBatchSpec, JSOP_NOP, pos);
    }

    ParseNode* newImportMeta(ParseNode* importHolder, ParseNode* metaHolder) {
        return new_<BinaryNode>(ParseNodeKind::ImportMeta, JSOP_NOP, importHolder, metaHolder);
    }

    ParseNode* newCallImport(ParseNode* importHolder, ParseNode* singleArg) {
        return new_<BinaryNode>(ParseNodeKind::CallImport, JSOP_NOP, importHolder, singleArg);
    }

    ParseNode* newExprStatement(ParseNode* expr, uint32_t end) {
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

    ParseNode* newDoWhileStatement(ParseNode* body, ParseNode* cond, const TokenPos& pos) {
        return new_<BinaryNode>(ParseNodeKind::DoWhile, JSOP_NOP, pos, body, cond);
    }

    ParseNode* newWhileStatement(uint32_t begin, ParseNode* cond, ParseNode* body) {
        TokenPos pos(begin, body->pn_pos.end);
        return new_<BinaryNode>(ParseNodeKind::While, JSOP_NOP, pos, cond, body);
    }

    Node newForStatement(uint32_t begin, TernaryNodeType forHead, Node body, unsigned iflags) {
        /* A FOR node is binary, left is loop control and right is the body. */
        JSOp op = forHead->isKind(ParseNodeKind::ForIn) ? JSOP_ITER : JSOP_NOP;
        BinaryNode* pn = new_<BinaryNode>(ParseNodeKind::For, op,
                                          TokenPos(begin, body->pn_pos.end),
                                          forHead, body);
        if (!pn) {
            return null();
        }
        pn->pn_iflags = iflags;
        return pn;
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

    ParseNode* newSwitchStatement(uint32_t begin, ParseNode* discriminant,
                                  ParseNode* lexicalForCaseList, bool hasDefault)
    {
        return new_<SwitchStatement>(begin, discriminant, lexicalForCaseList, hasDefault);
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
        return new_<UnaryNode>(ParseNodeKind::Return, pos, expr);
    }

    ParseNode* newExpressionBody(ParseNode* expr) {
        return new_<UnaryNode>(ParseNodeKind::Return, expr->pn_pos, expr);
    }

    ParseNode* newWithStatement(uint32_t begin, ParseNode* expr, ParseNode* body) {
        return new_<BinaryNode>(ParseNodeKind::With, JSOP_NOP, TokenPos(begin, body->pn_pos.end),
                                expr, body);
    }

    ParseNode* newLabeledStatement(PropertyName* label, ParseNode* stmt, uint32_t begin) {
        return new_<LabeledStatement>(label, stmt, begin);
    }

    ParseNode* newThrowStatement(ParseNode* expr, const TokenPos& pos) {
        MOZ_ASSERT(pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(ParseNodeKind::Throw, pos, expr);
    }

    TernaryNodeType newTryStatement(uint32_t begin, Node body, Node catchScope,
                                    Node finallyBlock)
    {
        TokenPos pos(begin, (finallyBlock ? finallyBlock : catchScope)->pn_pos.end);
        return new_<TernaryNode>(ParseNodeKind::Try, body, catchScope, finallyBlock, pos);
    }

    ParseNode* newDebuggerStatement(const TokenPos& pos) {
        return new_<DebuggerStatement>(pos);
    }

    ParseNode* newPropertyName(PropertyName* name, const TokenPos& pos) {
        return new_<NameNode>(ParseNodeKind::PropertyName, JSOP_NOP, name, pos);
    }

    ParseNode* newPropertyAccess(ParseNode* expr, ParseNode* key) {
        return new_<PropertyAccess>(expr, key, expr->pn_pos.begin, key->pn_pos.end);
    }

    ParseNode* newPropertyByValue(ParseNode* lhs, ParseNode* index, uint32_t end) {
        return new_<PropertyByValue>(lhs, index, lhs->pn_pos.begin, end);
    }

    bool setupCatchScope(ParseNode* lexicalScope, ParseNode* catchName, ParseNode* catchBody) {
        ParseNode* catchpn;
        if (catchName) {
            catchpn = new_<BinaryNode>(ParseNodeKind::Catch, JSOP_NOP, catchName, catchBody);
        } else {
            catchpn = new_<BinaryNode>(ParseNodeKind::Catch, JSOP_NOP, catchBody->pn_pos,
                                       catchName, catchBody);
        }
        if (!catchpn) {
            return false;
        }
        lexicalScope->setScopeBody(catchpn);
        return true;
    }

    inline MOZ_MUST_USE bool setLastFunctionFormalParameterDefault(ParseNode* funcpn,
                                                                   ParseNode* pn);

  private:
    void checkAndSetIsDirectRHSAnonFunction(ParseNode* pn) {
        if (IsAnonymousFunctionDefinition(pn)) {
            pn->setDirectRHSAnonFunction(true);
        }
    }

  public:
    ParseNode* newFunctionStatement(const TokenPos& pos) {
        return new_<CodeNode>(ParseNodeKind::Function, JSOP_NOP, pos);
    }

    ParseNode* newFunctionExpression(const TokenPos& pos) {
        return new_<CodeNode>(ParseNodeKind::Function, JSOP_LAMBDA, pos);
    }

    ParseNode* newArrowFunction(const TokenPos& pos) {
        return new_<CodeNode>(ParseNodeKind::Function, JSOP_LAMBDA_ARROW, pos);
    }

    ParseNode* newObjectMethodOrPropertyDefinition(ParseNode* key, ParseNode* fn, AccessorType atype) {
        MOZ_ASSERT(isUsableAsObjectPropertyName(key));

        return newBinary(ParseNodeKind::Colon, key, fn, AccessorTypeToJSOp(atype));
    }

    void setFunctionFormalParametersAndBody(ParseNode* funcNode, ParseNode* kid) {
        MOZ_ASSERT_IF(kid, kid->isKind(ParseNodeKind::ParamsBody));
        funcNode->pn_body = kid;
    }
    void setFunctionBox(ParseNode* pn, FunctionBox* funbox) {
        MOZ_ASSERT(pn->isKind(ParseNodeKind::Function));
        pn->pn_funbox = funbox;
        funbox->functionNode = pn;
    }
    void addFunctionFormalParameter(ParseNode* pn, ParseNode* argpn) {
        addList(/* list = */ &pn->pn_body->as<ListNode>(), /* child = */ argpn);
    }
    void setFunctionBody(ParseNode* fn, ParseNode* body) {
        MOZ_ASSERT(fn->pn_body->isKind(ParseNodeKind::ParamsBody));
        addList(/* list = */ &fn->pn_body->as<ListNode>(), /* child = */ body);
    }

    ParseNode* newModule(const TokenPos& pos) {
        return new_<CodeNode>(ParseNodeKind::Module, JSOP_NOP, pos);
    }

    ParseNode* newLexicalScope(LexicalScope::Data* bindings, ParseNode* body) {
        return new_<LexicalScopeNode>(bindings, body);
    }

    Node newNewExpression(uint32_t begin, ParseNode* ctor, ParseNode* args) {
        return new_<BinaryNode>(ParseNodeKind::New, JSOP_NEW, TokenPos(begin, args->pn_pos.end), ctor, args);
    }

    ParseNode* newAssignment(ParseNodeKind kind, ParseNode* lhs, ParseNode* rhs) {
        if (kind == ParseNodeKind::Assign && lhs->isKind(ParseNodeKind::Name) &&
            !lhs->isInParens())
        {
            checkAndSetIsDirectRHSAnonFunction(rhs);
        }

        return newBinary(kind, lhs, rhs);
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
        return kind == ParseNodeKind::Var ||
               kind == ParseNodeKind::Let ||
               kind == ParseNodeKind::Const;
    }

    ListNodeType newList(ParseNodeKind kind, const TokenPos& pos) {
        MOZ_ASSERT(!isDeclarationKind(kind));
        return new_<ListNode>(kind, JSOP_NOP, pos);
    }

  private:
    template<typename T>
    ParseNode* newList(ParseNodeKind kind, const T& begin) = delete;

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

    bool isName(ParseNode* node) {
        return node->isKind(ParseNodeKind::Name);
    }

    bool isArgumentsName(ParseNode* node, JSContext* cx) {
        return node->isKind(ParseNodeKind::Name) && node->pn_atom == cx->names().arguments;
    }

    bool isEvalName(ParseNode* node, JSContext* cx) {
        return node->isKind(ParseNodeKind::Name) && node->pn_atom == cx->names().eval;
    }

    bool isAsyncKeyword(ParseNode* node, JSContext* cx) {
        return node->isKind(ParseNodeKind::Name) &&
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
    MOZ_ASSERT(funcpn->isKind(ParseNodeKind::Function));
    MOZ_ASSERT(funcpn->isArity(PN_CODE));

    ListNode* body = &funcpn->pn_body->as<ListNode>();
    ParseNode* arg = body->last();
    ParseNode* pn = newAssignment(ParseNodeKind::Assign, arg, defaultValue);
    if (!pn) {
        return false;
    }

    body->replaceLast(pn);
    return true;
}

inline bool
FullParseHandler::finishInitializerAssignment(ParseNode* pn, ParseNode* init)
{
    MOZ_ASSERT(pn->isKind(ParseNodeKind::Name));
    MOZ_ASSERT(!pn->isInParens());

    checkAndSetIsDirectRHSAnonFunction(init);

    pn->pn_expr = init;
    pn->setOp(JSOP_SETNAME);

    /* The declarator's position must include the initializer. */
    pn->pn_pos.end = init->pn_pos.end;
    return true;
}

} // namespace frontend
} // namespace js

#endif /* frontend_FullParseHandler_h */

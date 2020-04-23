/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FullParseHandler_h
#define frontend_FullParseHandler_h

#include "mozilla/Attributes.h"
#include "mozilla/PodOperations.h"

#include <cstddef>  // std::nullptr_t
#include <string.h>

#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"
#include "frontend/Stencil.h"
#include "vm/JSContext.h"

namespace js {

class RegExpObject;

namespace frontend {

class TokenStreamAnyChars;

enum class SourceKind {
  // We are parsing from a text source (Parser.h)
  Text,
  // We are parsing from a binary source (BinASTParser.h)
  Binary,
};

// Parse handler used when generating a full parse tree for all code which the
// parser encounters.
class FullParseHandler {
  ParseNodeAllocator allocator;

  ParseNode* allocParseNode(size_t size) {
    return static_cast<ParseNode*>(allocator.allocNode(size));
  }

  /*
   * If this is a full parse to construct the bytecode for a function that
   * was previously lazily parsed, we still don't want to full parse the
   * inner functions. These members are used for this functionality:
   *
   * - lazyOuterFunction_ holds the lazyScript for this current parse
   * - lazyInnerFunctionIndex is used as we skip over inner functions
   *   (see skipLazyInnerFunction),
   */
  const Rooted<BaseScript*> lazyOuterFunction_;
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

  bool isPropertyAccess(Node node) {
    return node->isKind(ParseNodeKind::DotExpr) ||
           node->isKind(ParseNodeKind::ElemExpr);
  }

  bool isOptionalPropertyAccess(Node node) {
    return node->isKind(ParseNodeKind::OptionalDotExpr) ||
           node->isKind(ParseNodeKind::OptionalElemExpr);
  }

  bool isFunctionCall(Node node) {
    // Note: super() is a special form, *not* a function call.
    return node->isKind(ParseNodeKind::CallExpr);
  }

  static bool isUnparenthesizedDestructuringPattern(Node node) {
    return !node->isInParens() && (node->isKind(ParseNodeKind::ObjectExpr) ||
                                   node->isKind(ParseNodeKind::ArrayExpr));
  }

  static bool isParenthesizedDestructuringPattern(Node node) {
    // Technically this isn't a destructuring pattern at all -- the grammar
    // doesn't treat it as such.  But we need to know when this happens to
    // consider it a SyntaxError rather than an invalid-left-hand-side
    // ReferenceError.
    return node->isInParens() && (node->isKind(ParseNodeKind::ObjectExpr) ||
                                  node->isKind(ParseNodeKind::ArrayExpr));
  }

  FullParseHandler(JSContext* cx, LifoAlloc& alloc,
                   BaseScript* lazyOuterFunction,
                   SourceKind kind = SourceKind::Text)
      : allocator(cx, alloc),
        lazyOuterFunction_(cx, lazyOuterFunction),
        lazyInnerFunctionIndex(0),
        lazyClosedOverBindingIndex(0),
        sourceKind_(kind) {
    // The BaseScript::gcthings() array contains the inner function list
    // followed by the closed-over bindings data. Advance the index for
    // closed-over bindings to the end of the inner functions. The
    // nextLazyInnerFunction / nextLazyClosedOverBinding accessors confirm we
    // have the expected types. See also: BaseScript::CreateLazy.
    if (lazyOuterFunction) {
      for (JS::GCCellPtr gcThing : lazyOuterFunction->gcthings()) {
        if (gcThing.is<JSObject>()) {
          lazyClosedOverBindingIndex++;
        } else {
          break;
        }
      }
    }
  }

  static NullNode null() { return NullNode(); }

#define DECLARE_AS(typeName, longTypeName, asMethodName) \
  static longTypeName asMethodName(Node node) { return &node->as<typeName>(); }
  FOR_EACH_PARSENODE_SUBCLASS(DECLARE_AS)
#undef DECLARE_AS

  // The FullParseHandler may be used to create nodes for text sources
  // (from Parser.h) or for binary sources (from BinASTParser.h). In the latter
  // case, some common assumptions on offsets are incorrect, e.g. in `a + b`,
  // `a`, `b` and `+` may be stored in any order. We use `sourceKind()`
  // to determine whether we need to check these assumptions.
  SourceKind sourceKind() const { return sourceKind_; }

  NameNodeType newName(PropertyName* name, const TokenPos& pos, JSContext* cx) {
    return new_<NameNode>(ParseNodeKind::Name, name, pos);
  }

  UnaryNodeType newComputedName(Node expr, uint32_t begin, uint32_t end) {
    TokenPos pos(begin, end);
    return new_<UnaryNode>(ParseNodeKind::ComputedName, pos, expr);
  }

  NameNodeType newObjectLiteralPropertyName(JSAtom* atom, const TokenPos& pos) {
    return new_<NameNode>(ParseNodeKind::ObjectPropertyName, atom, pos);
  }

  NumericLiteralType newNumber(double value, DecimalPoint decimalPoint,
                               const TokenPos& pos) {
    return new_<NumericLiteral>(value, decimalPoint, pos);
  }

  BigIntLiteralType newBigInt(BigIntIndex index,
                              CompilationInfo& compilationInfo,
                              const TokenPos& pos) {
    return new_<BigIntLiteral>(index, compilationInfo, pos);
  }

  BooleanLiteralType newBooleanLiteral(bool cond, const TokenPos& pos) {
    return new_<BooleanLiteral>(cond, pos);
  }

  NameNodeType newStringLiteral(JSAtom* atom, const TokenPos& pos) {
    return new_<NameNode>(ParseNodeKind::StringExpr, atom, pos);
  }

  NameNodeType newTemplateStringLiteral(JSAtom* atom, const TokenPos& pos) {
    return new_<NameNode>(ParseNodeKind::TemplateStringExpr, atom, pos);
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

  void addToCallSiteObject(CallSiteNodeType callSiteObj, Node rawNode,
                           Node cookedNode) {
    MOZ_ASSERT(callSiteObj->isKind(ParseNodeKind::CallSiteObj));
    MOZ_ASSERT(rawNode->isKind(ParseNodeKind::TemplateStringExpr));
    MOZ_ASSERT(cookedNode->isKind(ParseNodeKind::TemplateStringExpr) ||
               cookedNode->isKind(ParseNodeKind::RawUndefinedExpr));

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

  RegExpLiteralType newRegExp(RegExpIndex index, const TokenPos& pos) {
    return new_<RegExpLiteral>(index, pos);
  }

  ConditionalExpressionType newConditional(Node cond, Node thenExpr,
                                           Node elseExpr) {
    return new_<ConditionalExpression>(cond, thenExpr, elseExpr);
  }

  UnaryNodeType newDelete(uint32_t begin, Node expr) {
    if (expr->isKind(ParseNodeKind::Name)) {
      return newUnary(ParseNodeKind::DeleteNameExpr, begin, expr);
    }

    if (expr->isKind(ParseNodeKind::DotExpr)) {
      return newUnary(ParseNodeKind::DeletePropExpr, begin, expr);
    }

    if (expr->isKind(ParseNodeKind::ElemExpr)) {
      return newUnary(ParseNodeKind::DeleteElemExpr, begin, expr);
    }

    if (expr->isKind(ParseNodeKind::OptionalChain)) {
      Node kid = expr->as<UnaryNode>().kid();
      // Handle property deletion explicitly. OptionalCall is handled
      // via DeleteExpr.
      if (kid->isKind(ParseNodeKind::DotExpr) ||
          kid->isKind(ParseNodeKind::OptionalDotExpr) ||
          kid->isKind(ParseNodeKind::ElemExpr) ||
          kid->isKind(ParseNodeKind::OptionalElemExpr)) {
        return newUnary(ParseNodeKind::DeleteOptionalChainExpr, begin, kid);
      }
    }

    return newUnary(ParseNodeKind::DeleteExpr, begin, expr);
  }

  UnaryNodeType newTypeof(uint32_t begin, Node kid) {
    ParseNodeKind pnk = kid->isKind(ParseNodeKind::Name)
                            ? ParseNodeKind::TypeOfNameExpr
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
  BinaryNodeType newBinary(ParseNodeKind kind, Node left, Node right) {
    TokenPos pos(left->pn_pos.begin, right->pn_pos.end);
    return new_<BinaryNode>(kind, pos, left, right);
  }

 public:
  Node appendOrCreateList(ParseNodeKind kind, Node left, Node right,
                          ParseContext* pc) {
    return ParseNode::appendOrCreateList(kind, left, right, this, pc);
  }

  // Expressions

  ListNodeType newArrayLiteral(uint32_t begin) {
    return new_<ListNode>(ParseNodeKind::ArrayExpr, TokenPos(begin, begin + 1));
  }

  MOZ_MUST_USE bool addElision(ListNodeType literal, const TokenPos& pos) {
    MOZ_ASSERT(literal->isKind(ParseNodeKind::ArrayExpr));

    NullaryNode* elision = new_<NullaryNode>(ParseNodeKind::Elision, pos);
    if (!elision) {
      return false;
    }
    addList(/* list = */ literal, /* kid = */ elision);
    literal->setHasArrayHoleOrSpread();
    literal->setHasNonConstInitializer();
    return true;
  }

  MOZ_MUST_USE bool addSpreadElement(ListNodeType literal, uint32_t begin,
                                     Node inner) {
    MOZ_ASSERT(literal->isKind(ParseNodeKind::ArrayExpr));

    UnaryNodeType spread = newSpread(begin, inner);
    if (!spread) {
      return false;
    }
    addList(/* list = */ literal, /* kid = */ spread);
    literal->setHasArrayHoleOrSpread();
    literal->setHasNonConstInitializer();
    return true;
  }

  void addArrayElement(ListNodeType literal, Node element) {
    if (!element->isConstant()) {
      literal->setHasNonConstInitializer();
    }
    addList(/* list = */ literal, /* kid = */ element);
  }

  CallNodeType newCall(Node callee, Node args, JSOp callOp) {
    return new_<CallNode>(ParseNodeKind::CallExpr, callOp, callee, args);
  }

  OptionalCallNodeType newOptionalCall(Node callee, Node args, JSOp callOp) {
    return new_<CallNode>(ParseNodeKind::OptionalCallExpr, callOp, callee,
                          args);
  }

  ListNodeType newArguments(const TokenPos& pos) {
    return new_<ListNode>(ParseNodeKind::Arguments, pos);
  }

  CallNodeType newSuperCall(Node callee, Node args, bool isSpread) {
    return new_<CallNode>(ParseNodeKind::SuperCallExpr,
                          isSpread ? JSOp::SpreadSuperCall : JSOp::SuperCall,
                          callee, args);
  }

  CallNodeType newTaggedTemplate(Node tag, Node args, JSOp callOp) {
    return new_<CallNode>(ParseNodeKind::TaggedTemplateExpr, callOp, tag, args);
  }

  ListNodeType newObjectLiteral(uint32_t begin) {
    return new_<ListNode>(ParseNodeKind::ObjectExpr,
                          TokenPos(begin, begin + 1));
  }

  ClassNodeType newClass(Node name, Node heritage,
                         LexicalScopeNodeType memberBlock,
                         const TokenPos& pos) {
    return new_<ClassNode>(name, heritage, memberBlock, pos);
  }
  ListNodeType newClassMemberList(uint32_t begin) {
    return new_<ListNode>(ParseNodeKind::ClassMemberList,
                          TokenPos(begin, begin + 1));
  }
  ClassNamesType newClassNames(Node outer, Node inner, const TokenPos& pos) {
    return new_<ClassNames>(outer, inner, pos);
  }
  BinaryNodeType newNewTarget(NullaryNodeType newHolder,
                              NullaryNodeType targetHolder) {
    return new_<BinaryNode>(ParseNodeKind::NewTargetExpr, newHolder,
                            targetHolder);
  }
  NullaryNodeType newPosHolder(const TokenPos& pos) {
    return new_<NullaryNode>(ParseNodeKind::PosHolder, pos);
  }
  UnaryNodeType newSuperBase(Node thisName, const TokenPos& pos) {
    return new_<UnaryNode>(ParseNodeKind::SuperBase, pos, thisName);
  }
  MOZ_MUST_USE bool addPrototypeMutation(ListNodeType literal, uint32_t begin,
                                         Node expr) {
    MOZ_ASSERT(literal->isKind(ParseNodeKind::ObjectExpr));

    // Object literals with mutated [[Prototype]] are non-constant so that
    // singleton objects will have Object.prototype as their [[Prototype]].
    literal->setHasNonConstInitializer();

    UnaryNode* mutation = newUnary(ParseNodeKind::MutateProto, begin, expr);
    if (!mutation) {
      return false;
    }
    addList(/* list = */ literal, /* kid = */ mutation);
    return true;
  }

  BinaryNodeType newPropertyDefinition(Node key, Node val) {
    MOZ_ASSERT(isUsableAsObjectPropertyName(key));
    checkAndSetIsDirectRHSAnonFunction(val);
    return new_<PropertyDefinition>(key, val, AccessorType::None);
  }

  void addPropertyDefinition(ListNodeType literal, BinaryNodeType propdef) {
    MOZ_ASSERT(literal->isKind(ParseNodeKind::ObjectExpr));
    MOZ_ASSERT(propdef->isKind(ParseNodeKind::PropertyDefinition));

    if (!propdef->right()->isConstant()) {
      literal->setHasNonConstInitializer();
    }

    addList(/* list = */ literal, /* kid = */ propdef);
  }

  MOZ_MUST_USE bool addPropertyDefinition(ListNodeType literal, Node key,
                                          Node val) {
    BinaryNode* propdef = newPropertyDefinition(key, val);
    if (!propdef) {
      return false;
    }
    addPropertyDefinition(literal, propdef);
    return true;
  }

  MOZ_MUST_USE bool addShorthand(ListNodeType literal, NameNodeType name,
                                 NameNodeType expr) {
    MOZ_ASSERT(literal->isKind(ParseNodeKind::ObjectExpr));
    MOZ_ASSERT(name->isKind(ParseNodeKind::ObjectPropertyName));
    MOZ_ASSERT(expr->isKind(ParseNodeKind::Name));
    MOZ_ASSERT(name->atom() == expr->atom());

    literal->setHasNonConstInitializer();
    BinaryNode* propdef = newBinary(ParseNodeKind::Shorthand, name, expr);
    if (!propdef) {
      return false;
    }
    addList(/* list = */ literal, /* kid = */ propdef);
    return true;
  }

  MOZ_MUST_USE bool addSpreadProperty(ListNodeType literal, uint32_t begin,
                                      Node inner) {
    MOZ_ASSERT(literal->isKind(ParseNodeKind::ObjectExpr));

    literal->setHasNonConstInitializer();
    ParseNode* spread = newSpread(begin, inner);
    if (!spread) {
      return false;
    }
    addList(/* list = */ literal, /* kid = */ spread);
    return true;
  }

  MOZ_MUST_USE bool addObjectMethodDefinition(ListNodeType literal, Node key,
                                              FunctionNodeType funNode,
                                              AccessorType atype) {
    literal->setHasNonConstInitializer();

    checkAndSetIsDirectRHSAnonFunction(funNode);

    ParseNode* propdef =
        newObjectMethodOrPropertyDefinition(key, funNode, atype);
    if (!propdef) {
      return false;
    }

    addList(/* list = */ literal, /* kid = */ propdef);
    return true;
  }

  MOZ_MUST_USE ClassMethod* newClassMethodDefinition(Node key,
                                                     FunctionNodeType funNode,
                                                     AccessorType atype,
                                                     bool isStatic) {
    MOZ_ASSERT(isUsableAsObjectPropertyName(key));

    checkAndSetIsDirectRHSAnonFunction(funNode);

    return new_<ClassMethod>(key, funNode, atype, isStatic);
  }

  MOZ_MUST_USE ClassField* newClassFieldDefinition(Node name,
                                                   FunctionNodeType initializer,
                                                   bool isStatic) {
    MOZ_ASSERT(isUsableAsObjectPropertyName(name));

    return new_<ClassField>(name, initializer, isStatic);
  }

  MOZ_MUST_USE bool addClassMemberDefinition(ListNodeType memberList,
                                             Node member) {
    MOZ_ASSERT(memberList->isKind(ParseNodeKind::ClassMemberList));
    // Constructors can be surrounded by LexicalScopes.
    MOZ_ASSERT(member->isKind(ParseNodeKind::ClassMethod) ||
               member->isKind(ParseNodeKind::ClassField) ||
               (member->isKind(ParseNodeKind::LexicalScope) &&
                member->as<LexicalScopeNode>().scopeBody()->isKind(
                    ParseNodeKind::ClassMethod)));

    addList(/* list = */ memberList, /* kid = */ member);
    return true;
  }

  UnaryNodeType newInitialYieldExpression(uint32_t begin, Node gen) {
    TokenPos pos(begin, begin + 1);
    return new_<UnaryNode>(ParseNodeKind::InitialYield, pos, gen);
  }

  UnaryNodeType newYieldExpression(uint32_t begin, Node value) {
    TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
    return new_<UnaryNode>(ParseNodeKind::YieldExpr, pos, value);
  }

  UnaryNodeType newYieldStarExpression(uint32_t begin, Node value) {
    TokenPos pos(begin, value->pn_pos.end);
    return new_<UnaryNode>(ParseNodeKind::YieldStarExpr, pos, value);
  }

  UnaryNodeType newAwaitExpression(uint32_t begin, Node value) {
    TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
    return new_<UnaryNode>(ParseNodeKind::AwaitExpr, pos, value);
  }

  UnaryNodeType newOptionalChain(uint32_t begin, Node value) {
    TokenPos pos(begin, value->pn_pos.end);
    return new_<UnaryNode>(ParseNodeKind::OptionalChain, pos, value);
  }

  // Statements

  ListNodeType newStatementList(const TokenPos& pos) {
    return new_<ListNode>(ParseNodeKind::StatementList, pos);
  }

  MOZ_MUST_USE bool isFunctionStmt(Node stmt) {
    while (stmt->isKind(ParseNodeKind::LabelStmt)) {
      stmt = stmt->as<LabeledStatement>().statement();
    }
    return stmt->is<FunctionNode>();
  }

  void addStatementToList(ListNodeType list, Node stmt) {
    MOZ_ASSERT(list->isKind(ParseNodeKind::StatementList));

    addList(/* list = */ list, /* kid = */ stmt);

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

    addList(/* list = */ list, /* kid = */ caseClause);

    if (caseClause->statementList()->hasTopLevelFunctionDeclarations()) {
      list->setHasTopLevelFunctionDeclarations();
    }
  }

  MOZ_MUST_USE bool prependInitialYield(ListNodeType stmtList, Node genName) {
    MOZ_ASSERT(stmtList->isKind(ParseNodeKind::StatementList));

    TokenPos yieldPos(stmtList->pn_pos.begin, stmtList->pn_pos.begin + 1);
    NullaryNode* makeGen =
        new_<NullaryNode>(ParseNodeKind::Generator, yieldPos);
    if (!makeGen) {
      return false;
    }

    ParseNode* genInit =
        newAssignment(ParseNodeKind::AssignExpr, /* lhs = */ genName,
                      /* rhs = */ makeGen);
    if (!genInit) {
      return false;
    }

    UnaryNode* initialYield =
        newInitialYieldExpression(yieldPos.begin, genInit);
    if (!initialYield) {
      return false;
    }

    stmtList->prepend(initialYield);
    return true;
  }

  BinaryNodeType newSetThis(Node thisName, Node value) {
    return newBinary(ParseNodeKind::SetThis, thisName, value);
  }

  NullaryNodeType newEmptyStatement(const TokenPos& pos) {
    return new_<NullaryNode>(ParseNodeKind::EmptyStmt, pos);
  }

  BinaryNodeType newImportDeclaration(Node importSpecSet, Node moduleSpec,
                                      const TokenPos& pos) {
    return new_<BinaryNode>(ParseNodeKind::ImportDecl, pos, importSpecSet,
                            moduleSpec);
  }

  BinaryNodeType newImportSpec(Node importNameNode, Node bindingName) {
    return newBinary(ParseNodeKind::ImportSpec, importNameNode, bindingName);
  }

  UnaryNodeType newExportDeclaration(Node kid, const TokenPos& pos) {
    return new_<UnaryNode>(ParseNodeKind::ExportStmt, pos, kid);
  }

  BinaryNodeType newExportFromDeclaration(uint32_t begin, Node exportSpecSet,
                                          Node moduleSpec) {
    BinaryNode* decl = new_<BinaryNode>(ParseNodeKind::ExportFromStmt,
                                        exportSpecSet, moduleSpec);
    if (!decl) {
      return nullptr;
    }
    decl->pn_pos.begin = begin;
    return decl;
  }

  BinaryNodeType newExportDefaultDeclaration(Node kid, Node maybeBinding,
                                             const TokenPos& pos) {
    if (maybeBinding) {
      MOZ_ASSERT(maybeBinding->isKind(ParseNodeKind::Name));
      MOZ_ASSERT(!maybeBinding->isInParens());

      checkAndSetIsDirectRHSAnonFunction(kid);
    }

    return new_<BinaryNode>(ParseNodeKind::ExportDefaultStmt, pos, kid,
                            maybeBinding);
  }

  BinaryNodeType newExportSpec(Node bindingName, Node exportName) {
    return newBinary(ParseNodeKind::ExportSpec, bindingName, exportName);
  }

  NullaryNodeType newExportBatchSpec(const TokenPos& pos) {
    return new_<NullaryNode>(ParseNodeKind::ExportBatchSpecStmt, pos);
  }

  BinaryNodeType newImportMeta(NullaryNodeType importHolder,
                               NullaryNodeType metaHolder) {
    return new_<BinaryNode>(ParseNodeKind::ImportMetaExpr, importHolder,
                            metaHolder);
  }

  BinaryNodeType newCallImport(NullaryNodeType importHolder, Node singleArg) {
    return new_<BinaryNode>(ParseNodeKind::CallImportExpr, importHolder,
                            singleArg);
  }

  UnaryNodeType newExprStatement(Node expr, uint32_t end) {
    MOZ_ASSERT_IF(sourceKind() == SourceKind::Text, expr->pn_pos.end <= end);
    return new_<UnaryNode>(ParseNodeKind::ExpressionStmt,
                           TokenPos(expr->pn_pos.begin, end), expr);
  }

  UnaryNodeType newExprStatement(Node expr) {
    return newExprStatement(expr, expr->pn_pos.end);
  }

  TernaryNodeType newIfStatement(uint32_t begin, Node cond, Node thenBranch,
                                 Node elseBranch) {
    TernaryNode* node =
        new_<TernaryNode>(ParseNodeKind::IfStmt, cond, thenBranch, elseBranch);
    if (!node) {
      return nullptr;
    }
    node->pn_pos.begin = begin;
    return node;
  }

  BinaryNodeType newDoWhileStatement(Node body, Node cond,
                                     const TokenPos& pos) {
    return new_<BinaryNode>(ParseNodeKind::DoWhileStmt, pos, body, cond);
  }

  BinaryNodeType newWhileStatement(uint32_t begin, Node cond, Node body) {
    TokenPos pos(begin, body->pn_pos.end);
    return new_<BinaryNode>(ParseNodeKind::WhileStmt, pos, cond, body);
  }

  ForNodeType newForStatement(uint32_t begin, TernaryNodeType forHead,
                              Node body, unsigned iflags) {
    return new_<ForNode>(TokenPos(begin, body->pn_pos.end), forHead, body,
                         iflags);
  }

  TernaryNodeType newForHead(Node init, Node test, Node update,
                             const TokenPos& pos) {
    return new_<TernaryNode>(ParseNodeKind::ForHead, init, test, update, pos);
  }

  TernaryNodeType newForInOrOfHead(ParseNodeKind kind, Node target,
                                   Node iteratedExpr, const TokenPos& pos) {
    MOZ_ASSERT(kind == ParseNodeKind::ForIn || kind == ParseNodeKind::ForOf);
    return new_<TernaryNode>(kind, target, nullptr, iteratedExpr, pos);
  }

  SwitchStatementType newSwitchStatement(
      uint32_t begin, Node discriminant,
      LexicalScopeNodeType lexicalForCaseList, bool hasDefault) {
    return new_<SwitchStatement>(begin, discriminant, lexicalForCaseList,
                                 hasDefault);
  }

  CaseClauseType newCaseOrDefault(uint32_t begin, Node expr, Node body) {
    return new_<CaseClause>(expr, body, begin);
  }

  ContinueStatementType newContinueStatement(PropertyName* label,
                                             const TokenPos& pos) {
    return new_<ContinueStatement>(label, pos);
  }

  BreakStatementType newBreakStatement(PropertyName* label,
                                       const TokenPos& pos) {
    return new_<BreakStatement>(label, pos);
  }

  UnaryNodeType newReturnStatement(Node expr, const TokenPos& pos) {
    MOZ_ASSERT_IF(expr && sourceKind() == SourceKind::Text,
                  pos.encloses(expr->pn_pos));
    return new_<UnaryNode>(ParseNodeKind::ReturnStmt, pos, expr);
  }

  UnaryNodeType newExpressionBody(Node expr) {
    return new_<UnaryNode>(ParseNodeKind::ReturnStmt, expr->pn_pos, expr);
  }

  BinaryNodeType newWithStatement(uint32_t begin, Node expr, Node body) {
    return new_<BinaryNode>(ParseNodeKind::WithStmt,
                            TokenPos(begin, body->pn_pos.end), expr, body);
  }

  LabeledStatementType newLabeledStatement(PropertyName* label, Node stmt,
                                           uint32_t begin) {
    return new_<LabeledStatement>(label, stmt, begin);
  }

  UnaryNodeType newThrowStatement(Node expr, const TokenPos& pos) {
    MOZ_ASSERT_IF(sourceKind() == SourceKind::Text, pos.encloses(expr->pn_pos));
    return new_<UnaryNode>(ParseNodeKind::ThrowStmt, pos, expr);
  }

  TernaryNodeType newTryStatement(uint32_t begin, Node body,
                                  LexicalScopeNodeType catchScope,
                                  Node finallyBlock) {
    return new_<TryNode>(begin, body, catchScope, finallyBlock);
  }

  DebuggerStatementType newDebuggerStatement(const TokenPos& pos) {
    return new_<DebuggerStatement>(pos);
  }

  NameNodeType newPropertyName(PropertyName* name, const TokenPos& pos) {
    return new_<NameNode>(ParseNodeKind::PropertyNameExpr, name, pos);
  }

  PropertyAccessType newPropertyAccess(Node expr, NameNodeType key) {
    return new_<PropertyAccess>(expr, key, expr->pn_pos.begin, key->pn_pos.end);
  }

  PropertyByValueType newPropertyByValue(Node lhs, Node index, uint32_t end) {
    return new_<PropertyByValue>(lhs, index, lhs->pn_pos.begin, end);
  }

  OptionalPropertyAccessType newOptionalPropertyAccess(Node expr,
                                                       NameNodeType key) {
    return new_<OptionalPropertyAccess>(expr, key, expr->pn_pos.begin,
                                        key->pn_pos.end);
  }

  OptionalPropertyByValueType newOptionalPropertyByValue(Node lhs, Node index,
                                                         uint32_t end) {
    return new_<OptionalPropertyByValue>(lhs, index, lhs->pn_pos.begin, end);
  }

  bool setupCatchScope(LexicalScopeNodeType lexicalScope, Node catchName,
                       Node catchBody) {
    BinaryNode* catchClause;
    if (catchName) {
      catchClause =
          new_<BinaryNode>(ParseNodeKind::Catch, catchName, catchBody);
    } else {
      catchClause = new_<BinaryNode>(ParseNodeKind::Catch, catchBody->pn_pos,
                                     catchName, catchBody);
    }
    if (!catchClause) {
      return false;
    }
    lexicalScope->setScopeBody(catchClause);
    return true;
  }

  inline MOZ_MUST_USE bool setLastFunctionFormalParameterDefault(
      FunctionNodeType funNode, Node defaultValue);

  void checkAndSetIsDirectRHSAnonFunction(Node pn) {
    if (IsAnonymousFunctionDefinition(pn)) {
      pn->setDirectRHSAnonFunction(true);
    }
  }

  FunctionNodeType newFunction(FunctionSyntaxKind syntaxKind,
                               const TokenPos& pos) {
    return new_<FunctionNode>(syntaxKind, pos);
  }

  BinaryNodeType newObjectMethodOrPropertyDefinition(Node key, Node value,
                                                     AccessorType atype) {
    MOZ_ASSERT(isUsableAsObjectPropertyName(key));

    return new_<PropertyDefinition>(key, value, atype);
  }

  BinaryNodeType newShorthandPropertyDefinition(Node key, Node value) {
    MOZ_ASSERT(isUsableAsObjectPropertyName(key));

    return newBinary(ParseNodeKind::Shorthand, key, value);
  }

  ListNodeType newParamsBody(const TokenPos& pos) {
    return new_<ListNode>(ParseNodeKind::ParamsBody, pos);
  }

  void setFunctionFormalParametersAndBody(FunctionNodeType funNode,
                                          ListNodeType paramsBody) {
    MOZ_ASSERT_IF(paramsBody, paramsBody->isKind(ParseNodeKind::ParamsBody));
    funNode->setBody(paramsBody);
  }
  void setFunctionBox(FunctionNodeType funNode, FunctionBox* funbox) {
    funNode->setFunbox(funbox);
    funbox->functionNode = funNode;
  }
  void addFunctionFormalParameter(FunctionNodeType funNode, Node argpn) {
    addList(/* list = */ funNode->body(), /* kid = */ argpn);
  }
  void setFunctionBody(FunctionNodeType funNode, LexicalScopeNodeType body) {
    MOZ_ASSERT(funNode->body()->isKind(ParseNodeKind::ParamsBody));
    addList(/* list = */ funNode->body(), /* kid = */ body);
  }

  ModuleNodeType newModule(const TokenPos& pos) {
    return new_<ModuleNode>(pos);
  }

  LexicalScopeNodeType newLexicalScope(LexicalScope::Data* bindings, Node body,
                                       ScopeKind kind = ScopeKind::Lexical) {
    return new_<LexicalScopeNode>(bindings, body, kind);
  }

  CallNodeType newNewExpression(uint32_t begin, Node ctor, Node args,
                                bool isSpread) {
    return new_<CallNode>(ParseNodeKind::NewExpr,
                          isSpread ? JSOp::SpreadNew : JSOp::New,
                          TokenPos(begin, args->pn_pos.end), ctor, args);
  }

  AssignmentNodeType newAssignment(ParseNodeKind kind, Node lhs, Node rhs) {
    if ((kind == ParseNodeKind::AssignExpr ||
         kind == ParseNodeKind::InitExpr) &&
        lhs->isKind(ParseNodeKind::Name) && !lhs->isInParens()) {
      checkAndSetIsDirectRHSAnonFunction(rhs);
    }

    return new_<AssignmentNode>(kind, lhs, rhs);
  }

  bool isUnparenthesizedAssignment(Node node) {
    if ((node->isKind(ParseNodeKind::AssignExpr)) && !node->isInParens()) {
      return true;
    }

    return false;
  }

  bool isUnparenthesizedUnaryExpression(Node node) {
    if (!node->isInParens()) {
      ParseNodeKind kind = node->getKind();
      return kind == ParseNodeKind::VoidExpr ||
             kind == ParseNodeKind::NotExpr ||
             kind == ParseNodeKind::BitNotExpr ||
             kind == ParseNodeKind::PosExpr || kind == ParseNodeKind::NegExpr ||
             IsTypeofKind(kind) || IsDeleteKind(kind);
    }
    return false;
  }

  bool isReturnStatement(Node node) {
    return node->isKind(ParseNodeKind::ReturnStmt);
  }

  bool isStatementPermittedAfterReturnStatement(Node node) {
    ParseNodeKind kind = node->getKind();
    return kind == ParseNodeKind::Function || kind == ParseNodeKind::VarStmt ||
           kind == ParseNodeKind::BreakStmt ||
           kind == ParseNodeKind::ThrowStmt || kind == ParseNodeKind::EmptyStmt;
  }

  bool isSuperBase(Node node) { return node->isKind(ParseNodeKind::SuperBase); }

  bool isUsableAsObjectPropertyName(Node node) {
    return node->isKind(ParseNodeKind::NumberExpr) ||
           node->isKind(ParseNodeKind::BigIntExpr) ||
           node->isKind(ParseNodeKind::ObjectPropertyName) ||
           node->isKind(ParseNodeKind::StringExpr) ||
           node->isKind(ParseNodeKind::ComputedName);
  }

  AssignmentNodeType finishInitializerAssignment(NameNodeType nameNode,
                                                 Node init) {
    MOZ_ASSERT(nameNode->isKind(ParseNodeKind::Name));
    MOZ_ASSERT(!nameNode->isInParens());

    checkAndSetIsDirectRHSAnonFunction(init);

    return newAssignment(ParseNodeKind::AssignExpr, nameNode, init);
  }

  void setBeginPosition(Node pn, Node oth) {
    setBeginPosition(pn, oth->pn_pos.begin);
  }
  void setBeginPosition(Node pn, uint32_t begin) {
    pn->pn_pos.begin = begin;
    MOZ_ASSERT_IF(sourceKind() == SourceKind::Text,
                  pn->pn_pos.begin <= pn->pn_pos.end);
  }

  void setEndPosition(Node pn, Node oth) {
    setEndPosition(pn, oth->pn_pos.end);
  }
  void setEndPosition(Node pn, uint32_t end) {
    pn->pn_pos.end = end;
    MOZ_ASSERT_IF(sourceKind() == SourceKind::Text,
                  pn->pn_pos.begin <= pn->pn_pos.end);
  }

  uint32_t getFunctionNameOffset(Node func, TokenStreamAnyChars& ts) {
    return func->pn_pos.begin;
  }

  bool isDeclarationKind(ParseNodeKind kind) {
    return kind == ParseNodeKind::VarStmt || kind == ParseNodeKind::LetDecl ||
           kind == ParseNodeKind::ConstDecl;
  }

  ListNodeType newList(ParseNodeKind kind, const TokenPos& pos) {
    MOZ_ASSERT(!isDeclarationKind(kind));
    return new_<ListNode>(kind, pos);
  }

 public:
  ListNodeType newList(ParseNodeKind kind, Node kid) {
    MOZ_ASSERT(!isDeclarationKind(kind));
    return new_<ListNode>(kind, kid);
  }

  ListNodeType newDeclarationList(ParseNodeKind kind, const TokenPos& pos) {
    MOZ_ASSERT(isDeclarationKind(kind));
    return new_<ListNode>(kind, pos);
  }

  bool isDeclarationList(Node node) {
    return isDeclarationKind(node->getKind());
  }

  Node singleBindingFromDeclaration(ListNodeType decl) {
    MOZ_ASSERT(isDeclarationList(decl));
    MOZ_ASSERT(decl->count() == 1);
    return decl->head();
  }

  ListNodeType newCommaExpressionList(Node kid) {
    return new_<ListNode>(ParseNodeKind::CommaExpr, kid);
  }

  void addList(ListNodeType list, Node kid) {
    if (sourceKind_ == SourceKind::Text) {
      list->append(kid);
    } else {
      list->appendWithoutOrderAssumption(kid);
    }
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

  bool isName(Node node) { return node->isKind(ParseNodeKind::Name); }

  bool isArgumentsName(Node node, JSContext* cx) {
    return node->isKind(ParseNodeKind::Name) &&
           node->as<NameNode>().atom() == cx->names().arguments;
  }

  bool isEvalName(Node node, JSContext* cx) {
    return node->isKind(ParseNodeKind::Name) &&
           node->as<NameNode>().atom() == cx->names().eval;
  }

  bool isAsyncKeyword(Node node, JSContext* cx) {
    return node->isKind(ParseNodeKind::Name) &&
           node->pn_pos.begin + strlen("async") == node->pn_pos.end &&
           node->as<NameNode>().atom() == cx->names().async;
  }

  PropertyName* maybeDottedProperty(Node pn) {
    return pn->is<PropertyAccessBase>() ? &pn->as<PropertyAccessBase>().name()
                                        : nullptr;
  }
  JSAtom* isStringExprStatement(Node pn, TokenPos* pos) {
    if (pn->is<UnaryNode>()) {
      UnaryNode* unary = &pn->as<UnaryNode>();
      if (JSAtom* atom = unary->isStringExprStatement()) {
        *pos = unary->kid()->pn_pos;
        return atom;
      }
    }
    return nullptr;
  }

  bool canSkipLazyInnerFunctions() { return !!lazyOuterFunction_; }
  bool canSkipLazyClosedOverBindings() { return !!lazyOuterFunction_; }
  bool canSkipRegexpSyntaxParse() { return !!lazyOuterFunction_; }
  JSFunction* nextLazyInnerFunction() {
    return &lazyOuterFunction_->gcthings()[lazyInnerFunctionIndex++]
                .as<JSObject>()
                .as<JSFunction>();
  }
  JSAtom* nextLazyClosedOverBinding() {
    auto gcthings = lazyOuterFunction_->gcthings();

    // Trailing nullptrs were elided in PerHandlerParser::finishFunction().
    if (lazyClosedOverBindingIndex >= gcthings.Length()) {
      return nullptr;
    }

    // These entries are either JSAtom* or nullptr, so use the 'asCell()'
    // accessor which is faster.
    gc::Cell* cell = gcthings[lazyClosedOverBindingIndex++].asCell();
    MOZ_ASSERT_IF(cell, cell->is<JSAtom>());
    return static_cast<JSAtom*>(cell);
  }
};

inline bool FullParseHandler::setLastFunctionFormalParameterDefault(
    FunctionNodeType funNode, Node defaultValue) {
  ListNode* body = funNode->body();
  ParseNode* arg = body->last();
  ParseNode* pn = newAssignment(ParseNodeKind::AssignExpr, arg, defaultValue);
  if (!pn) {
    return false;
  }

  body->replaceLast(pn);
  return true;
}

}  // namespace frontend
}  // namespace js

#endif /* frontend_FullParseHandler_h */

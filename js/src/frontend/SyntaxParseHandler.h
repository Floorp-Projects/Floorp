/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SyntaxParseHandler_h
#define frontend_SyntaxParseHandler_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <string.h>

#include "frontend/ParseNode.h"
#include "frontend/TokenStream.h"
#include "js/GCAnnotations.h"
#include "vm/JSContext.h"

namespace js {

namespace frontend {

// Parse handler used when processing the syntax in a block of code, to generate
// the minimal information which is required to detect syntax errors and allow
// bytecode to be emitted for outer functions.
//
// When parsing, we start at the top level with a full parse, and when possible
// only check the syntax for inner functions, so that they can be lazily parsed
// into bytecode when/if they first run. Checking the syntax of a function is
// several times faster than doing a full parse/emit, and lazy parsing improves
// both performance and memory usage significantly when pages contain large
// amounts of code that never executes (which happens often).
class SyntaxParseHandler {
  // Remember the last encountered name or string literal during syntax parses.
  JSAtom* lastAtom;
  TokenPos lastStringPos;

  // WARNING: Be careful about adding fields to this function, that might be
  //          GC things (like JSAtom*).  The JS_HAZ_ROOTED causes the GC
  //          analysis to *ignore* anything that might be a rooting hazard in
  //          this class.  The |lastAtom| field above is safe because
  //          SyntaxParseHandler only appears as a field in
  //          PerHandlerParser<SyntaxParseHandler>, and that class inherits
  //          from ParserBase which contains an AutoKeepAtoms field that
  //          prevents atoms from being moved around while the AutoKeepAtoms
  //          lives -- which is as long as ParserBase lives, which is longer
  //          than the PerHandlerParser<SyntaxParseHandler> that inherits
  //          from it will live.

 public:
  enum Node {
    NodeFailure = 0,
    NodeGeneric,
    NodeGetProp,
    NodeStringExprStatement,
    NodeReturn,
    NodeBreak,
    NodeThrow,
    NodeEmptyStatement,

    NodeVarDeclaration,
    NodeLexicalDeclaration,

    // A non-arrow function expression with block body, from bog-standard
    // ECMAScript.
    NodeFunctionExpression,

    NodeFunctionArrow,
    NodeFunctionStatement,

    // This is needed for proper assignment-target handling.  ES6 formally
    // requires function calls *not* pass IsValidSimpleAssignmentTarget,
    // but at last check there were still sites with |f() = 5| and similar
    // in code not actually executed (or at least not executed enough to be
    // noticed).
    NodeFunctionCall,

    // Node representing normal names which don't require any special
    // casing.
    NodeName,

    // Nodes representing the names "arguments" and "eval".
    NodeArgumentsName,
    NodeEvalName,

    // Node representing the "async" name, which may actually be a
    // contextual keyword.
    NodePotentialAsyncKeyword,

    NodeDottedProperty,
    NodeElement,

    // Destructuring target patterns can't be parenthesized: |([a]) = [3];|
    // must be a syntax error.  (We can't use NodeGeneric instead of these
    // because that would trigger invalid-left-hand-side ReferenceError
    // semantics when SyntaxError semantics are desired.)
    NodeParenthesizedArray,
    NodeParenthesizedObject,

    // In rare cases a parenthesized |node| doesn't have the same semantics
    // as |node|.  Each such node has a special Node value, and we use a
    // different Node value to represent the parenthesized form.  See also
    // is{Unp,P}arenthesized*(Node), parenthesize(Node), and the various
    // functions that deal in NodeUnparenthesized* below.

    // Valuable for recognizing potential destructuring patterns.
    NodeUnparenthesizedArray,
    NodeUnparenthesizedObject,

    // The directive prologue at the start of a FunctionBody or ScriptBody
    // is the longest sequence (possibly empty) of string literal
    // expression statements at the start of a function.  Thus we need this
    // to treat |"use strict";| as a possible Use Strict Directive and
    // |("use strict");| as a useless statement.
    NodeUnparenthesizedString,

    // Assignment expressions in condition contexts could be typos for
    // equality checks.  (Think |if (x = y)| versus |if (x == y)|.)  Thus
    // we need this to treat |if (x = y)| as a possible typo and
    // |if ((x = y))| as a deliberate assignment within a condition.
    //
    // (Technically this isn't needed, as these are *only* extraWarnings
    // warnings, and parsing with that option disables syntax parsing.  But
    // it seems best to be consistent, and perhaps the syntax parser will
    // eventually enforce extraWarnings and will require this then.)
    NodeUnparenthesizedAssignment,

    // This node is necessary to determine if the base operand in an
    // exponentiation operation is an unparenthesized unary expression.
    // We want to reject |-2 ** 3|, but still need to allow |(-2) ** 3|.
    NodeUnparenthesizedUnary,

    // This node is necessary to determine if the LHS of a property access is
    // super related.
    NodeSuperBase
  };

#define DECLARE_TYPE(typeName, longTypeName, asMethodName) \
  using longTypeName = Node;
  FOR_EACH_PARSENODE_SUBCLASS(DECLARE_TYPE)
#undef DECLARE_TYPE

  using NullNode = Node;

  bool isNonArrowFunctionExpression(Node node) const {
    return node == NodeFunctionExpression;
  }

  bool isPropertyAccess(Node node) {
    return node == NodeDottedProperty || node == NodeElement;
  }

  bool isFunctionCall(Node node) {
    // Note: super() is a special form, *not* a function call.
    return node == NodeFunctionCall;
  }

  static bool isUnparenthesizedDestructuringPattern(Node node) {
    return node == NodeUnparenthesizedArray ||
           node == NodeUnparenthesizedObject;
  }

  static bool isParenthesizedDestructuringPattern(Node node) {
    // Technically this isn't a destructuring target at all -- the grammar
    // doesn't treat it as such.  But we need to know when this happens to
    // consider it a SyntaxError rather than an invalid-left-hand-side
    // ReferenceError.
    return node == NodeParenthesizedArray || node == NodeParenthesizedObject;
  }

 public:
  SyntaxParseHandler(JSContext* cx, LifoAlloc& alloc,
                     LazyScript* lazyOuterFunction)
      : lastAtom(nullptr) {}

  static NullNode null() { return NodeFailure; }

#define DECLARE_AS(typeName, longTypeName, asMethodName) \
  static longTypeName asMethodName(Node node) { return node; }
  FOR_EACH_PARSENODE_SUBCLASS(DECLARE_AS)
#undef DECLARE_AS

  NameNodeType newName(PropertyName* name, const TokenPos& pos, JSContext* cx) {
    lastAtom = name;
    if (name == cx->names().arguments) {
      return NodeArgumentsName;
    }
    if (pos.begin + strlen("async") == pos.end && name == cx->names().async) {
      return NodePotentialAsyncKeyword;
    }
    if (name == cx->names().eval) {
      return NodeEvalName;
    }
    return NodeName;
  }

  UnaryNodeType newComputedName(Node expr, uint32_t start, uint32_t end) {
    return NodeGeneric;
  }

  NameNodeType newObjectLiteralPropertyName(JSAtom* atom, const TokenPos& pos) {
    return NodeName;
  }

  NumericLiteralType newNumber(double value, DecimalPoint decimalPoint,
                               const TokenPos& pos) {
    return NodeGeneric;
  }

  BigIntLiteralType newBigInt() { return NodeGeneric; }

  BooleanLiteralType newBooleanLiteral(bool cond, const TokenPos& pos) {
    return NodeGeneric;
  }

  NameNodeType newStringLiteral(JSAtom* atom, const TokenPos& pos) {
    lastAtom = atom;
    lastStringPos = pos;
    return NodeUnparenthesizedString;
  }

  NameNodeType newTemplateStringLiteral(JSAtom* atom, const TokenPos& pos) {
    return NodeGeneric;
  }

  CallSiteNodeType newCallSiteObject(uint32_t begin) { return NodeGeneric; }

  void addToCallSiteObject(CallSiteNodeType callSiteObj, Node rawNode,
                           Node cookedNode) {}

  ThisLiteralType newThisLiteral(const TokenPos& pos, Node thisName) {
    return NodeGeneric;
  }
  NullLiteralType newNullLiteral(const TokenPos& pos) { return NodeGeneric; }
  RawUndefinedLiteralType newRawUndefinedLiteral(const TokenPos& pos) {
    return NodeGeneric;
  }

  template <class Boxer>
  RegExpLiteralType newRegExp(Node reobj, const TokenPos& pos, Boxer& boxer) {
    return NodeGeneric;
  }

  ConditionalExpressionType newConditional(Node cond, Node thenExpr,
                                           Node elseExpr) {
    return NodeGeneric;
  }

  Node newElision() { return NodeGeneric; }

  UnaryNodeType newDelete(uint32_t begin, Node expr) {
    return NodeUnparenthesizedUnary;
  }

  UnaryNodeType newTypeof(uint32_t begin, Node kid) {
    return NodeUnparenthesizedUnary;
  }

  UnaryNodeType newUnary(ParseNodeKind kind, uint32_t begin, Node kid) {
    return NodeUnparenthesizedUnary;
  }

  UnaryNodeType newUpdate(ParseNodeKind kind, uint32_t begin, Node kid) {
    return NodeGeneric;
  }

  UnaryNodeType newSpread(uint32_t begin, Node kid) { return NodeGeneric; }

  Node appendOrCreateList(ParseNodeKind kind, Node left, Node right,
                          ParseContext* pc) {
    return NodeGeneric;
  }

  // Expressions

  ListNodeType newArrayLiteral(uint32_t begin) {
    return NodeUnparenthesizedArray;
  }
  MOZ_MUST_USE bool addElision(ListNodeType literal, const TokenPos& pos) {
    return true;
  }
  MOZ_MUST_USE bool addSpreadElement(ListNodeType literal, uint32_t begin,
                                     Node inner) {
    return true;
  }
  void addArrayElement(ListNodeType literal, Node element) {}

  ListNodeType newArguments(const TokenPos& pos) { return NodeGeneric; }
  CallNodeType newCall(Node callee, Node args, JSOp callOp) {
    return NodeFunctionCall;
  }

  CallNodeType newSuperCall(Node callee, Node args, bool isSpread) {
    return NodeGeneric;
  }
  CallNodeType newTaggedTemplate(Node tag, Node args, JSOp callOp) {
    return NodeGeneric;
  }

  ListNodeType newObjectLiteral(uint32_t begin) {
    return NodeUnparenthesizedObject;
  }
  ListNodeType newClassMemberList(uint32_t begin) { return NodeGeneric; }
  ClassNamesType newClassNames(Node outer, Node inner, const TokenPos& pos) {
    return NodeGeneric;
  }
  ClassNodeType newClass(Node name, Node heritage, Node methodBlock,
                         const TokenPos& pos) {
    return NodeGeneric;
  }

  LexicalScopeNodeType newLexicalScope(Node body) {
    return NodeLexicalDeclaration;
  }

  BinaryNodeType newNewTarget(NullaryNodeType newHolder,
                              NullaryNodeType targetHolder) {
    return NodeGeneric;
  }
  NullaryNodeType newPosHolder(const TokenPos& pos) { return NodeGeneric; }
  UnaryNodeType newSuperBase(Node thisName, const TokenPos& pos) {
    return NodeSuperBase;
  }

  MOZ_MUST_USE bool addPrototypeMutation(ListNodeType literal, uint32_t begin,
                                         Node expr) {
    return true;
  }
  BinaryNodeType newPropertyDefinition(Node key, Node val) {
    return NodeGeneric;
  }
  void addPropertyDefinition(ListNodeType literal, BinaryNodeType propdef) {}
  MOZ_MUST_USE bool addPropertyDefinition(ListNodeType literal, Node key,
                                          Node expr) {
    return true;
  }
  MOZ_MUST_USE bool addShorthand(ListNodeType literal, NameNodeType name,
                                 NameNodeType expr) {
    return true;
  }
  MOZ_MUST_USE bool addSpreadProperty(ListNodeType literal, uint32_t begin,
                                      Node inner) {
    return true;
  }
  MOZ_MUST_USE bool addObjectMethodDefinition(ListNodeType literal, Node key,
                                              FunctionNodeType funNode,
                                              AccessorType atype) {
    return true;
  }
  MOZ_MUST_USE bool addClassMethodDefinition(ListNodeType memberList, Node key,
                                             FunctionNodeType funNode,
                                             AccessorType atype,
                                             bool isStatic) {
    return true;
  }
  MOZ_MUST_USE bool addClassFieldDefinition(ListNodeType memberList, Node name,
                                            FunctionNodeType initializer) {
    return true;
  }
  UnaryNodeType newYieldExpression(uint32_t begin, Node value) {
    return NodeGeneric;
  }
  UnaryNodeType newYieldStarExpression(uint32_t begin, Node value) {
    return NodeGeneric;
  }
  UnaryNodeType newAwaitExpression(uint32_t begin, Node value) {
    return NodeGeneric;
  }

  // Statements

  ListNodeType newStatementList(const TokenPos& pos) { return NodeGeneric; }
  void addStatementToList(ListNodeType list, Node stmt) {}
  void setListEndPosition(ListNodeType list, const TokenPos& pos) {}
  void addCaseStatementToList(ListNodeType list, CaseClauseType caseClause) {}
  MOZ_MUST_USE bool prependInitialYield(ListNodeType stmtList, Node genName) {
    return true;
  }
  NullaryNodeType newEmptyStatement(const TokenPos& pos) {
    return NodeEmptyStatement;
  }

  UnaryNodeType newExportDeclaration(Node kid, const TokenPos& pos) {
    return NodeGeneric;
  }
  BinaryNodeType newExportFromDeclaration(uint32_t begin, Node exportSpecSet,
                                          Node moduleSpec) {
    return NodeGeneric;
  }
  BinaryNodeType newExportDefaultDeclaration(Node kid, Node maybeBinding,
                                             const TokenPos& pos) {
    return NodeGeneric;
  }
  BinaryNodeType newExportSpec(Node bindingName, Node exportName) {
    return NodeGeneric;
  }
  NullaryNodeType newExportBatchSpec(const TokenPos& pos) {
    return NodeGeneric;
  }
  BinaryNodeType newImportMeta(NullaryNodeType importHolder,
                               NullaryNodeType metaHolder) {
    return NodeGeneric;
  }
  BinaryNodeType newCallImport(NullaryNodeType importHolder, Node singleArg) {
    return NodeGeneric;
  }

  BinaryNodeType newSetThis(Node thisName, Node value) { return value; }

  UnaryNodeType newExprStatement(Node expr, uint32_t end) {
    return expr == NodeUnparenthesizedString ? NodeStringExprStatement
                                             : NodeGeneric;
  }

  TernaryNodeType newIfStatement(uint32_t begin, Node cond, Node thenBranch,
                                 Node elseBranch) {
    return NodeGeneric;
  }
  BinaryNodeType newDoWhileStatement(Node body, Node cond,
                                     const TokenPos& pos) {
    return NodeGeneric;
  }
  BinaryNodeType newWhileStatement(uint32_t begin, Node cond, Node body) {
    return NodeGeneric;
  }
  SwitchStatementType newSwitchStatement(
      uint32_t begin, Node discriminant,
      LexicalScopeNodeType lexicalForCaseList, bool hasDefault) {
    return NodeGeneric;
  }
  CaseClauseType newCaseOrDefault(uint32_t begin, Node expr, Node body) {
    return NodeGeneric;
  }
  ContinueStatementType newContinueStatement(PropertyName* label,
                                             const TokenPos& pos) {
    return NodeGeneric;
  }
  BreakStatementType newBreakStatement(PropertyName* label,
                                       const TokenPos& pos) {
    return NodeBreak;
  }
  UnaryNodeType newReturnStatement(Node expr, const TokenPos& pos) {
    return NodeReturn;
  }
  UnaryNodeType newExpressionBody(Node expr) { return NodeReturn; }
  BinaryNodeType newWithStatement(uint32_t begin, Node expr, Node body) {
    return NodeGeneric;
  }

  LabeledStatementType newLabeledStatement(PropertyName* label, Node stmt,
                                           uint32_t begin) {
    return NodeGeneric;
  }

  UnaryNodeType newThrowStatement(Node expr, const TokenPos& pos) {
    return NodeThrow;
  }
  TernaryNodeType newTryStatement(uint32_t begin, Node body,
                                  LexicalScopeNodeType catchScope,
                                  Node finallyBlock) {
    return NodeGeneric;
  }
  DebuggerStatementType newDebuggerStatement(const TokenPos& pos) {
    return NodeGeneric;
  }

  NameNodeType newPropertyName(PropertyName* name, const TokenPos& pos) {
    lastAtom = name;
    return NodeGeneric;
  }

  PropertyAccessType newPropertyAccess(Node expr, NameNodeType key) {
    return NodeDottedProperty;
  }

  PropertyByValueType newPropertyByValue(Node lhs, Node index, uint32_t end) {
    return NodeElement;
  }

  MOZ_MUST_USE bool setupCatchScope(LexicalScopeNodeType lexicalScope,
                                    Node catchName, Node catchBody) {
    return true;
  }

  MOZ_MUST_USE bool setLastFunctionFormalParameterDefault(
      FunctionNodeType funNode, Node defaultValue) {
    return true;
  }

  FunctionNodeType newFunction(FunctionSyntaxKind syntaxKind,
                               const TokenPos& pos) {
    switch (syntaxKind) {
      case FunctionSyntaxKind::Statement:
        return NodeFunctionStatement;
      case FunctionSyntaxKind::Arrow:
        return NodeFunctionArrow;
      default:
        // All non-arrow function expressions are initially presumed to have
        // block body.  This will be overridden later *if* the function
        // expression permissibly has an AssignmentExpression body.
        return NodeFunctionExpression;
    }
  }

  void setFunctionFormalParametersAndBody(FunctionNodeType funNode,
                                          ListNodeType paramsBody) {}
  void setFunctionBody(FunctionNodeType funNode, LexicalScopeNodeType body) {}
  void setFunctionBox(FunctionNodeType funNode, FunctionBox* funbox) {}
  void addFunctionFormalParameter(FunctionNodeType funNode, Node argpn) {}

  ForNodeType newForStatement(uint32_t begin, TernaryNodeType forHead,
                              Node body, unsigned iflags) {
    return NodeGeneric;
  }

  TernaryNodeType newForHead(Node init, Node test, Node update,
                             const TokenPos& pos) {
    return NodeGeneric;
  }

  TernaryNodeType newForInOrOfHead(ParseNodeKind kind, Node target,
                                   Node iteratedExpr, const TokenPos& pos) {
    return NodeGeneric;
  }

  AssignmentNodeType finishInitializerAssignment(NameNodeType nameNode,
                                                 Node init) {
    return NodeUnparenthesizedAssignment;
  }

  void setBeginPosition(Node pn, Node oth) {}
  void setBeginPosition(Node pn, uint32_t begin) {}

  void setEndPosition(Node pn, Node oth) {}
  void setEndPosition(Node pn, uint32_t end) {}

  uint32_t getFunctionNameOffset(Node func, TokenStreamAnyChars& ts) {
    // XXX This offset isn't relevant to the offending function name.  But
    //     we may not *have* that function name around, because of how lazy
    //     parsing works -- the actual name could be outside
    //     |tokenStream.userbuf|'s observed region.  So the current offset
    //     is the best we can do.
    return ts.currentToken().pos.begin;
  }

  ListNodeType newList(ParseNodeKind kind, const TokenPos& pos) {
    MOZ_ASSERT(kind != ParseNodeKind::VarStmt);
    MOZ_ASSERT(kind != ParseNodeKind::LetDecl);
    MOZ_ASSERT(kind != ParseNodeKind::ConstDecl);
    return NodeGeneric;
  }

  ListNodeType newList(ParseNodeKind kind, Node kid) {
    return newList(kind, TokenPos());
  }

  ListNodeType newDeclarationList(ParseNodeKind kind, const TokenPos& pos) {
    if (kind == ParseNodeKind::VarStmt) {
      return NodeVarDeclaration;
    }
    MOZ_ASSERT(kind == ParseNodeKind::LetDecl ||
               kind == ParseNodeKind::ConstDecl);
    return NodeLexicalDeclaration;
  }

  bool isDeclarationList(Node node) {
    return node == NodeVarDeclaration || node == NodeLexicalDeclaration;
  }

  // This method should only be called from parsers using FullParseHandler.
  Node singleBindingFromDeclaration(ListNodeType decl) = delete;

  ListNodeType newCommaExpressionList(Node kid) { return NodeGeneric; }

  void addList(ListNodeType list, Node kid) {
    MOZ_ASSERT(list == NodeGeneric || list == NodeUnparenthesizedArray ||
               list == NodeUnparenthesizedObject ||
               list == NodeVarDeclaration || list == NodeLexicalDeclaration ||
               list == NodeFunctionCall);
  }

  CallNodeType newNewExpression(uint32_t begin, Node ctor, Node args,
                                bool isSpread) {
    return NodeGeneric;
  }

  AssignmentNodeType newAssignment(ParseNodeKind kind, Node lhs, Node rhs) {
    return kind == ParseNodeKind::AssignExpr ? NodeUnparenthesizedAssignment
                                             : NodeGeneric;
  }

  bool isUnparenthesizedAssignment(Node node) {
    return node == NodeUnparenthesizedAssignment;
  }

  bool isUnparenthesizedUnaryExpression(Node node) {
    return node == NodeUnparenthesizedUnary;
  }

  bool isReturnStatement(Node node) { return node == NodeReturn; }

  bool isStatementPermittedAfterReturnStatement(Node pn) {
    return pn == NodeFunctionStatement || isNonArrowFunctionExpression(pn) ||
           pn == NodeVarDeclaration || pn == NodeBreak || pn == NodeThrow ||
           pn == NodeEmptyStatement;
  }

  bool isSuperBase(Node pn) { return pn == NodeSuperBase; }

  void setListHasNonConstInitializer(ListNodeType literal) {}
  MOZ_MUST_USE Node parenthesize(Node node) {
    // A number of nodes have different behavior upon parenthesization, but
    // only in some circumstances.  Convert these nodes to special
    // parenthesized forms.
    if (node == NodeUnparenthesizedArray) {
      return NodeParenthesizedArray;
    }
    if (node == NodeUnparenthesizedObject) {
      return NodeParenthesizedObject;
    }

    // Other nodes need not be recognizable after parenthesization; convert
    // them to a generic node.
    if (node == NodeUnparenthesizedString ||
        node == NodeUnparenthesizedAssignment ||
        node == NodeUnparenthesizedUnary) {
      return NodeGeneric;
    }

    // Convert parenthesized |async| to a normal name node.
    if (node == NodePotentialAsyncKeyword) {
      return NodeName;
    }

    // In all other cases, the parenthesized form of |node| is equivalent
    // to the unparenthesized form: return |node| unchanged.
    return node;
  }
  template <typename NodeType>
  MOZ_MUST_USE NodeType setLikelyIIFE(NodeType node) {
    return node;  // Remain in syntax-parse mode.
  }
  void setInDirectivePrologue(UnaryNodeType exprStmt) {}

  bool isName(Node node) {
    return node == NodeName || node == NodeArgumentsName ||
           node == NodeEvalName || node == NodePotentialAsyncKeyword;
  }

  bool isArgumentsName(Node node, JSContext* cx) {
    return node == NodeArgumentsName;
  }

  bool isEvalName(Node node, JSContext* cx) { return node == NodeEvalName; }

  bool isAsyncKeyword(Node node, JSContext* cx) {
    return node == NodePotentialAsyncKeyword;
  }

  PropertyName* maybeDottedProperty(Node node) {
    // Note: |super.apply(...)| is a special form that calls an "apply"
    // method retrieved from one value, but using a *different* value as
    // |this|.  It's not really eligible for the funapply/funcall
    // optimizations as they're currently implemented (assuming a single
    // value is used for both retrieval and |this|).
    if (node != NodeDottedProperty) {
      return nullptr;
    }
    return lastAtom->asPropertyName();
  }

  JSAtom* isStringExprStatement(Node pn, TokenPos* pos) {
    if (pn == NodeStringExprStatement) {
      *pos = lastStringPos;
      return lastAtom;
    }
    return nullptr;
  }

  bool canSkipLazyInnerFunctions() { return false; }
  bool canSkipLazyClosedOverBindings() { return false; }
  JSAtom* nextLazyClosedOverBinding() {
    MOZ_CRASH(
        "SyntaxParseHandler::canSkipLazyClosedOverBindings must return false");
  }
} JS_HAZ_ROOTED;  // See the top of SyntaxParseHandler for why this is safe.

}  // namespace frontend
}  // namespace js

#endif /* frontend_SyntaxParseHandler_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SyntaxParseHandler_h
#define frontend_SyntaxParseHandler_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <string.h>

#include "jscntxt.h"

#include "frontend/ParseNode.h"

namespace js {

namespace frontend {

template <template <typename CharT> class ParseHandler, typename CharT>
class Parser;

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
class SyntaxParseHandlerBase
{
    // Remember the last encountered name or string literal during syntax parses.
    JSAtom* lastAtom;
    TokenPos lastStringPos;

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

        NodeFunctionDefinition,

        // This is needed for proper assignment-target handling.  ES6 formally
        // requires function calls *not* pass IsValidSimpleAssignmentTarget,
        // but at last check there were still sites with |f() = 5| and similar
        // in code not actually executed (or at least not executed enough to be
        // noticed).
        NodeFunctionCall,

        // Nodes representing *parenthesized* IsValidSimpleAssignmentTarget
        // nodes.  We can't simply treat all such parenthesized nodes
        // identically, because in assignment and increment/decrement contexts
        // ES6 says that parentheses constitute a syntax error.
        //
        //   var obj = {};
        //   var val;
        //   (val) = 3; (obj.prop) = 4;       // okay per ES5's little mind
        //   [(a)] = [3]; [(obj.prop)] = [4]; // invalid ES6 syntax
        //   // ...and so on for the other IsValidSimpleAssignmentTarget nodes
        //
        // We don't know in advance in the current parser when we're parsing
        // in a place where name parenthesization changes meaning, so we must
        // have multiple node values for these cases.
        NodeParenthesizedArgumentsName,
        NodeParenthesizedEvalName,
        NodeParenthesizedName,

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

        // Nodes representing unparenthesized names.
        NodeUnparenthesizedArgumentsName,
        NodeUnparenthesizedEvalName,
        NodeUnparenthesizedName,

        // Node representing the "async" name, which may actually be a
        // contextual keyword.
        NodePotentialAsyncKeyword,

        // Valuable for recognizing potential destructuring patterns.
        NodeUnparenthesizedArray,
        NodeUnparenthesizedObject,

        // The directive prologue at the start of a FunctionBody or ScriptBody
        // is the longest sequence (possibly empty) of string literal
        // expression statements at the start of a function.  Thus we need this
        // to treat |"use strict";| as a possible Use Strict Directive and
        // |("use strict");| as a useless statement.
        NodeUnparenthesizedString,

        // Legacy generator expressions of the form |(expr for (...))| and
        // array comprehensions of the form |[expr for (...)]|) don't permit
        // |expr| to be a comma expression.  Thus we need this to treat
        // |(a(), b for (x in []))| as a syntax error and
        // |((a(), b) for (x in []))| as a generator that calls |a| and then
        // yields |b| each time it's resumed.
        NodeUnparenthesizedCommaExpr,

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

    bool isPropertyAccess(Node node) {
        return node == NodeDottedProperty || node == NodeElement;
    }

    bool isFunctionCall(Node node) {
        // Note: super() is a special form, *not* a function call.
        return node == NodeFunctionCall;
    }

    static bool isUnparenthesizedDestructuringPattern(Node node) {
        return node == NodeUnparenthesizedArray || node == NodeUnparenthesizedObject;
    }

    static bool isParenthesizedDestructuringPattern(Node node) {
        // Technically this isn't a destructuring target at all -- the grammar
        // doesn't treat it as such.  But we need to know when this happens to
        // consider it a SyntaxError rather than an invalid-left-hand-side
        // ReferenceError.
        return node == NodeParenthesizedArray || node == NodeParenthesizedObject;
    }

    static bool isDestructuringPatternAnyParentheses(Node node) {
        return isUnparenthesizedDestructuringPattern(node) ||
                isParenthesizedDestructuringPattern(node);
    }

  public:
    SyntaxParseHandlerBase(JSContext* cx, LifoAlloc& alloc, LazyScript* lazyOuterFunction)
      : lastAtom(nullptr)
    {}

    static Node null() { return NodeFailure; }

    void prepareNodeForMutation(Node node) {}
    void freeTree(Node node) {}

    void trace(JSTracer* trc) {}

    Node newName(PropertyName* name, const TokenPos& pos, JSContext* cx) {
        lastAtom = name;
        if (name == cx->names().arguments)
            return NodeUnparenthesizedArgumentsName;
        if (pos.begin + strlen("async") == pos.end && name == cx->names().async)
            return NodePotentialAsyncKeyword;
        if (name == cx->names().eval)
            return NodeUnparenthesizedEvalName;
        return NodeUnparenthesizedName;
    }

    Node newComputedName(Node expr, uint32_t start, uint32_t end) {
        return NodeGeneric;
    }

    Node newObjectLiteralPropertyName(JSAtom* atom, const TokenPos& pos) {
        return NodeUnparenthesizedName;
    }

    Node newNumber(double value, DecimalPoint decimalPoint, const TokenPos& pos) { return NodeGeneric; }
    Node newBooleanLiteral(bool cond, const TokenPos& pos) { return NodeGeneric; }

    Node newStringLiteral(JSAtom* atom, const TokenPos& pos) {
        lastAtom = atom;
        lastStringPos = pos;
        return NodeUnparenthesizedString;
    }

    Node newTemplateStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return NodeGeneric;
    }

    Node newCallSiteObject(uint32_t begin) {
        return NodeGeneric;
    }

    void addToCallSiteObject(Node callSiteObj, Node rawNode, Node cookedNode) {}

    Node newThisLiteral(const TokenPos& pos, Node thisName) { return NodeGeneric; }
    Node newNullLiteral(const TokenPos& pos) { return NodeGeneric; }
    Node newRawUndefinedLiteral(const TokenPos& pos) { return NodeGeneric; }

    template <class Boxer>
    Node newRegExp(Node reobj, const TokenPos& pos, Boxer& boxer) { return NodeGeneric; }

    Node newConditional(Node cond, Node thenExpr, Node elseExpr) { return NodeGeneric; }

    Node newElision() { return NodeGeneric; }

    Node newDelete(uint32_t begin, Node expr) {
        return NodeUnparenthesizedUnary;
    }

    Node newTypeof(uint32_t begin, Node kid) {
        return NodeUnparenthesizedUnary;
    }

    Node newNullary(ParseNodeKind kind, JSOp op, const TokenPos& pos) {
        return NodeGeneric;
    }

    Node newUnary(ParseNodeKind kind, JSOp op, uint32_t begin, Node kid) {
        return NodeUnparenthesizedUnary;
    }

    Node newUpdate(ParseNodeKind kind, uint32_t begin, Node kid) {
        return NodeGeneric;
    }

    Node newSpread(uint32_t begin, Node kid) {
        return NodeGeneric;
    }

    Node newArrayPush(uint32_t begin, Node kid) {
        return NodeGeneric;
    }

    Node newBinary(ParseNodeKind kind, Node left, Node right, JSOp op = JSOP_NOP) {
        return NodeGeneric;
    }
    Node appendOrCreateList(ParseNodeKind kind, Node left, Node right,
                            ParseContext* pc, JSOp op = JSOP_NOP) {
        return NodeGeneric;
    }

    Node newTernary(ParseNodeKind kind, Node first, Node second, Node third, JSOp op = JSOP_NOP) {
        return NodeGeneric;
    }

    // Expressions

    Node newArrayComprehension(Node body, const TokenPos& pos) { return NodeGeneric; }
    Node newArrayLiteral(uint32_t begin) { return NodeUnparenthesizedArray; }
    MOZ_MUST_USE bool addElision(Node literal, const TokenPos& pos) { return true; }
    MOZ_MUST_USE bool addSpreadElement(Node literal, uint32_t begin, Node inner) { return true; }
    void addArrayElement(Node literal, Node element) { }

    Node newCall(const TokenPos& pos) { return NodeFunctionCall; }
    Node newTaggedTemplate(const TokenPos& pos) { return NodeGeneric; }

    Node newObjectLiteral(uint32_t begin) { return NodeUnparenthesizedObject; }
    Node newClassMethodList(uint32_t begin) { return NodeGeneric; }
    Node newClassNames(Node outer, Node inner, const TokenPos& pos) { return NodeGeneric; }
    Node newClass(Node name, Node heritage, Node methodBlock, const TokenPos& pos) { return NodeGeneric; }

    Node newNewTarget(Node newHolder, Node targetHolder) { return NodeGeneric; }
    Node newPosHolder(const TokenPos& pos) { return NodeGeneric; }
    Node newSuperBase(Node thisName, const TokenPos& pos) { return NodeSuperBase; }

    MOZ_MUST_USE bool addPrototypeMutation(Node literal, uint32_t begin, Node expr) { return true; }
    MOZ_MUST_USE bool addPropertyDefinition(Node literal, Node name, Node expr) { return true; }
    MOZ_MUST_USE bool addShorthand(Node literal, Node name, Node expr) { return true; }
    MOZ_MUST_USE bool addSpreadProperty(Node literal, uint32_t begin, Node inner) { return true; }
    MOZ_MUST_USE bool addObjectMethodDefinition(Node literal, Node name, Node fn, JSOp op) { return true; }
    MOZ_MUST_USE bool addClassMethodDefinition(Node literal, Node name, Node fn, JSOp op, bool isStatic) { return true; }
    Node newYieldExpression(uint32_t begin, Node value) { return NodeGeneric; }
    Node newYieldStarExpression(uint32_t begin, Node value) { return NodeGeneric; }
    Node newAwaitExpression(uint32_t begin, Node value) { return NodeGeneric; }

    // Statements

    Node newStatementList(const TokenPos& pos) { return NodeGeneric; }
    void addStatementToList(Node list, Node stmt) {}
    void addCaseStatementToList(Node list, Node stmt) {}
    MOZ_MUST_USE bool prependInitialYield(Node stmtList, Node gen) { return true; }
    Node newEmptyStatement(const TokenPos& pos) { return NodeEmptyStatement; }

    Node newExportDeclaration(Node kid, const TokenPos& pos) {
        return NodeGeneric;
    }
    Node newExportFromDeclaration(uint32_t begin, Node exportSpecSet, Node moduleSpec) {
        return NodeGeneric;
    }
    Node newExportDefaultDeclaration(Node kid, Node maybeBinding, const TokenPos& pos) {
        return NodeGeneric;
    }

    Node newSetThis(Node thisName, Node value) { return value; }

    Node newExprStatement(Node expr, uint32_t end) {
        return expr == NodeUnparenthesizedString ? NodeStringExprStatement : NodeGeneric;
    }

    Node newIfStatement(uint32_t begin, Node cond, Node then, Node else_) { return NodeGeneric; }
    Node newDoWhileStatement(Node body, Node cond, const TokenPos& pos) { return NodeGeneric; }
    Node newWhileStatement(uint32_t begin, Node cond, Node body) { return NodeGeneric; }
    Node newSwitchStatement(uint32_t begin, Node discriminant, Node caseList) { return NodeGeneric; }
    Node newCaseOrDefault(uint32_t begin, Node expr, Node body) { return NodeGeneric; }
    Node newContinueStatement(PropertyName* label, const TokenPos& pos) { return NodeGeneric; }
    Node newBreakStatement(PropertyName* label, const TokenPos& pos) { return NodeBreak; }
    Node newReturnStatement(Node expr, const TokenPos& pos) { return NodeReturn; }
    Node newExpressionBody(Node expr) { return NodeReturn; }
    Node newWithStatement(uint32_t begin, Node expr, Node body) { return NodeGeneric; }

    Node newLabeledStatement(PropertyName* label, Node stmt, uint32_t begin) {
        return NodeGeneric;
    }

    Node newThrowStatement(Node expr, const TokenPos& pos) { return NodeThrow; }
    Node newTryStatement(uint32_t begin, Node body, Node catchList, Node finallyBlock) {
        return NodeGeneric;
    }
    Node newDebuggerStatement(const TokenPos& pos) { return NodeGeneric; }

    Node newPropertyAccess(Node pn, PropertyName* name, uint32_t end) {
        lastAtom = name;
        return NodeDottedProperty;
    }

    Node newPropertyByValue(Node pn, Node kid, uint32_t end) { return NodeElement; }

    MOZ_MUST_USE bool addCatchBlock(Node catchList, Node letBlock, Node catchName,
                                    Node catchGuard, Node catchBody) { return true; }

    MOZ_MUST_USE bool setLastFunctionFormalParameterDefault(Node funcpn, Node pn) { return true; }

    void checkAndSetIsDirectRHSAnonFunction(Node pn) {}

    Node newFunctionStatement(const TokenPos& pos) { return NodeFunctionDefinition; }
    Node newFunctionExpression(const TokenPos& pos) { return NodeFunctionDefinition; }
    Node newArrowFunction(const TokenPos& pos) { return NodeFunctionDefinition; }

    bool setComprehensionLambdaBody(Node pn, Node body) { return true; }
    void setFunctionFormalParametersAndBody(Node pn, Node kid) {}
    void setFunctionBody(Node pn, Node kid) {}
    void setFunctionBox(Node pn, FunctionBox* funbox) {}
    void addFunctionFormalParameter(Node pn, Node argpn) {}

    Node newForStatement(uint32_t begin, Node forHead, Node body, unsigned iflags) {
        return NodeGeneric;
    }

    Node newComprehensionFor(uint32_t begin, Node forHead, Node body) {
        return NodeGeneric;
    }

    Node newComprehensionBinding(Node kid) {
        // Careful: we're asking this well after the name was parsed, so the
        // value returned may not correspond to |kid|'s actual name.  But it
        // *will* be truthy iff |kid| was a name, so we're safe.
        MOZ_ASSERT(isUnparenthesizedName(kid));
        return NodeGeneric;
    }

    Node newForHead(Node init, Node test, Node update, const TokenPos& pos) {
        return NodeGeneric;
    }

    Node newForInOrOfHead(ParseNodeKind kind, Node target, Node iteratedExpr, const TokenPos& pos) {
        return NodeGeneric;
    }

    MOZ_MUST_USE bool finishInitializerAssignment(Node pn, Node init) { return true; }

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

    Node newList(ParseNodeKind kind, const TokenPos& pos, JSOp op = JSOP_NOP) {
        MOZ_ASSERT(kind != PNK_VAR);
        MOZ_ASSERT(kind != PNK_LET);
        MOZ_ASSERT(kind != PNK_CONST);
        return NodeGeneric;
    }

  private:
    Node newList(ParseNodeKind kind, uint32_t begin, JSOp op = JSOP_NOP) {
        return newList(kind, TokenPos(begin, begin + 1), op);
    }

    template<typename T>
    Node newList(ParseNodeKind kind, const T& begin, JSOp op = JSOP_NOP) = delete;

  public:
    Node newList(ParseNodeKind kind, Node kid, JSOp op = JSOP_NOP) {
        return newList(kind, TokenPos(), op);
    }

    Node newDeclarationList(ParseNodeKind kind, const TokenPos& pos, JSOp op = JSOP_NOP) {
        if (kind == PNK_VAR)
            return NodeVarDeclaration;
        MOZ_ASSERT(kind == PNK_LET || kind == PNK_CONST);
        return NodeLexicalDeclaration;
    }

    bool isDeclarationList(Node node) {
        return node == NodeVarDeclaration || node == NodeLexicalDeclaration;
    }

    Node singleBindingFromDeclaration(Node decl) {
        MOZ_ASSERT(isDeclarationList(decl));

        // This is, unfortunately, very dodgy.  Obviously NodeVarDeclaration
        // and NodeLexicalDeclaration can store no info on the arbitrary
        // number of bindings it could contain.
        //
        // But this method is called only for cloning for-in/of declarations
        // as initialization targets.  That context simplifies matters.  If the
        // binding is a single name, it'll always syntax-parse (or it would
        // already have been rejected as assigning/binding a forbidden name).
        // Otherwise the binding is a destructuring pattern.  But syntax
        // parsing would *already* have aborted when it saw a destructuring
        // pattern.  So we can just say any old thing here, because the only
        // time we'll be wrong is a case that syntax parsing has already
        // rejected.  Use NodeUnparenthesizedName so the SyntaxParseHandler
        // Parser::cloneLeftHandSide can assert it sees only this.
        return NodeUnparenthesizedName;
    }

    Node newCatchList(const TokenPos& pos) {
        return newList(PNK_CATCHLIST, pos, JSOP_NOP);
    }

    Node newCommaExpressionList(Node kid) {
        return NodeUnparenthesizedCommaExpr;
    }

    void addList(Node list, Node kid) {
        MOZ_ASSERT(list == NodeGeneric ||
                   list == NodeUnparenthesizedArray ||
                   list == NodeUnparenthesizedObject ||
                   list == NodeUnparenthesizedCommaExpr ||
                   list == NodeVarDeclaration ||
                   list == NodeLexicalDeclaration ||
                   list == NodeFunctionCall);
    }

    Node newNewExpression(uint32_t begin, Node ctor) {
        Node newExpr = newList(PNK_NEW, begin, JSOP_NEW);
        if (!newExpr)
            return newExpr;

        addList(newExpr, ctor);
        return newExpr;
    }

    Node newAssignment(ParseNodeKind kind, Node lhs, Node rhs, JSOp op) {
        if (kind == PNK_ASSIGN)
            return NodeUnparenthesizedAssignment;
        return newBinary(kind, lhs, rhs, op);
    }

    bool isUnparenthesizedCommaExpression(Node node) {
        return node == NodeUnparenthesizedCommaExpr;
    }

    bool isUnparenthesizedAssignment(Node node) {
        return node == NodeUnparenthesizedAssignment;
    }

    bool isUnparenthesizedUnaryExpression(Node node) {
        return node == NodeUnparenthesizedUnary;
    }

    bool isReturnStatement(Node node) {
        return node == NodeReturn;
    }

    bool isStatementPermittedAfterReturnStatement(Node pn) {
        return pn == NodeFunctionDefinition || pn == NodeVarDeclaration ||
               pn == NodeBreak ||
               pn == NodeThrow ||
               pn == NodeEmptyStatement;
    }

    bool isSuperBase(Node pn) {
        return pn == NodeSuperBase;
    }

    void setOp(Node pn, JSOp op) {}
    void setListFlag(Node pn, unsigned flag) {}
    MOZ_MUST_USE Node parenthesize(Node node) {
        // A number of nodes have different behavior upon parenthesization, but
        // only in some circumstances.  Convert these nodes to special
        // parenthesized forms.
        if (node == NodeUnparenthesizedArgumentsName)
            return NodeParenthesizedArgumentsName;
        if (node == NodeUnparenthesizedEvalName)
            return NodeParenthesizedEvalName;
        if (node == NodeUnparenthesizedName || node == NodePotentialAsyncKeyword)
            return NodeParenthesizedName;

        if (node == NodeUnparenthesizedArray)
            return NodeParenthesizedArray;
        if (node == NodeUnparenthesizedObject)
            return NodeParenthesizedObject;

        // Other nodes need not be recognizable after parenthesization; convert
        // them to a generic node.
        if (node == NodeUnparenthesizedString ||
            node == NodeUnparenthesizedCommaExpr ||
            node == NodeUnparenthesizedAssignment ||
            node == NodeUnparenthesizedUnary)
        {
            return NodeGeneric;
        }

        // In all other cases, the parenthesized form of |node| is equivalent
        // to the unparenthesized form: return |node| unchanged.
        return node;
    }
    MOZ_MUST_USE Node setLikelyIIFE(Node pn) {
        return pn; // Remain in syntax-parse mode.
    }
    void setInDirectivePrologue(Node pn) {}

    bool isConstant(Node pn) { return false; }

    bool isUnparenthesizedName(Node node) {
        return node == NodeUnparenthesizedArgumentsName ||
               node == NodeUnparenthesizedEvalName ||
               node == NodeUnparenthesizedName ||
               node == NodePotentialAsyncKeyword;
    }

    bool isNameAnyParentheses(Node node) {
        if (isUnparenthesizedName(node))
            return true;
        return node == NodeParenthesizedArgumentsName ||
               node == NodeParenthesizedEvalName ||
               node == NodeParenthesizedName;
    }

    bool isArgumentsAnyParentheses(Node node, JSContext* cx) {
        return node == NodeUnparenthesizedArgumentsName || node == NodeParenthesizedArgumentsName;
    }

    bool isEvalAnyParentheses(Node node, JSContext* cx) {
        return node == NodeUnparenthesizedEvalName || node == NodeParenthesizedEvalName;
    }

    const char* nameIsArgumentsEvalAnyParentheses(Node node, JSContext* cx) {
        MOZ_ASSERT(isNameAnyParentheses(node),
                   "must only call this method on known names");

        if (isEvalAnyParentheses(node, cx))
            return js_eval_str;
        if (isArgumentsAnyParentheses(node, cx))
            return js_arguments_str;
        return nullptr;
    }

    bool isAsyncKeyword(Node node, JSContext* cx) {
        return node == NodePotentialAsyncKeyword;
    }

    PropertyName* maybeDottedProperty(Node node) {
        // Note: |super.apply(...)| is a special form that calls an "apply"
        // method retrieved from one value, but using a *different* value as
        // |this|.  It's not really eligible for the funapply/funcall
        // optimizations as they're currently implemented (assuming a single
        // value is used for both retrieval and |this|).
        if (node != NodeDottedProperty)
            return nullptr;
        return lastAtom->asPropertyName();
    }

    JSAtom* isStringExprStatement(Node pn, TokenPos* pos) {
        if (pn == NodeStringExprStatement) {
            *pos = lastStringPos;
            return lastAtom;
        }
        return nullptr;
    }

    bool canSkipLazyInnerFunctions() {
        return false;
    }
    bool canSkipLazyClosedOverBindings() {
        return false;
    }
    JSAtom* nextLazyClosedOverBinding() {
        MOZ_CRASH("SyntaxParseHandlerBase::canSkipLazyClosedOverBindings must return false");
    }

    void adjustGetToSet(Node node) {}

    void disableSyntaxParser() {
    }
};

template<typename CharT>
class SyntaxParseHandler : public SyntaxParseHandlerBase
{
  public:
    // Using frontend::SyntaxParseHandler versus SyntaxParseHandler shouldn't
    // be necessary per C++11 [temp.local]p1: in template argument lists inside
    // a template class, the template class name refers to the template (i.e.
    // SyntaxParseHandler) and not to the particular instantiation of the
    // template class (i.e. SyntaxParseHandler<CharT>).
    //
    // Unfortunately, some versions of clang and MSVC are buggy in this regard.
    // So we refer to SyntaxParseHandler with a qualified name.
    SyntaxParseHandler(JSContext* cx, LifoAlloc& alloc,
                       Parser<frontend::SyntaxParseHandler, CharT>* syntaxParser,
                       LazyScript* lazyOuterFunction)
      : SyntaxParseHandlerBase(cx, alloc, lazyOuterFunction)
    {}

};

} // namespace frontend
} // namespace js

#endif /* frontend_SyntaxParseHandler_h */

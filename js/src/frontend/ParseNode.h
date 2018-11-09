/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParseNode_h
#define frontend_ParseNode_h

#include "mozilla/Attributes.h"

#include "frontend/TokenStream.h"
#include "vm/BytecodeUtil.h"
#include "vm/Printer.h"
#include "vm/Scope.h"

// A few notes on lifetime of ParseNode trees:
//
// - All the `ParseNode` instances MUST BE explicitly allocated in the context's `LifoAlloc`.
//   This is typically implemented by the `FullParseHandler` or it can be reimplemented with
//   a custom `new_`.
//
// - The tree is bulk-deallocated when the parser is deallocated. Consequently, references
//   to a subtree MUST NOT exist once the parser has been deallocated.
//
// - This bulk-deallocation DOES NOT run destructors.
//
// - Instances of `LexicalScope::Data` MUST BE allocated as instances of `ParseNode`, in the same
//   `LifoAlloc`. They are bulk-deallocated alongside the rest of the tree.
//
// - Instances of `JSAtom` used throughout the tree (including instances of `PropertyName`) MUST
//   be kept alive by the parser. This is done through an instance of `AutoKeepAtoms` held by
//   the parser.
//
// - Once the parser is deallocated, the `JSAtom` instances MAY be garbage-collected.


namespace js {
namespace frontend {

class ParseContext;
class FullParseHandler;
class FunctionBox;
class ObjectBox;

#define FOR_EACH_PARSE_NODE_KIND(F) \
    F(EmptyStatement, PN_NULLARY) \
    F(ExpressionStatement, PN_UNARY) \
    F(Comma, PN_LIST) \
    F(Conditional, PN_TERNARY) \
    F(Colon, PN_BINARY) \
    F(Shorthand, PN_BINARY) \
    F(Pos, PN_UNARY) \
    F(Neg, PN_UNARY) \
    F(PreIncrement, PN_UNARY) \
    F(PostIncrement, PN_UNARY) \
    F(PreDecrement, PN_UNARY) \
    F(PostDecrement, PN_UNARY) \
    F(PropertyName, PN_NAME) \
    F(Dot, PN_BINARY) \
    F(Elem, PN_BINARY) \
    F(Array, PN_LIST) \
    F(Elision, PN_NULLARY) \
    F(StatementList, PN_LIST) \
    F(Label, PN_NAME) \
    F(Object, PN_LIST) \
    F(Call, PN_BINARY) \
    F(Arguments, PN_LIST) \
    F(Name, PN_NAME) \
    F(ObjectPropertyName, PN_NAME) \
    F(PrivateName, PN_NAME) \
    F(ComputedName, PN_UNARY) \
    F(Number, PN_NUMBER) \
    F(String, PN_NAME) \
    F(TemplateStringList, PN_LIST) \
    F(TemplateString, PN_NAME) \
    F(TaggedTemplate, PN_BINARY) \
    F(CallSiteObj, PN_LIST) \
    F(RegExp, PN_REGEXP) \
    F(True, PN_NULLARY) \
    F(False, PN_NULLARY) \
    F(Null, PN_NULLARY) \
    F(RawUndefined, PN_NULLARY) \
    F(This, PN_UNARY) \
    F(Function, PN_CODE) \
    F(Module, PN_CODE) \
    F(If, PN_TERNARY) \
    F(Switch, PN_BINARY) \
    F(Case, PN_BINARY) \
    F(While, PN_BINARY) \
    F(DoWhile, PN_BINARY) \
    F(For, PN_BINARY) \
    F(Break, PN_LOOP) \
    F(Continue, PN_LOOP) \
    F(Var, PN_LIST) \
    F(Const, PN_LIST) \
    F(With, PN_BINARY) \
    F(Return, PN_UNARY) \
    F(New, PN_BINARY) \
    /* Delete operations.  These must be sequential. */ \
    F(DeleteName, PN_UNARY) \
    F(DeleteProp, PN_UNARY) \
    F(DeleteElem, PN_UNARY) \
    F(DeleteExpr, PN_UNARY) \
    F(Try, PN_TERNARY) \
    F(Catch, PN_BINARY) \
    F(Throw, PN_UNARY) \
    F(Debugger, PN_NULLARY) \
    F(Generator, PN_NULLARY) \
    F(InitialYield, PN_UNARY) \
    F(Yield, PN_UNARY) \
    F(YieldStar, PN_UNARY) \
    F(LexicalScope, PN_SCOPE) \
    F(Let, PN_LIST) \
    F(Import, PN_BINARY) \
    F(ImportSpecList, PN_LIST) \
    F(ImportSpec, PN_BINARY) \
    F(Export, PN_UNARY) \
    F(ExportFrom, PN_BINARY) \
    F(ExportDefault, PN_BINARY) \
    F(ExportSpecList, PN_LIST) \
    F(ExportSpec, PN_BINARY) \
    F(ExportBatchSpec, PN_NULLARY) \
    F(ForIn, PN_TERNARY) \
    F(ForOf, PN_TERNARY) \
    F(ForHead, PN_TERNARY) \
    F(ParamsBody, PN_LIST) \
    F(Spread, PN_UNARY) \
    F(MutateProto, PN_UNARY) \
    F(Class, PN_TERNARY) \
    F(ClassMethod, PN_BINARY) \
    F(ClassField, PN_FIELD) \
    F(ClassMemberList, PN_LIST) \
    F(ClassNames, PN_BINARY) \
    F(NewTarget, PN_BINARY) \
    F(PosHolder, PN_NULLARY) \
    F(SuperBase, PN_UNARY) \
    F(SuperCall, PN_BINARY) \
    F(SetThis, PN_BINARY) \
    F(ImportMeta, PN_BINARY) \
    F(CallImport, PN_BINARY) \
    \
    /* Unary operators. */ \
    F(TypeOfName, PN_UNARY) \
    F(TypeOfExpr, PN_UNARY) \
    F(Void, PN_UNARY) \
    F(Not, PN_UNARY) \
    F(BitNot, PN_UNARY) \
    F(Await, PN_UNARY) \
    \
    /* \
     * Binary operators. \
     * These must be in the same order as TOK_OR and friends in TokenStream.h. \
     */ \
    F(Pipeline, PN_LIST) \
    F(Or, PN_LIST) \
    F(And, PN_LIST) \
    F(BitOr, PN_LIST) \
    F(BitXor, PN_LIST) \
    F(BitAnd, PN_LIST) \
    F(StrictEq, PN_LIST) \
    F(Eq, PN_LIST) \
    F(StrictNe, PN_LIST) \
    F(Ne, PN_LIST) \
    F(Lt, PN_LIST) \
    F(Le, PN_LIST) \
    F(Gt, PN_LIST) \
    F(Ge, PN_LIST) \
    F(InstanceOf, PN_LIST) \
    F(In, PN_LIST) \
    F(Lsh, PN_LIST) \
    F(Rsh, PN_LIST) \
    F(Ursh, PN_LIST) \
    F(Add, PN_LIST) \
    F(Sub, PN_LIST) \
    F(Star, PN_LIST) \
    F(Div, PN_LIST) \
    F(Mod, PN_LIST) \
    F(Pow, PN_LIST) \
    \
    /* Assignment operators (= += -= etc.). */ \
    /* ParseNode::isAssignment assumes all these are consecutive. */ \
    F(Assign, PN_BINARY) \
    F(AddAssign, PN_BINARY) \
    F(SubAssign, PN_BINARY) \
    F(BitOrAssign, PN_BINARY) \
    F(BitXorAssign, PN_BINARY) \
    F(BitAndAssign, PN_BINARY) \
    F(LshAssign, PN_BINARY) \
    F(RshAssign, PN_BINARY) \
    F(UrshAssign, PN_BINARY) \
    F(MulAssign, PN_BINARY) \
    F(DivAssign, PN_BINARY) \
    F(ModAssign, PN_BINARY) \
    F(PowAssign, PN_BINARY)

/*
 * Parsing builds a tree of nodes that directs code generation.  This tree is
 * not a concrete syntax tree in all respects (for example, || and && are left
 * associative, but (A && B && C) translates into the right-associated tree
 * <A && <B && C>> so that code generation can emit a left-associative branch
 * around <B && C> when A is false).  Nodes are labeled by kind, with a
 * secondary JSOp label when needed.
 *
 * The long comment after this enum block describes the kinds in detail.
 */
enum class ParseNodeKind : uint16_t
{
#define EMIT_ENUM(name, _arity) name,
    FOR_EACH_PARSE_NODE_KIND(EMIT_ENUM)
#undef EMIT_ENUM
    Limit, /* domain size */
    BinOpFirst = ParseNodeKind::Pipeline,
    BinOpLast = ParseNodeKind::Pow,
    AssignmentStart = ParseNodeKind::Assign,
    AssignmentLast = ParseNodeKind::PowAssign
};

inline bool
IsDeleteKind(ParseNodeKind kind)
{
    return ParseNodeKind::DeleteName <= kind && kind <= ParseNodeKind::DeleteExpr;
}

inline bool
IsTypeofKind(ParseNodeKind kind)
{
    return ParseNodeKind::TypeOfName <= kind && kind <= ParseNodeKind::TypeOfExpr;
}

/*
 * <Definitions>
 * Function (CodeNode)
 *   funbox: ptr to js::FunctionBox holding function object containing arg and
 *           var properties.  We create the function object at parse (not emit)
 *           time to specialize arg and var bytecodes early.
 *   body: ParamsBody or null for lazily-parsed function, ordinarily;
 *         ParseNodeKind::LexicalScope for implicit function in genexpr
 * ParamsBody (ListNode)
 *   head: list of formal parameters with
 *           * Name node with non-empty name for SingleNameBinding without
 *             Initializer
 *           * Assign node for SingleNameBinding with Initializer
 *           * Name node with empty name for destructuring
 *               expr: Array or Object for BindingPattern without
 *                     Initializer, Assign for BindingPattern with
 *                     Initializer
 *         followed by either:
 *           * StatementList node for function body statements
 *           * Return for expression closure
 *   count: number of formal parameters + 1
 * Spread (UnaryNode)
 *   kid: expression being spread
 * Class (ClassNode)
 *   kid1: ClassNames for class name. can be null for anonymous class.
 *   kid2: expression after `extends`. null if no expression
 *   kid3: either of
 *           * ClassMemberList, if anonymous class
 *           * LexicalScopeNode which contains ClassMemberList as scopeBody,
 *             if named class
 * ClassNames (ClassNames)
 *   left: Name node for outer binding, or null if the class is an expression
 *         that doesn't create an outer binding
 *   right: Name node for inner binding
 * ClassMemberList (ListNode)
 *   head: list of N ClassMethod or ClassField nodes
 *   count: N >= 0
 * ClassMethod (ClassMethod)
 *   name: propertyName
 *   method: methodDefinition
 * Module (CodeNode)
 *   funbox: ?
 *   body: ?
 *
 * <Statements>
 * StatementList (ListNode)
 *   head: list of N statements
 *   count: N >= 0
 * If (TernaryNode)
 *   kid1: cond
 *   kid2: then
 *   kid3: else or null
 * Switch (SwitchStatement)
 *   left: discriminant
 *   right: LexicalScope node that contains the list of Case nodes, with at
 *          most one default node.
 *   hasDefault: true if there's a default case
 * Case (CaseClause)
 *   left: case-expression if CaseClause, or null if DefaultClause
 *   right: StatementList node for this case's statements
 * While (BinaryNode)
 *   left: cond
 *   right: body
 * DoWhile (BinaryNode)
 *   left: body
 *   right: cond
 * For (ForNode)
 *   left: one of
 *           * ForIn: for (x in y) ...
 *           * ForOf: for (x of x) ...
 *           * ForHead: for (;;) ...
 *   right: body
 * ForIn (TernaryNode)
 *   kid1: declaration or expression to left of 'in'
 *   kid2: null
 *   kid3: object expr to right of 'in'
 * ForOf (TernaryNode)
 *   kid1: declaration or expression to left of 'of'
 *   kid2: null
 *   kid3: expr to right of 'of'
 * ForHead (TernaryNode)
 *   kid1:  init expr before first ';' or nullptr
 *   kid2:  cond expr before second ';' or nullptr
 *   kid3:  update expr after second ';' or nullptr
 * Throw (UnaryNode)
 *   kid: thrown exception
 * Try (TernaryNode)
 *   kid1: try block
 *   kid2: null or LexicalScope for catch-block with scopeBody pointing to a
 *         Catch node
 *   kid3: null or finally block
 * Catch (BinaryNode)
 *   left: Name, Array, or Object catch var node
 *         (Array or Object if destructuring),
 *         or null if optional catch binding
 *   right: catch block statements
 * Break (BreakStatement)
 *   label: label or null
 * Continue (ContinueStatement)
 *   label: label or null
 * With (BinaryNode)
 *   left: head expr
 *   right: body
 * Var, Let, Const (ListNode)
 *   head: list of N Name or Assign nodes
 *         each name node has either
 *           atom: variable name
 *           expr: initializer or null
 *         or
 *           atom: variable name
 *         each assignment node has
 *           left: pattern
 *           right: initializer
 *   count: N > 0
 * Return (UnaryNode)
 *   kid: returned expression, or null if none
 * ExpressionStatement (UnaryNode)
 *   kid: expr
 *   prologue: true if Directive Prologue member in original source, not
 *             introduced via constant folding or other tree rewriting
 * EmptyStatement (NullaryNode)
 *   (no fields)
 * Label (LabeledStatement)
 *   atom: label
 *   expr: labeled statement
 * Import (BinaryNode)
 *   left: ImportSpecList import specifiers
 *   right: String module specifier
 * ImportSpecList (ListNode)
 *   head: list of N ImportSpec nodes
 *   count: N >= 0 (N = 0 for `import {} from ...`)
 * ImportSpec (BinaryNode)
 *   left: import name
 *   right: local binding name
 * Export (UnaryNode)
 *   kid: declaration expression
 * ExportFrom (BinaryNode)
 *   left: ExportSpecList export specifiers
 *   right: String module specifier
 * ExportSpecList (ListNode)
 *   head: list of N ExportSpec nodes
 *   count: N >= 0 (N = 0 for `export {}`)
 * ExportSpec (BinaryNode)
 *   left: local binding name
 *   right: export name
 * ExportDefault (BinaryNode)
 *   left: export default declaration or expression
 *   right: Name node for assignment
 *
 * <Expressions>
 * All left-associated binary trees of the same type are optimized into lists
 * to avoid recursion when processing expression chains.
 * Comma (ListNode)
 *   head: list of N comma-separated exprs
 *   count: N >= 2
 * Assign (BinaryNode)
 *   left: target of assignment
 *   right: value to assign
 * AddAssign, SubAssign, BitOrAssign, BitXorAssign, BitAndAssign,
 * LshAssign, RshAssign, UrshAssign, MulAssign, DivAssign, ModAssign,
 * PowAssign (AssignmentNode)
 *   left: target of assignment
 *   right: value to assign
 *   pn_op: JSOP_ADD for +=, etc
 * Conditional (ConditionalExpression)
 *   (cond ? thenExpr : elseExpr)
 *   kid1: cond
 *   kid2: thenExpr
 *   kid3: elseExpr
 * Pipeline, Or, And, BitOr, BitXor, BitAnd, StrictEq, Eq, StrictNe, Ne,
 * Lt, Le, Gt, Ge, InstanceOf, In, Lsh, Rsh, Ursh, Add, Sub, Star, Div, Mod,
 * Pow (ListNode)
 *   head: list of N subexpressions
 *         All of these operators are left-associative except Pow which is
 *         right-associative, but still forms a list (see comments in
 *         ParseNode::appendOrCreateList).
 *   count: N >= 2
 * Pos, Neg, Void, Not, BitNot, TypeOfName, TypeOfExpr (UnaryNode)
 *   kid: unary expr
 * PreIncrement PostIncrement, PreDecrement, PostDecrement (UnaryNode)
 *   kid: member expr
 * New (BinaryNode)
 *   left: ctor expression on the left of the '('
 *   right: Arguments
 * DeleteName, DeleteProp, DeleteElem, DeleteExpr (UnaryNode)
 *   kid: expression that's evaluated, then the overall delete evaluates to
 *        true; can't be a kind for a more-specific ParseNodeKind::Delete*
 *        unless constant folding (or a similar parse tree manipulation) has
 *        occurred
 *          * DeleteName: Name expr
 *          * DeleteProp: Dot expr
 *          * DeleteElem: Elem expr
 *          * DeleteExpr: Member expr
 * PropertyName (NameNode)
 *   atom: property name being accessed
 * Dot (PropertyAccess)
 *   left: MEMBER expr to left of '.'
 *   right: PropertyName to right of '.'
 * Elem (PropertyByValue)
 *   left: MEMBER expr to left of '['
 *   right: expr between '[' and ']'
 * Call (BinaryNode)
 *   left: callee expression on the left of the '('
 *   right: Arguments
 * Arguments (ListNode)
 *   head: list of arg1, arg2, ... argN
 *   count: N >= 0
 * Array (ListNode)
 *   head: list of N array element expressions
 *         holes ([,,]) are represented by Elision nodes,
 *         spread elements ([...X]) are represented by Spread nodes
 *   count: N >= 0
 * Object (ListNode)
 *   head: list of N nodes, each item is one of:
 *           * MutateProto
 *           * Colon
 *           * Shorthand
 *           * Spread
 *   count: N >= 0
 * Colon (BinaryNode)
 *   key-value pair in object initializer or destructuring lhs
 *   left: property id
 *   right: value
 * Shorthand (BinaryNode)
 *   Same fields as Colon. This is used for object literal properties using
 *   shorthand ({x}).
 * ComputedName (UnaryNode)
 *   ES6 ComputedPropertyName.
 *   kid: the AssignmentExpression inside the square brackets
 * Name (NameNode)
 *   atom: name, or object atom
 *   pn_op: JSOP_GETNAME, JSOP_STRING, or JSOP_OBJECT
 *          If JSOP_GETNAME, pn_op may be JSOP_*ARG or JSOP_*VAR telling
 *          const-ness and static analysis results
 * String (NameNode)
 *   atom: string
 * TemplateStringList (ListNode)
 *   head: list of alternating expr and template strings
 *           TemplateString [, expression, TemplateString]+
 *         there's at least one expression.  If the template literal contains
 *         no ${}-delimited expression, it's parsed as a single TemplateString
 * TemplateString (NameNode)
 *   atom: template string atom
 * TaggedTemplate (BinaryNode)
 *   left: tag expression
 *   right: Arguments, with the first being the call site object, then
 *          arg1, arg2, ... argN
 * CallSiteObj (CallSiteNode)
 *   head:  an Array of raw TemplateString, then corresponding cooked
 *          TemplateString nodes
 *            Array [, cooked TemplateString]+
 *          where the Array is
 *            [raw TemplateString]+
 * RegExp (RegExpLiteral)
 *   regexp: RegExp model object
 * Number (NumericLiteral)
 *   value: double value of numeric literal
 * True, False (BooleanLiteral)
 *   pn_op: JSOp bytecode
 * Null (NullLiteral)
 *   pn_op: JSOp bytecode
 * RawUndefined (RawUndefinedLiteral)
 *   pn_op: JSOp bytecode
 *
 * This (UnaryNode)
 *   kid: '.this' Name if function `this`, else nullptr
 * SuperBase (UnaryNode)
 *   kid: '.this' Name
 * SuperCall (BinaryNode)
 *   left: SuperBase
 *   right: Arguments
 * SetThis (BinaryNode)
 *   left: '.this' Name
 *   right: SuperCall
 *
 * LexicalScope (LexicalScopeNode)
 *   scopeBindings: scope bindings
 *   scopeBody: scope body
 * Generator (NullaryNode)
 * InitialYield (UnaryNode)
 *   kid: generator object
 * Yield, YieldStar, Await (UnaryNode)
 *   kid: expr or null
 * Nop (NullaryNode)
 */
enum ParseNodeArity
{
    PN_NULLARY,                         /* 0 kids */
    PN_UNARY,                           /* one kid, plus a couple of scalars */
    PN_BINARY,                          /* two kids, plus a couple of scalars */
    PN_TERNARY,                         /* three kids */
    PN_CODE,                            /* module or function definition node */
    PN_LIST,                            /* generic singly linked list */
    PN_NAME,                            /* name, label, string */
    PN_FIELD,                           /* field name, optional initializer */
    PN_NUMBER,                          /* numeric literal */
    PN_REGEXP,                          /* regexp literal */
    PN_LOOP,                            /* loop control (break/continue) */
    PN_SCOPE                            /* lexical scope */
};

// FIXME: Remove `*Type` (bug 1489008)
#define FOR_EACH_PARSENODE_SUBCLASS(macro) \
    macro(BinaryNode, BinaryNodeType, asBinary) \
    macro(AssignmentNode, AssignmentNodeType, asAssignment) \
    macro(CaseClause, CaseClauseType, asCaseClause) \
    macro(ClassMethod, ClassMethodType, asClassMethod) \
    macro(ClassField, ClassFieldType, asClassField) \
    macro(ClassNames, ClassNamesType, asClassNames) \
    macro(ForNode, ForNodeType, asFor) \
    macro(PropertyAccess, PropertyAccessType, asPropertyAccess) \
    macro(PropertyByValue, PropertyByValueType, asPropertyByValue) \
    macro(SwitchStatement, SwitchStatementType, asSwitchStatement) \
    \
    macro(CodeNode, CodeNodeType, asCode) \
    \
    macro(LexicalScopeNode, LexicalScopeNodeType, asLexicalScope) \
    \
    macro(ListNode, ListNodeType, asList) \
    macro(CallSiteNode, CallSiteNodeType, asCallSite) \
    \
    macro(LoopControlStatement, LoopControlStatementType, asLoopControlStatement) \
    macro(BreakStatement, BreakStatementType, asBreakStatement) \
    macro(ContinueStatement, ContinueStatementType, asContinueStatement) \
    \
    macro(NameNode, NameNodeType, asName) \
    macro(LabeledStatement, LabeledStatementType, asLabeledStatement) \
    \
    macro(NullaryNode, NullaryNodeType, asNullary) \
    macro(BooleanLiteral, BooleanLiteralType, asBooleanLiteral) \
    macro(DebuggerStatement, DebuggerStatementType, asDebuggerStatement) \
    macro(NullLiteral, NullLiteralType, asNullLiteral) \
    macro(RawUndefinedLiteral, RawUndefinedLiteralType, asRawUndefinedLiteral) \
    \
    macro(NumericLiteral, NumericLiteralType, asNumericLiteral) \
    \
    macro(RegExpLiteral, RegExpLiteralType, asRegExpLiteral) \
    \
    macro(TernaryNode, TernaryNodeType, asTernary) \
    macro(ClassNode, ClassNodeType, asClass) \
    macro(ConditionalExpression, ConditionalExpressionType, asConditionalExpression) \
    macro(TryNode, TryNodeType, asTry) \
    \
    macro(UnaryNode, UnaryNodeType, asUnary) \
    macro(ThisLiteral, ThisLiteralType, asThisLiteral)

#define DECLARE_CLASS(typeName, longTypeName, asMethodName) \
class typeName;
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_CLASS)
#undef DECLARE_CLASS

#ifdef DEBUG
// ParseNodeKindArity[size_t(pnk)] is the arity of a ParseNode of kind pnk.
extern const ParseNodeArity ParseNodeKindArity[];
#endif

class ParseNode
{
    ParseNodeKind pn_type;   /* ParseNodeKind::PNK_* type */
    // pn_op and pn_arity are not declared as the correct enum types
    // due to difficulties with MS bitfield layout rules and a GCC
    // bug.  See https://bugzilla.mozilla.org/show_bug.cgi?id=1383157#c4 for
    // details.
    uint8_t pn_op;      /* see JSOp enum and jsopcode.tbl */
    uint8_t pn_arity:4; /* see ParseNodeArity enum */
    bool pn_parens:1;   /* this expr was enclosed in parens */
    bool pn_rhs_anon_fun:1;  /* this expr is anonymous function or class that
                              * is a direct RHS of ParseNodeKind::Assign or ParseNodeKind::Colon of
                              * property, that needs SetFunctionName. */

    ParseNode(const ParseNode& other) = delete;
    void operator=(const ParseNode& other) = delete;

  public:
    ParseNode(ParseNodeKind kind, JSOp op, ParseNodeArity arity)
      : pn_type(kind),
        pn_op(op),
        pn_arity(arity),
        pn_parens(false),
        pn_rhs_anon_fun(false),
        pn_pos(0, 0),
        pn_next(nullptr)
    {
        MOZ_ASSERT(kind < ParseNodeKind::Limit);
        MOZ_ASSERT(hasExpectedArity());
        memset(&pn_u, 0, sizeof pn_u);
    }

    ParseNode(ParseNodeKind kind, JSOp op, ParseNodeArity arity, const TokenPos& pos)
      : pn_type(kind),
        pn_op(op),
        pn_arity(arity),
        pn_parens(false),
        pn_rhs_anon_fun(false),
        pn_pos(pos),
        pn_next(nullptr)
    {
        MOZ_ASSERT(kind < ParseNodeKind::Limit);
        MOZ_ASSERT(hasExpectedArity());
        memset(&pn_u, 0, sizeof pn_u);
    }

    JSOp getOp() const                     { return JSOp(pn_op); }
    void setOp(JSOp op)                    { pn_op = op; }
    bool isOp(JSOp op) const               { return getOp() == op; }

    ParseNodeKind getKind() const {
        MOZ_ASSERT(pn_type < ParseNodeKind::Limit);
        MOZ_ASSERT(hasExpectedArity());
        return pn_type;
    }
    void setKind(ParseNodeKind kind) {
        MOZ_ASSERT(kind < ParseNodeKind::Limit);
        pn_type = kind;
    }
    bool isKind(ParseNodeKind kind) const  { return getKind() == kind; }

    ParseNodeArity getArity() const        { return ParseNodeArity(pn_arity); }
#ifdef DEBUG
    bool hasExpectedArity() const          { return isArity(ParseNodeKindArity[size_t(pn_type)]); }
#endif
    bool isArity(ParseNodeArity a) const   { return getArity() == a; }
    void setArity(ParseNodeArity a)        { pn_arity = a; MOZ_ASSERT(hasExpectedArity()); }

    bool isBinaryOperation() const {
        ParseNodeKind kind = getKind();
        return ParseNodeKind::BinOpFirst <= kind && kind <= ParseNodeKind::BinOpLast;
    }
    inline bool isName(PropertyName* name) const;

    /* Boolean attributes. */
    bool isInParens() const                { return pn_parens; }
    bool isLikelyIIFE() const              { return isInParens(); }
    void setInParens(bool enabled)         { pn_parens = enabled; }

    bool isDirectRHSAnonFunction() const {
        return pn_rhs_anon_fun;
    }
    void setDirectRHSAnonFunction(bool enabled) {
        pn_rhs_anon_fun = enabled;
    }

    TokenPos            pn_pos;         /* two 16-bit pairs here, for 64 bits */
    ParseNode*          pn_next;        /* intrinsic link in parent PN_LIST */

    union {
        struct {                        /* list of next-linked nodes */
          private:
            friend class ListNode;
            ParseNode*  head;           /* first node in list */
            ParseNode** tail;           /* ptr to last node's pn_next in list */
            uint32_t    count;          /* number of nodes in list */
            uint32_t    xflags;         /* see ListNode class */
        } list;
        struct {                        /* ternary: if, for(;;), ?: */
          private:
            friend class TernaryNode;
            ParseNode*  kid1;           /* condition, discriminant, etc. */
            ParseNode*  kid2;           /* then-part, case list, etc. */
            ParseNode*  kid3;           /* else-part, default case, etc. */
        } ternary;
        struct {                        /* two kids if binary */
          private:
            friend class BinaryNode;
            friend class ForNode;
            friend class ClassMethod;
            friend class PropertyAccess;
            friend class SwitchStatement;
            ParseNode*  left;
            ParseNode*  right;
            union {
                unsigned iflags;        /* JSITER_* flags for ParseNodeKind::For node */
                bool isStatic;          /* only for ParseNodeKind::ClassMethod */
                bool hasDefault;        /* only for ParseNodeKind::Switch */
            };
        } binary;
        struct {                        /* one kid if unary */
          private:
            friend class UnaryNode;
            ParseNode*  kid;
            bool        prologue;       /* directive prologue member */
        } unary;
        struct {                        /* name, labeled statement, etc. */
          private:
            friend class NameNode;
            JSAtom*      atom;          /* lexical name or label atom */
            ParseNode*  initOrStmt;     /* var initializer, argument default,
                                         * or label statement target */
        } name;
        struct {
          private:
            friend class ClassField;
            ParseNode* name;
            ParseNode* initializer;     /* field initializer - optional */
        } field;
        struct {
          private:
            friend class RegExpLiteral;
            ObjectBox* objbox;
        } regexp;
        struct {
          private:
            friend class CodeNode;
            FunctionBox* funbox;        /* function object */
            ParseNode*  body;           /* module or function body */
        } code;
        struct {
          private:
            friend class LexicalScopeNode;
            LexicalScope::Data* bindings;
            ParseNode*          body;
        } scope;
        struct {
          private:
            friend class NumericLiteral;
            double       value;         /* aligned numeric literal value */
            DecimalPoint decimalPoint;  /* Whether the number has a decimal point */
        } number;
        class {
          private:
            friend class LoopControlStatement;
            PropertyName*    label;    /* target of break/continue statement */
        } loopControl;
    } pn_u;

  public:
    /*
     * If |left| is a list of the given kind/left-associative op, append
     * |right| to it and return |left|.  Otherwise return a [left, right] list.
     */
    static ParseNode*
    appendOrCreateList(ParseNodeKind kind, ParseNode* left, ParseNode* right,
                       FullParseHandler* handler, ParseContext* pc);

    /* True if pn is a parsenode representing a literal constant. */
    bool isLiteral() const {
        return isKind(ParseNodeKind::Number) ||
               isKind(ParseNodeKind::String) ||
               isKind(ParseNodeKind::True) ||
               isKind(ParseNodeKind::False) ||
               isKind(ParseNodeKind::Null) ||
               isKind(ParseNodeKind::RawUndefined);
    }

    // True iff this is a for-in/of loop variable declaration (var/let/const).
    inline bool isForLoopDeclaration() const;

    enum AllowConstantObjects {
        DontAllowObjects = 0,
        AllowObjects,
        ForCopyOnWriteArray
    };

    MOZ_MUST_USE bool getConstantValue(JSContext* cx, AllowConstantObjects allowObjects,
                                       MutableHandleValue vp, Value* compare = nullptr,
                                       size_t ncompare = 0, NewObjectKind newKind = TenuredObject);
    inline bool isConstant();

    template <class NodeType>
    inline bool is() const {
        return NodeType::test(*this);
    }

    /* Casting operations. */
    template <class NodeType>
    inline NodeType& as() {
        MOZ_ASSERT(NodeType::test(*this));
        return *static_cast<NodeType*>(this);
    }

    template <class NodeType>
    inline const NodeType& as() const {
        MOZ_ASSERT(NodeType::test(*this));
        return *static_cast<const NodeType*>(this);
    }

#ifdef DEBUG
    // Debugger-friendly stderr printer.
    void dump();
    void dump(GenericPrinter& out);
    void dump(GenericPrinter& out, int indent);
#endif
};

class NullaryNode : public ParseNode
{
  public:
    NullaryNode(ParseNodeKind kind, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_NULLARY, pos) {}

    NullaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_NULLARY, pos) {}

    static bool test(const ParseNode& node) {
        return node.isArity(PN_NULLARY);
    }

#ifdef DEBUG
    void dump(GenericPrinter& out);
#endif
};

class NameNode : public ParseNode
{
  protected:
    NameNode(ParseNodeKind kind, JSOp op, JSAtom* atom, ParseNode* initOrStmt, const TokenPos& pos)
      : ParseNode(kind, op, PN_NAME, pos)
    {
        pn_u.name.atom = atom;
        pn_u.name.initOrStmt = initOrStmt;
    }

  public:
    NameNode(ParseNodeKind kind, JSOp op, JSAtom* atom, const TokenPos& pos)
      : ParseNode(kind, op, PN_NAME, pos)
    {
        pn_u.name.atom = atom;
        pn_u.name.initOrStmt = nullptr;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_NAME);
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    JSAtom* atom() const {
        return pn_u.name.atom;
    }

    PropertyName* name() const {
        MOZ_ASSERT(isKind(ParseNodeKind::Name));
        return atom()->asPropertyName();
    }

    ParseNode* initializer() const {
        return pn_u.name.initOrStmt;
    }

    void setAtom(JSAtom* atom) {
        pn_u.name.atom = atom;
    }

    void setInitializer(ParseNode* init) {
        pn_u.name.initOrStmt = init;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeInitializerReference() {
        return &pn_u.name.initOrStmt;
    }
};

inline bool
ParseNode::isName(PropertyName* name) const
{
    return getKind() == ParseNodeKind::Name && as<NameNode>().name() == name;
}

class UnaryNode : public ParseNode
{
  public:
    UnaryNode(ParseNodeKind kind, const TokenPos& pos, ParseNode* kid)
      : ParseNode(kind, JSOP_NOP, PN_UNARY, pos)
    {
        pn_u.unary.kid = kid;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_UNARY);
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    ParseNode* kid() const {
        return pn_u.unary.kid;
    }

    /* Return true if this node appears in a Directive Prologue. */
    bool isDirectivePrologueMember() const {
        return pn_u.unary.prologue;
    }

    void setIsDirectivePrologueMember() {
        pn_u.unary.prologue = true;
    }

    /*
     * Non-null if this is a statement node which could be a member of a
     * Directive Prologue: an expression statement consisting of a single
     * string literal.
     *
     * This considers only the node and its children, not its context. After
     * parsing, check the node's prologue flag to see if it is indeed part of
     * a directive prologue.
     *
     * Note that a Directive Prologue can contain statements that cannot
     * themselves be directives (string literals that include escape sequences
     * or escaped newlines, say). This member function returns true for such
     * nodes; we use it to determine the extent of the prologue.
     */
    JSAtom* isStringExprStatement() const {
        if (isKind(ParseNodeKind::ExpressionStatement)) {
            if (kid()->isKind(ParseNodeKind::String) && !kid()->isInParens()) {
                return kid()->as<NameNode>().atom();
            }
        }
        return nullptr;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeKidReference() {
        return &pn_u.unary.kid;
    }
};

class BinaryNode : public ParseNode
{
  public:
    BinaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos, ParseNode* left, ParseNode* right)
      : ParseNode(kind, op, PN_BINARY, pos)
    {
        pn_u.binary.left = left;
        pn_u.binary.right = right;
    }

    BinaryNode(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right)
      : ParseNode(kind, op, PN_BINARY, TokenPos::box(left->pn_pos, right->pn_pos))
    {
        pn_u.binary.left = left;
        pn_u.binary.right = right;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_BINARY);
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    ParseNode* left() const {
        return pn_u.binary.left;
    }

    ParseNode* right() const {
        return pn_u.binary.right;
    }

    // Methods used by FoldConstants.cpp.
    // callers are responsible for keeping the list consistent.
    ParseNode** unsafeLeftReference() {
        return &pn_u.binary.left;
    }

    ParseNode** unsafeRightReference() {
        return &pn_u.binary.right;
    }
};

class AssignmentNode : public BinaryNode
{
  public:
    AssignmentNode(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right)
      : BinaryNode(kind, op, TokenPos(left->pn_pos.begin, right->pn_pos.end), left, right)
    {}

    static bool test(const ParseNode& node) {
        ParseNodeKind kind = node.getKind();
        bool match = ParseNodeKind::AssignmentStart <= kind &&
                     kind <= ParseNodeKind::AssignmentLast;
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }
};

class ForNode : public BinaryNode
{
  public:
    ForNode(const TokenPos& pos, ParseNode* forHead, ParseNode* body, unsigned iflags)
      : BinaryNode(ParseNodeKind::For,
                   forHead->isKind(ParseNodeKind::ForIn) ? JSOP_ITER : JSOP_NOP,
                   pos, forHead, body)
    {
        MOZ_ASSERT(forHead->isKind(ParseNodeKind::ForIn) ||
                   forHead->isKind(ParseNodeKind::ForOf) ||
                   forHead->isKind(ParseNodeKind::ForHead));
        pn_u.binary.iflags = iflags;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::For);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    TernaryNode* head() const {
        return &left()->as<TernaryNode>();
    }

    ParseNode* body() const {
        return right();
    }

    unsigned iflags() const {
        return pn_u.binary.iflags;
    }
};

class TernaryNode : public ParseNode
{
  public:
    TernaryNode(ParseNodeKind kind, ParseNode* kid1, ParseNode* kid2, ParseNode* kid3)
      : TernaryNode(kind, kid1, kid2, kid3,
                    TokenPos((kid1 ? kid1 : kid2 ? kid2 : kid3)->pn_pos.begin,
                             (kid3 ? kid3 : kid2 ? kid2 : kid1)->pn_pos.end))
    {}

    TernaryNode(ParseNodeKind kind, ParseNode* kid1, ParseNode* kid2, ParseNode* kid3,
                const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_TERNARY, pos)
    {
        pn_u.ternary.kid1 = kid1;
        pn_u.ternary.kid2 = kid2;
        pn_u.ternary.kid3 = kid3;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_TERNARY);
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    ParseNode* kid1() const {
        return pn_u.ternary.kid1;
    }

    ParseNode* kid2() const {
        return pn_u.ternary.kid2;
    }

    ParseNode* kid3() const {
        return pn_u.ternary.kid3;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeKid1Reference() {
        return &pn_u.ternary.kid1;
    }

    ParseNode** unsafeKid2Reference() {
        return &pn_u.ternary.kid2;
    }

    ParseNode** unsafeKid3Reference() {
        return &pn_u.ternary.kid3;
    }
};

class ListNode : public ParseNode
{
  private:
    // xflags bits.

    // Statement list has top-level function statements.
    static constexpr uint32_t hasTopLevelFunctionDeclarationsBit = 0x01;

    // One or more of
    //   * array has holes
    //   * array has spread node
    static constexpr uint32_t hasArrayHoleOrSpreadBit = 0x02;

    // Array/Object/Class initializer has non-constants.
    //   * array has holes
    //   * array has spread node
    //   * array has element which is known not to be constant
    //   * array has no element
    //   * object/class has __proto__
    //   * object/class has property which is known not to be constant
    //   * object/class shorthand property
    //   * object/class spread property
    //   * object/class has method
    //   * object/class has computed property
    static constexpr uint32_t hasNonConstInitializerBit = 0x04;

    void checkConsistency() const
#ifndef DEBUG
    {}
#endif
    ;

  public:
    ListNode(ParseNodeKind kind, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_LIST, pos)
    {
        makeEmpty();
    }

    ListNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_LIST, pos)
    {
        makeEmpty();
    }

    ListNode(ParseNodeKind kind, JSOp op, ParseNode* kid)
      : ParseNode(kind, op, PN_LIST, kid->pn_pos)
    {
        if (kid->pn_pos.begin < pn_pos.begin) {
            pn_pos.begin = kid->pn_pos.begin;
        }
        pn_pos.end = kid->pn_pos.end;

        pn_u.list.head = kid;
        pn_u.list.tail = &kid->pn_next;
        pn_u.list.count = 1;
        pn_u.list.xflags = 0;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_LIST);
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    ParseNode* head() const {
        return pn_u.list.head;
    }

    ParseNode** tail() const {
        return pn_u.list.tail;
    }

    uint32_t count() const {
        return pn_u.list.count;
    }

    bool empty() const {
        return count() == 0;
    }

    MOZ_MUST_USE bool hasTopLevelFunctionDeclarations() const {
        MOZ_ASSERT(isKind(ParseNodeKind::StatementList));
        return pn_u.list.xflags & hasTopLevelFunctionDeclarationsBit;
    }

    MOZ_MUST_USE bool hasArrayHoleOrSpread() const {
        MOZ_ASSERT(isKind(ParseNodeKind::Array));
        return pn_u.list.xflags & hasArrayHoleOrSpreadBit;
    }

    MOZ_MUST_USE bool hasNonConstInitializer() const {
        MOZ_ASSERT(isKind(ParseNodeKind::Array) ||
                   isKind(ParseNodeKind::Object) ||
                   isKind(ParseNodeKind::ClassMemberList));
        return pn_u.list.xflags & hasNonConstInitializerBit;
    }

    void setHasTopLevelFunctionDeclarations() {
        MOZ_ASSERT(isKind(ParseNodeKind::StatementList));
        pn_u.list.xflags |= hasTopLevelFunctionDeclarationsBit;
    }

    void setHasArrayHoleOrSpread() {
        MOZ_ASSERT(isKind(ParseNodeKind::Array));
        pn_u.list.xflags |= hasArrayHoleOrSpreadBit;
    }

    void setHasNonConstInitializer() {
        MOZ_ASSERT(isKind(ParseNodeKind::Array) ||
                   isKind(ParseNodeKind::Object) ||
                   isKind(ParseNodeKind::ClassMemberList));
        pn_u.list.xflags |= hasNonConstInitializerBit;
    }

    /*
     * Compute a pointer to the last element in a singly-linked list. NB: list
     * must be non-empty -- this is asserted!
     */
    ParseNode* last() const {
        MOZ_ASSERT(!empty());
        //
        // ParseNode                      ParseNode
        // +-----+---------+-----+        +-----+---------+-----+
        // | ... | pn_next | ... | +-...->| ... | pn_next | ... |
        // +-----+---------+-----+ |      +-----+---------+-----+
        // ^       |               |      ^     ^
        // |       +---------------+      |     |
        // |                              |     tail()
        // |                              |
        // head()                         last()
        //
        return (ParseNode*)(uintptr_t(tail()) - offsetof(ParseNode, pn_next));
    }

    void replaceLast(ParseNode* node) {
        MOZ_ASSERT(!empty());
        pn_pos.end = node->pn_pos.end;

        ParseNode* item = head();
        ParseNode* lastNode = last();
        MOZ_ASSERT(item);
        if (item == lastNode) {
            pn_u.list.head = node;
        } else {
            while (item->pn_next != lastNode) {
                MOZ_ASSERT(item->pn_next);
                item = item->pn_next;
            }
            item->pn_next = node;
        }
        pn_u.list.tail = &node->pn_next;
    }

    void makeEmpty() {
        pn_u.list.head = nullptr;
        pn_u.list.tail = &pn_u.list.head;
        pn_u.list.count = 0;
        pn_u.list.xflags = 0;
    }

    void append(ParseNode* item) {
        MOZ_ASSERT(item->pn_pos.begin >= pn_pos.begin);
        appendWithoutOrderAssumption(item);
    }

    void appendWithoutOrderAssumption(ParseNode* item) {
        pn_pos.end = item->pn_pos.end;
        *pn_u.list.tail = item;
        pn_u.list.tail = &item->pn_next;
        pn_u.list.count++;
    }

    void prepend(ParseNode* item) {
        item->pn_next = pn_u.list.head;
        pn_u.list.head = item;
        if (pn_u.list.tail == &pn_u.list.head) {
            pn_u.list.tail = &item->pn_next;
        }
        pn_u.list.count++;
    }

    void prependAndUpdatePos(ParseNode* item) {
        prepend(item);
        pn_pos.begin = item->pn_pos.begin;
    }

    // Methods used by FoldConstants.cpp.
    // Caller is responsible for keeping the list consistent.
    ParseNode** unsafeHeadReference() {
        return &pn_u.list.head;
    }

    void unsafeReplaceTail(ParseNode** newTail) {
        pn_u.list.tail = newTail;
        checkConsistency();
    }

    void unsafeDecrementCount() {
        MOZ_ASSERT(count() > 1);
        pn_u.list.count--;
    }

  private:
    // Classes to iterate over ListNode contents:
    //
    // Usage:
    //   ListNode* list;
    //   for (ParseNode* item : list->contents()) {
    //     // item is ParseNode* typed.
    //   }
    class iterator
    {
      private:
        ParseNode* node_;

        friend class ListNode;
        explicit iterator(ParseNode* node)
          : node_(node)
        {}

      public:
        bool operator==(const iterator& other) const {
            return node_ == other.node_;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        iterator& operator++() {
            node_ = node_->pn_next;
            return *this;
        }

        ParseNode* operator*() {
            return node_;
        }

        const ParseNode* operator*() const {
            return node_;
        }
    };

    class range
    {
      private:
        ParseNode* begin_;
        ParseNode* end_;

        friend class ListNode;
        range(ParseNode* begin, ParseNode* end)
          : begin_(begin),
            end_(end)
        {}

      public:
        iterator begin() {
            return iterator(begin_);
        }

        iterator end() {
            return iterator(end_);
        }

        const iterator begin() const {
            return iterator(begin_);
        }

        const iterator end() const {
            return iterator(end_);
        }

        const iterator cbegin() const {
            return begin();
        }

        const iterator cend() const {
            return end();
        }
    };

#ifdef DEBUG
  MOZ_MUST_USE bool contains(ParseNode* target) const {
      MOZ_ASSERT(target);
      for (ParseNode* node : contents()) {
          if (target == node) {
              return true;
          }
      }
      return false;
  }
#endif

  public:
    range contents() {
        return range(head(), nullptr);
    }

    const range contents() const {
        return range(head(), nullptr);
    }

    range contentsFrom(ParseNode* begin) {
        MOZ_ASSERT_IF(begin, contains(begin));
        return range(begin, nullptr);
    }

    const range contentsFrom(ParseNode* begin) const {
        MOZ_ASSERT_IF(begin, contains(begin));
        return range(begin, nullptr);
    }

    range contentsTo(ParseNode* end) {
        MOZ_ASSERT_IF(end, contains(end));
        return range(head(), end);
    }

    const range contentsTo(ParseNode* end) const {
        MOZ_ASSERT_IF(end, contains(end));
        return range(head(), end);
    }
};

inline bool
ParseNode::isForLoopDeclaration() const
{
    if (isKind(ParseNodeKind::Var) || isKind(ParseNodeKind::Let) || isKind(ParseNodeKind::Const)) {
        MOZ_ASSERT(!as<ListNode>().empty());
        return true;
    }

    return false;
}

class CodeNode : public ParseNode
{
  public:
    CodeNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_CODE, pos)
    {
        MOZ_ASSERT(kind == ParseNodeKind::Function || kind == ParseNodeKind::Module);
        MOZ_ASSERT_IF(kind == ParseNodeKind::Module, op == JSOP_NOP);
        MOZ_ASSERT(op == JSOP_NOP || // statement, module
                   op == JSOP_LAMBDA_ARROW || // arrow function
                   op == JSOP_LAMBDA); // expression, method, accessor, &c.
        MOZ_ASSERT(!pn_u.code.body);
        MOZ_ASSERT(!pn_u.code.funbox);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Function) || node.isKind(ParseNodeKind::Module);
        MOZ_ASSERT_IF(match, node.isArity(PN_CODE));
        return match;
    }

#ifdef DEBUG
  void dump(GenericPrinter& out, int indent);
#endif

    FunctionBox* funbox() const {
        return pn_u.code.funbox;
    }

    ListNode* body() const {
        return pn_u.code.body ? &pn_u.code.body->as<ListNode>() : nullptr;
    }

    void setFunbox(FunctionBox* funbox) {
        pn_u.code.funbox = funbox;
    }

    void setBody(ListNode* body) {
        pn_u.code.body = body;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeBodyReference() {
        return &pn_u.code.body;
    }

    bool functionIsHoisted() const {
        MOZ_ASSERT(isKind(ParseNodeKind::Function));
        MOZ_ASSERT(isOp(JSOP_LAMBDA) ||        // lambda
                   isOp(JSOP_LAMBDA_ARROW) ||  // arrow function
                   isOp(JSOP_DEFFUN) ||        // non-body-level function statement
                   isOp(JSOP_NOP) ||           // body-level function stmt in global code
                   isOp(JSOP_GETLOCAL) ||      // body-level function stmt in function code
                   isOp(JSOP_GETARG) ||        // body-level function redeclaring formal
                   isOp(JSOP_INITLEXICAL));    // block-level function stmt
        return !isOp(JSOP_LAMBDA) && !isOp(JSOP_LAMBDA_ARROW) && !isOp(JSOP_DEFFUN);
    }
};

class NumericLiteral : public ParseNode
{
  public:
    NumericLiteral(double value, DecimalPoint decimalPoint, const TokenPos& pos)
      : ParseNode(ParseNodeKind::Number, JSOP_NOP, PN_NUMBER, pos)
    {
        pn_u.number.value = value;
        pn_u.number.decimalPoint = decimalPoint;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Number);
        MOZ_ASSERT_IF(match, node.isArity(PN_NUMBER));
        return match;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    double value() const {
        return pn_u.number.value;
    }

    DecimalPoint decimalPoint() const {
        return pn_u.number.decimalPoint;
    }

    void setValue(double v) {
        pn_u.number.value = v;
    }
};

class LexicalScopeNode : public ParseNode
{
  public:
    LexicalScopeNode(LexicalScope::Data* bindings, ParseNode* body)
      : ParseNode(ParseNodeKind::LexicalScope, JSOP_NOP, PN_SCOPE, body->pn_pos)
    {
        pn_u.scope.bindings = bindings;
        pn_u.scope.body = body;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::LexicalScope);
        MOZ_ASSERT_IF(match, node.isArity(PN_SCOPE));
        return match;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    Handle<LexicalScope::Data*> scopeBindings() const {
        MOZ_ASSERT(!isEmptyScope());
        // Bindings' GC safety depend on the presence of an AutoKeepAtoms that
        // the rest of the frontend also depends on.
        return Handle<LexicalScope::Data*>::fromMarkedLocation(&pn_u.scope.bindings);
    }

    ParseNode* scopeBody() const {
        return pn_u.scope.body;
    }

    void setScopeBody(ParseNode* body) {
        pn_u.scope.body = body;
    }

    bool isEmptyScope() const {
        return !pn_u.scope.bindings;
    }

    ParseNode** unsafeScopeBodyReference() {
        return &pn_u.scope.body;
    }
};

class LabeledStatement : public NameNode
{
  public:
    LabeledStatement(PropertyName* label, ParseNode* stmt, uint32_t begin)
      : NameNode(ParseNodeKind::Label, JSOP_NOP, label, stmt, TokenPos(begin, stmt->pn_pos.end))
    {}

    PropertyName* label() const {
        return atom()->asPropertyName();
    }

    ParseNode* statement() const {
        return initializer();
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Label);
        MOZ_ASSERT_IF(match, node.isArity(PN_NAME));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeStatementReference() {
        return unsafeInitializerReference();
    }
};

// Inside a switch statement, a CaseClause is a case-label and the subsequent
// statements. The same node type is used for DefaultClauses. The only
// difference is that their caseExpression() is null.
class CaseClause : public BinaryNode
{
  public:
    CaseClause(ParseNode* expr, ParseNode* stmts, uint32_t begin)
      : BinaryNode(ParseNodeKind::Case, JSOP_NOP, TokenPos(begin, stmts->pn_pos.end), expr, stmts)
    {}

    ParseNode* caseExpression() const {
        return left();
    }

    bool isDefault() const {
        return !caseExpression();
    }

    ListNode* statementList() const {
        return &right()->as<ListNode>();
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Case);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class LoopControlStatement : public ParseNode
{
  protected:
    LoopControlStatement(ParseNodeKind kind, PropertyName* label, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_LOOP, pos)
    {
        MOZ_ASSERT(kind == ParseNodeKind::Break || kind == ParseNodeKind::Continue);
        pn_u.loopControl.label = label;
    }

  public:
    /* Label associated with this break/continue statement, if any. */
    PropertyName* label() const {
        return pn_u.loopControl.label;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Break) || node.isKind(ParseNodeKind::Continue);
        MOZ_ASSERT_IF(match, node.isArity(PN_LOOP));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class BreakStatement : public LoopControlStatement
{
  public:
    BreakStatement(PropertyName* label, const TokenPos& pos)
      : LoopControlStatement(ParseNodeKind::Break, label, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Break);
        MOZ_ASSERT_IF(match, node.is<LoopControlStatement>());
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class ContinueStatement : public LoopControlStatement
{
  public:
    ContinueStatement(PropertyName* label, const TokenPos& pos)
      : LoopControlStatement(ParseNodeKind::Continue, label, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Continue);
        MOZ_ASSERT_IF(match, node.is<LoopControlStatement>());
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class DebuggerStatement : public NullaryNode
{
  public:
    explicit DebuggerStatement(const TokenPos& pos)
      : NullaryNode(ParseNodeKind::Debugger, JSOP_NOP, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Debugger);
        MOZ_ASSERT_IF(match, node.is<NullaryNode>());
        return match;
    }
};

class ConditionalExpression : public TernaryNode
{
  public:
    ConditionalExpression(ParseNode* condition, ParseNode* thenExpr, ParseNode* elseExpr)
      : TernaryNode(ParseNodeKind::Conditional, condition, thenExpr, elseExpr,
                    TokenPos(condition->pn_pos.begin, elseExpr->pn_pos.end))
    {
        MOZ_ASSERT(condition);
        MOZ_ASSERT(thenExpr);
        MOZ_ASSERT(elseExpr);
    }

    ParseNode& condition() const {
        return *kid1();
    }

    ParseNode& thenExpression() const {
        return *kid2();
    }

    ParseNode& elseExpression() const {
        return *kid3();
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Conditional);
        MOZ_ASSERT_IF(match, node.is<TernaryNode>());
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class TryNode : public TernaryNode
{
  public:
    TryNode(uint32_t begin, ParseNode* body, LexicalScopeNode* catchScope,
            ParseNode* finallyBlock)
      : TernaryNode(ParseNodeKind::Try, body, catchScope, finallyBlock,
                    TokenPos(begin, (finallyBlock ? finallyBlock : catchScope)->pn_pos.end))
    {
        MOZ_ASSERT(body);
        MOZ_ASSERT(catchScope || finallyBlock);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Try);
        MOZ_ASSERT_IF(match, node.is<TernaryNode>());
        return match;
    }

    ParseNode* body() const {
        return kid1();
    }

    LexicalScopeNode* catchScope() const {
        return kid2() ? &kid2()->as<LexicalScopeNode>() : nullptr;
    }

    ParseNode* finallyBlock() const {
        return kid3();
    }
};

class ThisLiteral : public UnaryNode
{
  public:
    ThisLiteral(const TokenPos& pos, ParseNode* thisName)
      : UnaryNode(ParseNodeKind::This, pos, thisName)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::This);
        MOZ_ASSERT_IF(match, node.is<UnaryNode>());
        return match;
    }
};

class NullLiteral : public NullaryNode
{
  public:
    explicit NullLiteral(const TokenPos& pos)
      : NullaryNode(ParseNodeKind::Null, JSOP_NULL, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Null);
        MOZ_ASSERT_IF(match, node.is<NullaryNode>());
        return match;
    }
};

// This is only used internally, currently just for tagged templates.
// It represents the value 'undefined' (aka `void 0`), like NullLiteral
// represents the value 'null'.
class RawUndefinedLiteral : public NullaryNode
{
  public:
    explicit RawUndefinedLiteral(const TokenPos& pos)
      : NullaryNode(ParseNodeKind::RawUndefined, JSOP_UNDEFINED, pos) { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::RawUndefined);
        MOZ_ASSERT_IF(match, node.is<NullaryNode>());
        return match;
    }
};

class BooleanLiteral : public NullaryNode
{
  public:
    BooleanLiteral(bool b, const TokenPos& pos)
      : NullaryNode(b ? ParseNodeKind::True : ParseNodeKind::False,
                    b ? JSOP_TRUE : JSOP_FALSE, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::True) || node.isKind(ParseNodeKind::False);
        MOZ_ASSERT_IF(match, node.is<NullaryNode>());
        return match;
    }
};

class RegExpLiteral : public ParseNode
{
  public:
    RegExpLiteral(ObjectBox* reobj, const TokenPos& pos)
      : ParseNode(ParseNodeKind::RegExp, JSOP_REGEXP, PN_REGEXP, pos)
    {
        pn_u.regexp.objbox = reobj;
    }

    ObjectBox* objbox() const {
        return pn_u.regexp.objbox;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::RegExp);
        MOZ_ASSERT_IF(match, node.isArity(PN_REGEXP));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_REGEXP));
        return match;
    }
};

class PropertyAccess : public BinaryNode
{
  public:
    /*
     * PropertyAccess nodes can have any expression/'super' as left-hand
     * side, but the name must be a ParseNodeKind::PropertyName node.
     */
    PropertyAccess(ParseNode* lhs, NameNode* name, uint32_t begin, uint32_t end)
      : BinaryNode(ParseNodeKind::Dot, JSOP_NOP, TokenPos(begin, end), lhs, name)
    {
        MOZ_ASSERT(lhs);
        MOZ_ASSERT(name);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Dot);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        MOZ_ASSERT_IF(match, node.as<BinaryNode>().right()->isKind(ParseNodeKind::PropertyName));
        return match;
    }

    ParseNode& expression() const {
        return *left();
    }

    NameNode& key() const {
        return right()->as<NameNode>();
    }

    // Method used by BytecodeEmitter::emitPropLHS for optimization.
    // Those methods allow expression to temporarily be nullptr for
    // optimization purpose.
    ParseNode* maybeExpression() const {
        return left();
    }

    void setExpression(ParseNode* pn) {
        pn_u.binary.left = pn;
    }

    PropertyName& name() const {
        return *right()->as<NameNode>().atom()->asPropertyName();
    }

    bool isSuper() const {
        // ParseNodeKind::SuperBase cannot result from any expression syntax.
        return expression().isKind(ParseNodeKind::SuperBase);
    }
};

class PropertyByValue : public BinaryNode
{
  public:
    PropertyByValue(ParseNode* lhs, ParseNode* propExpr, uint32_t begin, uint32_t end)
      : BinaryNode(ParseNodeKind::Elem, JSOP_NOP, TokenPos(begin, end), lhs, propExpr)
    {}

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Elem);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    ParseNode& expression() const {
        return *left();
    }

    ParseNode& key() const {
        return *right();
    }

    bool isSuper() const {
        return left()->isKind(ParseNodeKind::SuperBase);
    }
};

/*
 * A CallSiteNode represents the implicit call site object argument in a TaggedTemplate.
 */
class CallSiteNode : public ListNode
{
  public:
    explicit CallSiteNode(uint32_t begin): ListNode(ParseNodeKind::CallSiteObj, TokenPos(begin, begin + 1)) {}

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::CallSiteObj);
        MOZ_ASSERT_IF(match, node.is<ListNode>());
        return match;
    }

    MOZ_MUST_USE bool getRawArrayValue(JSContext* cx, MutableHandleValue vp) {
        return head()->getConstantValue(cx, AllowObjects, vp);
    }

    ListNode* rawNodes() const {
        MOZ_ASSERT(head());
        return &head()->as<ListNode>();
    }
};

class ClassMethod : public BinaryNode
{
  public:
    /*
     * Method definitions often keep a name and function body that overlap,
     * so explicitly define the beginning and end here.
     */
    ClassMethod(ParseNode* name, ParseNode* body, JSOp op, bool isStatic)
      : BinaryNode(ParseNodeKind::ClassMethod, op, TokenPos(name->pn_pos.begin, body->pn_pos.end), name, body)
    {
        pn_u.binary.isStatic = isStatic;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::ClassMethod);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    ParseNode& name() const {
        return *left();
    }

    CodeNode& method() const {
        return right()->as<CodeNode>();
    }

    bool isStatic() const {
        return pn_u.binary.isStatic;
    }
};

class ClassField : public ParseNode
{
  public:
    ClassField(ParseNode* name, ParseNode* initializer)
      : ParseNode(ParseNodeKind::ClassField, JSOP_NOP, PN_FIELD,
                  initializer == nullptr ? name->pn_pos : TokenPos::box(name->pn_pos, initializer->pn_pos))
    {
        pn_u.field.name = name;
        pn_u.field.initializer = initializer;
    }

    static bool test(const ParseNode& node) {
        return node.isKind(ParseNodeKind::ClassField);
    }

    ParseNode& name() const {
        return *pn_u.field.name;
    }

    bool hasInitializer() const {
        return pn_u.field.initializer != nullptr;
    }

    ParseNode& initializer() const {
        return *pn_u.field.initializer;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif

    // Methods used by FoldConstants.cpp.
    // callers are responsible for keeping the list consistent.
    ParseNode** unsafeNameReference() {
        return &pn_u.field.name;
    }

    ParseNode** unsafeInitializerReference() {
        return &pn_u.field.initializer;
    }
};

class SwitchStatement : public BinaryNode
{
  public:
    SwitchStatement(uint32_t begin, ParseNode* discriminant, LexicalScopeNode* lexicalForCaseList,
                    bool hasDefault)
      : BinaryNode(ParseNodeKind::Switch, JSOP_NOP,
                   TokenPos(begin, lexicalForCaseList->pn_pos.end),
                   discriminant, lexicalForCaseList)
    {
#ifdef DEBUG
        ListNode* cases = &lexicalForCaseList->scopeBody()->as<ListNode>();
        MOZ_ASSERT(cases->isKind(ParseNodeKind::StatementList));
        bool found = false;
        for (ParseNode* item : cases->contents()) {
            CaseClause* caseNode = &item->as<CaseClause>();
            if (caseNode->isDefault()) {
                found = true;
                break;
            }
        }
        MOZ_ASSERT(found == hasDefault);
#endif

        pn_u.binary.hasDefault = hasDefault;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Switch);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    ParseNode& discriminant() const {
        return *left();
    }

    LexicalScopeNode& lexicalForCaseList() const {
        return right()->as<LexicalScopeNode>();
    }

    bool hasDefault() const {
        return pn_u.binary.hasDefault;
    }
};

class ClassNames : public BinaryNode
{
  public:
    ClassNames(ParseNode* outerBinding, ParseNode* innerBinding, const TokenPos& pos)
      : BinaryNode(ParseNodeKind::ClassNames, JSOP_NOP, pos, outerBinding, innerBinding)
    {
        MOZ_ASSERT_IF(outerBinding, outerBinding->isKind(ParseNodeKind::Name));
        MOZ_ASSERT(innerBinding->isKind(ParseNodeKind::Name));
        MOZ_ASSERT_IF(outerBinding,
                      innerBinding->as<NameNode>().atom() == outerBinding->as<NameNode>().atom());
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::ClassNames);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    /*
     * Classes require two definitions: The first "outer" binding binds the
     * class into the scope in which it was declared. the outer binding is a
     * mutable lexial binding. The second "inner" binding binds the class by
     * name inside a block in which the methods are evaulated. It is immutable,
     * giving the methods access to the static members of the class even if
     * the outer binding has been overwritten.
     */
    NameNode* outerBinding() const {
        if (ParseNode* binding = left()) {
            return &binding->as<NameNode>();
        }
        return nullptr;
    }

    NameNode* innerBinding() const {
        return &right()->as<NameNode>();
    }
};

class ClassNode : public TernaryNode
{
  public:
    ClassNode(ParseNode* names, ParseNode* heritage, ParseNode* membersOrBlock,
              const TokenPos& pos)
      : TernaryNode(ParseNodeKind::Class, names, heritage, membersOrBlock, pos)
    {
        MOZ_ASSERT_IF(names, names->is<ClassNames>());
        MOZ_ASSERT(membersOrBlock->is<LexicalScopeNode>() ||
                   membersOrBlock->isKind(ParseNodeKind::ClassMemberList));
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(ParseNodeKind::Class);
        MOZ_ASSERT_IF(match, node.is<TernaryNode>());
        return match;
    }

    ClassNames* names() const {
        return kid1() ? &kid1()->as<ClassNames>() : nullptr;
    }
    ParseNode* heritage() const {
        return kid2();
    }
    ListNode* memberList() const {
        ParseNode* membersOrBlock = kid3();
        if (membersOrBlock->isKind(ParseNodeKind::ClassMemberList)) {
            return &membersOrBlock->as<ListNode>();
        }

        ListNode* list = &membersOrBlock->as<LexicalScopeNode>().scopeBody()->as<ListNode>();
        MOZ_ASSERT(list->isKind(ParseNodeKind::ClassMemberList));
        return list;
    }
    Handle<LexicalScope::Data*> scopeBindings() const {
        ParseNode* scope = kid3();
        return scope->as<LexicalScopeNode>().scopeBindings();
    }
};

#ifdef DEBUG
void DumpParseTree(ParseNode* pn, GenericPrinter& out, int indent = 0);
#endif

class ParseNodeAllocator
{
  public:
    explicit ParseNodeAllocator(JSContext* cx, LifoAlloc& alloc)
      : cx(cx), alloc(alloc)
    {}

    void* allocNode();

  private:
    JSContext* cx;
    LifoAlloc& alloc;
};

inline bool
ParseNode::isConstant()
{
    switch (pn_type) {
      case ParseNodeKind::Number:
      case ParseNodeKind::String:
      case ParseNodeKind::TemplateString:
      case ParseNodeKind::Null:
      case ParseNodeKind::RawUndefined:
      case ParseNodeKind::False:
      case ParseNodeKind::True:
        return true;
      case ParseNodeKind::Array:
      case ParseNodeKind::Object:
        return !as<ListNode>().hasNonConstInitializer();
      default:
        return false;
    }
}

class ObjectBox
{
  public:
    JSObject* object;

    ObjectBox(JSObject* object, ObjectBox* traceLink);
    bool isFunctionBox() { return object->is<JSFunction>(); }
    FunctionBox* asFunctionBox();
    virtual void trace(JSTracer* trc);

    static void TraceList(JSTracer* trc, ObjectBox* listHead);

  protected:
    friend struct CGObjectList;

    ObjectBox* traceLink;
    ObjectBox* emitLink;

    ObjectBox(JSFunction* function, ObjectBox* traceLink);
};

enum ParseReportKind
{
    ParseError,
    ParseWarning,
    ParseExtraWarning,
    ParseStrictError
};

enum class AccessorType {
    None,
    Getter,
    Setter
};

inline JSOp
AccessorTypeToJSOp(AccessorType atype)
{
    switch (atype) {
      case AccessorType::None:
        return JSOP_INITPROP;
      case AccessorType::Getter:
        return JSOP_INITPROP_GETTER;
      case AccessorType::Setter:
        return JSOP_INITPROP_SETTER;
      default:
        MOZ_CRASH("unexpected accessor type");
    }
}

enum class FunctionSyntaxKind
{
    // A non-arrow function expression.
    Expression,

    // A named function appearing as a Statement.
    Statement,

    Arrow,
    Method,
    ClassConstructor,
    DerivedClassConstructor,
    Getter,
    Setter,
};

static inline bool
IsConstructorKind(FunctionSyntaxKind kind)
{
    return kind == FunctionSyntaxKind::ClassConstructor ||
           kind == FunctionSyntaxKind::DerivedClassConstructor;
}

static inline bool
IsMethodDefinitionKind(FunctionSyntaxKind kind)
{
    return IsConstructorKind(kind) ||
           kind == FunctionSyntaxKind::Method ||
           kind == FunctionSyntaxKind::Getter ||
           kind == FunctionSyntaxKind::Setter;
}

static inline ParseNode*
FunctionFormalParametersList(ParseNode* fn, unsigned* numFormals)
{
    MOZ_ASSERT(fn->isKind(ParseNodeKind::Function));
    ListNode* argsBody = fn->as<CodeNode>().body();
    MOZ_ASSERT(argsBody->isKind(ParseNodeKind::ParamsBody));
    *numFormals = argsBody->count();
    if (*numFormals > 0 &&
        argsBody->last()->is<LexicalScopeNode>() &&
        argsBody->last()->as<LexicalScopeNode>().scopeBody()->isKind(ParseNodeKind::StatementList))
    {
        (*numFormals)--;
    }
    return argsBody->head();
}

bool
IsAnonymousFunctionDefinition(ParseNode* pn);

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_ParseNode_h */

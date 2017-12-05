/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParseNode_h
#define frontend_ParseNode_h

#include "mozilla/Attributes.h"

#include "builtin/ModuleObject.h"
#include "frontend/TokenStream.h"
#include "vm/Printer.h"

namespace js {
namespace frontend {

class ParseContext;
class FullParseHandler;
class FunctionBox;
class ObjectBox;

#define FOR_EACH_PARSE_NODE_KIND(F) \
    F(NOP) \
    F(SEMI) \
    F(COMMA) \
    F(CONDITIONAL) \
    F(COLON) \
    F(SHORTHAND) \
    F(POS) \
    F(NEG) \
    F(PREINCREMENT) \
    F(POSTINCREMENT) \
    F(PREDECREMENT) \
    F(POSTDECREMENT) \
    F(DOT) \
    F(ELEM) \
    F(ARRAY) \
    F(ELISION) \
    F(STATEMENTLIST) \
    F(LABEL) \
    F(OBJECT) \
    F(CALL) \
    F(NAME) \
    F(OBJECT_PROPERTY_NAME) \
    F(COMPUTED_NAME) \
    F(NUMBER) \
    F(STRING) \
    F(TEMPLATE_STRING_LIST) \
    F(TEMPLATE_STRING) \
    F(TAGGED_TEMPLATE) \
    F(CALLSITEOBJ) \
    F(REGEXP) \
    F(TRUE) \
    F(FALSE) \
    F(NULL) \
    F(RAW_UNDEFINED) \
    F(THIS) \
    F(FUNCTION) \
    F(MODULE) \
    F(IF) \
    F(SWITCH) \
    F(CASE) \
    F(WHILE) \
    F(DOWHILE) \
    F(FOR) \
    F(BREAK) \
    F(CONTINUE) \
    F(VAR) \
    F(CONST) \
    F(WITH) \
    F(RETURN) \
    F(NEW) \
    /* Delete operations.  These must be sequential. */ \
    F(DELETENAME) \
    F(DELETEPROP) \
    F(DELETEELEM) \
    F(DELETEEXPR) \
    F(TRY) \
    F(CATCH) \
    F(CATCHLIST) \
    F(THROW) \
    F(DEBUGGER) \
    F(GENERATOR) \
    F(INITIALYIELD) \
    F(YIELD) \
    F(YIELD_STAR) \
    F(LEXICALSCOPE) \
    F(LET) \
    F(IMPORT) \
    F(IMPORT_SPEC_LIST) \
    F(IMPORT_SPEC) \
    F(EXPORT) \
    F(EXPORT_FROM) \
    F(EXPORT_DEFAULT) \
    F(EXPORT_SPEC_LIST) \
    F(EXPORT_SPEC) \
    F(EXPORT_BATCH_SPEC) \
    F(FORIN) \
    F(FOROF) \
    F(FORHEAD) \
    F(PARAMSBODY) \
    F(SPREAD) \
    F(MUTATEPROTO) \
    F(CLASS) \
    F(CLASSMETHOD) \
    F(CLASSMETHODLIST) \
    F(CLASSNAMES) \
    F(NEWTARGET) \
    F(POSHOLDER) \
    F(SUPERBASE) \
    F(SUPERCALL) \
    F(SETTHIS) \
    \
    /* Unary operators. */ \
    F(TYPEOFNAME) \
    F(TYPEOFEXPR) \
    F(VOID) \
    F(NOT) \
    F(BITNOT) \
    F(AWAIT) \
    \
    /* \
     * Binary operators. \
     * These must be in the same order as TOK_OR and friends in TokenStream.h. \
     */ \
    F(PIPELINE) \
    F(OR) \
    F(AND) \
    F(BITOR) \
    F(BITXOR) \
    F(BITAND) \
    F(STRICTEQ) \
    F(EQ) \
    F(STRICTNE) \
    F(NE) \
    F(LT) \
    F(LE) \
    F(GT) \
    F(GE) \
    F(INSTANCEOF) \
    F(IN) \
    F(LSH) \
    F(RSH) \
    F(URSH) \
    F(ADD) \
    F(SUB) \
    F(STAR) \
    F(DIV) \
    F(MOD) \
    F(POW) \
    \
    /* Assignment operators (= += -= etc.). */ \
    /* ParseNode::isAssignment assumes all these are consecutive. */ \
    F(ASSIGN) \
    F(ADDASSIGN) \
    F(SUBASSIGN) \
    F(BITORASSIGN) \
    F(BITXORASSIGN) \
    F(BITANDASSIGN) \
    F(LSHASSIGN) \
    F(RSHASSIGN) \
    F(URSHASSIGN) \
    F(MULASSIGN) \
    F(DIVASSIGN) \
    F(MODASSIGN) \
    F(POWASSIGN)

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
enum ParseNodeKind : uint16_t
{
#define EMIT_ENUM(name) PNK_##name,
    FOR_EACH_PARSE_NODE_KIND(EMIT_ENUM)
#undef EMIT_ENUM
    PNK_LIMIT, /* domain size */
    PNK_BINOP_FIRST = PNK_PIPELINE,
    PNK_BINOP_LAST = PNK_POW,
    PNK_ASSIGNMENT_START = PNK_ASSIGN,
    PNK_ASSIGNMENT_LAST = PNK_POWASSIGN
};

inline bool
IsDeleteKind(ParseNodeKind kind)
{
    return PNK_DELETENAME <= kind && kind <= PNK_DELETEEXPR;
}

inline bool
IsTypeofKind(ParseNodeKind kind)
{
    return PNK_TYPEOFNAME <= kind && kind <= PNK_TYPEOFEXPR;
}

/*
 * Label        Variant     Members
 * -----        -------     -------
 * <Definitions>
 * PNK_FUNCTION name        pn_funbox: ptr to js::FunctionBox holding function
 *                            object containing arg and var properties.  We
 *                            create the function object at parse (not emit)
 *                            time to specialize arg and var bytecodes early.
 *                          pn_body: PNK_PARAMSBODY, ordinarily;
 *                            PNK_LEXICALSCOPE for implicit function in genexpr
 * PNK_PARAMSBODY list      list of formal parameters with
 *                              PNK_NAME node with non-empty name for
 *                                SingleNameBinding without Initializer
 *                              PNK_ASSIGN node for SingleNameBinding with
 *                                Initializer
 *                              PNK_NAME node with empty name for destructuring
 *                                pn_expr: PNK_ARRAY, PNK_OBJECT, or PNK_ASSIGN
 *                                  PNK_ARRAY or PNK_OBJECT for BindingPattern
 *                                    without Initializer
 *                                  PNK_ASSIGN for BindingPattern with
 *                                    Initializer
 *                          followed by:
 *                              PNK_STATEMENTLIST node for function body
 *                                statements,
 *                              PNK_RETURN for expression closure
 *                          pn_count: 1 + number of formal parameters
 *                          pn_tree: PNK_PARAMSBODY or PNK_STATEMENTLIST node
 * PNK_SPREAD   unary       pn_kid: expression being spread
 *
 * <Statements>
 * PNK_STATEMENTLIST list   pn_head: list of pn_count statements
 * PNK_IF       ternary     pn_kid1: cond, pn_kid2: then, pn_kid3: else or null.
 * PNK_SWITCH   binary      pn_left: discriminant
 *                          pn_right: list of PNK_CASE nodes, with at most one
 *                            default node, or if there are let bindings
 *                            in the top level of the switch body's cases, a
 *                            PNK_LEXICALSCOPE node that contains the list of
 *                            PNK_CASE nodes.
 * PNK_CASE     binary      pn_left: case-expression if CaseClause, or
 *                            null if DefaultClause
 *                          pn_right: PNK_STATEMENTLIST node for this case's
 *                            statements
 *                          pn_u.binary.offset: scratch space for the emitter
 * PNK_WHILE    binary      pn_left: cond, pn_right: body
 * PNK_DOWHILE  binary      pn_left: body, pn_right: cond
 * PNK_FOR      binary      pn_left: either PNK_FORIN (for-in statement),
 *                            PNK_FOROF (for-of) or PNK_FORHEAD (for(;;))
 *                          pn_right: body
 * PNK_FORIN    ternary     pn_kid1: declaration or expression to left of 'in'
 *                          pn_kid2: null
 *                          pn_kid3: object expr to right of 'in'
 * PNK_FOROF    ternary     pn_kid1: declaration or expression to left of 'of'
 *                          pn_kid2: null
 *                          pn_kid3: expr to right of 'of'
 * PNK_FORHEAD  ternary     pn_kid1:  init expr before first ';' or nullptr
 *                          pn_kid2:  cond expr before second ';' or nullptr
 *                          pn_kid3:  update expr after second ';' or nullptr
 * PNK_THROW    unary       pn_kid: exception
 * PNK_TRY      ternary     pn_kid1: try block
 *                          pn_kid2: null or PNK_CATCHLIST list
 *                          pn_kid3: null or finally block
 * PNK_CATCHLIST list       pn_head: list of PNK_LEXICALSCOPE nodes, one per
 *                                   catch-block, each with pn_expr pointing
 *                                   to a PNK_CATCH node
 * PNK_CATCH    ternary     pn_kid1: PNK_NAME, PNK_ARRAY, or PNK_OBJECT catch var node
 *                                   (PNK_ARRAY or PNK_OBJECT if destructuring)
 *                          pn_kid2: null or the catch guard expression
 *                          pn_kid3: catch block statements
 * PNK_BREAK    name        pn_atom: label or null
 * PNK_CONTINUE name        pn_atom: label or null
 * PNK_WITH     binary      pn_left: head expr; pn_right: body;
 * PNK_VAR,     list        pn_head: list of PNK_NAME or PNK_ASSIGN nodes
 * PNK_LET,                          each name node has either
 * PNK_CONST                           pn_used: false
 *                                     pn_atom: variable name
 *                                     pn_expr: initializer or null
 *                                   or
 *                                     pn_used: true
 *                                     pn_atom: variable name
 *                                     pn_lexdef: def node
 *                                   each assignment node has
 *                                     pn_left: PNK_NAME with pn_used true and
 *                                              pn_lexdef (NOT pn_expr) set
 *                                     pn_right: initializer
 * PNK_RETURN   unary       pn_kid: return expr or null
 * PNK_SEMI     unary       pn_kid: expr or null statement
 *                          pn_prologue: true if Directive Prologue member
 *                              in original source, not introduced via
 *                              constant folding or other tree rewriting
 * PNK_LABEL    name        pn_atom: label, pn_expr: labeled statement
 * PNK_IMPORT   binary      pn_left: PNK_IMPORT_SPEC_LIST import specifiers
 *                          pn_right: PNK_STRING module specifier
 * PNK_EXPORT   unary       pn_kid: declaration expression
 * PNK_EXPORT_FROM binary   pn_left: PNK_EXPORT_SPEC_LIST export specifiers
 *                          pn_right: PNK_STRING module specifier
 * PNK_EXPORT_DEFAULT unary pn_kid: export default declaration or expression
 *
 * <Expressions>
 * All left-associated binary trees of the same type are optimized into lists
 * to avoid recursion when processing expression chains.
 * PNK_COMMA    list        pn_head: list of pn_count comma-separated exprs
 * PNK_ASSIGN   binary      pn_left: lvalue, pn_right: rvalue
 * PNK_ADDASSIGN,   binary  pn_left: lvalue, pn_right: rvalue
 * PNK_SUBASSIGN,           pn_op: JSOP_ADD for +=, etc.
 * PNK_BITORASSIGN,
 * PNK_BITXORASSIGN,
 * PNK_BITANDASSIGN,
 * PNK_LSHASSIGN,
 * PNK_RSHASSIGN,
 * PNK_URSHASSIGN,
 * PNK_MULASSIGN,
 * PNK_DIVASSIGN,
 * PNK_MODASSIGN,
 * PNK_POWASSIGN
 * PNK_CONDITIONAL ternary  (cond ? trueExpr : falseExpr)
 *                          pn_kid1: cond, pn_kid2: then, pn_kid3: else
 * PNK_OR,      list        pn_head; list of pn_count subexpressions
 * PNK_AND,                 All of these operators are left-associative except (**).
 * PNK_BITOR,
 * PNK_BITXOR,
 * PNK_BITAND,
 * PNK_EQ,
 * PNK_NE,
 * PNK_STRICTEQ,
 * PNK_STRICTNE,
 * PNK_LT,
 * PNK_LE,
 * PNK_GT,
 * PNK_GE,
 * PNK_LSH,
 * PNK_RSH,
 * PNK_URSH,
 * PNK_ADD,
 * PNK_SUB,
 * PNK_STAR,
 * PNK_DIV,
 * PNK_MOD,
 * PNK_POW                  (**) is right-associative, but still forms a list.
 *                          See comments in ParseNode::appendOrCreateList.
 *
 * PNK_POS,     unary       pn_kid: UNARY expr
 * PNK_NEG
 * PNK_VOID,    unary       pn_kid: UNARY expr
 * PNK_NOT,
 * PNK_BITNOT
 * PNK_TYPEOFNAME, unary    pn_kid: UNARY expr
 * PNK_TYPEOFEXPR
 * PNK_PREINCREMENT, unary  pn_kid: MEMBER expr
 * PNK_POSTINCREMENT,
 * PNK_PREDECREMENT,
 * PNK_POSTDECREMENT
 * PNK_NEW      list        pn_head: list of ctor, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          ctor is a MEMBER expr
 * PNK_DELETENAME unary     pn_kid: PNK_NAME expr
 * PNK_DELETEPROP unary     pn_kid: PNK_DOT expr
 * PNK_DELETEELEM unary     pn_kid: PNK_ELEM expr
 * PNK_DELETEEXPR unary     pn_kid: MEMBER expr that's evaluated, then the
 *                          overall delete evaluates to true; can't be a kind
 *                          for a more-specific PNK_DELETE* unless constant
 *                          folding (or a similar parse tree manipulation) has
 *                          occurred
 * PNK_DOT      name        pn_expr: MEMBER expr to left of .
 *                          pn_atom: name to right of .
 * PNK_ELEM     binary      pn_left: MEMBER expr to left of [
 *                          pn_right: expr between [ and ]
 * PNK_CALL     list        pn_head: list of call, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          call is a MEMBER expr naming a callable object
 * PNK_ARRAY    list        pn_head: list of pn_count array element exprs
 *                          [,,] holes are represented by PNK_ELISION nodes
 *                          pn_xflags: PN_ENDCOMMA if extra comma at end
 * PNK_OBJECT   list        pn_head: list of pn_count binary PNK_COLON nodes
 * PNK_COLON    binary      key-value pair in object initializer or
 *                          destructuring lhs
 *                          pn_left: property id, pn_right: value
 * PNK_SHORTHAND binary     Same fields as PNK_COLON. This is used for object
 *                          literal properties using shorthand ({x}).
 * PNK_COMPUTED_NAME unary  ES6 ComputedPropertyName.
 *                          pn_kid: the AssignmentExpression inside the square brackets
 * PNK_NAME,    name        pn_atom: name, string, or object atom
 * PNK_STRING               pn_op: JSOP_GETNAME, JSOP_STRING, or JSOP_OBJECT
 *                          If JSOP_GETNAME, pn_op may be JSOP_*ARG or JSOP_*VAR
 *                          telling const-ness and static analysis results
 * PNK_TEMPLATE_STRING_LIST pn_head: list of alternating expr and template strings
 *              list
 * PNK_TEMPLATE_STRING      pn_atom: template string atom
                nullary     pn_op: JSOP_NOP
 * PNK_TAGGED_TEMPLATE      pn_head: list of call, call site object, arg1, arg2, ... argN
 *              list        pn_count: 2 + N (N is the number of substitutions)
 * PNK_CALLSITEOBJ list     pn_head: a PNK_ARRAY node followed by
 *                          list of pn_count - 1 PNK_TEMPLATE_STRING nodes
 * PNK_REGEXP   nullary     pn_objbox: RegExp model object
 * PNK_NUMBER   dval        pn_dval: double value of numeric literal
 * PNK_TRUE,    nullary     pn_op: JSOp bytecode
 * PNK_FALSE,
 * PNK_NULL,
 * PNK_RAW_UNDEFINED
 *
 * PNK_THIS,        unary   pn_kid: '.this' Name if function `this`, else nullptr
 * PNK_SUPERBASE    unary   pn_kid: '.this' Name
 *
 * PNK_SETTHIS      binary  pn_left: '.this' Name, pn_right: SuperCall
 *
 * PNK_LEXICALSCOPE scope   pn_u.scope.bindings: scope bindings
 *                          pn_u.scope.body: scope body
 * PNK_GENERATOR    nullary
 * PNK_INITIALYIELD unary   pn_kid: generator object
 * PNK_YIELD,       unary   pn_kid: expr or null
 * PNK_YIELD_STAR,
 * PNK_AWAIT
 * PNK_NOP          nullary
 */
enum ParseNodeArity
{
    PN_NULLARY,                         /* 0 kids, only pn_atom/pn_dval/etc. */
    PN_UNARY,                           /* one kid, plus a couple of scalars */
    PN_BINARY,                          /* two kids, plus a couple of scalars */
    PN_TERNARY,                         /* three kids */
    PN_CODE,                            /* module or function definition node */
    PN_LIST,                            /* generic singly linked list */
    PN_NAME,                            /* name, label, or regexp */
    PN_SCOPE                            /* lexical scope */
};

class LoopControlStatement;
class BreakStatement;
class ContinueStatement;
class ConditionalExpression;
class PropertyAccess;

class ParseNode
{
    ParseNodeKind pn_type;   /* PNK_* type */
    // pn_op and pn_arity are not declared as the correct enum types
    // due to difficulties with MS bitfield layout rules and a GCC
    // bug.  See https://bugzilla.mozilla.org/show_bug.cgi?id=1383157#c4 for
    // details.
    uint8_t pn_op;      /* see JSOp enum and jsopcode.tbl */
    uint8_t pn_arity:4; /* see ParseNodeArity enum */
    bool pn_parens:1;   /* this expr was enclosed in parens */
    bool pn_rhs_anon_fun:1;  /* this expr is anonymous function or class that
                              * is a direct RHS of PNK_ASSIGN or PNK_COLON of
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
        MOZ_ASSERT(kind < PNK_LIMIT);
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
        MOZ_ASSERT(kind < PNK_LIMIT);
        memset(&pn_u, 0, sizeof pn_u);
    }

    JSOp getOp() const                     { return JSOp(pn_op); }
    void setOp(JSOp op)                    { pn_op = op; }
    bool isOp(JSOp op) const               { return getOp() == op; }

    ParseNodeKind getKind() const {
        MOZ_ASSERT(pn_type < PNK_LIMIT);
        return pn_type;
    }
    void setKind(ParseNodeKind kind) {
        MOZ_ASSERT(kind < PNK_LIMIT);
        pn_type = kind;
    }
    bool isKind(ParseNodeKind kind) const  { return getKind() == kind; }

    ParseNodeArity getArity() const        { return ParseNodeArity(pn_arity); }
    bool isArity(ParseNodeArity a) const   { return getArity() == a; }
    void setArity(ParseNodeArity a)        { pn_arity = a; }

    bool isAssignment() const {
        ParseNodeKind kind = getKind();
        return PNK_ASSIGNMENT_START <= kind && kind <= PNK_ASSIGNMENT_LAST;
    }

    bool isBinaryOperation() const {
        ParseNodeKind kind = getKind();
        return PNK_BINOP_FIRST <= kind && kind <= PNK_BINOP_LAST;
    }

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
            ParseNode*  head;           /* first node in list */
            ParseNode** tail;           /* ptr to ptr to last node in list */
            uint32_t    count;          /* number of nodes in list */
            uint32_t    xflags;         /* see PNX_* below */
        } list;
        struct {                        /* ternary: if, for(;;), ?: */
            ParseNode*  kid1;           /* condition, discriminant, etc. */
            ParseNode*  kid2;           /* then-part, case list, etc. */
            ParseNode*  kid3;           /* else-part, default case, etc. */
        } ternary;
        struct {                        /* two kids if binary */
            ParseNode*  left;
            ParseNode*  right;
            union {
                unsigned iflags;        /* JSITER_* flags for PNK_FOR node */
                bool isStatic;          /* only for PNK_CLASSMETHOD */
                uint32_t offset;        /* for the emitter's use on PNK_CASE nodes */
            };
        } binary;
        struct {                        /* one kid if unary */
            ParseNode*  kid;
            bool        prologue;       /* directive prologue member (as
                                           pn_prologue) */
        } unary;
        struct {                        /* name, labeled statement, etc. */
            union {
                JSAtom*      atom;      /* lexical name or label atom */
                ObjectBox*   objbox;    /* regexp object */
                FunctionBox* funbox;    /* function object */
            };
            ParseNode*  expr;           /* module or function body, var
                                           initializer, argument default, or
                                           base object of PNK_DOT */
        } name;
        struct {
            LexicalScope::Data* bindings;
            ParseNode*          body;
        } scope;
        struct {
            double       value;         /* aligned numeric literal value */
            DecimalPoint decimalPoint;  /* Whether the number has a decimal point */
        } number;
        class {
            friend class LoopControlStatement;
            PropertyName*    label;    /* target of break/continue statement */
        } loopControl;
    } pn_u;

#define pn_objbox       pn_u.name.objbox
#define pn_funbox       pn_u.name.funbox
#define pn_body         pn_u.name.expr
#define pn_head         pn_u.list.head
#define pn_tail         pn_u.list.tail
#define pn_count        pn_u.list.count
#define pn_xflags       pn_u.list.xflags
#define pn_kid1         pn_u.ternary.kid1
#define pn_kid2         pn_u.ternary.kid2
#define pn_kid3         pn_u.ternary.kid3
#define pn_left         pn_u.binary.left
#define pn_right        pn_u.binary.right
#define pn_pval         pn_u.binary.pval
#define pn_iflags       pn_u.binary.iflags
#define pn_kid          pn_u.unary.kid
#define pn_prologue     pn_u.unary.prologue
#define pn_atom         pn_u.name.atom
#define pn_objbox       pn_u.name.objbox
#define pn_expr         pn_u.name.expr
#define pn_dval         pn_u.number.value


  public:
    /*
     * If |left| is a list of the given kind/left-associative op, append
     * |right| to it and return |left|.  Otherwise return a [left, right] list.
     */
    static ParseNode*
    appendOrCreateList(ParseNodeKind kind, ParseNode* left, ParseNode* right,
                       FullParseHandler* handler, ParseContext* pc);

    inline PropertyName* name() const;
    inline JSAtom* atom() const;

    ParseNode* expr() const {
        MOZ_ASSERT(pn_arity == PN_NAME || pn_arity == PN_CODE);
        return pn_expr;
    }

    bool isEmptyScope() const {
        MOZ_ASSERT(pn_arity == PN_SCOPE);
        return !pn_u.scope.bindings;
    }

    Handle<LexicalScope::Data*> scopeBindings() const {
        MOZ_ASSERT(!isEmptyScope());
        // Bindings' GC safety depend on the presence of an AutoKeepAtoms that
        // the rest of the frontend also depends on.
        return Handle<LexicalScope::Data*>::fromMarkedLocation(&pn_u.scope.bindings);
    }

    ParseNode* scopeBody() const {
        MOZ_ASSERT(pn_arity == PN_SCOPE);
        return pn_u.scope.body;
    }

    void setScopeBody(ParseNode* body) {
        MOZ_ASSERT(pn_arity == PN_SCOPE);
        pn_u.scope.body = body;
    }

/* PN_LIST pn_xflags bits. */
#define PNX_FUNCDEFS    0x01            /* contains top-level function statements */
#define PNX_ARRAYHOLESPREAD 0x02        /* one or more of
                                           1. array initialiser has holes
                                           2. array initializer has spread node */
#define PNX_NONCONST    0x04            /* initialiser has non-constants */

    bool functionIsHoisted() const {
        MOZ_ASSERT(pn_arity == PN_CODE && getKind() == PNK_FUNCTION);
        MOZ_ASSERT(isOp(JSOP_LAMBDA) ||        // lambda
                   isOp(JSOP_LAMBDA_ARROW) ||  // arrow function
                   isOp(JSOP_DEFFUN) ||        // non-body-level function statement
                   isOp(JSOP_NOP) ||           // body-level function stmt in global code
                   isOp(JSOP_GETLOCAL) ||      // body-level function stmt in function code
                   isOp(JSOP_GETARG) ||        // body-level function redeclaring formal
                   isOp(JSOP_INITLEXICAL));    // block-level function stmt
        return !isOp(JSOP_LAMBDA) && !isOp(JSOP_LAMBDA_ARROW) && !isOp(JSOP_DEFFUN);
    }

    /*
     * True if this statement node could be a member of a Directive Prologue: an
     * expression statement consisting of a single string literal.
     *
     * This considers only the node and its children, not its context. After
     * parsing, check the node's pn_prologue flag to see if it is indeed part of
     * a directive prologue.
     *
     * Note that a Directive Prologue can contain statements that cannot
     * themselves be directives (string literals that include escape sequences
     * or escaped newlines, say). This member function returns true for such
     * nodes; we use it to determine the extent of the prologue.
     */
    JSAtom* isStringExprStatement() const {
        if (getKind() == PNK_SEMI) {
            MOZ_ASSERT(pn_arity == PN_UNARY);
            ParseNode* kid = pn_kid;
            if (kid && kid->getKind() == PNK_STRING && !kid->pn_parens)
                return kid->pn_atom;
        }
        return nullptr;
    }

    /* True if pn is a parsenode representing a literal constant. */
    bool isLiteral() const {
        return isKind(PNK_NUMBER) ||
               isKind(PNK_STRING) ||
               isKind(PNK_TRUE) ||
               isKind(PNK_FALSE) ||
               isKind(PNK_NULL) ||
               isKind(PNK_RAW_UNDEFINED);
    }

    /* Return true if this node appears in a Directive Prologue. */
    bool isDirectivePrologueMember() const { return pn_prologue; }

    // True iff this is a for-in/of loop variable declaration (var/let/const).
    bool isForLoopDeclaration() const {
        if (isKind(PNK_VAR) || isKind(PNK_LET) || isKind(PNK_CONST)) {
            MOZ_ASSERT(isArity(PN_LIST));
            MOZ_ASSERT(pn_count > 0);
            return true;
        }

        return false;
    }

    /*
     * Compute a pointer to the last element in a singly-linked list. NB: list
     * must be non-empty for correct PN_LAST usage -- this is asserted!
     */
    ParseNode* last() const {
        MOZ_ASSERT(pn_arity == PN_LIST);
        MOZ_ASSERT(pn_count != 0);
        return (ParseNode*)(uintptr_t(pn_tail) - offsetof(ParseNode, pn_next));
    }

    void initNumber(double value, DecimalPoint decimalPoint) {
        MOZ_ASSERT(pn_arity == PN_NULLARY);
        MOZ_ASSERT(getKind() == PNK_NUMBER);
        pn_u.number.value = value;
        pn_u.number.decimalPoint = decimalPoint;
    }

    void makeEmpty() {
        MOZ_ASSERT(pn_arity == PN_LIST);
        pn_head = nullptr;
        pn_tail = &pn_head;
        pn_count = 0;
        pn_xflags = 0;
    }

    void initList(ParseNode* pn) {
        MOZ_ASSERT(pn_arity == PN_LIST);
        if (pn->pn_pos.begin < pn_pos.begin)
            pn_pos.begin = pn->pn_pos.begin;
        pn_pos.end = pn->pn_pos.end;
        pn_head = pn;
        pn_tail = &pn->pn_next;
        pn_count = 1;
        pn_xflags = 0;
    }

    void append(ParseNode* pn) {
        MOZ_ASSERT(pn_arity == PN_LIST);
        MOZ_ASSERT(pn->pn_pos.begin >= pn_pos.begin);
        pn_pos.end = pn->pn_pos.end;
        *pn_tail = pn;
        pn_tail = &pn->pn_next;
        pn_count++;
    }

    void prepend(ParseNode* pn) {
        MOZ_ASSERT(pn_arity == PN_LIST);
        pn->pn_next = pn_head;
        pn_head = pn;
        if (pn_tail == &pn_head)
            pn_tail = &pn->pn_next;
        pn_count++;
    }

    void checkListConsistency()
#ifndef DEBUG
    {}
#endif
    ;

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

struct NullaryNode : public ParseNode
{
    NullaryNode(ParseNodeKind kind, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_NULLARY, pos) {}
    NullaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_NULLARY, pos) {}

    // This constructor is for a few mad uses in the emitter. It populates
    // the pn_atom field even though that field belongs to a branch in pn_u
    // that nullary nodes shouldn't use -- bogus.
    NullaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos, JSAtom* atom)
      : ParseNode(kind, op, PN_NULLARY, pos)
    {
        pn_atom = atom;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out);
#endif
};

struct UnaryNode : public ParseNode
{
    UnaryNode(ParseNodeKind kind, const TokenPos& pos, ParseNode* kid)
      : ParseNode(kind, JSOP_NOP, PN_UNARY, pos)
    {
        pn_kid = kid;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif
};

struct BinaryNode : public ParseNode
{
    BinaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos, ParseNode* left, ParseNode* right)
      : ParseNode(kind, op, PN_BINARY, pos)
    {
        pn_left = left;
        pn_right = right;
    }

    BinaryNode(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right)
      : ParseNode(kind, op, PN_BINARY, TokenPos::box(left->pn_pos, right->pn_pos))
    {
        pn_left = left;
        pn_right = right;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif
};

struct TernaryNode : public ParseNode
{
    TernaryNode(ParseNodeKind kind, ParseNode* kid1, ParseNode* kid2, ParseNode* kid3)
      : ParseNode(kind, JSOP_NOP, PN_TERNARY,
                  TokenPos((kid1 ? kid1 : kid2 ? kid2 : kid3)->pn_pos.begin,
                           (kid3 ? kid3 : kid2 ? kid2 : kid1)->pn_pos.end))
    {
        pn_kid1 = kid1;
        pn_kid2 = kid2;
        pn_kid3 = kid3;
    }

    TernaryNode(ParseNodeKind kind, ParseNode* kid1, ParseNode* kid2, ParseNode* kid3,
                const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_TERNARY, pos)
    {
        pn_kid1 = kid1;
        pn_kid2 = kid2;
        pn_kid3 = kid3;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif
};

struct ListNode : public ParseNode
{
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
        initList(kid);
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_LIST);
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif
};

struct CodeNode : public ParseNode
{
    CodeNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_CODE, pos)
    {
        MOZ_ASSERT(kind == PNK_FUNCTION || kind == PNK_MODULE);
        MOZ_ASSERT_IF(kind == PNK_MODULE, op == JSOP_NOP);
        MOZ_ASSERT(op == JSOP_NOP || // statement, module
                   op == JSOP_LAMBDA_ARROW || // arrow function
                   op == JSOP_LAMBDA); // expression, method, accessor, &c.
        MOZ_ASSERT(!pn_body);
        MOZ_ASSERT(!pn_objbox);
    }

  public:
#ifdef DEBUG
  void dump(GenericPrinter& out, int indent);
#endif
};

struct NameNode : public ParseNode
{
    NameNode(ParseNodeKind kind, JSOp op, JSAtom* atom, const TokenPos& pos)
      : ParseNode(kind, op, PN_NAME, pos)
    {
        pn_atom = atom;
        pn_expr = nullptr;
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif
};

struct LexicalScopeNode : public ParseNode
{
    LexicalScopeNode(LexicalScope::Data* bindings, ParseNode* body)
      : ParseNode(PNK_LEXICALSCOPE, JSOP_NOP, PN_SCOPE, body->pn_pos)
    {
        pn_u.scope.bindings = bindings;
        pn_u.scope.body = body;
    }

    static bool test(const ParseNode& node) {
        return node.isKind(PNK_LEXICALSCOPE);
    }

#ifdef DEBUG
    void dump(GenericPrinter& out, int indent);
#endif
};

class LabeledStatement : public ParseNode
{
  public:
    LabeledStatement(PropertyName* label, ParseNode* stmt, uint32_t begin)
      : ParseNode(PNK_LABEL, JSOP_NOP, PN_NAME, TokenPos(begin, stmt->pn_pos.end))
    {
        pn_atom = label;
        pn_expr = stmt;
    }

    PropertyName* label() const {
        return pn_atom->asPropertyName();
    }

    ParseNode* statement() const {
        return pn_expr;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_LABEL);
        MOZ_ASSERT_IF(match, node.isArity(PN_NAME));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

// Inside a switch statement, a CaseClause is a case-label and the subsequent
// statements. The same node type is used for DefaultClauses. The only
// difference is that their caseExpression() is null.
class CaseClause : public BinaryNode
{
  public:
    CaseClause(ParseNode* expr, ParseNode* stmts, uint32_t begin)
      : BinaryNode(PNK_CASE, JSOP_NOP, TokenPos(begin, stmts->pn_pos.end), expr, stmts) {}

    ParseNode* caseExpression() const { return pn_left; }
    bool isDefault() const { return !caseExpression(); }
    ParseNode* statementList() const { return pn_right; }

    // The next CaseClause in the same switch statement.
    CaseClause* next() const { return pn_next ? &pn_next->as<CaseClause>() : nullptr; }

    // Scratch space used by the emitter.
    uint32_t offset() const { return pn_u.binary.offset; }
    void setOffset(uint32_t u) { pn_u.binary.offset = u; }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CASE);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class LoopControlStatement : public ParseNode
{
  protected:
    LoopControlStatement(ParseNodeKind kind, PropertyName* label, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_NULLARY, pos)
    {
        MOZ_ASSERT(kind == PNK_BREAK || kind == PNK_CONTINUE);
        pn_u.loopControl.label = label;
    }

  public:
    /* Label associated with this break/continue statement, if any. */
    PropertyName* label() const {
        return pn_u.loopControl.label;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_BREAK) || node.isKind(PNK_CONTINUE);
        MOZ_ASSERT_IF(match, node.isArity(PN_NULLARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class BreakStatement : public LoopControlStatement
{
  public:
    BreakStatement(PropertyName* label, const TokenPos& pos)
      : LoopControlStatement(PNK_BREAK, label, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_BREAK);
        MOZ_ASSERT_IF(match, node.isArity(PN_NULLARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class ContinueStatement : public LoopControlStatement
{
  public:
    ContinueStatement(PropertyName* label, const TokenPos& pos)
      : LoopControlStatement(PNK_CONTINUE, label, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CONTINUE);
        MOZ_ASSERT_IF(match, node.isArity(PN_NULLARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class DebuggerStatement : public ParseNode
{
  public:
    explicit DebuggerStatement(const TokenPos& pos)
      : ParseNode(PNK_DEBUGGER, JSOP_NOP, PN_NULLARY, pos)
    { }
};

class ConditionalExpression : public ParseNode
{
  public:
    ConditionalExpression(ParseNode* condition, ParseNode* thenExpr, ParseNode* elseExpr)
      : ParseNode(PNK_CONDITIONAL, JSOP_NOP, PN_TERNARY,
                  TokenPos(condition->pn_pos.begin, elseExpr->pn_pos.end))
    {
        MOZ_ASSERT(condition);
        MOZ_ASSERT(thenExpr);
        MOZ_ASSERT(elseExpr);
        pn_u.ternary.kid1 = condition;
        pn_u.ternary.kid2 = thenExpr;
        pn_u.ternary.kid3 = elseExpr;
    }

    ParseNode& condition() const {
        return *pn_u.ternary.kid1;
    }

    ParseNode& thenExpression() const {
        return *pn_u.ternary.kid2;
    }

    ParseNode& elseExpression() const {
        return *pn_u.ternary.kid3;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CONDITIONAL);
        MOZ_ASSERT_IF(match, node.isArity(PN_TERNARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class ThisLiteral : public UnaryNode
{
  public:
    ThisLiteral(const TokenPos& pos, ParseNode* thisName)
      : UnaryNode(PNK_THIS, pos, thisName)
    { }
};

class NullLiteral : public ParseNode
{
  public:
    explicit NullLiteral(const TokenPos& pos) : ParseNode(PNK_NULL, JSOP_NULL, PN_NULLARY, pos) { }
};

// This is only used internally, currently just for tagged templates.
// It represents the value 'undefined' (aka `void 0`), like NullLiteral
// represents the value 'null'.
class RawUndefinedLiteral : public ParseNode
{
  public:
    explicit RawUndefinedLiteral(const TokenPos& pos)
      : ParseNode(PNK_RAW_UNDEFINED, JSOP_UNDEFINED, PN_NULLARY, pos) { }
};

class BooleanLiteral : public ParseNode
{
  public:
    BooleanLiteral(bool b, const TokenPos& pos)
      : ParseNode(b ? PNK_TRUE : PNK_FALSE, b ? JSOP_TRUE : JSOP_FALSE, PN_NULLARY, pos)
    { }
};

class RegExpLiteral : public NullaryNode
{
  public:
    RegExpLiteral(ObjectBox* reobj, const TokenPos& pos)
      : NullaryNode(PNK_REGEXP, JSOP_REGEXP, pos)
    {
        pn_objbox = reobj;
    }

    ObjectBox* objbox() const { return pn_objbox; }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_REGEXP);
        MOZ_ASSERT_IF(match, node.isArity(PN_NULLARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_REGEXP));
        return match;
    }
};

class PropertyAccess : public ParseNode
{
  public:
    PropertyAccess(ParseNode* lhs, PropertyName* name, uint32_t begin, uint32_t end)
      : ParseNode(PNK_DOT, JSOP_NOP, PN_NAME, TokenPos(begin, end))
    {
        MOZ_ASSERT(lhs != nullptr);
        MOZ_ASSERT(name != nullptr);
        pn_u.name.expr = lhs;
        pn_u.name.atom = name;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_DOT);
        MOZ_ASSERT_IF(match, node.isArity(PN_NAME));
        return match;
    }

    ParseNode& expression() const {
        return *pn_u.name.expr;
    }

    PropertyName& name() const {
        return *pn_u.name.atom->asPropertyName();
    }

    bool isSuper() const {
        // PNK_SUPERBASE cannot result from any expression syntax.
        return expression().isKind(PNK_SUPERBASE);
    }
};

class PropertyByValue : public ParseNode
{
  public:
    PropertyByValue(ParseNode* lhs, ParseNode* propExpr, uint32_t begin, uint32_t end)
      : ParseNode(PNK_ELEM, JSOP_NOP, PN_BINARY, TokenPos(begin, end))
    {
        pn_u.binary.left = lhs;
        pn_u.binary.right = propExpr;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_ELEM);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        return match;
    }

    bool isSuper() const {
        return pn_left->isKind(PNK_SUPERBASE);
    }
};

/*
 * A CallSiteNode represents the implicit call site object argument in a TaggedTemplate.
 */
struct CallSiteNode : public ListNode {
    explicit CallSiteNode(uint32_t begin): ListNode(PNK_CALLSITEOBJ, TokenPos(begin, begin + 1)) {}

    static bool test(const ParseNode& node) {
        return node.isKind(PNK_CALLSITEOBJ);
    }

    MOZ_MUST_USE bool getRawArrayValue(JSContext* cx, MutableHandleValue vp) {
        return pn_head->getConstantValue(cx, AllowObjects, vp);
    }
};

struct ClassMethod : public BinaryNode {
    /*
     * Method defintions often keep a name and function body that overlap,
     * so explicitly define the beginning and end here.
     */
    ClassMethod(ParseNode* name, ParseNode* body, JSOp op, bool isStatic)
      : BinaryNode(PNK_CLASSMETHOD, op, TokenPos(name->pn_pos.begin, body->pn_pos.end), name, body)
    {
        pn_u.binary.isStatic = isStatic;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASSMETHOD);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        return match;
    }

    ParseNode& name() const {
        return *pn_u.binary.left;
    }
    ParseNode& method() const {
        return *pn_u.binary.right;
    }
    bool isStatic() const {
        return pn_u.binary.isStatic;
    }
};

struct ClassNames : public BinaryNode {
    ClassNames(ParseNode* outerBinding, ParseNode* innerBinding, const TokenPos& pos)
      : BinaryNode(PNK_CLASSNAMES, JSOP_NOP, pos, outerBinding, innerBinding)
    {
        MOZ_ASSERT_IF(outerBinding, outerBinding->isKind(PNK_NAME));
        MOZ_ASSERT(innerBinding->isKind(PNK_NAME));
        MOZ_ASSERT_IF(outerBinding, innerBinding->pn_atom == outerBinding->pn_atom);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASSNAMES);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
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
    ParseNode* outerBinding() const {
        return pn_u.binary.left;
    }
    ParseNode* innerBinding() const {
        return pn_u.binary.right;
    }
};

struct ClassNode : public TernaryNode {
    ClassNode(ParseNode* names, ParseNode* heritage, ParseNode* methodsOrBlock,
              const TokenPos& pos)
      : TernaryNode(PNK_CLASS, names, heritage, methodsOrBlock, pos)
    {
        MOZ_ASSERT_IF(names, names->is<ClassNames>());
        MOZ_ASSERT(methodsOrBlock->is<LexicalScopeNode>() ||
                   methodsOrBlock->isKind(PNK_CLASSMETHODLIST));
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASS);
        MOZ_ASSERT_IF(match, node.isArity(PN_TERNARY));
        return match;
    }

    ClassNames* names() const {
        return pn_kid1 ? &pn_kid1->as<ClassNames>() : nullptr;
    }
    ParseNode* heritage() const {
        return pn_kid2;
    }
    ParseNode* methodList() const {
        if (pn_kid3->isKind(PNK_CLASSMETHODLIST))
            return pn_kid3;

        MOZ_ASSERT(pn_kid3->is<LexicalScopeNode>());
        ParseNode* list = pn_kid3->scopeBody();
        MOZ_ASSERT(list->isKind(PNK_CLASSMETHODLIST));
        return list;
    }
    Handle<LexicalScope::Data*> scopeBindings() const {
        MOZ_ASSERT(pn_kid3->is<LexicalScopeNode>());
        return pn_kid3->scopeBindings();
    }
};

#ifdef DEBUG
void DumpParseTree(ParseNode* pn, GenericPrinter& out, int indent = 0);
#endif

class ParseNodeAllocator
{
  public:
    explicit ParseNodeAllocator(JSContext* cx, LifoAlloc& alloc)
      : cx(cx), alloc(alloc), freelist(nullptr)
    {}

    void* allocNode();
    void freeNode(ParseNode* pn);
    ParseNode* freeTree(ParseNode* pn);
    void prepareNodeForMutation(ParseNode* pn);

  private:
    JSContext* cx;
    LifoAlloc& alloc;
    ParseNode* freelist;
};

inline bool
ParseNode::isConstant()
{
    switch (pn_type) {
      case PNK_NUMBER:
      case PNK_STRING:
      case PNK_TEMPLATE_STRING:
      case PNK_NULL:
      case PNK_RAW_UNDEFINED:
      case PNK_FALSE:
      case PNK_TRUE:
        return true;
      case PNK_ARRAY:
      case PNK_OBJECT:
        return !(pn_xflags & PNX_NONCONST);
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

enum FunctionSyntaxKind
{
    // A non-arrow function expression that is a PrimaryExpression and *also* a
    // complete AssignmentExpression.  For example, in
    //
    //   var x = (function y() {});
    //
    // |y| is such a function expression.
    AssignmentExpression,

    // A non-arrow function expression that is a PrimaryExpression but *not* a
    // complete AssignmentExpression.  For example, in
    //
    //   var x = (1 + function y() {});
    //
    // |y| is such a function expression.
    PrimaryExpression,

    // A named function appearing as a Statement.
    Statement,

    Arrow,
    Method,
    ClassConstructor,
    DerivedClassConstructor,
    Getter,
    GetterNoExpressionClosure,
    Setter,
    SetterNoExpressionClosure
};

static inline bool
IsFunctionExpression(FunctionSyntaxKind kind)
{
    return kind == AssignmentExpression || kind == PrimaryExpression;
}

static inline bool
IsConstructorKind(FunctionSyntaxKind kind)
{
    return kind == ClassConstructor || kind == DerivedClassConstructor;
}

static inline bool
IsGetterKind(FunctionSyntaxKind kind)
{
    return kind == Getter || kind == GetterNoExpressionClosure;
}

static inline bool
IsSetterKind(FunctionSyntaxKind kind)
{
    return kind == Setter || kind == SetterNoExpressionClosure;
}

static inline bool
IsMethodDefinitionKind(FunctionSyntaxKind kind)
{
    return kind == Method || IsConstructorKind(kind) ||
           IsGetterKind(kind) || IsSetterKind(kind);
}

static inline ParseNode*
FunctionFormalParametersList(ParseNode* fn, unsigned* numFormals)
{
    MOZ_ASSERT(fn->isKind(PNK_FUNCTION));
    ParseNode* argsBody = fn->pn_body;
    MOZ_ASSERT(argsBody->isKind(PNK_PARAMSBODY));
    *numFormals = argsBody->pn_count;
    if (*numFormals > 0 &&
        argsBody->last()->isKind(PNK_LEXICALSCOPE) &&
        argsBody->last()->scopeBody()->isKind(PNK_STATEMENTLIST))
    {
        (*numFormals)--;
    }
    MOZ_ASSERT(argsBody->isArity(PN_LIST));
    return argsBody->pn_head;
}

bool
IsAnonymousFunctionDefinition(ParseNode* pn);

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_ParseNode_h */

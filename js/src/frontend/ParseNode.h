/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ParseNode_h__
#define ParseNode_h__

#include "jsscript.h"

#include "frontend/ParseMaps.h"
#include "frontend/TokenStream.h"

namespace js {

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
enum ParseNodeKind {
    PNK_SEMI,
    PNK_COMMA,
    PNK_HOOK,
    PNK_COLON,
    PNK_OR,
    PNK_AND,
    PNK_BITOR,
    PNK_BITXOR,
    PNK_BITAND,
    PNK_POS,
    PNK_NEG,
    PNK_ADD,
    PNK_SUB,
    PNK_STAR,
    PNK_DIV,
    PNK_MOD,
    PNK_INC,
    PNK_DEC,
    PNK_DOT,
    PNK_LB,
    PNK_RB,
    PNK_STATEMENTLIST,
    PNK_XMLCURLYEXPR,
    PNK_RC,
    PNK_LP,
    PNK_RP,
    PNK_NAME,
    PNK_NUMBER,
    PNK_STRING,
    PNK_REGEXP,
    PNK_TRUE,
    PNK_FALSE,
    PNK_NULL,
    PNK_THIS,
    PNK_FUNCTION,
    PNK_IF,
    PNK_ELSE,
    PNK_SWITCH,
    PNK_CASE,
    PNK_DEFAULT,
    PNK_WHILE,
    PNK_DOWHILE,
    PNK_FOR,
    PNK_BREAK,
    PNK_CONTINUE,
    PNK_IN,
    PNK_VAR,
    PNK_CONST,
    PNK_WITH,
    PNK_RETURN,
    PNK_NEW,
    PNK_DELETE,
    PNK_DEFSHARP,
    PNK_USESHARP,
    PNK_TRY,
    PNK_CATCH,
    PNK_CATCHLIST,
    PNK_FINALLY,
    PNK_THROW,
    PNK_INSTANCEOF,
    PNK_DEBUGGER,
    PNK_DEFXMLNS,
    PNK_XMLSTAGO,
    PNK_XMLETAGO,
    PNK_XMLPTAGC,
    PNK_XMLTAGC,
    PNK_XMLNAME,
    PNK_XMLATTR,
    PNK_XMLSPACE,
    PNK_XMLTEXT,
    PNK_XMLCOMMENT,
    PNK_XMLCDATA,
    PNK_XMLPI,
    PNK_XMLUNARY,
    PNK_AT,
    PNK_DBLCOLON,
    PNK_ANYNAME,
    PNK_DBLDOT,
    PNK_FILTER,
    PNK_XMLELEM,
    PNK_XMLLIST,
    PNK_YIELD,
    PNK_ARRAYCOMP,
    PNK_ARRAYPUSH,
    PNK_LEXICALSCOPE,
    PNK_LET,
    PNK_SEQ,
    PNK_FORIN,
    PNK_FORHEAD,
    PNK_ARGSBODY,
    PNK_UPVARS,

    /*
     * The following parse node kinds occupy contiguous ranges to enable easy
     * range-testing.
     */

    /* Equality operators. */
    PNK_STRICTEQ,
    PNK_EQ,
    PNK_STRICTNE,
    PNK_NE,

    /* Unary operators. */
    PNK_TYPEOF,
    PNK_VOID,
    PNK_NOT,
    PNK_BITNOT,

    /* Relational operators (< <= > >=). */
    PNK_LT,
    PNK_LE,
    PNK_GT,
    PNK_GE,

    /* Shift operators (<< >> >>>). */
    PNK_LSH,
    PNK_RSH,
    PNK_URSH,

    /* Assignment operators (= += -= etc.). */
    PNK_ASSIGN,
    PNK_ASSIGNMENT_START = PNK_ASSIGN,
    PNK_ADDASSIGN,
    PNK_SUBASSIGN,
    PNK_BITORASSIGN,
    PNK_BITXORASSIGN,
    PNK_BITANDASSIGN,
    PNK_LSHASSIGN,
    PNK_RSHASSIGN,
    PNK_URSHASSIGN,
    PNK_MULASSIGN,
    PNK_DIVASSIGN,
    PNK_MODASSIGN,
    PNK_ASSIGNMENT_LAST = PNK_MODASSIGN,

    PNK_LIMIT /* domain size */
};

/*
 * Label        Variant     Members
 * -----        -------     -------
 * <Definitions>
 * PNK_FUNCTION name        pn_funbox: ptr to js::FunctionBox holding function
 *                            object containing arg and var properties.  We
 *                            create the function object at parse (not emit)
 *                            time to specialize arg and var bytecodes early.
 *                          pn_body: PNK_UPVARS if the function's source body
 *                                     depends on outer names,
 *                                   PNK_ARGSBODY if formal parameters,
 *                                   PNK_STATEMENTLIST node for function body
 *                                     statements,
 *                                   PNK_RETURN for expression closure, or
 *                                   PNK_SEQ for expression closure with
 *                                     destructured formal parameters
 *                          pn_cookie: static level and var index for function
 *                          pn_dflags: PND_* definition/use flags (see below)
 *                          pn_blockid: block id number
 * PNK_ARGSBODY list        list of formal parameters followed by
 *                            PNK_STATEMENTLIST node for function body
 *                            statements as final element
 *                          pn_count: 1 + number of formal parameters
 * PNK_UPVARS   nameset     pn_names: lexical dependencies (js::Definitions)
 *                            defined in enclosing scopes, or ultimately not
 *                            defined (free variables, either global property
 *                            references or reference errors).
 *                          pn_tree: PNK_ARGSBODY or PNK_STATEMENTLIST node
 *
 * <Statements>
 * PNK_STATEMENTLIST list   pn_head: list of pn_count statements
 * PNK_IF       ternary     pn_kid1: cond, pn_kid2: then, pn_kid3: else or null.
 *                            In body of a comprehension or desugared generator
 *                            expression, pn_kid2 is PNK_YIELD, PNK_ARRAYPUSH,
 *                            or (if the push was optimized away) empty
 *                            PNK_STATEMENTLIST.
 * PNK_SWITCH   binary      pn_left: discriminant
 *                          pn_right: list of PNK_CASE nodes, with at most one
 *                            PNK_DEFAULT node, or if there are let bindings
 *                            in the top level of the switch body's cases, a
 *                            PNK_LEXICALSCOPE node that contains the list of
 *                            PNK_CASE nodes.
 * PNK_CASE,    binary      pn_left: case expr
 *                          pn_right: PNK_STATEMENTLIST node for this case's
 *                            statements
 * PNK_DEFAULT  binary      pn_left: null
 *                          pn_right: PNK_STATEMENTLIST node for this default's
 *                            statements
 *                          pn_val: constant value if lookup or table switch
 * PNK_WHILE    binary      pn_left: cond, pn_right: body
 * PNK_DOWHILE  binary      pn_left: body, pn_right: cond
 * PNK_FOR      binary      pn_left: either PNK_FORIN (for-in statement) or
 *                            PNK_FORHEAD (for(;;) statement)
 *                          pn_right: body
 * PNK_FORIN    ternary     pn_kid1:  PNK_VAR to left of 'in', or NULL
 *                            its pn_xflags may have PNX_POPVAR
 *                            and PNX_FORINVAR bits set
 *                          pn_kid2: PNK_NAME or destructuring expr
 *                            to left of 'in'; if pn_kid1, then this
 *                            is a clone of pn_kid1->pn_head
 *                          pn_kid3: object expr to right of 'in'
 * PNK_FORHEAD  ternary     pn_kid1:  init expr before first ';' or NULL
 *                          pn_kid2:  cond expr before second ';' or NULL
 *                          pn_kid3:  update expr after second ';' or NULL
 * PNK_THROW    unary       pn_op: JSOP_THROW, pn_kid: exception
 * PNK_TRY      ternary     pn_kid1: try block
 *                          pn_kid2: null or PNK_CATCHLIST list of
 *                          PNK_LEXICALSCOPE nodes, each with pn_expr pointing
 *                          to a PNK_CATCH node
 *                          pn_kid3: null or finally block
 * PNK_CATCH    ternary     pn_kid1: PNK_NAME, PNK_RB, or PNK_RC catch var node
 *                                   (PNK_RB or PNK_RC if destructuring)
 *                          pn_kid2: null or the catch guard expression
 *                          pn_kid3: catch block statements
 * PNK_BREAK    name        pn_atom: label or null
 * PNK_CONTINUE name        pn_atom: label or null
 * PNK_WITH     binary      pn_left: head expr, pn_right: body
 * PNK_VAR,     list        pn_head: list of PNK_NAME or PNK_ASSIGN nodes
 * PNK_CONST                         each name node has either
 *                                     pn_used: false
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
 * PNK_COLON    name        pn_atom: label, pn_expr: labeled statement
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
 * PNK_MODASSIGN
 * PNK_HOOK     ternary     pn_kid1: cond, pn_kid2: then, pn_kid3: else
 * PNK_OR       binary      pn_left: first in || chain, pn_right: rest of chain
 * PNK_AND      binary      pn_left: first in && chain, pn_right: rest of chain
 * PNK_BITOR    binary      pn_left: left-assoc | expr, pn_right: ^ expr
 * PNK_BITXOR   binary      pn_left: left-assoc ^ expr, pn_right: & expr
 * PNK_BITAND   binary      pn_left: left-assoc & expr, pn_right: EQ expr
 *
 * PNK_EQ,      binary      pn_left: left-assoc EQ expr, pn_right: REL expr
 * PNK_NE,
 * PNK_STRICTEQ,
 * PNK_STRICTNE
 * PNK_LT,      binary      pn_left: left-assoc REL expr, pn_right: SH expr
 * PNK_LE,
 * PNK_GT,
 * PNK_GE
 * PNK_LSH,     binary      pn_left: left-assoc SH expr, pn_right: ADD expr
 * PNK_RSH,
 * PNK_URSH
 * PNK_ADD      binary      pn_left: left-assoc ADD expr, pn_right: MUL expr
 *                          pn_xflags: if a left-associated binary PNK_ADD
 *                            tree has been flattened into a list (see above
 *                            under <Expressions>), pn_xflags will contain
 *                            PNX_STRCAT if at least one list element is a
 *                            string literal (PNK_STRING); if such a list has
 *                            any non-string, non-number term, pn_xflags will
 *                            contain PNX_CANTFOLD.
 * PNK_SUB      binary      pn_left: left-assoc SH expr, pn_right: ADD expr
 * PNK_STAR,    binary      pn_left: left-assoc MUL expr, pn_right: UNARY expr
 * PNK_DIV,                 pn_op: JSOP_MUL, JSOP_DIV, JSOP_MOD
 * PNK_MOD
 * PNK_POS,     unary       pn_kid: UNARY expr
 * PNK_NEG
 * PNK_TYPEOF,  unary       pn_kid: UNARY expr
 * PNK_VOID,
 * PNK_NOT,
 * PNK_BITNOT
 * PNK_INC,     unary       pn_kid: MEMBER expr
 * PNK_DEC
 * PNK_NEW      list        pn_head: list of ctor, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          ctor is a MEMBER expr
 * PNK_DELETE   unary       pn_kid: MEMBER expr
 * PNK_DOT,     name        pn_expr: MEMBER expr to left of .
 * PNK_DBLDOT               pn_atom: name to right of .
 * PNK_LB       binary      pn_left: MEMBER expr to left of [
 *                          pn_right: expr between [ and ]
 * PNK_LP       list        pn_head: list of call, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          call is a MEMBER expr naming a callable object
 * PNK_RB       list        pn_head: list of pn_count array element exprs
 *                          [,,] holes are represented by PNK_COMMA nodes
 *                          pn_xflags: PN_ENDCOMMA if extra comma at end
 * PNK_RC       list        pn_head: list of pn_count binary PNK_COLON nodes
 * PNK_COLON    binary      key-value pair in object initializer or
 *                          destructuring lhs
 *                          pn_left: property id, pn_right: value
 *                          var {x} = object destructuring shorthand shares
 *                          PN_NAME node for x on left and right of PNK_COLON
 *                          node in PNK_RC's list, has PNX_DESTRUCT flag
 * PNK_DEFSHARP unary       pn_num: jsint value of n in #n=
 *                          pn_kid: primary function, paren, name, object or
 *                                  array literal expressions
 * PNK_USESHARP nullary     pn_num: jsint value of n in #n#
 * PNK_NAME,    name        pn_atom: name, string, or object atom
 * PNK_STRING,              pn_op: JSOP_NAME, JSOP_STRING, or JSOP_OBJECT, or
 *                                 JSOP_REGEXP
 * PNK_REGEXP               If JSOP_NAME, pn_op may be JSOP_*ARG or JSOP_*VAR
 *                          with pn_cookie telling (staticLevel, slot) (see
 *                          jsscript.h's UPVAR macros) and pn_dflags telling
 *                          const-ness and static analysis results
 * PNK_NAME     name        If pn_used, PNK_NAME uses the lexdef member instead
 *                          of the expr member it overlays
 * PNK_NUMBER   dval        pn_dval: double value of numeric literal
 * PNK_TRUE,    nullary     pn_op: JSOp bytecode
 * PNK_FALSE,
 * PNK_NULL,
 * PNK_THIS
 *
 * <E4X node descriptions>
 * PNK_XMLUNARY unary       pn_kid: PNK_AT, PNK_ANYNAME, or PNK_DBLCOLON node
 *                          pn_op: JSOP_XMLNAME, JSOP_BINDXMLNAME, or
 *                                 JSOP_SETXMLNAME
 * PNK_DEFXMLNS name        pn_kid: namespace expr
 * PNK_FILTER   binary      pn_left: container expr, pn_right: filter expr
 * PNK_DBLDOT   binary      pn_left: container expr, pn_right: selector expr
 * PNK_ANYNAME  nullary     pn_op: JSOP_ANYNAME
 *                          pn_atom: cx->runtime->atomState.starAtom
 * PNK_AT       unary       pn_op: JSOP_TOATTRNAME; pn_kid attribute id/expr
 * PNK_DBLCOLON binary      pn_op: JSOP_QNAME
 *                          pn_left: PNK_ANYNAME or PNK_NAME node
 *                          pn_right: PNK_STRING "*" node, or expr within []
 *              name        pn_op: JSOP_QNAMECONST
 *                          pn_expr: PNK_ANYNAME or PNK_NAME left operand
 *                          pn_atom: name on right of ::
 * PNK_XMLELEM  list        XML element node
 *                          pn_head: start tag, content1, ... contentN, end tag
 *                          pn_count: 2 + N where N is number of content nodes
 *                                    N may be > x.length() if {expr} embedded
 *                            After constant folding, these contents may be
 *                            concatenated into string nodes.
 * PNK_XMLLIST  list        XML list node
 *                          pn_head: content1, ... contentN
 * PNK_XMLSTAGO, list       XML start, end, and point tag contents
 * PNK_XMLETAGO,            pn_head: tag name or {expr}, ... XML attrs ...
 * PNK_XMLPTAGC
 * PNK_XMLNAME  nullary     pn_atom: XML name, with no {expr} embedded
 * PNK_XMLNAME  list        pn_head: tag name or {expr}, ... name or {expr}
 * PNK_XMLATTR, nullary     pn_atom: attribute value string; pn_op: JSOP_STRING
 * PNK_XMLCDATA,
 * PNK_XMLCOMMENT
 * PNK_XMLPI    nullary     pn_pitarget: XML processing instruction target
 *                          pn_pidata: XML PI data, or null if no data
 * PNK_XMLTEXT  nullary     pn_atom: marked-up text, or null if empty string
 * PNK_XMLCURLYEXPR unary   {expr} in XML tag or content; pn_kid is expr
 *
 * So an XML tag with no {expr} and three attributes is a list with the form:
 *
 *    (tagname attrname1 attrvalue1 attrname2 attrvalue2 attrname2 attrvalue3)
 *
 * An XML tag with embedded expressions like so:
 *
 *    <name1{expr1} name2{expr2}name3={expr3}>
 *
 * would have the form:
 *
 *    ((name1 {expr1}) (name2 {expr2} name3) {expr3})
 *
 * where () bracket a list with elements separated by spaces, and {expr} is a
 * PNK_XMLCURLYEXPR unary node with expr as its kid.
 *
 * Thus, the attribute name/value pairs occupy successive odd and even list
 * locations, where pn_head is the PNK_XMLNAME node at list location 0.  The
 * parser builds the same sort of structures for elements:
 *
 *    <a x={x}>Hi there!<b y={y}>How are you?</b><answer>{x + y}</answer></a>
 *
 * translates to:
 *
 *    ((a x {x}) 'Hi there!' ((b y {y}) 'How are you?') ((answer) {x + y}))
 *
 * <Non-E4X node descriptions, continued>
 *
 * Label              Variant   Members
 * -----              -------   -------
 * PNK_LEXICALSCOPE   name      pn_op: JSOP_LEAVEBLOCK or JSOP_LEAVEBLOCKEXPR
 *                              pn_objbox: block object in ObjectBox holder
 *                              pn_expr: block body
 * PNK_ARRAYCOMP      list      pn_count: 1
 *                              pn_head: list of 1 element, which is block
 *                                enclosing for loop(s) and optionally
 *                                if-guarded PNK_ARRAYPUSH
 * PNK_ARRAYPUSH      unary     pn_op: JSOP_ARRAYCOMP
 *                              pn_kid: array comprehension expression
 */
enum ParseNodeArity {
    PN_NULLARY,                         /* 0 kids, only pn_atom/pn_dval/etc. */
    PN_UNARY,                           /* one kid, plus a couple of scalars */
    PN_BINARY,                          /* two kids, plus a couple of scalars */
    PN_TERNARY,                         /* three kids */
    PN_FUNC,                            /* function definition node */
    PN_LIST,                            /* generic singly linked list */
    PN_NAME,                            /* name use or definition node */
    PN_NAMESET                          /* AtomDefnMapPtr + ParseNode ptr */
};

struct Definition;

struct ParseNode {
  private:
    uint32              pn_type   : 16, /* PNK_* type */
                        pn_op     : 8,  /* see JSOp enum and jsopcode.tbl */
                        pn_arity  : 5,  /* see ParseNodeArity enum */
                        pn_parens : 1,  /* this expr was enclosed in parens */
                        pn_used   : 1,  /* name node is on a use-chain */
                        pn_defn   : 1;  /* this node is a Definition */

  public:
    ParseNode(ParseNodeKind kind, JSOp op, ParseNodeArity arity)
      : pn_type(kind), pn_op(op), pn_arity(arity), pn_parens(0), pn_used(0), pn_defn(0),
        pn_offset(0), pn_next(NULL), pn_link(NULL)
    {
        JS_ASSERT(kind < PNK_LIMIT);
        pn_pos.begin.index = 0;
        pn_pos.begin.lineno = 0;
        pn_pos.end.index = 0;
        pn_pos.end.lineno = 0;
        memset(&pn_u, 0, sizeof pn_u);
    }

    ParseNode(ParseNodeKind kind, JSOp op, ParseNodeArity arity, const TokenPos &pos)
      : pn_type(kind), pn_op(op), pn_arity(arity), pn_parens(0), pn_used(0), pn_defn(0),
        pn_pos(pos), pn_offset(0), pn_next(NULL), pn_link(NULL)
    {
        JS_ASSERT(kind < PNK_LIMIT);
        memset(&pn_u, 0, sizeof pn_u);
    }

    JSOp getOp() const                     { return JSOp(pn_op); }
    void setOp(JSOp op)                    { pn_op = op; }
    bool isOp(JSOp op) const               { return getOp() == op; }

    ParseNodeKind getKind() const {
        JS_ASSERT(pn_type < PNK_LIMIT);
        return ParseNodeKind(pn_type);
    }
    void setKind(ParseNodeKind kind) {
        JS_ASSERT(kind < PNK_LIMIT);
        pn_type = kind;
    }
    bool isKind(ParseNodeKind kind) const  { return getKind() == kind; }

    ParseNodeArity getArity() const        { return ParseNodeArity(pn_arity); }
    bool isArity(ParseNodeArity a) const   { return getArity() == a; }
    void setArity(ParseNodeArity a)        { pn_arity = a; }

    bool isXMLNameOp() const {
        ParseNodeKind kind = getKind();
        return kind == PNK_ANYNAME || kind == PNK_AT || kind == PNK_DBLCOLON;
    }
    bool isAssignment() const {
        ParseNodeKind kind = getKind();
        return PNK_ASSIGNMENT_START <= kind && kind <= PNK_ASSIGNMENT_LAST;
    }

    bool isXMLPropertyIdentifier() const {
        ParseNodeKind kind = getKind();
        return kind == PNK_ANYNAME || kind == PNK_AT || kind == PNK_DBLCOLON;
    }

    bool isXMLItem() const {
        ParseNodeKind kind = getKind();
        return kind == PNK_XMLCOMMENT || kind == PNK_XMLCDATA || kind == PNK_XMLPI ||
               kind == PNK_XMLELEM || kind == PNK_XMLLIST;
    }

    /* Boolean attributes. */
    bool isInParens() const                { return pn_parens; }
    void setInParens(bool enabled)         { pn_parens = enabled; }
    bool isUsed() const                    { return pn_used; }
    void setUsed(bool enabled)             { pn_used = enabled; }
    bool isDefn() const                    { return pn_defn; }
    void setDefn(bool enabled)             { pn_defn = enabled; }

    TokenPos            pn_pos;         /* two 16-bit pairs here, for 64 bits */
    int32               pn_offset;      /* first generated bytecode offset */
    ParseNode           *pn_next;       /* intrinsic link in parent PN_LIST */
    ParseNode           *pn_link;       /* def/use link (alignment freebie);
                                           also links FunctionBox::methods
                                           lists of would-be |this| methods */

    union {
        struct {                        /* list of next-linked nodes */
            ParseNode   *head;          /* first node in list */
            ParseNode   **tail;         /* ptr to ptr to last node in list */
            uint32      count;          /* number of nodes in list */
            uint32      xflags:12,      /* extra flags, see below */
                        blockid:20;     /* see name variant below */
        } list;
        struct {                        /* ternary: if, for(;;), ?: */
            ParseNode   *kid1;          /* condition, discriminant, etc. */
            ParseNode   *kid2;          /* then-part, case list, etc. */
            ParseNode   *kid3;          /* else-part, default case, etc. */
        } ternary;
        struct {                        /* two kids if binary */
            ParseNode   *left;
            ParseNode   *right;
            Value       *pval;          /* switch case value */
            uintN       iflags;         /* JSITER_* flags for PNK_FOR node */
        } binary;
        struct {                        /* one kid if unary */
            ParseNode   *kid;
            jsint       num;            /* -1 or sharp variable number */
            JSBool      hidden;         /* hidden genexp-induced JSOP_YIELD
                                           or directive prologue member (as
                                           pn_prologue) */
        } unary;
        struct {                        /* name, labeled statement, etc. */
            union {
                JSAtom        *atom;    /* lexical name or label atom */
                FunctionBox   *funbox;  /* function object */
                ObjectBox     *objbox;  /* block or regexp object */
            };
            union {
                ParseNode    *expr;     /* function body, var initializer, or
                                           base object of PNK_DOT */
                Definition   *lexdef;   /* lexical definition for this use */
            };
            UpvarCookie cookie;         /* upvar cookie with absolute frame
                                           level (not relative skip), possibly
                                           in current frame */
            uint32      dflags:12,      /* definition/use flags, see below */
                        blockid:20;     /* block number, for subset dominance
                                           computation */
        } name;
        struct {                        /* lexical dependencies + sub-tree */
            AtomDefnMapPtr   defnMap;
            ParseNode        *tree;     /* sub-tree containing name uses */
        } nameset;
        struct {                        /* PN_NULLARY variant for E4X XML PI */
            PropertyName     *target;   /* target in <?target data?> */
            JSAtom           *data;     /* data (or null) in <?target data?> */
        } xmlpi;
        jsdouble        dval;           /* aligned numeric literal value */
    } pn_u;

#define pn_funbox       pn_u.name.funbox
#define pn_body         pn_u.name.expr
#define pn_cookie       pn_u.name.cookie
#define pn_dflags       pn_u.name.dflags
#define pn_blockid      pn_u.name.blockid
#define pn_index        pn_u.name.blockid /* reuse as object table index */
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
#define pn_num          pn_u.unary.num
#define pn_hidden       pn_u.unary.hidden
#define pn_prologue     pn_u.unary.hidden
#define pn_atom         pn_u.name.atom
#define pn_objbox       pn_u.name.objbox
#define pn_expr         pn_u.name.expr
#define pn_lexdef       pn_u.name.lexdef
#define pn_names        pn_u.nameset.defnMap
#define pn_tree         pn_u.nameset.tree
#define pn_dval         pn_u.dval
#define pn_pitarget     pn_u.xmlpi.target
#define pn_pidata       pn_u.xmlpi.data

  protected:
    void init(TokenKind type, JSOp op, ParseNodeArity arity) {
        pn_type = type;
        pn_op = op;
        pn_arity = arity;
        pn_parens = false;
        JS_ASSERT(!pn_used);
        JS_ASSERT(!pn_defn);
        pn_names.init();
        pn_next = pn_link = NULL;
    }

    static ParseNode *create(ParseNodeKind kind, ParseNodeArity arity, TreeContext *tc);

  public:
    /*
     * Append right to left, forming a list node.  |left| must have the given
     * kind and op, and op must be left-associative.
     */
    static ParseNode *
    append(ParseNodeKind tt, JSOp op, ParseNode *left, ParseNode *right);

    /*
     * Either append right to left, if left meets the conditions necessary to
     * append (see append), or form a binary node whose children are right and
     * left.
     */
    static ParseNode *
    newBinaryOrAppend(ParseNodeKind kind, JSOp op, ParseNode *left, ParseNode *right,
                      TreeContext *tc);

    /*
     * The pn_expr and lexdef members are arms of an unsafe union. Unless you
     * know exactly what you're doing, use only the following methods to access
     * them. For less overhead and assertions for protection, use pn->expr()
     * and pn->lexdef(). Otherwise, use pn->maybeExpr() and pn->maybeLexDef().
     */
    ParseNode *expr() const {
        JS_ASSERT(!pn_used);
        JS_ASSERT(pn_arity == PN_NAME || pn_arity == PN_FUNC);
        return pn_expr;
    }

    Definition *lexdef() const {
        JS_ASSERT(pn_used || isDeoptimized());
        JS_ASSERT(pn_arity == PN_NAME);
        return pn_lexdef;
    }

    ParseNode  *maybeExpr()   { return pn_used ? NULL : expr(); }
    Definition *maybeLexDef() { return pn_used ? lexdef() : NULL; }

/* PN_FUNC and PN_NAME pn_dflags bits. */
#define PND_LET         0x01            /* let (block-scoped) binding */
#define PND_CONST       0x02            /* const binding (orthogonal to let) */
#define PND_INITIALIZED 0x04            /* initialized declaration */
#define PND_ASSIGNED    0x08            /* set if ever LHS of assignment */
#define PND_TOPLEVEL    0x10            /* see isTopLevel() below */
#define PND_BLOCKCHILD  0x20            /* use or def is direct block child */
#define PND_GVAR        0x40            /* gvar binding, can't close over
                                           because it could be deleted */
#define PND_PLACEHOLDER 0x80            /* placeholder definition for lexdep */
#define PND_FUNARG     0x100            /* downward or upward funarg usage */
#define PND_BOUND      0x200            /* bound to a stack or global slot */
#define PND_DEOPTIMIZED 0x400           /* former pn_used name node, pn_lexdef
                                           still valid, but this use no longer
                                           optimizable via an upvar opcode */
#define PND_CLOSED      0x800           /* variable is closed over */

/* Flags to propagate from uses to definition. */
#define PND_USE2DEF_FLAGS (PND_ASSIGNED | PND_FUNARG | PND_CLOSED)

/* PN_LIST pn_xflags bits. */
#define PNX_STRCAT      0x01            /* PNK_ADD list has string term */
#define PNX_CANTFOLD    0x02            /* PNK_ADD list has unfoldable term */
#define PNX_POPVAR      0x04            /* PNK_VAR or PNK_CONST last result
                                           needs popping */
#define PNX_FORINVAR    0x08            /* PNK_VAR is left kid of PNK_FORIN node
                                           which is left kid of PNK_FOR */
#define PNX_ENDCOMMA    0x10            /* array literal has comma at end */
#define PNX_XMLROOT     0x20            /* top-most node in XML literal tree */
#define PNX_GROUPINIT   0x40            /* var [a, b] = [c, d]; unit list */
#define PNX_NEEDBRACES  0x80            /* braces necessary due to closure */
#define PNX_FUNCDEFS   0x100            /* contains top-level function statements */
#define PNX_SETCALL    0x100            /* call expression in lvalue context */
#define PNX_DESTRUCT   0x200            /* destructuring special cases:
                                           1. shorthand syntax used, at present
                                              object destructuring ({x,y}) only;
                                           2. code evaluating destructuring
                                              arguments occurs before function
                                              body */
#define PNX_HOLEY      0x400            /* array initialiser has holes */
#define PNX_NONCONST   0x800            /* initialiser has non-constants */

    uintN frameLevel() const {
        JS_ASSERT(pn_arity == PN_FUNC || pn_arity == PN_NAME);
        return pn_cookie.level();
    }

    uintN frameSlot() const {
        JS_ASSERT(pn_arity == PN_FUNC || pn_arity == PN_NAME);
        return pn_cookie.slot();
    }

    inline bool test(uintN flag) const;

    bool isLet() const          { return test(PND_LET); }
    bool isConst() const        { return test(PND_CONST); }
    bool isInitialized() const  { return test(PND_INITIALIZED); }
    bool isBlockChild() const   { return test(PND_BLOCKCHILD); }
    bool isPlaceholder() const  { return test(PND_PLACEHOLDER); }
    bool isDeoptimized() const  { return test(PND_DEOPTIMIZED); }
    bool isAssigned() const     { return test(PND_ASSIGNED); }
    bool isFunArg() const       { return test(PND_FUNARG); }
    bool isClosed() const       { return test(PND_CLOSED); }

    /*
     * True iff this definition creates a top-level binding in the overall
     * script being compiled -- that is, it affects the whole program's
     * bindings, not bindings for a specific function (unless this definition
     * is in the outermost scope in eval code, executed within a function) or
     * the properties of a specific object (through the with statement).
     *
     * NB: Function sub-statements found in overall program code and not nested
     *     within other functions are not currently top level, even though (if
     *     executed) they do create top-level bindings; there is no particular
     *     rationale for this behavior.
     */
    bool isTopLevel() const     { return test(PND_TOPLEVEL); }

    /* Defined below, see after struct Definition. */
    void setFunArg();

    void become(ParseNode *pn2);
    void clear();

    /* True if pn is a parsenode representing a literal constant. */
    bool isLiteral() const {
        return isKind(PNK_NUMBER) ||
               isKind(PNK_STRING) ||
               isKind(PNK_TRUE) ||
               isKind(PNK_FALSE) ||
               isKind(PNK_NULL);
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
     * isEscapeFreeStringLiteral, below, checks whether the node itself could be
     * a directive.
     */
    bool isStringExprStatement() const {
        if (getKind() == PNK_SEMI) {
            JS_ASSERT(pn_arity == PN_UNARY);
            ParseNode *kid = pn_kid;
            return kid && kid->getKind() == PNK_STRING && !kid->pn_parens;
        }
        return false;
    }

    /*
     * Return true if this node, known to be an unparenthesized string literal,
     * could be the string of a directive in a Directive Prologue. Directive
     * strings never contain escape sequences or line continuations.
     */
    bool isEscapeFreeStringLiteral() const {
        JS_ASSERT(isKind(PNK_STRING) && !pn_parens);

        /*
         * If the string's length in the source code is its length as a value,
         * accounting for the quotes, then it must not contain any escape
         * sequences or line continuations.
         */
        JSString *str = pn_atom;
        return (pn_pos.begin.lineno == pn_pos.end.lineno &&
                pn_pos.begin.index + str->length() + 2 == pn_pos.end.index);
    }

    /* Return true if this node appears in a Directive Prologue. */
    bool isDirectivePrologueMember() const { return pn_prologue; }

#ifdef JS_HAS_GENERATOR_EXPRS
    /*
     * True if this node is a desugared generator expression.
     */
    bool isGeneratorExpr() const {
        if (getKind() == PNK_LP) {
            ParseNode *callee = this->pn_head;
            if (callee->getKind() == PNK_FUNCTION) {
                ParseNode *body = (callee->pn_body->getKind() == PNK_UPVARS)
                                  ? callee->pn_body->pn_tree
                                  : callee->pn_body;
                if (body->getKind() == PNK_LEXICALSCOPE)
                    return true;
            }
        }
        return false;
    }

    ParseNode *generatorExpr() const {
        JS_ASSERT(isGeneratorExpr());
        ParseNode *callee = this->pn_head;
        ParseNode *body = callee->pn_body->getKind() == PNK_UPVARS
                          ? callee->pn_body->pn_tree
                          : callee->pn_body;
        JS_ASSERT(body->getKind() == PNK_LEXICALSCOPE);
        return body->pn_expr;
    }
#endif

    /*
     * Compute a pointer to the last element in a singly-linked list. NB: list
     * must be non-empty for correct PN_LAST usage -- this is asserted!
     */
    ParseNode *last() const {
        JS_ASSERT(pn_arity == PN_LIST);
        JS_ASSERT(pn_count != 0);
        return (ParseNode *)(uintptr_t(pn_tail) - offsetof(ParseNode, pn_next));
    }

    void makeEmpty() {
        JS_ASSERT(pn_arity == PN_LIST);
        pn_head = NULL;
        pn_tail = &pn_head;
        pn_count = 0;
        pn_xflags = 0;
        pn_blockid = 0;
    }

    void initList(ParseNode *pn) {
        JS_ASSERT(pn_arity == PN_LIST);
        pn_head = pn;
        pn_tail = &pn->pn_next;
        pn_count = 1;
        pn_xflags = 0;
        pn_blockid = 0;
    }

    void append(ParseNode *pn) {
        JS_ASSERT(pn_arity == PN_LIST);
        *pn_tail = pn;
        pn_tail = &pn->pn_next;
        pn_count++;
    }

    bool getConstantValue(JSContext *cx, bool strictChecks, Value *vp);
    inline bool isConstant();
};

struct NullaryNode : public ParseNode {
    static inline NullaryNode *create(ParseNodeKind kind, TreeContext *tc) {
        return (NullaryNode *)ParseNode::create(kind, PN_NULLARY, tc);
    }
};

struct UnaryNode : public ParseNode {
    UnaryNode(ParseNodeKind kind, JSOp op, const TokenPos &pos, ParseNode *kid)
      : ParseNode(kind, op, PN_UNARY, pos)
    {
        pn_kid = kid;
    }

    static inline UnaryNode *create(ParseNodeKind kind, TreeContext *tc) {
        return (UnaryNode *)ParseNode::create(kind, PN_UNARY, tc);
    }
};

struct BinaryNode : public ParseNode {
    BinaryNode(ParseNodeKind kind, JSOp op, const TokenPos &pos, ParseNode *left, ParseNode *right)
      : ParseNode(kind, op, PN_BINARY, pos)
    {
        pn_left = left;
        pn_right = right;
    }

    BinaryNode(ParseNodeKind kind, JSOp op, ParseNode *left, ParseNode *right)
      : ParseNode(kind, op, PN_BINARY, TokenPos::box(left->pn_pos, right->pn_pos))
    {
        pn_left = left;
        pn_right = right;
    }

    static inline BinaryNode *create(ParseNodeKind kind, TreeContext *tc) {
        return (BinaryNode *)ParseNode::create(kind, PN_BINARY, tc);
    }
};

struct TernaryNode : public ParseNode {
    TernaryNode(ParseNodeKind kind, JSOp op, ParseNode *kid1, ParseNode *kid2, ParseNode *kid3)
      : ParseNode(kind, op, PN_TERNARY,
                  TokenPos::make((kid1 ? kid1 : kid2 ? kid2 : kid3)->pn_pos.begin,
                                 (kid3 ? kid3 : kid2 ? kid2 : kid1)->pn_pos.end))
    {
        pn_kid1 = kid1;
        pn_kid2 = kid2;
        pn_kid3 = kid3;
    }

    static inline TernaryNode *create(ParseNodeKind kind, TreeContext *tc) {
        return (TernaryNode *)ParseNode::create(kind, PN_TERNARY, tc);
    }
};

struct ListNode : public ParseNode {
    static inline ListNode *create(ParseNodeKind kind, TreeContext *tc) {
        return (ListNode *)ParseNode::create(kind, PN_LIST, tc);
    }
};

struct FunctionNode : public ParseNode {
    static inline FunctionNode *create(ParseNodeKind kind, TreeContext *tc) {
        return (FunctionNode *)ParseNode::create(kind, PN_FUNC, tc);
    }
};

struct NameNode : public ParseNode {
    static NameNode *create(ParseNodeKind kind, JSAtom *atom, TreeContext *tc);

    inline void initCommon(TreeContext *tc);
};

struct NameSetNode : public ParseNode {
    static inline NameSetNode *create(ParseNodeKind kind, TreeContext *tc) {
        return (NameSetNode *)ParseNode::create(kind, PN_NAMESET, tc);
    }
};

struct LexicalScopeNode : public ParseNode {
    static inline LexicalScopeNode *create(ParseNodeKind kind, TreeContext *tc) {
        return (LexicalScopeNode *)ParseNode::create(kind, PN_NAME, tc);
    }
};

ParseNode *
CloneLeftHandSide(ParseNode *opn, TreeContext *tc);

/*
 * js::Definition is a degenerate subtype of the PN_FUNC and PN_NAME variants
 * of js::ParseNode, allocated only for function, var, const, and let
 * declarations that define truly lexical bindings. This means that a child of
 * a PNK_VAR list may be a Definition as well as a ParseNode. The pn_defn bit
 * is set for all Definitions, clear otherwise.
 *
 * In an upvars list, defn->resolve() is the outermost definition the
 * name may reference. If a with block or a function that calls eval encloses
 * the use, the name may end up referring to something else at runtime.
 *
 * Note that not all var declarations are definitions: JS allows multiple var
 * declarations in a function or script, but only the first creates the hoisted
 * binding. JS programmers do redeclare variables for good refactoring reasons,
 * for example:
 *
 *   function foo() {
 *       ...
 *       for (var i ...) ...;
 *       ...
 *       for (var i ...) ...;
 *       ...
 *   }
 *
 * Not all definitions bind lexical variables, alas. In global and eval code
 * var may re-declare a pre-existing property having any attributes, with or
 * without JSPROP_PERMANENT. In eval code, indeed, ECMA-262 Editions 1 through
 * 3 require function and var to bind deletable bindings. Global vars thus are
 * properties of the global object, so they can be aliased even if they can't
 * be deleted.
 *
 * Only bindings within function code may be treated as lexical, of course with
 * the caveat that hoisting means use before initialization is allowed. We deal
 * with use before declaration in one pass as follows (error checking elided):
 *
 *   for (each use of unqualified name x in parse order) {
 *       if (this use of x is a declaration) {
 *           if (x in tc->decls) {                          // redeclaring
 *               pn = allocate a PN_NAME ParseNode;
 *           } else {                                       // defining
 *               dn = lookup x in tc->lexdeps;
 *               if (dn)                                    // use before def
 *                   remove x from tc->lexdeps;
 *               else                                       // def before use
 *                   dn = allocate a PN_NAME Definition;
 *               map x to dn via tc->decls;
 *               pn = dn;
 *           }
 *           insert pn into its parent PNK_VAR/PNK_CONST list;
 *       } else {
 *           pn = allocate a ParseNode for this reference to x;
 *           dn = lookup x in tc's lexical scope chain;
 *           if (!dn) {
 *               dn = lookup x in tc->lexdeps;
 *               if (!dn) {
 *                   dn = pre-allocate a Definition for x;
 *                   map x to dn in tc->lexdeps;
 *               }
 *           }
 *           append pn to dn's use chain;
 *       }
 *   }
 *
 * See frontend/BytecodeEmitter.h for js::TreeContext and its top*Stmt,
 * decls, and lexdeps members.
 *
 * Notes:
 *
 *  0. To avoid bloating ParseNode, we steal a bit from pn_arity for pn_defn
 *     and set it on a ParseNode instead of allocating a Definition.
 *
 *  1. Due to hoisting, a definition cannot be eliminated even if its "Variable
 *     statement" (ECMA-262 12.2) can be proven to be dead code. RecycleTree in
 *     ParseNode.cpp will not recycle a node whose pn_defn bit is set.
 *
 *  2. "lookup x in tc's lexical scope chain" gives up on def/use chaining if a
 *     with statement is found along the the scope chain, which includes tc,
 *     tc->parent, etc. Thus we eagerly connect an inner function's use of an
 *     outer's var x if the var x was parsed before the inner function.
 *
 *  3. A use may be eliminated as dead by the constant folder, which therefore
 *     must remove the dead name node from its singly-linked use chain, which
 *     would mean hashing to find the definition node and searching to update
 *     the pn_link pointing at the use to be removed. This is costly, so as for
 *     dead definitions, we do not recycle dead pn_used nodes.
 *
 * At the end of parsing a function body or global or eval program, tc->lexdeps
 * holds the lexical dependencies of the parsed unit. The name to def/use chain
 * mappings are then merged into the parent tc->lexdeps.
 *
 * Thus if a later var x is parsed in the outer function satisfying an earlier
 * inner function's use of x, we will remove dn from tc->lexdeps and re-use it
 * as the new definition node in the outer function's parse tree.
 *
 * When the compiler unwinds from the outermost tc, tc->lexdeps contains the
 * definition nodes with use chains for all free variables. These are either
 * global variables or reference errors.
 *
 * We analyze whether a binding is initialized, whether the bound names is ever
 * assigned apart from its initializer, and if the bound name definition or use
 * is in a direct child of a block. These PND_* flags allow a subset dominance
 * computation telling whether an initialized var dominates its uses. An inner
 * function using only such outer vars (and formal parameters) can be optimized
 * into a flat closure. See JSOP_{GET,CALL}DSLOT.
 *
 * Another important subset dominance relation: ... { var x = ...; ... x ... }
 * where x is not assigned after initialization and not used outside the block.
 * This style is common in the absence of 'let'. Even though the var x is not
 * at top level, we can tell its initialization dominates all uses cheaply,
 * because the above one-pass algorithm sees the definition before any uses,
 * and because all uses are contained in the same block as the definition.
 *
 * We also analyze function uses to flag upward/downward funargs.  If a lambda
 * post-dominates each of its upvars' sole, inevitable (i.e. not hidden behind
 * conditions or within loops or the like) initialization or assignment; then
 * we can optimize the lambda as a flat closure (after Chez Scheme's display
 * closures).
 */
#define dn_uses         pn_link

struct Definition : public ParseNode
{
    /*
     * We store definition pointers in PN_NAMESET AtomDefnMapPtrs in the AST,
     * but due to redefinition these nodes may become uses of other
     * definitions.  This is unusual, so we simply chase the pn_lexdef link to
     * find the final definition node. See functions called from
     * js::frontend::AnalyzeFunctions.
     *
     * FIXME: MakeAssignment mutates for want of a parent link...
     */
    Definition *resolve() {
        ParseNode *pn = this;
        while (!pn->isDefn()) {
            if (pn->isAssignment()) {
                pn = pn->pn_left;
                continue;
            }
            pn = pn->lexdef();
        }
        return (Definition *) pn;
    }

    bool isFreeVar() const {
        JS_ASSERT(isDefn());
        return pn_cookie.isFree() || test(PND_GVAR);
    }

    bool isGlobal() const {
        JS_ASSERT(isDefn());
        return test(PND_GVAR);
    }

    enum Kind { VAR, CONST, LET, FUNCTION, ARG, UNKNOWN };

    bool isBindingForm() { return int(kind()) <= int(LET); }

    static const char *kindString(Kind kind);

    Kind kind() {
        if (getKind() == PNK_FUNCTION)
            return FUNCTION;
        JS_ASSERT(getKind() == PNK_NAME);
        if (isOp(JSOP_NOP))
            return UNKNOWN;
        if (isOp(JSOP_GETARG))
            return ARG;
        if (isConst())
            return CONST;
        if (isLet())
            return LET;
        return VAR;
    }
};

class ParseNodeAllocator {
  public:
    explicit ParseNodeAllocator(JSContext *cx) : cx(cx), freelist(NULL) {}

    void *allocNode();
    void freeNode(ParseNode *pn);
    ParseNode *freeTree(ParseNode *pn);
    void prepareNodeForMutation(ParseNode *pn);

  private:
    JSContext *cx;
    ParseNode *freelist;
};

inline bool
ParseNode::test(uintN flag) const
{
    JS_ASSERT(pn_defn || pn_arity == PN_FUNC || pn_arity == PN_NAME);
#ifdef DEBUG
    if ((flag & (PND_ASSIGNED | PND_FUNARG)) && pn_defn && !(pn_dflags & flag)) {
        for (ParseNode *pn = ((Definition *) this)->dn_uses; pn; pn = pn->pn_link) {
            JS_ASSERT(!pn->pn_defn);
            JS_ASSERT(!(pn->pn_dflags & flag));
        }
    }
#endif
    return !!(pn_dflags & flag);
}

inline void
ParseNode::setFunArg()
{
    /*
     * pn_defn NAND pn_used must be true, per this chart:
     *
     *   pn_defn pn_used
     *         0       0        anonymous function used implicitly, e.g. by
     *                          hidden yield in a genexp
     *         0       1        a use of a definition or placeholder
     *         1       0        a definition or placeholder
     *         1       1        error: this case must not be possible
     */
    JS_ASSERT(!(pn_defn & pn_used));
    if (pn_used)
        pn_lexdef->pn_dflags |= PND_FUNARG;
    pn_dflags |= PND_FUNARG;
}

inline void
LinkUseToDef(ParseNode *pn, Definition *dn, TreeContext *tc)
{
    JS_ASSERT(!pn->isUsed());
    JS_ASSERT(!pn->isDefn());
    JS_ASSERT(pn != dn->dn_uses);
    pn->pn_link = dn->dn_uses;
    dn->dn_uses = pn;
    dn->pn_dflags |= pn->pn_dflags & PND_USE2DEF_FLAGS;
    pn->setUsed(true);
    pn->pn_lexdef = dn;
}

struct ObjectBox {
    ObjectBox           *traceLink;
    ObjectBox           *emitLink;
    JSObject            *object;
    ObjectBox           *parent;
    uintN               index;
    bool                isFunctionBox;
};

#define JSFB_LEVEL_BITS 14

struct FunctionBox : public ObjectBox
{
    ParseNode           *node;
    FunctionBox         *siblings;
    FunctionBox         *kids;
    FunctionBox         *parent;
    ParseNode           *methods;               /* would-be methods set on this;
                                                   these nodes are linked via
                                                   pn_link, since lambdas are
                                                   neither definitions nor uses
                                                   of a binding */
    Bindings            bindings;               /* bindings for this function */
    uint32              queued:1,
                        inLoop:1,               /* in a loop in parent function */
                        level:JSFB_LEVEL_BITS;
    uint32              tcflags;

    JSFunction *function() const { return (JSFunction *) object; }

    bool joinable() const;

    /*
     * True if this function is inside the scope of a with-statement, an E4X
     * filter-expression, or a function that uses direct eval.
     */
    bool inAnyDynamicScope() const;

    /* 
     * Must this function's descendants be marked as having an extensible
     * ancestor?
     */
    bool scopeIsExtensible() const;

    /*
     * Unbrand an object being initialized or constructed if any method cannot
     * be joined to one compiler-created null closure shared among N different
     * closure environments.
     *
     * We despecialize from caching function objects, caching slots or shapes
     * instead, because an unbranded object may still have joined methods (for
     * which shape->isMethod), since PropertyCache::fill gives precedence to
     * joined methods over branded methods.
     */
    bool shouldUnbrand(uintN methods, uintN slowMethods) const;
};

struct FunctionBoxQueue {
    FunctionBox         **vector;
    size_t              head, tail;
    size_t              lengthMask;

    size_t count()  { return head - tail; }
    size_t length() { return lengthMask + 1; }

    FunctionBoxQueue()
      : vector(NULL), head(0), tail(0), lengthMask(0) { }

    bool init(uint32 count) {
        lengthMask = JS_BITMASK(JS_CEILING_LOG2W(count));
        vector = (FunctionBox **) OffTheBooks::malloc_(sizeof(FunctionBox) * length());
        return !!vector;
    }

    ~FunctionBoxQueue() { UnwantedForeground::free_(vector); }

    void push(FunctionBox *funbox) {
        if (!funbox->queued) {
            JS_ASSERT(count() < length());
            vector[head++ & lengthMask] = funbox;
            funbox->queued = true;
        }
    }

    FunctionBox *pull() {
        if (tail == head)
            return NULL;
        JS_ASSERT(tail < head);
        FunctionBox *funbox = vector[tail++ & lengthMask];
        funbox->queued = false;
        return funbox;
    }
};

} /* namespace js */

#endif /* ParseNode_h__ */

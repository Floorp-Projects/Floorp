/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsparse_h___
#define jsparse_h___
/*
 * JS parser definitions.
 */
#include "jsversion.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsatom.h"
#include "jsscan.h"

JS_BEGIN_EXTERN_C

/*
 * Parsing builds a tree of nodes that directs code generation.  This tree is
 * not a concrete syntax tree in all respects (for example, || and && are left
 * associative, but (A && B && C) translates into the right-associated tree
 * <A && <B && C>> so that code generation can emit a left-associative branch
 * around <B && C> when A is false).  Nodes are labeled by token type, with a
 * JSOp secondary label when needed:
 *
 * Label        Variant     Members
 * -----        -------     -------
 * <Definitions>
 * TOK_FUNCTION name        pn_funbox: ptr to JSFunctionBox holding function
 *                            object containing arg and var properties.  We
 *                            create the function object at parse (not emit)
 *                            time to specialize arg and var bytecodes early.
 *                          pn_body: TOK_UPVARS if the function's source body
 *                                   depends on outer names, else TOK_ARGSBODY
 *                                   if formal parameters, else TOK_LC node for
 *                                   function body statements, else TOK_RETURN
 *                                   for expression closure, else TOK_SEQ for
 *                                   expression closure with destructured
 *                                   formal parameters
 *                          pn_cookie: static level and var index for function
 *                          pn_dflags: PND_* definition/use flags (see below)
 *                          pn_blockid: block id number
 * TOK_ARGSBODY list        list of formal parameters followed by TOK_LC node
 *                            for function body statements as final element
 *                          pn_count: 1 + number of formal parameters
 * TOK_UPVARS   nameset     pn_names: lexical dependencies (JSDefinitions)
 *                            defined in enclosing scopes, or ultimately not
 *                            defined (free variables, either global property
 *                            references or reference errors).
 *                          pn_tree: TOK_ARGSBODY or TOK_LC node
 *
 * <Statements>
 * TOK_LC       list        pn_head: list of pn_count statements
 * TOK_IF       ternary     pn_kid1: cond, pn_kid2: then, pn_kid3: else or null.
 *                            In body of a comprehension or desugared generator
 *                            expression, pn_kid2 is TOK_YIELD, TOK_ARRAYPUSH,
 *                            or (if the push was optimized away) empty TOK_LC.
 * TOK_SWITCH   binary      pn_left: discriminant
 *                          pn_right: list of TOK_CASE nodes, with at most one
 *                            TOK_DEFAULT node, or if there are let bindings
 *                            in the top level of the switch body's cases, a
 *                            TOK_LEXICALSCOPE node that contains the list of
 *                            TOK_CASE nodes.
 * TOK_CASE,    binary      pn_left: case expr or null if TOK_DEFAULT
 * TOK_DEFAULT              pn_right: TOK_LC node for this case's statements
 *                          pn_val: constant value if lookup or table switch
 * TOK_WHILE    binary      pn_left: cond, pn_right: body
 * TOK_DO       binary      pn_left: body, pn_right: cond
 * TOK_FOR      binary      pn_left: either
 *                            for/in loop: a binary TOK_IN node with
 *                              pn_left:  TOK_VAR or TOK_NAME to left of 'in'
 *                                if TOK_VAR, its pn_xflags may have PNX_POPVAR
 *                                and PNX_FORINVAR bits set
 *                              pn_right: object expr to right of 'in'
 *                            for(;;) loop: a ternary TOK_RESERVED node with
 *                              pn_kid1:  init expr before first ';'
 *                              pn_kid2:  cond expr before second ';'
 *                              pn_kid3:  update expr after second ';'
 *                              any kid may be null
 *                          pn_right: body
 * TOK_THROW    unary       pn_op: JSOP_THROW, pn_kid: exception
 * TOK_TRY      ternary     pn_kid1: try block
 *                          pn_kid2: null or TOK_RESERVED list of
 *                          TOK_LEXICALSCOPE nodes, each with pn_expr pointing
 *                          to a TOK_CATCH node
 *                          pn_kid3: null or finally block
 * TOK_CATCH    ternary     pn_kid1: TOK_NAME, TOK_RB, or TOK_RC catch var node
 *                                   (TOK_RB or TOK_RC if destructuring)
 *                          pn_kid2: null or the catch guard expression
 *                          pn_kid3: catch block statements
 * TOK_BREAK    name        pn_atom: label or null
 * TOK_CONTINUE name        pn_atom: label or null
 * TOK_WITH     binary      pn_left: head expr, pn_right: body
 * TOK_VAR      list        pn_head: list of TOK_NAME or TOK_ASSIGN nodes
 *                                   each name node has
 *                                     pn_used: false
 *                                     pn_atom: variable name
 *                                     pn_expr: initializer or null
 *                                   each assignment node has
 *                                     pn_left: TOK_NAME with pn_used true and
 *                                              pn_lexdef (NOT pn_expr) set
 *                                     pn_right: initializer
 * TOK_RETURN   unary       pn_kid: return expr or null
 * TOK_SEMI     unary       pn_kid: expr or null statement
 * TOK_COLON    name        pn_atom: label, pn_expr: labeled statement
 *
 * <Expressions>
 * All left-associated binary trees of the same type are optimized into lists
 * to avoid recursion when processing expression chains.
 * TOK_COMMA    list        pn_head: list of pn_count comma-separated exprs
 * TOK_ASSIGN   binary      pn_left: lvalue, pn_right: rvalue
 *                          pn_op: JSOP_ADD for +=, etc.
 * TOK_HOOK     ternary     pn_kid1: cond, pn_kid2: then, pn_kid3: else
 * TOK_OR       binary      pn_left: first in || chain, pn_right: rest of chain
 * TOK_AND      binary      pn_left: first in && chain, pn_right: rest of chain
 * TOK_BITOR    binary      pn_left: left-assoc | expr, pn_right: ^ expr
 * TOK_BITXOR   binary      pn_left: left-assoc ^ expr, pn_right: & expr
 * TOK_BITAND   binary      pn_left: left-assoc & expr, pn_right: EQ expr
 * TOK_EQOP     binary      pn_left: left-assoc EQ expr, pn_right: REL expr
 *                          pn_op: JSOP_EQ, JSOP_NE,
 *                                 JSOP_STRICTEQ, JSOP_STRICTNE
 * TOK_RELOP    binary      pn_left: left-assoc REL expr, pn_right: SH expr
 *                          pn_op: JSOP_LT, JSOP_LE, JSOP_GT, JSOP_GE
 * TOK_SHOP     binary      pn_left: left-assoc SH expr, pn_right: ADD expr
 *                          pn_op: JSOP_LSH, JSOP_RSH, JSOP_URSH
 * TOK_PLUS,    binary      pn_left: left-assoc ADD expr, pn_right: MUL expr
 *                          pn_xflags: if a left-associated binary TOK_PLUS
 *                            tree has been flattened into a list (see above
 *                            under <Expressions>), pn_xflags will contain
 *                            PNX_STRCAT if at least one list element is a
 *                            string literal (TOK_STRING); if such a list has
 *                            any non-string, non-number term, pn_xflags will
 *                            contain PNX_CANTFOLD.
 *                          pn_
 * TOK_MINUS                pn_op: JSOP_ADD, JSOP_SUB
 * TOK_STAR,    binary      pn_left: left-assoc MUL expr, pn_right: UNARY expr
 * TOK_DIVOP                pn_op: JSOP_MUL, JSOP_DIV, JSOP_MOD
 * TOK_UNARYOP  unary       pn_kid: UNARY expr, pn_op: JSOP_NEG, JSOP_POS,
 *                          JSOP_NOT, JSOP_BITNOT, JSOP_TYPEOF, JSOP_VOID
 * TOK_INC,     unary       pn_kid: MEMBER expr
 * TOK_DEC
 * TOK_NEW      list        pn_head: list of ctor, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          ctor is a MEMBER expr
 * TOK_DELETE   unary       pn_kid: MEMBER expr
 * TOK_DOT,     name        pn_expr: MEMBER expr to left of .
 * TOK_DBLDOT               pn_atom: name to right of .
 * TOK_LB       binary      pn_left: MEMBER expr to left of [
 *                          pn_right: expr between [ and ]
 * TOK_LP       list        pn_head: list of call, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          call is a MEMBER expr naming a callable object
 * TOK_RB       list        pn_head: list of pn_count array element exprs
 *                          [,,] holes are represented by TOK_COMMA nodes
 *                          pn_xflags: PN_ENDCOMMA if extra comma at end
 * TOK_RC       list        pn_head: list of pn_count binary TOK_COLON nodes
 * TOK_COLON    binary      key-value pair in object initializer or
 *                          destructuring lhs
 *                          pn_left: property id, pn_right: value
 *                          var {x} = object destructuring shorthand shares
 *                          PN_NAME node for x on left and right of TOK_COLON
 *                          node in TOK_RC's list, has PNX_DESTRUCT flag
 * TOK_DEFSHARP unary       pn_num: jsint value of n in #n=
 *                          pn_kid: primary function, paren, name, object or
 *                                  array literal expressions
 * TOK_USESHARP nullary     pn_num: jsint value of n in #n#
 * TOK_NAME,    name        pn_atom: name, string, or object atom
 * TOK_STRING,              pn_op: JSOP_NAME, JSOP_STRING, or JSOP_OBJECT, or
 *                                 JSOP_REGEXP
 * TOK_REGEXP               If JSOP_NAME, pn_op may be JSOP_*ARG or JSOP_*VAR
 *                          with pn_cookie telling (staticLevel, slot) (see
 *                          jsscript.h's UPVAR macros) and pn_dflags telling
 *                          const-ness and static analysis results
 * TOK_NAME     name        If pn_used, TOK_NAME uses the lexdef member instead
 *                          of the expr member it overlays
 * TOK_NUMBER   dval        pn_dval: double value of numeric literal
 * TOK_PRIMARY  nullary     pn_op: JSOp bytecode
 *
 * <E4X node descriptions>
 * TOK_DEFAULT  name        pn_atom: default XML namespace string literal
 * TOK_FILTER   binary      pn_left: container expr, pn_right: filter expr
 * TOK_DBLDOT   binary      pn_left: container expr, pn_right: selector expr
 * TOK_ANYNAME  nullary     pn_op: JSOP_ANYNAME
 *                          pn_atom: cx->runtime->atomState.starAtom
 * TOK_AT       unary       pn_op: JSOP_TOATTRNAME; pn_kid attribute id/expr
 * TOK_DBLCOLON binary      pn_op: JSOP_QNAME
 *                          pn_left: TOK_ANYNAME or TOK_NAME node
 *                          pn_right: TOK_STRING "*" node, or expr within []
 *              name        pn_op: JSOP_QNAMECONST
 *                          pn_expr: TOK_ANYNAME or TOK_NAME left operand
 *                          pn_atom: name on right of ::
 * TOK_XMLELEM  list        XML element node
 *                          pn_head: start tag, content1, ... contentN, end tag
 *                          pn_count: 2 + N where N is number of content nodes
 *                                    N may be > x.length() if {expr} embedded
 *                            After constant folding, these contents may be
 *                            concatenated into string nodes.
 * TOK_XMLLIST  list        XML list node
 *                          pn_head: content1, ... contentN
 * TOK_XMLSTAGO, list       XML start, end, and point tag contents
 * TOK_XMLETAGO,            pn_head: tag name or {expr}, ... XML attrs ...
 * TOK_XMLPTAGC
 * TOK_XMLNAME  nullary     pn_atom: XML name, with no {expr} embedded
 * TOK_XMLNAME  list        pn_head: tag name or {expr}, ... name or {expr}
 * TOK_XMLATTR, nullary     pn_atom: attribute value string; pn_op: JSOP_STRING
 * TOK_XMLCDATA,
 * TOK_XMLCOMMENT
 * TOK_XMLPI    nullary     pn_atom: XML processing instruction target
 *                          pn_atom2: XML PI content, or null if no content
 * TOK_XMLTEXT  nullary     pn_atom: marked-up text, or null if empty string
 * TOK_LC       unary       {expr} in XML tag or content; pn_kid is expr
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
 * TOK_LC unary node with expr as its kid.
 *
 * Thus, the attribute name/value pairs occupy successive odd and even list
 * locations, where pn_head is the TOK_XMLNAME node at list location 0.  The
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
 * TOK_LEXICALSCOPE   name      pn_op: JSOP_LEAVEBLOCK or JSOP_LEAVEBLOCKEXPR
 *                              pn_objbox: block object in JSObjectBox holder
 *                              pn_expr: block body
 * TOK_ARRAYCOMP      list      pn_count: 1
 *                              pn_head: list of 1 element, which is block
 *                                enclosing for loop(s) and optionally
 *                                if-guarded TOK_ARRAYPUSH
 * TOK_ARRAYPUSH      unary     pn_op: JSOP_ARRAYCOMP
 *                              pn_kid: array comprehension expression
 */
typedef enum JSParseNodeArity {
    PN_NULLARY,                         /* 0 kids, only pn_atom/pn_dval/etc. */
    PN_UNARY,                           /* one kid, plus a couple of scalars */
    PN_BINARY,                          /* two kids, plus a couple of scalars */
    PN_TERNARY,                         /* three kids */
    PN_FUNC,                            /* function definition node */
    PN_LIST,                            /* generic singly linked list */
    PN_NAME,                            /* name use or definition node */
    PN_NAMESET                          /* JSAtomList + JSParseNode ptr */
} JSParseNodeArity;

struct JSDefinition;

namespace js {

struct GlobalScope {
    GlobalScope(JSContext *cx, JSObject *globalObj, JSCodeGenerator *cg)
      : globalObj(globalObj), cg(cg), defs(ContextAllocPolicy(cx))
    { }

    struct GlobalDef {
        JSAtom        *atom;        // If non-NULL, specifies the property name to add.
        JSFunctionBox *funbox;      // If non-NULL, function value for the property.
                                    // This value is only set/used if atom is non-NULL.
        uint32        knownSlot;    // If atom is NULL, this is the known shape slot.

        GlobalDef() { }
        GlobalDef(uint32 knownSlot)
          : atom(NULL), knownSlot(knownSlot)
        { }
        GlobalDef(JSAtom *atom, JSFunctionBox *box) :
          atom(atom), funbox(box)
        { }
    };

    JSObject        *globalObj;
    JSCodeGenerator *cg;

    /*
     * This is the table of global names encountered during parsing. Each
     * global name appears in the list only once, and the |names| table
     * maps back into |defs| for fast lookup.
     *
     * A definition may either specify an existing global property, or a new
     * one that must be added after compilation succeeds.
     */
    Vector<GlobalDef, 16, ContextAllocPolicy> defs;
    JSAtomList      names;
};

} /* namespace js */

struct JSParseNode {
    uint32              pn_type:16,     /* TOK_* type, see jsscan.h */
                        pn_op:8,        /* see JSOp enum and jsopcode.tbl */
                        pn_arity:5,     /* see JSParseNodeArity enum */
                        pn_parens:1,    /* this expr was enclosed in parens */
                        pn_used:1,      /* name node is on a use-chain */
                        pn_defn:1;      /* this node is a JSDefinition */

#define PN_OP(pn)    ((JSOp)(pn)->pn_op)
#define PN_TYPE(pn)  ((js::TokenKind)(pn)->pn_type)

    js::TokenPos        pn_pos;         /* two 16-bit pairs here, for 64 bits */
    int32               pn_offset;      /* first generated bytecode offset */
    JSParseNode         *pn_next;       /* intrinsic link in parent PN_LIST */
    JSParseNode         *pn_link;       /* def/use link (alignment freebie);
                                           also links JSFunctionBox::methods
                                           lists of would-be |this| methods */
    union {
        struct {                        /* list of next-linked nodes */
            JSParseNode *head;          /* first node in list */
            JSParseNode **tail;         /* ptr to ptr to last node in list */
            uint32      count;          /* number of nodes in list */
            uint32      xflags:12,      /* extra flags, see below */
                        blockid:20;     /* see name variant below */
        } list;
        struct {                        /* ternary: if, for(;;), ?: */
            JSParseNode *kid1;          /* condition, discriminant, etc. */
            JSParseNode *kid2;          /* then-part, case list, etc. */
            JSParseNode *kid3;          /* else-part, default case, etc. */
        } ternary;
        struct {                        /* two kids if binary */
            JSParseNode *left;
            JSParseNode *right;
            js::Value   *pval;          /* switch case value */
            uintN       iflags;         /* JSITER_* flags for TOK_FOR node */
        } binary;
        struct {                        /* one kid if unary */
            JSParseNode *kid;
            jsint       num;            /* -1 or sharp variable number */
            JSBool      hidden;         /* hidden genexp-induced JSOP_YIELD */
        } unary;
        struct {                        /* name, labeled statement, etc. */
            union {
                JSAtom        *atom;    /* lexical name or label atom */
                JSFunctionBox *funbox;  /* function object */
                JSObjectBox   *objbox;  /* block or regexp object */
            };
            union {
                JSParseNode  *expr;     /* function body, var initializer, or
                                           base object of TOK_DOT */
                JSDefinition *lexdef;   /* lexical definition for this use */
            };
            js::UpvarCookie cookie;     /* upvar cookie with absolute frame
                                           level (not relative skip), possibly
                                           in current frame */
            uint32      dflags:12,      /* definition/use flags, see below */
                        blockid:20;     /* block number, for subset dominance
                                           computation */
        } name;
        struct {                        /* lexical dependencies + sub-tree */
            JSAtomSet   names;          /* set of names with JSDefinitions */
            JSParseNode *tree;          /* sub-tree containing name uses */
        } nameset;
        struct {                        /* PN_NULLARY variant for E4X */
            JSAtom      *atom;          /* first atom in pair */
            JSAtom      *atom2;         /* second atom in pair or null */
        } apair;
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
#define pn_atom         pn_u.name.atom
#define pn_objbox       pn_u.name.objbox
#define pn_expr         pn_u.name.expr
#define pn_lexdef       pn_u.name.lexdef
#define pn_names        pn_u.nameset.names
#define pn_tree         pn_u.nameset.tree
#define pn_dval         pn_u.dval
#define pn_atom2        pn_u.apair.atom2

protected:
    void inline init(js::TokenKind type, JSOp op, JSParseNodeArity arity) {
        pn_type = type;
        pn_op = op;
        pn_arity = arity;
        pn_parens = false;
        JS_ASSERT(!pn_used);
        JS_ASSERT(!pn_defn);
        pn_next = pn_link = NULL;
    }

    static JSParseNode *create(JSParseNodeArity arity, JSTreeContext *tc);

public:
    static JSParseNode *newBinaryOrAppend(js::TokenKind tt, JSOp op, JSParseNode *left,
                                          JSParseNode *right, JSTreeContext *tc);

    /*
     * The pn_expr and lexdef members are arms of an unsafe union. Unless you
     * know exactly what you're doing, use only the following methods to access
     * them. For less overhead and assertions for protection, use pn->expr()
     * and pn->lexdef(). Otherwise, use pn->maybeExpr() and pn->maybeLexDef().
     */
    JSParseNode  *expr() const {
        JS_ASSERT(!pn_used);
        JS_ASSERT(pn_arity == PN_NAME || pn_arity == PN_FUNC);
        return pn_expr;
    }

    JSDefinition *lexdef() const {
        JS_ASSERT(pn_used || isDeoptimized());
        JS_ASSERT(pn_arity == PN_NAME);
        return pn_lexdef;
    }

    JSParseNode  *maybeExpr()   { return pn_used ? NULL : expr(); }
    JSDefinition *maybeLexDef() { return pn_used ? lexdef() : NULL; }

/* PN_FUNC and PN_NAME pn_dflags bits. */
#define PND_LET         0x01            /* let (block-scoped) binding */
#define PND_CONST       0x02            /* const binding (orthogonal to let) */
#define PND_INITIALIZED 0x04            /* initialized declaration */
#define PND_ASSIGNED    0x08            /* set if ever LHS of assignment */
#define PND_TOPLEVEL    0x10            /* function at top of body or prog */
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
#define PNX_STRCAT      0x01            /* TOK_PLUS list has string term */
#define PNX_CANTFOLD    0x02            /* TOK_PLUS list has unfoldable term */
#define PNX_POPVAR      0x04            /* TOK_VAR last result needs popping */
#define PNX_FORINVAR    0x08            /* TOK_VAR is left kid of TOK_IN node,
                                           which is left kid of TOK_FOR */
#define PNX_ENDCOMMA    0x10            /* array literal has comma at end */
#define PNX_XMLROOT     0x20            /* top-most node in XML literal tree */
#define PNX_GROUPINIT   0x40            /* var [a, b] = [c, d]; unit list */
#define PNX_NEEDBRACES  0x80            /* braces necessary due to closure */
#define PNX_FUNCDEFS   0x100            /* contains top-level function
                                           statements */
#define PNX_DESTRUCT   0x200            /* destructuring special cases:
                                           1. shorthand syntax used, at present
                                              object destructuring ({x,y}) only;
                                           2. code evaluating destructuring
                                              arguments occurs before function
                                              body */
#define PNX_HOLEY      0x400            /* array initialiser has holes */

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
    bool isTopLevel() const     { return test(PND_TOPLEVEL); }
    bool isBlockChild() const   { return test(PND_BLOCKCHILD); }
    bool isPlaceholder() const  { return test(PND_PLACEHOLDER); }
    bool isDeoptimized() const  { return test(PND_DEOPTIMIZED); }
    bool isAssigned() const     { return test(PND_ASSIGNED); }
    bool isFunArg() const       { return test(PND_FUNARG); }

    /* Defined below, see after struct JSDefinition. */
    void setFunArg();

    void become(JSParseNode *pn2);
    void clear();

    /* True if pn is a parsenode representing a literal constant. */
    bool isLiteral() const {
        return PN_TYPE(this) == js::TOK_NUMBER ||
               PN_TYPE(this) == js::TOK_STRING ||
               (PN_TYPE(this) == js::TOK_PRIMARY && PN_OP(this) != JSOP_THIS);
    }

    /*
     * True if this statement node could be a member of a Directive
     * Prologue.  Note that the prologue may contain strings that
     * cannot themselves be directives; that's a stricter test.
     * If Statement begins to simplify trees into this form, then
     * we'll need additional flags that we can test here.
     */
    bool isDirectivePrologueMember() const {
        if (PN_TYPE(this) == js::TOK_SEMI) {
            JS_ASSERT(pn_arity == PN_UNARY);
            JSParseNode *kid = pn_kid;
            return kid && PN_TYPE(kid) == js::TOK_STRING && !kid->pn_parens;
        }
        return false;
    }

    /*
     * True if this node, known to be a Directive Prologue member,
     * could be a directive itself.
     */
    bool isDirective() const {
        JS_ASSERT(isDirectivePrologueMember());
        JSParseNode *kid = pn_kid;
        JSString *str = ATOM_TO_STRING(kid->pn_atom);

        /*
         * Directives must contain no EscapeSequences or LineContinuations.
         * If the string's length in the source code is its length as a value,
         * accounting for the quotes, then it qualifies.
         */
        return (pn_pos.begin.lineno == pn_pos.end.lineno &&
                pn_pos.begin.index + str->length() + 2 == pn_pos.end.index);
    }

#ifdef JS_HAS_GENERATOR_EXPRS
    /*
     * True if this node is a desugared generator expression.
     */
    bool isGeneratorExpr() const {
        if (PN_TYPE(this) == js::TOK_LP) {
            JSParseNode *callee = this->pn_head;
            if (PN_TYPE(callee) == js::TOK_FUNCTION) {
                JSParseNode *body = (PN_TYPE(callee->pn_body) == js::TOK_UPVARS)
                                    ? callee->pn_body->pn_tree
                                    : callee->pn_body;
                if (PN_TYPE(body) == js::TOK_LEXICALSCOPE)
                    return true;
            }
        }
        return false;
    }

    JSParseNode *generatorExpr() const {
        JS_ASSERT(isGeneratorExpr());
        JSParseNode *callee = this->pn_head;
        JSParseNode *body = PN_TYPE(callee->pn_body) == js::TOK_UPVARS
            ? callee->pn_body->pn_tree
            : callee->pn_body;
        JS_ASSERT(PN_TYPE(body) == js::TOK_LEXICALSCOPE);
        return body->pn_expr;
    }
#endif

    /*
     * Compute a pointer to the last element in a singly-linked list. NB: list
     * must be non-empty for correct PN_LAST usage -- this is asserted!
     */
    JSParseNode *last() const {
        JS_ASSERT(pn_arity == PN_LIST);
        JS_ASSERT(pn_count != 0);
        return (JSParseNode *)((char *)pn_tail - offsetof(JSParseNode, pn_next));
    }

    void makeEmpty() {
        JS_ASSERT(pn_arity == PN_LIST);
        pn_head = NULL;
        pn_tail = &pn_head;
        pn_count = 0;
        pn_xflags = 0;
        pn_blockid = 0;
    }

    void initList(JSParseNode *pn) {
        JS_ASSERT(pn_arity == PN_LIST);
        pn_head = pn;
        pn_tail = &pn->pn_next;
        pn_count = 1;
        pn_xflags = 0;
        pn_blockid = 0;
    }

    void append(JSParseNode *pn) {
        JS_ASSERT(pn_arity == PN_LIST);
        *pn_tail = pn;
        pn_tail = &pn->pn_next;
        pn_count++;
    }
};

namespace js {

struct NullaryNode : public JSParseNode {
    static inline NullaryNode *create(JSTreeContext *tc) {
        return (NullaryNode *)JSParseNode::create(PN_NULLARY, tc);
    }
};

struct UnaryNode : public JSParseNode {
    static inline UnaryNode *create(JSTreeContext *tc) {
        return (UnaryNode *)JSParseNode::create(PN_UNARY, tc);
    }
};

struct BinaryNode : public JSParseNode {
    static inline BinaryNode *create(JSTreeContext *tc) {
        return (BinaryNode *)JSParseNode::create(PN_BINARY, tc);
    }
};

struct TernaryNode : public JSParseNode {
    static inline TernaryNode *create(JSTreeContext *tc) {
        return (TernaryNode *)JSParseNode::create(PN_TERNARY, tc);
    }
};

struct ListNode : public JSParseNode {
    static inline ListNode *create(JSTreeContext *tc) {
        return (ListNode *)JSParseNode::create(PN_LIST, tc);
    }
};

struct FunctionNode : public JSParseNode {
    static inline FunctionNode *create(JSTreeContext *tc) {
        return (FunctionNode *)JSParseNode::create(PN_FUNC, tc);
    }
};

struct NameNode : public JSParseNode {
    static NameNode *create(JSAtom *atom, JSTreeContext *tc);

    void inline initCommon(JSTreeContext *tc);
};

struct NameSetNode : public JSParseNode {
    static inline NameSetNode *create(JSTreeContext *tc) {
        return (NameSetNode *)JSParseNode::create(PN_NAMESET, tc);
    }
};

struct LexicalScopeNode : public JSParseNode {
    static inline LexicalScopeNode *create(JSTreeContext *tc) {
        return (LexicalScopeNode *)JSParseNode::create(PN_NAME, tc);
    }
};

} /* namespace js */

/*
 * JSDefinition is a degenerate subtype of the PN_FUNC and PN_NAME variants of
 * JSParseNode, allocated only for function, var, const, and let declarations
 * that define truly lexical bindings. This means that a child of a TOK_VAR
 * list may be a JSDefinition instead of a JSParseNode. The pn_defn bit is set
 * for all JSDefinitions, clear otherwise.
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
 *               pn = allocate a PN_NAME JSParseNode;
 *           } else {                                       // defining
 *               dn = lookup x in tc->lexdeps;
 *               if (dn)                                    // use before def
 *                   remove x from tc->lexdeps;
 *               else                                       // def before use
 *                   dn = allocate a PN_NAME JSDefinition;
 *               map x to dn via tc->decls;
 *               pn = dn;
 *           }
 *           insert pn into its parent TOK_VAR list;
 *       } else {
 *           pn = allocate a JSParseNode for this reference to x;
 *           dn = lookup x in tc's lexical scope chain;
 *           if (!dn) {
 *               dn = lookup x in tc->lexdeps;
 *               if (!dn) {
 *                   dn = pre-allocate a JSDefinition for x;
 *                   map x to dn in tc->lexdeps;
 *               }
 *           }
 *           append pn to dn's use chain;
 *       }
 *   }
 *
 * See jsemit.h for JSTreeContext and its top*Stmt, decls, and lexdeps members.
 *
 * Notes:
 *
 *  0. To avoid bloating JSParseNode, we steal a bit from pn_arity for pn_defn
 *     and set it on a JSParseNode instead of allocating a JSDefinition.
 *
 *  1. Due to hoisting, a definition cannot be eliminated even if its "Variable
 *     statement" (ECMA-262 12.2) can be proven to be dead code. RecycleTree in
 *     jsparse.cpp will not recycle a node whose pn_defn bit is set.
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
 * We also analyze function uses to flag upward/downward funargs, optimizing
 * Algol-like (not passed as funargs, only ever called) lightweight functions
 * using cx->display. See JSOP_{GET,CALL}UPVAR.
 *
 * This means that closure optimizations may be frustrated by with, eval, or
 * assignment to an outer var. Such hard cases require heavyweight functions
 * and JSOP_NAME, etc.
 */
#define dn_uses         pn_link

struct JSDefinition : public JSParseNode
{
    /*
     * We store definition pointers in PN_NAMESET JSAtomLists in the AST, but
     * due to redefinition these nodes may become uses of other definitions.
     * This is unusual, so we simply chase the pn_lexdef link to find the final
     * definition node. See methods called from Parser::analyzeFunctions.
     *
     * FIXME: MakeAssignment mutates for want of a parent link...
     */
    JSDefinition *resolve() {
        JSParseNode *pn = this;
        while (!pn->pn_defn) {
            if (pn->pn_type == js::TOK_ASSIGN) {
                pn = pn->pn_left;
                continue;
            }
            pn = pn->lexdef();
        }
        return (JSDefinition *) pn;
    }

    bool isFreeVar() const {
        JS_ASSERT(pn_defn);
        return pn_cookie.isFree() || test(PND_GVAR);
    }

    // Grr, windows.h or something under it #defines CONST...
#ifdef CONST
# undef CONST
#endif
    enum Kind { VAR, CONST, LET, FUNCTION, ARG, UNKNOWN };

    bool isBindingForm() { return int(kind()) <= int(LET); }

    static const char *kindString(Kind kind);

    Kind kind() {
        if (PN_TYPE(this) == js::TOK_FUNCTION)
            return FUNCTION;
        JS_ASSERT(PN_TYPE(this) == js::TOK_NAME);
        if (PN_OP(this) == JSOP_NOP)
            return UNKNOWN;
        if (PN_OP(this) == JSOP_GETARG)
            return ARG;
        if (isConst())
            return CONST;
        if (isLet())
            return LET;
        return VAR;
    }
};

inline bool
JSParseNode::test(uintN flag) const
{
    JS_ASSERT(pn_defn || pn_arity == PN_FUNC || pn_arity == PN_NAME);
#ifdef DEBUG
    if ((flag & (PND_ASSIGNED | PND_FUNARG)) && pn_defn && !(pn_dflags & flag)) {
        for (JSParseNode *pn = ((JSDefinition *) this)->dn_uses; pn; pn = pn->pn_link) {
            JS_ASSERT(!pn->pn_defn);
            JS_ASSERT(!(pn->pn_dflags & flag));
        }
    }
#endif
    return !!(pn_dflags & flag);
}

inline void
JSParseNode::setFunArg()
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

struct JSObjectBox {
    JSObjectBox         *traceLink;
    JSObjectBox         *emitLink;
    JSObject            *object;
};

#define JSFB_LEVEL_BITS 14

struct JSFunctionBox : public JSObjectBox
{
    JSParseNode         *node;
    JSFunctionBox       *siblings;
    JSFunctionBox       *kids;
    JSFunctionBox       *parent;
    JSParseNode         *methods;               /* would-be methods set on this;
                                                   these nodes are linked via
                                                   pn_link, since lambdas are
                                                   neither definitions nor uses
                                                   of a binding */
    uint32              queued:1,
                        inLoop:1,               /* in a loop in parent function */
                        level:JSFB_LEVEL_BITS;
    uint32              tcflags;

    bool joinable() const;

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

struct JSFunctionBoxQueue {
    JSFunctionBox       **vector;
    size_t              head, tail;
    size_t              lengthMask;

    size_t count()  { return head - tail; }
    size_t length() { return lengthMask + 1; }

    JSFunctionBoxQueue()
      : vector(NULL), head(0), tail(0), lengthMask(0) { }

    bool init(uint32 count) {
        lengthMask = JS_BITMASK(JS_CeilingLog2(count));
        vector = new JSFunctionBox*[length()];
        return !!vector;
    }

    ~JSFunctionBoxQueue() { delete[] vector; }

    void push(JSFunctionBox *funbox) {
        if (!funbox->queued) {
            JS_ASSERT(count() < length());
            vector[head++ & lengthMask] = funbox;
            funbox->queued = true;
        }
    }

    JSFunctionBox *pull() {
        if (tail == head)
            return NULL;
        JS_ASSERT(tail < head);
        JSFunctionBox *funbox = vector[tail++ & lengthMask];
        funbox->queued = false;
        return funbox;
    }
};

#define NUM_TEMP_FREELISTS      6U      /* 32 to 2048 byte size classes (32 bit) */

typedef struct BindData BindData;

namespace js {

struct Parser : private js::AutoGCRooter
{
    JSContext           * const context; /* FIXME Bug 551291: use AutoGCRooter::context? */
    JSAtomListElement   *aleFreeList;
    void                *tempFreeList[NUM_TEMP_FREELISTS];
    js::TokenStream     tokenStream;
    void                *tempPoolMark;  /* initial JSContext.tempPool mark */
    JSPrincipals        *principals;    /* principals associated with source */
    JSStackFrame *const callerFrame;    /* scripted caller frame for eval and dbgapi */
    JSObject     *const callerVarObj;   /* callerFrame's varObj */
    JSParseNode         *nodeList;      /* list of recyclable parse-node structs */
    uint32              functionCount;  /* number of functions in current unit */
    JSObjectBox         *traceListHead; /* list of parsed object for GC tracing */
    JSTreeContext       *tc;            /* innermost tree context (stack-allocated) */
    JSVersion           version;        /* cached version to avoid repeated lookups */

    /* Root atoms and objects allocated for the parsed tree. */
    js::AutoKeepAtoms   keepAtoms;

    Parser(JSContext *cx, JSPrincipals *prin = NULL, JSStackFrame *cfp = NULL);
    ~Parser();

    friend void js::AutoGCRooter::trace(JSTracer *trc);
    friend struct ::JSTreeContext;
    friend struct Compiler;

    /*
     * Initialize a parser. Parameters are passed on to init tokenStream.
     * The compiler owns the arena pool "tops-of-stack" space above the current
     * JSContext.tempPool mark. This means you cannot allocate from tempPool
     * and save the pointer beyond the next Parser destructor invocation.
     */
    bool init(const jschar *base, size_t length,
              FILE *fp, const char *filename, uintN lineno);

    void setPrincipals(JSPrincipals *prin);

    const char *getFilename()
    {
        return tokenStream.getFilename();
    }

    /*
     * Parse a top-level JS script.
     */
    JSParseNode *parse(JSObject *chain);

#if JS_HAS_XML_SUPPORT
    JSParseNode *parseXMLText(JSObject *chain, bool allowList);
#endif

    /*
     * Allocate a new parsed object or function container from cx->tempPool.
     */
    JSObjectBox *newObjectBox(JSObject *obj);

    JSFunctionBox *newFunctionBox(JSObject *obj, JSParseNode *fn, JSTreeContext *tc);

    /*
     * Create a new function object given tree context (tc), optional name
     * (atom may be null) and lambda flag (JSFUN_LAMBDA or 0).
     */
    JSFunction *newFunction(JSTreeContext *tc, JSAtom *atom, uintN lambda);

    /*
     * Analyze the tree of functions nested within a single compilation unit,
     * starting at funbox, recursively walking its kids, then following its
     * siblings, their kids, etc.
     */
    bool analyzeFunctions(JSFunctionBox *funbox, uint32& tcflags);
    bool markFunArgs(JSFunctionBox *funbox, uintN tcflags);
    void setFunctionKinds(JSFunctionBox *funbox, uint32& tcflags);

    void trace(JSTracer *trc);

    /*
     * Report a parse (compile) error.
     */
    inline bool reportErrorNumber(JSParseNode *pn, uintN flags, uintN errorNumber, ...);

private:
    /*
     * JS parsers, from lowest to highest precedence.
     *
     * Each parser must be called during the dynamic scope of a JSTreeContext
     * object, pointed to by this->tc.
     *
     * Each returns a parse node tree or null on error.
     */
    JSParseNode *functionStmt();
    JSParseNode *functionExpr();
    JSParseNode *statements();
    JSParseNode *statement();
    JSParseNode *variables(bool inLetHead);
    JSParseNode *expr();
    JSParseNode *assignExpr();
    JSParseNode *condExpr();
    JSParseNode *orExpr();
    JSParseNode *andExpr();
    JSParseNode *bitOrExpr();
    JSParseNode *bitXorExpr();
    JSParseNode *bitAndExpr();
    JSParseNode *eqExpr();
    JSParseNode *relExpr();
    JSParseNode *shiftExpr();
    JSParseNode *addExpr();
    JSParseNode *mulExpr();
    JSParseNode *unaryExpr();
    JSParseNode *memberExpr(JSBool allowCallSyntax);
    JSParseNode *primaryExpr(js::TokenKind tt, JSBool afterDot);
    JSParseNode *parenExpr(JSParseNode *pn1, JSBool *genexp);

    /*
     * Additional JS parsers.
     */
    bool recognizeDirectivePrologue(JSParseNode *pn);

    enum FunctionType { GETTER, SETTER, GENERAL };
    bool functionArguments(JSTreeContext &funtc, JSFunctionBox *funbox, JSFunction *fun,
                           JSParseNode **list);
    JSParseNode *functionBody();
    JSParseNode *functionDef(JSAtom *name, FunctionType type, uintN lambda);

    JSParseNode *condition();
    JSParseNode *comprehensionTail(JSParseNode *kid, uintN blockid,
                                   js::TokenKind type = js::TOK_SEMI, JSOp op = JSOP_NOP);
    JSParseNode *generatorExpr(JSParseNode *pn, JSParseNode *kid);
    JSBool argumentList(JSParseNode *listNode);
    JSParseNode *bracketedExpr();
    JSParseNode *letBlock(JSBool statement);
    JSParseNode *returnOrYield(bool useAssignExpr);
    JSParseNode *destructuringExpr(BindData *data, js::TokenKind tt);

#if JS_HAS_XML_SUPPORT
    JSParseNode *endBracketedExpr();

    JSParseNode *propertySelector();
    JSParseNode *qualifiedSuffix(JSParseNode *pn);
    JSParseNode *qualifiedIdentifier();
    JSParseNode *attributeIdentifier();
    JSParseNode *xmlExpr(JSBool inTag);
    JSParseNode *xmlAtomNode();
    JSParseNode *xmlNameExpr();
    JSParseNode *xmlTagContent(js::TokenKind tagtype, JSAtom **namep);
    JSBool xmlElementContent(JSParseNode *pn);
    JSParseNode *xmlElementOrList(JSBool allowList);
    JSParseNode *xmlElementOrListRoot(JSBool allowList);
#endif /* JS_HAS_XML_SUPPORT */
};

inline bool
Parser::reportErrorNumber(JSParseNode *pn, uintN flags, uintN errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream.reportCompileErrorNumberVA(pn, flags, errorNumber, args);
    va_end(args);
    return result;
}

struct Compiler
{
    Parser parser;
    GlobalScope *globalScope;

    Compiler(JSContext *cx, JSPrincipals *prin = NULL, JSStackFrame *cfp = NULL);

    /*
     * Initialize a compiler. Parameters are passed on to init parser.
     */
    inline bool
    init(const jschar *base, size_t length,
         FILE *fp, const char *filename, uintN lineno)
    {
        return parser.init(base, length, fp, filename, lineno);
    }

    static bool
    compileFunctionBody(JSContext *cx, JSFunction *fun, JSPrincipals *principals,
                        const jschar *chars, size_t length,
                        const char *filename, uintN lineno);

    static JSScript *
    compileScript(JSContext *cx, JSObject *scopeChain, JSStackFrame *callerFrame,
                  JSPrincipals *principals, uint32 tcflags,
                  const jschar *chars, size_t length,
                  FILE *file, const char *filename, uintN lineno,
                  JSString *source = NULL,
                  uintN staticLevel = 0);

  private:
    static bool
    defineGlobals(JSContext *cx, GlobalScope &globalScope, JSScript *script);
};

} /* namespace js */

/*
 * Convenience macro to access Parser.tokenStream as a pointer.
 */
#define TS(p) (&(p)->tokenStream)

extern JSBool
js_FoldConstants(JSContext *cx, JSParseNode *pn, JSTreeContext *tc,
                 bool inCond = false);

JS_END_EXTERN_C

#endif /* jsparse_h___ */

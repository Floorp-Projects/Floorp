/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef jsparse_h___
#define jsparse_h___
/*
 * JS parser definitions.
 */
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsscan.h"

PR_BEGIN_EXTERN_C

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
 * TOK_FUNCTION func        pn_fun: function, contains arg and var properties
 *                          pn_body: TOK_LC node for function body
 *                            NB: We define or create the function object at
 *                            parse (not emit) time, in order to specialize arg
 *                            and var bytecodes early.
 *                          pn_tryCount: of try statements in function
 *
 * <Statements>
 * TOK_LC       list        pn_head: list of pn_count statements
 * TOK_EXPORT   list        pn_head: list of pn_count TOK_NAMEs or one TOK_STAR
 *                            (which is not a multiply node)
 * TOK_IMPORT   list        pn_head: list of pn_count sub-trees of the form
 *                            a.b.*, a[b].*, a.*, a.b, or a[b] -- but never a.
 *                            Each member is expressed with TOK_DOT or TOK_LB.
 *                            Each sub-tree's root node has a pn_op in the set
 *                            JSOP_IMPORT{ALL,PROP,ELEM}
 * TOK_IF       ternary     pn_kid1: cond, pn_kid2: then, pn_kid3: else or null
 * TOK_SWITCH   ternary     pn_kid1: discriminant
 *                          pn_kid2: list of TOK_CASE nodes
 *                          pn_kid3: TOK_LC node for default statements or null
 * TOK_CASE     binary      pn_left: case expr
 *                          pn_right: TOK_LC node for case statements
 * TOK_WHILE    binary      pn_left: cond, pn_right: body
 * TOK_DO       binary      pn_left: body, pn_right: cond
 * TOK_FOR      binary      pn_left: either
 *                            for/in loop: a binary TOK_IN node with
 *                              pn_left:  TOK_VAR or TOK_NAME to left of 'in'
 *                              pn_right: object expr to right of 'in'
 *                            for(;;) loop: a ternary TOK_RESERVED node with
 *                              pn_kid1:  init expr before first ';'
 *                              pn_kid2:  cond expr before second ';'
 *                              pn_kid3:  update expr after second ';'
 *                              any kid may be null
 *                          pn_right: body
 * TOK_THROW    unary       pn_op: JSOP_THROW, pn_kid: exception
 * TOK_BREAK    name        pn_atom: label or null
 * TOK_CONTINUE name        pn_atom: label or null
 * TOK_WITH     binary      pn_left: head expr, pn_right: body
 * TOK_VAR      list        pn_head: list of pn_count TOK_NAME nodes
 *                          each name node has pn_atom: variable name and
 *                          pn_expr: initializer or null
 * TOK_RETURN   unary       pn_kid: return expr or null
 * TOK_SEMI     unary       pn_kid: expr or null statement
 * TOK_COLON    name        pn_atom: label, pn_expr: labeled statement
 *
 * <Expressions>
 * TOK_COMMA    list        pn_head: list of pn_count comma-separated exprs
 * TOK_ASSIGN   binary      pn_left: lvalue, pn_right: rvalue
 * TOK_HOOK     ternary     pn_kid1: cond, pn_kid2: then, pn_kid3: else
 * TOK_OR       binary      pn_left: first in || chain, pn_right: rest of chain
 * TOK_AND      binary      pn_left: first in && chain, pn_right: rest of chain
 * TOK_BITOR    binary      pn_left: left-assoc | expr, pn_right: ^ expr
 * TOK_BITXOR   binary      pn_left: left-assoc ^ expr, pn_right: & expr
 * TOK_BITAND   binary      pn_left: left-assoc & expr, pn_right: EQ expr
 * TOK_EQOP     binary      pn_left: left-assoc EQ expr, pn_right: REL expr
 *                          pn_op: JSOP_EQ, JSOP_NE, JSOP_NEW_EQ, JSOP_NEW_NE
 * TOK_RELOP    binary      pn_left: left-assoc REL expr, pn_right: SH expr
 *                          pn_op: JSOP_LT, JSOP_LE, JSOP_GT, JSOP_GE
 * TOK_SHOP     binary      pn_left: left-assoc SH expr, pn_right: ADD expr
 *                          pn_op: JSOP_LSH, JSOP_RSH, JSOP_URSH
 * TOK_PLUS,    binary      pn_left: left-assoc ADD expr, pn_right: MUL expr
 * TOK_MINUS                pn_op: JSOP_ADD, JSOP_SUB
 * TOK_STAR,    binary      pn_left: left-assoc MUL expr, pn_right: UNARY expr
 * TOK_DIVOP                pn_op: JSOP_MUL, JSOP_DIV, JSOP_MOD
 * TOK_UNARYOP  unary       pn_kid: UNARY expr, pn_op: JSOP_NEG, JSOP_POS,
 *                          JSOP_NOT, JSOP_BITNOT, JSOP_TYPEOF, JSOP_VOID
 * TOK_INC,     unary       pn_kid: MEMBER expr
 * TOK_DEC                  pn_num: arg or local var slot if non-negative
 * TOK_NEW      list        pn_head: list of ctor, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          ctor is a restricted MEMBER expr (XXX not ECMA)
 * TOK_DELETE   unary       pn_kid: MEMBER expr
 * TOK_DOT      name        pn_expr: MEMBER expr to left of .
 *                          pn_atom: name to right of .
 * TOK_LB       binary      pn_left: MEMBER expr to left of [
 *                          pn_right: expr between [ and ]
 * TOK_LP       list        pn_head: list of call, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          call is a MEMBER expr naming a callable object
 * TOK_RB       list        pn_head: list of pn_count array element exprs
 *                          [,,] holes are represented by TOK_COMMA nodes
 *                          #n=[...] produces TOK_DEFSHARP at head of list
 *                          pn_extra: true if extra comma at end
 * TOK_RC       list        pn_head: list of pn_count TOK_COLON nodes where
 *                          each has pn_left: property id, pn_right: value
 *                          #n={...} produces TOK_DEFSHARP at head of list
 * TOK_DEFSHARP unary       pn_num: jsint value of n in #n=
 * TOK_USESHARP nullary     pn_num: jsint value of n in #n#
 * TOK_RP       unary       pn_kid: parenthesized expression
 * TOK_NAME,    name        pn_atom: name, string, or object atom
 * TOK_STRING,              pn_op: JSOP_NAME, JSOP_STRING, or JSOP_OBJECT
 * TOK_OBJECT               pn_op may be *ARG or *VAR with pn_slot >= 0
 * TOK_NUMBER   dval        pn_dval: double value of numeric literal
 * TOK_PRIMARY  nullary     pn_op: JSOp bytecode
 */
typedef enum JSParseNodeArity {
    PN_FUNC     = -3,
    PN_LIST     = -2,
    PN_TERNARY  =  3,
    PN_BINARY   =  2,
    PN_UNARY    =  1,
    PN_NAME     = -1,
    PN_NULLARY  =  0
} JSParseNodeArity;

struct JSParseNode {
    JSTokenType         pn_type;
    JSTokenPos          pn_pos;
    JSOp                pn_op;
    ptrdiff_t           pn_offset;      /* first generated bytecode offset */
    JSParseNodeArity    pn_arity;
    union {
	struct {                        /* TOK_FUNCTION node */
            JSFunction  *fun;           /* function object private data */
            JSParseNode *body;          /* TOK_LC list of statements */
	    uint32      tryCount;       /* try statement count */
	} func;
        struct {                        /* list of next-linked nodes */
	    JSParseNode *head;          /* first node in list */
	    JSParseNode **tail;         /* ptr to ptr to last node in list */
	    uint32      count;          /* number of nodes in list */
	    JSBool      extra;          /* extra comma flag for [1,2,,] */
        } list;
        struct {                        /* ternary: if, switch, for(;;), ?: */
            JSParseNode *kid1;          /* condition, discriminant, etc. */
            JSParseNode *kid2;          /* then-part, case list, etc. */
            JSParseNode *kid3;          /* else-part, default case, etc. */
	} ternary;
        struct {                        /* two kids if binary */
            JSParseNode *left;
            JSParseNode *right;
	    jsval       val;            /* switch case value */
        } binary;
        struct {                        /* one kid if unary */
            JSParseNode *kid;
            jsint       num;            /* -1 or arg or local/sharp var num */
        } unary;
        struct {                        /* name, labeled statement, etc. */
            JSAtom      *atom;          /* name or label atom, null if slot */
            JSParseNode *expr;          /* object or initializer */
            jsint       slot;           /* -1 or arg or local var slot */
	} name;
	jsdouble        dval;           /* aligned numeric literal value */
    } pn_u;
    JSParseNode         *pn_next;       /* here to align dval and pn_u on RISCs */
};

#define pn_fun          pn_u.func.fun
#define pn_body         pn_u.func.body
#define pn_tryCount     pn_u.func.tryCount
#define pn_head         pn_u.list.head
#define pn_tail         pn_u.list.tail
#define pn_count        pn_u.list.count
#define pn_extra        pn_u.list.extra
#define pn_kid1         pn_u.ternary.kid1
#define pn_kid2         pn_u.ternary.kid2
#define pn_kid3         pn_u.ternary.kid3
#define pn_left         pn_u.binary.left
#define pn_right        pn_u.binary.right
#define pn_val          pn_u.binary.val
#define pn_kid          pn_u.unary.kid
#define pn_num          pn_u.unary.num
#define pn_atom         pn_u.name.atom
#define pn_expr         pn_u.name.expr
#define pn_slot         pn_u.name.slot
#define pn_dval         pn_u.dval

/*
 * Compute a pointer to the last JSParseNode element in a singly-linked list.
 * NB: list must be non-empty for correct PN_LAST usage!
 */
#define PN_LAST(list) \
    ((JSParseNode *)((char *)(list)->pn_tail - offsetof(JSParseNode, pn_next)))

#define PN_INIT_LIST(list)                                                    \
    PR_BEGIN_MACRO                                                            \
    	(list)->pn_head = NULL;                                               \
    	(list)->pn_tail = &(list)->pn_head;                                   \
    	(list)->pn_count = 0;                                                 \
    PR_END_MACRO

#define PN_INIT_LIST_1(list, pn)                                              \
    PR_BEGIN_MACRO                                                            \
    	(list)->pn_head = (pn);                                               \
    	(list)->pn_tail = &(pn)->pn_next;                                     \
    	(list)->pn_count = 1;                                                 \
    PR_END_MACRO

#define PN_APPEND(list, pn)                                                   \
    PR_BEGIN_MACRO                                                            \
	*(list)->pn_tail = (pn);                                              \
	(list)->pn_tail = &(pn)->pn_next;                                     \
	(list)->pn_count++;                                                   \
    PR_END_MACRO

/* New names to connote code generation in addition to parse tree gen. */
#define js_CompileTokenStream   js_Parse
#define js_CompileFunctionBody  js_ParseFunctionBody

extern JS_FRIEND_API(JSBool)
js_CompileTokenStream(JSContext *cx, JSObject *chain, JSTokenStream *ts,
		      JSCodeGenerator *cg);

extern JSBool
js_CompileFunctionBody(JSContext *cx, JSTokenStream *ts, JSFunction *fun);

/* XXXbe expose js_Parse{TokenStream,FunctionBody} that return trees? */
extern JSBool
js_FoldConstants(JSContext *cx, JSParseNode *pn);

PR_END_EXTERN_C

#endif /* jsparse_h___ */

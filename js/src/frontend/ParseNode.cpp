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

#include "frontend/ParseNode.h"

#include "jsscriptinlines.h"

#include "frontend/ParseMaps-inl.h"
#include "frontend/ParseNode-inl.h"

using namespace js;

/*
 * Asserts to verify assumptions behind pn_ macros.
 */
#define pn_offsetof(m)  offsetof(ParseNode, m)

JS_STATIC_ASSERT(pn_offsetof(pn_link) == pn_offsetof(dn_uses));

#undef pn_offsetof

void
ParseNode::become(ParseNode *pn2)
{
    JS_ASSERT(!pn_defn);
    JS_ASSERT(!pn2->isDefn());

    JS_ASSERT(!pn_used);
    if (pn2->isUsed()) {
        ParseNode **pnup = &pn2->pn_lexdef->dn_uses;
        while (*pnup != pn2)
            pnup = &(*pnup)->pn_link;
        *pnup = this;
        pn_link = pn2->pn_link;
        pn_used = true;
        pn2->pn_link = NULL;
        pn2->pn_used = false;
    }

    pn_type = pn2->pn_type;
    pn_op = pn2->pn_op;
    pn_arity = pn2->pn_arity;
    pn_parens = pn2->pn_parens;
    pn_u = pn2->pn_u;

    /*
     * If any pointers are pointing to pn2, change them to point to this
     * instead, since pn2 will be cleared and probably recycled.
     */
    if (this->isKind(PNK_FUNCTION) && isArity(PN_FUNC)) {
        /* Function node: fix up the pn_funbox->node back-pointer. */
        JS_ASSERT(pn_funbox->node == pn2);
        pn_funbox->node = this;
    } else if (pn_arity == PN_LIST && !pn_head) {
        /* Empty list: fix up the pn_tail pointer. */
        JS_ASSERT(pn_count == 0);
        JS_ASSERT(pn_tail == &pn2->pn_head);
        pn_tail = &pn_head;
    }

    pn2->clear();
}

void
ParseNode::clear()
{
    pn_type = PNK_LIMIT;
    setOp(JSOP_NOP);
    pn_used = pn_defn = false;
    pn_arity = PN_NULLARY;
    pn_parens = false;
}

bool
FunctionBox::joinable() const
{
    return function()->isNullClosure() &&
           (tcflags & (TCF_FUN_USES_ARGUMENTS |
                       TCF_FUN_USES_OWN_NAME |
                       TCF_COMPILE_N_GO)) == TCF_COMPILE_N_GO;
}

bool
FunctionBox::inAnyDynamicScope() const
{
    for (const FunctionBox *funbox = this; funbox; funbox = funbox->parent) {
        if (funbox->tcflags & (TCF_IN_WITH | TCF_FUN_EXTENSIBLE_SCOPE))
            return true;
    }
    return false;
}

bool
FunctionBox::scopeIsExtensible() const
{
    return tcflags & TCF_FUN_EXTENSIBLE_SCOPE;
}

bool
FunctionBox::shouldUnbrand(uintN methods, uintN slowMethods) const
{
    if (slowMethods != 0) {
        for (const FunctionBox *funbox = this; funbox; funbox = funbox->parent) {
            if (!(funbox->tcflags & TCF_FUN_MODULE_PATTERN))
                return true;
            if (funbox->inLoop)
                return true;
        }
    }
    return false;
}

/* Add |node| to |parser|'s free node list. */
void
ParseNodeAllocator::freeNode(ParseNode *pn)
{
    /* Catch back-to-back dup recycles. */
    JS_ASSERT(pn != freelist);

    /* 
     * It's too hard to clear these nodes from the AtomDefnMaps, etc. that
     * hold references to them, so we never free them. It's our caller's job to
     * recognize and process these, since their children do need to be dealt
     * with.
     */
    JS_ASSERT(!pn->isUsed());
    JS_ASSERT(!pn->isDefn());

    if (pn->isArity(PN_NAMESET) && pn->pn_names.hasMap())
        pn->pn_names.releaseMap(cx);

#ifdef DEBUG
    /* Poison the node, to catch attempts to use it without initializing it. */
    memset(pn, 0xab, sizeof(*pn));
#endif

    pn->pn_next = freelist;
    freelist = pn;
}

/*
 * A work pool of ParseNodes. The work pool is a stack, chained together
 * by nodes' pn_next fields. We use this to avoid creating deep C++ stacks
 * when recycling deep parse trees.
 *
 * Since parse nodes are probably allocated in something close to the order
 * they appear in a depth-first traversal of the tree, making the work pool
 * a stack should give us pretty good locality.
 */
class NodeStack {
  public:
    NodeStack() : top(NULL) { }
    bool empty() { return top == NULL; }
    void push(ParseNode *pn) {
        pn->pn_next = top;
        top = pn;
    }
    void pushUnlessNull(ParseNode *pn) { if (pn) push(pn); }
    /* Push the children of the PN_LIST node |pn| on the stack. */
    void pushList(ParseNode *pn) {
        /* This clobbers pn->pn_head if the list is empty; should be okay. */
        *pn->pn_tail = top;
        top = pn->pn_head;
    }
    ParseNode *pop() {
        JS_ASSERT(!empty());
        ParseNode *hold = top; /* my kingdom for a prog1 */
        top = top->pn_next;
        return hold;
    }
  private:
    ParseNode *top;
};

/*
 * Push the children of |pn| on |stack|. Return true if |pn| itself could be
 * safely recycled, or false if it must be cleaned later (pn_used and pn_defn
 * nodes, and all function nodes; see comments for CleanFunctionList in
 * SemanticAnalysis.cpp). Some callers want to free |pn|; others
 * (js::ParseNodeAllocator::prepareNodeForMutation) don't care about |pn|, and
 * just need to take care of its children.
 */
static bool
PushNodeChildren(ParseNode *pn, NodeStack *stack)
{
    switch (pn->getArity()) {
      case PN_FUNC:
        /*
         * Function nodes are linked into the function box tree, and may appear
         * on method lists. Both of those lists are singly-linked, so trying to
         * update them now could result in quadratic behavior when recycling
         * trees containing many functions; and the lists can be very long. So
         * we put off cleaning the lists up until just before function
         * analysis, when we call CleanFunctionList.
         *
         * In fact, we can't recycle the parse node yet, either: it may appear
         * on a method list, and reusing the node would corrupt that. Instead,
         * we clear its pn_funbox pointer to mark it as deleted;
         * CleanFunctionList recycles it as well.
         *
         * We do recycle the nodes around it, though, so we must clear pointers
         * to them to avoid leaving dangling references where someone can find
         * them.
         */
        pn->pn_funbox = NULL;
        stack->pushUnlessNull(pn->pn_body);
        pn->pn_body = NULL;
        return false;

      case PN_NAME:
        /*
         * Because used/defn nodes appear in AtomDefnMaps and elsewhere, we
         * don't recycle them. (We'll recover their storage when we free the
         * temporary arena.) However, we do recycle the nodes around them, so
         * clean up the pointers to avoid dangling references. The top-level
         * decls table carries references to them that later iterations through
         * the compileScript loop may find, so they need to be neat.
         *
         * pn_expr and pn_lexdef share storage; the latter isn't an owning
         * reference.
         */
        if (!pn->isUsed()) {
            stack->pushUnlessNull(pn->pn_expr);
            pn->pn_expr = NULL;
        }
        return !pn->isUsed() && !pn->isDefn();

      case PN_LIST:
        stack->pushList(pn);
        break;
      case PN_TERNARY:
        stack->pushUnlessNull(pn->pn_kid1);
        stack->pushUnlessNull(pn->pn_kid2);
        stack->pushUnlessNull(pn->pn_kid3);
        break;
      case PN_BINARY:
        if (pn->pn_left != pn->pn_right)
            stack->pushUnlessNull(pn->pn_left);
        stack->pushUnlessNull(pn->pn_right);
        break;
      case PN_UNARY:
        stack->pushUnlessNull(pn->pn_kid);
        break;
      case PN_NULLARY:
        /* 
         * E4X function namespace nodes are PN_NULLARY, but can appear on use
         * lists.
         */
        return !pn->isUsed() && !pn->isDefn();
      default:
        ;
    }

    return true;
}

/*
 * Prepare |pn| to be mutated in place into a new kind of node. Recycle all
 * |pn|'s recyclable children (but not |pn| itself!), and disconnect it from
 * metadata structures (the function box tree).
 */
void
ParseNodeAllocator::prepareNodeForMutation(ParseNode *pn)
{
    if (!pn->isArity(PN_NULLARY)) {
        if (pn->isArity(PN_FUNC)) {
            /*
             * Since this node could be turned into anything, we can't
             * ensure it won't be subsequently recycled, so we must
             * disconnect it from the funbox tree entirely.
             *
             * Note that pn_funbox may legitimately be NULL. functionDef
             * applies MakeDefIntoUse to definition nodes, which can come
             * from prior iterations of the big loop in compileScript. In
             * such cases, the defn nodes have been visited by the recycler
             * (but not actually recycled!), and their funbox pointers
             * cleared. But it's fine to mutate them into uses of some new
             * definition.
             */
            if (pn->pn_funbox)
                pn->pn_funbox->node = NULL;
        }

        /* Put |pn|'s children (but not |pn| itself) on a work stack. */
        NodeStack stack;
        PushNodeChildren(pn, &stack);
        /*
         * For each node on the work stack, push its children on the work stack,
         * and free the node if we can.
         */
        while (!stack.empty()) {
            pn = stack.pop();
            if (PushNodeChildren(pn, &stack))
                freeNode(pn);
        }
    }
}

/*
 * Return the nodes in the subtree |pn| to the parser's free node list, for
 * reallocation.
 *
 * Note that all functions in |pn| that are not enclosed by other functions
 * in |pn| must be direct children of |tc|, because we only clean up |tc|'s
 * function and method lists. You must not reach into a function and
 * recycle some part of it (unless you've updated |tc|->functionList, the
 * way js_FoldConstants does).
 */
ParseNode *
ParseNodeAllocator::freeTree(ParseNode *pn)
{
    if (!pn)
        return NULL;

    ParseNode *savedNext = pn->pn_next;

    NodeStack stack;
    for (;;) {
        if (PushNodeChildren(pn, &stack))
            freeNode(pn);
        if (stack.empty())
            break;
        pn = stack.pop();
    }

    return savedNext;
}

/*
 * Allocate a ParseNode from parser's node freelist or, failing that, from
 * cx's temporary arena.
 */
void *
ParseNodeAllocator::allocNode()
{
    if (ParseNode *pn = freelist) {
        freelist = pn->pn_next;
        return pn;
    }

    void *p = cx->tempLifoAlloc().alloc(sizeof (ParseNode));
    if (!p)
        js_ReportOutOfMemory(cx);
    return p;
}

/* used only by static create methods of subclasses */

ParseNode *
ParseNode::create(ParseNodeKind kind, ParseNodeArity arity, TreeContext *tc)
{
    Parser *parser = tc->parser;
    const Token &tok = parser->tokenStream.currentToken();
    return parser->new_<ParseNode>(kind, JSOP_NOP, arity, tok.pos);
}

ParseNode *
ParseNode::append(ParseNodeKind kind, JSOp op, ParseNode *left, ParseNode *right)
{
    if (!left || !right)
        return NULL;

    JS_ASSERT(left->isKind(kind) && left->isOp(op) && (js_CodeSpec[op].format & JOF_LEFTASSOC));

    if (left->pn_arity != PN_LIST) {
        ParseNode *pn1 = left->pn_left, *pn2 = left->pn_right;
        left->setArity(PN_LIST);
        left->pn_parens = false;
        left->initList(pn1);
        left->append(pn2);
        if (kind == PNK_PLUS) {
            if (pn1->isKind(PNK_STRING))
                left->pn_xflags |= PNX_STRCAT;
            else if (!pn1->isKind(PNK_NUMBER))
                left->pn_xflags |= PNX_CANTFOLD;
            if (pn2->isKind(PNK_STRING))
                left->pn_xflags |= PNX_STRCAT;
            else if (!pn2->isKind(PNK_NUMBER))
                left->pn_xflags |= PNX_CANTFOLD;
        }
    }
    left->append(right);
    left->pn_pos.end = right->pn_pos.end;
    if (kind == PNK_PLUS) {
        if (right->isKind(PNK_STRING))
            left->pn_xflags |= PNX_STRCAT;
        else if (!right->isKind(PNK_NUMBER))
            left->pn_xflags |= PNX_CANTFOLD;
    }

    return left;
}

ParseNode *
ParseNode::newBinaryOrAppend(ParseNodeKind kind, JSOp op, ParseNode *left, ParseNode *right,
                             TreeContext *tc)
{
    if (!left || !right)
        return NULL;

    /*
     * Flatten a left-associative (left-heavy) tree of a given operator into
     * a list, to reduce js_FoldConstants and js_EmitTree recursion.
     */
    if (left->isKind(kind) && left->isOp(op) && (js_CodeSpec[op].format & JOF_LEFTASSOC))
        return append(kind, op, left, right);

    /*
     * Fold constant addition immediately, to conserve node space and, what's
     * more, so js_FoldConstants never sees mixed addition and concatenation
     * operations with more than one leading non-string operand in a PN_LIST
     * generated for expressions such as 1 + 2 + "pt" (which should evaluate
     * to "3pt", not "12pt").
     */
    if (kind == PNK_PLUS &&
        left->isKind(PNK_NUMBER) &&
        right->isKind(PNK_NUMBER) &&
        tc->parser->foldConstants)
    {
        left->pn_dval += right->pn_dval;
        left->pn_pos.end = right->pn_pos.end;
        tc->freeTree(right);
        return left;
    }

    return tc->parser->new_<BinaryNode>(kind, op, left, right);
}

NameNode *
NameNode::create(ParseNodeKind kind, JSAtom *atom, TreeContext *tc)
{
    ParseNode *pn = ParseNode::create(kind, PN_NAME, tc);
    if (pn) {
        pn->pn_atom = atom;
        ((NameNode *)pn)->initCommon(tc);
    }
    return (NameNode *)pn;
}

const char js_argument_str[] = "argument";
const char js_variable_str[] = "variable";
const char js_unknown_str[]  = "unknown";

const char *
Definition::kindString(Kind kind)
{
    static const char *table[] = {
        js_var_str, js_const_str, js_let_str,
        js_function_str, js_argument_str, js_unknown_str
    };

    JS_ASSERT(unsigned(kind) <= unsigned(ARG));
    return table[kind];
}

#if JS_HAS_DESTRUCTURING

/*
 * This function assumes the cloned tree is for use in the same statement and
 * binding context as the original tree.
 */
static ParseNode *
CloneParseTree(ParseNode *opn, TreeContext *tc)
{
    JS_CHECK_RECURSION(tc->parser->context, return NULL);

    ParseNode *pn = tc->parser->new_<ParseNode>(opn->getKind(), opn->getOp(), opn->getArity(),
                                                opn->pn_pos);
    if (!pn)
        return NULL;
    pn->setInParens(opn->isInParens());
    pn->setDefn(opn->isDefn());
    pn->setUsed(opn->isUsed());

    switch (pn->getArity()) {
#define NULLCHECK(e)    JS_BEGIN_MACRO if (!(e)) return NULL; JS_END_MACRO

      case PN_FUNC:
        NULLCHECK(pn->pn_funbox =
                  tc->parser->newFunctionBox(opn->pn_funbox->object, pn, tc));
        NULLCHECK(pn->pn_body = CloneParseTree(opn->pn_body, tc));
        pn->pn_cookie = opn->pn_cookie;
        pn->pn_dflags = opn->pn_dflags;
        pn->pn_blockid = opn->pn_blockid;
        break;

      case PN_LIST:
        pn->makeEmpty();
        for (ParseNode *opn2 = opn->pn_head; opn2; opn2 = opn2->pn_next) {
            ParseNode *pn2;
            NULLCHECK(pn2 = CloneParseTree(opn2, tc));
            pn->append(pn2);
        }
        pn->pn_xflags = opn->pn_xflags;
        break;

      case PN_TERNARY:
        NULLCHECK(pn->pn_kid1 = CloneParseTree(opn->pn_kid1, tc));
        NULLCHECK(pn->pn_kid2 = CloneParseTree(opn->pn_kid2, tc));
        NULLCHECK(pn->pn_kid3 = CloneParseTree(opn->pn_kid3, tc));
        break;

      case PN_BINARY:
        NULLCHECK(pn->pn_left = CloneParseTree(opn->pn_left, tc));
        if (opn->pn_right != opn->pn_left)
            NULLCHECK(pn->pn_right = CloneParseTree(opn->pn_right, tc));
        else
            pn->pn_right = pn->pn_left;
        pn->pn_pval = opn->pn_pval;
        pn->pn_iflags = opn->pn_iflags;
        break;

      case PN_UNARY:
        NULLCHECK(pn->pn_kid = CloneParseTree(opn->pn_kid, tc));
        pn->pn_num = opn->pn_num;
        pn->pn_hidden = opn->pn_hidden;
        break;

      case PN_NAME:
        // PN_NAME could mean several arms in pn_u, so copy the whole thing.
        pn->pn_u = opn->pn_u;
        if (opn->isUsed()) {
            /*
             * The old name is a use of its pn_lexdef. Make the clone also be a
             * use of that definition.
             */
            Definition *dn = pn->pn_lexdef;

            pn->pn_link = dn->dn_uses;
            dn->dn_uses = pn;
        } else if (opn->pn_expr) {
            NULLCHECK(pn->pn_expr = CloneParseTree(opn->pn_expr, tc));

            /*
             * If the old name is a definition, the new one has pn_defn set.
             * Make the old name a use of the new node.
             */
            if (opn->isDefn()) {
                opn->setDefn(false);
                LinkUseToDef(opn, (Definition *) pn, tc);
            }
        }
        break;

      case PN_NAMESET:
        pn->pn_names = opn->pn_names;
        NULLCHECK(pn->pn_tree = CloneParseTree(opn->pn_tree, tc));
        break;

      case PN_NULLARY:
        // Even PN_NULLARY may have data (xmlpi for E4X -- what a botch).
        pn->pn_u = opn->pn_u;
        break;

#undef NULLCHECK
    }
    return pn;
}

#endif /* JS_HAS_DESTRUCTURING */

/*
 * Used by Parser::forStatement and comprehensionTail to clone the TARGET in
 *   for (var/const/let TARGET in EXPR)
 *
 * opn must be the pn_head of a node produced by Parser::variables, so its form
 * is known to be LHS = NAME | [LHS] | {id:LHS}.
 *
 * The cloned tree is for use only in the same statement and binding context as
 * the original tree.
 */
ParseNode *
js::CloneLeftHandSide(ParseNode *opn, TreeContext *tc)
{
    ParseNode *pn = tc->parser->new_<ParseNode>(opn->getKind(), opn->getOp(), opn->getArity(),
                                                opn->pn_pos);
    if (!pn)
        return NULL;
    pn->setInParens(opn->isInParens());
    pn->setDefn(opn->isDefn());
    pn->setUsed(opn->isUsed());

#if JS_HAS_DESTRUCTURING
    if (opn->isArity(PN_LIST)) {
        JS_ASSERT(opn->isKind(PNK_RB) || opn->isKind(PNK_RC));
        pn->makeEmpty();
        for (ParseNode *opn2 = opn->pn_head; opn2; opn2 = opn2->pn_next) {
            ParseNode *pn2;
            if (opn->isKind(PNK_RC)) {
                JS_ASSERT(opn2->isArity(PN_BINARY));
                JS_ASSERT(opn2->isKind(PNK_COLON));

                ParseNode *tag = CloneParseTree(opn2->pn_left, tc);
                if (!tag)
                    return NULL;
                ParseNode *target = CloneLeftHandSide(opn2->pn_right, tc);
                if (!target)
                    return NULL;

                pn2 = tc->parser->new_<BinaryNode>(PNK_COLON, JSOP_INITPROP, opn2->pn_pos, tag, target);
            } else if (opn2->isArity(PN_NULLARY)) {
                JS_ASSERT(opn2->isKind(PNK_COMMA));
                pn2 = CloneParseTree(opn2, tc);
            } else {
                pn2 = CloneLeftHandSide(opn2, tc);
            }

            if (!pn2)
                return NULL;
            pn->append(pn2);
        }
        pn->pn_xflags = opn->pn_xflags;
        return pn;
    }
#endif

    JS_ASSERT(opn->isArity(PN_NAME));
    JS_ASSERT(opn->isKind(PNK_NAME));

    /* If opn is a definition or use, make pn a use. */
    pn->pn_u.name = opn->pn_u.name;
    pn->setOp(JSOP_SETNAME);
    if (opn->isUsed()) {
        Definition *dn = pn->pn_lexdef;

        pn->pn_link = dn->dn_uses;
        dn->dn_uses = pn;
    } else {
        pn->pn_expr = NULL;
        if (opn->isDefn()) {
            /* We copied some definition-specific state into pn. Clear it out. */
            pn->pn_cookie.makeFree();
            pn->pn_dflags &= ~PND_BOUND;
            pn->setDefn(false);

            LinkUseToDef(pn, (Definition *) opn, tc);
        }
    }
    return pn;
}

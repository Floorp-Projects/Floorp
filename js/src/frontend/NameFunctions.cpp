/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/NameFunctions.h"

#include "jsfun.h"
#include "jsprf.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"
#include "vm/StringBuffer.h"

#include "jsfuninlines.h"

using namespace js;
using namespace js::frontend;

class NameResolver
{
    static const size_t MaxParents = 100;

    JSContext *cx;
    size_t nparents;                /* number of parents in the parents array */
    ParseNode *parents[MaxParents]; /* history of ParseNodes we've been looking at */
    StringBuffer *buf;              /* when resolving, buffer to append to */

    /* Test whether a ParseNode represents a function invocation */
    bool call(ParseNode *pn) {
        return pn && pn->isKind(PNK_CALL);
    }

    /*
     * Append a reference to a property named |name| to |buf|. If |name| is
     * a proper identifier name, then we append '.name'; otherwise, we
     * append '["name"]'.
     *
     * Note that we need the IsIdentifier check for atoms from both
     * PNK_NAME nodes and PNK_STRING nodes: given code like a["b c"], the
     * front end will produce a PNK_DOT with a PNK_NAME child whose name
     * contains spaces.
     */
    bool appendPropertyReference(JSAtom *name) {
        if (IsIdentifier(name))
            return buf->append(".") && buf->append(name);

        /* Quote the string as needed. */
        JSString *source = js_QuoteString(cx, name, '"');
        return source && buf->append("[") && buf->append(source) && buf->append("]");
    }

    /* Append a number to buf. */
    bool appendNumber(double n) {
        char number[30];
        int digits = JS_snprintf(number, sizeof(number), "%g", n);
        return buf->appendInflated(number, digits);
    }

    /* Append "[<n>]" to buf, referencing a property named by a numeric literal. */
    bool appendNumericPropertyReference(double n) {
        return buf->append("[") && appendNumber(n) && buf->append("]");
    }

    /*
     * Walk over the given ParseNode, converting it to a stringified name that
     * respresents where the function is being assigned to.
     */
    bool nameExpression(ParseNode *n) {
        switch (n->getKind()) {
          case PNK_DOT:
            return nameExpression(n->expr()) && appendPropertyReference(n->pn_atom);

          case PNK_NAME:
            return buf->append(n->pn_atom);

          case PNK_ELEM:
            return nameExpression(n->pn_left) &&
                   buf->append("[") &&
                   nameExpression(n->pn_right) &&
                   buf->append("]");

          case PNK_NUMBER:
            return appendNumber(n->pn_dval);

          default:
            /*
             * Technically this isn't an "abort" situation, we're just confused
             * on what to call this function, but failures in naming aren't
             * treated as fatal.
             */
            return false;
        }
    }

    /*
     * When naming an anonymous function, the process works loosely by walking
     * up the AST and then translating that to a string. The stringification
     * happens from some far-up assignment and then going back down the parse
     * tree to the function definition point.
     *
     * This function will walk up the parse tree, gathering relevant nodes used
     * for naming, and return the assignment node if there is one. The provided
     * array and size will be filled in, and the returned node could be NULL if
     * no assignment is found. The first element of the array will be the
     * innermost node relevant to naming, and the last element will be the
     * outermost node.
     */
    ParseNode *gatherNameable(ParseNode **nameable, size_t *size) {
        *size = 0;

        for (int pos = nparents - 1; pos >= 0; pos--) {
            ParseNode *cur = parents[pos];
            if (cur->isAssignment())
                return cur;

            switch (cur->getKind()) {
              case PNK_NAME:     return cur;  /* found the initialized declaration */
              case PNK_FUNCTION: return NULL; /* won't find an assignment or declaration */

              case PNK_RETURN:
                /*
                 * Normally the relevant parent of a node is its direct parent, but
                 * sometimes with code like:
                 *
                 *    var foo = (function() { return function() {}; })();
                 *
                 * the outer function is just a helper to create a scope for the
                 * returned function. Hence the name of the returned function should
                 * actually be 'foo'.  This loop sees if the current node is a
                 * PNK_RETURN, and if there is a direct function call we skip to
                 * that.
                 */
                for (int tmp = pos - 1; tmp > 0; tmp--) {
                    if (isDirectCall(tmp, cur)) {
                        pos = tmp;
                        break;
                    } else if (call(cur)) {
                        /* Don't skip too high in the tree */
                        break;
                    }
                    cur = parents[tmp];
                }
                break;

              case PNK_COLON:
                /*
                 * If this is a PNK_COLON, but our parent is not a PNK_OBJECT,
                 * then this is a label and we're done naming. Otherwise we
                 * record the PNK_COLON but skip the PNK_OBJECT so we're not
                 * flagged as a contributor.
                 */
                if (pos == 0 || !parents[pos - 1]->isKind(PNK_OBJECT))
                    return NULL;
                pos--;
                /* fallthrough */

              default:
                /* Save any other nodes we encounter on the way up. */
                JS_ASSERT(*size < MaxParents);
                nameable[(*size)++] = cur;
                break;
            }
        }

        return NULL;
    }

    /*
     * Resolve the name of a function. If the function already has a name
     * listed, then it is skipped. Otherwise an intelligent name is guessed to
     * assign to the function's displayAtom field
     */
    JSAtom *resolveFun(ParseNode *pn, HandleAtom prefix) {
        JS_ASSERT(pn != NULL && pn->isKind(PNK_FUNCTION));
        RootedFunction fun(cx, pn->pn_funbox->function());

        StringBuffer buf(cx);
        this->buf = &buf;

        /* If the function already has a name, use that */
        if (fun->displayAtom() != NULL) {
            if (prefix == NULL)
                return fun->displayAtom();
            if (!buf.append(prefix) ||
                !buf.append("/") ||
                !buf.append(fun->displayAtom()))
                return NULL;
            return buf.finishAtom();
        }

        /* If a prefix is specified, then it is a form of namespace */
        if (prefix != NULL && (!buf.append(prefix) || !buf.append("/")))
            return NULL;

        /* Gather all nodes relevant to naming */
        ParseNode *toName[MaxParents];
        size_t size;
        ParseNode *assignment = gatherNameable(toName, &size);

        /* If the function is assigned to something, then that is very relevant */
        if (assignment) {
            if (assignment->isAssignment())
                assignment = assignment->pn_left;
            if (!nameExpression(assignment))
                return NULL;
        }

        /*
         * Other than the actual assignment, other relevant nodes to naming are
         * those in object initializers and then particular nodes marking a
         * contribution.
         */
        for (int pos = size - 1; pos >= 0; pos--) {
            ParseNode *node = toName[pos];

            if (node->isKind(PNK_COLON)) {
                ParseNode *left = node->pn_left;
                if (left->isKind(PNK_NAME) || left->isKind(PNK_STRING)) {
                    if (!appendPropertyReference(left->pn_atom))
                        return NULL;
                } else if (left->isKind(PNK_NUMBER)) {
                    if (!appendNumericPropertyReference(left->pn_dval))
                        return NULL;
                }
            } else {
                /*
                 * Don't have consecutive '<' characters, and also don't start
                 * with a '<' character.
                 */
                if (!buf.empty() && *(buf.end() - 1) != '<' && !buf.append("<"))
                    return NULL;
            }
        }

        /*
         * functions which are "genuinely anonymous" but are contained in some
         * other namespace are rather considered as "contributing" to the outer
         * function, so give them a contribution symbol here.
         */
        if (!buf.empty() && *(buf.end() - 1) == '/' && !buf.append("<"))
            return NULL;
        if (buf.empty())
            return NULL;

        JSAtom *atom = buf.finishAtom();
        if (!atom)
            return NULL;
        fun->setGuessedAtom(atom);
        return atom;
    }

    /*
     * Tests whether parents[pos] is a function call whose callee is cur.
     * This is the case for functions which do things like simply create a scope
     * for new variables and then return an anonymous function using this scope.
     */
    bool isDirectCall(int pos, ParseNode *cur) {
        return pos >= 0 && call(parents[pos]) && parents[pos]->pn_head == cur;
    }

  public:
    explicit NameResolver(JSContext *cx) : cx(cx), nparents(0), buf(NULL) {}

    /*
     * Resolve all names for anonymous functions recursively within the
     * ParseNode instance given. The prefix is for each subsequent name, and
     * should initially be NULL.
     */
    void resolve(ParseNode *cur, HandleAtom prefixArg = NullPtr()) {
        RootedAtom prefix(cx, prefixArg);
        if (cur == NULL)
            return;

        if (cur->isKind(PNK_FUNCTION) && cur->isArity(PN_CODE)) {
            RootedAtom prefix2(cx, resolveFun(cur, prefix));
            /*
             * If a function looks like (function(){})() where the parent node
             * of the definition of the function is a call, then it shouldn't
             * contribute anything to the namespace, so don't bother updating
             * the prefix to whatever was returned.
             */
            if (!isDirectCall(nparents - 1, cur))
                prefix = prefix2;
        }
        if (nparents >= MaxParents)
            return;
        parents[nparents++] = cur;

        switch (cur->getArity()) {
          case PN_NULLARY:
            break;
          case PN_NAME:
            resolve(cur->maybeExpr(), prefix);
            break;
          case PN_UNARY:
            resolve(cur->pn_kid, prefix);
            break;
          case PN_BINARY:
            resolve(cur->pn_left, prefix);

            /*
             * FIXME? Occasionally pn_left == pn_right for something like
             * destructuring assignment in (function({foo}){}), so skip the
             * duplicate here if this is the case because we want to traverse
             * everything at most once.
             */
            if (cur->pn_left != cur->pn_right)
                resolve(cur->pn_right, prefix);
            break;
          case PN_TERNARY:
            resolve(cur->pn_kid1, prefix);
            resolve(cur->pn_kid2, prefix);
            resolve(cur->pn_kid3, prefix);
            break;
          case PN_CODE:
            JS_ASSERT(cur->isKind(PNK_MODULE) || cur->isKind(PNK_FUNCTION));
            resolve(cur->pn_body, prefix);
            break;
          case PN_LIST:
            for (ParseNode *nxt = cur->pn_head; nxt; nxt = nxt->pn_next)
                resolve(nxt, prefix);
            break;
        }
        nparents--;
    }
};

bool
frontend::NameFunctions(JSContext *cx, ParseNode *pn)
{
    NameResolver nr(cx);
    nr.resolve(pn);
    return true;
}

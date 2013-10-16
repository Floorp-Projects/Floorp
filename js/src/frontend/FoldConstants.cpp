/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/FoldConstants.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/TypedEnum.h"

#include "jslibmath.h"

#include "frontend/ParseNode.h"
#include "frontend/Parser.h"
#include "vm/NumericConversions.h"

#include "jscntxtinlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::frontend;

using mozilla::IsNaN;
using mozilla::IsNegative;
using mozilla::NegativeInfinity;
using mozilla::PositiveInfinity;
using JS::GenericNaN;

static ParseNode *
ContainsVarOrConst(ParseNode *pn)
{
    if (!pn)
        return nullptr;
    if (pn->isKind(PNK_VAR) || pn->isKind(PNK_CONST))
        return pn;
    switch (pn->getArity()) {
      case PN_LIST:
        for (ParseNode *pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (ParseNode *pnt = ContainsVarOrConst(pn2))
                return pnt;
        }
        break;
      case PN_TERNARY:
        if (ParseNode *pnt = ContainsVarOrConst(pn->pn_kid1))
            return pnt;
        if (ParseNode *pnt = ContainsVarOrConst(pn->pn_kid2))
            return pnt;
        return ContainsVarOrConst(pn->pn_kid3);
      case PN_BINARY:
        /*
         * Limit recursion if pn is a binary expression, which can't contain a
         * var statement.
         */
        if (!pn->isOp(JSOP_NOP))
            return nullptr;
        if (ParseNode *pnt = ContainsVarOrConst(pn->pn_left))
            return pnt;
        return ContainsVarOrConst(pn->pn_right);
      case PN_UNARY:
        if (!pn->isOp(JSOP_NOP))
            return nullptr;
        return ContainsVarOrConst(pn->pn_kid);
      case PN_NAME:
        return ContainsVarOrConst(pn->maybeExpr());
      default:;
    }
    return nullptr;
}

/*
 * Fold from one constant type to another.
 * XXX handles only strings and numbers for now
 */
static bool
FoldType(ExclusiveContext *cx, ParseNode *pn, ParseNodeKind kind)
{
    if (!pn->isKind(kind)) {
        switch (kind) {
          case PNK_NUMBER:
            if (pn->isKind(PNK_STRING)) {
                double d;
                if (!StringToNumber(cx, pn->pn_atom, &d))
                    return false;
                pn->pn_dval = d;
                pn->setKind(PNK_NUMBER);
                pn->setOp(JSOP_DOUBLE);
            }
            break;

          case PNK_STRING:
            if (pn->isKind(PNK_NUMBER)) {
                pn->pn_atom = NumberToAtom<CanGC>(cx, pn->pn_dval);
                if (!pn->pn_atom)
                    return false;
                pn->setKind(PNK_STRING);
                pn->setOp(JSOP_STRING);
            }
            break;

          default:;
        }
    }
    return true;
}

/*
 * Fold two numeric constants.  Beware that pn1 and pn2 are recycled, unless
 * one of them aliases pn, so you can't safely fetch pn2->pn_next, e.g., after
 * a successful call to this function.
 */
static bool
FoldBinaryNumeric(ExclusiveContext *cx, JSOp op, ParseNode *pn1, ParseNode *pn2,
                  ParseNode *pn)
{
    double d, d2;
    int32_t i, j;

    JS_ASSERT(pn1->isKind(PNK_NUMBER) && pn2->isKind(PNK_NUMBER));
    d = pn1->pn_dval;
    d2 = pn2->pn_dval;
    switch (op) {
      case JSOP_LSH:
      case JSOP_RSH:
        i = ToInt32(d);
        j = ToInt32(d2);
        j &= 31;
        d = (op == JSOP_LSH) ? i << j : i >> j;
        break;

      case JSOP_URSH:
        j = ToInt32(d2);
        j &= 31;
        d = ToUint32(d) >> j;
        break;

      case JSOP_ADD:
        d += d2;
        break;

      case JSOP_SUB:
        d -= d2;
        break;

      case JSOP_MUL:
        d *= d2;
        break;

      case JSOP_DIV:
        if (d2 == 0) {
#if defined(XP_WIN)
            /* XXX MSVC miscompiles such that (NaN == 0) */
            if (IsNaN(d2))
                d = GenericNaN();
            else
#endif
            if (d == 0 || IsNaN(d))
                d = GenericNaN();
            else if (IsNegative(d) != IsNegative(d2))
                d = NegativeInfinity();
            else
                d = PositiveInfinity();
        } else {
            d /= d2;
        }
        break;

      case JSOP_MOD:
        if (d2 == 0) {
            d = GenericNaN();
        } else {
            d = js_fmod(d, d2);
        }
        break;

      default:;
    }

    /* Take care to allow pn1 or pn2 to alias pn. */
    pn->setKind(PNK_NUMBER);
    pn->setOp(JSOP_DOUBLE);
    pn->setArity(PN_NULLARY);
    pn->pn_dval = d;
    return true;
}

// Remove a ParseNode, **pnp, from a parse tree, putting another ParseNode,
// *pn, in its place.
//
// pnp points to a ParseNode pointer. This must be the only pointer that points
// to the parse node being replaced. The replacement, *pn, is unchanged except
// for its pn_next pointer; updating that is necessary if *pn's new parent is a
// list node.
static void
ReplaceNode(ParseNode **pnp, ParseNode *pn)
{
    pn->pn_next = (*pnp)->pn_next;
    *pnp = pn;
}

enum Truthiness { Truthy, Falsy, Unknown };

static Truthiness
Boolish(ParseNode *pn)
{
    switch (pn->getKind()) {
      case PNK_NUMBER:
        return (pn->pn_dval != 0 && !IsNaN(pn->pn_dval)) ? Truthy : Falsy;

      case PNK_STRING:
        return (pn->pn_atom->length() > 0) ? Truthy : Falsy;

      case PNK_TRUE:
      case PNK_FUNCTION:
      case PNK_GENEXP:
        return Truthy;

      case PNK_FALSE:
      case PNK_NULL:
        return Falsy;

      default:
        return Unknown;
    }
}

// Expressions that appear in a few specific places are treated specially
// during constant folding. This enum tells where a parse node appears.
MOZ_BEGIN_ENUM_CLASS(SyntacticContext, int)
    // pn is an expression, and it appears in a context where only its side
    // effects and truthiness matter: the condition of an if statement,
    // conditional expression, while loop, or for(;;) loop; or an operand of &&
    // or || in such a context.
    Condition,

    // pn is the operand of the 'delete' keyword.
    Delete,

    // Any other syntactic context.
    Other
MOZ_END_ENUM_CLASS(SyntacticContext)

static SyntacticContext
condIf(const ParseNode *pn, ParseNodeKind kind)
{
    return pn->isKind(kind) ? SyntacticContext::Condition : SyntacticContext::Other;
}

static bool
Fold(ExclusiveContext *cx, ParseNode **pnp,
     FullParseHandler &handler, const CompileOptions &options,
     bool inGenexpLambda, SyntacticContext sc)
{
    ParseNode *pn = *pnp;
    ParseNode *pn1 = nullptr, *pn2 = nullptr, *pn3 = nullptr;

    JS_CHECK_RECURSION(cx, return false);

    // First, recursively fold constants on the children of this node.
    switch (pn->getArity()) {
      case PN_CODE:
        if (pn->isKind(PNK_FUNCTION) &&
            pn->pn_funbox->useAsmOrInsideUseAsm() && options.asmJSOption)
        {
            return true;
        }
        if (pn->getKind() == PNK_MODULE) {
            if (!Fold(cx, &pn->pn_body, handler, options, false, SyntacticContext::Other))
                return false;
        } else {
            // Note: pn_body is nullptr for functions which are being lazily parsed.
            JS_ASSERT(pn->getKind() == PNK_FUNCTION);
            if (pn->pn_body) {
                if (!Fold(cx, &pn->pn_body, handler, options, pn->pn_funbox->inGenexpLambda,
                          SyntacticContext::Other))
                    return false;
            }
        }
        break;

      case PN_LIST:
      {
        // Propagate Condition context through logical connectives.
        SyntacticContext kidsc = SyntacticContext::Other;
        if (pn->isKind(PNK_OR) || pn->isKind(PNK_AND))
            kidsc = sc;

        // Don't fold a parenthesized call expression. See bug 537673.
        ParseNode **listp = &pn->pn_head;
        if ((pn->isKind(PNK_CALL) || pn->isKind(PNK_NEW)) && (*listp)->isInParens())
            listp = &(*listp)->pn_next;

        for (; *listp; listp = &(*listp)->pn_next) {
            if (!Fold(cx, listp, handler, options, inGenexpLambda, kidsc))
                return false;
        }

        // If the last node in the list was replaced, pn_tail points into the wrong node.
        pn->pn_tail = listp;

        // Save the list head in pn1 for later use.
        pn1 = pn->pn_head;
        pn2 = nullptr;
        break;
      }

      case PN_TERNARY:
        /* Any kid may be null (e.g. for (;;)). */
        if (pn->pn_kid1) {
            if (!Fold(cx, &pn->pn_kid1, handler, options, inGenexpLambda, condIf(pn, PNK_IF)))
                return false;
        }
        pn1 = pn->pn_kid1;

        if (pn->pn_kid2) {
            if (!Fold(cx, &pn->pn_kid2, handler, options, inGenexpLambda, condIf(pn, PNK_FORHEAD)))
                return false;
            if (pn->isKind(PNK_FORHEAD) && pn->pn_kid2->isKind(PNK_TRUE)) {
                handler.freeTree(pn->pn_kid2);
                pn->pn_kid2 = nullptr;
            }
        }
        pn2 = pn->pn_kid2;

        if (pn->pn_kid3) {
            if (!Fold(cx, &pn->pn_kid3, handler, options, inGenexpLambda, SyntacticContext::Other))
                return false;
        }
        pn3 = pn->pn_kid3;
        break;

      case PN_BINARY:
        if (pn->isKind(PNK_OR) || pn->isKind(PNK_AND)) {
            // Propagate Condition context through logical connectives.
            SyntacticContext kidsc = SyntacticContext::Other;
            if (sc == SyntacticContext::Condition)
                kidsc = sc;
            if (!Fold(cx, &pn->pn_left, handler, options, inGenexpLambda, kidsc))
                return false;
            if (!Fold(cx, &pn->pn_right, handler, options, inGenexpLambda, kidsc))
                return false;
        } else {
            /* First kid may be null (for default case in switch). */
            if (pn->pn_left) {
                if (!Fold(cx, &pn->pn_left, handler, options, inGenexpLambda, condIf(pn, PNK_WHILE)))
                    return false;
            }
            if (!Fold(cx, &pn->pn_right, handler, options, inGenexpLambda, condIf(pn, PNK_DOWHILE)))
                return false;
        }
        pn1 = pn->pn_left;
        pn2 = pn->pn_right;
        break;

      case PN_UNARY:
        /*
         * Kludge to deal with typeof expressions: because constant folding
         * can turn an expression into a name node, we have to check here,
         * before folding, to see if we should throw undefined name errors.
         *
         * NB: We know that if pn->pn_op is JSOP_TYPEOF, pn1 will not be
         * null. This assumption does not hold true for other unary
         * expressions.
         */
        if (pn->isKind(PNK_TYPEOF) && !pn->pn_kid->isKind(PNK_NAME))
            pn->setOp(JSOP_TYPEOFEXPR);

        if (pn->pn_kid) {
            SyntacticContext kidsc =
                pn->isKind(PNK_NOT)
                ? SyntacticContext::Condition
                : pn->isKind(PNK_DELETE)
                ? SyntacticContext::Delete
                : SyntacticContext::Other;
            if (!Fold(cx, &pn->pn_kid, handler, options, inGenexpLambda, kidsc))
                return false;
        }
        pn1 = pn->pn_kid;
        break;

      case PN_NAME:
        /*
         * Skip pn1 down along a chain of dotted member expressions to avoid
         * excessive recursion.  Our only goal here is to fold constants (if
         * any) in the primary expression operand to the left of the first
         * dot in the chain.
         */
        if (!pn->isUsed()) {
            ParseNode **lhsp = &pn->pn_expr;
            while (*lhsp && (*lhsp)->isArity(PN_NAME) && !(*lhsp)->isUsed())
                lhsp = &(*lhsp)->pn_expr;
            if (*lhsp && !Fold(cx, lhsp, handler, options, inGenexpLambda, SyntacticContext::Other))
                return false;
            pn1 = *lhsp;
        }
        break;

      case PN_NULLARY:
        break;
    }

    // The immediate child of a PNK_DELETE node should not be replaced
    // with node indicating a different syntactic form; |delete x| is not
    // the same as |delete (true && x)|. See bug 888002.
    //
    // pn is the immediate child in question. Its descendents were already
    // constant-folded above, so we're done.
    if (sc == SyntacticContext::Delete)
        return true;

    switch (pn->getKind()) {
      case PNK_IF:
        if (ContainsVarOrConst(pn2) || ContainsVarOrConst(pn3))
            break;
        /* FALL THROUGH */

      case PNK_CONDITIONAL:
        /* Reduce 'if (C) T; else E' into T for true C, E for false. */
        switch (pn1->getKind()) {
          case PNK_NUMBER:
            if (pn1->pn_dval == 0 || IsNaN(pn1->pn_dval))
                pn2 = pn3;
            break;
          case PNK_STRING:
            if (pn1->pn_atom->length() == 0)
                pn2 = pn3;
            break;
          case PNK_TRUE:
            break;
          case PNK_FALSE:
          case PNK_NULL:
            pn2 = pn3;
            break;
          default:
            /* Early return to dodge common code that copies pn2 to pn. */
            return true;
        }

#if JS_HAS_GENERATOR_EXPRS
        /* Don't fold a trailing |if (0)| in a generator expression. */
        if (!pn2 && inGenexpLambda)
            break;
#endif

        if (pn2 && !pn2->isDefn()) {
            ReplaceNode(pnp, pn2);
            pn = pn2;
        }
        if (!pn2 || (pn->isKind(PNK_SEMI) && !pn->pn_kid)) {
            /*
             * False condition and no else, or an empty then-statement was
             * moved up over pn.  Either way, make pn an empty block (not an
             * empty statement, which does not decompile, even when labeled).
             * NB: pn must be a PNK_IF as PNK_CONDITIONAL can never have a null
             * kid or an empty statement for a child.
             */
            pn->setKind(PNK_STATEMENTLIST);
            pn->setArity(PN_LIST);
            pn->makeEmpty();
        }
        if (pn3 && pn3 != pn2)
            handler.freeTree(pn3);
        break;

      case PNK_OR:
      case PNK_AND:
        if (sc == SyntacticContext::Condition) {
            if (pn->isArity(PN_LIST)) {
                ParseNode **listp = &pn->pn_head;
                JS_ASSERT(*listp == pn1);
                uint32_t orig = pn->pn_count;
                do {
                    Truthiness t = Boolish(pn1);
                    if (t == Unknown) {
                        listp = &pn1->pn_next;
                        continue;
                    }
                    if ((t == Truthy) == pn->isKind(PNK_OR)) {
                        for (pn2 = pn1->pn_next; pn2; pn2 = pn3) {
                            pn3 = pn2->pn_next;
                            handler.freeTree(pn2);
                            --pn->pn_count;
                        }
                        pn1->pn_next = nullptr;
                        break;
                    }
                    JS_ASSERT((t == Truthy) == pn->isKind(PNK_AND));
                    if (pn->pn_count == 1)
                        break;
                    *listp = pn1->pn_next;
                    handler.freeTree(pn1);
                    --pn->pn_count;
                } while ((pn1 = *listp) != nullptr);

                // We may have to change arity from LIST to BINARY.
                pn1 = pn->pn_head;
                if (pn->pn_count == 2) {
                    pn2 = pn1->pn_next;
                    pn1->pn_next = nullptr;
                    JS_ASSERT(!pn2->pn_next);
                    pn->setArity(PN_BINARY);
                    pn->pn_left = pn1;
                    pn->pn_right = pn2;
                } else if (pn->pn_count == 1) {
                    ReplaceNode(pnp, pn1);
                    pn = pn1;
                } else if (orig != pn->pn_count) {
                    // Adjust list tail.
                    pn2 = pn1->pn_next;
                    for (; pn1; pn2 = pn1, pn1 = pn1->pn_next)
                        ;
                    pn->pn_tail = &pn2->pn_next;
                }
            } else {
                Truthiness t = Boolish(pn1);
                if (t != Unknown) {
                    if ((t == Truthy) == pn->isKind(PNK_OR)) {
                        handler.freeTree(pn2);
                        ReplaceNode(pnp, pn1);
                        pn = pn1;
                    } else {
                        JS_ASSERT((t == Truthy) == pn->isKind(PNK_AND));
                        handler.freeTree(pn1);
                        ReplaceNode(pnp, pn2);
                        pn = pn2;
                    }
                }
            }
        }
        break;

      case PNK_SUBASSIGN:
      case PNK_BITORASSIGN:
      case PNK_BITXORASSIGN:
      case PNK_BITANDASSIGN:
      case PNK_LSHASSIGN:
      case PNK_RSHASSIGN:
      case PNK_URSHASSIGN:
      case PNK_MULASSIGN:
      case PNK_DIVASSIGN:
      case PNK_MODASSIGN:
        /*
         * Compound operators such as *= should be subject to folding, in case
         * the left-hand side is constant, and so that the decompiler produces
         * the same string that you get from decompiling a script or function
         * compiled from that same string.  += is special and so must be
         * handled below.
         */
        goto do_binary_op;

      case PNK_ADDASSIGN:
        JS_ASSERT(pn->isOp(JSOP_ADD));
        /* FALL THROUGH */
      case PNK_ADD:
        if (pn->isArity(PN_LIST)) {
            /*
             * Any string literal term with all others number or string means
             * this is a concatenation.  If any term is not a string or number
             * literal, we can't fold.
             */
            JS_ASSERT(pn->pn_count > 2);
            if (pn->pn_xflags & PNX_CANTFOLD)
                return true;
            if (pn->pn_xflags != PNX_STRCAT)
                goto do_binary_op;

            /* Ok, we're concatenating: convert non-string constant operands. */
            size_t length = 0;
            for (pn2 = pn1; pn2; pn2 = pn2->pn_next) {
                if (!FoldType(cx, pn2, PNK_STRING))
                    return false;
                /* XXX fold only if all operands convert to string */
                if (!pn2->isKind(PNK_STRING))
                    return true;
                length += pn2->pn_atom->length();
            }

            /* Allocate a new buffer and string descriptor for the result. */
            jschar *chars = cx->pod_malloc<jschar>(length + 1);
            if (!chars)
                return false;
            chars[length] = 0;
            JSString *str = js_NewString<CanGC>(cx, chars, length);
            if (!str) {
                js_free(chars);
                return false;
            }

            /* Fill the buffer, advancing chars and recycling kids as we go. */
            for (pn2 = pn1; pn2; pn2 = handler.freeTree(pn2)) {
                JSAtom *atom = pn2->pn_atom;
                size_t length2 = atom->length();
                js_strncpy(chars, atom->chars(), length2);
                chars += length2;
            }
            JS_ASSERT(*chars == 0);

            /* Atomize the result string and mutate pn to refer to it. */
            pn->pn_atom = AtomizeString<CanGC>(cx, str);
            if (!pn->pn_atom)
                return false;
            pn->setKind(PNK_STRING);
            pn->setOp(JSOP_STRING);
            pn->setArity(PN_NULLARY);
            break;
        }

        /* Handle a binary string concatenation. */
        JS_ASSERT(pn->isArity(PN_BINARY));
        if (pn1->isKind(PNK_STRING) || pn2->isKind(PNK_STRING)) {
            if (!FoldType(cx, !pn1->isKind(PNK_STRING) ? pn1 : pn2, PNK_STRING))
                return false;
            if (!pn1->isKind(PNK_STRING) || !pn2->isKind(PNK_STRING))
                return true;
            RootedString left(cx, pn1->pn_atom);
            RootedString right(cx, pn2->pn_atom);
            RootedString str(cx, ConcatStrings<CanGC>(cx, left, right));
            if (!str)
                return false;
            pn->pn_atom = AtomizeString<CanGC>(cx, str);
            if (!pn->pn_atom)
                return false;
            pn->setKind(PNK_STRING);
            pn->setOp(JSOP_STRING);
            pn->setArity(PN_NULLARY);
            handler.freeTree(pn1);
            handler.freeTree(pn2);
            break;
        }

        /* Can't concatenate string literals, let's try numbers. */
        goto do_binary_op;

      case PNK_SUB:
      case PNK_STAR:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_DIV:
      case PNK_MOD:
      do_binary_op:
        if (pn->isArity(PN_LIST)) {
            JS_ASSERT(pn->pn_count > 2);
            for (pn2 = pn1; pn2; pn2 = pn2->pn_next) {
                if (!FoldType(cx, pn2, PNK_NUMBER))
                    return false;
            }
            for (pn2 = pn1; pn2; pn2 = pn2->pn_next) {
                /* XXX fold only if all operands convert to number */
                if (!pn2->isKind(PNK_NUMBER))
                    break;
            }
            if (!pn2) {
                JSOp op = pn->getOp();

                pn2 = pn1->pn_next;
                pn3 = pn2->pn_next;
                if (!FoldBinaryNumeric(cx, op, pn1, pn2, pn))
                    return false;
                while ((pn2 = pn3) != nullptr) {
                    pn3 = pn2->pn_next;
                    if (!FoldBinaryNumeric(cx, op, pn, pn2, pn))
                        return false;
                }
            }
        } else {
            JS_ASSERT(pn->isArity(PN_BINARY));
            if (!FoldType(cx, pn1, PNK_NUMBER) ||
                !FoldType(cx, pn2, PNK_NUMBER)) {
                return false;
            }
            if (pn1->isKind(PNK_NUMBER) && pn2->isKind(PNK_NUMBER)) {
                if (!FoldBinaryNumeric(cx, pn->getOp(), pn1, pn2, pn))
                    return false;
            }
        }
        break;

      case PNK_TYPEOF:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG:
        if (pn1->isKind(PNK_NUMBER)) {
            double d;

            /* Operate on one numeric constant. */
            d = pn1->pn_dval;
            switch (pn->getKind()) {
              case PNK_BITNOT:
                d = ~ToInt32(d);
                break;

              case PNK_NEG:
                d = -d;
                break;

              case PNK_POS:
                break;

              case PNK_NOT:
                if (d == 0 || IsNaN(d)) {
                    pn->setKind(PNK_TRUE);
                    pn->setOp(JSOP_TRUE);
                } else {
                    pn->setKind(PNK_FALSE);
                    pn->setOp(JSOP_FALSE);
                }
                pn->setArity(PN_NULLARY);
                /* FALL THROUGH */

              default:
                /* Return early to dodge the common PNK_NUMBER code. */
                return true;
            }
            pn->setKind(PNK_NUMBER);
            pn->setOp(JSOP_DOUBLE);
            pn->setArity(PN_NULLARY);
            pn->pn_dval = d;
            handler.freeTree(pn1);
        } else if (pn1->isKind(PNK_TRUE) || pn1->isKind(PNK_FALSE)) {
            if (pn->isKind(PNK_NOT)) {
                ReplaceNode(pnp, pn1);
                pn = pn1;
                if (pn->isKind(PNK_TRUE)) {
                    pn->setKind(PNK_FALSE);
                    pn->setOp(JSOP_FALSE);
                } else {
                    pn->setKind(PNK_TRUE);
                    pn->setOp(JSOP_TRUE);
                }
            }
        }
        break;

      case PNK_ELEM: {
        // An indexed expression, pn1[pn2]. A few cases can be improved.
        PropertyName *name = nullptr;
        if (pn2->isKind(PNK_STRING)) {
            JSAtom *atom = pn2->pn_atom;
            uint32_t index;

            if (atom->isIndex(&index)) {
                // Optimization 1: We have something like pn1["100"]. This is
                // equivalent to pn1[100] which is faster.
                pn2->setKind(PNK_NUMBER);
                pn2->setOp(JSOP_DOUBLE);
                pn2->pn_dval = index;
            } else {
                name = atom->asPropertyName();
            }
        } else if (pn2->isKind(PNK_NUMBER)) {
            double number = pn2->pn_dval;
            if (number != ToUint32(number)) {
                // Optimization 2: We have something like pn1[3.14]. The number
                // is not an array index. This is equivalent to pn1["3.14"]
                // which enables optimization 3 below.
                JSAtom *atom = ToAtom<NoGC>(cx, DoubleValue(number));
                if (!atom)
                    return false;
                name = atom->asPropertyName();
            }
        }

        if (name && NameToId(name) == types::IdToTypeId(NameToId(name))) {
            // Optimization 3: We have pn1["foo"] where foo is not an index.
            // Convert to a property access (like pn1.foo) which we optimize
            // better downstream. Don't bother with this for names which TI
            // considers to be indexes, to simplify downstream analysis.
            ParseNode *expr = handler.newPropertyAccess(pn->pn_left, name, pn->pn_pos.end);
            if (!expr)
                return false;
            ReplaceNode(pnp, expr);

            pn->pn_left = nullptr;
            pn->pn_right = nullptr;
            handler.freeTree(pn);
            pn = expr;
        }
        break;
      }

      default:;
    }

    if (sc == SyntacticContext::Condition) {
        Truthiness t = Boolish(pn);
        if (t != Unknown) {
            /*
             * We can turn function nodes into constant nodes here, but mutating function
             * nodes is tricky --- in particular, mutating a function node that appears on
             * a method list corrupts the method list. However, methods are M's in
             * statements of the form 'this.foo = M;', which we never fold, so we're okay.
             */
            handler.prepareNodeForMutation(pn);
            if (t == Truthy) {
                pn->setKind(PNK_TRUE);
                pn->setOp(JSOP_TRUE);
            } else {
                pn->setKind(PNK_FALSE);
                pn->setOp(JSOP_FALSE);
            }
            pn->setArity(PN_NULLARY);
        }
    }

    return true;
}

bool
frontend::FoldConstants(ExclusiveContext *cx, ParseNode **pnp, Parser<FullParseHandler> *parser)
{
    // Don't fold constants if the code has requested "use asm" as
    // constant-folding will misrepresent the source text for the purpose
    // of type checking. (Also guard against entering a function containing
    // "use asm", see PN_FUNC case below.)
    if (parser->pc->useAsmOrInsideUseAsm() && parser->options().asmJSOption)
        return true;

    return Fold(cx, pnp, parser->handler, parser->options(), false, SyntacticContext::Other);
}

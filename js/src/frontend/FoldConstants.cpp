/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/FoldConstants.h"

#include "mozilla/FloatingPoint.h"

#include "jslibmath.h"

#include "frontend/ParseNode.h"
#include "frontend/Parser.h"
#include "js/Conversions.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::frontend;

using mozilla::IsNaN;
using mozilla::IsNegative;
using mozilla::NegativeInfinity;
using mozilla::PositiveInfinity;
using JS::GenericNaN;
using JS::ToInt32;
using JS::ToUint32;

static bool
ContainsHoistedDeclaration(JSContext* cx, ParseNode* node, bool* result);

static bool
ListContainsHoistedDeclaration(JSContext* cx, ListNode* list, bool* result)
{
    for (ParseNode* node = list->pn_head; node; node = node->pn_next) {
        if (!ContainsHoistedDeclaration(cx, node, result))
            return false;
        if (*result)
            return true;
    }

    *result = false;
    return true;
}

// Determines whether the given ParseNode contains any declarations whose
// visibility will extend outside the node itself -- that is, whether the
// ParseNode contains any var statements.
//
// THIS IS NOT A GENERAL-PURPOSE FUNCTION.  It is only written to work in the
// specific context of deciding that |node|, as one arm of a ParseNodeKind::PNK_IF
// controlled by a constant condition, contains a declaration that forbids
// |node| being completely eliminated as dead.
static bool
ContainsHoistedDeclaration(JSContext* cx, ParseNode* node, bool* result)
{
    if (!CheckRecursionLimit(cx))
        return false;

  restart:

    // With a better-typed AST, we would have distinct parse node classes for
    // expressions and for statements and would characterize expressions with
    // ExpressionKind and statements with StatementKind.  Perhaps someday.  In
    // the meantime we must characterize every ParseNodeKind, even the
    // expression/sub-expression ones that, if we handle all statement kinds
    // correctly, we'll never see.
    switch (node->getKind()) {
      // Base case.
      case ParseNodeKind::PNK_VAR:
        *result = true;
        return true;

      // Non-global lexical declarations are block-scoped (ergo not hoistable).
      case ParseNodeKind::PNK_LET:
      case ParseNodeKind::PNK_CONST:
        MOZ_ASSERT(node->isArity(PN_LIST));
        *result = false;
        return true;

      // Similarly to the lexical declarations above, classes cannot add hoisted
      // declarations
      case ParseNodeKind::PNK_CLASS:
        MOZ_ASSERT(node->isArity(PN_TERNARY));
        *result = false;
        return true;

      // Function declarations *can* be hoisted declarations.  But in the
      // magical world of the rewritten frontend, the declaration necessitated
      // by a nested function statement, not at body level, doesn't require
      // that we preserve an unreachable function declaration node against
      // dead-code removal.
      case ParseNodeKind::PNK_FUNCTION:
        MOZ_ASSERT(node->isArity(PN_CODE));
        *result = false;
        return true;

      case ParseNodeKind::PNK_MODULE:
        *result = false;
        return true;

      // Statements with no sub-components at all.
      case ParseNodeKind::PNK_NOP: // induced by function f() {} function f() {}
      case ParseNodeKind::PNK_DEBUGGER:
        MOZ_ASSERT(node->isArity(PN_NULLARY));
        *result = false;
        return true;

      // Statements containing only an expression have no declarations.
      case ParseNodeKind::PNK_SEMI:
      case ParseNodeKind::PNK_THROW:
      case ParseNodeKind::PNK_RETURN:
        MOZ_ASSERT(node->isArity(PN_UNARY));
        *result = false;
        return true;

      // These two aren't statements in the spec, but we sometimes insert them
      // in statement lists anyway.
      case ParseNodeKind::PNK_INITIALYIELD:
      case ParseNodeKind::PNK_YIELD_STAR:
      case ParseNodeKind::PNK_YIELD:
        MOZ_ASSERT(node->isArity(PN_UNARY));
        *result = false;
        return true;

      // Other statements with no sub-statement components.
      case ParseNodeKind::PNK_BREAK:
      case ParseNodeKind::PNK_CONTINUE:
      case ParseNodeKind::PNK_IMPORT:
      case ParseNodeKind::PNK_IMPORT_SPEC_LIST:
      case ParseNodeKind::PNK_IMPORT_SPEC:
      case ParseNodeKind::PNK_EXPORT_FROM:
      case ParseNodeKind::PNK_EXPORT_DEFAULT:
      case ParseNodeKind::PNK_EXPORT_SPEC_LIST:
      case ParseNodeKind::PNK_EXPORT_SPEC:
      case ParseNodeKind::PNK_EXPORT:
      case ParseNodeKind::PNK_EXPORT_BATCH_SPEC:
        *result = false;
        return true;

      // Statements possibly containing hoistable declarations only in the left
      // half, in ParseNode terms -- the loop body in AST terms.
      case ParseNodeKind::PNK_DOWHILE:
        return ContainsHoistedDeclaration(cx, node->pn_left, result);

      // Statements possibly containing hoistable declarations only in the
      // right half, in ParseNode terms -- the loop body or nested statement
      // (usually a block statement), in AST terms.
      case ParseNodeKind::PNK_WHILE:
      case ParseNodeKind::PNK_WITH:
        return ContainsHoistedDeclaration(cx, node->pn_right, result);

      case ParseNodeKind::PNK_LABEL:
        return ContainsHoistedDeclaration(cx, node->pn_expr, result);

      // Statements with more complicated structures.

      // if-statement nodes may have hoisted declarations in their consequent
      // and alternative components.
      case ParseNodeKind::PNK_IF: {
        MOZ_ASSERT(node->isArity(PN_TERNARY));

        ParseNode* consequent = node->pn_kid2;
        if (!ContainsHoistedDeclaration(cx, consequent, result))
            return false;
        if (*result)
            return true;

        if ((node = node->pn_kid3))
            goto restart;

        *result = false;
        return true;
      }

      // try-statements have statements to execute, and one or both of a
      // catch-list and a finally-block.
      case ParseNodeKind::PNK_TRY: {
        MOZ_ASSERT(node->isArity(PN_TERNARY));
        MOZ_ASSERT(node->pn_kid2 || node->pn_kid3,
                   "must have either catch(es) or finally");

        ParseNode* tryBlock = node->pn_kid1;
        if (!ContainsHoistedDeclaration(cx, tryBlock, result))
            return false;
        if (*result)
            return true;

        if (ParseNode* catchScope = node->pn_kid2) {
            MOZ_ASSERT(catchScope->isKind(ParseNodeKind::PNK_LEXICALSCOPE));

            ParseNode* catchNode = catchScope->pn_expr;
            MOZ_ASSERT(catchNode->isKind(ParseNodeKind::PNK_CATCH));

            ParseNode* catchStatements = catchNode->pn_right;
            if (!ContainsHoistedDeclaration(cx, catchStatements, result))
                return false;
            if (*result)
                return true;
        }

        if (ParseNode* finallyBlock = node->pn_kid3)
            return ContainsHoistedDeclaration(cx, finallyBlock, result);

        *result = false;
        return true;
      }

      // A switch node's left half is an expression; only its right half (a
      // list of cases/defaults, or a block node) could contain hoisted
      // declarations.
      case ParseNodeKind::PNK_SWITCH:
        MOZ_ASSERT(node->isArity(PN_BINARY));
        return ContainsHoistedDeclaration(cx, node->pn_right, result);

      case ParseNodeKind::PNK_CASE:
        return ContainsHoistedDeclaration(cx, node->as<CaseClause>().statementList(), result);

      case ParseNodeKind::PNK_FOR: {
        MOZ_ASSERT(node->isArity(PN_BINARY));

        ParseNode* loopHead = node->pn_left;
        MOZ_ASSERT(loopHead->isKind(ParseNodeKind::PNK_FORHEAD) ||
                   loopHead->isKind(ParseNodeKind::PNK_FORIN) ||
                   loopHead->isKind(ParseNodeKind::PNK_FOROF));

        if (loopHead->isKind(ParseNodeKind::PNK_FORHEAD)) {
            // for (init?; cond?; update?), with only init possibly containing
            // a hoisted declaration.  (Note: a lexical-declaration |init| is
            // (at present) hoisted in SpiderMonkey parlance -- but such
            // hoisting doesn't extend outside of this statement, so it is not
            // hoisting in the sense meant by ContainsHoistedDeclaration.)
            MOZ_ASSERT(loopHead->isArity(PN_TERNARY));

            ParseNode* init = loopHead->pn_kid1;
            if (init && init->isKind(ParseNodeKind::PNK_VAR)) {
                *result = true;
                return true;
            }
        } else {
            MOZ_ASSERT(loopHead->isKind(ParseNodeKind::PNK_FORIN) ||
                       loopHead->isKind(ParseNodeKind::PNK_FOROF));

            // for each? (target in ...), where only target may introduce
            // hoisted declarations.
            //
            //   -- or --
            //
            // for (target of ...), where only target may introduce hoisted
            // declarations.
            //
            // Either way, if |target| contains a declaration, it's |loopHead|'s
            // first kid.
            MOZ_ASSERT(loopHead->isArity(PN_TERNARY));

            ParseNode* decl = loopHead->pn_kid1;
            if (decl && decl->isKind(ParseNodeKind::PNK_VAR)) {
                *result = true;
                return true;
            }
        }

        ParseNode* loopBody = node->pn_right;
        return ContainsHoistedDeclaration(cx, loopBody, result);
      }

      case ParseNodeKind::PNK_LEXICALSCOPE: {
        MOZ_ASSERT(node->isArity(PN_SCOPE));
        ParseNode* expr = node->pn_expr;

        if (expr->isKind(ParseNodeKind::PNK_FOR) || expr->isKind(ParseNodeKind::PNK_FUNCTION))
            return ContainsHoistedDeclaration(cx, expr, result);

        MOZ_ASSERT(expr->isKind(ParseNodeKind::PNK_STATEMENTLIST));
        return ListContainsHoistedDeclaration(cx, &node->pn_expr->as<ListNode>(), result);
      }

      // List nodes with all non-null children.
      case ParseNodeKind::PNK_STATEMENTLIST:
        return ListContainsHoistedDeclaration(cx, &node->as<ListNode>(), result);

      // Grammar sub-components that should never be reached directly by this
      // method, because some parent component should have asserted itself.
      case ParseNodeKind::PNK_OBJECT_PROPERTY_NAME:
      case ParseNodeKind::PNK_COMPUTED_NAME:
      case ParseNodeKind::PNK_SPREAD:
      case ParseNodeKind::PNK_MUTATEPROTO:
      case ParseNodeKind::PNK_COLON:
      case ParseNodeKind::PNK_SHORTHAND:
      case ParseNodeKind::PNK_CONDITIONAL:
      case ParseNodeKind::PNK_TYPEOFNAME:
      case ParseNodeKind::PNK_TYPEOFEXPR:
      case ParseNodeKind::PNK_AWAIT:
      case ParseNodeKind::PNK_VOID:
      case ParseNodeKind::PNK_NOT:
      case ParseNodeKind::PNK_BITNOT:
      case ParseNodeKind::PNK_DELETENAME:
      case ParseNodeKind::PNK_DELETEPROP:
      case ParseNodeKind::PNK_DELETEELEM:
      case ParseNodeKind::PNK_DELETEEXPR:
      case ParseNodeKind::PNK_POS:
      case ParseNodeKind::PNK_NEG:
      case ParseNodeKind::PNK_PREINCREMENT:
      case ParseNodeKind::PNK_POSTINCREMENT:
      case ParseNodeKind::PNK_PREDECREMENT:
      case ParseNodeKind::PNK_POSTDECREMENT:
      case ParseNodeKind::PNK_OR:
      case ParseNodeKind::PNK_AND:
      case ParseNodeKind::PNK_BITOR:
      case ParseNodeKind::PNK_BITXOR:
      case ParseNodeKind::PNK_BITAND:
      case ParseNodeKind::PNK_STRICTEQ:
      case ParseNodeKind::PNK_EQ:
      case ParseNodeKind::PNK_STRICTNE:
      case ParseNodeKind::PNK_NE:
      case ParseNodeKind::PNK_LT:
      case ParseNodeKind::PNK_LE:
      case ParseNodeKind::PNK_GT:
      case ParseNodeKind::PNK_GE:
      case ParseNodeKind::PNK_INSTANCEOF:
      case ParseNodeKind::PNK_IN:
      case ParseNodeKind::PNK_LSH:
      case ParseNodeKind::PNK_RSH:
      case ParseNodeKind::PNK_URSH:
      case ParseNodeKind::PNK_ADD:
      case ParseNodeKind::PNK_SUB:
      case ParseNodeKind::PNK_STAR:
      case ParseNodeKind::PNK_DIV:
      case ParseNodeKind::PNK_MOD:
      case ParseNodeKind::PNK_POW:
      case ParseNodeKind::PNK_ASSIGN:
      case ParseNodeKind::PNK_ADDASSIGN:
      case ParseNodeKind::PNK_SUBASSIGN:
      case ParseNodeKind::PNK_BITORASSIGN:
      case ParseNodeKind::PNK_BITXORASSIGN:
      case ParseNodeKind::PNK_BITANDASSIGN:
      case ParseNodeKind::PNK_LSHASSIGN:
      case ParseNodeKind::PNK_RSHASSIGN:
      case ParseNodeKind::PNK_URSHASSIGN:
      case ParseNodeKind::PNK_MULASSIGN:
      case ParseNodeKind::PNK_DIVASSIGN:
      case ParseNodeKind::PNK_MODASSIGN:
      case ParseNodeKind::PNK_POWASSIGN:
      case ParseNodeKind::PNK_COMMA:
      case ParseNodeKind::PNK_ARRAY:
      case ParseNodeKind::PNK_OBJECT:
      case ParseNodeKind::PNK_DOT:
      case ParseNodeKind::PNK_ELEM:
      case ParseNodeKind::PNK_CALL:
      case ParseNodeKind::PNK_NAME:
      case ParseNodeKind::PNK_TEMPLATE_STRING:
      case ParseNodeKind::PNK_TEMPLATE_STRING_LIST:
      case ParseNodeKind::PNK_TAGGED_TEMPLATE:
      case ParseNodeKind::PNK_CALLSITEOBJ:
      case ParseNodeKind::PNK_STRING:
      case ParseNodeKind::PNK_REGEXP:
      case ParseNodeKind::PNK_TRUE:
      case ParseNodeKind::PNK_FALSE:
      case ParseNodeKind::PNK_NULL:
      case ParseNodeKind::PNK_RAW_UNDEFINED:
      case ParseNodeKind::PNK_THIS:
      case ParseNodeKind::PNK_ELISION:
      case ParseNodeKind::PNK_NUMBER:
      case ParseNodeKind::PNK_NEW:
      case ParseNodeKind::PNK_GENERATOR:
      case ParseNodeKind::PNK_PARAMSBODY:
      case ParseNodeKind::PNK_CATCH:
      case ParseNodeKind::PNK_FORIN:
      case ParseNodeKind::PNK_FOROF:
      case ParseNodeKind::PNK_FORHEAD:
      case ParseNodeKind::PNK_CLASSMETHOD:
      case ParseNodeKind::PNK_CLASSMETHODLIST:
      case ParseNodeKind::PNK_CLASSNAMES:
      case ParseNodeKind::PNK_NEWTARGET:
      case ParseNodeKind::PNK_POSHOLDER:
      case ParseNodeKind::PNK_SUPERCALL:
      case ParseNodeKind::PNK_SUPERBASE:
      case ParseNodeKind::PNK_SETTHIS:
        MOZ_CRASH("ContainsHoistedDeclaration should have indicated false on "
                  "some parent node without recurring to test this node");

      case ParseNodeKind::PNK_PIPELINE:
        MOZ_ASSERT(node->isArity(PN_LIST));
        *result = false;
        return true;

      case ParseNodeKind::PNK_LIMIT: // invalid sentinel value
        MOZ_CRASH("unexpected ParseNodeKind::PNK_LIMIT in node");
    }

    MOZ_CRASH("invalid node kind");
}

/*
 * Fold from one constant type to another.
 * XXX handles only strings and numbers for now
 */
static bool
FoldType(JSContext* cx, ParseNode* pn, ParseNodeKind kind)
{
    if (!pn->isKind(kind)) {
        switch (kind) {
          case ParseNodeKind::PNK_NUMBER:
            if (pn->isKind(ParseNodeKind::PNK_STRING)) {
                double d;
                if (!StringToNumber(cx, pn->pn_atom, &d))
                    return false;
                pn->pn_dval = d;
                pn->setKind(ParseNodeKind::PNK_NUMBER);
                pn->setOp(JSOP_DOUBLE);
            }
            break;

          case ParseNodeKind::PNK_STRING:
            if (pn->isKind(ParseNodeKind::PNK_NUMBER)) {
                pn->pn_atom = NumberToAtom(cx, pn->pn_dval);
                if (!pn->pn_atom)
                    return false;
                pn->setKind(ParseNodeKind::PNK_STRING);
                pn->setOp(JSOP_STRING);
            }
            break;

          default:;
        }
    }
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
ReplaceNode(ParseNode** pnp, ParseNode* pn)
{
    pn->pn_next = (*pnp)->pn_next;
    *pnp = pn;
}

static bool
IsEffectless(ParseNode* node)
{
    return node->isKind(ParseNodeKind::PNK_TRUE) ||
           node->isKind(ParseNodeKind::PNK_FALSE) ||
           node->isKind(ParseNodeKind::PNK_STRING) ||
           node->isKind(ParseNodeKind::PNK_TEMPLATE_STRING) ||
           node->isKind(ParseNodeKind::PNK_NUMBER) ||
           node->isKind(ParseNodeKind::PNK_NULL) ||
           node->isKind(ParseNodeKind::PNK_RAW_UNDEFINED) ||
           node->isKind(ParseNodeKind::PNK_FUNCTION);
}

enum Truthiness { Truthy, Falsy, Unknown };

static Truthiness
Boolish(ParseNode* pn)
{
    switch (pn->getKind()) {
      case ParseNodeKind::PNK_NUMBER:
        return (pn->pn_dval != 0 && !IsNaN(pn->pn_dval)) ? Truthy : Falsy;

      case ParseNodeKind::PNK_STRING:
      case ParseNodeKind::PNK_TEMPLATE_STRING:
        return (pn->pn_atom->length() > 0) ? Truthy : Falsy;

      case ParseNodeKind::PNK_TRUE:
      case ParseNodeKind::PNK_FUNCTION:
        return Truthy;

      case ParseNodeKind::PNK_FALSE:
      case ParseNodeKind::PNK_NULL:
      case ParseNodeKind::PNK_RAW_UNDEFINED:
        return Falsy;

      case ParseNodeKind::PNK_VOID: {
        // |void <foo>| evaluates to |undefined| which isn't truthy.  But the
        // sense of this method requires that the expression be literally
        // replaceable with true/false: not the case if the nested expression
        // is effectful, might throw, &c.  Walk past the |void| (and nested
        // |void| expressions, for good measure) and check that the nested
        // expression doesn't break this requirement before indicating falsity.
        do {
            pn = pn->pn_kid;
        } while (pn->isKind(ParseNodeKind::PNK_VOID));

        return IsEffectless(pn) ? Falsy : Unknown;
      }

      default:
        return Unknown;
    }
}

static bool
Fold(JSContext* cx, ParseNode** pnp, GeneralParser<FullParseHandler, char16_t>& parser);

static bool
FoldCondition(JSContext* cx, ParseNode** nodePtr,
              GeneralParser<FullParseHandler, char16_t>& parser)
{
    // Conditions fold like any other expression...
    if (!Fold(cx, nodePtr, parser))
        return false;

    // ...but then they sometimes can be further folded to constants.
    ParseNode* node = *nodePtr;
    Truthiness t = Boolish(node);
    if (t != Unknown) {
        // We can turn function nodes into constant nodes here, but mutating
        // function nodes is tricky --- in particular, mutating a function node
        // that appears on a method list corrupts the method list. However,
        // methods are M's in statements of the form 'this.foo = M;', which we
        // never fold, so we're okay.
        parser.prepareNodeForMutation(node);
        if (t == Truthy) {
            node->setKind(ParseNodeKind::PNK_TRUE);
            node->setOp(JSOP_TRUE);
        } else {
            node->setKind(ParseNodeKind::PNK_FALSE);
            node->setOp(JSOP_FALSE);
        }
        node->setArity(PN_NULLARY);
    }

    return true;
}

static bool
FoldTypeOfExpr(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_TYPEOFEXPR));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser))
        return false;

    // Constant-fold the entire |typeof| if given a constant with known type.
    RootedPropertyName result(cx);
    if (expr->isKind(ParseNodeKind::PNK_STRING) || expr->isKind(ParseNodeKind::PNK_TEMPLATE_STRING))
        result = cx->names().string;
    else if (expr->isKind(ParseNodeKind::PNK_NUMBER))
        result = cx->names().number;
    else if (expr->isKind(ParseNodeKind::PNK_NULL))
        result = cx->names().object;
    else if (expr->isKind(ParseNodeKind::PNK_TRUE) || expr->isKind(ParseNodeKind::PNK_FALSE))
        result = cx->names().boolean;
    else if (expr->isKind(ParseNodeKind::PNK_FUNCTION))
        result = cx->names().function;

    if (result) {
        parser.prepareNodeForMutation(node);

        node->setKind(ParseNodeKind::PNK_STRING);
        node->setArity(PN_NULLARY);
        node->setOp(JSOP_NOP);
        node->pn_atom = result;
    }

    return true;
}

static bool
FoldDeleteExpr(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_DELETEEXPR));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser))
        return false;

    // Expression deletion evaluates the expression, then evaluates to true.
    // For effectless expressions, eliminate the expression evaluation.
    if (IsEffectless(expr)) {
        parser.prepareNodeForMutation(node);
        node->setKind(ParseNodeKind::PNK_TRUE);
        node->setArity(PN_NULLARY);
        node->setOp(JSOP_TRUE);
    }

    return true;
}

static bool
FoldDeleteElement(JSContext* cx, ParseNode* node,
                  GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_DELETEELEM));
    MOZ_ASSERT(node->isArity(PN_UNARY));
    MOZ_ASSERT(node->pn_kid->isKind(ParseNodeKind::PNK_ELEM));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser))
        return false;

    // If we're deleting an element, but constant-folding converted our
    // element reference into a dotted property access, we must *also*
    // morph the node's kind.
    //
    // In principle this also applies to |super["foo"] -> super.foo|,
    // but we don't constant-fold |super["foo"]| yet.
    MOZ_ASSERT(expr->isKind(ParseNodeKind::PNK_ELEM) || expr->isKind(ParseNodeKind::PNK_DOT));
    if (expr->isKind(ParseNodeKind::PNK_DOT))
        node->setKind(ParseNodeKind::PNK_DELETEPROP);

    return true;
}

static bool
FoldDeleteProperty(JSContext* cx, ParseNode* node,
                   GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_DELETEPROP));
    MOZ_ASSERT(node->isArity(PN_UNARY));
    MOZ_ASSERT(node->pn_kid->isKind(ParseNodeKind::PNK_DOT));

    ParseNode*& expr = node->pn_kid;
#ifdef DEBUG
    ParseNodeKind oldKind = expr->getKind();
#endif

    if (!Fold(cx, &expr, parser))
        return false;

    MOZ_ASSERT(expr->isKind(oldKind),
               "kind should have remained invariant under folding");

    return true;
}

static bool
FoldNot(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_NOT));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!FoldCondition(cx, &expr, parser))
        return false;

    if (expr->isKind(ParseNodeKind::PNK_NUMBER)) {
        double d = expr->pn_dval;

        parser.prepareNodeForMutation(node);
        if (d == 0 || IsNaN(d)) {
            node->setKind(ParseNodeKind::PNK_TRUE);
            node->setOp(JSOP_TRUE);
        } else {
            node->setKind(ParseNodeKind::PNK_FALSE);
            node->setOp(JSOP_FALSE);
        }
        node->setArity(PN_NULLARY);
    } else if (expr->isKind(ParseNodeKind::PNK_TRUE) || expr->isKind(ParseNodeKind::PNK_FALSE)) {
        bool newval = !expr->isKind(ParseNodeKind::PNK_TRUE);

        parser.prepareNodeForMutation(node);
        node->setKind(newval ? ParseNodeKind::PNK_TRUE : ParseNodeKind::PNK_FALSE);
        node->setArity(PN_NULLARY);
        node->setOp(newval ? JSOP_TRUE : JSOP_FALSE);
    }

    return true;
}

static bool
FoldUnaryArithmetic(JSContext* cx, ParseNode* node,
                    GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_BITNOT) ||
               node->isKind(ParseNodeKind::PNK_POS) ||
               node->isKind(ParseNodeKind::PNK_NEG),
               "need a different method for this node kind");
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser))
        return false;

    if (expr->isKind(ParseNodeKind::PNK_NUMBER) ||
        expr->isKind(ParseNodeKind::PNK_TRUE) ||
        expr->isKind(ParseNodeKind::PNK_FALSE))
    {
        double d = expr->isKind(ParseNodeKind::PNK_NUMBER)
                   ? expr->pn_dval
                   : double(expr->isKind(ParseNodeKind::PNK_TRUE));

        if (node->isKind(ParseNodeKind::PNK_BITNOT))
            d = ~ToInt32(d);
        else if (node->isKind(ParseNodeKind::PNK_NEG))
            d = -d;
        else
            MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_POS)); // nothing to do

        parser.prepareNodeForMutation(node);
        node->setKind(ParseNodeKind::PNK_NUMBER);
        node->setOp(JSOP_DOUBLE);
        node->setArity(PN_NULLARY);
        node->pn_dval = d;
    }

    return true;
}

static bool
FoldIncrementDecrement(JSContext* cx, ParseNode* node,
                       GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_PREINCREMENT) ||
               node->isKind(ParseNodeKind::PNK_POSTINCREMENT) ||
               node->isKind(ParseNodeKind::PNK_PREDECREMENT) ||
               node->isKind(ParseNodeKind::PNK_POSTDECREMENT));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& target = node->pn_kid;
    MOZ_ASSERT(parser.isValidSimpleAssignmentTarget(target, PermitAssignmentToFunctionCalls));

    if (!Fold(cx, &target, parser))
        return false;

    MOZ_ASSERT(parser.isValidSimpleAssignmentTarget(target, PermitAssignmentToFunctionCalls));

    return true;
}

static bool
FoldAndOr(JSContext* cx, ParseNode** nodePtr, GeneralParser<FullParseHandler, char16_t>& parser)
{
    ParseNode* node = *nodePtr;

    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_AND) || node->isKind(ParseNodeKind::PNK_OR));
    MOZ_ASSERT(node->isArity(PN_LIST));

    bool isOrNode = node->isKind(ParseNodeKind::PNK_OR);
    ParseNode** elem = &node->pn_head;
    do {
        if (!Fold(cx, elem, parser))
            return false;

        Truthiness t = Boolish(*elem);

        // If we don't know the constant-folded node's truthiness, we can't
        // reduce this node with its surroundings.  Continue folding any
        // remaining nodes.
        if (t == Unknown) {
            elem = &(*elem)->pn_next;
            continue;
        }

        // If the constant-folded node's truthiness will terminate the
        // condition -- `a || true || expr` or |b && false && expr| -- then
        // trailing nodes will never be evaluated.  Truncate the list after
        // the known-truthiness node, as it's the overall result.
        if ((t == Truthy) == isOrNode) {
            ParseNode* afterNext;
            for (ParseNode* next = (*elem)->pn_next; next; next = afterNext) {
                afterNext = next->pn_next;
                parser.handler.freeTree(next);
                --node->pn_count;
            }

            // Terminate the original and/or list at the known-truthiness
            // node.
            (*elem)->pn_next = nullptr;
            elem = &(*elem)->pn_next;
            break;
        }

        MOZ_ASSERT((t == Truthy) == !isOrNode);

        // We've encountered a vacuous node that'll never short- circuit
        // evaluation.
        if ((*elem)->pn_next) {
            // This node is never the overall result when there are
            // subsequent nodes.  Remove it.
            ParseNode* elt = *elem;
            *elem = elt->pn_next;
            parser.handler.freeTree(elt);
            --node->pn_count;
        } else {
            // Otherwise this node is the result of the overall expression,
            // so leave it alone.  And we're done.
            elem = &(*elem)->pn_next;
            break;
        }
    } while (*elem);

    // If the last node in the list was replaced, we need to update the
    // tail pointer in the original and/or node.
    node->pn_tail = elem;

    node->checkListConsistency();

    // If we removed nodes, we may have to replace a one-element list with
    // its element.
    if (node->pn_count == 1) {
        ParseNode* first = node->pn_head;
        ReplaceNode(nodePtr, first);

        node->setKind(ParseNodeKind::PNK_NULL);
        node->setArity(PN_NULLARY);
        parser.freeTree(node);
    }

    return true;
}

static bool
FoldConditional(JSContext* cx, ParseNode** nodePtr,
                GeneralParser<FullParseHandler, char16_t>& parser)
{
    ParseNode** nextNode = nodePtr;

    do {
        // |nextNode| on entry points to the C?T:F expression to be folded.
        // Reset it to exit the loop in the common case where F isn't another
        // ?: expression.
        nodePtr = nextNode;
        nextNode = nullptr;

        ParseNode* node = *nodePtr;
        MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_CONDITIONAL));
        MOZ_ASSERT(node->isArity(PN_TERNARY));

        ParseNode*& expr = node->pn_kid1;
        if (!FoldCondition(cx, &expr, parser))
            return false;

        ParseNode*& ifTruthy = node->pn_kid2;
        if (!Fold(cx, &ifTruthy, parser))
            return false;

        ParseNode*& ifFalsy = node->pn_kid3;

        // If our C?T:F node has F as another ?: node, *iteratively* constant-
        // fold F *after* folding C and T (and possibly eliminating C and one
        // of T/F entirely); otherwise fold F normally.  Making |nextNode| non-
        // null causes this loop to run again to fold F.
        //
        // Conceivably we could instead/also iteratively constant-fold T, if T
        // were more complex than F.  Such an optimization is unimplemented.
        if (ifFalsy->isKind(ParseNodeKind::PNK_CONDITIONAL)) {
            nextNode = &ifFalsy;
        } else {
            if (!Fold(cx, &ifFalsy, parser))
                return false;
        }

        // Try to constant-fold based on the condition expression.
        Truthiness t = Boolish(expr);
        if (t == Unknown)
            continue;

        // Otherwise reduce 'C ? T : F' to T or F as directed by C.
        ParseNode* replacement;
        ParseNode* discarded;
        if (t == Truthy) {
            replacement = ifTruthy;
            discarded = ifFalsy;
        } else {
            replacement = ifFalsy;
            discarded = ifTruthy;
        }

        // Otherwise perform a replacement.  This invalidates |nextNode|, so
        // reset it (if the replacement requires folding) or clear it (if
        // |ifFalsy| is dead code) as needed.
        if (nextNode)
            nextNode = (*nextNode == replacement) ? nodePtr : nullptr;
        ReplaceNode(nodePtr, replacement);

        parser.freeTree(discarded);
    } while (nextNode);

    return true;
}

static bool
FoldIf(JSContext* cx, ParseNode** nodePtr, GeneralParser<FullParseHandler, char16_t>& parser)
{
    ParseNode** nextNode = nodePtr;

    do {
        // |nextNode| on entry points to the initial |if| to be folded.  Reset
        // it to exit the loop when the |else| arm isn't another |if|.
        nodePtr = nextNode;
        nextNode = nullptr;

        ParseNode* node = *nodePtr;
        MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_IF));
        MOZ_ASSERT(node->isArity(PN_TERNARY));

        ParseNode*& expr = node->pn_kid1;
        if (!FoldCondition(cx, &expr, parser))
            return false;

        ParseNode*& consequent = node->pn_kid2;
        if (!Fold(cx, &consequent, parser))
            return false;

        ParseNode*& alternative = node->pn_kid3;
        if (alternative) {
            // If in |if (C) T; else F;| we have |F| as another |if|,
            // *iteratively* constant-fold |F| *after* folding |C| and |T| (and
            // possibly completely replacing the whole thing with |T| or |F|);
            // otherwise fold F normally.  Making |nextNode| non-null causes
            // this loop to run again to fold F.
            if (alternative->isKind(ParseNodeKind::PNK_IF)) {
                nextNode = &alternative;
            } else {
                if (!Fold(cx, &alternative, parser))
                    return false;
            }
        }

        // Eliminate the consequent or alternative if the condition has
        // constant truthiness.
        Truthiness t = Boolish(expr);
        if (t == Unknown)
            continue;

        // Careful!  Either of these can be null: |replacement| in |if (0) T;|,
        // and |discarded| in |if (true) T;|.
        ParseNode* replacement;
        ParseNode* discarded;
        if (t == Truthy) {
            replacement = consequent;
            discarded = alternative;
        } else {
            replacement = alternative;
            discarded = consequent;
        }

        bool performReplacement = true;
        if (discarded) {
            // A declaration that hoists outside the discarded arm prevents the
            // |if| from being folded away.
            bool containsHoistedDecls;
            if (!ContainsHoistedDeclaration(cx, discarded, &containsHoistedDecls))
                return false;

            performReplacement = !containsHoistedDecls;
        }

        if (!performReplacement)
            continue;

        if (!replacement) {
            // If there's no replacement node, we have a constantly-false |if|
            // with no |else|.  Replace the entire thing with an empty
            // statement list.
            parser.prepareNodeForMutation(node);
            node->setKind(ParseNodeKind::PNK_STATEMENTLIST);
            node->setArity(PN_LIST);
            node->makeEmpty();
        } else {
            // Replacement invalidates |nextNode|, so reset it (if the
            // replacement requires folding) or clear it (if |alternative|
            // is dead code) as needed.
            if (nextNode)
                nextNode = (*nextNode == replacement) ? nodePtr : nullptr;
            ReplaceNode(nodePtr, replacement);

            // Morph the original node into a discardable node, then
            // aggressively free it and the discarded arm (if any) to suss out
            // any bugs in the preceding logic.
            node->setKind(ParseNodeKind::PNK_STATEMENTLIST);
            node->setArity(PN_LIST);
            node->makeEmpty();
            if (discarded)
                node->append(discarded);
            parser.freeTree(node);
        }
    } while (nextNode);

    return true;
}

static bool
FoldFunction(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_FUNCTION));
    MOZ_ASSERT(node->isArity(PN_CODE));

    // Don't constant-fold inside "use asm" code, as this could create a parse
    // tree that doesn't type-check as asm.js.
    if (node->pn_funbox->useAsmOrInsideUseAsm())
        return true;

    // Note: pn_body is null for lazily-parsed functions.
    if (ParseNode*& functionBody = node->pn_body) {
        if (!Fold(cx, &functionBody, parser))
            return false;
    }

    return true;
}

static double
ComputeBinary(ParseNodeKind kind, double left, double right)
{
    if (kind == ParseNodeKind::PNK_ADD)
        return left + right;

    if (kind == ParseNodeKind::PNK_SUB)
        return left - right;

    if (kind == ParseNodeKind::PNK_STAR)
        return left * right;

    if (kind == ParseNodeKind::PNK_MOD)
        return right == 0 ? GenericNaN() : js_fmod(left, right);

    if (kind == ParseNodeKind::PNK_URSH)
        return ToUint32(left) >> (ToUint32(right) & 31);

    if (kind == ParseNodeKind::PNK_DIV) {
        if (right == 0) {
#if defined(XP_WIN)
            /* XXX MSVC miscompiles such that (NaN == 0) */
            if (IsNaN(right))
                return GenericNaN();
#endif
            if (left == 0 || IsNaN(left))
                return GenericNaN();
            if (IsNegative(left) != IsNegative(right))
                return NegativeInfinity<double>();
            return PositiveInfinity<double>();
        }

        return left / right;
    }

    MOZ_ASSERT(kind == ParseNodeKind::PNK_LSH || kind == ParseNodeKind::PNK_RSH);

    int32_t i = ToInt32(left);
    uint32_t j = ToUint32(right) & 31;
    return int32_t((kind == ParseNodeKind::PNK_LSH) ? uint32_t(i) << j : i >> j);
}

static bool
FoldModule(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_MODULE));
    MOZ_ASSERT(node->isArity(PN_CODE));

    ParseNode*& moduleBody = node->pn_body;
    MOZ_ASSERT(moduleBody);
    return Fold(cx, &moduleBody, parser);
}

static bool
FoldBinaryArithmetic(JSContext* cx, ParseNode* node,
                     GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_SUB) ||
               node->isKind(ParseNodeKind::PNK_STAR) ||
               node->isKind(ParseNodeKind::PNK_LSH) ||
               node->isKind(ParseNodeKind::PNK_RSH) ||
               node->isKind(ParseNodeKind::PNK_URSH) ||
               node->isKind(ParseNodeKind::PNK_DIV) ||
               node->isKind(ParseNodeKind::PNK_MOD));
    MOZ_ASSERT(node->isArity(PN_LIST));
    MOZ_ASSERT(node->pn_count >= 2);

    // Fold each operand, ideally into a number.
    ParseNode** listp = &node->pn_head;
    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser))
            return false;

        if (!FoldType(cx, *listp, ParseNodeKind::PNK_NUMBER))
            return false;
    }

    // Repoint the list's tail pointer.
    node->pn_tail = listp;

    // Now fold all leading numeric terms together into a single number.
    // (Trailing terms for the non-shift operations can't be folded together
    // due to floating point imprecision.  For example, if |x === -2**53|,
    // |x - 1 - 1 === -2**53| but |x - 2 === -2**53 - 2|.  Shifts could be
    // folded, but it doesn't seem worth the effort.)
    ParseNode* elem = node->pn_head;
    ParseNode* next = elem->pn_next;
    if (elem->isKind(ParseNodeKind::PNK_NUMBER)) {
        ParseNodeKind kind = node->getKind();
        while (true) {
            if (!next || !next->isKind(ParseNodeKind::PNK_NUMBER))
                break;

            double d = ComputeBinary(kind, elem->pn_dval, next->pn_dval);

            ParseNode* afterNext = next->pn_next;
            parser.freeTree(next);
            next = afterNext;
            elem->pn_next = next;

            elem->setKind(ParseNodeKind::PNK_NUMBER);
            elem->setOp(JSOP_DOUBLE);
            elem->setArity(PN_NULLARY);
            elem->pn_dval = d;

            node->pn_count--;
        }

        if (node->pn_count == 1) {
            MOZ_ASSERT(node->pn_head == elem);
            MOZ_ASSERT(elem->isKind(ParseNodeKind::PNK_NUMBER));

            double d = elem->pn_dval;
            node->setKind(ParseNodeKind::PNK_NUMBER);
            node->setArity(PN_NULLARY);
            node->setOp(JSOP_DOUBLE);
            node->pn_dval = d;

            parser.freeTree(elem);
        }
    }

    return true;
}

static bool
FoldExponentiation(JSContext* cx, ParseNode* node,
                   GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_POW));
    MOZ_ASSERT(node->isArity(PN_LIST));
    MOZ_ASSERT(node->pn_count >= 2);

    // Fold each operand, ideally into a number.
    ParseNode** listp = &node->pn_head;
    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser))
            return false;

        if (!FoldType(cx, *listp, ParseNodeKind::PNK_NUMBER))
            return false;
    }

    // Repoint the list's tail pointer.
    node->pn_tail = listp;

    // Unlike all other binary arithmetic operators, ** is right-associative:
    // 2**3**5 is 2**(3**5), not (2**3)**5.  As list nodes singly-link their
    // children, full constant-folding requires either linear space or dodgy
    // in-place linked list reversal.  So we only fold one exponentiation: it's
    // easy and addresses common cases like |2**32|.
    if (node->pn_count > 2)
        return true;

    ParseNode* base = node->pn_head;
    ParseNode* exponent = base->pn_next;
    if (!base->isKind(ParseNodeKind::PNK_NUMBER) || !exponent->isKind(ParseNodeKind::PNK_NUMBER))
        return true;

    double d1 = base->pn_dval, d2 = exponent->pn_dval;

    parser.prepareNodeForMutation(node);
    node->setKind(ParseNodeKind::PNK_NUMBER);
    node->setArity(PN_NULLARY);
    node->setOp(JSOP_DOUBLE);
    node->pn_dval = ecmaPow(d1, d2);
    return true;
}

static bool
FoldList(JSContext* cx, ParseNode* list, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(list->isArity(PN_LIST));

    ParseNode** elem = &list->pn_head;
    for (; *elem; elem = &(*elem)->pn_next) {
        if (!Fold(cx, elem, parser))
            return false;
    }

    // Repoint the list's tail pointer if the final element was replaced.
    list->pn_tail = elem;

    list->checkListConsistency();

    return true;
}

static bool
FoldReturn(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_RETURN));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    if (ParseNode*& expr = node->pn_kid) {
        if (!Fold(cx, &expr, parser))
            return false;
    }

    return true;
}

static bool
FoldTry(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_TRY));
    MOZ_ASSERT(node->isArity(PN_TERNARY));

    ParseNode*& statements = node->pn_kid1;
    if (!Fold(cx, &statements, parser))
        return false;

    if (ParseNode*& catchScope = node->pn_kid2) {
        if (!Fold(cx, &catchScope, parser))
            return false;
    }

    if (ParseNode*& finally = node->pn_kid3) {
        if (!Fold(cx, &finally, parser))
            return false;
    }

    return true;
}

static bool
FoldCatch(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_CATCH));
    MOZ_ASSERT(node->isArity(PN_BINARY));

    if (ParseNode*& declPattern = node->pn_left) {
        if (!Fold(cx, &declPattern, parser))
            return false;
    }

    if (ParseNode*& statements = node->pn_right) {
        if (!Fold(cx, &statements, parser))
            return false;
    }

    return true;
}

static bool
FoldClass(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_CLASS));
    MOZ_ASSERT(node->isArity(PN_TERNARY));

    if (ParseNode*& classNames = node->pn_kid1) {
        if (!Fold(cx, &classNames, parser))
            return false;
    }

    if (ParseNode*& heritage = node->pn_kid2) {
        if (!Fold(cx, &heritage, parser))
            return false;
    }

    ParseNode*& body = node->pn_kid3;
    return Fold(cx, &body, parser);
}

static bool
FoldElement(JSContext* cx, ParseNode** nodePtr, GeneralParser<FullParseHandler, char16_t>& parser)
{
    ParseNode* node = *nodePtr;

    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_ELEM));
    MOZ_ASSERT(node->isArity(PN_BINARY));

    ParseNode*& expr = node->pn_left;
    if (!Fold(cx, &expr, parser))
        return false;

    ParseNode*& key = node->pn_right;
    if (!Fold(cx, &key, parser))
        return false;

    PropertyName* name = nullptr;
    if (key->isKind(ParseNodeKind::PNK_STRING)) {
        JSAtom* atom = key->pn_atom;
        uint32_t index;

        if (atom->isIndex(&index)) {
            // Optimization 1: We have something like expr["100"]. This is
            // equivalent to expr[100] which is faster.
            key->setKind(ParseNodeKind::PNK_NUMBER);
            key->setOp(JSOP_DOUBLE);
            key->pn_dval = index;
        } else {
            name = atom->asPropertyName();
        }
    } else if (key->isKind(ParseNodeKind::PNK_NUMBER)) {
        double number = key->pn_dval;
        if (number != ToUint32(number)) {
            // Optimization 2: We have something like expr[3.14]. The number
            // isn't an array index, so it converts to a string ("3.14"),
            // enabling optimization 3 below.
            JSAtom* atom = ToAtom<NoGC>(cx, DoubleValue(number));
            if (!atom)
                return false;
            name = atom->asPropertyName();
        }
    }

    // If we don't have a name, we can't optimize to getprop.
    if (!name)
        return true;

    // Optimization 3: We have expr["foo"] where foo is not an index.  Convert
    // to a property access (like expr.foo) that optimizes better downstream.
    ParseNode* dottedAccess = parser.handler.newPropertyAccess(expr, name, node->pn_pos.end);
    if (!dottedAccess)
        return false;
    dottedAccess->setInParens(node->isInParens());
    ReplaceNode(nodePtr, dottedAccess);

    // If we've replaced |expr["prop"]| with |expr.prop|, we can now free the
    // |"prop"| and |expr["prop"]| nodes -- but not the |expr| node that we're
    // now using as a sub-node of |dottedAccess|.  Munge |expr["prop"]| into a
    // node with |"prop"| as its only child, that'll pass AST sanity-checking
    // assertions during freeing, then free it.
    node->setKind(ParseNodeKind::PNK_TYPEOFEXPR);
    node->setArity(PN_UNARY);
    node->pn_kid = key;
    parser.freeTree(node);

    return true;
}

static bool
FoldAdd(JSContext* cx, ParseNode** nodePtr, GeneralParser<FullParseHandler, char16_t>& parser)
{
    ParseNode* node = *nodePtr;

    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_ADD));
    MOZ_ASSERT(node->isArity(PN_LIST));
    MOZ_ASSERT(node->pn_count >= 2);

    // Generically fold all operands first.
    if (!FoldList(cx, node, parser))
        return false;

    // Fold leading numeric operands together:
    //
    //   (1 + 2 + x)  becomes  (3 + x)
    //
    // Don't go past the leading operands: additions after a string are
    // string concatenations, not additions: ("1" + 2 + 3 === "123").
    ParseNode* current = node->pn_head;
    ParseNode* next = current->pn_next;
    if (current->isKind(ParseNodeKind::PNK_NUMBER)) {
        do {
            if (!next->isKind(ParseNodeKind::PNK_NUMBER))
                break;

            current->pn_dval += next->pn_dval;
            current->pn_next = next->pn_next;
            parser.freeTree(next);
            next = current->pn_next;

            MOZ_ASSERT(node->pn_count > 1);
            node->pn_count--;
        } while (next);
    }

    // If any operands remain, attempt string concatenation folding.
    do {
        // If no operands remain, we're done.
        if (!next)
            break;

        // (number + string) is string concatenation *only* at the start of
        // the list: (x + 1 + "2" !== x + "12") when x is a number.
        if (current->isKind(ParseNodeKind::PNK_NUMBER) && next->isKind(ParseNodeKind::PNK_STRING)) {
            if (!FoldType(cx, current, ParseNodeKind::PNK_STRING))
                return false;
            next = current->pn_next;
        }

        // The first string forces all subsequent additions to be
        // string concatenations.
        do {
            if (current->isKind(ParseNodeKind::PNK_STRING))
                break;

            current = next;
            next = next->pn_next;
        } while (next);

        // If there's nothing left to fold, we're done.
        if (!next)
            break;

        RootedString combination(cx);
        RootedString tmp(cx);
        do {
            // Create a rope of the current string and all succeeding
            // constants that we can convert to strings, then atomize it
            // and replace them all with that fresh string.
            MOZ_ASSERT(current->isKind(ParseNodeKind::PNK_STRING));

            combination = current->pn_atom;

            do {
                // Try folding the next operand to a string.
                if (!FoldType(cx, next, ParseNodeKind::PNK_STRING))
                    return false;

                // Stop glomming once folding doesn't produce a string.
                if (!next->isKind(ParseNodeKind::PNK_STRING))
                    break;

                // Add this string to the combination and remove the node.
                tmp = next->pn_atom;
                combination = ConcatStrings<CanGC>(cx, combination, tmp);
                if (!combination)
                    return false;

                current->pn_next = next->pn_next;
                parser.freeTree(next);
                next = current->pn_next;

                MOZ_ASSERT(node->pn_count > 1);
                node->pn_count--;
            } while (next);

            // Replace |current|'s string with the entire combination.
            MOZ_ASSERT(current->isKind(ParseNodeKind::PNK_STRING));
            combination = AtomizeString(cx, combination);
            if (!combination)
                return false;
            current->pn_atom = &combination->asAtom();


            // If we're out of nodes, we're done.
            if (!next)
                break;

            current = next;
            next = current->pn_next;

            // If we're out of nodes *after* the non-foldable-to-string
            // node, we're done.
            if (!next)
                break;

            // Otherwise find the next node foldable to a string, and loop.
            do {
                current = next;
                next = current->pn_next;

                if (!FoldType(cx, current, ParseNodeKind::PNK_STRING))
                    return false;
                next = current->pn_next;
            } while (!current->isKind(ParseNodeKind::PNK_STRING) && next);
        } while (next);
    } while (false);

    MOZ_ASSERT(!next, "must have considered all nodes here");
    MOZ_ASSERT(!current->pn_next, "current node must be the last node");

    node->pn_tail = &current->pn_next;
    node->checkListConsistency();

    if (node->pn_count == 1) {
        // We reduced the list to a constant.  Replace the ParseNodeKind::PNK_ADD node
        // with that constant.
        ReplaceNode(nodePtr, current);

        // Free the old node to aggressively verify nothing uses it.
        node->setKind(ParseNodeKind::PNK_TRUE);
        node->setArity(PN_NULLARY);
        node->setOp(JSOP_TRUE);
        parser.freeTree(node);
    }

    return true;
}

static bool
FoldCall(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_CALL) ||
               node->isKind(ParseNodeKind::PNK_SUPERCALL) ||
               node->isKind(ParseNodeKind::PNK_TAGGED_TEMPLATE));
    MOZ_ASSERT(node->isArity(PN_LIST));

    // Don't fold a parenthesized callable component in an invocation, as this
    // might cause a different |this| value to be used, changing semantics:
    //
    //   var prop = "global";
    //   var obj = { prop: "obj", f: function() { return this.prop; } };
    //   assertEq((true ? obj.f : null)(), "global");
    //   assertEq(obj.f(), "obj");
    //   assertEq((true ? obj.f : null)``, "global");
    //   assertEq(obj.f``, "obj");
    //
    // See bug 537673 and bug 1182373.
    ParseNode** listp = &node->pn_head;
    if ((*listp)->isInParens())
        listp = &(*listp)->pn_next;

    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser))
            return false;
    }

    // If the last node in the list was replaced, pn_tail points into the wrong node.
    node->pn_tail = listp;

    node->checkListConsistency();
    return true;
}

static bool
FoldForInOrOf(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_FORIN) ||
               node->isKind(ParseNodeKind::PNK_FOROF));
    MOZ_ASSERT(node->isArity(PN_TERNARY));
    MOZ_ASSERT(!node->pn_kid2);

    return Fold(cx, &node->pn_kid1, parser) &&
           Fold(cx, &node->pn_kid3, parser);
}

static bool
FoldForHead(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_FORHEAD));
    MOZ_ASSERT(node->isArity(PN_TERNARY));

    if (ParseNode*& init = node->pn_kid1) {
        if (!Fold(cx, &init, parser))
            return false;
    }

    if (ParseNode*& test = node->pn_kid2) {
        if (!FoldCondition(cx, &test, parser))
            return false;

        if (test->isKind(ParseNodeKind::PNK_TRUE)) {
            parser.freeTree(test);
            test = nullptr;
        }
    }

    if (ParseNode*& update = node->pn_kid3) {
        if (!Fold(cx, &update, parser))
            return false;
    }

    return true;
}

static bool
FoldDottedProperty(JSContext* cx, ParseNode* node,
                   GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_DOT));
    MOZ_ASSERT(node->isArity(PN_NAME));

    // Iterate through a long chain of dotted property accesses to find the
    // most-nested non-dotted property node, then fold that.
    ParseNode** nested = &node->pn_expr;
    while ((*nested)->isKind(ParseNodeKind::PNK_DOT)) {
        MOZ_ASSERT((*nested)->isArity(PN_NAME));
        nested = &(*nested)->pn_expr;
    }

    return Fold(cx, nested, parser);
}

static bool
FoldName(JSContext* cx, ParseNode* node, GeneralParser<FullParseHandler, char16_t>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::PNK_NAME));
    MOZ_ASSERT(node->isArity(PN_NAME));

    if (!node->pn_expr)
        return true;

    return Fold(cx, &node->pn_expr, parser);
}

bool
Fold(JSContext* cx, ParseNode** pnp, GeneralParser<FullParseHandler, char16_t>& parser)
{
    if (!CheckRecursionLimit(cx))
        return false;

    ParseNode* pn = *pnp;

    switch (pn->getKind()) {
      case ParseNodeKind::PNK_NOP:
      case ParseNodeKind::PNK_REGEXP:
      case ParseNodeKind::PNK_STRING:
      case ParseNodeKind::PNK_TRUE:
      case ParseNodeKind::PNK_FALSE:
      case ParseNodeKind::PNK_NULL:
      case ParseNodeKind::PNK_RAW_UNDEFINED:
      case ParseNodeKind::PNK_ELISION:
      case ParseNodeKind::PNK_NUMBER:
      case ParseNodeKind::PNK_DEBUGGER:
      case ParseNodeKind::PNK_BREAK:
      case ParseNodeKind::PNK_CONTINUE:
      case ParseNodeKind::PNK_TEMPLATE_STRING:
      case ParseNodeKind::PNK_GENERATOR:
      case ParseNodeKind::PNK_EXPORT_BATCH_SPEC:
      case ParseNodeKind::PNK_OBJECT_PROPERTY_NAME:
      case ParseNodeKind::PNK_POSHOLDER:
        MOZ_ASSERT(pn->isArity(PN_NULLARY));
        return true;

      case ParseNodeKind::PNK_SUPERBASE:
      case ParseNodeKind::PNK_TYPEOFNAME:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        MOZ_ASSERT(pn->pn_kid->isKind(ParseNodeKind::PNK_NAME));
        MOZ_ASSERT(!pn->pn_kid->expr());
        return true;

      case ParseNodeKind::PNK_TYPEOFEXPR:
        return FoldTypeOfExpr(cx, pn, parser);

      case ParseNodeKind::PNK_DELETENAME: {
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        MOZ_ASSERT(pn->pn_kid->isKind(ParseNodeKind::PNK_NAME));
        return true;
      }

      case ParseNodeKind::PNK_DELETEEXPR:
        return FoldDeleteExpr(cx, pn, parser);

      case ParseNodeKind::PNK_DELETEELEM:
        return FoldDeleteElement(cx, pn, parser);

      case ParseNodeKind::PNK_DELETEPROP:
        return FoldDeleteProperty(cx, pn, parser);

      case ParseNodeKind::PNK_CONDITIONAL:
        return FoldConditional(cx, pnp, parser);

      case ParseNodeKind::PNK_IF:
        return FoldIf(cx, pnp, parser);

      case ParseNodeKind::PNK_NOT:
        return FoldNot(cx, pn, parser);

      case ParseNodeKind::PNK_BITNOT:
      case ParseNodeKind::PNK_POS:
      case ParseNodeKind::PNK_NEG:
        return FoldUnaryArithmetic(cx, pn, parser);

      case ParseNodeKind::PNK_PREINCREMENT:
      case ParseNodeKind::PNK_POSTINCREMENT:
      case ParseNodeKind::PNK_PREDECREMENT:
      case ParseNodeKind::PNK_POSTDECREMENT:
        return FoldIncrementDecrement(cx, pn, parser);

      case ParseNodeKind::PNK_THROW:
      case ParseNodeKind::PNK_MUTATEPROTO:
      case ParseNodeKind::PNK_COMPUTED_NAME:
      case ParseNodeKind::PNK_SPREAD:
      case ParseNodeKind::PNK_EXPORT:
      case ParseNodeKind::PNK_VOID:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        return Fold(cx, &pn->pn_kid, parser);

      case ParseNodeKind::PNK_EXPORT_DEFAULT:
        MOZ_ASSERT(pn->isArity(PN_BINARY));
        return Fold(cx, &pn->pn_left, parser);

      case ParseNodeKind::PNK_SEMI:
      case ParseNodeKind::PNK_THIS:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        if (ParseNode*& expr = pn->pn_kid)
            return Fold(cx, &expr, parser);
        return true;

      case ParseNodeKind::PNK_PIPELINE:
        return true;

      case ParseNodeKind::PNK_AND:
      case ParseNodeKind::PNK_OR:
        return FoldAndOr(cx, pnp, parser);

      case ParseNodeKind::PNK_FUNCTION:
        return FoldFunction(cx, pn, parser);

      case ParseNodeKind::PNK_MODULE:
        return FoldModule(cx, pn, parser);

      case ParseNodeKind::PNK_SUB:
      case ParseNodeKind::PNK_STAR:
      case ParseNodeKind::PNK_LSH:
      case ParseNodeKind::PNK_RSH:
      case ParseNodeKind::PNK_URSH:
      case ParseNodeKind::PNK_DIV:
      case ParseNodeKind::PNK_MOD:
        return FoldBinaryArithmetic(cx, pn, parser);

      case ParseNodeKind::PNK_POW:
        return FoldExponentiation(cx, pn, parser);

      // Various list nodes not requiring care to minimally fold.  Some of
      // these could be further folded/optimized, but we don't make the effort.
      case ParseNodeKind::PNK_BITOR:
      case ParseNodeKind::PNK_BITXOR:
      case ParseNodeKind::PNK_BITAND:
      case ParseNodeKind::PNK_STRICTEQ:
      case ParseNodeKind::PNK_EQ:
      case ParseNodeKind::PNK_STRICTNE:
      case ParseNodeKind::PNK_NE:
      case ParseNodeKind::PNK_LT:
      case ParseNodeKind::PNK_LE:
      case ParseNodeKind::PNK_GT:
      case ParseNodeKind::PNK_GE:
      case ParseNodeKind::PNK_INSTANCEOF:
      case ParseNodeKind::PNK_IN:
      case ParseNodeKind::PNK_COMMA:
      case ParseNodeKind::PNK_NEW:
      case ParseNodeKind::PNK_ARRAY:
      case ParseNodeKind::PNK_OBJECT:
      case ParseNodeKind::PNK_STATEMENTLIST:
      case ParseNodeKind::PNK_CLASSMETHODLIST:
      case ParseNodeKind::PNK_TEMPLATE_STRING_LIST:
      case ParseNodeKind::PNK_VAR:
      case ParseNodeKind::PNK_CONST:
      case ParseNodeKind::PNK_LET:
      case ParseNodeKind::PNK_PARAMSBODY:
      case ParseNodeKind::PNK_CALLSITEOBJ:
      case ParseNodeKind::PNK_EXPORT_SPEC_LIST:
      case ParseNodeKind::PNK_IMPORT_SPEC_LIST:
        return FoldList(cx, pn, parser);

      case ParseNodeKind::PNK_INITIALYIELD:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        MOZ_ASSERT(pn->pn_kid->isKind(ParseNodeKind::PNK_ASSIGN) &&
                   pn->pn_kid->pn_left->isKind(ParseNodeKind::PNK_NAME) &&
                   pn->pn_kid->pn_right->isKind(ParseNodeKind::PNK_GENERATOR));
        return true;

      case ParseNodeKind::PNK_YIELD_STAR:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        return Fold(cx, &pn->pn_kid, parser);

      case ParseNodeKind::PNK_YIELD:
      case ParseNodeKind::PNK_AWAIT:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        if (!pn->pn_kid)
            return true;
        return Fold(cx, &pn->pn_kid, parser);

      case ParseNodeKind::PNK_RETURN:
        return FoldReturn(cx, pn, parser);

      case ParseNodeKind::PNK_TRY:
        return FoldTry(cx, pn, parser);

      case ParseNodeKind::PNK_CATCH:
        return FoldCatch(cx, pn, parser);

      case ParseNodeKind::PNK_CLASS:
        return FoldClass(cx, pn, parser);

      case ParseNodeKind::PNK_ELEM:
        return FoldElement(cx, pnp, parser);

      case ParseNodeKind::PNK_ADD:
        return FoldAdd(cx, pnp, parser);

      case ParseNodeKind::PNK_CALL:
      case ParseNodeKind::PNK_SUPERCALL:
      case ParseNodeKind::PNK_TAGGED_TEMPLATE:
        return FoldCall(cx, pn, parser);

      case ParseNodeKind::PNK_SWITCH:
      case ParseNodeKind::PNK_COLON:
      case ParseNodeKind::PNK_ASSIGN:
      case ParseNodeKind::PNK_ADDASSIGN:
      case ParseNodeKind::PNK_SUBASSIGN:
      case ParseNodeKind::PNK_BITORASSIGN:
      case ParseNodeKind::PNK_BITANDASSIGN:
      case ParseNodeKind::PNK_BITXORASSIGN:
      case ParseNodeKind::PNK_LSHASSIGN:
      case ParseNodeKind::PNK_RSHASSIGN:
      case ParseNodeKind::PNK_URSHASSIGN:
      case ParseNodeKind::PNK_DIVASSIGN:
      case ParseNodeKind::PNK_MODASSIGN:
      case ParseNodeKind::PNK_MULASSIGN:
      case ParseNodeKind::PNK_POWASSIGN:
      case ParseNodeKind::PNK_IMPORT:
      case ParseNodeKind::PNK_EXPORT_FROM:
      case ParseNodeKind::PNK_SHORTHAND:
      case ParseNodeKind::PNK_FOR:
      case ParseNodeKind::PNK_CLASSMETHOD:
      case ParseNodeKind::PNK_IMPORT_SPEC:
      case ParseNodeKind::PNK_EXPORT_SPEC:
      case ParseNodeKind::PNK_SETTHIS:
        MOZ_ASSERT(pn->isArity(PN_BINARY));
        return Fold(cx, &pn->pn_left, parser) &&
               Fold(cx, &pn->pn_right, parser);

      case ParseNodeKind::PNK_NEWTARGET:
        MOZ_ASSERT(pn->isArity(PN_BINARY));
        MOZ_ASSERT(pn->pn_left->isKind(ParseNodeKind::PNK_POSHOLDER));
        MOZ_ASSERT(pn->pn_right->isKind(ParseNodeKind::PNK_POSHOLDER));
        return true;

      case ParseNodeKind::PNK_CLASSNAMES:
        MOZ_ASSERT(pn->isArity(PN_BINARY));
        if (ParseNode*& outerBinding = pn->pn_left) {
            if (!Fold(cx, &outerBinding, parser))
                return false;
        }
        return Fold(cx, &pn->pn_right, parser);

      case ParseNodeKind::PNK_DOWHILE:
        MOZ_ASSERT(pn->isArity(PN_BINARY));
        return Fold(cx, &pn->pn_left, parser) &&
               FoldCondition(cx, &pn->pn_right, parser);

      case ParseNodeKind::PNK_WHILE:
        MOZ_ASSERT(pn->isArity(PN_BINARY));
        return FoldCondition(cx, &pn->pn_left, parser) &&
               Fold(cx, &pn->pn_right, parser);

      case ParseNodeKind::PNK_CASE: {
        MOZ_ASSERT(pn->isArity(PN_BINARY));

        // pn_left is null for DefaultClauses.
        if (pn->pn_left) {
            if (!Fold(cx, &pn->pn_left, parser))
                return false;
        }
        return Fold(cx, &pn->pn_right, parser);
      }

      case ParseNodeKind::PNK_WITH:
        MOZ_ASSERT(pn->isArity(PN_BINARY));
        return Fold(cx, &pn->pn_left, parser) &&
               Fold(cx, &pn->pn_right, parser);

      case ParseNodeKind::PNK_FORIN:
      case ParseNodeKind::PNK_FOROF:
        return FoldForInOrOf(cx, pn, parser);

      case ParseNodeKind::PNK_FORHEAD:
        return FoldForHead(cx, pn, parser);

      case ParseNodeKind::PNK_LABEL:
        MOZ_ASSERT(pn->isArity(PN_NAME));
        return Fold(cx, &pn->pn_expr, parser);

      case ParseNodeKind::PNK_DOT:
        return FoldDottedProperty(cx, pn, parser);

      case ParseNodeKind::PNK_LEXICALSCOPE:
        MOZ_ASSERT(pn->isArity(PN_SCOPE));
        if (!pn->scopeBody())
            return true;
        return Fold(cx, &pn->pn_u.scope.body, parser);

      case ParseNodeKind::PNK_NAME:
        return FoldName(cx, pn, parser);

      case ParseNodeKind::PNK_LIMIT: // invalid sentinel value
        MOZ_CRASH("invalid node kind");
    }

    MOZ_CRASH("shouldn't reach here");
    return false;
}

template<typename CharT>
bool
frontend::FoldConstants(JSContext* cx, ParseNode** pnp,
                        GeneralParser<FullParseHandler, CharT>* parser)
{
    // Don't constant-fold inside "use asm" code, as this could create a parse
    // tree that doesn't type-check as asm.js.
    if (parser->pc->useAsmOrInsideUseAsm())
        return true;

    AutoTraceLog traceLog(TraceLoggerForCurrentThread(cx), TraceLogger_BytecodeFoldConstants);
    return Fold(cx, pnp, *parser);
}

template bool
frontend::FoldConstants(JSContext* cx, ParseNode** pnp,
                        GeneralParser<FullParseHandler, char16_t>* parser);

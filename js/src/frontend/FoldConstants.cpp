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
ContainsHoistedDeclaration(ExclusiveContext* cx, ParseNode* node, bool* result);

static bool
ListContainsHoistedDeclaration(ExclusiveContext* cx, ListNode* list, bool* result)
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
// specific context of deciding that |node|, as one arm of a PNK_IF controlled
// by a constant condition, contains a declaration that forbids |node| being
// completely eliminated as dead.
static bool
ContainsHoistedDeclaration(ExclusiveContext* cx, ParseNode* node, bool* result)
{
    JS_CHECK_RECURSION(cx, return false);

  restart:

    // With a better-typed AST, we would have distinct parse node classes for
    // expressions and for statements and would characterize expressions with
    // ExpressionKind and statements with StatementKind.  Perhaps someday.  In
    // the meantime we must characterize every ParseNodeKind, even the
    // expression/sub-expression ones that, if we handle all statement kinds
    // correctly, we'll never see.
    switch (node->getKind()) {
      // Base case.
      case PNK_VAR:
        *result = true;
        return true;

      // Non-global lexical declarations are block-scoped (ergo not hoistable).
      // (Global lexical declarations, in addition to being irrelevant here as
      // ContainsHoistedDeclaration is only used on the arms of an |if|
      // statement, are handled by PNK_GLOBALCONST and PNK_VAR.)
      case PNK_LET:
      case PNK_CONST:
        MOZ_ASSERT(node->isArity(PN_LIST));
        *result = false;
        return true;

      // Similarly to the lexical declarations above, classes cannot add hoisted
      // declarations
      case PNK_CLASS:
        MOZ_ASSERT(node->isArity(PN_TERNARY));
        *result = false;
        return true;

      // ContainsHoistedDeclaration is only called on nested nodes, so any
      // instance of this can't be function statements at body level.  In
      // SpiderMonkey, a binding induced by a function statement is added when
      // the function statement is evaluated.  Thus any declaration introduced
      // by a function statement, as observed by this function, isn't a hoisted
      // declaration.
      case PNK_FUNCTION:
        MOZ_ASSERT(node->isArity(PN_CODE));
        *result = false;
        return true;

      // Statements with no sub-components at all.
      case PNK_NOP: // induced by function f() {} function f() {}
      case PNK_DEBUGGER:
        MOZ_ASSERT(node->isArity(PN_NULLARY));
        *result = false;
        return true;

      // Statements containing only an expression have no declarations.
      case PNK_SEMI:
      case PNK_THROW:
        MOZ_ASSERT(node->isArity(PN_UNARY));
        *result = false;
        return true;

      case PNK_RETURN:
      // These two aren't statements in the spec, but we sometimes insert them
      // in statement lists anyway.
      case PNK_YIELD_STAR:
      case PNK_YIELD:
        MOZ_ASSERT(node->isArity(PN_BINARY));
        *result = false;
        return true;

      // Other statements with no sub-statement components.
      case PNK_BREAK:
      case PNK_CONTINUE:
      case PNK_IMPORT:
      case PNK_IMPORT_SPEC_LIST:
      case PNK_IMPORT_SPEC:
      case PNK_EXPORT_FROM:
      case PNK_EXPORT_DEFAULT:
      case PNK_EXPORT_SPEC_LIST:
      case PNK_EXPORT_SPEC:
      case PNK_EXPORT:
      case PNK_EXPORT_BATCH_SPEC:
        *result = false;
        return true;

      // Statements possibly containing hoistable declarations only in the left
      // half, in ParseNode terms -- the loop body in AST terms.
      case PNK_DOWHILE:
        return ContainsHoistedDeclaration(cx, node->pn_left, result);

      // Statements possibly containing hoistable declarations only in the
      // right half, in ParseNode terms -- the loop body or nested statement
      // (usually a block statement), in AST terms.
      case PNK_WHILE:
      case PNK_WITH:
        return ContainsHoistedDeclaration(cx, node->pn_right, result);

      case PNK_LABEL:
        return ContainsHoistedDeclaration(cx, node->pn_expr, result);

      // Statements with more complicated structures.

      // if-statement nodes may have hoisted declarations in their consequent
      // and alternative components.
      case PNK_IF: {
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

      // Legacy array and generator comprehensions use PNK_IF to represent
      // conditions specified in the comprehension tail: for example,
      // [x for (x in obj) if (x)].  The consequent of such PNK_IF nodes is
      // either PNK_YIELD in a PNK_SEMI statement (generator comprehensions) or
      // PNK_ARRAYPUSH (array comprehensions) .  The first case is consistent
      // with normal if-statement structure with consequent/alternative as
      // statements.  The second case is abnormal and requires that we not
      // banish PNK_ARRAYPUSH to the unreachable list, handling it explicitly.
      //
      // We could require that this one weird PNK_ARRAYPUSH case be packaged in
      // a PNK_SEMI, for consistency.  That requires careful bytecode emitter
      // adjustment that seems unwarranted for a deprecated feature.
      case PNK_ARRAYPUSH:
        *result = false;
        return true;

      // try-statements have statements to execute, and one or both of a
      // catch-list and a finally-block.
      case PNK_TRY: {
        MOZ_ASSERT(node->isArity(PN_TERNARY));
        MOZ_ASSERT(node->pn_kid2 || node->pn_kid3,
                   "must have either catch(es) or finally");

        ParseNode* tryBlock = node->pn_kid1;
        if (!ContainsHoistedDeclaration(cx, tryBlock, result))
            return false;
        if (*result)
            return true;

        if (ParseNode* catchList = node->pn_kid2) {
            for (ParseNode* lexicalScope = catchList->pn_head;
                 lexicalScope;
                 lexicalScope = lexicalScope->pn_next)
            {
                MOZ_ASSERT(lexicalScope->isKind(PNK_LEXICALSCOPE));

                ParseNode* catchNode = lexicalScope->pn_expr;
                MOZ_ASSERT(catchNode->isKind(PNK_CATCH));

                ParseNode* catchStatements = catchNode->pn_kid3;
                if (!ContainsHoistedDeclaration(cx, catchStatements, result))
                    return false;
                if (*result)
                    return true;
            }
        }

        if (ParseNode* finallyBlock = node->pn_kid3)
            return ContainsHoistedDeclaration(cx, finallyBlock, result);

        *result = false;
        return true;
      }

      // A switch node's left half is an expression; only its right half (a
      // list of cases/defaults, or a block node) could contain hoisted
      // declarations.
      case PNK_SWITCH:
        MOZ_ASSERT(node->isArity(PN_BINARY));
        return ContainsHoistedDeclaration(cx, node->pn_right, result);

      // A case/default node's right half is its statements.  A default node's
      // left half is null; a case node's left half is its expression.
      case PNK_DEFAULT:
        MOZ_ASSERT(!node->pn_left);
      case PNK_CASE:
        MOZ_ASSERT(node->isArity(PN_BINARY));
        return ContainsHoistedDeclaration(cx, node->pn_right, result);

      case PNK_FOR: {
        MOZ_ASSERT(node->isArity(PN_BINARY));

        ParseNode* loopHead = node->pn_left;
        MOZ_ASSERT(loopHead->isKind(PNK_FORHEAD) ||
                   loopHead->isKind(PNK_FORIN) ||
                   loopHead->isKind(PNK_FOROF));

        if (loopHead->isKind(PNK_FORHEAD)) {
            // for (init?; cond?; update?), with only init possibly containing
            // a hoisted declaration.  (Note: a lexical-declaration |init| is
            // (at present) hoisted in SpiderMonkey parlance -- but such
            // hoisting doesn't extend outside of this statement, so it is not
            // hoisting in the sense meant by ContainsHoistedDeclaration.)
            MOZ_ASSERT(loopHead->isArity(PN_TERNARY));

            ParseNode* init = loopHead->pn_kid1;
            if (init && init->isKind(PNK_VAR)) {
                *result = true;
                return true;
            }
        } else {
            MOZ_ASSERT(loopHead->isKind(PNK_FORIN) || loopHead->isKind(PNK_FOROF));

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
            if (decl && decl->isKind(PNK_VAR)) {
                *result = true;
                return true;
            }
        }

        ParseNode* loopBody = node->pn_right;
        return ContainsHoistedDeclaration(cx, loopBody, result);
      }

      case PNK_LETBLOCK: {
        MOZ_ASSERT(node->isArity(PN_BINARY));
        MOZ_ASSERT(node->pn_right->isKind(PNK_LEXICALSCOPE));
        MOZ_ASSERT(node->pn_left->isKind(PNK_LET) ||
                   (node->pn_left->isKind(PNK_CONST) && node->pn_right->pn_expr->isKind(PNK_FOR)),
                   "a let-block's left half is its declarations: ordinarily a PNK_LET node but "
                   "PNK_CONST in the weird case of |for (const x ...)|");
        return ContainsHoistedDeclaration(cx, node->pn_right, result);
      }

      case PNK_LEXICALSCOPE: {
        MOZ_ASSERT(node->isArity(PN_NAME));
        ParseNode* expr = node->pn_expr;

        if (expr->isKind(PNK_FOR))
            return ContainsHoistedDeclaration(cx, expr, result);

        MOZ_ASSERT(expr->isKind(PNK_STATEMENTLIST));
        return ListContainsHoistedDeclaration(cx, &node->pn_expr->as<ListNode>(), result);
      }

      // List nodes with all non-null children.
      case PNK_STATEMENTLIST:
        return ListContainsHoistedDeclaration(cx, &node->as<ListNode>(), result);

      // Grammar sub-components that should never be reached directly by this
      // method, because some parent component should have asserted itself.
      case PNK_OBJECT_PROPERTY_NAME:
      case PNK_COMPUTED_NAME:
      case PNK_SPREAD:
      case PNK_MUTATEPROTO:
      case PNK_COLON:
      case PNK_SHORTHAND:
      case PNK_CONDITIONAL:
      case PNK_TYPEOFNAME:
      case PNK_TYPEOFEXPR:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_DELETENAME:
      case PNK_DELETEPROP:
      case PNK_DELETESUPERPROP:
      case PNK_DELETEELEM:
      case PNK_DELETESUPERELEM:
      case PNK_DELETEEXPR:
      case PNK_POS:
      case PNK_NEG:
      case PNK_PREINCREMENT:
      case PNK_POSTINCREMENT:
      case PNK_PREDECREMENT:
      case PNK_POSTDECREMENT:
      case PNK_OR:
      case PNK_AND:
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_STRICTEQ:
      case PNK_EQ:
      case PNK_STRICTNE:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_INSTANCEOF:
      case PNK_IN:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_ADD:
      case PNK_SUB:
      case PNK_STAR:
      case PNK_DIV:
      case PNK_MOD:
      case PNK_POW:
      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
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
      case PNK_POWASSIGN:
      case PNK_COMMA:
      case PNK_ARRAY:
      case PNK_OBJECT:
      case PNK_DOT:
      case PNK_ELEM:
      case PNK_CALL:
      case PNK_NAME:
      case PNK_TEMPLATE_STRING:
      case PNK_TEMPLATE_STRING_LIST:
      case PNK_TAGGED_TEMPLATE:
      case PNK_CALLSITEOBJ:
      case PNK_STRING:
      case PNK_REGEXP:
      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_NULL:
      case PNK_THIS:
      case PNK_ELISION:
      case PNK_NUMBER:
      case PNK_NEW:
      case PNK_GENERATOR:
      case PNK_GENEXP:
      case PNK_ARRAYCOMP:
      case PNK_ARGSBODY:
      case PNK_CATCHLIST:
      case PNK_CATCH:
      case PNK_FORIN:
      case PNK_FOROF:
      case PNK_FORHEAD:
      case PNK_FRESHENBLOCK:
      case PNK_CLASSMETHOD:
      case PNK_CLASSMETHODLIST:
      case PNK_CLASSNAMES:
      case PNK_SUPERPROP:
      case PNK_SUPERELEM:
      case PNK_NEWTARGET:
        MOZ_CRASH("ContainsHoistedDeclaration should have indicated false on "
                  "some parent node without recurring to test this node");

      case PNK_GLOBALCONST:
        MOZ_CRASH("ContainsHoistedDeclaration is only called on nested nodes where "
                  "globalconst nodes should never have been generated");

      case PNK_LIMIT: // invalid sentinel value
        MOZ_CRASH("unexpected PNK_LIMIT in node");
    }

    MOZ_CRASH("invalid node kind");
}

/*
 * Fold from one constant type to another.
 * XXX handles only strings and numbers for now
 */
static bool
FoldType(ExclusiveContext* cx, ParseNode* pn, ParseNodeKind kind)
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
                pn->pn_atom = NumberToAtom(cx, pn->pn_dval);
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
FoldBinaryNumeric(ExclusiveContext* cx, JSOp op, ParseNode* pn1, ParseNode* pn2,
                  ParseNode* pn)
{
    double d, d2;
    int32_t i, j;

    MOZ_ASSERT(pn1->isKind(PNK_NUMBER) && pn2->isKind(PNK_NUMBER));
    d = pn1->pn_dval;
    d2 = pn2->pn_dval;
    switch (op) {
      case JSOP_LSH:
      case JSOP_RSH:
        i = ToInt32(d);
        j = ToInt32(d2);
        j &= 31;
        d = int32_t((op == JSOP_LSH) ? uint32_t(i) << j : i >> j);
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
                d = NegativeInfinity<double>();
            else
                d = PositiveInfinity<double>();
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

      case JSOP_POW:
        d = ecmaPow(d, d2);
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
ReplaceNode(ParseNode** pnp, ParseNode* pn)
{
    pn->pn_next = (*pnp)->pn_next;
    *pnp = pn;
}

enum Truthiness { Truthy, Falsy, Unknown };

static Truthiness
Boolish(ParseNode* pn)
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
enum class SyntacticContext : int {
    // pn is an expression, and it appears in a context where only its side
    // effects and truthiness matter: the condition of an if statement,
    // conditional expression, while loop, or for(;;) loop; or an operand of &&
    // or || in such a context.
    Condition,

    // pn is the operand of the 'delete' keyword.
    Delete,

    // Any other syntactic context.
    Other
};

static SyntacticContext
condIf(const ParseNode* pn, ParseNodeKind kind)
{
    return pn->isKind(kind) ? SyntacticContext::Condition : SyntacticContext::Other;
}

static bool
Fold(ExclusiveContext* cx, ParseNode** pnp, Parser<FullParseHandler>& parser, bool inGenexpLambda,
     SyntacticContext sc);

static bool
FoldTypeOfExpr(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
               bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_TYPEOFEXPR));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Other))
        return false;

    // Constant-fold the entire |typeof| if given a constant with known type.
    RootedPropertyName result(cx);
    if (expr->isKind(PNK_STRING) || expr->isKind(PNK_TEMPLATE_STRING))
        result = cx->names().string;
    else if (expr->isKind(PNK_NUMBER))
        result = cx->names().number;
    else if (expr->isKind(PNK_NULL))
        result = cx->names().object;
    else if (expr->isKind(PNK_TRUE) || expr->isKind(PNK_FALSE))
        result = cx->names().boolean;
    else if (expr->isKind(PNK_FUNCTION))
        result = cx->names().function;

    if (result) {
        parser.prepareNodeForMutation(node);

        node->setKind(PNK_STRING);
        node->setArity(PN_NULLARY);
        node->setOp(JSOP_NOP);
        node->pn_atom = result;
    }

    return true;
}

static bool
FoldVoid(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
         bool inGenexpLambda, SyntacticContext sc)
{
    MOZ_ASSERT(node->isKind(PNK_VOID));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Other))
        return false;

    if (sc == SyntacticContext::Condition) {
        if (expr->isKind(PNK_TRUE) ||
            expr->isKind(PNK_FALSE) ||
            expr->isKind(PNK_STRING) ||
            expr->isKind(PNK_TEMPLATE_STRING) ||
            expr->isKind(PNK_NUMBER) ||
            expr->isKind(PNK_NULL) ||
            expr->isKind(PNK_FUNCTION))
        {
            parser.prepareNodeForMutation(node);
            node->setKind(PNK_FALSE);
            node->setArity(PN_NULLARY);
            node->setOp(JSOP_FALSE);
        }
    }

    return true;
}

static bool
FoldDeleteExpr(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
               bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_DELETEEXPR));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Other))
        return false;

    // Expression deletion evaluates the expression, then evaluates to
    // true.  For trivial expressions, eliminate the expression evaluation.
    if (expr->isKind(PNK_TRUE) ||
        expr->isKind(PNK_FALSE) ||
        expr->isKind(PNK_STRING) ||
        expr->isKind(PNK_TEMPLATE_STRING) ||
        expr->isKind(PNK_NUMBER) ||
        expr->isKind(PNK_NULL) ||
        expr->isKind(PNK_FUNCTION))
    {
        parser.prepareNodeForMutation(node);
        node->setKind(PNK_TRUE);
        node->setArity(PN_NULLARY);
        node->setOp(JSOP_TRUE);
    }

    return true;
}

static bool
FoldDeleteElement(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
                  bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_DELETEELEM) || node->isKind(PNK_DELETESUPERELEM));
    MOZ_ASSERT(node->isArity(PN_UNARY));
    MOZ_ASSERT(node->pn_kid->isKind(PNK_ELEM) || node->pn_kid->isKind(PNK_SUPERELEM));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Other))
        return false;

    // If we're deleting an element, but constant-folding converted our
    // element reference into a dotted property access, we must *also*
    // morph the node's kind.
    //
    // In principle this also applies to |super["foo"] -> super.foo|,
    // but we don't constant-fold |super["foo"]| yet.
    if (node->isKind(PNK_DELETEELEM)) {
        MOZ_ASSERT(expr->isKind(PNK_ELEM) || expr->isKind(PNK_DOT));
        if (expr->isKind(PNK_DOT))
            node->setKind(PNK_DELETEPROP);
    }

    return true;
}

static bool
FoldDeleteProperty(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
                   bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_DELETEPROP) || node->isKind(PNK_DELETESUPERPROP));
    MOZ_ASSERT(node->isArity(PN_UNARY));
    MOZ_ASSERT(node->pn_kid->isKind(PNK_DOT) || node->pn_kid->isKind(PNK_SUPERPROP));

    ParseNode*& expr = node->pn_kid;
#ifdef DEBUG
    ParseNodeKind oldKind = expr->getKind();
#endif

    if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Other))
        return false;

    MOZ_ASSERT(expr->isKind(oldKind),
               "kind should have remained invariant under folding");

    return true;
}

static bool
FoldNot(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
        bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_NOT));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Condition))
        return false;

    if (expr->isKind(PNK_NUMBER)) {
        double d = expr->pn_dval;

        parser.prepareNodeForMutation(node);
        if (d == 0 || IsNaN(d)) {
            node->setKind(PNK_TRUE);
            node->setOp(JSOP_TRUE);
        } else {
            node->setKind(PNK_FALSE);
            node->setOp(JSOP_FALSE);
        }
        node->setArity(PN_NULLARY);
    } else if (expr->isKind(PNK_TRUE) || expr->isKind(PNK_FALSE)) {
        bool newval = !expr->isKind(PNK_TRUE);

        parser.prepareNodeForMutation(node);
        node->setKind(newval ? PNK_TRUE : PNK_FALSE);
        node->setArity(PN_NULLARY);
        node->setOp(newval ? JSOP_TRUE : JSOP_FALSE);
    }

    return true;
}

static bool
FoldUnaryArithmetic(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
                    bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_BITNOT) || node->isKind(PNK_POS) || node->isKind(PNK_NEG),
               "need a different method for this node kind");
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& expr = node->pn_kid;
    if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Other))
        return false;

    if (expr->isKind(PNK_NUMBER) || expr->isKind(PNK_TRUE) || expr->isKind(PNK_FALSE)) {
        double d = expr->isKind(PNK_NUMBER)
                   ? expr->pn_dval
                   : double(expr->isKind(PNK_TRUE));

        if (node->isKind(PNK_BITNOT))
            d = ~ToInt32(d);
        else if (node->isKind(PNK_NEG))
            d = -d;
        else
            MOZ_ASSERT(node->isKind(PNK_POS)); // nothing to do

        parser.prepareNodeForMutation(node);
        node->setKind(PNK_NUMBER);
        node->setOp(JSOP_DOUBLE);
        node->setArity(PN_NULLARY);
        node->pn_dval = d;
    }

    return true;
}

static bool
FoldIncrementDecrement(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
                       bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_PREINCREMENT) ||
               node->isKind(PNK_POSTINCREMENT) ||
               node->isKind(PNK_PREDECREMENT) ||
               node->isKind(PNK_POSTDECREMENT));
    MOZ_ASSERT(node->isArity(PN_UNARY));

    ParseNode*& target = node->pn_kid;
    MOZ_ASSERT(parser.isValidSimpleAssignmentTarget(target, Parser<FullParseHandler>::PermitAssignmentToFunctionCalls));

    if (!Fold(cx, &target, parser, inGenexpLambda, SyntacticContext::Other))
        return false;

    MOZ_ASSERT(parser.isValidSimpleAssignmentTarget(target, Parser<FullParseHandler>::PermitAssignmentToFunctionCalls));

    return true;
}

static bool
FoldAndOr(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
          bool inGenexpLambda, SyntacticContext sc)
{
    ParseNode* node = *nodePtr;

    MOZ_ASSERT(node->isKind(PNK_AND) || node->isKind(PNK_OR));
    MOZ_ASSERT(node->isArity(PN_LIST));

    bool isOrNode = node->isKind(PNK_OR);
    ParseNode** elem = &node->pn_head;
    do {
        // Pass |sc| through to propagate conditionality.
        if (!Fold(cx, elem, parser, inGenexpLambda, sc))
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

        node->setKind(PNK_NULL);
        node->setArity(PN_NULLARY);
        parser.freeTree(node);
    }

    return true;
}

static bool
FoldConditional(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
                bool inGenexpLambda)
{
    ParseNode** nextNode = nodePtr;

    do {
        // |nextNode| on entry points to the C?T:F expression to be folded.
        // Reset it to exit the loop in the common case where F isn't another
        // ?: expression.
        nodePtr = nextNode;
        nextNode = nullptr;

        ParseNode* node = *nodePtr;
        MOZ_ASSERT(node->isKind(PNK_CONDITIONAL));
        MOZ_ASSERT(node->isArity(PN_TERNARY));

        ParseNode*& expr = node->pn_kid1;
        if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Condition))
            return false;

        ParseNode*& ifTruthy = node->pn_kid2;
        if (!Fold(cx, &ifTruthy, parser, inGenexpLambda, SyntacticContext::Other))
            return false;

        ParseNode*& ifFalsy = node->pn_kid3;

        // If our C?T:F node has F as another ?: node, *iteratively* constant-
        // fold F *after* folding C and T (and possibly eliminating C and one
        // of T/F entirely); otherwise fold F normally.  Making |nextNode| non-
        // null causes this loop to run again to fold F.
        //
        // Conceivably we could instead/also iteratively constant-fold T, if T
        // were more complex than F.  Such an optimization is unimplemented.
        if (ifFalsy->isKind(PNK_CONDITIONAL)) {
            nextNode = &ifFalsy;
        } else {
            if (!Fold(cx, &ifFalsy, parser, inGenexpLambda, SyntacticContext::Other))
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

        // Don't decay the overall expression if the replacement node is a
        // a definition.
        //
        // The rationale for this pre-existing restriction is unclear; if you
        // discover it, please document it!  Speculation is that it has
        // something to do with constant-folding something like:
        //
        //   true ? function f() {} : false;
        //
        // into
        //
        //   function f() {}
        //
        // and worrying this might convert a function *expression* into a
        // function *statement* that defined its name early.  But function
        // expressions aren't isDefn(), so this can't be it.
        //
        // This lack of explanation is tolerated only because failing to
        // optimize *should* always be okay.
        if (replacement->isDefn())
            continue;

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
FoldIf(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
       bool inGenexpLambda)
{
    ParseNode** nextNode = nodePtr;

    do {
        // |nextNode| on entry points to the initial |if| to be folded.  Reset
        // it to exit the loop when the |else| arm isn't another |if|.
        nodePtr = nextNode;
        nextNode = nullptr;

        ParseNode* node = *nodePtr;
        MOZ_ASSERT(node->isKind(PNK_IF));
        MOZ_ASSERT(node->isArity(PN_TERNARY));

        ParseNode*& expr = node->pn_kid1;
        if (!Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Condition))
            return false;

        ParseNode*& consequent = node->pn_kid2;
        if (!Fold(cx, &consequent, parser, inGenexpLambda, SyntacticContext::Other))
            return false;

        ParseNode*& alternative = node->pn_kid3;
        if (alternative) {
            // If in |if (C) T; else F;| we have |F| as another |if|,
            // *iteratively* constant-fold |F| *after* folding |C| and |T| (and
            // possibly completely replacing the whole thing with |T| or |F|);
            // otherwise fold F normally.  Making |nextNode| non-null causes
            // this loop to run again to fold F.
            if (alternative->isKind(PNK_IF)) {
                nextNode = &alternative;
            } else {
                if (!Fold(cx, &alternative, parser, inGenexpLambda,
                          SyntacticContext::Other))
                {
                    return false;
                }
            }
        }

        // Eliminate the consequent or alternative if the condition has
        // constant truthiness.  Don't eliminate if we have an |if (0)| in
        // trailing position in a generator expression, as this is a special
        // form we can't fold away.
        Truthiness t = Boolish(expr);
        if (t == Unknown || inGenexpLambda)
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
            node->setKind(PNK_STATEMENTLIST);
            node->setArity(PN_LIST);
            node->makeEmpty();
        } else {
            // As with PNK_CONDITIONAL, replace only if the replacement isn't a
            // definition.  As there, the rationale for this restriction is
            // unclear and undocumented: tolerated only because a failure to
            // optimize *should* be safe.  The best guess is that this test was
            // an incomplete, buggy version of the |ContainsHoistedDeclaration|
            // test.
            if (replacement->isDefn())
                continue;

            // Replacement invalidates |nextNode|, so reset it (if the
            // replacement requires folding) or clear it (if |alternative|
            // is dead code) as needed.
            if (nextNode)
                nextNode = (*nextNode == replacement) ? nodePtr : nullptr;
            ReplaceNode(nodePtr, replacement);

            // Morph the original node into a discardable node, then
            // aggressively free it and the discarded arm (if any) to suss out
            // any bugs in the preceding logic.
            node->setKind(PNK_STATEMENTLIST);
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
FoldFunction(ExclusiveContext* cx, ParseNode* node, Parser<FullParseHandler>& parser,
             bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_FUNCTION));
    MOZ_ASSERT(node->isArity(PN_CODE));

    // Don't constant-fold inside "use asm" code, as this could create a parse
    // tree that doesn't type-check as asm.js.
    if (node->pn_funbox->useAsmOrInsideUseAsm())
        return true;

    // Note: pn_body is null for lazily-parsed functions.
    if (ParseNode*& functionBody = node->pn_body) {
        if (!Fold(cx, &functionBody, parser, node->pn_funbox->inGenexpLambda,
                  SyntacticContext::Other))
        {
            return false;
        }
    }

    return true;
}

bool
Fold(ExclusiveContext* cx, ParseNode** pnp, Parser<FullParseHandler>& parser, bool inGenexpLambda,
     SyntacticContext sc)
{
    JS_CHECK_RECURSION(cx, return false);

    ParseNode* pn = *pnp;
    ParseNode* pn1 = nullptr;
    ParseNode* pn2 = nullptr;
    ParseNode* pn3 = nullptr;

    switch (pn->getKind()) {
      case PNK_NEWTARGET:
      case PNK_NOP:
      case PNK_REGEXP:
      case PNK_STRING:
      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_NULL:
      case PNK_ELISION:
      case PNK_NUMBER:
      case PNK_DEBUGGER:
      case PNK_BREAK:
      case PNK_CONTINUE:
      case PNK_TEMPLATE_STRING:
      case PNK_THIS:
      case PNK_GENERATOR:
      case PNK_EXPORT_BATCH_SPEC:
      case PNK_OBJECT_PROPERTY_NAME:
      case PNK_SUPERPROP:
      case PNK_FRESHENBLOCK:
        MOZ_ASSERT(pn->isArity(PN_NULLARY));
        goto afterFolding;

      case PNK_TYPEOFNAME:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        MOZ_ASSERT(pn->pn_kid->isKind(PNK_NAME));
        MOZ_ASSERT(!pn->pn_kid->maybeExpr());
        return true;

      case PNK_TYPEOFEXPR:
        return FoldTypeOfExpr(cx, pn, parser, inGenexpLambda);

      case PNK_VOID:
        return FoldVoid(cx, pn, parser, inGenexpLambda, sc);

      case PNK_DELETENAME: {
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        MOZ_ASSERT(pn->pn_kid->isKind(PNK_NAME));
        return true;
      }

      case PNK_DELETEEXPR:
        return FoldDeleteExpr(cx, pn, parser, inGenexpLambda);

      case PNK_DELETEELEM:
      case PNK_DELETESUPERELEM:
        return FoldDeleteElement(cx, pn, parser, inGenexpLambda);

      case PNK_DELETEPROP:
      case PNK_DELETESUPERPROP:
        return FoldDeleteProperty(cx, pn, parser, inGenexpLambda);

      case PNK_CONDITIONAL:
        return FoldConditional(cx, pnp, parser, inGenexpLambda);

      case PNK_IF:
        return FoldIf(cx, pnp, parser, inGenexpLambda);

      case PNK_NOT:
        return FoldNot(cx, pn, parser, inGenexpLambda);

      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG:
        return FoldUnaryArithmetic(cx, pn, parser, inGenexpLambda);

      case PNK_PREINCREMENT:
      case PNK_POSTINCREMENT:
      case PNK_PREDECREMENT:
      case PNK_POSTDECREMENT:
        return FoldIncrementDecrement(cx, pn, parser, inGenexpLambda);

      case PNK_THROW:
      case PNK_ARRAYPUSH:
      case PNK_MUTATEPROTO:
      case PNK_COMPUTED_NAME:
      case PNK_SPREAD:
      case PNK_SUPERELEM:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        return Fold(cx, &pn->pn_kid, parser, inGenexpLambda, SyntacticContext::Other);

      case PNK_SEMI:
        MOZ_ASSERT(pn->isArity(PN_UNARY));
        if (ParseNode*& expr = pn->pn_kid)
            return Fold(cx, &expr, parser, inGenexpLambda, SyntacticContext::Other);
        return true;

      case PNK_AND:
      case PNK_OR:
        return FoldAndOr(cx, pnp, parser, inGenexpLambda, sc);

      case PNK_FUNCTION:
        return FoldFunction(cx, pn, parser, inGenexpLambda);

      case PNK_EXPORT:
      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
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
      case PNK_POWASSIGN:
      case PNK_ELEM:
      case PNK_COLON:
      case PNK_CASE:
      case PNK_SHORTHAND:
      case PNK_DOWHILE:
      case PNK_WHILE:
      case PNK_SWITCH:
      case PNK_LETBLOCK:
      case PNK_FOR:
      case PNK_CLASSMETHOD:
      case PNK_WITH:
      case PNK_CLASSNAMES:
      case PNK_DEFAULT:
      case PNK_YIELD_STAR:
      case PNK_YIELD:
      case PNK_RETURN:
      case PNK_IMPORT:
      case PNK_EXPORT_FROM:
      case PNK_EXPORT_DEFAULT:
      case PNK_FORIN:
      case PNK_FOROF:
      case PNK_FORHEAD:
      case PNK_CLASS:
      case PNK_TRY:
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_STRICTEQ:
      case PNK_EQ:
      case PNK_STRICTNE:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_INSTANCEOF:
      case PNK_IN:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_ADD:
      case PNK_SUB:
      case PNK_STAR:
      case PNK_DIV:
      case PNK_MOD:
      case PNK_POW:
      case PNK_COMMA:
      case PNK_NEW:
      case PNK_CALL:
      case PNK_GENEXP:
      case PNK_ARRAY:
      case PNK_STATEMENTLIST:
      case PNK_ARGSBODY:
      case PNK_ARRAYCOMP:
      case PNK_VAR:
      case PNK_CONST:
      case PNK_LET:
      case PNK_GLOBALCONST:
      case PNK_OBJECT:
      case PNK_CLASSMETHODLIST:
      case PNK_TEMPLATE_STRING_LIST:
      case PNK_TAGGED_TEMPLATE:
      case PNK_EXPORT_SPEC_LIST:
      case PNK_IMPORT_SPEC_LIST:
      case PNK_CATCHLIST:
      case PNK_LABEL:
      case PNK_DOT:
      case PNK_LEXICALSCOPE:
      case PNK_NAME:
      case PNK_CATCH:
      case PNK_EXPORT_SPEC:
      case PNK_IMPORT_SPEC:
      case PNK_CALLSITEOBJ:
        MOZ_ASSERT(!pn->isArity(PN_CODE), "only functions are code nodes");
        break; // for now

      case PNK_LIMIT: // invalid sentinel value
        MOZ_CRASH("invalid node kind");
    }

    // First, recursively fold constants on the children of this node.
    switch (pn->getArity()) {
      case PN_CODE:
        MOZ_ASSERT(pn->isKind(PNK_FUNCTION));
        MOZ_CRASH("should have been handled above");

      case PN_LIST:
      {
        // Don't fold a parenthesized call expression. See bug 537673.
        ParseNode** listp = &pn->pn_head;
        if ((pn->isKind(PNK_CALL) || pn->isKind(PNK_TAGGED_TEMPLATE)) && (*listp)->isInParens())
            listp = &(*listp)->pn_next;

        for (; *listp; listp = &(*listp)->pn_next) {
            if (!Fold(cx, listp, parser, inGenexpLambda, SyntacticContext::Other))
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
        MOZ_ASSERT(!pn->isKind(PNK_CONDITIONAL),
                   "should be skipping this above");
        MOZ_ASSERT(!pn->isKind(PNK_IF),
                   "should be skipping this above");
        /* Any kid may be null (e.g. for (;;)). */
        if (pn->pn_kid1) {
            if (!Fold(cx, &pn->pn_kid1, parser, inGenexpLambda, SyntacticContext::Other))
                return false;
        }
        pn1 = pn->pn_kid1;

        if (pn->pn_kid2) {
            if (!Fold(cx, &pn->pn_kid2, parser, inGenexpLambda, condIf(pn, PNK_FORHEAD)))
                return false;
            if (pn->isKind(PNK_FORHEAD) && pn->pn_kid2->isKind(PNK_TRUE)) {
                parser.freeTree(pn->pn_kid2);
                pn->pn_kid2 = nullptr;
            }
        }
        pn2 = pn->pn_kid2;

        if (pn->pn_kid3) {
            if (!Fold(cx, &pn->pn_kid3, parser, inGenexpLambda, SyntacticContext::Other))
                return false;
        }
        pn3 = pn->pn_kid3;
        break;

      case PN_BINARY:
      case PN_BINARY_OBJ:
        /* First kid may be null (for default case in switch). */
        if (pn->pn_left) {
            if (!Fold(cx, &pn->pn_left, parser, inGenexpLambda, condIf(pn, PNK_WHILE)))
                return false;
        }
        /* Second kid may be null (for return in non-generator). */
        if (pn->pn_right) {
            if (!Fold(cx, &pn->pn_right, parser, inGenexpLambda, condIf(pn, PNK_DOWHILE)))
                return false;
        }
        pn1 = pn->pn_left;
        pn2 = pn->pn_right;
        break;

      case PN_UNARY:
        MOZ_ASSERT(!IsDeleteKind(pn->getKind()),
                   "should have been handled above");
        if (pn->pn_kid) {
            if (!Fold(cx, &pn->pn_kid, parser, inGenexpLambda, SyntacticContext::Other))
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
            ParseNode** lhsp = &pn->pn_expr;
            while (*lhsp && (*lhsp)->isArity(PN_NAME) && !(*lhsp)->isUsed())
                lhsp = &(*lhsp)->pn_expr;
            if (*lhsp && !Fold(cx, lhsp, parser, inGenexpLambda, SyntacticContext::Other))
                return false;
            pn1 = *lhsp;
        }
        break;

      case PN_NULLARY:
        break;
    }

    // The immediate child of a PNK_DELETE* node should not be replaced
    // with node indicating a different syntactic form; |delete x| is not
    // the same as |delete (true && x)|. See bug 888002.
    //
    // pn is the immediate child in question. Its descendants were already
    // constant-folded above, so we're done.
    if (sc == SyntacticContext::Delete)
        return true;

    switch (pn->getKind()) {
      case PNK_ADD: {
        MOZ_ASSERT(pn->isArity(PN_LIST));

        bool folded = false;

        pn2 = pn1->pn_next;
        if (pn1->isKind(PNK_NUMBER)) {
            // Fold addition of numeric literals: (1 + 2 + x === 3 + x).
            // Note that we can only do this the front of the list:
            // (x + 1 + 2 !== x + 3) when x is a string.
            while (pn2 && pn2->isKind(PNK_NUMBER)) {
                pn1->pn_dval += pn2->pn_dval;
                pn1->pn_next = pn2->pn_next;
                parser.freeTree(pn2);
                pn2 = pn1->pn_next;
                pn->pn_count--;
                folded = true;
            }
        }

        // Now search for adjacent pairs of literals to fold for string
        // concatenation.
        //
        // isStringConcat is true if we know the operation we're looking at
        // will be string concatenation at runtime.  As soon as we see a
        // string, we know that every addition to the right of it will be
        // string concatenation, even if both operands are numbers:
        // ("s" + x + 1 + 2 === "s" + x + "12").
        //
        bool isStringConcat = false;
        RootedString foldedStr(cx);

        // (number + string) is definitely concatenation, but only at the
        // front of the list: (x + 1 + "2" !== x + "12") when x is a
        // number.
        if (pn1->isKind(PNK_NUMBER) && pn2 && pn2->isKind(PNK_STRING))
            isStringConcat = true;

        while (pn2) {
            isStringConcat = isStringConcat || pn1->isKind(PNK_STRING);

            if (isStringConcat &&
                (pn1->isKind(PNK_STRING) || pn1->isKind(PNK_NUMBER)) &&
                (pn2->isKind(PNK_STRING) || pn2->isKind(PNK_NUMBER)))
            {
                // Fold string concatenation of literals.
                if (pn1->isKind(PNK_NUMBER) && !FoldType(cx, pn1, PNK_STRING))
                    return false;
                if (pn2->isKind(PNK_NUMBER) && !FoldType(cx, pn2, PNK_STRING))
                    return false;
                if (!foldedStr)
                    foldedStr = pn1->pn_atom;
                RootedString right(cx, pn2->pn_atom);
                foldedStr = ConcatStrings<CanGC>(cx, foldedStr, right);
                if (!foldedStr)
                    return false;
                pn1->pn_next = pn2->pn_next;
                parser.freeTree(pn2);
                pn2 = pn1->pn_next;
                pn->pn_count--;
                folded = true;
            } else {
                if (foldedStr) {
                    // Convert the rope of folded strings into an Atom.
                    pn1->pn_atom = AtomizeString(cx, foldedStr);
                    if (!pn1->pn_atom)
                        return false;
                    foldedStr = nullptr;
                }
                pn1 = pn2;
                pn2 = pn2->pn_next;
            }
        }

        if (foldedStr) {
            // Convert the rope of folded strings into an Atom.
            pn1->pn_atom = AtomizeString(cx, foldedStr);
            if (!pn1->pn_atom)
                return false;
        }

        if (folded) {
            if (pn->pn_count == 1) {
                // We reduced the list to one constant. There is no
                // addition anymore. Replace the PNK_ADD node with the
                // single PNK_STRING or PNK_NUMBER node.
                ReplaceNode(pnp, pn1);
                pn = pn1;
            } else if (!pn2) {
                pn->pn_tail = &pn1->pn_next;
            }
        }

        break;
      }

      case PNK_SUB:
      case PNK_STAR:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_DIV:
      case PNK_MOD:
      case PNK_POW:
        MOZ_ASSERT(pn->getArity() == PN_LIST);
        MOZ_ASSERT(pn->pn_count >= 2);
        for (pn2 = pn1; pn2; pn2 = pn2->pn_next) {
            if (!FoldType(cx, pn2, PNK_NUMBER))
                return false;
        }
        for (pn2 = pn1; pn2; pn2 = pn2->pn_next) {
            /* XXX fold only if all operands convert to number */
            if (!pn2->isKind(PNK_NUMBER))
                break;
        }

        // No constant-folding for (2**3**5), because (**) is right-
        // associative. We would have to reverse the list. It's not worth it.
        if (pn->getKind() == PNK_POW && pn->pn_count > 2)
            break;

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
        break;

      case PNK_TYPEOFNAME:
      case PNK_TYPEOFEXPR:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG:
      case PNK_CONDITIONAL:
      case PNK_IF:
      case PNK_AND:
      case PNK_OR:
        MOZ_CRASH("should have been fully handled above");

      case PNK_ELEM: {
        // An indexed expression, pn1[pn2]. A few cases can be improved.
        PropertyName* name = nullptr;
        if (pn2->isKind(PNK_STRING)) {
            JSAtom* atom = pn2->pn_atom;
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
                JSAtom* atom = ToAtom<NoGC>(cx, DoubleValue(number));
                if (!atom)
                    return false;
                name = atom->asPropertyName();
            }
        }

        if (name && NameToId(name) == IdToTypeId(NameToId(name))) {
            // Optimization 3: We have pn1["foo"] where foo is not an index.
            // Convert to a property access (like pn1.foo) which we optimize
            // better downstream. Don't bother with this for names which TI
            // considers to be indexes, to simplify downstream analysis.
            ParseNode* expr = parser.handler.newPropertyAccess(pn->pn_left, name, pn->pn_pos.end);
            if (!expr)
                return false;
            expr->setInParens(pn->isInParens());
            ReplaceNode(pnp, expr);

            // Supposing we're replacing |obj["prop"]| with |obj.prop|, we now
            // can free the |"prop"| node and |obj["prop"]| nodes -- but not
            // the |obj| node now a sub-node of |expr|.  Mutate |pn| into a
            // node with |"prop"| as its child so that our |pn| doesn't have a
            // necessarily-weird structure (say, by nulling out |pn->pn_left|
            // only) that would fail AST sanity assertions performed by
            // |parser.freeTree(pn)|.
            pn->setKind(PNK_TYPEOFEXPR);
            pn->setArity(PN_UNARY);
            pn->pn_kid = pn2;
            parser.freeTree(pn);

            pn = expr;
        }
        break;
      }

      default:;
    }

  afterFolding:
    if (sc == SyntacticContext::Condition) {
        Truthiness t = Boolish(pn);
        if (t != Unknown) {
            /*
             * We can turn function nodes into constant nodes here, but mutating function
             * nodes is tricky --- in particular, mutating a function node that appears on
             * a method list corrupts the method list. However, methods are M's in
             * statements of the form 'this.foo = M;', which we never fold, so we're okay.
             */
            parser.prepareNodeForMutation(pn);
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
frontend::FoldConstants(ExclusiveContext* cx, ParseNode** pnp, Parser<FullParseHandler>* parser)
{
    // Don't constant-fold inside "use asm" code, as this could create a parse
    // tree that doesn't type-check as asm.js.
    if (parser->pc->useAsmOrInsideUseAsm())
        return true;

    return Fold(cx, pnp, *parser, false, SyntacticContext::Other);
}

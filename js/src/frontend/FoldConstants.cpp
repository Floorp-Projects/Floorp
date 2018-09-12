/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/FoldConstants.h"

#include "mozilla/FloatingPoint.h"

#include "jslibmath.h"
#include "jsnum.h"

#include "frontend/ParseNode.h"
#include "frontend/Parser.h"
#include "js/Conversions.h"
#include "vm/StringType.h"

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
    for (ParseNode* node : list->contents()) {
        if (!ContainsHoistedDeclaration(cx, node, result)) {
            return false;
        }
        if (*result) {
            return true;
        }
    }

    *result = false;
    return true;
}

// Determines whether the given ParseNode contains any declarations whose
// visibility will extend outside the node itself -- that is, whether the
// ParseNode contains any var statements.
//
// THIS IS NOT A GENERAL-PURPOSE FUNCTION.  It is only written to work in the
// specific context of deciding that |node|, as one arm of a ParseNodeKind::If
// controlled by a constant condition, contains a declaration that forbids
// |node| being completely eliminated as dead.
static bool
ContainsHoistedDeclaration(JSContext* cx, ParseNode* node, bool* result)
{
    if (!CheckRecursionLimit(cx)) {
        return false;
    }

  restart:

    // With a better-typed AST, we would have distinct parse node classes for
    // expressions and for statements and would characterize expressions with
    // ExpressionKind and statements with StatementKind.  Perhaps someday.  In
    // the meantime we must characterize every ParseNodeKind, even the
    // expression/sub-expression ones that, if we handle all statement kinds
    // correctly, we'll never see.
    switch (node->getKind()) {
      // Base case.
      case ParseNodeKind::Var:
        *result = true;
        return true;

      // Non-global lexical declarations are block-scoped (ergo not hoistable).
      case ParseNodeKind::Let:
      case ParseNodeKind::Const:
        MOZ_ASSERT(node->is<ListNode>());
        *result = false;
        return true;

      // Similarly to the lexical declarations above, classes cannot add hoisted
      // declarations
      case ParseNodeKind::Class:
        MOZ_ASSERT(node->is<ClassNode>());
        *result = false;
        return true;

      // Function declarations *can* be hoisted declarations.  But in the
      // magical world of the rewritten frontend, the declaration necessitated
      // by a nested function statement, not at body level, doesn't require
      // that we preserve an unreachable function declaration node against
      // dead-code removal.
      case ParseNodeKind::Function:
        MOZ_ASSERT(node->is<CodeNode>());
        *result = false;
        return true;

      case ParseNodeKind::Module:
        *result = false;
        return true;

      // Statements with no sub-components at all.
      case ParseNodeKind::EmptyStatement:
        MOZ_ASSERT(node->is<NullaryNode>());
        *result = false;
        return true;

      case ParseNodeKind::Debugger:
        MOZ_ASSERT(node->is<DebuggerStatement>());
        *result = false;
        return true;

      // Statements containing only an expression have no declarations.
      case ParseNodeKind::ExpressionStatement:
      case ParseNodeKind::Throw:
      case ParseNodeKind::Return:
        MOZ_ASSERT(node->is<UnaryNode>());
        *result = false;
        return true;

      // These two aren't statements in the spec, but we sometimes insert them
      // in statement lists anyway.
      case ParseNodeKind::InitialYield:
      case ParseNodeKind::YieldStar:
      case ParseNodeKind::Yield:
        MOZ_ASSERT(node->is<UnaryNode>());
        *result = false;
        return true;

      // Other statements with no sub-statement components.
      case ParseNodeKind::Break:
      case ParseNodeKind::Continue:
      case ParseNodeKind::Import:
      case ParseNodeKind::ImportSpecList:
      case ParseNodeKind::ImportSpec:
      case ParseNodeKind::ExportFrom:
      case ParseNodeKind::ExportDefault:
      case ParseNodeKind::ExportSpecList:
      case ParseNodeKind::ExportSpec:
      case ParseNodeKind::Export:
      case ParseNodeKind::ExportBatchSpec:
      case ParseNodeKind::CallImport:
        *result = false;
        return true;

      // Statements possibly containing hoistable declarations only in the left
      // half, in ParseNode terms -- the loop body in AST terms.
      case ParseNodeKind::DoWhile:
        return ContainsHoistedDeclaration(cx, node->as<BinaryNode>().left(), result);

      // Statements possibly containing hoistable declarations only in the
      // right half, in ParseNode terms -- the loop body or nested statement
      // (usually a block statement), in AST terms.
      case ParseNodeKind::While:
      case ParseNodeKind::With:
        return ContainsHoistedDeclaration(cx, node->as<BinaryNode>().right(), result);

      case ParseNodeKind::Label:
        return ContainsHoistedDeclaration(cx, node->as<NameNode>().expression(), result);

      // Statements with more complicated structures.

      // if-statement nodes may have hoisted declarations in their consequent
      // and alternative components.
      case ParseNodeKind::If: {
        TernaryNode* ifNode = &node->as<TernaryNode>();
        ParseNode* consequent = ifNode->kid2();
        if (!ContainsHoistedDeclaration(cx, consequent, result)) {
            return false;
        }
        if (*result) {
            return true;
        }

        if ((node = ifNode->kid3())) {
            goto restart;
        }

        *result = false;
        return true;
      }

      // try-statements have statements to execute, and one or both of a
      // catch-list and a finally-block.
      case ParseNodeKind::Try: {
        TernaryNode* tryNode = &node->as<TernaryNode>();

        MOZ_ASSERT(tryNode->kid2() || tryNode->kid3(),
                   "must have either catch or finally");

        ParseNode* tryBlock = tryNode->kid1();
        if (!ContainsHoistedDeclaration(cx, tryBlock, result)) {
            return false;
        }
        if (*result) {
            return true;
        }

        if (ParseNode* catchScope = tryNode->kid2()) {
            BinaryNode* catchNode =
                &catchScope->as<LexicalScopeNode>().scopeBody()->as<BinaryNode>();
            MOZ_ASSERT(catchNode->isKind(ParseNodeKind::Catch));

            ParseNode* catchStatements = catchNode->right();
            if (!ContainsHoistedDeclaration(cx, catchStatements, result)) {
                return false;
            }
            if (*result) {
                return true;
            }
        }

        if (ParseNode* finallyBlock = tryNode->kid3()) {
            return ContainsHoistedDeclaration(cx, finallyBlock, result);
        }

        *result = false;
        return true;
      }

      // A switch node's left half is an expression; only its right half (a
      // list of cases/defaults, or a block node) could contain hoisted
      // declarations.
      case ParseNodeKind::Switch: {
        SwitchStatement* switchNode = &node->as<SwitchStatement>();
        return ContainsHoistedDeclaration(cx, &switchNode->lexicalForCaseList(), result);
      }

      case ParseNodeKind::Case: {
        CaseClause* caseClause = &node->as<CaseClause>();
        return ContainsHoistedDeclaration(cx, caseClause->statementList(), result);
      }

      case ParseNodeKind::For: {
        ForNode* forNode = &node->as<ForNode>();
        TernaryNode* loopHead = forNode->head();
        MOZ_ASSERT(loopHead->isKind(ParseNodeKind::ForHead) ||
                   loopHead->isKind(ParseNodeKind::ForIn) ||
                   loopHead->isKind(ParseNodeKind::ForOf));

        if (loopHead->isKind(ParseNodeKind::ForHead)) {
            // for (init?; cond?; update?), with only init possibly containing
            // a hoisted declaration.  (Note: a lexical-declaration |init| is
            // (at present) hoisted in SpiderMonkey parlance -- but such
            // hoisting doesn't extend outside of this statement, so it is not
            // hoisting in the sense meant by ContainsHoistedDeclaration.)
            ParseNode* init = loopHead->kid1();
            if (init && init->isKind(ParseNodeKind::Var)) {
                *result = true;
                return true;
            }
        } else {
            MOZ_ASSERT(loopHead->isKind(ParseNodeKind::ForIn) ||
                       loopHead->isKind(ParseNodeKind::ForOf));

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
            ParseNode* decl = loopHead->kid1();
            if (decl && decl->isKind(ParseNodeKind::Var)) {
                *result = true;
                return true;
            }
        }

        ParseNode* loopBody = forNode->body();
        return ContainsHoistedDeclaration(cx, loopBody, result);
      }

      case ParseNodeKind::LexicalScope: {
        LexicalScopeNode* scope = &node->as<LexicalScopeNode>();
        ParseNode* expr = scope->scopeBody();

        if (expr->isKind(ParseNodeKind::For) || expr->isKind(ParseNodeKind::Function)) {
            return ContainsHoistedDeclaration(cx, expr, result);
        }

        MOZ_ASSERT(expr->isKind(ParseNodeKind::StatementList));
        return ListContainsHoistedDeclaration(cx, &scope->scopeBody()->as<ListNode>(), result);
      }

      // List nodes with all non-null children.
      case ParseNodeKind::StatementList:
        return ListContainsHoistedDeclaration(cx, &node->as<ListNode>(), result);

      // Grammar sub-components that should never be reached directly by this
      // method, because some parent component should have asserted itself.
      case ParseNodeKind::ObjectPropertyName:
      case ParseNodeKind::ComputedName:
      case ParseNodeKind::Spread:
      case ParseNodeKind::MutateProto:
      case ParseNodeKind::Colon:
      case ParseNodeKind::Shorthand:
      case ParseNodeKind::Conditional:
      case ParseNodeKind::TypeOfName:
      case ParseNodeKind::TypeOfExpr:
      case ParseNodeKind::Await:
      case ParseNodeKind::Void:
      case ParseNodeKind::Not:
      case ParseNodeKind::BitNot:
      case ParseNodeKind::DeleteName:
      case ParseNodeKind::DeleteProp:
      case ParseNodeKind::DeleteElem:
      case ParseNodeKind::DeleteExpr:
      case ParseNodeKind::Pos:
      case ParseNodeKind::Neg:
      case ParseNodeKind::PreIncrement:
      case ParseNodeKind::PostIncrement:
      case ParseNodeKind::PreDecrement:
      case ParseNodeKind::PostDecrement:
      case ParseNodeKind::Or:
      case ParseNodeKind::And:
      case ParseNodeKind::BitOr:
      case ParseNodeKind::BitXor:
      case ParseNodeKind::BitAnd:
      case ParseNodeKind::StrictEq:
      case ParseNodeKind::Eq:
      case ParseNodeKind::StrictNe:
      case ParseNodeKind::Ne:
      case ParseNodeKind::Lt:
      case ParseNodeKind::Le:
      case ParseNodeKind::Gt:
      case ParseNodeKind::Ge:
      case ParseNodeKind::InstanceOf:
      case ParseNodeKind::In:
      case ParseNodeKind::Lsh:
      case ParseNodeKind::Rsh:
      case ParseNodeKind::Ursh:
      case ParseNodeKind::Add:
      case ParseNodeKind::Sub:
      case ParseNodeKind::Star:
      case ParseNodeKind::Div:
      case ParseNodeKind::Mod:
      case ParseNodeKind::Pow:
      case ParseNodeKind::Assign:
      case ParseNodeKind::AddAssign:
      case ParseNodeKind::SubAssign:
      case ParseNodeKind::BitOrAssign:
      case ParseNodeKind::BitXorAssign:
      case ParseNodeKind::BitAndAssign:
      case ParseNodeKind::LshAssign:
      case ParseNodeKind::RshAssign:
      case ParseNodeKind::UrshAssign:
      case ParseNodeKind::MulAssign:
      case ParseNodeKind::DivAssign:
      case ParseNodeKind::ModAssign:
      case ParseNodeKind::PowAssign:
      case ParseNodeKind::Comma:
      case ParseNodeKind::Array:
      case ParseNodeKind::Object:
      case ParseNodeKind::PropertyName:
      case ParseNodeKind::Dot:
      case ParseNodeKind::Elem:
      case ParseNodeKind::Arguments:
      case ParseNodeKind::Call:
      case ParseNodeKind::Name:
      case ParseNodeKind::TemplateString:
      case ParseNodeKind::TemplateStringList:
      case ParseNodeKind::TaggedTemplate:
      case ParseNodeKind::CallSiteObj:
      case ParseNodeKind::String:
      case ParseNodeKind::RegExp:
      case ParseNodeKind::True:
      case ParseNodeKind::False:
      case ParseNodeKind::Null:
      case ParseNodeKind::RawUndefined:
      case ParseNodeKind::This:
      case ParseNodeKind::Elision:
      case ParseNodeKind::Number:
      case ParseNodeKind::New:
      case ParseNodeKind::Generator:
      case ParseNodeKind::ParamsBody:
      case ParseNodeKind::Catch:
      case ParseNodeKind::ForIn:
      case ParseNodeKind::ForOf:
      case ParseNodeKind::ForHead:
      case ParseNodeKind::ClassMethod:
      case ParseNodeKind::ClassMethodList:
      case ParseNodeKind::ClassNames:
      case ParseNodeKind::NewTarget:
      case ParseNodeKind::ImportMeta:
      case ParseNodeKind::PosHolder:
      case ParseNodeKind::SuperCall:
      case ParseNodeKind::SuperBase:
      case ParseNodeKind::SetThis:
        MOZ_CRASH("ContainsHoistedDeclaration should have indicated false on "
                  "some parent node without recurring to test this node");

      case ParseNodeKind::Pipeline:
        MOZ_ASSERT(node->is<ListNode>());
        *result = false;
        return true;

      case ParseNodeKind::Limit: // invalid sentinel value
        MOZ_CRASH("unexpected ParseNodeKind::Limit in node");
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
          case ParseNodeKind::Number:
            if (pn->isKind(ParseNodeKind::String)) {
                double d;
                if (!StringToNumber(cx, pn->as<NameNode>().atom(), &d)) {
                    return false;
                }
                pn->setKind(ParseNodeKind::Number);
                pn->setArity(PN_NUMBER);
                pn->setOp(JSOP_DOUBLE);
                pn->as<NumericLiteral>().setValue(d);
            }
            break;

          case ParseNodeKind::String:
            if (pn->isKind(ParseNodeKind::Number)) {
                JSAtom* atom = NumberToAtom(cx, pn->as<NumericLiteral>().value());
                if (!atom) {
                    return false;
                }
                pn->setKind(ParseNodeKind::String);
                pn->setArity(PN_NAME);
                pn->setOp(JSOP_STRING);
                pn->as<NameNode>().setAtom(atom);
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
    return node->isKind(ParseNodeKind::True) ||
           node->isKind(ParseNodeKind::False) ||
           node->isKind(ParseNodeKind::String) ||
           node->isKind(ParseNodeKind::TemplateString) ||
           node->isKind(ParseNodeKind::Number) ||
           node->isKind(ParseNodeKind::Null) ||
           node->isKind(ParseNodeKind::RawUndefined) ||
           node->isKind(ParseNodeKind::Function);
}

enum Truthiness { Truthy, Falsy, Unknown };

static Truthiness
Boolish(ParseNode* pn)
{
    switch (pn->getKind()) {
      case ParseNodeKind::Number:
        return (pn->as<NumericLiteral>().value() != 0 && !IsNaN(pn->as<NumericLiteral>().value())) ? Truthy : Falsy;

      case ParseNodeKind::String:
      case ParseNodeKind::TemplateString:
        return (pn->as<NameNode>().atom()->length() > 0) ? Truthy : Falsy;

      case ParseNodeKind::True:
      case ParseNodeKind::Function:
        return Truthy;

      case ParseNodeKind::False:
      case ParseNodeKind::Null:
      case ParseNodeKind::RawUndefined:
        return Falsy;

      case ParseNodeKind::Void: {
        // |void <foo>| evaluates to |undefined| which isn't truthy.  But the
        // sense of this method requires that the expression be literally
        // replaceable with true/false: not the case if the nested expression
        // is effectful, might throw, &c.  Walk past the |void| (and nested
        // |void| expressions, for good measure) and check that the nested
        // expression doesn't break this requirement before indicating falsity.
        do {
            pn = pn->as<UnaryNode>().kid();
        } while (pn->isKind(ParseNodeKind::Void));

        return IsEffectless(pn) ? Falsy : Unknown;
      }

      default:
        return Unknown;
    }
}

static bool
Fold(JSContext* cx, ParseNode** pnp, PerHandlerParser<FullParseHandler>& parser);

static bool
FoldCondition(JSContext* cx, ParseNode** nodePtr, PerHandlerParser<FullParseHandler>& parser)
{
    // Conditions fold like any other expression...
    if (!Fold(cx, nodePtr, parser)) {
        return false;
    }

    // ...but then they sometimes can be further folded to constants.
    ParseNode* node = *nodePtr;
    Truthiness t = Boolish(node);
    if (t != Unknown) {
        // We can turn function nodes into constant nodes here, but mutating
        // function nodes is tricky --- in particular, mutating a function node
        // that appears on a method list corrupts the method list. However,
        // methods are M's in statements of the form 'this.foo = M;', which we
        // never fold, so we're okay.
        if (t == Truthy) {
            node->setKind(ParseNodeKind::True);
            node->setOp(JSOP_TRUE);
        } else {
            node->setKind(ParseNodeKind::False);
            node->setOp(JSOP_FALSE);
        }
        node->setArity(PN_NULLARY);
    }

    return true;
}

static bool
FoldTypeOfExpr(JSContext* cx, UnaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::TypeOfExpr));

    if (!Fold(cx, node->unsafeKidReference(), parser)) {
        return false;
    }

    ParseNode* expr = node->kid();

    // Constant-fold the entire |typeof| if given a constant with known type.
    RootedPropertyName result(cx);
    if (expr->isKind(ParseNodeKind::String) || expr->isKind(ParseNodeKind::TemplateString)) {
        result = cx->names().string;
    } else if (expr->isKind(ParseNodeKind::Number)) {
        result = cx->names().number;
    } else if (expr->isKind(ParseNodeKind::Null)) {
        result = cx->names().object;
    } else if (expr->isKind(ParseNodeKind::True) || expr->isKind(ParseNodeKind::False)) {
        result = cx->names().boolean;
    } else if (expr->isKind(ParseNodeKind::Function)) {
        result = cx->names().function;
    }

    if (result) {
        node->setKind(ParseNodeKind::String);
        node->setArity(PN_NAME);
        node->setOp(JSOP_NOP);
        node->as<NameNode>().setAtom(result);
    }

    return true;
}

static bool
FoldDeleteExpr(JSContext* cx, UnaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::DeleteExpr));

    if (!Fold(cx, node->unsafeKidReference(), parser)) {
        return false;
    }

    ParseNode* expr = node->kid();

    // Expression deletion evaluates the expression, then evaluates to true.
    // For effectless expressions, eliminate the expression evaluation.
    if (IsEffectless(expr)) {
        node->setKind(ParseNodeKind::True);
        node->setArity(PN_NULLARY);
        node->setOp(JSOP_TRUE);
    }

    return true;
}

static bool
FoldDeleteElement(JSContext* cx, UnaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::DeleteElem));
    MOZ_ASSERT(node->kid()->isKind(ParseNodeKind::Elem));

    if (!Fold(cx, node->unsafeKidReference(), parser)) {
        return false;
    }

    ParseNode* expr = node->kid();

    // If we're deleting an element, but constant-folding converted our
    // element reference into a dotted property access, we must *also*
    // morph the node's kind.
    //
    // In principle this also applies to |super["foo"] -> super.foo|,
    // but we don't constant-fold |super["foo"]| yet.
    MOZ_ASSERT(expr->isKind(ParseNodeKind::Elem) || expr->isKind(ParseNodeKind::Dot));
    if (expr->isKind(ParseNodeKind::Dot)) {
        node->setKind(ParseNodeKind::DeleteProp);
    }

    return true;
}

static bool
FoldDeleteProperty(JSContext* cx, UnaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::DeleteProp));
    MOZ_ASSERT(node->kid()->isKind(ParseNodeKind::Dot));

#ifdef DEBUG
    ParseNodeKind oldKind = node->kid()->getKind();
#endif

    if (!Fold(cx, node->unsafeKidReference(), parser)) {
        return false;
    }

    MOZ_ASSERT(node->kid()->isKind(oldKind),
               "kind should have remained invariant under folding");

    return true;
}

static bool
FoldNot(JSContext* cx, UnaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Not));

    if (!FoldCondition(cx, node->unsafeKidReference(), parser)) {
        return false;
    }

    ParseNode* expr = node->kid();

    if (expr->isKind(ParseNodeKind::Number)) {
        double d = expr->as<NumericLiteral>().value();

        if (d == 0 || IsNaN(d)) {
            node->setKind(ParseNodeKind::True);
            node->setOp(JSOP_TRUE);
        } else {
            node->setKind(ParseNodeKind::False);
            node->setOp(JSOP_FALSE);
        }
        node->setArity(PN_NULLARY);
    } else if (expr->isKind(ParseNodeKind::True) || expr->isKind(ParseNodeKind::False)) {
        bool newval = !expr->isKind(ParseNodeKind::True);

        node->setKind(newval ? ParseNodeKind::True : ParseNodeKind::False);
        node->setArity(PN_NULLARY);
        node->setOp(newval ? JSOP_TRUE : JSOP_FALSE);
    }

    return true;
}

static bool
FoldUnaryArithmetic(JSContext* cx, UnaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::BitNot) ||
               node->isKind(ParseNodeKind::Pos) ||
               node->isKind(ParseNodeKind::Neg),
               "need a different method for this node kind");

    if (!Fold(cx, node->unsafeKidReference(), parser)) {
        return false;
    }

    ParseNode* expr = node->kid();

    if (expr->isKind(ParseNodeKind::Number) ||
        expr->isKind(ParseNodeKind::True) ||
        expr->isKind(ParseNodeKind::False))
    {
        double d = expr->isKind(ParseNodeKind::Number)
                   ? expr->as<NumericLiteral>().value()
                   : double(expr->isKind(ParseNodeKind::True));

        if (node->isKind(ParseNodeKind::BitNot)) {
            d = ~ToInt32(d);
        } else if (node->isKind(ParseNodeKind::Neg)) {
            d = -d;
        } else {
            MOZ_ASSERT(node->isKind(ParseNodeKind::Pos)); // nothing to do
        }

        node->setKind(ParseNodeKind::Number);
        node->setArity(PN_NUMBER);
        node->setOp(JSOP_DOUBLE);
        node->as<NumericLiteral>().setValue(d);
    }

    return true;
}

static bool
FoldIncrementDecrement(JSContext* cx, UnaryNode* incDec,
                       PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(incDec->isKind(ParseNodeKind::PreIncrement) ||
               incDec->isKind(ParseNodeKind::PostIncrement) ||
               incDec->isKind(ParseNodeKind::PreDecrement) ||
               incDec->isKind(ParseNodeKind::PostDecrement));

    MOZ_ASSERT(parser.isValidSimpleAssignmentTarget(incDec->kid(),
                                                    PermitAssignmentToFunctionCalls));

    if (!Fold(cx, incDec->unsafeKidReference(), parser)) {
        return false;
    }

    MOZ_ASSERT(parser.isValidSimpleAssignmentTarget(incDec->kid(),
                                                    PermitAssignmentToFunctionCalls));

    return true;
}

static bool
FoldAndOr(JSContext* cx, ParseNode** nodePtr, PerHandlerParser<FullParseHandler>& parser)
{
    ListNode* node = &(*nodePtr)->as<ListNode>();

    MOZ_ASSERT(node->isKind(ParseNodeKind::And) || node->isKind(ParseNodeKind::Or));

    bool isOrNode = node->isKind(ParseNodeKind::Or);
    ParseNode** elem = node->unsafeHeadReference();
    do {
        if (!Fold(cx, elem, parser)) {
            return false;
        }

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
            for (ParseNode* next = (*elem)->pn_next; next; next = next->pn_next) {
                node->unsafeDecrementCount();
            }

            // Terminate the original and/or list at the known-truthiness
            // node.
            (*elem)->pn_next = nullptr;
            elem = &(*elem)->pn_next;
            break;
        }

        MOZ_ASSERT((t == Truthy) == !isOrNode);

        // We've encountered a vacuous node that'll never short-circuit
        // evaluation.
        if ((*elem)->pn_next) {
            // This node is never the overall result when there are
            // subsequent nodes.  Remove it.
            ParseNode* elt = *elem;
            *elem = elt->pn_next;
            node->unsafeDecrementCount();
        } else {
            // Otherwise this node is the result of the overall expression,
            // so leave it alone.  And we're done.
            elem = &(*elem)->pn_next;
            break;
        }
    } while (*elem);

    node->unsafeReplaceTail(elem);

    // If we removed nodes, we may have to replace a one-element list with
    // its element.
    if (node->count() == 1) {
        ParseNode* first = node->head();
        ReplaceNode(nodePtr, first);
    }

    return true;
}

static bool
FoldConditional(JSContext* cx, ParseNode** nodePtr, PerHandlerParser<FullParseHandler>& parser)
{
    ParseNode** nextNode = nodePtr;

    do {
        // |nextNode| on entry points to the C?T:F expression to be folded.
        // Reset it to exit the loop in the common case where F isn't another
        // ?: expression.
        nodePtr = nextNode;
        nextNode = nullptr;

        TernaryNode* node = &(*nodePtr)->as<TernaryNode>();
        MOZ_ASSERT(node->isKind(ParseNodeKind::Conditional));

        ParseNode** expr = node->unsafeKid1Reference();
        if (!FoldCondition(cx, expr, parser)) {
            return false;
        }

        ParseNode** ifTruthy = node->unsafeKid2Reference();
        if (!Fold(cx, ifTruthy, parser)) {
            return false;
        }

        ParseNode** ifFalsy = node->unsafeKid3Reference();

        // If our C?T:F node has F as another ?: node, *iteratively* constant-
        // fold F *after* folding C and T (and possibly eliminating C and one
        // of T/F entirely); otherwise fold F normally.  Making |nextNode| non-
        // null causes this loop to run again to fold F.
        //
        // Conceivably we could instead/also iteratively constant-fold T, if T
        // were more complex than F.  Such an optimization is unimplemented.
        if ((*ifFalsy)->isKind(ParseNodeKind::Conditional)) {
            MOZ_ASSERT((*ifFalsy)->is<TernaryNode>());
            nextNode = ifFalsy;
        } else {
            if (!Fold(cx, ifFalsy, parser)) {
                return false;
            }
        }

        // Try to constant-fold based on the condition expression.
        Truthiness t = Boolish(*expr);
        if (t == Unknown) {
            continue;
        }

        // Otherwise reduce 'C ? T : F' to T or F as directed by C.
        ParseNode* replacement = t == Truthy ? *ifTruthy : *ifFalsy;

        // Otherwise perform a replacement.  This invalidates |nextNode|, so
        // reset it (if the replacement requires folding) or clear it (if
        // |ifFalsy| is dead code) as needed.
        if (nextNode) {
            nextNode = (*nextNode == replacement) ? nodePtr : nullptr;
        }
        ReplaceNode(nodePtr, replacement);
    } while (nextNode);

    return true;
}

static bool
FoldIf(JSContext* cx, ParseNode** nodePtr, PerHandlerParser<FullParseHandler>& parser)
{
    ParseNode** nextNode = nodePtr;

    do {
        // |nextNode| on entry points to the initial |if| to be folded.  Reset
        // it to exit the loop when the |else| arm isn't another |if|.
        nodePtr = nextNode;
        nextNode = nullptr;

        TernaryNode* node = &(*nodePtr)->as<TernaryNode>();
        MOZ_ASSERT(node->isKind(ParseNodeKind::If));

        ParseNode** expr = node->unsafeKid1Reference();
        if (!FoldCondition(cx, expr, parser)) {
            return false;
        }

        ParseNode** consequent = node->unsafeKid2Reference();
        if (!Fold(cx, consequent, parser)) {
            return false;
        }

        ParseNode** alternative = node->unsafeKid3Reference();
        if (*alternative) {
            // If in |if (C) T; else F;| we have |F| as another |if|,
            // *iteratively* constant-fold |F| *after* folding |C| and |T| (and
            // possibly completely replacing the whole thing with |T| or |F|);
            // otherwise fold F normally.  Making |nextNode| non-null causes
            // this loop to run again to fold F.
            if ((*alternative)->isKind(ParseNodeKind::If)) {
                MOZ_ASSERT((*alternative)->is<TernaryNode>());
                nextNode = alternative;
            } else {
                if (!Fold(cx, alternative, parser)) {
                    return false;
                }
            }
        }

        // Eliminate the consequent or alternative if the condition has
        // constant truthiness.
        Truthiness t = Boolish(*expr);
        if (t == Unknown) {
            continue;
        }

        // Careful!  Either of these can be null: |replacement| in |if (0) T;|,
        // and |discarded| in |if (true) T;|.
        ParseNode* replacement;
        ParseNode* discarded;
        if (t == Truthy) {
            replacement = *consequent;
            discarded = *alternative;
        } else {
            replacement = *alternative;
            discarded = *consequent;
        }

        bool performReplacement = true;
        if (discarded) {
            // A declaration that hoists outside the discarded arm prevents the
            // |if| from being folded away.
            bool containsHoistedDecls;
            if (!ContainsHoistedDeclaration(cx, discarded, &containsHoistedDecls)) {
                return false;
            }

            performReplacement = !containsHoistedDecls;
        }

        if (!performReplacement) {
            continue;
        }

        if (!replacement) {
            // If there's no replacement node, we have a constantly-false |if|
            // with no |else|.  Replace the entire thing with an empty
            // statement list.
            node->setKind(ParseNodeKind::StatementList);
            node->setArity(PN_LIST);
            node->as<ListNode>().makeEmpty();
        } else {
            // Replacement invalidates |nextNode|, so reset it (if the
            // replacement requires folding) or clear it (if |alternative|
            // is dead code) as needed.
            if (nextNode) {
                nextNode = (*nextNode == replacement) ? nodePtr : nullptr;
            }
            ReplaceNode(nodePtr, replacement);
        }
    } while (nextNode);

    return true;
}

static bool
FoldFunction(JSContext* cx, CodeNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Function));

    // Don't constant-fold inside "use asm" code, as this could create a parse
    // tree that doesn't type-check as asm.js.
    if (node->funbox()->useAsmOrInsideUseAsm()) {
        return true;
    }

    // Note: body is null for lazily-parsed functions.
    if (node->body()) {
        if (!Fold(cx, node->unsafeBodyReference(), parser)) {
            return false;
        }
    }

    return true;
}

static double
ComputeBinary(ParseNodeKind kind, double left, double right)
{
    if (kind == ParseNodeKind::Add) {
        return left + right;
    }

    if (kind == ParseNodeKind::Sub) {
        return left - right;
    }

    if (kind == ParseNodeKind::Star) {
        return left * right;
    }

    if (kind == ParseNodeKind::Mod) {
        return NumberMod(left, right);
    }

    if (kind == ParseNodeKind::Ursh) {
        return ToUint32(left) >> (ToUint32(right) & 31);
    }

    if (kind == ParseNodeKind::Div) {
        return NumberDiv(left, right);
    }

    MOZ_ASSERT(kind == ParseNodeKind::Lsh || kind == ParseNodeKind::Rsh);

    int32_t i = ToInt32(left);
    uint32_t j = ToUint32(right) & 31;
    return int32_t((kind == ParseNodeKind::Lsh) ? uint32_t(i) << j : i >> j);
}

static bool
FoldModule(JSContext* cx, CodeNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Module));

    MOZ_ASSERT(node->body());
    return Fold(cx, node->unsafeBodyReference(), parser);
}

static bool
FoldBinaryArithmetic(JSContext* cx, ListNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Sub) ||
               node->isKind(ParseNodeKind::Star) ||
               node->isKind(ParseNodeKind::Lsh) ||
               node->isKind(ParseNodeKind::Rsh) ||
               node->isKind(ParseNodeKind::Ursh) ||
               node->isKind(ParseNodeKind::Div) ||
               node->isKind(ParseNodeKind::Mod));
    MOZ_ASSERT(node->count() >= 2);

    // Fold each operand, ideally into a number.
    ParseNode** listp = node->unsafeHeadReference();
    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser)) {
            return false;
        }

        if (!FoldType(cx, *listp, ParseNodeKind::Number)) {
            return false;
        }
    }

    node->unsafeReplaceTail(listp);

    // Now fold all leading numeric terms together into a single number.
    // (Trailing terms for the non-shift operations can't be folded together
    // due to floating point imprecision.  For example, if |x === -2**53|,
    // |x - 1 - 1 === -2**53| but |x - 2 === -2**53 - 2|.  Shifts could be
    // folded, but it doesn't seem worth the effort.)
    ParseNode* elem = node->head();
    ParseNode* next = elem->pn_next;
    if (elem->isKind(ParseNodeKind::Number)) {
        ParseNodeKind kind = node->getKind();
        while (true) {
            if (!next || !next->isKind(ParseNodeKind::Number)) {
                break;
            }

            double d = ComputeBinary(kind, elem->as<NumericLiteral>().value(), next->as<NumericLiteral>().value());

            next = next->pn_next;
            elem->pn_next = next;

            elem->setKind(ParseNodeKind::Number);
            elem->setArity(PN_NUMBER);
            elem->setOp(JSOP_DOUBLE);
            elem->as<NumericLiteral>().setValue(d);

            node->unsafeDecrementCount();
        }

        if (node->count() == 1) {
            MOZ_ASSERT(node->head() == elem);
            MOZ_ASSERT(elem->isKind(ParseNodeKind::Number));

            double d = elem->as<NumericLiteral>().value();
            node->setKind(ParseNodeKind::Number);
            node->setArity(PN_NUMBER);
            node->setOp(JSOP_DOUBLE);
            node->as<NumericLiteral>().setValue(d);
        }
    }

    return true;
}

static bool
FoldExponentiation(JSContext* cx, ListNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Pow));
    MOZ_ASSERT(node->count() >= 2);

    // Fold each operand, ideally into a number.
    ParseNode** listp = node->unsafeHeadReference();
    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser)) {
            return false;
        }

        if (!FoldType(cx, *listp, ParseNodeKind::Number)) {
            return false;
        }
    }

    node->unsafeReplaceTail(listp);

    // Unlike all other binary arithmetic operators, ** is right-associative:
    // 2**3**5 is 2**(3**5), not (2**3)**5.  As list nodes singly-link their
    // children, full constant-folding requires either linear space or dodgy
    // in-place linked list reversal.  So we only fold one exponentiation: it's
    // easy and addresses common cases like |2**32|.
    if (node->count() > 2) {
        return true;
    }

    ParseNode* base = node->head();
    ParseNode* exponent = base->pn_next;
    if (!base->isKind(ParseNodeKind::Number) || !exponent->isKind(ParseNodeKind::Number)) {
        return true;
    }

    double d1 = base->as<NumericLiteral>().value();
    double d2 = exponent->as<NumericLiteral>().value();

    node->setKind(ParseNodeKind::Number);
    node->setArity(PN_NUMBER);
    node->setOp(JSOP_DOUBLE);
    node->as<NumericLiteral>().setValue(ecmaPow(d1, d2));
    return true;
}

static bool
FoldList(JSContext* cx, ListNode* list, PerHandlerParser<FullParseHandler>& parser)
{
    ParseNode** elem = list->unsafeHeadReference();
    for (; *elem; elem = &(*elem)->pn_next) {
        if (!Fold(cx, elem, parser)) {
            return false;
        }
    }

    list->unsafeReplaceTail(elem);

    return true;
}

static bool
FoldReturn(JSContext* cx, UnaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Return));

    if (node->kid()) {
        if (!Fold(cx, node->unsafeKidReference(), parser)) {
            return false;
        }
    }

    return true;
}

static bool
FoldTry(JSContext* cx, TryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    ParseNode** statements = node->unsafeKid1Reference();
    if (!Fold(cx, statements, parser)) {
        return false;
    }

    ParseNode** catchScope = node->unsafeKid2Reference();
    if (*catchScope) {
        if (!Fold(cx, catchScope, parser)) {
            return false;
        }
    }

    ParseNode** finally = node->unsafeKid3Reference();
    if (*finally) {
        if (!Fold(cx, finally, parser)) {
            return false;
        }
    }

    return true;
}

static bool
FoldCatch(JSContext* cx, BinaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    ParseNode** declPattern = node->unsafeLeftReference();
    if (*declPattern) {
        if (!Fold(cx, declPattern, parser)) {
            return false;
        }
    }

    ParseNode** statements = node->unsafeRightReference();
    if (*statements) {
        if (!Fold(cx, statements, parser)) {
            return false;
        }
    }

    return true;
}

static bool
FoldClass(JSContext* cx, ClassNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Class));

    ParseNode** classNames = node->unsafeKid1Reference();
    if (*classNames) {
        if (!Fold(cx, classNames, parser)) {
            return false;
        }
    }

    ParseNode** heritage = node->unsafeKid2Reference();
    if (*heritage) {
        if (!Fold(cx, heritage, parser)) {
            return false;
        }
    }

    ParseNode** body = node->unsafeKid3Reference();
    return Fold(cx, body, parser);
}

static bool
FoldElement(JSContext* cx, ParseNode** nodePtr, PerHandlerParser<FullParseHandler>& parser)
{
    PropertyByValue* elem = &(*nodePtr)->as<PropertyByValue>();

    if (!Fold(cx, elem->unsafeLeftReference(), parser)) {
        return false;
    }

    if (!Fold(cx, elem->unsafeRightReference(), parser)) {
        return false;
    }

    ParseNode* expr = &elem->expression();
    ParseNode* key = &elem->key();
    PropertyName* name = nullptr;
    if (key->isKind(ParseNodeKind::String)) {
        JSAtom* atom = key->as<NameNode>().atom();
        uint32_t index;

        if (atom->isIndex(&index)) {
            // Optimization 1: We have something like expr["100"]. This is
            // equivalent to expr[100] which is faster.
            key->setKind(ParseNodeKind::Number);
            key->setArity(PN_NUMBER);
            key->setOp(JSOP_DOUBLE);
            key->as<NumericLiteral>().setValue(index);
        } else {
            name = atom->asPropertyName();
        }
    } else if (key->isKind(ParseNodeKind::Number)) {
        double number = key->as<NumericLiteral>().value();
        if (number != ToUint32(number)) {
            // Optimization 2: We have something like expr[3.14]. The number
            // isn't an array index, so it converts to a string ("3.14"),
            // enabling optimization 3 below.
            JSAtom* atom = NumberToAtom(cx, number);
            if (!atom) {
                return false;
            }
            name = atom->asPropertyName();
        }
    }

    // If we don't have a name, we can't optimize to getprop.
    if (!name) {
        return true;
    }

    // Optimization 3: We have expr["foo"] where foo is not an index.  Convert
    // to a property access (like expr.foo) that optimizes better downstream.
    NameNode* nameNode = parser.newPropertyName(name, key->pn_pos);
    if (!nameNode) {
        return false;
    }
    ParseNode* dottedAccess = parser.newPropertyAccess(expr, nameNode);
    if (!dottedAccess) {
        return false;
    }
    dottedAccess->setInParens(elem->isInParens());
    ReplaceNode(nodePtr, dottedAccess);

    return true;
}

static bool
FoldAdd(JSContext* cx, ParseNode** nodePtr, PerHandlerParser<FullParseHandler>& parser)
{
    ListNode* node = &(*nodePtr)->as<ListNode>();

    MOZ_ASSERT(node->isKind(ParseNodeKind::Add));
    MOZ_ASSERT(node->count() >= 2);

    // Generically fold all operands first.
    if (!FoldList(cx, node, parser)) {
        return false;
    }

    // Fold leading numeric operands together:
    //
    //   (1 + 2 + x)  becomes  (3 + x)
    //
    // Don't go past the leading operands: additions after a string are
    // string concatenations, not additions: ("1" + 2 + 3 === "123").
    ParseNode* current = node->head();
    ParseNode* next = current->pn_next;
    if (current->isKind(ParseNodeKind::Number)) {
        do {
            if (!next->isKind(ParseNodeKind::Number)) {
                break;
            }

            NumericLiteral* num = &current->as<NumericLiteral>();

            num->setValue(num->value() + next->as<NumericLiteral>().value());
            current->pn_next = next->pn_next;
            next = current->pn_next;

            node->unsafeDecrementCount();
        } while (next);
    }

    // If any operands remain, attempt string concatenation folding.
    do {
        // If no operands remain, we're done.
        if (!next) {
            break;
        }

        // (number + string) is string concatenation *only* at the start of
        // the list: (x + 1 + "2" !== x + "12") when x is a number.
        if (current->isKind(ParseNodeKind::Number) && next->isKind(ParseNodeKind::String)) {
            if (!FoldType(cx, current, ParseNodeKind::String)) {
                return false;
            }
            next = current->pn_next;
        }

        // The first string forces all subsequent additions to be
        // string concatenations.
        do {
            if (current->isKind(ParseNodeKind::String)) {
                break;
            }

            current = next;
            next = next->pn_next;
        } while (next);

        // If there's nothing left to fold, we're done.
        if (!next) {
            break;
        }

        RootedString combination(cx);
        RootedString tmp(cx);
        do {
            // Create a rope of the current string and all succeeding
            // constants that we can convert to strings, then atomize it
            // and replace them all with that fresh string.
            MOZ_ASSERT(current->isKind(ParseNodeKind::String));

            combination = current->as<NameNode>().atom();

            do {
                // Try folding the next operand to a string.
                if (!FoldType(cx, next, ParseNodeKind::String)) {
                    return false;
                }

                // Stop glomming once folding doesn't produce a string.
                if (!next->isKind(ParseNodeKind::String)) {
                    break;
                }

                // Add this string to the combination and remove the node.
                tmp = next->as<NameNode>().atom();
                combination = ConcatStrings<CanGC>(cx, combination, tmp);
                if (!combination) {
                    return false;
                }

                next = next->pn_next;
                current->pn_next = next;

                node->unsafeDecrementCount();
            } while (next);

            // Replace |current|'s string with the entire combination.
            MOZ_ASSERT(current->isKind(ParseNodeKind::String));
            combination = AtomizeString(cx, combination);
            if (!combination) {
                return false;
            }
            current->as<NameNode>().setAtom(&combination->asAtom());


            // If we're out of nodes, we're done.
            if (!next) {
                break;
            }

            current = next;
            next = current->pn_next;

            // If we're out of nodes *after* the non-foldable-to-string
            // node, we're done.
            if (!next) {
                break;
            }

            // Otherwise find the next node foldable to a string, and loop.
            do {
                current = next;
                next = current->pn_next;

                if (!FoldType(cx, current, ParseNodeKind::String)) {
                    return false;
                }
                next = current->pn_next;
            } while (!current->isKind(ParseNodeKind::String) && next);
        } while (next);
    } while (false);

    MOZ_ASSERT(!next, "must have considered all nodes here");
    MOZ_ASSERT(!current->pn_next, "current node must be the last node");

    node->unsafeReplaceTail(&current->pn_next);

    if (node->count() == 1) {
        // We reduced the list to a constant.  Replace the ParseNodeKind::Add node
        // with that constant.
        ReplaceNode(nodePtr, current);
    }

    return true;
}

static bool
FoldCall(JSContext* cx, BinaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Call) ||
               node->isKind(ParseNodeKind::SuperCall) ||
               node->isKind(ParseNodeKind::New) ||
               node->isKind(ParseNodeKind::TaggedTemplate));

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
    ParseNode* callee = node->left();
    if (node->isKind(ParseNodeKind::New) || !callee->isInParens()) {
        if (!Fold(cx, node->unsafeLeftReference(), parser)) {
            return false;
        }
    }

    if (!Fold(cx, node->unsafeRightReference(), parser)) {
        return false;
    }

    return true;
}

static bool
FoldArguments(JSContext* cx, ListNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Arguments));

    ParseNode** listp = node->unsafeHeadReference();
    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser)) {
            return false;
        }
    }

    node->unsafeReplaceTail(listp);
    return true;
}

static bool
FoldForInOrOf(JSContext* cx, TernaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::ForIn) ||
               node->isKind(ParseNodeKind::ForOf));
    MOZ_ASSERT(!node->kid2());

    return Fold(cx, node->unsafeKid1Reference(), parser) &&
           Fold(cx, node->unsafeKid3Reference(), parser);
}

static bool
FoldForHead(JSContext* cx, TernaryNode* node, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::ForHead));

    ParseNode** init = node->unsafeKid1Reference();
    if (*init) {
        if (!Fold(cx, init, parser)) {
            return false;
        }
    }

    ParseNode** test = node->unsafeKid2Reference();
    if (*test) {
        if (!FoldCondition(cx, test, parser)) {
            return false;
        }

        if ((*test)->isKind(ParseNodeKind::True)) {
            (*test) = nullptr;
        }
    }

    ParseNode** update = node->unsafeKid3Reference();
    if (*update) {
        if (!Fold(cx, update, parser)) {
            return false;
        }
    }

    return true;
}

static bool
FoldDottedProperty(JSContext* cx, PropertyAccess* prop, PerHandlerParser<FullParseHandler>& parser)
{
    // Iterate through a long chain of dotted property accesses to find the
    // most-nested non-dotted property node, then fold that.
    ParseNode** nested = prop->unsafeLeftReference();
    while ((*nested)->isKind(ParseNodeKind::Dot)) {
        nested = (*nested)->as<PropertyAccess>().unsafeLeftReference();
    }

    return Fold(cx, nested, parser);
}

static bool
FoldName(JSContext* cx, NameNode* nameNode, PerHandlerParser<FullParseHandler>& parser)
{
    MOZ_ASSERT(nameNode->isKind(ParseNodeKind::Name));

    if (!nameNode->expression()) {
        return true;
    }

    return Fold(cx, nameNode->unsafeExpressionReference(), parser);
}

bool
Fold(JSContext* cx, ParseNode** pnp, PerHandlerParser<FullParseHandler>& parser)
{
    if (!CheckRecursionLimit(cx)) {
        return false;
    }

    ParseNode* pn = *pnp;

    switch (pn->getKind()) {
      case ParseNodeKind::EmptyStatement:
      case ParseNodeKind::True:
      case ParseNodeKind::False:
      case ParseNodeKind::Null:
      case ParseNodeKind::RawUndefined:
      case ParseNodeKind::Elision:
      case ParseNodeKind::Generator:
      case ParseNodeKind::ExportBatchSpec:
      case ParseNodeKind::PosHolder:
        MOZ_ASSERT(pn->is<NullaryNode>());
        return true;

      case ParseNodeKind::Debugger:
        MOZ_ASSERT(pn->is<DebuggerStatement>());
        return true;

      case ParseNodeKind::Break:
        MOZ_ASSERT(pn->is<BreakStatement>());
        return true;

      case ParseNodeKind::Continue:
        MOZ_ASSERT(pn->is<ContinueStatement>());
        return true;

      case ParseNodeKind::ObjectPropertyName:
      case ParseNodeKind::String:
      case ParseNodeKind::TemplateString:
        MOZ_ASSERT(pn->is<NameNode>());
        return true;

      case ParseNodeKind::RegExp:
        MOZ_ASSERT(pn->is<RegExpLiteral>());
        return true;

      case ParseNodeKind::Number:
        MOZ_ASSERT(pn->is<NumericLiteral>());
        return true;

      case ParseNodeKind::SuperBase:
      case ParseNodeKind::TypeOfName: {
#ifdef DEBUG
        UnaryNode* node = &pn->as<UnaryNode>();
        NameNode* nameNode = &node->kid()->as<NameNode>();
        MOZ_ASSERT(nameNode->isKind(ParseNodeKind::Name));
        MOZ_ASSERT(!nameNode->expression());
#endif
        return true;
      }

      case ParseNodeKind::TypeOfExpr:
        return FoldTypeOfExpr(cx, &pn->as<UnaryNode>(), parser);

      case ParseNodeKind::DeleteName:
        MOZ_ASSERT(pn->as<UnaryNode>().kid()->isKind(ParseNodeKind::Name));
        return true;

      case ParseNodeKind::DeleteExpr:
        return FoldDeleteExpr(cx, &pn->as<UnaryNode>(), parser);

      case ParseNodeKind::DeleteElem:
        return FoldDeleteElement(cx, &pn->as<UnaryNode>(), parser);

      case ParseNodeKind::DeleteProp:
        return FoldDeleteProperty(cx, &pn->as<UnaryNode>(), parser);

      case ParseNodeKind::Conditional:
        MOZ_ASSERT((*pnp)->is<TernaryNode>());
        return FoldConditional(cx, pnp, parser);

      case ParseNodeKind::If:
        MOZ_ASSERT((*pnp)->is<TernaryNode>());
        return FoldIf(cx, pnp, parser);

      case ParseNodeKind::Not:
        return FoldNot(cx, &pn->as<UnaryNode>(), parser);

      case ParseNodeKind::BitNot:
      case ParseNodeKind::Pos:
      case ParseNodeKind::Neg:
        return FoldUnaryArithmetic(cx, &pn->as<UnaryNode>(), parser);

      case ParseNodeKind::PreIncrement:
      case ParseNodeKind::PostIncrement:
      case ParseNodeKind::PreDecrement:
      case ParseNodeKind::PostDecrement:
        return FoldIncrementDecrement(cx, &pn->as<UnaryNode>(), parser);

      case ParseNodeKind::ExpressionStatement:
      case ParseNodeKind::Throw:
      case ParseNodeKind::MutateProto:
      case ParseNodeKind::ComputedName:
      case ParseNodeKind::Spread:
      case ParseNodeKind::Export:
      case ParseNodeKind::Void:
        return Fold(cx, pn->as<UnaryNode>().unsafeKidReference(), parser);

      case ParseNodeKind::ExportDefault:
        return Fold(cx, pn->as<BinaryNode>().unsafeLeftReference(), parser);

      case ParseNodeKind::This: {
        ThisLiteral* node = &pn->as<ThisLiteral>();
        ParseNode** expr = node->unsafeKidReference();
        if (*expr) {
            return Fold(cx, expr, parser);
        }
        return true;
      }

      case ParseNodeKind::Pipeline:
        return true;

      case ParseNodeKind::And:
      case ParseNodeKind::Or:
        MOZ_ASSERT((*pnp)->is<ListNode>());
        return FoldAndOr(cx, pnp, parser);

      case ParseNodeKind::Function:
        return FoldFunction(cx, &pn->as<CodeNode>(), parser);

      case ParseNodeKind::Module:
        return FoldModule(cx, &pn->as<CodeNode>(), parser);

      case ParseNodeKind::Sub:
      case ParseNodeKind::Star:
      case ParseNodeKind::Lsh:
      case ParseNodeKind::Rsh:
      case ParseNodeKind::Ursh:
      case ParseNodeKind::Div:
      case ParseNodeKind::Mod:
        return FoldBinaryArithmetic(cx, &pn->as<ListNode>(), parser);

      case ParseNodeKind::Pow:
        return FoldExponentiation(cx, &pn->as<ListNode>(), parser);

      // Various list nodes not requiring care to minimally fold.  Some of
      // these could be further folded/optimized, but we don't make the effort.
      case ParseNodeKind::BitOr:
      case ParseNodeKind::BitXor:
      case ParseNodeKind::BitAnd:
      case ParseNodeKind::StrictEq:
      case ParseNodeKind::Eq:
      case ParseNodeKind::StrictNe:
      case ParseNodeKind::Ne:
      case ParseNodeKind::Lt:
      case ParseNodeKind::Le:
      case ParseNodeKind::Gt:
      case ParseNodeKind::Ge:
      case ParseNodeKind::InstanceOf:
      case ParseNodeKind::In:
      case ParseNodeKind::Comma:
      case ParseNodeKind::Array:
      case ParseNodeKind::Object:
      case ParseNodeKind::StatementList:
      case ParseNodeKind::ClassMethodList:
      case ParseNodeKind::TemplateStringList:
      case ParseNodeKind::Var:
      case ParseNodeKind::Const:
      case ParseNodeKind::Let:
      case ParseNodeKind::ParamsBody:
      case ParseNodeKind::CallSiteObj:
      case ParseNodeKind::ExportSpecList:
      case ParseNodeKind::ImportSpecList:
        return FoldList(cx, &pn->as<ListNode>(), parser);

      case ParseNodeKind::InitialYield: {
#ifdef DEBUG
        AssignmentNode* assignNode = &pn->as<UnaryNode>().kid()->as<AssignmentNode>();
        MOZ_ASSERT(assignNode->left()->isKind(ParseNodeKind::Name));
        MOZ_ASSERT(assignNode->right()->isKind(ParseNodeKind::Generator));
#endif
        return true;
      }

      case ParseNodeKind::YieldStar:
        return Fold(cx, pn->as<UnaryNode>().unsafeKidReference(), parser);

      case ParseNodeKind::Yield:
      case ParseNodeKind::Await: {
        UnaryNode* node = &pn->as<UnaryNode>();
        if (!node->kid()) {
            return true;
        }
        return Fold(cx, node->unsafeKidReference(), parser);
      }

      case ParseNodeKind::Return:
        return FoldReturn(cx, &pn->as<UnaryNode>(), parser);

      case ParseNodeKind::Try:
        return FoldTry(cx, &pn->as<TryNode>(), parser);

      case ParseNodeKind::Catch:
        return FoldCatch(cx, &pn->as<BinaryNode>(), parser);

      case ParseNodeKind::Class:
        return FoldClass(cx, &pn->as<ClassNode>(), parser);

      case ParseNodeKind::Elem: {
        MOZ_ASSERT((*pnp)->is<PropertyByValue>());
        return FoldElement(cx, pnp, parser);
      }

      case ParseNodeKind::Add:
        MOZ_ASSERT((*pnp)->is<ListNode>());
        return FoldAdd(cx, pnp, parser);

      case ParseNodeKind::Call:
      case ParseNodeKind::New:
      case ParseNodeKind::SuperCall:
      case ParseNodeKind::TaggedTemplate:
        return FoldCall(cx, &pn->as<BinaryNode>(), parser);

      case ParseNodeKind::Arguments:
        return FoldArguments(cx, &pn->as<ListNode>(), parser);

      case ParseNodeKind::Switch:
      case ParseNodeKind::Colon:
      case ParseNodeKind::Assign:
      case ParseNodeKind::AddAssign:
      case ParseNodeKind::SubAssign:
      case ParseNodeKind::BitOrAssign:
      case ParseNodeKind::BitAndAssign:
      case ParseNodeKind::BitXorAssign:
      case ParseNodeKind::LshAssign:
      case ParseNodeKind::RshAssign:
      case ParseNodeKind::UrshAssign:
      case ParseNodeKind::DivAssign:
      case ParseNodeKind::ModAssign:
      case ParseNodeKind::MulAssign:
      case ParseNodeKind::PowAssign:
      case ParseNodeKind::Import:
      case ParseNodeKind::ExportFrom:
      case ParseNodeKind::Shorthand:
      case ParseNodeKind::For:
      case ParseNodeKind::ClassMethod:
      case ParseNodeKind::ImportSpec:
      case ParseNodeKind::ExportSpec:
      case ParseNodeKind::SetThis: {
        BinaryNode* node = &pn->as<BinaryNode>();
        return Fold(cx, node->unsafeLeftReference(), parser) &&
               Fold(cx, node->unsafeRightReference(), parser);
      }

      case ParseNodeKind::NewTarget:
      case ParseNodeKind::ImportMeta: {
#ifdef DEBUG
        BinaryNode* node = &pn->as<BinaryNode>();
        MOZ_ASSERT(node->left()->isKind(ParseNodeKind::PosHolder));
        MOZ_ASSERT(node->right()->isKind(ParseNodeKind::PosHolder));
#endif
        return true;
      }

      case ParseNodeKind::CallImport: {
        BinaryNode* node = &pn->as<BinaryNode>();
        MOZ_ASSERT(node->left()->isKind(ParseNodeKind::PosHolder));
        return Fold(cx, node->unsafeRightReference(), parser);
      }

      case ParseNodeKind::ClassNames: {
        ClassNames* names = &pn->as<ClassNames>();
        if (names->outerBinding()) {
            if (!Fold(cx, names->unsafeLeftReference(), parser)) {
                return false;
            }
        }
        return Fold(cx, names->unsafeRightReference(), parser);
      }

      case ParseNodeKind::DoWhile: {
        BinaryNode* node = &pn->as<BinaryNode>();
        return Fold(cx, node->unsafeLeftReference(), parser) &&
               FoldCondition(cx, node->unsafeRightReference(), parser);
      }

      case ParseNodeKind::While: {
        BinaryNode* node = &pn->as<BinaryNode>();
        return FoldCondition(cx, node->unsafeLeftReference(), parser) &&
               Fold(cx, node->unsafeRightReference(), parser);
      }

      case ParseNodeKind::Case: {
        CaseClause* caseClause = &pn->as<CaseClause>();

        // left (caseExpression) is null for DefaultClauses.
        if (caseClause->left()) {
            if (!Fold(cx, caseClause->unsafeLeftReference(), parser)) {
                return false;
            }
        }
        return Fold(cx, caseClause->unsafeRightReference(), parser);
      }

      case ParseNodeKind::With: {
        BinaryNode* node = &pn->as<BinaryNode>();
        return Fold(cx, node->unsafeLeftReference(), parser) &&
               Fold(cx, node->unsafeRightReference(), parser);
      }

      case ParseNodeKind::ForIn:
      case ParseNodeKind::ForOf:
        return FoldForInOrOf(cx, &pn->as<TernaryNode>(), parser);

      case ParseNodeKind::ForHead:
        return FoldForHead(cx, &pn->as<TernaryNode>(), parser);

      case ParseNodeKind::Label:
        return Fold(cx, pn->as<NameNode>().unsafeExpressionReference(), parser);

      case ParseNodeKind::PropertyName:
        MOZ_CRASH("unreachable, handled by ::Dot");

      case ParseNodeKind::Dot:
        return FoldDottedProperty(cx, &pn->as<PropertyAccess>(), parser);

      case ParseNodeKind::LexicalScope: {
        LexicalScopeNode* node = &pn->as<LexicalScopeNode>();
        if (!node->scopeBody()) {
            return true;
        }
        return Fold(cx, node->unsafeScopeBodyReference(), parser);
      }

      case ParseNodeKind::Name:
        return FoldName(cx, &pn->as<NameNode>(), parser);

      case ParseNodeKind::Limit: // invalid sentinel value
        MOZ_CRASH("invalid node kind");
    }

    MOZ_CRASH("shouldn't reach here");
    return false;
}

bool
frontend::FoldConstants(JSContext* cx, ParseNode** pnp, PerHandlerParser<FullParseHandler>* parser)
{
    // Don't constant-fold inside "use asm" code, as this could create a parse
    // tree that doesn't type-check as asm.js.
    if (parser->pc->useAsmOrInsideUseAsm()) {
        return true;
    }

    AutoTraceLog traceLog(TraceLoggerForCurrentThread(cx), TraceLogger_BytecodeFoldConstants);
    return Fold(cx, pnp, *parser);
}

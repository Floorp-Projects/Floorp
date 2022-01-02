/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ExprParser
 * This class is used to parse XSL Expressions
 * @see ExprLexer
 **/

#include "txExprParser.h"

#include <utility>

#include "mozilla/UniquePtrExtensions.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "txExpr.h"
#include "txExprLexer.h"
#include "txIXPathContext.h"
#include "txStack.h"
#include "txStringUtils.h"
#include "txXPathNode.h"
#include "txXPathOptimizer.h"

using mozilla::MakeUnique;
using mozilla::UniquePtr;
using mozilla::Unused;
using mozilla::WrapUnique;

/**
 * Creates an Attribute Value Template using the given value
 * This should move to XSLProcessor class
 */
nsresult txExprParser::createAVT(const nsAString& aAttrValue,
                                 txIParseContext* aContext, Expr** aResult) {
  *aResult = nullptr;
  nsresult rv = NS_OK;
  UniquePtr<Expr> expr;
  FunctionCall* concat = nullptr;

  nsAutoString literalString;
  bool inExpr = false;
  nsAString::const_char_iterator iter, start, end, avtStart;
  aAttrValue.BeginReading(iter);
  aAttrValue.EndReading(end);
  avtStart = iter;

  while (iter != end) {
    // Every iteration through this loop parses either a literal section
    // or an expression
    start = iter;
    UniquePtr<Expr> newExpr;
    if (!inExpr) {
      // Parse literal section
      literalString.Truncate();
      while (iter != end) {
        char16_t q = *iter;
        if (q == '{' || q == '}') {
          // Store what we've found so far and set a new |start| to
          // skip the (first) brace
          literalString.Append(Substring(start, iter));
          start = ++iter;
          // Unless another brace follows we've found the start of
          // an expression (in case of '{') or an unbalanced brace
          // (in case of '}')
          if (iter == end || *iter != q) {
            if (q == '}') {
              aContext->SetErrorOffset(iter - avtStart);
              return NS_ERROR_XPATH_UNBALANCED_CURLY_BRACE;
            }

            inExpr = true;
            break;
          }
          // We found a second brace, let that be part of the next
          // literal section being parsed and continue looping
        }
        ++iter;
      }

      if (start == iter && literalString.IsEmpty()) {
        // Restart the loop since we didn't create an expression
        continue;
      }
      newExpr =
          MakeUnique<txLiteralExpr>(literalString + Substring(start, iter));
    } else {
      // Parse expressions, iter is already past the initial '{' when
      // we get here.
      while (iter != end) {
        if (*iter == '}') {
          rv = createExprInternal(Substring(start, iter), start - avtStart,
                                  aContext, getter_Transfers(newExpr));
          NS_ENSURE_SUCCESS(rv, rv);

          inExpr = false;
          ++iter;  // skip closing '}'
          break;
        } else if (*iter == '\'' || *iter == '"') {
          char16_t q = *iter;
          while (++iter != end && *iter != q) {
          } /* do nothing */
          if (iter == end) {
            break;
          }
        }
        ++iter;
      }

      if (inExpr) {
        aContext->SetErrorOffset(start - avtStart);
        return NS_ERROR_XPATH_UNBALANCED_CURLY_BRACE;
      }
    }

    // Add expression, create a concat() call if necessary
    if (!expr) {
      expr = std::move(newExpr);
    } else {
      if (!concat) {
        concat = new txCoreFunctionCall(txCoreFunctionCall::CONCAT);
        concat->addParam(expr.release());
        expr = WrapUnique(concat);
      }

      concat->addParam(newExpr.release());
    }
  }

  if (inExpr) {
    aContext->SetErrorOffset(iter - avtStart);
    return NS_ERROR_XPATH_UNBALANCED_CURLY_BRACE;
  }

  if (!expr) {
    expr = MakeUnique<txLiteralExpr>(u""_ns);
  }

  *aResult = expr.release();

  return NS_OK;
}

nsresult txExprParser::createExprInternal(const nsAString& aExpression,
                                          uint32_t aSubStringPos,
                                          txIParseContext* aContext,
                                          Expr** aExpr) {
  NS_ENSURE_ARG_POINTER(aExpr);
  *aExpr = nullptr;
  txExprLexer lexer;
  nsresult rv = lexer.parse(aExpression);
  if (NS_FAILED(rv)) {
    nsAString::const_char_iterator start;
    aExpression.BeginReading(start);
    aContext->SetErrorOffset(lexer.mPosition - start + aSubStringPos);
    return rv;
  }
  UniquePtr<Expr> expr;
  rv = createExpr(lexer, aContext, getter_Transfers(expr));
  if (NS_SUCCEEDED(rv) && lexer.peek()->mType != Token::END) {
    rv = NS_ERROR_XPATH_BINARY_EXPECTED;
  }
  if (NS_FAILED(rv)) {
    nsAString::const_char_iterator start;
    aExpression.BeginReading(start);
    aContext->SetErrorOffset(lexer.peek()->mStart - start + aSubStringPos);

    return rv;
  }

  txXPathOptimizer optimizer;
  Expr* newExpr = nullptr;
  optimizer.optimize(expr.get(), &newExpr);

  *aExpr = newExpr ? newExpr : expr.release();

  return NS_OK;
}

/**
 * Private Methods
 */

/**
 * Creates a binary Expr for the given operator
 */
nsresult txExprParser::createBinaryExpr(UniquePtr<Expr>& left,
                                        UniquePtr<Expr>& right, Token* op,
                                        Expr** aResult) {
  NS_ASSERTION(op, "internal error");
  *aResult = nullptr;

  Expr* expr = nullptr;
  switch (op->mType) {
    //-- math ops
    case Token::ADDITION_OP:
      expr = new txNumberExpr(left.get(), right.get(), txNumberExpr::ADD);
      break;
    case Token::SUBTRACTION_OP:
      expr = new txNumberExpr(left.get(), right.get(), txNumberExpr::SUBTRACT);
      break;
    case Token::DIVIDE_OP:
      expr = new txNumberExpr(left.get(), right.get(), txNumberExpr::DIVIDE);
      break;
    case Token::MODULUS_OP:
      expr = new txNumberExpr(left.get(), right.get(), txNumberExpr::MODULUS);
      break;
    case Token::MULTIPLY_OP:
      expr = new txNumberExpr(left.get(), right.get(), txNumberExpr::MULTIPLY);
      break;

    //-- case boolean ops
    case Token::AND_OP:
      expr = new BooleanExpr(left.get(), right.get(), BooleanExpr::AND);
      break;
    case Token::OR_OP:
      expr = new BooleanExpr(left.get(), right.get(), BooleanExpr::OR);
      break;

    //-- equality ops
    case Token::EQUAL_OP:
      expr = new RelationalExpr(left.get(), right.get(), RelationalExpr::EQUAL);
      break;
    case Token::NOT_EQUAL_OP:
      expr = new RelationalExpr(left.get(), right.get(),
                                RelationalExpr::NOT_EQUAL);
      break;

    //-- relational ops
    case Token::LESS_THAN_OP:
      expr = new RelationalExpr(left.get(), right.get(),
                                RelationalExpr::LESS_THAN);
      break;
    case Token::GREATER_THAN_OP:
      expr = new RelationalExpr(left.get(), right.get(),
                                RelationalExpr::GREATER_THAN);
      break;
    case Token::LESS_OR_EQUAL_OP:
      expr = new RelationalExpr(left.get(), right.get(),
                                RelationalExpr::LESS_OR_EQUAL);
      break;
    case Token::GREATER_OR_EQUAL_OP:
      expr = new RelationalExpr(left.get(), right.get(),
                                RelationalExpr::GREATER_OR_EQUAL);
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("operator tokens should be already checked");
      return NS_ERROR_UNEXPECTED;
  }

  Unused << left.release();
  Unused << right.release();

  *aResult = expr;
  return NS_OK;
}

nsresult txExprParser::createExpr(txExprLexer& lexer, txIParseContext* aContext,
                                  Expr** aResult) {
  *aResult = nullptr;

  nsresult rv = NS_OK;
  bool done = false;

  UniquePtr<Expr> expr;

  txStack exprs;
  txStack ops;

  while (!done) {
    uint16_t negations = 0;
    while (lexer.peek()->mType == Token::SUBTRACTION_OP) {
      negations++;
      lexer.nextToken();
    }

    rv = createUnionExpr(lexer, aContext, getter_Transfers(expr));
    if (NS_FAILED(rv)) {
      break;
    }

    if (negations > 0) {
      if (negations % 2 == 0) {
        auto fcExpr =
            MakeUnique<txCoreFunctionCall>(txCoreFunctionCall::NUMBER);

        fcExpr->addParam(expr.release());
        expr = std::move(fcExpr);
      } else {
        expr = MakeUnique<UnaryExpr>(expr.release());
      }
    }

    short tokPrecedence = precedence(lexer.peek());
    if (tokPrecedence != 0) {
      Token* tok = lexer.nextToken();
      while (!exprs.isEmpty() &&
             tokPrecedence <= precedence(static_cast<Token*>(ops.peek()))) {
        // can't use expr as argument due to order of evaluation
        UniquePtr<Expr> left(static_cast<Expr*>(exprs.pop()));
        UniquePtr<Expr> right(std::move(expr));
        rv = createBinaryExpr(left, right, static_cast<Token*>(ops.pop()),
                              getter_Transfers(expr));
        if (NS_FAILED(rv)) {
          done = true;
          break;
        }
      }
      exprs.push(expr.release());
      ops.push(tok);
    } else {
      done = true;
    }
  }

  while (NS_SUCCEEDED(rv) && !exprs.isEmpty()) {
    UniquePtr<Expr> left(static_cast<Expr*>(exprs.pop()));
    UniquePtr<Expr> right(std::move(expr));
    rv = createBinaryExpr(left, right, static_cast<Token*>(ops.pop()),
                          getter_Transfers(expr));
  }
  // clean up on error
  while (!exprs.isEmpty()) {
    delete static_cast<Expr*>(exprs.pop());
  }
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = expr.release();
  return NS_OK;
}

nsresult txExprParser::createFilterOrStep(txExprLexer& lexer,
                                          txIParseContext* aContext,
                                          Expr** aResult) {
  *aResult = nullptr;

  nsresult rv = NS_OK;
  Token* tok = lexer.peek();

  UniquePtr<Expr> expr;
  switch (tok->mType) {
    case Token::FUNCTION_NAME_AND_PAREN:
      rv = createFunctionCall(lexer, aContext, getter_Transfers(expr));
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case Token::VAR_REFERENCE:
      lexer.nextToken();
      {
        RefPtr<nsAtom> prefix, lName;
        int32_t nspace;
        nsresult rv = resolveQName(tok->Value(), getter_AddRefs(prefix),
                                   aContext, getter_AddRefs(lName), nspace);
        NS_ENSURE_SUCCESS(rv, rv);
        expr = MakeUnique<VariableRefExpr>(prefix, lName, nspace);
      }
      break;
    case Token::L_PAREN:
      lexer.nextToken();
      rv = createExpr(lexer, aContext, getter_Transfers(expr));
      NS_ENSURE_SUCCESS(rv, rv);

      if (lexer.peek()->mType != Token::R_PAREN) {
        return NS_ERROR_XPATH_PAREN_EXPECTED;
      }
      lexer.nextToken();
      break;
    case Token::LITERAL:
      lexer.nextToken();
      expr = MakeUnique<txLiteralExpr>(tok->Value());
      break;
    case Token::NUMBER: {
      lexer.nextToken();
      expr = MakeUnique<txLiteralExpr>(txDouble::toDouble(tok->Value()));
      break;
    }
    default:
      return createLocationStep(lexer, aContext, aResult);
  }

  if (lexer.peek()->mType == Token::L_BRACKET) {
    UniquePtr<FilterExpr> filterExpr(new FilterExpr(expr.get()));

    Unused << expr.release();

    //-- handle predicates
    rv = parsePredicates(filterExpr.get(), lexer, aContext);
    NS_ENSURE_SUCCESS(rv, rv);
    expr = std::move(filterExpr);
  }

  *aResult = expr.release();
  return NS_OK;
}

nsresult txExprParser::createFunctionCall(txExprLexer& lexer,
                                          txIParseContext* aContext,
                                          Expr** aResult) {
  *aResult = nullptr;

  UniquePtr<FunctionCall> fnCall;

  Token* tok = lexer.nextToken();
  NS_ASSERTION(tok->mType == Token::FUNCTION_NAME_AND_PAREN,
               "FunctionCall expected");

  //-- compare function names
  RefPtr<nsAtom> prefix, lName;
  int32_t namespaceID;
  nsresult rv = resolveQName(tok->Value(), getter_AddRefs(prefix), aContext,
                             getter_AddRefs(lName), namespaceID);
  NS_ENSURE_SUCCESS(rv, rv);

  txCoreFunctionCall::eType type;
  if (namespaceID == kNameSpaceID_None &&
      txCoreFunctionCall::getTypeFromAtom(lName, type)) {
    // It is a known built-in function.
    fnCall = MakeUnique<txCoreFunctionCall>(type);
  }

  // check extension functions and xslt
  if (!fnCall) {
    rv = aContext->resolveFunctionCall(lName, namespaceID,
                                       getter_Transfers(fnCall));

    if (rv == NS_ERROR_NOT_IMPLEMENTED) {
      // this should just happen for unparsed-entity-uri()
      NS_ASSERTION(!fnCall, "Now is it implemented or not?");
      rv = parseParameters(0, lexer, aContext);
      NS_ENSURE_SUCCESS(rv, rv);

      *aResult = new txLiteralExpr(tok->Value() + u" not implemented."_ns);

      return NS_OK;
    }

    NS_ENSURE_SUCCESS(rv, rv);
  }

  //-- handle parametes
  rv = parseParameters(fnCall.get(), lexer, aContext);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = fnCall.release();
  return NS_OK;
}

nsresult txExprParser::createLocationStep(txExprLexer& lexer,
                                          txIParseContext* aContext,
                                          Expr** aExpr) {
  *aExpr = nullptr;

  //-- child axis is default
  LocationStep::LocationStepType axisIdentifier = LocationStep::CHILD_AXIS;
  UniquePtr<txNodeTest> nodeTest;

  //-- get Axis Identifier or AbbreviatedStep, if present
  Token* tok = lexer.peek();
  switch (tok->mType) {
    case Token::AXIS_IDENTIFIER: {
      //-- eat token
      lexer.nextToken();
      RefPtr<nsAtom> axis = NS_Atomize(tok->Value());
      if (axis == nsGkAtoms::ancestor) {
        axisIdentifier = LocationStep::ANCESTOR_AXIS;
      } else if (axis == nsGkAtoms::ancestorOrSelf) {
        axisIdentifier = LocationStep::ANCESTOR_OR_SELF_AXIS;
      } else if (axis == nsGkAtoms::attribute) {
        axisIdentifier = LocationStep::ATTRIBUTE_AXIS;
      } else if (axis == nsGkAtoms::child) {
        axisIdentifier = LocationStep::CHILD_AXIS;
      } else if (axis == nsGkAtoms::descendant) {
        axisIdentifier = LocationStep::DESCENDANT_AXIS;
      } else if (axis == nsGkAtoms::descendantOrSelf) {
        axisIdentifier = LocationStep::DESCENDANT_OR_SELF_AXIS;
      } else if (axis == nsGkAtoms::following) {
        axisIdentifier = LocationStep::FOLLOWING_AXIS;
      } else if (axis == nsGkAtoms::followingSibling) {
        axisIdentifier = LocationStep::FOLLOWING_SIBLING_AXIS;
      } else if (axis == nsGkAtoms::_namespace) {
        axisIdentifier = LocationStep::NAMESPACE_AXIS;
      } else if (axis == nsGkAtoms::parent) {
        axisIdentifier = LocationStep::PARENT_AXIS;
      } else if (axis == nsGkAtoms::preceding) {
        axisIdentifier = LocationStep::PRECEDING_AXIS;
      } else if (axis == nsGkAtoms::precedingSibling) {
        axisIdentifier = LocationStep::PRECEDING_SIBLING_AXIS;
      } else if (axis == nsGkAtoms::self) {
        axisIdentifier = LocationStep::SELF_AXIS;
      } else {
        return NS_ERROR_XPATH_INVALID_AXIS;
      }
      break;
    }
    case Token::AT_SIGN:
      //-- eat token
      lexer.nextToken();
      axisIdentifier = LocationStep::ATTRIBUTE_AXIS;
      break;
    case Token::PARENT_NODE:
      //-- eat token
      lexer.nextToken();
      axisIdentifier = LocationStep::PARENT_AXIS;
      nodeTest = MakeUnique<txNodeTypeTest>(txNodeTypeTest::NODE_TYPE);
      break;
    case Token::SELF_NODE:
      //-- eat token
      lexer.nextToken();
      axisIdentifier = LocationStep::SELF_AXIS;
      nodeTest = MakeUnique<txNodeTypeTest>(txNodeTypeTest::NODE_TYPE);
      break;
    default:
      break;
  }

  //-- get NodeTest unless an AbbreviatedStep was found
  nsresult rv = NS_OK;
  if (!nodeTest) {
    tok = lexer.peek();

    if (tok->mType == Token::CNAME) {
      lexer.nextToken();
      // resolve QName
      RefPtr<nsAtom> prefix, lName;
      int32_t nspace;
      rv = resolveQName(tok->Value(), getter_AddRefs(prefix), aContext,
                        getter_AddRefs(lName), nspace, true);
      NS_ENSURE_SUCCESS(rv, rv);

      nodeTest = MakeUnique<txNameTest>(
          prefix, lName, nspace,
          axisIdentifier == LocationStep::ATTRIBUTE_AXIS
              ? static_cast<uint16_t>(txXPathNodeType::ATTRIBUTE_NODE)
              : static_cast<uint16_t>(txXPathNodeType::ELEMENT_NODE));
    } else {
      rv = createNodeTypeTest(lexer, getter_Transfers(nodeTest));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  UniquePtr<LocationStep> lstep(
      new LocationStep(nodeTest.get(), axisIdentifier));

  Unused << nodeTest.release();

  //-- handle predicates
  rv = parsePredicates(lstep.get(), lexer, aContext);
  NS_ENSURE_SUCCESS(rv, rv);

  *aExpr = lstep.release();
  return NS_OK;
}

/**
 * This method only handles comment(), text(), processing-instructing()
 * and node()
 */
nsresult txExprParser::createNodeTypeTest(txExprLexer& lexer,
                                          txNodeTest** aTest) {
  *aTest = 0;
  UniquePtr<txNodeTypeTest> nodeTest;

  Token* nodeTok = lexer.peek();

  switch (nodeTok->mType) {
    case Token::COMMENT_AND_PAREN:
      lexer.nextToken();
      nodeTest = MakeUnique<txNodeTypeTest>(txNodeTypeTest::COMMENT_TYPE);
      break;
    case Token::NODE_AND_PAREN:
      lexer.nextToken();
      nodeTest = MakeUnique<txNodeTypeTest>(txNodeTypeTest::NODE_TYPE);
      break;
    case Token::PROC_INST_AND_PAREN:
      lexer.nextToken();
      nodeTest = MakeUnique<txNodeTypeTest>(txNodeTypeTest::PI_TYPE);
      break;
    case Token::TEXT_AND_PAREN:
      lexer.nextToken();
      nodeTest = MakeUnique<txNodeTypeTest>(txNodeTypeTest::TEXT_TYPE);
      break;
    default:
      return NS_ERROR_XPATH_NO_NODE_TYPE_TEST;
  }

  if (nodeTok->mType == Token::PROC_INST_AND_PAREN &&
      lexer.peek()->mType == Token::LITERAL) {
    Token* tok = lexer.nextToken();
    nodeTest->setNodeName(tok->Value());
  }
  if (lexer.peek()->mType != Token::R_PAREN) {
    return NS_ERROR_XPATH_PAREN_EXPECTED;
  }
  lexer.nextToken();

  *aTest = nodeTest.release();
  return NS_OK;
}

/**
 * Creates a PathExpr using the given txExprLexer
 * @param lexer the txExprLexer for retrieving Tokens
 */
nsresult txExprParser::createPathExpr(txExprLexer& lexer,
                                      txIParseContext* aContext,
                                      Expr** aResult) {
  *aResult = nullptr;

  UniquePtr<Expr> expr;

  Token* tok = lexer.peek();

  // is this a root expression?
  if (tok->mType == Token::PARENT_OP) {
    if (!isLocationStepToken(lexer.peekAhead())) {
      lexer.nextToken();
      *aResult = new RootExpr();
      return NS_OK;
    }
  }

  // parse first step (possibly a FilterExpr)
  nsresult rv = NS_OK;
  if (tok->mType != Token::PARENT_OP && tok->mType != Token::ANCESTOR_OP) {
    rv = createFilterOrStep(lexer, aContext, getter_Transfers(expr));
    NS_ENSURE_SUCCESS(rv, rv);

    // is this a singlestep path expression?
    tok = lexer.peek();
    if (tok->mType != Token::PARENT_OP && tok->mType != Token::ANCESTOR_OP) {
      *aResult = expr.release();
      return NS_OK;
    }
  } else {
    expr = MakeUnique<RootExpr>();

#ifdef TX_TO_STRING
    static_cast<RootExpr*>(expr.get())->setSerialize(false);
#endif
  }

  // We have a PathExpr containing several steps
  UniquePtr<PathExpr> pathExpr(new PathExpr());
  pathExpr->addExpr(expr.release(), PathExpr::RELATIVE_OP);

  // this is ugly
  while (1) {
    PathExpr::PathOperator pathOp;
    switch (lexer.peek()->mType) {
      case Token::ANCESTOR_OP:
        pathOp = PathExpr::DESCENDANT_OP;
        break;
      case Token::PARENT_OP:
        pathOp = PathExpr::RELATIVE_OP;
        break;
      default:
        *aResult = pathExpr.release();
        return NS_OK;
    }
    lexer.nextToken();

    rv = createLocationStep(lexer, aContext, getter_Transfers(expr));
    NS_ENSURE_SUCCESS(rv, rv);

    pathExpr->addExpr(expr.release(), pathOp);
  }
  MOZ_ASSERT_UNREACHABLE("internal xpath parser error");
  return NS_ERROR_UNEXPECTED;
}

/**
 * Creates a PathExpr using the given txExprLexer
 * @param lexer the txExprLexer for retrieving Tokens
 */
nsresult txExprParser::createUnionExpr(txExprLexer& lexer,
                                       txIParseContext* aContext,
                                       Expr** aResult) {
  *aResult = nullptr;

  UniquePtr<Expr> expr;
  nsresult rv = createPathExpr(lexer, aContext, getter_Transfers(expr));
  NS_ENSURE_SUCCESS(rv, rv);

  if (lexer.peek()->mType != Token::UNION_OP) {
    *aResult = expr.release();
    return NS_OK;
  }

  UniquePtr<UnionExpr> unionExpr(new UnionExpr());
  unionExpr->addExpr(expr.release());

  while (lexer.peek()->mType == Token::UNION_OP) {
    lexer.nextToken();  //-- eat token

    rv = createPathExpr(lexer, aContext, getter_Transfers(expr));
    NS_ENSURE_SUCCESS(rv, rv);

    unionExpr->addExpr(expr.release());
  }

  *aResult = unionExpr.release();
  return NS_OK;
}

bool txExprParser::isLocationStepToken(Token* aToken) {
  // We could put these in consecutive order in ExprLexer.h for speed
  return aToken->mType == Token::AXIS_IDENTIFIER ||
         aToken->mType == Token::AT_SIGN ||
         aToken->mType == Token::PARENT_NODE ||
         aToken->mType == Token::SELF_NODE || aToken->mType == Token::CNAME ||
         aToken->mType == Token::COMMENT_AND_PAREN ||
         aToken->mType == Token::NODE_AND_PAREN ||
         aToken->mType == Token::PROC_INST_AND_PAREN ||
         aToken->mType == Token::TEXT_AND_PAREN;
}

/**
 * Using the given lexer, parses the tokens if they represent a predicate list
 * If an error occurs a non-zero String pointer will be returned containing the
 * error message.
 * @param predicateList, the PredicateList to add predicate expressions to
 * @param lexer the txExprLexer to use for parsing tokens
 * @return 0 if successful, or a String pointer to the error message
 */
nsresult txExprParser::parsePredicates(PredicateList* aPredicateList,
                                       txExprLexer& lexer,
                                       txIParseContext* aContext) {
  UniquePtr<Expr> expr;
  nsresult rv = NS_OK;
  while (lexer.peek()->mType == Token::L_BRACKET) {
    //-- eat Token
    lexer.nextToken();

    rv = createExpr(lexer, aContext, getter_Transfers(expr));
    NS_ENSURE_SUCCESS(rv, rv);

    aPredicateList->add(expr.release());

    if (lexer.peek()->mType != Token::R_BRACKET) {
      return NS_ERROR_XPATH_BRACKET_EXPECTED;
    }
    lexer.nextToken();
  }
  return NS_OK;
}

/**
 * Using the given lexer, parses the tokens if they represent a parameter list
 * If an error occurs a non-zero String pointer will be returned containing the
 * error message.
 * @param list, the List to add parameter expressions to
 * @param lexer the txExprLexer to use for parsing tokens
 * @return NS_OK if successful, or another rv otherwise
 */
nsresult txExprParser::parseParameters(FunctionCall* aFnCall,
                                       txExprLexer& lexer,
                                       txIParseContext* aContext) {
  if (lexer.peek()->mType == Token::R_PAREN) {
    lexer.nextToken();
    return NS_OK;
  }

  UniquePtr<Expr> expr;
  nsresult rv = NS_OK;
  while (1) {
    rv = createExpr(lexer, aContext, getter_Transfers(expr));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aFnCall) {
      aFnCall->addParam(expr.release());
    }

    switch (lexer.peek()->mType) {
      case Token::R_PAREN:
        lexer.nextToken();
        return NS_OK;
      case Token::COMMA:  //-- param separator
        lexer.nextToken();
        break;
      default:
        return NS_ERROR_XPATH_PAREN_EXPECTED;
    }
  }

  MOZ_ASSERT_UNREACHABLE("internal xpath parser error");
  return NS_ERROR_UNEXPECTED;
}

short txExprParser::precedence(Token* aToken) {
  switch (aToken->mType) {
    case Token::OR_OP:
      return 1;
    case Token::AND_OP:
      return 2;
    //-- equality
    case Token::EQUAL_OP:
    case Token::NOT_EQUAL_OP:
      return 3;
    //-- relational
    case Token::LESS_THAN_OP:
    case Token::GREATER_THAN_OP:
    case Token::LESS_OR_EQUAL_OP:
    case Token::GREATER_OR_EQUAL_OP:
      return 4;
    //-- additive operators
    case Token::ADDITION_OP:
    case Token::SUBTRACTION_OP:
      return 5;
    //-- multiplicative
    case Token::DIVIDE_OP:
    case Token::MULTIPLY_OP:
    case Token::MODULUS_OP:
      return 6;
    default:
      break;
  }
  return 0;
}

nsresult txExprParser::resolveQName(const nsAString& aQName, nsAtom** aPrefix,
                                    txIParseContext* aContext,
                                    nsAtom** aLocalName, int32_t& aNamespace,
                                    bool aIsNameTest) {
  aNamespace = kNameSpaceID_None;
  int32_t idx = aQName.FindChar(':');
  if (idx > 0) {
    *aPrefix = NS_Atomize(StringHead(aQName, (uint32_t)idx)).take();
    if (!*aPrefix) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    *aLocalName = NS_Atomize(Substring(aQName, (uint32_t)idx + 1,
                                       aQName.Length() - (idx + 1)))
                      .take();
    if (!*aLocalName) {
      NS_RELEASE(*aPrefix);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return aContext->resolveNamespacePrefix(*aPrefix, aNamespace);
  }
  // the lexer dealt with idx == 0
  *aPrefix = 0;
  if (aIsNameTest && aContext->caseInsensitiveNameTests()) {
    nsAutoString lcname;
    nsContentUtils::ASCIIToLower(aQName, lcname);
    *aLocalName = NS_Atomize(lcname).take();
  } else {
    *aLocalName = NS_Atomize(aQName).take();
  }
  if (!*aLocalName) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

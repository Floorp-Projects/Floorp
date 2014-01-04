/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ExprParser
 * This class is used to parse XSL Expressions
 * @see ExprLexer
**/

#include "txExprParser.h"
#include "txExprLexer.h"
#include "txExpr.h"
#include "txStack.h"
#include "nsGkAtoms.h"
#include "nsError.h"
#include "txIXPathContext.h"
#include "txStringUtils.h"
#include "txXPathNode.h"
#include "txXPathOptimizer.h"

/**
 * Creates an Attribute Value Template using the given value
 * This should move to XSLProcessor class
 */
nsresult
txExprParser::createAVT(const nsSubstring& aAttrValue,
                        txIParseContext* aContext,
                        Expr** aResult)
{
    *aResult = nullptr;
    nsresult rv = NS_OK;
    nsAutoPtr<Expr> expr;
    FunctionCall* concat = nullptr;

    nsAutoString literalString;
    bool inExpr = false;
    nsSubstring::const_char_iterator iter, start, end, avtStart;
    aAttrValue.BeginReading(iter);
    aAttrValue.EndReading(end);
    avtStart = iter;

    while (iter != end) {
        // Every iteration through this loop parses either a literal section
        // or an expression
        start = iter;
        nsAutoPtr<Expr> newExpr;
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
            newExpr = new txLiteralExpr(literalString +
                                        Substring(start, iter));
        }
        else {
            // Parse expressions, iter is already past the initial '{' when
            // we get here.
            while (iter != end) {
                if (*iter == '}') {
                    rv = createExprInternal(Substring(start, iter),
                                            start - avtStart, aContext,
                                            getter_Transfers(newExpr));
                    NS_ENSURE_SUCCESS(rv, rv);

                    inExpr = false;
                    ++iter; // skip closing '}'
                    break;
                }
                else if (*iter == '\'' || *iter == '"') {
                    char16_t q = *iter;
                    while (++iter != end && *iter != q) {} /* do nothing */
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
            expr = newExpr;
        }
        else {
            if (!concat) {
                concat = new txCoreFunctionCall(txCoreFunctionCall::CONCAT);
                NS_ENSURE_TRUE(concat, NS_ERROR_OUT_OF_MEMORY);

                rv = concat->addParam(expr.forget());
                expr = concat;
                NS_ENSURE_SUCCESS(rv, rv);
            }

            rv = concat->addParam(newExpr.forget());
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    if (inExpr) {
        aContext->SetErrorOffset(iter - avtStart);
        return NS_ERROR_XPATH_UNBALANCED_CURLY_BRACE;
    }

    if (!expr) {
        expr = new txLiteralExpr(EmptyString());
    }

    *aResult = expr.forget();

    return NS_OK;
}

nsresult
txExprParser::createExprInternal(const nsSubstring& aExpression,
                                 uint32_t aSubStringPos,
                                 txIParseContext* aContext, Expr** aExpr)
{
    NS_ENSURE_ARG_POINTER(aExpr);
    *aExpr = nullptr;
    txExprLexer lexer;
    nsresult rv = lexer.parse(aExpression);
    if (NS_FAILED(rv)) {
        nsASingleFragmentString::const_char_iterator start;
        aExpression.BeginReading(start);
        aContext->SetErrorOffset(lexer.mPosition - start + aSubStringPos);
        return rv;
    }
    nsAutoPtr<Expr> expr;
    rv = createExpr(lexer, aContext, getter_Transfers(expr));
    if (NS_SUCCEEDED(rv) && lexer.peek()->mType != Token::END) {
        rv = NS_ERROR_XPATH_BINARY_EXPECTED;
    }
    if (NS_FAILED(rv)) {
        nsASingleFragmentString::const_char_iterator start;
        aExpression.BeginReading(start);
        aContext->SetErrorOffset(lexer.peek()->mStart - start + aSubStringPos);

        return rv;
    }

    txXPathOptimizer optimizer;
    Expr* newExpr = nullptr;
    rv = optimizer.optimize(expr, &newExpr);
    NS_ENSURE_SUCCESS(rv, rv);

    *aExpr = newExpr ? newExpr : expr.forget();

    return NS_OK;
}

/**
 * Private Methods
 */

/**
 * Creates a binary Expr for the given operator
 */
nsresult
txExprParser::createBinaryExpr(nsAutoPtr<Expr>& left, nsAutoPtr<Expr>& right,
                               Token* op, Expr** aResult)
{
    NS_ASSERTION(op, "internal error");
    *aResult = nullptr;

    Expr* expr = nullptr;
    switch (op->mType) {
        //-- math ops
        case Token::ADDITION_OP :
            expr = new txNumberExpr(left, right, txNumberExpr::ADD);
            break;
        case Token::SUBTRACTION_OP:
            expr = new txNumberExpr(left, right, txNumberExpr::SUBTRACT);
            break;
        case Token::DIVIDE_OP :
            expr = new txNumberExpr(left, right, txNumberExpr::DIVIDE);
            break;
        case Token::MODULUS_OP :
            expr = new txNumberExpr(left, right, txNumberExpr::MODULUS);
            break;
        case Token::MULTIPLY_OP :
            expr = new txNumberExpr(left, right, txNumberExpr::MULTIPLY);
            break;

        //-- case boolean ops
        case Token::AND_OP:
            expr = new BooleanExpr(left, right, BooleanExpr::AND);
            break;
        case Token::OR_OP:
            expr = new BooleanExpr(left, right, BooleanExpr::OR);
            break;

        //-- equality ops
        case Token::EQUAL_OP :
            expr = new RelationalExpr(left, right, RelationalExpr::EQUAL);
            break;
        case Token::NOT_EQUAL_OP :
            expr = new RelationalExpr(left, right, RelationalExpr::NOT_EQUAL);
            break;

        //-- relational ops
        case Token::LESS_THAN_OP:
            expr = new RelationalExpr(left, right, RelationalExpr::LESS_THAN);
            break;
        case Token::GREATER_THAN_OP:
            expr = new RelationalExpr(left, right,
                                      RelationalExpr::GREATER_THAN);
            break;
        case Token::LESS_OR_EQUAL_OP:
            expr = new RelationalExpr(left, right,
                                      RelationalExpr::LESS_OR_EQUAL);
            break;
        case Token::GREATER_OR_EQUAL_OP:
            expr = new RelationalExpr(left, right,
                                      RelationalExpr::GREATER_OR_EQUAL);
            break;

        default:
            NS_NOTREACHED("operator tokens should be already checked");
            return NS_ERROR_UNEXPECTED;
    }
    NS_ENSURE_TRUE(expr, NS_ERROR_OUT_OF_MEMORY);

    left.forget();
    right.forget();

    *aResult = expr;
    return NS_OK;
}


nsresult
txExprParser::createExpr(txExprLexer& lexer, txIParseContext* aContext,
                         Expr** aResult)
{
    *aResult = nullptr;

    nsresult rv = NS_OK;
    bool done = false;

    nsAutoPtr<Expr> expr;

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
                FunctionCall* fcExpr = new txCoreFunctionCall(txCoreFunctionCall::NUMBER);
                
                rv = fcExpr->addParam(expr);
                if (NS_FAILED(rv))
                    return rv;
                expr.forget();
                expr = fcExpr;
            }
            else {
                expr = new UnaryExpr(expr.forget());
            }
        }

        short tokPrecedence = precedence(lexer.peek());
        if (tokPrecedence != 0) {
            Token* tok = lexer.nextToken();
            while (!exprs.isEmpty() && tokPrecedence
                   <= precedence(static_cast<Token*>(ops.peek()))) {
                // can't use expr as argument due to order of evaluation
                nsAutoPtr<Expr> left(static_cast<Expr*>(exprs.pop()));
                nsAutoPtr<Expr> right(expr);
                rv = createBinaryExpr(left, right,
                                      static_cast<Token*>(ops.pop()),
                                      getter_Transfers(expr));
                if (NS_FAILED(rv)) {
                    done = true;
                    break;
                }
            }
            exprs.push(expr.forget());
            ops.push(tok);
        }
        else {
            done = true;
        }
    }

    while (NS_SUCCEEDED(rv) && !exprs.isEmpty()) {
        nsAutoPtr<Expr> left(static_cast<Expr*>(exprs.pop()));
        nsAutoPtr<Expr> right(expr);
        rv = createBinaryExpr(left, right, static_cast<Token*>(ops.pop()),
                              getter_Transfers(expr));
    }
    // clean up on error
    while (!exprs.isEmpty()) {
        delete static_cast<Expr*>(exprs.pop());
    }
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = expr.forget();
    return NS_OK;
}

nsresult
txExprParser::createFilterOrStep(txExprLexer& lexer, txIParseContext* aContext,
                                 Expr** aResult)
{
    *aResult = nullptr;

    nsresult rv = NS_OK;
    Token* tok = lexer.peek();

    nsAutoPtr<Expr> expr;
    switch (tok->mType) {
        case Token::FUNCTION_NAME_AND_PAREN:
            rv = createFunctionCall(lexer, aContext, getter_Transfers(expr));
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        case Token::VAR_REFERENCE :
            lexer.nextToken();
            {
                nsCOMPtr<nsIAtom> prefix, lName;
                int32_t nspace;
                nsresult rv = resolveQName(tok->Value(), getter_AddRefs(prefix),
                                           aContext, getter_AddRefs(lName),
                                           nspace);
                NS_ENSURE_SUCCESS(rv, rv);
                expr = new VariableRefExpr(prefix, lName, nspace);
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
        case Token::LITERAL :
            lexer.nextToken();
            expr = new txLiteralExpr(tok->Value());
            break;
        case Token::NUMBER:
        {
            lexer.nextToken();
            expr = new txLiteralExpr(txDouble::toDouble(tok->Value()));
            break;
        }
        default:
            return createLocationStep(lexer, aContext, aResult);
    }

    if (lexer.peek()->mType == Token::L_BRACKET) {
        nsAutoPtr<FilterExpr> filterExpr(new FilterExpr(expr));

        expr.forget();

        //-- handle predicates
        rv = parsePredicates(filterExpr, lexer, aContext);
        NS_ENSURE_SUCCESS(rv, rv);
        expr = filterExpr.forget();
    }

    *aResult = expr.forget();
    return NS_OK;
}

nsresult
txExprParser::createFunctionCall(txExprLexer& lexer, txIParseContext* aContext,
                                 Expr** aResult)
{
    *aResult = nullptr;

    nsAutoPtr<FunctionCall> fnCall;

    Token* tok = lexer.nextToken();
    NS_ASSERTION(tok->mType == Token::FUNCTION_NAME_AND_PAREN,
                 "FunctionCall expected");

    //-- compare function names
    nsCOMPtr<nsIAtom> prefix, lName;
    int32_t namespaceID;
    nsresult rv = resolveQName(tok->Value(), getter_AddRefs(prefix), aContext,
                               getter_AddRefs(lName), namespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    txCoreFunctionCall::eType type;
    if (namespaceID == kNameSpaceID_None &&
        txCoreFunctionCall::getTypeFromAtom(lName, type)) {
        // It is a known built-in function.
        fnCall = new txCoreFunctionCall(type);
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

            *aResult = new txLiteralExpr(tok->Value() +
                                         NS_LITERAL_STRING(" not implemented."));

            return NS_OK;
        }

        NS_ENSURE_SUCCESS(rv, rv);
    }

    //-- handle parametes
    rv = parseParameters(fnCall, lexer, aContext);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = fnCall.forget();
    return NS_OK;
}

nsresult
txExprParser::createLocationStep(txExprLexer& lexer, txIParseContext* aContext,
                                 Expr** aExpr)
{
    *aExpr = nullptr;

    //-- child axis is default
    LocationStep::LocationStepType axisIdentifier = LocationStep::CHILD_AXIS;
    nsAutoPtr<txNodeTest> nodeTest;

    //-- get Axis Identifier or AbbreviatedStep, if present
    Token* tok = lexer.peek();
    switch (tok->mType) {
        case Token::AXIS_IDENTIFIER:
        {
            //-- eat token
            lexer.nextToken();
            nsCOMPtr<nsIAtom> axis = do_GetAtom(tok->Value());
            if (axis == nsGkAtoms::ancestor) {
                axisIdentifier = LocationStep::ANCESTOR_AXIS;
            }
            else if (axis == nsGkAtoms::ancestorOrSelf) {
                axisIdentifier = LocationStep::ANCESTOR_OR_SELF_AXIS;
            }
            else if (axis == nsGkAtoms::attribute) {
                axisIdentifier = LocationStep::ATTRIBUTE_AXIS;
            }
            else if (axis == nsGkAtoms::child) {
                axisIdentifier = LocationStep::CHILD_AXIS;
            }
            else if (axis == nsGkAtoms::descendant) {
                axisIdentifier = LocationStep::DESCENDANT_AXIS;
            }
            else if (axis == nsGkAtoms::descendantOrSelf) {
                axisIdentifier = LocationStep::DESCENDANT_OR_SELF_AXIS;
            }
            else if (axis == nsGkAtoms::following) {
                axisIdentifier = LocationStep::FOLLOWING_AXIS;
            }
            else if (axis == nsGkAtoms::followingSibling) {
                axisIdentifier = LocationStep::FOLLOWING_SIBLING_AXIS;
            }
            else if (axis == nsGkAtoms::_namespace) {
                axisIdentifier = LocationStep::NAMESPACE_AXIS;
            }
            else if (axis == nsGkAtoms::parent) {
                axisIdentifier = LocationStep::PARENT_AXIS;
            }
            else if (axis == nsGkAtoms::preceding) {
                axisIdentifier = LocationStep::PRECEDING_AXIS;
            }
            else if (axis == nsGkAtoms::precedingSibling) {
                axisIdentifier = LocationStep::PRECEDING_SIBLING_AXIS;
            }
            else if (axis == nsGkAtoms::self) {
                axisIdentifier = LocationStep::SELF_AXIS;
            }
            else {
                return NS_ERROR_XPATH_INVALID_AXIS;
            }
            break;
        }
        case Token::AT_SIGN:
            //-- eat token
            lexer.nextToken();
            axisIdentifier = LocationStep::ATTRIBUTE_AXIS;
            break;
        case Token::PARENT_NODE :
            //-- eat token
            lexer.nextToken();
            axisIdentifier = LocationStep::PARENT_AXIS;
            nodeTest = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
            break;
        case Token::SELF_NODE :
            //-- eat token
            lexer.nextToken();
            axisIdentifier = LocationStep::SELF_AXIS;
            nodeTest = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
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
            nsCOMPtr<nsIAtom> prefix, lName;
            int32_t nspace;
            rv = resolveQName(tok->Value(), getter_AddRefs(prefix),
                              aContext, getter_AddRefs(lName),
                              nspace, true);
            NS_ENSURE_SUCCESS(rv, rv);

            nodeTest =
              new txNameTest(prefix, lName, nspace,
                             axisIdentifier == LocationStep::ATTRIBUTE_AXIS ?
                             static_cast<uint16_t>(txXPathNodeType::ATTRIBUTE_NODE) :
                             static_cast<uint16_t>(txXPathNodeType::ELEMENT_NODE));
        }
        else {
            rv = createNodeTypeTest(lexer, getter_Transfers(nodeTest));
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }
    
    nsAutoPtr<LocationStep> lstep(new LocationStep(nodeTest, axisIdentifier));

    nodeTest.forget();

    //-- handle predicates
    rv = parsePredicates(lstep, lexer, aContext);
    NS_ENSURE_SUCCESS(rv, rv);

    *aExpr = lstep.forget();
    return NS_OK;
}

/**
 * This method only handles comment(), text(), processing-instructing()
 * and node()
 */
nsresult
txExprParser::createNodeTypeTest(txExprLexer& lexer, txNodeTest** aTest)
{
    *aTest = 0;
    nsAutoPtr<txNodeTypeTest> nodeTest;

    Token* nodeTok = lexer.peek();

    switch (nodeTok->mType) {
        case Token::COMMENT_AND_PAREN:
            lexer.nextToken();
            nodeTest = new txNodeTypeTest(txNodeTypeTest::COMMENT_TYPE);
            break;
        case Token::NODE_AND_PAREN:
            lexer.nextToken();
            nodeTest = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
            break;
        case Token::PROC_INST_AND_PAREN:
            lexer.nextToken();
            nodeTest = new txNodeTypeTest(txNodeTypeTest::PI_TYPE);
            break;
        case Token::TEXT_AND_PAREN:
            lexer.nextToken();
            nodeTest = new txNodeTypeTest(txNodeTypeTest::TEXT_TYPE);
            break;
        default:
            return NS_ERROR_XPATH_NO_NODE_TYPE_TEST;
    }

    NS_ENSURE_TRUE(nodeTest, NS_ERROR_OUT_OF_MEMORY);

    if (nodeTok->mType == Token::PROC_INST_AND_PAREN &&
        lexer.peek()->mType == Token::LITERAL) {
        Token* tok = lexer.nextToken();
        nodeTest->setNodeName(tok->Value());
    }
    if (lexer.peek()->mType != Token::R_PAREN) {
        return NS_ERROR_XPATH_PAREN_EXPECTED;
    }
    lexer.nextToken();

    *aTest = nodeTest.forget();
    return NS_OK;
}

/**
 * Creates a PathExpr using the given txExprLexer
 * @param lexer the txExprLexer for retrieving Tokens
 */
nsresult
txExprParser::createPathExpr(txExprLexer& lexer, txIParseContext* aContext,
                             Expr** aResult)
{
    *aResult = nullptr;

    nsAutoPtr<Expr> expr;

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
    if (tok->mType != Token::PARENT_OP &&
        tok->mType != Token::ANCESTOR_OP) {
        rv = createFilterOrStep(lexer, aContext, getter_Transfers(expr));
        NS_ENSURE_SUCCESS(rv, rv);

        // is this a singlestep path expression?
        tok = lexer.peek();
        if (tok->mType != Token::PARENT_OP &&
            tok->mType != Token::ANCESTOR_OP) {
            *aResult = expr.forget();
            return NS_OK;
        }
    }
    else {
        expr = new RootExpr();

#ifdef TX_TO_STRING
        static_cast<RootExpr*>(expr.get())->setSerialize(false);
#endif
    }
    
    // We have a PathExpr containing several steps
    nsAutoPtr<PathExpr> pathExpr(new PathExpr());

    rv = pathExpr->addExpr(expr, PathExpr::RELATIVE_OP);
    NS_ENSURE_SUCCESS(rv, rv);

    expr.forget();

    // this is ugly
    while (1) {
        PathExpr::PathOperator pathOp;
        switch (lexer.peek()->mType) {
            case Token::ANCESTOR_OP :
                pathOp = PathExpr::DESCENDANT_OP;
                break;
            case Token::PARENT_OP :
                pathOp = PathExpr::RELATIVE_OP;
                break;
            default:
                *aResult = pathExpr.forget();
                return NS_OK;
        }
        lexer.nextToken();

        rv = createLocationStep(lexer, aContext, getter_Transfers(expr));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = pathExpr->addExpr(expr, pathOp);
        NS_ENSURE_SUCCESS(rv, rv);

        expr.forget();
    }
    NS_NOTREACHED("internal xpath parser error");
    return NS_ERROR_UNEXPECTED;
}

/**
 * Creates a PathExpr using the given txExprLexer
 * @param lexer the txExprLexer for retrieving Tokens
 */
nsresult
txExprParser::createUnionExpr(txExprLexer& lexer, txIParseContext* aContext,
                              Expr** aResult)
{
    *aResult = nullptr;

    nsAutoPtr<Expr> expr;
    nsresult rv = createPathExpr(lexer, aContext, getter_Transfers(expr));
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (lexer.peek()->mType != Token::UNION_OP) {
        *aResult = expr.forget();
        return NS_OK;
    }

    nsAutoPtr<UnionExpr> unionExpr(new UnionExpr());

    rv = unionExpr->addExpr(expr);
    NS_ENSURE_SUCCESS(rv, rv);

    expr.forget();

    while (lexer.peek()->mType == Token::UNION_OP) {
        lexer.nextToken(); //-- eat token

        rv = createPathExpr(lexer, aContext, getter_Transfers(expr));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = unionExpr->addExpr(expr.forget());
        NS_ENSURE_SUCCESS(rv, rv);
    }

    *aResult = unionExpr.forget();
    return NS_OK;
}

bool
txExprParser::isLocationStepToken(Token* aToken)
{
    // We could put these in consecutive order in ExprLexer.h for speed
    return aToken->mType == Token::AXIS_IDENTIFIER ||
           aToken->mType == Token::AT_SIGN ||
           aToken->mType == Token::PARENT_NODE ||
           aToken->mType == Token::SELF_NODE ||
           aToken->mType == Token::CNAME ||
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
nsresult
txExprParser::parsePredicates(PredicateList* aPredicateList,
                              txExprLexer& lexer, txIParseContext* aContext)
{
    nsAutoPtr<Expr> expr;
    nsresult rv = NS_OK;
    while (lexer.peek()->mType == Token::L_BRACKET) {
        //-- eat Token
        lexer.nextToken();

        rv = createExpr(lexer, aContext, getter_Transfers(expr));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aPredicateList->add(expr);
        NS_ENSURE_SUCCESS(rv, rv);

        expr.forget();

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
nsresult
txExprParser::parseParameters(FunctionCall* aFnCall, txExprLexer& lexer,
                              txIParseContext* aContext)
{
    if (lexer.peek()->mType == Token::R_PAREN) {
        lexer.nextToken();
        return NS_OK;
    }

    nsAutoPtr<Expr> expr;
    nsresult rv = NS_OK;
    while (1) {
        rv = createExpr(lexer, aContext, getter_Transfers(expr));
        NS_ENSURE_SUCCESS(rv, rv);

        if (aFnCall) {
            rv = aFnCall->addParam(expr.forget());
            NS_ENSURE_SUCCESS(rv, rv);
        }
                    
        switch (lexer.peek()->mType) {
            case Token::R_PAREN :
                lexer.nextToken();
                return NS_OK;
            case Token::COMMA: //-- param separator
                lexer.nextToken();
                break;
            default:
                return NS_ERROR_XPATH_PAREN_EXPECTED;
        }
    }

    NS_NOTREACHED("internal xpath parser error");
    return NS_ERROR_UNEXPECTED;
}

short
txExprParser::precedence(Token* aToken)
{
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

nsresult
txExprParser::resolveQName(const nsAString& aQName,
                           nsIAtom** aPrefix, txIParseContext* aContext,
                           nsIAtom** aLocalName, int32_t& aNamespace,
                           bool aIsNameTest)
{
    aNamespace = kNameSpaceID_None;
    int32_t idx = aQName.FindChar(':');
    if (idx > 0) {
        *aPrefix = NS_NewAtom(StringHead(aQName, (uint32_t)idx)).get();
        if (!*aPrefix) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        *aLocalName = NS_NewAtom(Substring(aQName, (uint32_t)idx + 1,
                                           aQName.Length() - (idx + 1))).get();
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
        *aLocalName = NS_NewAtom(lcname).get();
    }
    else {
        *aLocalName = NS_NewAtom(aQName).get();
    }
    if (!*aLocalName) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

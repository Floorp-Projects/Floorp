/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
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

/**
 * ExprParser
 * This class is used to parse XSL Expressions
 * @see ExprLexer
**/

#include "ExprParser.h"
#include "ExprLexer.h"
#include "FunctionLib.h"
#include "txStack.h"
#include "txAtoms.h"
#include "txError.h"
#include "txIXPathContext.h"
#include "txStringUtils.h"
#include "txXPathNode.h"

/**
 * Creates an Attribute Value Template using the given value
 * This should move to XSLProcessor class
**/
AttributeValueTemplate* txExprParser::createAttributeValueTemplate
    (const nsAFlatString& attValue, txIParseContext* aContext)
{
    AttributeValueTemplate* avt = new AttributeValueTemplate();
    if (!avt) {
        // XXX ErrorReport: out of memory
        return 0;
    }

    if (attValue.IsEmpty())
        return avt;

    PRUint32 size = attValue.Length();
    PRUint32 cc = 0;
    PRUnichar nextCh;
    PRUnichar ch;
    nsAutoString buffer;
    MBool inExpr    = MB_FALSE;
    MBool inLiteral = MB_FALSE;
    PRUnichar endLiteral = 0;

    nextCh = attValue.CharAt(cc);
    while (cc++ < size) {
        ch = nextCh;
        nextCh = cc != size ? attValue.CharAt(cc) : 0;
        
        // if in literal just add ch to buffer
        if (inLiteral && (ch != endLiteral)) {
                buffer.Append(ch);
                continue;
        }
        switch ( ch ) {
            case '\'' :
            case '"' :
                buffer.Append(ch);
                if (inLiteral)
                    inLiteral = MB_FALSE;
                else if (inExpr) {
                    inLiteral = MB_TRUE;
                    endLiteral = ch;
                }
                break;
            case  '{' :
                if (!inExpr) {
                    // Ignore case where we find two {
                    if (nextCh == ch) {
                        buffer.Append(ch); //-- append '{'
                        cc++;
                        nextCh = cc != size ? attValue.CharAt(cc) : 0;
                    }
                    else {
                        if (!buffer.IsEmpty()) {
                            Expr* strExpr = new txLiteralExpr(buffer);
                            if (!strExpr) {
                                // XXX ErrorReport: out of memory
                                delete avt;
                                return 0;
                            }
                            avt->addExpr(strExpr);
                        }
                        buffer.Truncate();
                        inExpr = MB_TRUE;
                    }
                }
                else
                    buffer.Append(ch); //-- simply append '{'
                break;
            case '}':
                if (inExpr) {
                    inExpr = MB_FALSE;
                    txExprLexer lexer;
                    nsresult rv = lexer.parse(buffer);
                    if (NS_FAILED(rv)) {
                        delete avt;
                        return 0;
                    }
                    Expr* expr;
                    rv = createExpr(lexer, aContext, &expr);
                    if (NS_FAILED(rv)) {
                        delete avt;
                        return 0;
                    }
                    avt->addExpr(expr);
                    buffer.Truncate();
                }
                else if (nextCh == ch) {
                    buffer.Append(ch);
                    cc++;
                    nextCh = cc != size ? attValue.CharAt(cc) : 0;
                }
                else {
                    //XXX ErrorReport: unmatched '}' found
                    delete avt;
                    return 0;
                }
                break;
            default:
                buffer.Append(ch);
                break;
        }
    }

    if (inExpr) {
        //XXX ErrorReport: ending '}' missing
        delete avt;
        return 0;
    }

    if (!buffer.IsEmpty()) {
        Expr* strExpr = new txLiteralExpr(buffer);
        if (!strExpr) {
            // XXX ErrorReport: out of memory
            delete avt;
            return 0;
        }
        avt->addExpr(strExpr);
    }

    return avt;

}

nsresult
txExprParser::createExpr(const nsASingleFragmentString& aExpression,
                         txIParseContext* aContext, Expr** aExpr)
{
    NS_ENSURE_ARG_POINTER(aExpr);
    *aExpr = nsnull;
    txExprLexer lexer;
    nsresult rv = lexer.parse(aExpression);
    if (NS_FAILED(rv)) {
        nsASingleFragmentString::const_char_iterator start;
        aExpression.BeginReading(start);
        aContext->SetErrorOffset(lexer.mPosition - start);
        return rv;
    }
    rv = createExpr(lexer, aContext, aExpr);
    if (NS_SUCCEEDED(rv) && lexer.peek()->mType != Token::END) {
        delete *aExpr;
        *aExpr = nsnull;
        rv = NS_ERROR_XPATH_BINARY_EXPECTED;
    }
    if (NS_FAILED(rv)) {
        nsASingleFragmentString::const_char_iterator start;
        aExpression.BeginReading(start);
        aContext->SetErrorOffset(lexer.peek()->mStart - start);
    }
    return rv;
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
    *aResult = nsnull;

    Expr* expr = nsnull;
    switch (op->mType) {
        //-- additive ops
        case Token::ADDITION_OP :
            expr = new AdditiveExpr(left, right, AdditiveExpr::ADDITION);
            break;
        case Token::SUBTRACTION_OP:
            expr = new AdditiveExpr(left, right, AdditiveExpr::SUBTRACTION);
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

        //-- multiplicative ops
        case Token::DIVIDE_OP :
            expr = new MultiplicativeExpr(left, right,
                                          MultiplicativeExpr::DIVIDE);
            break;
        case Token::MODULUS_OP :
            expr = new MultiplicativeExpr(left, right,
                                          MultiplicativeExpr::MODULUS);
            break;
        case Token::MULTIPLY_OP :
            expr = new MultiplicativeExpr(left, right,
                                          MultiplicativeExpr::MULTIPLY);
            break;
        default:
            NS_NOTREACHED("operator tokens should be already checked");
            return NS_ERROR_UNEXPECTED;
    }
    NS_ENSURE_TRUE(expr, NS_ERROR_OUT_OF_MEMORY);

    *aResult = expr;
    return NS_OK;
}


nsresult
txExprParser::createExpr(txExprLexer& lexer, txIParseContext* aContext,
                         Expr** aResult)
{
    *aResult = nsnull;

    nsresult rv = NS_OK;
    MBool done = MB_FALSE;

    nsAutoPtr<Expr> expr;

    txStack exprs;
    txStack ops;
    
    while (!done) {

        MBool unary = MB_FALSE;
        while (lexer.peek()->mType == Token::SUBTRACTION_OP) {
            unary = !unary;
            lexer.nextToken();
        }

        rv = createUnionExpr(lexer, aContext, getter_Transfers(expr));
        if (NS_FAILED(rv)) {
            break;
        }

        if (unary) {
            Expr* uExpr = new UnaryExpr(expr);
            if (!uExpr) {
                rv = NS_ERROR_OUT_OF_MEMORY;
                break;
            }
            expr = uExpr;
        }

        Token* tok = lexer.nextToken();
        switch (tok->mType) {
            case Token::ADDITION_OP:
            case Token::DIVIDE_OP:
            //-- boolean ops
            case Token::AND_OP :
            case Token::OR_OP :
            //-- equality ops
            case Token::EQUAL_OP:
            case Token::NOT_EQUAL_OP:
            //-- relational ops
            case Token::LESS_THAN_OP:
            case Token::GREATER_THAN_OP:
            case Token::LESS_OR_EQUAL_OP:
            case Token::GREATER_OR_EQUAL_OP:
            //-- multiplicative ops
            case Token::MODULUS_OP:
            case Token::MULTIPLY_OP:
            case Token::SUBTRACTION_OP:
            {
                while (!exprs.isEmpty() && precedence(tok) 
                       <= precedence(NS_STATIC_CAST(Token*, ops.peek()))) {
                    // can't use expr as result due to order of evaluation
                    nsAutoPtr<Expr> left(NS_STATIC_CAST(Expr*, exprs.pop()));
                    nsAutoPtr<Expr> right(expr);
                    rv = createBinaryExpr(left, right,
                                          NS_STATIC_CAST(Token*, ops.pop()),
                                          getter_Transfers(expr));
                    if (NS_FAILED(rv)) {
                        break;
                    }
                }
                exprs.push(expr.forget());
                ops.push(tok);
                break;
            }
            default:
                lexer.pushBack();
                done = MB_TRUE;
                break;
        }
    }

    while (NS_SUCCEEDED(rv) && !exprs.isEmpty()) {
        nsAutoPtr<Expr> left(NS_STATIC_CAST(Expr*, exprs.pop()));
        nsAutoPtr<Expr> right(expr);
        rv = createBinaryExpr(left, right, NS_STATIC_CAST(Token*, ops.pop()),
                              getter_Transfers(expr));
    }
    // clean up on error
    while (!exprs.isEmpty()) {
        delete NS_STATIC_CAST(Expr*, exprs.pop());
    }
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = expr.forget();
    return NS_OK;
}

nsresult
txExprParser::createFilter(txExprLexer& lexer, txIParseContext* aContext,
                           Expr** aResult)
{
    *aResult = nsnull;

    nsresult rv = NS_OK;
    Token* tok = lexer.nextToken();

    nsAutoPtr<Expr> expr;
    switch (tok->mType) {
        case Token::FUNCTION_NAME :
            lexer.pushBack();
            rv = createFunctionCall(lexer, aContext, getter_Transfers(expr));
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        case Token::VAR_REFERENCE :
            {
                nsCOMPtr<nsIAtom> prefix, lName;
                PRInt32 nspace;
                nsresult rv = resolveQName(tok->Value(), getter_AddRefs(prefix),
                                           aContext, getter_AddRefs(lName),
                                           nspace);
                NS_ENSURE_SUCCESS(rv, rv);
                expr = new VariableRefExpr(prefix, lName, nspace);
                NS_ENSURE_TRUE(expr, NS_ERROR_OUT_OF_MEMORY);
            }
            break;
        case Token::L_PAREN:
            rv = createExpr(lexer, aContext, getter_Transfers(expr));
            NS_ENSURE_SUCCESS(rv, rv);

            if (lexer.nextToken()->mType != Token::R_PAREN) {
                lexer.pushBack();
                return NS_ERROR_XPATH_PAREN_EXPECTED;
            }
            break;
        case Token::LITERAL :
            expr = new txLiteralExpr(tok->Value());
            NS_ENSURE_TRUE(expr, NS_ERROR_OUT_OF_MEMORY);
            break;
        case Token::NUMBER:
        {
            expr = new txLiteralExpr(Double::toDouble(tok->Value()));
            NS_ENSURE_TRUE(expr, NS_ERROR_OUT_OF_MEMORY);
            break;
        }
        default:
            NS_NOTREACHED("internal error, this is not a filter token");
            lexer.pushBack();
            return NS_ERROR_UNEXPECTED;
    }

    if (lexer.peek()->mType == Token::L_BRACKET) {
        nsAutoPtr<FilterExpr> filterExpr(new FilterExpr(expr));
        NS_ENSURE_TRUE(filterExpr, NS_ERROR_OUT_OF_MEMORY);

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
    *aResult = nsnull;

    nsAutoPtr<FunctionCall> fnCall;

    Token* tok = lexer.nextToken();
    NS_ASSERTION(tok->mType == Token::FUNCTION_NAME, "FunctionCall expected");

    //-- compare function names
    nsCOMPtr<nsIAtom> prefix, lName;
    PRInt32 namespaceID;
    nsresult rv = resolveQName(tok->Value(), getter_AddRefs(prefix), aContext,
                               getter_AddRefs(lName), namespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    if (namespaceID == kNameSpaceID_None) {
        PRBool isOutOfMem = PR_TRUE;
        if (lName == txXPathAtoms::boolean) {
            fnCall = new BooleanFunctionCall(BooleanFunctionCall::TX_BOOLEAN);
        }
        else if (lName == txXPathAtoms::concat) {
            fnCall = new StringFunctionCall(StringFunctionCall::CONCAT);
        }
        else if (lName == txXPathAtoms::contains) {
            fnCall = new StringFunctionCall(StringFunctionCall::CONTAINS);
        }
        else if (lName == txXPathAtoms::count) {
            fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::COUNT);
        }
        else if (lName == txXPathAtoms::_false) {
            fnCall = new BooleanFunctionCall(BooleanFunctionCall::TX_FALSE);
        }
        else if (lName == txXPathAtoms::id) {
            fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::ID);
        }
        else if (lName == txXPathAtoms::lang) {
            fnCall = new BooleanFunctionCall(BooleanFunctionCall::TX_LANG);
        }
        else if (lName == txXPathAtoms::last) {
            fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::LAST);
        }
        else if (lName == txXPathAtoms::localName) {
            fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::LOCAL_NAME);
        }
        else if (lName == txXPathAtoms::name) {
            fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::NAME);
        }
        else if (lName == txXPathAtoms::namespaceUri) {
            fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::NAMESPACE_URI);
        }
        else if (lName == txXPathAtoms::normalizeSpace) {
            fnCall = new StringFunctionCall(StringFunctionCall::NORMALIZE_SPACE);
        }
        else if (lName == txXPathAtoms::_not) {
            fnCall = new BooleanFunctionCall(BooleanFunctionCall::TX_NOT);
        }
        else if (lName == txXPathAtoms::position) {
            fnCall = new NodeSetFunctionCall(NodeSetFunctionCall::POSITION);
        }
        else if (lName == txXPathAtoms::startsWith) {
            fnCall = new StringFunctionCall(StringFunctionCall::STARTS_WITH);
        }
        else if (lName == txXPathAtoms::string) {
            fnCall = new StringFunctionCall(StringFunctionCall::STRING);
        }
        else if (lName == txXPathAtoms::stringLength) {
            fnCall = new StringFunctionCall(StringFunctionCall::STRING_LENGTH);
        }
        else if (lName == txXPathAtoms::substring) {
            fnCall = new StringFunctionCall(StringFunctionCall::SUBSTRING);
        }
        else if (lName == txXPathAtoms::substringAfter) {
            fnCall = new StringFunctionCall(StringFunctionCall::SUBSTRING_AFTER);
        }
        else if (lName == txXPathAtoms::substringBefore) {
            fnCall = new StringFunctionCall(StringFunctionCall::SUBSTRING_BEFORE);
        }
        else if (lName == txXPathAtoms::sum) {
            fnCall = new NumberFunctionCall(NumberFunctionCall::SUM);
        }
        else if (lName == txXPathAtoms::translate) {
            fnCall = new StringFunctionCall(StringFunctionCall::TRANSLATE);
        }
        else if (lName == txXPathAtoms::_true) {
            fnCall = new BooleanFunctionCall(BooleanFunctionCall::TX_TRUE);
        }
        else if (lName == txXPathAtoms::number) {
            fnCall = new NumberFunctionCall(NumberFunctionCall::NUMBER);
        }
        else if (lName == txXPathAtoms::round) {
            fnCall = new NumberFunctionCall(NumberFunctionCall::ROUND);
        }
        else if (lName == txXPathAtoms::ceiling) {
            fnCall = new NumberFunctionCall(NumberFunctionCall::CEILING);
        }
        else if (lName == txXPathAtoms::floor) {
            fnCall = new NumberFunctionCall(NumberFunctionCall::FLOOR);
        }
        else {
            // didn't find functioncall here, fnCall should be null
            isOutOfMem = PR_FALSE;
        }
        if (!fnCall && isOutOfMem) {
            NS_ERROR("XPath FunctionLib failed on out-of-memory");
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    // check extension functions and xslt
    if (!fnCall) {
        rv = aContext->resolveFunctionCall(lName, namespaceID,
                                           *getter_Transfers(fnCall));

        if (rv == NS_ERROR_NOT_IMPLEMENTED) {
            // this should just happen for unparsed-entity-uri()
            NS_ASSERTION(!fnCall, "Now is it implemented or not?");
            rv = parseParameters(0, lexer, aContext);
            NS_ENSURE_SUCCESS(rv, rv);
            *aResult = new txLiteralExpr(tok->Value() +
                                         NS_LITERAL_STRING(" not implemented."));
            NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
            return NS_OK;
        }

        if (rv == NS_ERROR_XPATH_UNKNOWN_FUNCTION) {
            // Don't throw parse error, just error on evaluation
            fnCall = new txErrorFunctionCall(lName, namespaceID);
            NS_ENSURE_TRUE(fnCall, NS_ERROR_OUT_OF_MEMORY);
        }
        else if (NS_FAILED(rv)) {
            NS_ERROR("Creation of FunctionCall failed");
            return rv;
        }
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
    *aExpr = nsnull;

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
            if (axis == txXPathAtoms::ancestor) {
                axisIdentifier = LocationStep::ANCESTOR_AXIS;
            }
            else if (axis == txXPathAtoms::ancestorOrSelf) {
                axisIdentifier = LocationStep::ANCESTOR_OR_SELF_AXIS;
            }
            else if (axis == txXPathAtoms::attribute) {
                axisIdentifier = LocationStep::ATTRIBUTE_AXIS;
            }
            else if (axis == txXPathAtoms::child) {
                axisIdentifier = LocationStep::CHILD_AXIS;
            }
            else if (axis == txXPathAtoms::descendant) {
                axisIdentifier = LocationStep::DESCENDANT_AXIS;
            }
            else if (axis == txXPathAtoms::descendantOrSelf) {
                axisIdentifier = LocationStep::DESCENDANT_OR_SELF_AXIS;
            }
            else if (axis == txXPathAtoms::following) {
                axisIdentifier = LocationStep::FOLLOWING_AXIS;
            }
            else if (axis == txXPathAtoms::followingSibling) {
                axisIdentifier = LocationStep::FOLLOWING_SIBLING_AXIS;
            }
            else if (axis == txXPathAtoms::_namespace) {
                axisIdentifier = LocationStep::NAMESPACE_AXIS;
            }
            else if (axis == txXPathAtoms::parent) {
                axisIdentifier = LocationStep::PARENT_AXIS;
            }
            else if (axis == txXPathAtoms::preceding) {
                axisIdentifier = LocationStep::PRECEDING_AXIS;
            }
            else if (axis == txXPathAtoms::precedingSibling) {
                axisIdentifier = LocationStep::PRECEDING_SIBLING_AXIS;
            }
            else if (axis == txXPathAtoms::self) {
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
            NS_ENSURE_TRUE(nodeTest, NS_ERROR_OUT_OF_MEMORY);
            break;
        case Token::SELF_NODE :
            //-- eat token
            lexer.nextToken();
            axisIdentifier = LocationStep::SELF_AXIS;
            nodeTest = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
            NS_ENSURE_TRUE(nodeTest, NS_ERROR_OUT_OF_MEMORY);
            break;
        default:
            break;
    }

    //-- get NodeTest unless an AbbreviatedStep was found
    nsresult rv = NS_OK;
    if (!nodeTest) {
        tok = lexer.nextToken();

        switch (tok->mType) {
            case Token::CNAME :
                {
                    // resolve QName
                    nsCOMPtr<nsIAtom> prefix, lName;
                    PRInt32 nspace;
                    rv = resolveQName(tok->Value(), getter_AddRefs(prefix),
                                      aContext, getter_AddRefs(lName),
                                      nspace, PR_TRUE);
                    NS_ENSURE_SUCCESS(rv, rv);
                    switch (axisIdentifier) {
                        case LocationStep::ATTRIBUTE_AXIS:
                            nodeTest = new txNameTest(prefix, lName, nspace,
                                                      txXPathNodeType::ATTRIBUTE_NODE);
                            break;
                        default:
                            nodeTest = new txNameTest(prefix, lName, nspace,
                                                      txXPathNodeType::ELEMENT_NODE);
                            break;
                    }
                    NS_ENSURE_TRUE(nodeTest, NS_ERROR_OUT_OF_MEMORY);
                }
                break;
            default:
                lexer.pushBack();
                rv = createNodeTypeTest(lexer, getter_Transfers(nodeTest));
                NS_ENSURE_SUCCESS(rv, rv);
        }
    }
    
    nsAutoPtr<LocationStep> lstep(new LocationStep(nodeTest, axisIdentifier));
    NS_ENSURE_TRUE(lstep, NS_ERROR_OUT_OF_MEMORY);

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

    Token* nodeTok = lexer.nextToken();

    switch (nodeTok->mType) {
        case Token::COMMENT:
            nodeTest = new txNodeTypeTest(txNodeTypeTest::COMMENT_TYPE);
            break;
        case Token::NODE :
            nodeTest = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
            break;
        case Token::PROC_INST :
            nodeTest = new txNodeTypeTest(txNodeTypeTest::PI_TYPE);
            break;
        case Token::TEXT :
            nodeTest = new txNodeTypeTest(txNodeTypeTest::TEXT_TYPE);
            break;
        default:
            lexer.pushBack();
            return NS_ERROR_XPATH_NO_NODE_TYPE_TEST;
    }
    NS_ENSURE_TRUE(nodeTest, NS_ERROR_OUT_OF_MEMORY);

    if (lexer.nextToken()->mType != Token::L_PAREN) {
        lexer.pushBack();
        NS_NOTREACHED("txExprLexer doesn't generate nodetypetest without(");
        return NS_ERROR_UNEXPECTED;
    }
    if (nodeTok->mType == Token::PROC_INST &&
        lexer.peek()->mType == Token::LITERAL) {
        Token* tok = lexer.nextToken();
        nodeTest->setNodeName(tok->Value());
    }
    if (lexer.nextToken()->mType != Token::R_PAREN) {
        lexer.pushBack();
        return NS_ERROR_XPATH_PAREN_EXPECTED;
    }

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
    *aResult = nsnull;

    nsAutoPtr<Expr> expr;

    Token* tok = lexer.peek();

    // is this a root expression?
    if (tok->mType == Token::PARENT_OP) {
        lexer.nextToken();
        if (!isLocationStepToken(lexer.peek())) {
            *aResult = new RootExpr();
            NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
            return NS_OK;
        }
        lexer.pushBack();
    }

    // parse first step (possibly a FilterExpr)
    nsresult rv = NS_OK;
    if (tok->mType != Token::PARENT_OP &&
        tok->mType != Token::ANCESTOR_OP) {
        if (isFilterExprToken(tok)) {
            rv = createFilter(lexer, aContext, getter_Transfers(expr));
        }
        else {
            rv = createLocationStep(lexer, aContext, getter_Transfers(expr));
        }
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
        NS_ENSURE_TRUE(expr, NS_ERROR_OUT_OF_MEMORY);

#ifdef TX_TO_STRING
        NS_STATIC_CAST(RootExpr*, expr.get())->setSerialize(PR_FALSE);
#endif
    }
    
    // We have a PathExpr containing several steps
    nsAutoPtr<PathExpr> pathExpr(new PathExpr());
    NS_ENSURE_TRUE(pathExpr, NS_ERROR_OUT_OF_MEMORY);

    rv = pathExpr->addExpr(expr.forget(), PathExpr::RELATIVE_OP);
    NS_ENSURE_SUCCESS(rv, rv);

    // this is ugly
    while (1) {
        PathExpr::PathOperator pathOp;
        tok = lexer.nextToken();
        switch (tok->mType) {
            case Token::ANCESTOR_OP :
                pathOp = PathExpr::DESCENDANT_OP;
                break;
            case Token::PARENT_OP :
                pathOp = PathExpr::RELATIVE_OP;
                break;
            default:
                lexer.pushBack();
                *aResult = pathExpr.forget();
                return NS_OK;
        }
        
        rv = createLocationStep(lexer, aContext, getter_Transfers(expr));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = pathExpr->addExpr(expr.forget(), pathOp);
        NS_ENSURE_SUCCESS(rv, rv);
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
    *aResult = nsnull;

    nsAutoPtr<Expr> expr;
    nsresult rv = createPathExpr(lexer, aContext, getter_Transfers(expr));
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (lexer.peek()->mType != Token::UNION_OP) {
        *aResult = expr.forget();
        return NS_OK;
    }

    nsAutoPtr<UnionExpr> unionExpr(new UnionExpr());
    NS_ENSURE_TRUE(unionExpr, NS_ERROR_OUT_OF_MEMORY);

    rv = unionExpr->addExpr(expr.forget());
    NS_ENSURE_SUCCESS(rv, rv);

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

PRBool
txExprParser::isFilterExprToken(Token* aToken)
{
    switch (aToken->mType) {
        case Token::LITERAL:
        case Token::NUMBER:
        case Token::FUNCTION_NAME:
        case Token::VAR_REFERENCE:
        case Token::L_PAREN:            // grouping expr
            return PR_TRUE;
        default:
            return PR_FALSE;
    }
}

PRBool
txExprParser::isLocationStepToken(Token* aToken)
{
    switch (aToken->mType) {
        case Token::AXIS_IDENTIFIER :
        case Token::AT_SIGN :
        case Token::PARENT_NODE :
        case Token::SELF_NODE :
            return PR_TRUE;
        default:
            return isNodeTypeToken(aToken);
    }
}

PRBool
txExprParser::isNodeTypeToken(Token* aToken)
{
    switch (aToken->mType) {
        case Token::CNAME:
        case Token::COMMENT:
        case Token::NODE :
        case Token::PROC_INST :
        case Token::TEXT :
            return PR_TRUE;
        default:
            return PR_FALSE;
    }
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

        rv = aPredicateList->add(expr.forget());
        NS_ENSURE_SUCCESS(rv, rv);

        if (lexer.nextToken()->mType != Token::R_BRACKET) {
            lexer.pushBack();
            return NS_ERROR_XPATH_BRACKET_EXPECTED;
        }
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
    if (lexer.nextToken()->mType != Token::L_PAREN) {
        lexer.pushBack();
        NS_NOTREACHED("txExprLexer doesn't generate functions without(");
        return NS_ERROR_UNEXPECTED;
    }

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
                    
        switch (lexer.nextToken()->mType) {
            case Token::R_PAREN :
                return NS_OK;
            case Token::COMMA: //-- param separator
                break;
            default:
                lexer.pushBack();
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
                           nsIAtom** aLocalName, PRInt32& aNamespace,
                           PRBool aIsNameTest)
{
    aNamespace = kNameSpaceID_None;
    PRInt32 idx = aQName.FindChar(':');
    if (idx > 0) {
        *aPrefix = NS_NewAtom(Substring(aQName, 0, (PRUint32)idx));
        if (!*aPrefix) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        *aLocalName = NS_NewAtom(Substring(aQName, (PRUint32)idx + 1,
                                           aQName.Length() - (idx + 1)));
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
        TX_ToLowerCase(aQName, lcname);
        *aLocalName = NS_NewAtom(lcname);
    }
    else {
        *aLocalName = NS_NewAtom(aQName);
    }
    if (!*aLocalName) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

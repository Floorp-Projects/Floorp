/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 * 
 */

/**
 * ExprParser
 * This class is used to parse XSL Expressions
 * @see ExprLexer
**/

#ifndef MITREXSL_EXPRPARSER_H
#define MITREXSL_EXPRPARSER_H

#include "baseutils.h"
#include "nsIAtom.h"
#include "txError.h"
#include "Expr.h"

class AttributeValueTemplate;
class Expr;
class txExprLexer;
class FunctionCall;
class LocationStep;
class PredicateList;
class Token;
class txIParseContext;
class txNodeTypeTest;

class txExprParser
{
public:

    static nsresult createExpr(const nsASingleFragmentString& aExpression,
                               txIParseContext* aContext, Expr** aExpr);

    /**
     * Creates an Attribute Value Template using the given value
     */
    static AttributeValueTemplate* createAttributeValueTemplate
        (const nsAFlatString& attValue, txIParseContext* aContext);


protected:
    /**
     * Using nsAutoPtr& to optimize passing the ownership to the
     * created binary expression objects.
     */
    static nsresult createBinaryExpr(nsAutoPtr<Expr>& left,
                                     nsAutoPtr<Expr>& right, Token* op,
                                     Expr** aResult);
    static nsresult createExpr(txExprLexer& lexer, txIParseContext* aContext,
                               Expr** aResult);
    static nsresult createFilter(txExprLexer& lexer, txIParseContext* aContext,
                                 Expr** aResult);
    static nsresult createFunctionCall(txExprLexer& lexer,
                                       txIParseContext* aContext,
                                       Expr** aResult);
    static nsresult createLocationStep(txExprLexer& lexer,
                                       txIParseContext* aContext,
                                       Expr** aResult);
    static nsresult createNodeTypeTest(txExprLexer& lexer,
                                       txNodeTest** aResult);
    static nsresult createPathExpr(txExprLexer& lexer,
                                   txIParseContext* aContext,
                                   Expr** aResult);
    static nsresult createUnionExpr(txExprLexer& lexer,
                                    txIParseContext* aContext,
                                    Expr** aResult);
                  
    static PRBool isFilterExprToken(Token* aToken);
    static PRBool isLocationStepToken(Token* aToken);
    static PRBool isNodeTypeToken(Token* aToken);
                  
    static short precedence(Token* aToken);

    /**
     * Resolve a QName, given the mContext parse context.
     * Returns prefix and localName as well as namespace ID
     */
    static nsresult resolveQName(const nsAString& aQName, nsIAtom** aPrefix,
                                 txIParseContext* aContext,
                                 nsIAtom** aLocalName, PRInt32& aNamespace,
                                 PRBool aIsNameTest = MB_FALSE);

    /**
     * Using the given lexer, parses the tokens if they represent a
     * predicate list
     * If an error occurs a non-zero String pointer will be returned
     * containing the error message.
     * @param predicateList, the PredicateList to add predicate expressions to
     * @param lexer the ExprLexer to use for parsing tokens
     * @return 0 if successful, or a String pointer to the error message
     */
    static nsresult parsePredicates(PredicateList* aPredicateList,
                                    txExprLexer& lexer,
                                    txIParseContext* aContext);
    static nsresult parseParameters(FunctionCall* aFnCall, txExprLexer& lexer,
                                    txIParseContext* aContext);

};

#endif

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ExprParser
 * This class is used to parse XSL Expressions
 * @see ExprLexer
**/

#ifndef MITREXSL_EXPRPARSER_H
#define MITREXSL_EXPRPARSER_H

#include "txCore.h"
#include "nsAutoPtr.h"
#include "nsString.h"

class AttributeValueTemplate;
class Expr;
class txExprLexer;
class FunctionCall;
class LocationStep;
class nsIAtom;
class PredicateList;
class Token;
class txIParseContext;
class txNodeTest;
class txNodeTypeTest;

class txExprParser
{
public:

    static nsresult createExpr(const nsSubstring& aExpression,
                               txIParseContext* aContext, Expr** aExpr)
    {
        return createExprInternal(aExpression, 0, aContext, aExpr);
    }

    /**
     * Creates an Attribute Value Template using the given value
     */
    static nsresult createAVT(const nsSubstring& aAttrValue,
                              txIParseContext* aContext,
                              Expr** aResult);


protected:
    static nsresult createExprInternal(const nsSubstring& aExpression,
                                       PRUint32 aSubStringPos,
                                       txIParseContext* aContext,
                                       Expr** aExpr);
    /**
     * Using nsAutoPtr& to optimize passing the ownership to the
     * created binary expression objects.
     */
    static nsresult createBinaryExpr(nsAutoPtr<Expr>& left,
                                     nsAutoPtr<Expr>& right, Token* op,
                                     Expr** aResult);
    static nsresult createExpr(txExprLexer& lexer, txIParseContext* aContext,
                               Expr** aResult);
    static nsresult createFilterOrStep(txExprLexer& lexer,
                                       txIParseContext* aContext,
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
                  
    static bool isLocationStepToken(Token* aToken);
                  
    static short precedence(Token* aToken);

    /**
     * Resolve a QName, given the mContext parse context.
     * Returns prefix and localName as well as namespace ID
     */
    static nsresult resolveQName(const nsAString& aQName, nsIAtom** aPrefix,
                                 txIParseContext* aContext,
                                 nsIAtom** aLocalName, PRInt32& aNamespace,
                                 bool aIsNameTest = false);

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

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

/*
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
 * $Id: txExprParser.h,v 1.4 2005/11/02 07:33:54 peterv%netscape.com Exp $
 */

/**
 * ExprParser
 * This class is used to parse XSL Expressions
 * @author <A href="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @version $Revision: 1.4 $ $Date: 2005/11/02 07:33:54 $
 * @see ExprLexer
**/

#ifndef MITREXSL_EXPRPARSER_H
#define MITREXSL_EXPRPARSER_H

#include "TxString.h"
#include "ExprLexer.h"
#include "Expr.h"
#include "List.h"

class ExprParser {

public:

    static const String R_CURLY_BRACE;
    static const String L_CURLY_BRACE;

    /**
     * Creates a new ExprParser
    **/
    ExprParser();

    /**
     * destroys the ExprParser
    **/
    ~ExprParser();

    Expr*          createExpr        (const String& pattern);
    PatternExpr*   createPatternExpr (const String& pattern);
    LocationStep*  createLocationStep(const String& path);

    /**
     * Creates an Attribute Value Template using the given value
    **/
    AttributeValueTemplate* createAttributeValueTemplate(const String& attValue);


private:


    Expr*          createBinaryExpr   (Expr* left, Expr* right, Token* op);
    Expr*          createExpr         (ExprLexer& lexer);
    FilterExpr*    createFilterExpr   (ExprLexer& lexer);
    FunctionCall*  createFunctionCall (ExprLexer& lexer);
    LocationStep*  createLocationStep (ExprLexer& lexer);
    NodeExpr*      createNodeExpr     (ExprLexer& lexer);
    PathExpr*      createPathExpr     (ExprLexer& lexer);
    PatternExpr*   createPatternExpr  (ExprLexer& lexer);
    UnionExpr*     createUnionExpr    (ExprLexer& lexer);

    MBool          isFilterExprToken   (Token* tok);
    MBool          isLocationStepToken (Token* tok);
    MBool          isNodeTypeToken     (Token* tok);

    static short   precedenceLevel     (short tokenType);

    /**
     * Using the given lexer, parses the tokens if they represent a predicate list
     * If an error occurs a non-zero String pointer will be returned containing the
     * error message.
     * @param predicateList, the PredicateList to add predicate expressions to
     * @param lexer the ExprLexer to use for parsing tokens
     * @return 0 if successful, or a String pointer to the error message
    **/
    String* parsePredicates(PredicateList* predicateList, ExprLexer& lexer);
    String* parseParameters(List* list, ExprLexer& lexer);


}; //-- ExprParser

#endif

/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * ExprParser
 * This class is used to parse XSL Expressions
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
 * @see ExprLexer
**/

#ifndef MITREXSL_EXPRPARSER_H
#define MITREXSL_EXPRPARSER_H

#include "String.h"
#include "ExprLexer.h"
#include "Expr.h"
#include "FunctionLib.h"
#include "List.h"
#include "Names.h"
#include <iostream.h>

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

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
 * Test App for Expressions
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include "Expr.h"
#include "ExprLexer.h"
#include "ExprParser.h"
#include "String.h"
#include "NamedMap.h"
#include <iomanip.h>

void main(int argc, char** argv) {


    cout <<endl;
    //String pattern("element[position()=1]");
    //String pattern("*[text()='foo']");
    String pattern("@*|node()");

    cout <<"Lexically Analyzing: "<<pattern<<endl;
    cout<<endl;

    ExprLexer lexer(pattern);

    Token* token = 0;


    int tokenCount = lexer.countAllTokens();

    for (int i = 0; i < tokenCount; i++) {
        token = lexer.nextToken();
        cout <<"Token: ";
        if (token) {
            cout << token->value;

            //-- do padding
            for ( int i = token->value.length(); i < 20; i++) cout<<" ";

            //-- print token type
            switch (token->type) {
                case Token::AT_SIGN:
                    cout << "#AT_SIGN";
                    break;
                case Token::PARENT_OP :
                    cout<<"#PARENT_OP";
                    break;
                case Token::ANCESTOR_OP :
                    cout<<"#ANCESTOR_OP";
                    break;
                case Token::L_PAREN :
                    cout<<"#L_PAREN";
                    break;
                case Token::R_PAREN :
                    cout<<"#R_PAREN";
                    break;
                case Token::L_BRACKET:
                    cout<<"#L_BRACKET";
                    break;
                case Token::R_BRACKET:
                    cout<<"#R_BRACKET";
                    break;
                case Token::COMMA:
                    cout<<"#COMMA";
                    break;
                case Token::LITERAL:
                    cout<<"#LITERAL";
                    break;
                case Token::CNAME:
                    cout<<"#CNAME";
                    break;
                case Token::COMMENT:
                    cout<<"#COMMENT";
                    break;
                case Token::NODE:
                    cout<<"#NODE";
                    break;
                case Token::PI:
                    cout<<"#PI";
                    break;
                case Token::TEXT:
                    cout<<"#TEXT";
                    break;
                case Token::FUNCTION_NAME:
                    cout<<"#FUNCTION_NAME";
                    break;
                case Token::WILD_CARD:
                    cout << "#WILDCARD";
                    break;
                case Token::NUMBER:
                    cout << "#NUMBER";
                    break;
                case Token::PARENT_NODE :
                    cout << "#PARENT_NODE";
                    break;
                case Token::SELF_NODE :
                    cout << "#SELF_NODE";
                    break;
                case Token::VAR_REFERENCE:
                    cout << "#VAR_REFERENCE";
                case Token::AXIS_IDENTIFIER:
                    cout << "#AXIS_IDENTIFIER";
                    break;
                default:
                    if ( lexer.isOperatorToken(token) ) {
                        cout << "#operator";
                    }
                    else cout<<"#unknown";
                    break;
            }
        }
        else cout<<"NULL";
        cout<<endl;
    }


    cout <<endl;

    cout <<"Creating Expr: "<<pattern<<endl;
    cout<<endl;

    ExprParser parser;

    Expr* expr = (Expr*)parser.createExpr(pattern);

    cout << "Checking result"<<endl;

    String resultString;

    if ( !expr ) {
        cout << "#error, "<<pattern<< " is not valid."<<endl;
    }
    else {
        expr->toString(resultString);
        cout << "result: "<<resultString <<endl;
    }

    NumberExpr numberExpr( -24.0 );
    resultString.clear();
    numberExpr.toString(resultString);
    cout << "NumberExpr: " << resultString << endl << endl;
    StringResult strResult("-24");
    cout << "StringResult: " << strResult.numberValue() << endl;

    //-- AttributeValueTemplate test
    String avt("this is a {text()} of {{{{attr value templates}}}}");
    expr = parser.createAttributeValueTemplate(avt);
    resultString.clear();
    expr->toString(resultString);
    cout << "result of avt: "<<resultString <<endl;
    //-- clean up
    delete expr;

} //-- main


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
 * $Id: Parser.cpp,v 1.4 2000/07/23 07:25:29 kvisco%ziplink.net Exp $
 */

/**
 * Test App for Expressions
 * @version $Revision: 1.4 $ $Date: 2000/07/23 07:25:29 $
**/

#include <iostream.h>
#include "TxString.h"
#include "Expr.h"
#include "ExprLexer.h"
#include "ExprParser.h"
#include "NamedMap.h"
#include "ExprResult.h"

void main(int argc, char** argv) {


    cout <<endl;

    //-- old test cases, commented for re-use
    //String pattern("element[position()=1]");
    //String pattern("*[text()='foo']");
    //String pattern("@*|node()");
    //String pattern("FSContext/UserList/User[@id=/FSContext/SessionData/@userref]/@priv = 'admin'");

    //String pattern("10/3");
	//String pattern("*[1]foo|*[1]/bar|*[1]/baz");
    
    //-- Current Test
    String pattern("(4+5)-(9+9)");

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

    cout << endl << "----------------------" << endl << endl;

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


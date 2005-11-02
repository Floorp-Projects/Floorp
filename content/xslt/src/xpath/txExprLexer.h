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
 *    -- original author.
 *
 * Larry Fitzpatrick
 *    -- changed constant short declarations in Token and ExprLexer to
 *       enumerations, commented with //--LF
 *
 */


#ifndef MITREXSL_EXPRLEXER_H
#define MITREXSL_EXPRLEXER_H

#include "TxString.h"
#include "baseutils.h"

/**
 * A Token class for the ExprLexer.
 *
 * This class was ported from XSL:P, an open source Java based 
 * XSLT processor, written by yours truly.
**/
class Token {

public:

    //---------------/
    //- Token Types -/
    //---------------/

    //-- LF - changed from static const short declarations to enum
    //-- token types
    enum TokenType {
        //-- Trivial Tokens
        ERROR = 0,
        NULL_TOKEN,
        LITERAL,
        NUMBER,
        CNAME,
        FUNCTION_NAME,
        VAR_REFERENCE,
        PARENT_NODE,
        SELF_NODE,
        R_PAREN,
        R_BRACKET, // 10
        /**
         * start of tokens for 3.7, bullet 1
         * ExprLexer::nextIsOperatorToken bails if the tokens aren't
         * consecutive.
         **/
        COMMA,
        AT_SIGN,
        L_PAREN,
        L_BRACKET,
        AXIS_IDENTIFIER,
          //-------------/
         //- operators -/
        //-------------/

        //-- boolean ops
        AND_OP, // 16
        OR_OP,

        //-- relational
        EQUAL_OP, // 18
        NOT_EQUAL_OP,
        LESS_THAN_OP,
        GREATER_THAN_OP,
        LESS_OR_EQUAL_OP,
        GREATER_OR_EQUAL_OP,
        //-- additive operators
        ADDITION_OP, // 24
        SUBTRACTION_OP,
        //-- multiplicative
        DIVIDE_OP , // 26
        MULTIPLY_OP,
        MODULUS_OP,
        //-- path operators
        PARENT_OP, // 29
        ANCESTOR_OP,
        UNION_OP,
        /**
         * end of tokens for 3.7, bullet 1 -/
         **/
        //-- node type tokens
        COMMENT, // 32
        NODE,
        PROC_INST,
        TEXT,
        
        //-- Special endtoken
        END // 36
    };


    /**
     * Default Constructor
    **/
    Token();
    Token(short type);
    Token(const String& value, short type);
    Token(PRUnichar uniChar, short type);
    /**
     * Copy Constructor
    **/
    Token(const Token& token);

    ~Token();
    String value;
    short type;
}; //--Token

/**
 * A class for splitting an "Expr" String into tokens and
 * performing  basic Lexical Analysis.
 *
 * This class was ported from XSL:P, an open source Java based XSL processor
**/
class ExprLexer {


public:

    /*
     * Trivial Tokens
    */
    //-- LF, changed to enum
    enum _TrivialTokens {
        D_QUOTE        = '\"',
        S_QUOTE        = '\'',
        L_PAREN        = '(',
        R_PAREN        = ')',
        L_BRACKET      = '[',
        R_BRACKET      = ']',
        L_ANGLE        = '<',
        R_ANGLE        = '>',
        COMMA          = ',',
        PERIOD         = '.',
        ASTERIX        = '*',
        FORWARD_SLASH  = '/',
        EQUAL          = '=',
        BANG           = '!',
        VERT_BAR       = '|',
        AT_SIGN        = '@',
        DOLLAR_SIGN    = '$',
        PLUS           = '+',
        HYPHEN         = '-',
        COLON          = ':',
        //-- whitespace tokens
        SPACE          = ' ',
        TX_TAB            = '\t',
        TX_CR             = '\n',
        TX_LF             = '\r'
    };

    enum _error_consts {
        ERROR_UNRESOLVED_VAR_REFERENCE = 0,
        ERROR_OP_EXPECTED,
        ERROR_UNCLOSED_LITERAL,
        ERROR_COLON,
        ERROR_BANG,
        ERROR_UNKNOWN_CHAR
    };
    static const String error_message[];
    PRUint32 errorPos;
    short errorCode;

    /*
     * Complex Tokens
    */
    //-- Nodetype tokens
    static const String COMMENT;
    static const String NODE;
    static const String PROC_INST;
    static const String TEXT;

    //-- boolean
    static const String AND;
    static const String OR;

    //-- Multiplicative
    static const String MODULUS;
    static const String DIVIDE;

    /*
     * Default Token Set
    */
    static const Token TOKENS[];
    static const short NUMBER_OF_TOKENS;

    /**
     * Constructor for ExprLexer
    **/
    ExprLexer(const String& pattern);

    ~ExprLexer();

    /**
     * Functions for iterating over the TokenList
    **/

    Token* nextToken();
    Token* peek();
    void   pushBack();
    MBool  hasMoreTokens();

private:

    struct TokenListItem {
        Token* token;
        TokenListItem* next;
        TokenListItem* previous;
    };

    TokenListItem* currentItem;
    TokenListItem* firstItem;
    TokenListItem* lastItem;

    int tokenCount;

    Token* prevToken;
    Token endToken;

    void addToken(Token* token);

    /**
     * Returns true if the following Token should be an operator.
     * This is a helper for the first bullet of [XPath 3.7]
     *  Lexical Structure
     **/
    MBool nextIsOperatorToken(Token* token);

    /**
     * Returns true if the given character represents a numeric letter (digit)
     * Implemented in ExprLexerChars.cpp
    **/
    static MBool isXPathDigit(PRUnichar ch)
    {
        return (ch >= '0' && ch <= '9');
    }

    String subStr;
    void parse(const String& pattern);

}; //-- ExprLexer

#endif


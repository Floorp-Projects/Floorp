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


#ifndef MITREXSL_EXPRLEXER_H
#define MITREXSL_EXPRLEXER_H

#include "String.h"
#include "baseutils.h"
#include <iostream.h>

/**
 * A Token class for the ExprLexer.
 * <BR>This class was ported from XSL:P, an open source Java based XSL processor
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
 * <BR/>
 * <PRE>
 * Modifications:
 * 19990806: Larry Fitzpatrick
 *   - changed constant short declarations in Token and ExprLexer to
 *     enumerations, commented with //--LF
 * </PRE>
 *
**/
class Token {

public:

    //---------------/
    //- Token Types -/
    //---------------/

    //-- LF - changed from static const short declarations to enum
    //-- token types
    enum _TokenTypes {
        //-- Trivial Tokens
        ERROR = 0,
        NULL_TOKEN,
        LITERAL,
        NUMBER,
        CNAME,
        L_PAREN,
        R_PAREN,
        L_BRACKET,
        R_BRACKET,
        COMMA,
        FUNCTION_NAME,
        WILD_CARD,
        AT_SIGN,
        VAR_REFERENCE,
        PARENT_NODE,
        SELF_NODE,
        AXIS_IDENTIFIER,
          //-------------/
         //- operators -/
        //-------------/
        //-- relational
        EQUAL_OP,
        NOT_EQUAL_OP,
        LESS_THAN_OP,
        GREATER_THAN_OP,
        LESS_OR_EQUAL_OP,
        GREATER_OR_EQUAL_OP,
        //-- additive operators
        ADDITION_OP,
        SUBTRACTION_OP,
        //-- multiplicative
        DIVIDE_OP ,
        MULTIPLY_OP,
        MODULUS_OP,
        //-- path operators
        PARENT_OP,
        ANCESTOR_OP,
        UNION_OP,
        //-- node type tokens
        COMMENT,
        NODE,
        PI,
        TEXT
    };


    /**
     * Default Constructor
    **/
    Token();
    Token(short type);
    Token(const String& value, short type);
    Token(UNICODE_CHAR uniChar, short type);
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
 * <BR>This class was ported from XSL:P, an open source Java based XSL processor
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
class ExprLexer {


public:

    /*
     * Trivial Tokens
    */
    //-- LF, changed to enum
    enum _TrivialTokens {
        S_QUOTE        = '\'',
        L_PAREN        = '(',
        R_PAREN        = ')',
        L_BRACKET      = '[',
        R_BRACKET      = ']',
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
        TAB            = '\t',
        CR             = '\n',
        LF             = '\r'
    };


    /*
     * Complex Tokens
    */
    //-- Nodetype tokens
    static const String COMMENT;
    static const String NODE;
    static const String PI;
    static const String TEXT;

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
     * Counts the total number of tokens in this Lexer, even if the token
     * has already been seen
     * @return the total number of tokens in this Lexer
    **/
    int countAllTokens();

    /**
     * Counts the remaining number of tokens in this Lexer
     * @return the number of remaining tokens in this Lexer
    **/
    int countRemainingTokens();

    /**
     * Returns the type of token that was last return by a call to nextToken
    **/

    Token* lookAhead(int offset);
    Token* nextToken();
    Token* peek();
    void   pushBack();
    MBool  hasMoreTokens();

    MBool isOperatorToken(Token* token);

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

    void addToken(Token* token);

    /**
     * Returns true if the given character represents an Alpha letter
    **/
    static MBool isAlphaChar(Int32 ch);

    /**
     * Returns true if the given character represents a numeric letter (digit)
    **/
    static MBool isDigit(Int32 ch);

    /**
     * Returns true if the given character is an allowable QName character
    **/
    static MBool isQNameChar(Int32 ch);

    /**
     * Returns true if the given character is an allowable NCName character
    **/
    static MBool isNCNameChar(Int32 ch);

    /**
     * Returns true if the given String is a valid XML QName
    **/
    static MBool isValidQName(String& name);

    MBool matchDelimiter(UNICODE_CHAR ch);

    /**
     * Returns true if the value of the given String matches
     * an OperatorName
    **/
    MBool matchesOperator(String& buffer);

    /**
     * Matches the given String to the appropriate Token
     * @param buffer the current StringBuffer representing the value of the Token
     * @param ch, the current delimiter token
    **/
    void matchToken(String& buffer, UNICODE_CHAR ch);


    void parse(const String& pattern);

}; //-- ExprLexer

#endif

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
 *    -- original author.
 *
 * Larry Fitzpatrick
 *    -- changed constant short declarations in Token and ExprLexer to
 *       enumerations, commented with //--LF
 * 
 * $Id: txExprLexer.h,v 1.4 2005/11/02 07:33:52 axel%pike.org Exp $
 */


#ifndef MITREXSL_EXPRLEXER_H
#define MITREXSL_EXPRLEXER_H

#include "TxString.h"
#include "baseutils.h"
#ifndef MOZ_XSL
#include <iostream.h>
#endif

/**
 * A Token class for the ExprLexer.
 * <BR />
 * This class was ported from XSL:P, an open source Java based 
 * XSLT processor, written by yours truly.
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.4 $ $Date: 2005/11/02 07:33:52 $
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

        //-- boolean ops
        AND_OP,
        OR_OP,

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


    /*
     * Complex Tokens
    */
    //-- Nodetype tokens
    static const String COMMENT;
    static const String NODE;
    static const String PI;
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


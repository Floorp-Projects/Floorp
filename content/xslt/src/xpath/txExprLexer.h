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
 *   Axel Hecht <axel@pike.org>
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


#ifndef MITREXSL_EXPRLEXER_H
#define MITREXSL_EXPRLEXER_H

#include "txCore.h"
#include "nsString.h"

/**
 * A Token class for the ExprLexer.
 *
 * This class was ported from XSL:P, an open source Java based 
 * XSLT processor, written by yours truly.
 */
class Token
{
public:

    /**
     * Token types
     */
    enum Type {
        //-- Trivial Tokens
        NULL_TOKEN = 1,
        LITERAL,
        NUMBER,
        CNAME,
        VAR_REFERENCE,
        PARENT_NODE,
        SELF_NODE,
        R_PAREN,
        R_BRACKET, // 9
        /**
         * start of tokens for 3.7, bullet 1
         * ExprLexer::nextIsOperatorToken bails if the tokens aren't
         * consecutive.
         */
        COMMA,
        AT_SIGN,
        L_PAREN,
        L_BRACKET,
        AXIS_IDENTIFIER,

        // These tokens include their following left parenthesis
        FUNCTION_NAME_AND_PAREN, // 15
        COMMENT_AND_PAREN,
        NODE_AND_PAREN,
        PROC_INST_AND_PAREN,
        TEXT_AND_PAREN,

        /**
         * operators
         */
        //-- boolean ops
        AND_OP, // 20
        OR_OP,

        //-- relational
        EQUAL_OP, // 22
        NOT_EQUAL_OP,
        LESS_THAN_OP,
        GREATER_THAN_OP,
        LESS_OR_EQUAL_OP,
        GREATER_OR_EQUAL_OP,
        //-- additive operators
        ADDITION_OP, // 28
        SUBTRACTION_OP,
        //-- multiplicative
        DIVIDE_OP, // 30
        MULTIPLY_OP,
        MODULUS_OP,
        //-- path operators
        PARENT_OP, // 33
        ANCESTOR_OP,
        UNION_OP,
        /**
         * end of tokens for 3.7, bullet 1 -/
         */
        //-- Special endtoken
        END // 36
    };


    /**
     * Constructors
     */
    typedef nsASingleFragmentString::const_char_iterator iterator;

    Token(iterator aStart, iterator aEnd, Type aType)
        : mStart(aStart),
          mEnd(aEnd),
          mType(aType),
          mNext(nsnull),
          mPrevious(nsnull)
    {
    }
    Token(iterator aChar, Type aType)
        : mStart(aChar),
          mEnd(aChar + 1),
          mType(aType),
          mNext(nsnull),
          mPrevious(nsnull)
    {
    }

    const nsDependentSubstring Value()
    {
        return Substring(mStart, mEnd);
    }

    iterator mStart, mEnd;
    Type mType;
    Token* mNext;
    // XXX mPrevious needed for pushBack(), do we pushBack more than once?
    Token* mPrevious;
};

/**
 * A class for splitting an "Expr" String into tokens and
 * performing  basic Lexical Analysis.
 *
 * This class was ported from XSL:P, an open source Java based XSL processor
 */

class txExprLexer
{
public:

    txExprLexer();
    ~txExprLexer();

    /**
     * Parse the given string.
     * returns an error result if lexing failed.
     * The given string must outlive the use of the lexer, as the
     * generated Tokens point to Substrings of it.
     * mPosition points to the offending location in case of an error.
     */
    nsresult parse(const nsASingleFragmentString& aPattern);

    typedef nsASingleFragmentString::const_char_iterator iterator;
    iterator mPosition;

    /**
     * Functions for iterating over the TokenList
     */

    Token* nextToken();
    Token* peek()
    {
        return mCurrentItem;
    }
    void pushBack();
    bool hasMoreTokens()
    {
        return (mCurrentItem->mType != Token::END);
    }

    /**
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

private:

    Token* mCurrentItem;
    Token* mFirstItem;
    Token* mLastItem;

    int mTokenCount;

    void addToken(Token* aToken);

    /**
     * Returns true if the following Token should be an operator.
     * This is a helper for the first bullet of [XPath 3.7]
     *  Lexical Structure
     */
    bool nextIsOperatorToken(Token* aToken);

    /**
     * Returns true if the given character represents a numeric letter (digit)
     * Implemented in ExprLexerChars.cpp
     */
    static bool isXPathDigit(PRUnichar ch)
    {
        return (ch >= '0' && ch <= '9');
    }
};

#endif


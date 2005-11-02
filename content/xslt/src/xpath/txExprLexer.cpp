/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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
 *   -- fixed bug with '<=' and '>=' reported by Bob Miller
 *
 * Bob Miller, Oblix Inc., kbob@oblix.com
 *   -- fixed bug with single quotes inside double quotes
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *   -- Fixed bug in parse method so that we make sure we check for
 *      axis identifier wild cards, such as ancestor::*
 *
 * Axel Hecht <axel@pike.org>
 *   -- big beating, general overhaul
 *
 */

/**
 * Lexical analyzer for XPath expressions
**/

#include "ExprLexer.h"

  //---------------------------/
 //- Implementation of Token -/
//---------------------------/


/**
 * Default constructor for Token
**/
Token::Token()
{
  this->type =0;
} //-- Token;

/**
 * Constructor for Token
 * @param type, the type of Token being represented
**/
Token::Token(short type)
{
  this->type = type;
} //-- Token;

/**
 *  Constructor for Token
 * @param value the value of this Token
 * @param type, the type of Token being represented
**/
Token::Token(const String& value, short type)
{
  this->type = type;
  //-- make copy of value String
  this->value = value;
} //-- Token

Token::Token(UNICODE_CHAR uniChar, short type)
{
  this->type = type;
  this->value.append(uniChar);
} //-- Token

/**
 * Copy Constructor
**/
Token::Token(const Token& token)
{
  this->type = token.type;
  this->value = token.value;
} //-- Token

/**
 * Destructor for Token
**/
Token::~Token()
{
  //-- currently nothing is needed
} //-- ~Token


  //--------------------------------/
 //- Implementation of ExprLexer -/
//-------------------------------/

/*
 * Complex Tokens
*/
//-- Nodetype tokens
const String ExprLexer::COMMENT   = "comment";
const String ExprLexer::NODE      = "node";
const String ExprLexer::PROC_INST = "processing-instruction";
const String ExprLexer::TEXT      = "text";

//-- boolean
const String ExprLexer::AND     = "and";
const String ExprLexer::OR      = "or";

//-- multiplicative operators
const String ExprLexer::MODULUS = "mod";
const String ExprLexer::DIVIDE  = "div";

/**
 * The set of Lexer error messages
 **/
const String ExprLexer::error_message[] =
{
  "VariableReference expected",
  "Operator expected",
  "Literal is not closed",
  ": not expected",
  "! not expected, use != or not()",
  "found a unkown character"
};

  //---------------/
 //- Contructors -/
//---------------/

/**
 * Creates a new ExprLexer using the given String
**/
ExprLexer::ExprLexer(const String& pattern)
{
  firstItem    = 0;
  lastItem     = 0;
  tokenCount   = 0;
  prevToken    = 0;
  endToken.type = Token::END;
  parse(pattern);
  currentItem = firstItem;
} //-- ExprLexer

/**
 * Destroys this instance of an ExprLexer
**/
ExprLexer::~ExprLexer()
{
  //-- delete tokens
  currentItem = firstItem;
  while (currentItem) {
    TokenListItem* temp = currentItem->next;
    delete currentItem->token;
    delete currentItem;
    currentItem = temp;
  }
} //-- ~ExprLexer


MBool ExprLexer::hasMoreTokens()
{
  return (currentItem != 0);
} //-- hasMoreTokens

Token* ExprLexer::nextToken()
{
  if (currentItem) {
    Token* token = currentItem->token;
    currentItem = currentItem->next;
    return token;
  }
  return &endToken;
} //-- nextToken

void ExprLexer::pushBack()
{
  if (!currentItem)
    currentItem = lastItem;
  else 
    currentItem = currentItem->previous;
} //-- pushBack

Token* ExprLexer::peek()
{
  if (currentItem)
    return currentItem->token;
  return &endToken;
} //-- peek

void ExprLexer::addToken(Token* token)
{
  TokenListItem* tlItem = new TokenListItem;
  tlItem->token = token;
  tlItem->next  = 0;
  if (lastItem) {
    tlItem->previous = lastItem;
    lastItem->next = tlItem;
  }
  if (!firstItem)
    firstItem = tlItem;
  lastItem = tlItem;
  prevToken = token;
  ++tokenCount;
} //-- addToken

/**
 * Returns true if the following Token should be an operator.
 * This is a helper for the first bullet of [XPath 3.7]
 *  Lexical Structure
**/
MBool ExprLexer::nextIsOperatorToken(Token* token)
{
  if (!token || token->type == Token::NULL_TOKEN)
    return MB_FALSE;
  /* This relies on the tokens having the right order in ExprLexer.h */
  if (token->type >= Token::COMMA && 
      token->type <= Token::UNION_OP)
    return MB_FALSE;
  return MB_TRUE;
} //-- nextIsOperatorToken

/**
 *  Parses the given String into the set of Tokens
**/
void ExprLexer::parse(const String& pattern)
{
  if (pattern.isEmpty())
    return;

  String tokenBuffer;
  PRInt32 iter = 0, start;
  PRInt32 size = pattern.length();
  short defType;
  UNICODE_CHAR ch;

  //-- initialize previous token, this will automatically get
  //-- deleted when it goes out of scope
  Token nullToken('\0', Token::NULL_TOKEN);

  prevToken = &nullToken;

  while (iter < size) {

    ch = pattern.charAt(iter);
    defType = Token::CNAME;

    if (ch==DOLLAR_SIGN) {
      if (++iter == size || !isLetter(ch=pattern.charAt(iter))) {
        // Error, VariableReference expected
        errorPos = iter;
        errorCode = ERROR_UNRESOLVED_VAR_REFERENCE;
        if (firstItem)
          firstItem->token->type=Token::ERROR;
        else
          addToken(new Token('\0',Token::ERROR));
        iter=size; // bail
      }
      else
        defType = Token::VAR_REFERENCE;
    } 
    // just reuse the QName parsing, which will use defType 
    // the token to construct

    if (isLetter(ch)) {
      // NCName, can get QName or OperatorName;
      //  FunctionName, NodeName, and AxisSpecifier may want whitespace,
      //  and are dealt with below
      start = iter;
      while (++iter < size && 
             isNCNameChar(pattern.charAt(iter))) /* just go */ ;
      PRInt32 end = iter;
      if (pattern.charAt(iter)==COLON) {
        // try QName or wildcard, might need to step back for axis
        if (++iter < size)
          if (isLetter(pattern.charAt(iter)))
            while (++iter < size && 
                  isNCNameChar(pattern.charAt(iter))) /* just go */ ;
          else if (pattern.charAt(iter)=='*' 
                   && defType != Token::VAR_REFERENCE)
            ++iter; /* eat wildcard for NameTest, bail for var ref at COLON */
          else 
            iter--; // step back
      }
      if (nextIsOperatorToken(prevToken)) {
        if (pattern.subString(start,end,subStr).isEqual(AND))
          defType = Token::AND_OP;
        else if (pattern.subString(start,end,subStr).isEqual(OR))
          defType = Token::OR_OP;
        else if (pattern.subString(start,end,subStr).isEqual(MODULUS))
          defType = Token::MODULUS_OP;
        else if (pattern.subString(start,end,subStr).isEqual(DIVIDE))
          defType = Token::DIVIDE_OP;
        else {
          // Error "operator expected"
          // XXX QUESTION: spec is not too precise
          // badops is sure an error, but is bad:ops, too? We say yes!
          errorPos = iter;
          errorCode = ERROR_OP_EXPECTED;
          if (firstItem)
            firstItem->token->type=Token::ERROR;
          else
            addToken(new Token('\0',Token::ERROR));
          iter=size; // bail
        }
      }
      addToken(new Token(pattern.subString(start,iter,subStr),defType));
    }
    else if (isDigit(ch)) {
      start = iter;
      while (++iter < size && 
             isDigit(pattern.charAt(iter))) /* just go */;
      if (pattern.charAt(iter)=='.')
        while (++iter < size && 
               isDigit(pattern.charAt(iter))) /* just go */;
      addToken(new Token(pattern.subString(start,iter,subStr),Token::NUMBER));
    }
    else {
      switch (ch) {
        //-- ignore whitespace
      case SPACE:
      case TX_TAB:
      case TX_CR:
      case TX_LF:
        ++iter;
        break;
      case S_QUOTE :
      case D_QUOTE :
        start=iter;
        iter = pattern.indexOf(ch,start+1);
        if (iter==NOT_FOUND) {
          // XXX Error reporting "unclosed literal"
          errorPos = start;
          errorCode = ERROR_UNCLOSED_LITERAL;
          if (firstItem)
            firstItem->token->type=Token::ERROR;
          else
            addToken(new Token('\0',Token::ERROR));
          iter=size; // bail
        }
        else {
          addToken(new Token(pattern.subString(start+1,iter,subStr),
                             Token::LITERAL));
          ++iter;
        }
        break;
      case PERIOD:
        // period can be .., .(DIGITS)+ or ., check next
        if (++iter < size) {
          ch=pattern.charAt(iter);
          if (isDigit(ch)) {
            start=iter-1;
            while (++iter < size && 
                   isDigit(pattern.charAt(iter))) /* just go */;
            addToken(new Token(pattern.subString(start,iter,subStr),
                               Token::NUMBER));
          }
          else if (ch==PERIOD) {
            addToken(new Token(pattern.subString(iter-1,iter++,subStr),
                               Token::PARENT_NODE));
          }
          else
            addToken(new Token(PERIOD, Token::SELF_NODE));
        }
        else
          addToken(new Token(ch, Token::SELF_NODE));
        // iter++ is already in the number test
        
        break;
      case COLON: // QNames are dealt above, must be axis ident
        if (++iter < size && pattern.charAt(iter)==COLON &&
            prevToken->type == Token::CNAME) {
          prevToken->type = Token::AXIS_IDENTIFIER;
          ++iter;
        }
        else {
          // XXX Error report "colon is neither QName nor axis"
          errorPos = iter;
          errorCode = ERROR_COLON;
          if (firstItem)
            firstItem->token->type=Token::ERROR;
          else
            addToken(new Token('\0',Token::ERROR));
          iter=size; // bail
        }
        break;
      case FORWARD_SLASH :
        if (++iter < size && pattern.charAt(iter)==ch) {
          addToken(new Token(pattern.subString(iter-1,++iter,subStr),
                             Token::ANCESTOR_OP));
        }
        else {
          addToken(new Token(ch, Token::PARENT_OP));
        }
        break;
      case BANG : // can only be !=
        if (++iter < size && pattern.charAt(iter)==EQUAL) {
          addToken(new Token(pattern.subString(iter-1,++iter,subStr),
                             Token::NOT_EQUAL_OP));
        }
        else {
          // Error ! is not not()
          errorPos = iter;
          errorCode = ERROR_BANG;
          if (firstItem)
            firstItem->token->type=Token::ERROR;
          else
            addToken(new Token('\0',Token::ERROR));
          iter=size; // bail
        }
        break;
      case EQUAL:
        addToken(new Token(ch,Token::EQUAL_OP));
        ++iter;
        break;
      case L_ANGLE:
        if (++iter < size && pattern.charAt(iter)==EQUAL) {
          addToken(new Token(pattern.subString(iter-1,++iter,subStr),
                             Token::LESS_OR_EQUAL_OP));
        }
        else
          addToken(new Token(ch,Token::LESS_THAN_OP));
        break;
      case R_ANGLE:
        if (++iter < size && pattern.charAt(iter)==EQUAL) {
          addToken(new Token(pattern.subString(iter-1,++iter,subStr),
                             Token::GREATER_OR_EQUAL_OP));
        }
        else
          addToken(new Token(ch,Token::GREATER_THAN_OP));
        break;
      case HYPHEN :
        addToken(new Token(ch,Token::SUBTRACTION_OP));
        ++iter;
        break;
      case ASTERIX:
        if (nextIsOperatorToken(prevToken))
          addToken(new Token(ch,Token::MULTIPLY_OP));
        else
          addToken(new Token(ch,Token::CNAME));
        ++iter;
        break;
      case L_PAREN:
        if (prevToken->type == Token::CNAME) {
          if (prevToken->value.isEqual(COMMENT))
            prevToken->type = Token::COMMENT;
          else if (prevToken->value.isEqual(NODE))
            prevToken->type = Token::NODE;
          else if (prevToken->value.isEqual(PROC_INST))
            prevToken->type = Token::PROC_INST;
          else if (prevToken->value.isEqual(TEXT))
            prevToken->type = Token::TEXT;
          else
            prevToken->type = Token::FUNCTION_NAME;
        }
        ++iter;
        addToken(new Token(ch,Token::L_PAREN));
        break;
      case R_PAREN:
        ++iter;
        addToken(new Token(ch,Token::R_PAREN));
        break;
      case L_BRACKET:
        ++iter;
        addToken(new Token(ch,Token::L_BRACKET));
        break;
      case R_BRACKET:
        ++iter;
        addToken(new Token(ch,Token::R_BRACKET));
        break;
      case COMMA:
        ++iter;
        addToken(new Token(ch,Token::COMMA));
        break;
      case AT_SIGN :
        ++iter;
        addToken(new Token(ch,Token::AT_SIGN));
        break;
      case PLUS:
        ++iter;
        addToken(new Token(ch,Token::ADDITION_OP));
        break;
      case VERT_BAR:
        ++iter;
        addToken(new Token(ch,Token::UNION_OP));
        break;
      default:
        // Error, don't grok character :-(
        errorPos = iter;
        errorCode = ERROR_UNKNOWN_CHAR;
        if (firstItem)
          firstItem->token->type=Token::ERROR;
        else
          addToken(new Token('\0',Token::ERROR));
        iter=size; // bail       
      }
    }
  }    
} //-- parse


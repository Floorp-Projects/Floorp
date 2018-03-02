/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Lexical analyzer for XPath expressions
 */

#include "txExprLexer.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "nsError.h"
#include "txXMLUtils.h"

/**
 * Creates a new ExprLexer
 */
txExprLexer::txExprLexer()
  : mCurrentItem(nullptr),
    mFirstItem(nullptr),
    mLastItem(nullptr),
    mTokenCount(0)
{
}

/**
 * Destroys this instance of an txExprLexer
 */
txExprLexer::~txExprLexer()
{
  //-- delete tokens
  Token* tok = mFirstItem;
  while (tok) {
    Token* temp = tok->mNext;
    delete tok;
    tok = temp;
  }
  mCurrentItem = nullptr;
}

Token*
txExprLexer::nextToken()
{
  if (!mCurrentItem) {
    NS_NOTREACHED("nextToken called on uninitialized lexer");
    return nullptr;
  }

  if (mCurrentItem->mType == Token::END) {
    // Do not progress beyond the end token
    return mCurrentItem;
  }

  Token* token = mCurrentItem;
  mCurrentItem = mCurrentItem->mNext;
  return token;
}

void
txExprLexer::addToken(Token* aToken)
{
  if (mLastItem) {
    mLastItem->mNext = aToken;
  }
  if (!mFirstItem) {
    mFirstItem = aToken;
    mCurrentItem = aToken;
  }
  mLastItem = aToken;
  ++mTokenCount;
}

/**
 * Returns true if the following Token should be an operator.
 * This is a helper for the first bullet of [XPath 3.7]
 *  Lexical Structure
 */
bool
txExprLexer::nextIsOperatorToken(Token* aToken)
{
  if (!aToken || aToken->mType == Token::NULL_TOKEN) {
    return false;
  }
  /* This relies on the tokens having the right order in txExprLexer.h */
  return aToken->mType < Token::COMMA ||
    aToken->mType > Token::UNION_OP;

}

/**
 * Parses the given string into a sequence of Tokens
 */
nsresult
txExprLexer::parse(const nsAString& aPattern)
{
  iterator end;
  aPattern.BeginReading(mPosition);
  aPattern.EndReading(end);

  //-- initialize previous token, this will automatically get
  //-- deleted when it goes out of scope
  Token nullToken(nullptr, nullptr, Token::NULL_TOKEN);

  Token::Type defType;
  Token* newToken = nullptr;
  Token* prevToken = &nullToken;
  bool isToken;

  while (mPosition < end) {

    defType = Token::CNAME;
    isToken = true;

    if (*mPosition == DOLLAR_SIGN) {
      if (++mPosition == end || !XMLUtils::isLetter(*mPosition)) {
        return NS_ERROR_XPATH_INVALID_VAR_NAME;
      }
      defType = Token::VAR_REFERENCE;
    }
    // just reuse the QName parsing, which will use defType
    // the token to construct

    if (XMLUtils::isLetter(*mPosition)) {
      // NCName, can get QName or OperatorName;
      //  FunctionName, NodeName, and AxisSpecifier may want whitespace,
      //  and are dealt with below
      iterator start = mPosition;
      while (++mPosition < end && XMLUtils::isNCNameChar(*mPosition)) {
        /* just go */
      }
      if (mPosition < end && *mPosition == COLON) {
        // try QName or wildcard, might need to step back for axis
        if (++mPosition == end) {
          return NS_ERROR_XPATH_UNEXPECTED_END;
        }
        if (XMLUtils::isLetter(*mPosition)) {
          while (++mPosition < end && XMLUtils::isNCNameChar(*mPosition)) {
            /* just go */
          }
        }
        else if (*mPosition == '*' && defType != Token::VAR_REFERENCE) {
          // eat wildcard for NameTest, bail for var ref at COLON
          ++mPosition;
        }
        else {
          --mPosition; // step back
        }
      }
      if (nextIsOperatorToken(prevToken)) {
        nsDependentSubstring op(Substring(start, mPosition));
        if (nsGkAtoms::_and->Equals(op)) {
          defType = Token::AND_OP;
        }
        else if (nsGkAtoms::_or->Equals(op)) {
          defType = Token::OR_OP;
        }
        else if (nsGkAtoms::mod->Equals(op)) {
          defType = Token::MODULUS_OP;
        }
        else if (nsGkAtoms::div->Equals(op)) {
          defType = Token::DIVIDE_OP;
        }
        else {
          // XXX QUESTION: spec is not too precise
          // badops is sure an error, but is bad:ops, too? We say yes!
          return NS_ERROR_XPATH_OPERATOR_EXPECTED;
        }
      }
      newToken = new Token(start, mPosition, defType);
    }
    else if (isXPathDigit(*mPosition)) {
      iterator start = mPosition;
      while (++mPosition < end && isXPathDigit(*mPosition)) {
        /* just go */
      }
      if (mPosition < end && *mPosition == '.') {
        while (++mPosition < end && isXPathDigit(*mPosition)) {
          /* just go */
        }
      }
      newToken = new Token(start, mPosition, Token::NUMBER);
    }
    else {
      switch (*mPosition) {
        //-- ignore whitespace
      case SPACE:
      case TX_TAB:
      case TX_CR:
      case TX_LF:
        ++mPosition;
        isToken = false;
        break;
      case S_QUOTE :
      case D_QUOTE :
      {
        iterator start = mPosition;
        while (++mPosition < end && *mPosition != *start) {
          // eat literal
        }
        if (mPosition == end) {
          mPosition = start;
          return NS_ERROR_XPATH_UNCLOSED_LITERAL;
        }
        newToken = new Token(start + 1, mPosition, Token::LITERAL);
        ++mPosition;
      }
      break;
      case PERIOD:
        // period can be .., .(DIGITS)+ or ., check next
        if (++mPosition == end) {
          newToken = new Token(mPosition - 1, Token::SELF_NODE);
        }
        else if (isXPathDigit(*mPosition)) {
          iterator start = mPosition - 1;
          while (++mPosition < end && isXPathDigit(*mPosition)) {
            /* just go */
          }
          newToken = new Token(start, mPosition, Token::NUMBER);
        }
        else if (*mPosition == PERIOD) {
          ++mPosition;
          newToken = new Token(mPosition - 2, mPosition, Token::PARENT_NODE);
        }
        else {
          newToken = new Token(mPosition - 1, Token::SELF_NODE);
        }
        break;
      case COLON: // QNames are dealt above, must be axis ident
        if (++mPosition >= end || *mPosition != COLON ||
            prevToken->mType != Token::CNAME) {
          return NS_ERROR_XPATH_BAD_COLON;
        }
        prevToken->mType = Token::AXIS_IDENTIFIER;
        ++mPosition;
        isToken = false;
        break;
      case FORWARD_SLASH :
        if (++mPosition < end && *mPosition == FORWARD_SLASH) {
          ++mPosition;
          newToken = new Token(mPosition - 2, mPosition, Token::ANCESTOR_OP);
        }
        else {
          newToken = new Token(mPosition - 1, Token::PARENT_OP);
        }
        break;
      case BANG : // can only be !=
        if (++mPosition < end && *mPosition == EQUAL) {
          ++mPosition;
          newToken = new Token(mPosition - 2, mPosition, Token::NOT_EQUAL_OP);
          break;
        }
        // Error ! is not not()
        return NS_ERROR_XPATH_BAD_BANG;
      case EQUAL:
        newToken = new Token(mPosition, Token::EQUAL_OP);
        ++mPosition;
        break;
      case L_ANGLE:
        if (++mPosition == end) {
          return NS_ERROR_XPATH_UNEXPECTED_END;
        }
        if (*mPosition == EQUAL) {
          ++mPosition;
          newToken = new Token(mPosition - 2, mPosition,
                               Token::LESS_OR_EQUAL_OP);
        }
        else {
          newToken = new Token(mPosition - 1, Token::LESS_THAN_OP);
        }
        break;
      case R_ANGLE:
        if (++mPosition == end) {
          return NS_ERROR_XPATH_UNEXPECTED_END;
        }
        if (*mPosition == EQUAL) {
          ++mPosition;
          newToken = new Token(mPosition - 2, mPosition,
                               Token::GREATER_OR_EQUAL_OP);
        }
        else {
          newToken = new Token(mPosition - 1, Token::GREATER_THAN_OP);
        }
        break;
      case HYPHEN :
        newToken = new Token(mPosition, Token::SUBTRACTION_OP);
        ++mPosition;
        break;
      case ASTERISK:
        if (nextIsOperatorToken(prevToken)) {
          newToken = new Token(mPosition, Token::MULTIPLY_OP);
        }
        else {
          newToken = new Token(mPosition, Token::CNAME);
        }
        ++mPosition;
        break;
      case L_PAREN:
        if (prevToken->mType == Token::CNAME) {
          const nsDependentSubstring& val = prevToken->Value();
          if (val.EqualsLiteral("comment")) {
            prevToken->mType = Token::COMMENT_AND_PAREN;
          }
          else if (val.EqualsLiteral("node")) {
            prevToken->mType = Token::NODE_AND_PAREN;
          }
          else if (val.EqualsLiteral("processing-instruction")) {
            prevToken->mType = Token::PROC_INST_AND_PAREN;
          }
          else if (val.EqualsLiteral("text")) {
            prevToken->mType = Token::TEXT_AND_PAREN;
          }
          else {
            prevToken->mType = Token::FUNCTION_NAME_AND_PAREN;
          }
          isToken = false;
        }
        else {
          newToken = new Token(mPosition, Token::L_PAREN);
        }
        ++mPosition;
        break;
      case R_PAREN:
        newToken = new Token(mPosition, Token::R_PAREN);
        ++mPosition;
        break;
      case L_BRACKET:
        newToken = new Token(mPosition, Token::L_BRACKET);
        ++mPosition;
        break;
      case R_BRACKET:
        newToken = new Token(mPosition, Token::R_BRACKET);
        ++mPosition;
        break;
      case COMMA:
        newToken = new Token(mPosition, Token::COMMA);
        ++mPosition;
        break;
      case AT_SIGN :
        newToken = new Token(mPosition, Token::AT_SIGN);
        ++mPosition;
        break;
      case PLUS:
        newToken = new Token(mPosition, Token::ADDITION_OP);
        ++mPosition;
        break;
      case VERT_BAR:
        newToken = new Token(mPosition, Token::UNION_OP);
        ++mPosition;
        break;
      default:
        // Error, don't grok character :-(
        return NS_ERROR_XPATH_ILLEGAL_CHAR;
      }
    }
    if (isToken) {
      NS_ENSURE_TRUE(newToken, NS_ERROR_OUT_OF_MEMORY);
      NS_ENSURE_TRUE(newToken != mLastItem, NS_ERROR_FAILURE);
      prevToken = newToken;
      addToken(newToken);
    }
  }

  // add a endToken to the list
  newToken = new Token(end, end, Token::END);
  addToken(newToken);

  return NS_OK;
}

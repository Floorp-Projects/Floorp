/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/**
 * Lexical analyzer for XPath expressions
 */

#include "ExprLexer.h"
#include "txAtoms.h"
#include "nsString.h"
#include "txError.h"
#include "XMLUtils.h"

/**
 * Creates a new ExprLexer
 */
txExprLexer::txExprLexer()
  : mCurrentItem(nsnull),
    mFirstItem(nsnull),
    mLastItem(nsnull),
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
  mCurrentItem = nsnull;
}

Token*
txExprLexer::nextToken()
{
  NS_ASSERTION(mCurrentItem, "nextToken called beyoned the end");
  Token* token = mCurrentItem;
  mCurrentItem = mCurrentItem->mNext;
  return token;
}

void
txExprLexer::pushBack()
{
  mCurrentItem = mCurrentItem ? mCurrentItem->mPrevious : mLastItem;
}

void
txExprLexer::addToken(Token* aToken)
{
  if (mLastItem) {
    aToken->mPrevious = mLastItem;
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
PRBool
txExprLexer::nextIsOperatorToken(Token* aToken)
{
  if (!aToken || aToken->mType == Token::NULL_TOKEN) {
    return PR_FALSE;
  }
  /* This relies on the tokens having the right order in ExprLexer.h */
  return aToken->mType < Token::COMMA ||
    aToken->mType > Token::UNION_OP;

}

/**
 * Parses the given string into a sequence of Tokens
 */
nsresult
txExprLexer::parse(const nsASingleFragmentString& aPattern)
{
  iterator start, end;
  start = aPattern.BeginReading(mPosition);
  aPattern.EndReading(end);

  //-- initialize previous token, this will automatically get
  //-- deleted when it goes out of scope
  Token nullToken(nsnull, nsnull, Token::NULL_TOKEN);

  Token::Type defType;
  Token* newToken = nsnull;
  Token* prevToken = &nullToken;
  PRBool isToken;

  while (mPosition < end) {

    defType = Token::CNAME;
    isToken = PR_TRUE;

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
      start = mPosition;
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
        NS_ConvertUTF16toUTF8 opUTF8(Substring(start, mPosition));
        if (txXPathAtoms::_and->EqualsUTF8(opUTF8)) {
          defType = Token::AND_OP;
        }
        else if (txXPathAtoms::_or->EqualsUTF8(opUTF8)) {
          defType = Token::OR_OP;
        }
        else if (txXPathAtoms::mod->EqualsUTF8(opUTF8)) {
          defType = Token::MODULUS_OP;
        }
        else if (txXPathAtoms::div->EqualsUTF8(opUTF8)) {
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
      start = mPosition;
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
        isToken = PR_FALSE;
        break;
      case S_QUOTE :
      case D_QUOTE :
        start = mPosition;
        while (++mPosition < end && *mPosition != *start) {
          // eat literal
        }
        if (mPosition == end) {
          mPosition = start;
          return NS_ERROR_XPATH_UNCLOSED_LITERAL;
        }
        newToken = new Token(start + 1, mPosition, Token::LITERAL);
        ++mPosition;
        break;
      case PERIOD:
        // period can be .., .(DIGITS)+ or ., check next
        if (++mPosition == end) {
          newToken = new Token(mPosition - 1, Token::SELF_NODE);
        }
        else if (isXPathDigit(*mPosition)) {
          start = mPosition - 1;
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
        isToken = PR_FALSE;
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
      case ASTERIX:
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
          NS_ConvertUTF16toUTF8 utf8Value(prevToken->Value());
          if (txXPathAtoms::comment->EqualsUTF8(utf8Value)) {
            prevToken->mType = Token::COMMENT;
          }
          else if (txXPathAtoms::node->EqualsUTF8(utf8Value)) {
            prevToken->mType = Token::NODE;
          }
          else if (txXPathAtoms::processingInstruction->EqualsUTF8(utf8Value)) {
            prevToken->mType = Token::PROC_INST;
          }
          else if (txXPathAtoms::text->EqualsUTF8(utf8Value)) {
            prevToken->mType = Token::TEXT;
          }
          else {
            prevToken->mType = Token::FUNCTION_NAME;
          }
        }
        newToken = new Token(mPosition, Token::L_PAREN);
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
  if (!newToken) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  addToken(newToken);

  return NS_OK;
}

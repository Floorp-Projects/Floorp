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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *  David Landwehr <dlandwehr@novell.com>
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

#include "nsXFormsXPathScanner.h"
#include "nsXFormsXPathXMLUtil.h"

MOZ_DECL_CTOR_COUNTER(nsXFormsXPathScanner)

nsXFormsXPathScanner::nsXFormsXPathScanner()
{
  MOZ_COUNT_CTOR(nsXFormsXPathScanner);
}

nsXFormsXPathScanner::nsXFormsXPathScanner(const nsAString& aExpression)
{
  MOZ_COUNT_CTOR(nsXFormsXPathScanner);

  Init(aExpression);
}

nsXFormsXPathScanner::~nsXFormsXPathScanner()
{
  MOZ_COUNT_DTOR(nsXFormsXPathScanner);
}

void
nsXFormsXPathScanner::Init(const nsAString& aExpression)
{
  mExpression = aExpression;
  mSize = mExpression.Length();
  mLength = 0;
  mOffset = -1;
  mLast = NONE;
  mState = NONE;
}

PRInt32
nsXFormsXPathScanner::Length()
{
  return mLength;
}

PRInt32
nsXFormsXPathScanner::Offset()
{
  return mOffset;
}

nsAString&
nsXFormsXPathScanner::Expression()
{
  return mExpression;
}

PRUnichar
nsXFormsXPathScanner::PopChar()
{
  PRUnichar c = '\0';
  mLength++;
  if (mOffset + mLength < mSize) {
    c =  mExpression[mOffset + mLength];
  }
  return c;
}

PRUnichar
nsXFormsXPathScanner::PeekChar()
{
  return PeekChar(mOffset + mLength + 1);
}

PRUnichar
nsXFormsXPathScanner::PeekChar(PRInt32 aOffset)
{
  if (mSize > aOffset)
    return mExpression[aOffset];
  return '\0';
}

PRUnichar
nsXFormsXPathScanner::NextNonWhite()
{
  return PeekChar(GetOffsetForNonWhite());
}

int
nsXFormsXPathScanner::GetOffsetForNonWhite()
{
  PRInt32 co = mOffset + mLength + 1;
  while (nsXFormsXPathXMLUtil::IsWhitespace(PeekChar(co)))
    co++;
  return co;
}

PRBool
nsXFormsXPathScanner::HasMoreTokens()
{
  return mState != XPATHEOF;
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathScanner::NextToken()
{
  if (mState != WHITESPACE)
    mLast = mState;
  mOffset = mOffset + mLength;
  mLength = 0;

  PRUnichar c = PeekChar();
  if (c == '\0') {
    mState = XPATHEOF;
  } else if (nsXFormsXPathXMLUtil::IsDigit(c)) {
    mState = ScanNumber();
  } else if (c == '_' || nsXFormsXPathXMLUtil::IsLetter(c)) {
    mState = ScanQName();
  } else if (c == '"' || c == '\'') {
    mState = ScanLiteral();
  } else {
    switch (c) {
    case '(':
      mState = LPARAN;
      PopChar();
      break;
      
    case ')':
      mState = RPARAN;
      PopChar();
      break;
      
    case '[':
      mState = LBRACK;
      PopChar();
      break;
      
    case ']':
      mState = RBRACK;
      PopChar();
      break;
      
    case '@':
      mState = AT;
      PopChar();
      break;
      
    case ',':
      mState = COMMA;
      PopChar();
      break;
      
    case ':':
      PopChar();
      if (PeekChar() == ':') {
        mState = COLONCOLON;
        PopChar();
      } else
        mState = ERRORXPATHTOKEN;
      break;
      
    case '.':
      PopChar();
      if (PeekChar() == '.') {
        mState = DOTDOT;
        PopChar();
      } else if (nsXFormsXPathXMLUtil::IsDigit(PeekChar()))
        mState = ScanNumber();
      else
        mState = DOT;
      break;
      
    case '$':
      mState = ScanVariable();
      break;
      
    case '/':
      PopChar();
      if (PeekChar() == '/') {
        mState = SLASHSLASH;
        PopChar();
      } else
        mState = SLASH;
      break;
      
    case '|':
      PopChar();
      mState = UNION;
      break;
      
    case '+':
      PopChar();
      mState = PLUS;
      break;
      
    case '-':
      PopChar();
      mState = MINUS;
      break;
      
    case '=':
      PopChar();
      mState = EQUAL;
      break;
      
    case '!':
      PopChar();
      if (PeekChar() == '=') {
        mState = NOTEQUAL;
        PopChar();
      } else
        mState = ERRORXPATHTOKEN;
      break;
      
    case '<':
      PopChar();
      if (PeekChar() == '=') {
        mState = LEQUAL;
        PopChar();
      } else
        mState = LESS;
      break;
      
    case '>':
      PopChar();
      if (PeekChar() == '=') {
        mState = GEQUAL;
        PopChar();
      } else
        mState = GREATER;
      break;
      
    case '*':
      PopChar();
      mState = SolveStar();
      break;
      
    case '\r':
    case '\n':
    case '\t':
    case ' ':
      mState = ScanWhitespace();
      break;
      
    default:
      PopChar();
      mState = ERRORXPATHTOKEN;
    }
  }

  return mState;
}

PRBool
nsXFormsXPathScanner::SolveDiambiguate()
{
  return mLast != NONE && (mLast != AT && mLast != COLONCOLON && mLast != LPARAN && mLast != LBRACK &&
                            mLast != AND && mLast != OR && mLast != DIV && mLast != MOD && mLast != SLASH && mLast != MULTIPLYOPERATOR &&
                            mLast != SLASHSLASH && mLast != UNION && mLast != PLUS && mLast != MINUS && mLast != NOTEQUAL && mLast != EQUAL && mLast != LESS &&
                            mLast != LEQUAL && mLast != GEQUAL && mLast != GREATER && mLast != COMMA);
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathScanner::SolveStar()
{
  if (SolveDiambiguate())
    return MULTIPLYOPERATOR;
  return STAR;
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathScanner::ScanNumber()
{
  PRUnichar c = PopChar();
  PRBool decimal = (c == '.');

  while (c != '\0') {
    c = PeekChar();
    if (!decimal && c == '.') {
      decimal = PR_TRUE;
    } else if (!nsXFormsXPathXMLUtil::IsDigit(c)) {
      return NUMBER;
    }
    PopChar();
  }
  return NUMBER;
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathScanner::ScanVariable()
{
  PopChar();
  if (ScanQName() != ERRORXPATHTOKEN)
    return VARIABLE;
  return ERRORXPATHTOKEN;
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathScanner::ScanWhitespace()
{
  PRUnichar c;
  do {
    PopChar();
    c = PeekChar();
  } while (nsXFormsXPathXMLUtil::IsWhitespace(c));

  return WHITESPACE;
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathScanner::ScanLiteral()
{
  PRUnichar c = PopChar();
  PRUnichar p;
  while ((p = PeekChar()) != c && p != '\0')
    PopChar();
  if (p == '\0')
    return ERRORXPATHTOKEN;
  PopChar();
  return LITERAL;
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathScanner::ScanQName()
{
  XPATHTOKEN second = NONE;
  ScanNCName();
  if (PeekChar() == ':' && PeekChar(mOffset + mLength + 2) != ':') {
    PopChar();
    second = ScanNCName();
  }

  // TODO: nsSubString and .Equals() ?
  nsDependentSubstring image = Substring(mExpression, Offset());

  if (SolveDiambiguate()) {
    if (image.EqualsASCII("and", 3))
      return AND;
    else if (image.EqualsASCII("or", 2))
      return OR;
    else if (image.EqualsASCII("mod", 3))
      return MOD;
    else if (image.EqualsASCII("div", 3))
      return DIV;
    return ERRORXPATHTOKEN;
  }

  PRUnichar c = NextNonWhite();
  if (c == '(') {
    if (image.EqualsASCII("comment", 7))
      return COMMENT;
    else if (image.EqualsASCII("text", 4))
      return TEXT;
    else if (image.EqualsASCII("processing-instruction", 22))
      return PI;
    else if (image.EqualsASCII("node", 4))
      return NODE;

    return FUNCTIONNAME;
  }

  PRInt32 of = GetOffsetForNonWhite();
  if (PeekChar(of) == ':' && PeekChar(of + 1) == ':') {
    if (image.EqualsASCII("ancestor", 8))
      return ANCESTOR;
    else if (image.EqualsASCII("ancestor-or-self", 16))
      return ANCESTOR_OR_SELF;
    else if (image.EqualsASCII("attribute", 9))
      return ATTRIBUTE;
    else if (image.EqualsASCII("child", 5))
      return CHILD;
    else if (image.EqualsASCII("descendant", 10))
      return DESCENDANT;
    else if (image.EqualsASCII("descendant-or-self", 18))
      return DESCENDANT_OR_SELF;
    else if (image.EqualsASCII("following", 9))
      return FOLLOWING;
    else if (image.EqualsASCII("following-sibling", 17))
      return FOLLOWING_SIBLING;
    else if (image.EqualsASCII("namespace", 9))
      return NAMESPACE;
    else if (image.EqualsASCII("parent", 6))
      return PARENT;
    else if (image.EqualsASCII("preceding", 9))
      return PRECEDING;
    else if (image.EqualsASCII("preceding-sibling", 17))
      return PRECEDING_SIBLING;
    else if (image.EqualsASCII("self", 4))
      return SELF;
    return ERRORXPATHTOKEN;
  }
  return second != NONE ? QNAME : NCNAME;
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathScanner::ScanNCName()
{
  PRUnichar c = PopChar();
  if (c != '_' && !nsXFormsXPathXMLUtil::IsLetter(c)) {
    return ERRORXPATHTOKEN;
  }
    
  while (nsXFormsXPathXMLUtil::IsNCNameChar(PeekChar())) {
    PopChar();
  }
    
  return NCNAME;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCSSScanner.h"
#include "nsIInputStream.h"
#include "nsIUnicharInputStream.h"
#include "nsString.h"
#include "nsCRT.h"

#ifdef NS_DEBUG
static char* kNullPointer = "null pointer";
#endif

// Don't bother collecting whitespace characters in token's mIdent buffer
#undef COLLECT_WHITESPACE

#define BUFFER_SIZE 256

static const PRUnichar CSS_ESCAPE = PRUnichar('\\');

static const PRUint8 IS_LATIN1 = 0x01;
static const PRUint8 IS_DIGIT = 0x02;
static const PRUint8 IS_HEX_DIGIT = 0x04;
static const PRUint8 IS_ALPHA = 0x08;
static const PRUint8 START_IDENT = 0x10;
static const PRUint8 IS_IDENT = 0x20;
static const PRUint8 IS_WHITESPACE = 0x40;

static PRUint8* gLexTable;

static void BuildLexTable()
{
  PRUint8* lt = new PRUint8[256];
  nsCRT::zero(lt, 256);
  gLexTable = lt;

  int i;
  lt[CSS_ESCAPE] = START_IDENT;
  lt['-'] |= IS_IDENT;
  // XXX add in other whitespace chars
  lt[' '] |= IS_WHITESPACE;
  lt['\t'] |= IS_WHITESPACE;
  lt['\r'] |= IS_WHITESPACE;
  lt['\n'] |= IS_WHITESPACE;
  for (i = 161; i <= 255; i++) {
    lt[i] |= IS_LATIN1 | IS_IDENT | START_IDENT;
  }
  for (i = '0'; i <= '9'; i++) {
    lt[i] |= IS_DIGIT | IS_HEX_DIGIT | IS_IDENT;
  }
  for (i = 'A'; i <= 'Z'; i++) {
    if ((i >= 'A') && (i <= 'F')) {
      lt[i] |= IS_HEX_DIGIT;
      lt[i+32] |= IS_HEX_DIGIT;
    }
    lt[i] |= IS_ALPHA | IS_IDENT | START_IDENT;
    lt[i+32] |= IS_ALPHA | IS_IDENT | START_IDENT;
  }
}

nsCSSToken::nsCSSToken()
{
  mType = eCSSToken_Symbol;
}

nsCSSScanner::nsCSSScanner()
{
  if (nsnull == gLexTable) {
    // XXX need a monitor
    BuildLexTable();
  }
  mInput = nsnull;
  mBuffer = new PRUnichar[BUFFER_SIZE];
  mOffset = 0;
  mCount = 0;
  mLookAhead = -1;
}

nsCSSScanner::~nsCSSScanner()
{
  Close();
  if (nsnull != mBuffer) {
    delete mBuffer;
    mBuffer = nsnull;
  }
}

void nsCSSScanner::Init(nsIUnicharInputStream* aInput)
{
  NS_PRECONDITION(nsnull != aInput, kNullPointer);
  Close();
  mInput = aInput;
  if (nsnull != aInput) {
    aInput->AddRef();
  }
}

void nsCSSScanner::Close()
{
  NS_IF_RELEASE(mInput);
}

// Returns -1 on error or eof
PRInt32 nsCSSScanner::Read(PRInt32* aErrorCode)
{
  PRInt32 rv;
  if (mLookAhead >= 0) {
    rv = mLookAhead;
    mLookAhead = -1;
  } else {
    if (mCount < 0) {
      return -1;
    }
    if (mOffset == mCount) {
      mOffset = 0;
      mCount = mInput->Read(aErrorCode, mBuffer, 0, BUFFER_SIZE);
      if (mCount <= 0) {
        return -1;
      }
    }
    rv = PRInt32(mBuffer[mOffset++]);
  }
  mLastRead = rv;
//printf("Read => %x\n", rv);
  return rv;
}

PRInt32 nsCSSScanner::Peek(PRInt32* aErrorCode)
{
  if (mLookAhead < 0) {
    mLookAhead = Read(aErrorCode);
    if (mLookAhead < 0) {
      return -1;
    }
  }
//printf("Peek => %x\n", mLookAhead);
  return mLookAhead;
}

void nsCSSScanner::Unread()
{
  NS_PRECONDITION((mLastRead >= 0) && (mLookAhead < 0), "double pushback");
  mLookAhead = mLastRead;
  mLastRead = -1;
}

PRBool nsCSSScanner::LookAhead(PRInt32* aErrorCode, PRUnichar aChar)
{
  PRInt32 ch = Read(aErrorCode);
  if (ch < 0) {
    return PR_FALSE;
  }
  if (ch == aChar) {
    return PR_TRUE;
  }
  Unread();
  return PR_FALSE;
}

PRBool nsCSSScanner::EatWhiteSpace(PRInt32* aErrorCode)
{
  PRBool eaten = PR_FALSE;
  for (;;) {
    PRInt32 ch = Read(aErrorCode);
    if (ch < 0) {
      break;
    }
    if ((ch == ' ') || (ch == '\n') || (ch == '\r') || (ch == '\t')) {
      eaten = PR_TRUE;
      continue;
    }
    Unread();
    break;
  }
  return eaten;
}

PRBool nsCSSScanner::EatNewline(PRInt32* aErrorCode)
{
  PRInt32 ch = Read(aErrorCode);
  if (ch < 0) {
    return PR_FALSE;
  }
  PRBool eaten = PR_FALSE;
  if (ch == '\r') {
    eaten = PR_TRUE;
    ch = Peek(aErrorCode);
    if (ch == '\n') {
      (void) Read(aErrorCode);
    }
  } else if (ch == '\n') {
    eaten = PR_TRUE;
  } else {
    Unread();
  }
  return eaten;
}

PRBool nsCSSScanner::Next(PRInt32* aErrorCode, nsCSSToken* aToken)
{
  PRInt32 ch = Read(aErrorCode);
  if (ch < 0) {
    return PR_FALSE;
  }
  if (ch < 256) {
    PRUint8* lexTable = gLexTable;

    // IDENT
    if ((lexTable[ch] & START_IDENT) != 0) {
      return ParseIdent(aErrorCode, ch, aToken);
    }

    // AT_KEYWORD
    if (ch == '@') {
      PRInt32 nextChar = Peek(aErrorCode);
      if ((nextChar >= 0) && (nextChar <= 255)) {
        if ((lexTable[nextChar] & START_IDENT) != 0) {
          return ParseAtKeyword(aErrorCode, ch, aToken);
        }
      }
    }

    // NUMBER or DIM
    if ((ch == '.') || (ch == '+') || (ch == '-')) {
      PRInt32 nextChar = Peek(aErrorCode);
      if ((nextChar >= 0) && (nextChar <= 255)) {
        if ((lexTable[nextChar] & IS_DIGIT) != 0) {
          return ParseNumber(aErrorCode, ch, aToken);
        }
      }
    }
    if ((lexTable[ch] & IS_DIGIT) != 0) {
      return ParseNumber(aErrorCode, ch, aToken);
    }

    // ID
    if (ch == '#') {
      return ParseID(aErrorCode, ch, aToken);
    }

    // STRING
    if ((ch == '"') || (ch == '\'')) {
      return ParseString(aErrorCode, ch, aToken);
    }

    // WS
    if ((lexTable[ch] & IS_WHITESPACE) != 0) {
      aToken->mType = eCSSToken_WhiteSpace;
      aToken->mIdent.SetLength(0);
      aToken->mIdent.Append(PRUnichar(ch));
      (void) EatWhiteSpace(aErrorCode);
      return PR_TRUE;
    }
    if (ch == '/') {
      PRInt32 nextChar = Peek(aErrorCode);
      if (nextChar == '/') {
        (void) Read(aErrorCode);
        aToken->mIdent.SetLength(0);
        aToken->mIdent.Append(PRUnichar(ch));
        aToken->mIdent.Append(PRUnichar(ch));
        return ParseEOLComment(aErrorCode, aToken);
      } else if (nextChar == '*') {
        (void) Read(aErrorCode);
        aToken->mIdent.SetLength(0);
        aToken->mIdent.Append(PRUnichar(ch));
        aToken->mIdent.Append(PRUnichar(nextChar));
        return ParseCComment(aErrorCode, aToken);
      }
    }
  }
  aToken->mType = eCSSToken_Symbol;
  aToken->mSymbol = ch;
  return PR_TRUE;
}

PRInt32 nsCSSScanner::ParseEscape(PRInt32* aErrorCode)
{
  PRUint8* lexTable = gLexTable;
  PRInt32 ch = Peek(aErrorCode);
  if (ch < 0) {
    return CSS_ESCAPE;
  }
  if ((ch <= 255) && ((lexTable[ch] & IS_HEX_DIGIT) != 0)) {
    PRInt32 rv = 0;
    for (int i = 0; i < 4; i++) {
      ch = Read(aErrorCode);
      if (ch < 0) {
        // Whoops: error or premature eof
        break;
      }
      if ((lexTable[ch] & IS_HEX_DIGIT) != 0) {
        if ((lexTable[ch] & IS_DIGIT) != 0) {
          rv = rv * 16 + (ch - '0');
        } else {
          // Note: c&7 just keeps the low three bits which causes
          // upper and lower case alphabetics to both yield their
          // "relative to 10" value for computing the hex value.
          rv = rv * 16 + (ch & 0x7) + 10;
        }
      } else {
        Unread();
        break;
      }
    }
    return rv;
  } else {
    // "Any character except a hexidecimal digit can be escaped to
    // remove its special meaning by putting a backslash in front"
    // -- CSS1 spec section 7.1
    (void) Read(aErrorCode);
    return ch;
  }
}

/**
 * Gather up the characters in an identifier. The identfier was
 * started by "aChar" which will be appended to aIdent. The result
 * will be aIdent with all of the identifier characters appended
 * until the first non-identifier character is seen. The termination
 * character is unread for the future re-reading.
 */
PRBool nsCSSScanner::GatherIdent(PRInt32* aErrorCode, PRInt32 aChar,
                                 nsString& aIdent)
{
  if (aChar == CSS_ESCAPE) {
    aChar = ParseEscape(aErrorCode);
  }
  for (;;) {
    aChar = Read(aErrorCode);
    if (aChar < 0) break;
    if (aChar == CSS_ESCAPE) {
      aChar = ParseEscape(aErrorCode);
      aIdent.Append(PRUnichar(aChar));
    } else if ((aChar <= 255) && ((gLexTable[aChar] & IS_IDENT) != 0)) {
      aIdent.Append(PRUnichar(aChar));
    } else {
      Unread();
      break;
    }
  }
  return PR_TRUE;
}

PRBool nsCSSScanner::ParseID(PRInt32* aErrorCode,
                             PRInt32 aChar,
                             nsCSSToken* aToken)
{
  aToken->mIdent.SetLength(0);
  aToken->mType = eCSSToken_ID;
  return GatherIdent(aErrorCode, aChar, aToken->mIdent);
}

PRBool nsCSSScanner::ParseIdent(PRInt32* aErrorCode,
                                PRInt32 aChar,
                                nsCSSToken* aToken)
{
  nsString& ident = aToken->mIdent;
  ident.SetLength(0);
  ident.Append(PRUnichar(aChar));
  if (!GatherIdent(aErrorCode, aChar, ident)) {
    return PR_FALSE;
  }

  // Process a url lexical token. A CSS1 url token can contain
  // characters beyond identifier characters (e.g. '/', ':', etc.)
  // Because of this the normal rules for tokenizing the input don't
  // apply very well. To simplify the parser and relax some of the
  // requirements on the scanner we parse url's here. If we find a
  // malformed URL then we emit a token of type "InvalidURL" so that
  // the CSS1 parser can ignore the invalid input. We attempt to eat
  // the right amount of input data when an invalid URL is presented.
  nsCSSTokenType tokenType = eCSSToken_Ident;
  if (ident.EqualsIgnoreCase("url")) {
    tokenType = eCSSToken_InvalidURL;
    if (LookAhead(aErrorCode, '(')) {
      // Skip leading whitespace
      (void) EatWhiteSpace(aErrorCode);
      ident.SetLength(0);

      PRInt32 c = Read(aErrorCode);
      if (c == ')') {
        // empty url spec: this is invalid
      } else if ((c == '"') || (c == '\'')) {
        // start of a quoted url
        if (!GatherString(aErrorCode, c, ident)) {
          return PR_FALSE;
        }
        // Whitespace is allowed at the end of the URL before the right paren
        (void) EatWhiteSpace(aErrorCode);
        if (LookAhead(aErrorCode, ')')) {
          tokenType = eCSSToken_URL;
        }
      } else {
        // start of a non-quoted url
        Unread();
        PRBool ok = PR_TRUE;
        for (;;) {
          c = Read(aErrorCode);
          if (c < 0) break;
          if (c == CSS_ESCAPE) {
            c = ParseEscape(aErrorCode);
            ident.Append(PRUnichar(c));
          } else if ((c == '"') || (c == '\'') || (c == '(')) {
            // This is an invalid URL spec
            ok = PR_FALSE;
          } else if ((256 >= c) && ((gLexTable[c] & IS_WHITESPACE) != 0)) {
            // Whitespace is allowed at the end of the URL
            (void) EatWhiteSpace(aErrorCode);
            if (LookAhead(aErrorCode, ')')) {
              // done!
              break;
            }
            // Whitespace is followed by something other than a
            // ")". This is an invalid url spec.
            ok = PR_FALSE;
          } else if (c == ')') {
            // All done
            break;
          } else {
            // A regular url character.
            ident.Append(PRUnichar(c));
          }
        }

        // If the result of the above scanning is ok then change the token
        // type to a useful one.
        if (ok) {
          tokenType = eCSSToken_URL;
        }
      }
    }
  }
  aToken->mType = tokenType;
  return PR_TRUE;
}

PRBool nsCSSScanner::ParseAtKeyword(PRInt32* aErrorCode, PRInt32 aChar,
                                    nsCSSToken* aToken)
{
  aToken->mIdent.SetLength(0);
  aToken->mType = eCSSToken_AtKeyword;
  return GatherIdent(aErrorCode, aChar, aToken->mIdent);
}

PRBool nsCSSScanner::ParseNumber(PRInt32* aErrorCode, PRInt32 c,
                                 nsCSSToken* aToken)
{
  nsString& ident = aToken->mIdent;
  ident.SetLength(0);
  PRBool gotDot = (c == '.') ? PR_TRUE : PR_FALSE;
  if (c != '+') {
    ident.Append(PRUnichar(c));
  }

  // Gather up characters that make up the number
  PRUint8* lexTable = gLexTable;
  for (;;) {
    c = Read(aErrorCode);
    if (c < 0) break;
    if (!gotDot && (c == '.')) {
      gotDot = PR_TRUE;
    } else if ((c > 255) || ((lexTable[c] & IS_DIGIT) == 0)) {
      break;
    }
    ident.Append(PRUnichar(c));
  }

  // Convert number to floating point
  nsCSSTokenType type = eCSSToken_Number;
  PRInt32 ec;
  float value = ident.ToFloat(&ec);

  // Look at character that terminated the number
  aToken->mIntegerValid = PR_FALSE;
  if (c >= 0) {
    if ((c <= 255) && ((lexTable[c] & START_IDENT) != 0)) {
      ident.SetLength(0);
      ident.Append(PRUnichar(c));
      if (!GatherIdent(aErrorCode, c, ident)) {
        return PR_FALSE;
      }
      type = eCSSToken_Dimension;
    } else if ('%' == c) {
      type = eCSSToken_Percentage;
      value = value / 100.0f;
    } else {
      // Put back character that stopped numeric scan
      Unread();
      if (!gotDot) {
        aToken->mInteger = ident.ToInteger(&ec);
        aToken->mIntegerValid = PR_TRUE;
      }
      ident.SetLength(0);
    }
  }
  aToken->mNumber = value;
  aToken->mType = type;
  return PR_TRUE;
}

PRBool nsCSSScanner::ParseCComment(PRInt32* aErrorCode, nsCSSToken* aToken)
{
  nsString& ident = aToken->mIdent;
  for (;;) {
    PRInt32 ch = Read(aErrorCode);
    if (ch < 0) break;
    if (ch == '*') {
      if (LookAhead(aErrorCode, '/')) {
        ident.Append(PRUnichar(ch));
        ident.Append('/');
        break;
      }
    }
#ifdef COLLECT_WHITESPACE
    ident.Append(PRUnichar(ch));
#endif
  }
  aToken->mType = eCSSToken_WhiteSpace;
  return PR_TRUE;
}

PRBool nsCSSScanner::ParseEOLComment(PRInt32* aErrorCode, nsCSSToken* aToken)
{
  nsString& ident = aToken->mIdent;
  ident.SetLength(0);
  for (;;) {
    if (EatNewline(aErrorCode)) {
      break;
    }
    PRInt32 ch = Read(aErrorCode);
    if (ch < 0) {
      break;
    }
#ifdef COLLECT_WHITESPACE
    ident.Append(PRUnichar(ch));
#endif
  }
  aToken->mType = eCSSToken_WhiteSpace;
  return PR_TRUE;
}

PRBool nsCSSScanner::GatherString(PRInt32* aErrorCode, PRInt32 aStop,
                                  nsString& aBuffer)
{
  for (;;) {
    PRInt32 ch = Read(aErrorCode);
    if (ch < 0) {
      return PR_FALSE;
    }
    if (ch == aStop) {
      break;
    }
    if (ch == CSS_ESCAPE) {
      ch = ParseEscape(aErrorCode);
      if (ch < 0) {
        return PR_FALSE;
      }
    }
    aBuffer.Append(PRUnichar(ch));
  }
  return PR_TRUE;
}

PRBool nsCSSScanner::ParseString(PRInt32* aErrorCode, PRInt32 aStop,
                                 nsCSSToken* aToken)
{
  aToken->mIdent.SetLength(0);
  aToken->mType = eCSSToken_String;
  return GatherString(aErrorCode, aStop, aToken->mIdent);
}

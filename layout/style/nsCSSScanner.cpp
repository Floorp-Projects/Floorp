/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/* tokenization of CSS style sheets */

#include <math.h> // must be first due to symbol conflicts

#include "nsCSSScanner.h"
#include "nsStyleUtil.h"
#include "mozilla/css/ErrorReporter.h"
#include "mozilla/Likely.h"
#include "mozilla/Util.h"
#include <algorithm>

using mozilla::ArrayLength;

static const uint8_t IS_HEX_DIGIT  = 0x01;
static const uint8_t START_IDENT   = 0x02;
static const uint8_t IS_IDENT      = 0x04;
static const uint8_t IS_WHITESPACE = 0x08;
static const uint8_t IS_URL_CHAR   = 0x10;

#define W    IS_WHITESPACE
#define I    IS_IDENT
#define U                                      IS_URL_CHAR
#define S             START_IDENT
#define UI   IS_IDENT                         |IS_URL_CHAR
#define USI  IS_IDENT|START_IDENT             |IS_URL_CHAR
#define UXI  IS_IDENT            |IS_HEX_DIGIT|IS_URL_CHAR
#define UXSI IS_IDENT|START_IDENT|IS_HEX_DIGIT|IS_URL_CHAR

static const uint8_t gLexTable[] = {
//                                     TAB LF      FF  CR
   0,  0,  0,  0,  0,  0,  0,  0,  0,  W,  W,  0,  W,  W,  0,  0,
//
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
// SPC !   "   #   $   %   &   '   (   )   *   +   ,   -   .   /
   W,  U,  0,  U,  U,  U,  U,  0,  0,  0,  U,  U,  U,  UI, U,  U,
// 0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ?
   UXI,UXI,UXI,UXI,UXI,UXI,UXI,UXI,UXI,UXI,U,  U,  U,  U,  U,  U,
// @   A   B   C    D    E    F    G   H   I   J   K   L   M   N   O
   U,UXSI,UXSI,UXSI,UXSI,UXSI,UXSI,USI,USI,USI,USI,USI,USI,USI,USI,USI,
// P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _
   USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,U,  S,  U,  U,  USI,
// `   a   b   c    d    e    f    g   h   i   j   k   l   m   n   o
   U,UXSI,UXSI,UXSI,UXSI,UXSI,UXSI,USI,USI,USI,USI,USI,USI,USI,USI,USI,
// p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~
   USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,U,  U,  U,  U,  0,
// U+008*
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
// U+009*
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
// U+00A*
   USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,
// U+00B*
   USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,
// U+00C*
   USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,
// U+00D*
   USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,
// U+00E*
   USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,
// U+00F*
   USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI,USI
};

MOZ_STATIC_ASSERT(NS_ARRAY_LENGTH(gLexTable) == 256,
                  "gLexTable expected to cover all 2^8 possible PRUint8s");

#undef W
#undef S
#undef I
#undef U
#undef UI
#undef USI
#undef UXI
#undef UXSI

static inline bool
IsIdentStart(int32_t aChar)
{
  return aChar >= 0 &&
    (aChar >= 256 || (gLexTable[aChar] & START_IDENT) != 0);
}

static inline bool
StartsIdent(int32_t aFirstChar, int32_t aSecondChar)
{
  return IsIdentStart(aFirstChar) ||
    (aFirstChar == '-' && IsIdentStart(aSecondChar));
}

static inline bool
IsWhitespace(int32_t ch) {
  return uint32_t(ch) < 256 && (gLexTable[ch] & IS_WHITESPACE) != 0;
}

static inline bool
IsDigit(int32_t ch) {
  return (ch >= '0') && (ch <= '9');
}

static inline bool
IsHexDigit(int32_t ch) {
  return uint32_t(ch) < 256 && (gLexTable[ch] & IS_HEX_DIGIT) != 0;
}

static inline bool
IsIdent(int32_t ch) {
  return ch >= 0 && (ch >= 256 || (gLexTable[ch] & IS_IDENT) != 0);
}

static inline bool
IsURLChar(int32_t ch) {
  return ch >= 0 && (ch >= 256 || (gLexTable[ch] & IS_URL_CHAR) != 0);
}

static inline uint32_t
DecimalDigitValue(int32_t ch)
{
  return ch - '0';
}

static inline uint32_t
HexDigitValue(int32_t ch)
{
  if (IsDigit(ch)) {
    return DecimalDigitValue(ch);
  } else {
    // Note: c&7 just keeps the low three bits which causes
    // upper and lower case alphabetics to both yield their
    // "relative to 10" value for computing the hex value.
    return (ch & 0x7) + 9;
  }
}

nsCSSToken::nsCSSToken()
{
  mType = eCSSToken_Symbol;
}

void
nsCSSToken::AppendToString(nsString& aBuffer) const
{
  switch (mType) {
    case eCSSToken_Ident:
      nsStyleUtil::AppendEscapedCSSIdent(mIdent, aBuffer);
      break;

    case eCSSToken_AtKeyword:
      aBuffer.Append('@');
      nsStyleUtil::AppendEscapedCSSIdent(mIdent, aBuffer);
      break;

    case eCSSToken_ID:
    case eCSSToken_Ref:
      aBuffer.Append('#');
      nsStyleUtil::AppendEscapedCSSIdent(mIdent, aBuffer);
      break;

    case eCSSToken_Function:
      nsStyleUtil::AppendEscapedCSSIdent(mIdent, aBuffer);
      aBuffer.Append('(');
      break;

    case eCSSToken_URL:
    case eCSSToken_Bad_URL:
      aBuffer.AppendLiteral("url(");
      if (mSymbol != PRUnichar(0)) {
        nsStyleUtil::AppendEscapedCSSString(mIdent, aBuffer, mSymbol);
      } else {
        aBuffer.Append(mIdent);
      }
      if (mType == eCSSToken_URL) {
        aBuffer.Append(PRUnichar(')'));
      }
      break;

    case eCSSToken_Number:
      if (mIntegerValid) {
        aBuffer.AppendInt(mInteger, 10);
      } else {
        aBuffer.AppendFloat(mNumber);
      }
      break;

    case eCSSToken_Percentage:
      NS_ASSERTION(!mIntegerValid, "How did a percentage token get this set?");
      aBuffer.AppendFloat(mNumber * 100.0f);
      aBuffer.Append(PRUnichar('%'));
      break;

    case eCSSToken_Dimension:
      if (mIntegerValid) {
        aBuffer.AppendInt(mInteger, 10);
      } else {
        aBuffer.AppendFloat(mNumber);
      }
      nsStyleUtil::AppendEscapedCSSIdent(mIdent, aBuffer);
      break;

    case eCSSToken_Bad_String:
      nsStyleUtil::AppendEscapedCSSString(mIdent, aBuffer, mSymbol);
      // remove the trailing quote character
      aBuffer.Truncate(aBuffer.Length() - 1);
      break;

    case eCSSToken_String:
      nsStyleUtil::AppendEscapedCSSString(mIdent, aBuffer, mSymbol);
      break;

    case eCSSToken_Symbol:
      aBuffer.Append(mSymbol);
      break;

    case eCSSToken_WhiteSpace:
      aBuffer.Append(' ');
      break;

    case eCSSToken_HTMLComment:
    case eCSSToken_URange:
      aBuffer.Append(mIdent);
      break;

    case eCSSToken_Includes:
      aBuffer.AppendLiteral("~=");
      break;
    case eCSSToken_Dashmatch:
      aBuffer.AppendLiteral("|=");
      break;
    case eCSSToken_Beginsmatch:
      aBuffer.AppendLiteral("^=");
      break;
    case eCSSToken_Endsmatch:
      aBuffer.AppendLiteral("$=");
      break;
    case eCSSToken_Containsmatch:
      aBuffer.AppendLiteral("*=");
      break;

    default:
      NS_ERROR("invalid token type");
      break;
  }
}

nsCSSScanner::nsCSSScanner(const nsAString& aBuffer, uint32_t aLineNumber)
  : mReadPointer(aBuffer.BeginReading())
  , mOffset(0)
  , mCount(aBuffer.Length())
  , mPushback(mLocalPushback)
  , mPushbackCount(0)
  , mPushbackSize(ArrayLength(mLocalPushback))
  , mLineNumber(aLineNumber)
  , mLineOffset(0)
  , mTokenLineNumber(aLineNumber)
  , mTokenLineOffset(0)
  , mTokenOffset(0)
  , mRecordStartOffset(0)
  , mReporter(nullptr)
  , mSVGMode(false)
  , mRecording(false)
{
  MOZ_COUNT_CTOR(nsCSSScanner);
}

nsCSSScanner::~nsCSSScanner()
{
  MOZ_COUNT_DTOR(nsCSSScanner);
  if (mLocalPushback != mPushback) {
    delete [] mPushback;
  }
}

// Returns -1 on error or eof
int32_t
nsCSSScanner::Read()
{
  int32_t rv;
  if (0 < mPushbackCount) {
    rv = int32_t(mPushback[--mPushbackCount]);
  } else {
    if (mOffset == mCount) {
      return -1;
    }
    rv = int32_t(mReadPointer[mOffset++]);
    // There are four types of newlines in CSS: "\r", "\n", "\r\n", and "\f".
    // To simplify dealing with newlines, they are all normalized to "\n" here
    if (rv == '\r') {
      if (mOffset < mCount && mReadPointer[mOffset] == '\n') {
        mOffset++;
      }
      rv = '\n';
    } else if (rv == '\f') {
      rv = '\n';
    }
    if (rv == '\n') {
      // 0 is a magical line number meaning that we don't know (i.e., script)
      if (mLineNumber != 0)
        ++mLineNumber;
      mLineOffset = mOffset;
    }
  }
  return rv;
}

int32_t
nsCSSScanner::Peek()
{
  if (0 == mPushbackCount) {
    int32_t ch = Read();
    if (ch < 0) {
      return -1;
    }
    mPushback[0] = PRUnichar(ch);
    mPushbackCount++;
  }
  return int32_t(mPushback[mPushbackCount - 1]);
}

void
nsCSSScanner::Pushback(PRUnichar aChar)
{
  if (mPushbackCount == mPushbackSize) { // grow buffer
    PRUnichar*  newPushback = new PRUnichar[mPushbackSize + 4];
    if (nullptr == newPushback) {
      return;
    }
    mPushbackSize += 4;
    memcpy(newPushback, mPushback, sizeof(PRUnichar) * mPushbackCount);
    if (mPushback != mLocalPushback) {
      delete [] mPushback;
    }
    mPushback = newPushback;
  }
  mPushback[mPushbackCount++] = aChar;
}

void
nsCSSScanner::StartRecording()
{
  NS_ASSERTION(!mRecording, "already started recording");
  mRecording = true;
  mRecordStartOffset = mOffset - mPushbackCount;
}

void
nsCSSScanner::StopRecording()
{
  NS_ASSERTION(mRecording, "haven't started recording");
  mRecording = false;
}

void
nsCSSScanner::StopRecording(nsString& aBuffer)
{
  NS_ASSERTION(mRecording, "haven't started recording");
  mRecording = false;
  aBuffer.Append(mReadPointer + mRecordStartOffset,
                 mOffset - mPushbackCount - mRecordStartOffset);
}

nsDependentSubstring
nsCSSScanner::GetCurrentLine() const
{
  uint32_t end = mTokenOffset;
  while (end < mCount &&
         mReadPointer[end] != '\n' && mReadPointer[end] != '\r' &&
         mReadPointer[end] != '\f') {
    end++;
  }
  return nsDependentSubstring(mReadPointer + mTokenLineOffset,
                              mReadPointer + end);
}

bool
nsCSSScanner::LookAhead(PRUnichar aChar)
{
  int32_t ch = Read();
  if (ch < 0) {
    return false;
  }
  if (ch == aChar) {
    return true;
  }
  Pushback(ch);
  return false;
}

bool
nsCSSScanner::LookAheadOrEOF(PRUnichar aChar)
{
  int32_t ch = Read();
  if (ch < 0) {
    return true;
  }
  if (ch == aChar) {
    return true;
  }
  Pushback(ch);
  return false;
}

void
nsCSSScanner::EatWhiteSpace()
{
  for (;;) {
    int32_t ch = Read();
    if (ch < 0) {
      break;
    }
    if ((ch != ' ') && (ch != '\n') && (ch != '\t')) {
      Pushback(ch);
      break;
    }
  }
}

bool
nsCSSScanner::Next(nsCSSToken& aToken)
{
  for (;;) { // Infinite loop so we can restart after comments.
    mTokenOffset = mOffset;
    mTokenLineOffset = mLineOffset;
    mTokenLineNumber = mLineNumber;

    int32_t ch = Read();
    if (ch < 0) {
      return false;
    }

    // UNICODE-RANGE
    if ((ch == 'u' || ch == 'U') && Peek() == '+')
      return ParseURange(ch, aToken);

    // IDENT
    if (StartsIdent(ch, Peek()))
      return ParseIdent(ch, aToken);

    // AT_KEYWORD
    if (ch == '@') {
      return ParseAtKeyword(aToken);
    }

    // NUMBER or DIM
    if ((ch == '.') || (ch == '+') || (ch == '-')) {
      int32_t nextChar = Peek();
      if (IsDigit(nextChar)) {
        return ParseNumber(ch, aToken);
      }
      else if (('.' == nextChar) && ('.' != ch)) {
        nextChar = Read();
        int32_t followingChar = Peek();
        Pushback(nextChar);
        if (IsDigit(followingChar))
          return ParseNumber(ch, aToken);
      }
    }
    if (IsDigit(ch)) {
      return ParseNumber(ch, aToken);
    }

    // ID
    if (ch == '#') {
      return ParseRef(ch, aToken);
    }

    // STRING
    if ((ch == '"') || (ch == '\'')) {
      return ParseString(ch, aToken);
    }

    // WS
    if (IsWhitespace(ch)) {
      aToken.mType = eCSSToken_WhiteSpace;
      aToken.mIdent.Assign(PRUnichar(ch));
      EatWhiteSpace();
      return true;
    }
    if (ch == '/' && !IsSVGMode()) {
      int32_t nextChar = Peek();
      if (nextChar == '*') {
        Read();
        // FIXME: Editor wants comments to be preserved (bug 60290).
        if (!SkipCComment()) {
          return false;
        }
        continue; // start again at the beginning
      }
    }
    if (ch == '<') {  // consume HTML comment tags
      if (LookAhead('!')) {
        if (LookAhead('-')) {
          if (LookAhead('-')) {
            aToken.mType = eCSSToken_HTMLComment;
            aToken.mIdent.AssignLiteral("<!--");
            return true;
          }
          Pushback('-');
        }
        Pushback('!');
      }
    }
    if (ch == '-') {  // check for HTML comment end
      if (LookAhead('-')) {
        if (LookAhead('>')) {
          aToken.mType = eCSSToken_HTMLComment;
          aToken.mIdent.AssignLiteral("-->");
          return true;
        }
        Pushback('-');
      }
    }

    // INCLUDES ("~=") and DASHMATCH ("|=")
    if (( ch == '|' ) || ( ch == '~' ) || ( ch == '^' ) ||
        ( ch == '$' ) || ( ch == '*' )) {
      int32_t nextChar = Read();
      if ( nextChar == '=' ) {
        if (ch == '~') {
          aToken.mType = eCSSToken_Includes;
        }
        else if (ch == '|') {
          aToken.mType = eCSSToken_Dashmatch;
        }
        else if (ch == '^') {
          aToken.mType = eCSSToken_Beginsmatch;
        }
        else if (ch == '$') {
          aToken.mType = eCSSToken_Endsmatch;
        }
        else if (ch == '*') {
          aToken.mType = eCSSToken_Containsmatch;
        }
        return true;
      } else if (nextChar >= 0) {
        Pushback(nextChar);
      }
    }
    aToken.mType = eCSSToken_Symbol;
    aToken.mSymbol = ch;
    return true;
  }
}

bool
nsCSSScanner::NextURL(nsCSSToken& aToken)
{
  EatWhiteSpace();

  int32_t ch = Read();
  if (ch < 0) {
    return false;
  }

  // STRING
  if ((ch == '"') || (ch == '\'')) {
#ifdef DEBUG
    bool ok =
#endif
      ParseString(ch, aToken);
    NS_ABORT_IF_FALSE(ok, "ParseString should never fail, "
                          "since there's always something read");

    NS_ABORT_IF_FALSE(aToken.mType == eCSSToken_String ||
                      aToken.mType == eCSSToken_Bad_String,
                      "unexpected token type");
    if (MOZ_LIKELY(aToken.mType == eCSSToken_String)) {
      EatWhiteSpace();
      if (LookAheadOrEOF(')')) {
        aToken.mType = eCSSToken_URL;
      } else {
        aToken.mType = eCSSToken_Bad_URL;
      }
    } else {
      aToken.mType = eCSSToken_Bad_URL;
    }
    return true;
  }

  // Process a url lexical token. A CSS1 url token can contain
  // characters beyond identifier characters (e.g. '/', ':', etc.)
  // Because of this the normal rules for tokenizing the input don't
  // apply very well. To simplify the parser and relax some of the
  // requirements on the scanner we parse url's here. If we find a
  // malformed URL then we emit a token of type "Bad_URL" so that
  // the CSS1 parser can ignore the invalid input.  The parser must
  // treat a Bad_URL token like a Function token, and process
  // tokens until a matching parenthesis.

  aToken.mType = eCSSToken_Bad_URL;
  aToken.mSymbol = PRUnichar(0);
  nsString& ident = aToken.mIdent;
  ident.SetLength(0);

  // start of a non-quoted url (which may be empty)
  bool ok = true;
  for (;;) {
    if (IsURLChar(ch)) {
      // A regular url character.
      ident.Append(PRUnichar(ch));
    } else if (ch == ')') {
      // All done
      break;
    } else if (IsWhitespace(ch)) {
      // Whitespace is allowed at the end of the URL
      EatWhiteSpace();
      // Consume the close paren if we have it; if not we're an invalid URL.
      ok = LookAheadOrEOF(')');
      break;
    } else if (ch == '\\') {
      if (!ParseAndAppendEscape(ident, false)) {
        ok = false;
        Pushback(ch);
        break;
      }
    } else {
      // This is an invalid URL spec
      ok = false;
      Pushback(ch); // push it back so the parser can match tokens and
                    // then closing parenthesis
      break;
    }

    ch = Read();
    if (ch < 0) {
      break;
    }
  }

  // If the result of the above scanning is ok then change the token
  // type to a useful one.
  if (ok) {
    aToken.mType = eCSSToken_URL;
  }
  return true;
}


/**
 * Returns whether an escape was succesfully parsed; if it was not,
 * the backslash needs to be its own symbol token.
 */
bool
nsCSSScanner::ParseAndAppendEscape(nsString& aOutput, bool aInString)
{
  int32_t ch = Read();
  if (ch < 0) {
    return false;
  }
  if (IsHexDigit(ch)) {
    int32_t rv = 0;
    int i;
    Pushback(ch);
    for (i = 0; i < 6; i++) { // up to six digits
      ch = Read();
      if (ch < 0) {
        // Whoops: error or premature eof
        break;
      }
      if (!IsHexDigit(ch) && !IsWhitespace(ch)) {
        Pushback(ch);
        break;
      } else if (IsHexDigit(ch)) {
        rv = rv * 16 + HexDigitValue(ch);
      } else {
        NS_ASSERTION(IsWhitespace(ch), "bad control flow");
        // single space ends escape
        break;
      }
    }
    if (6 == i) { // look for trailing whitespace and eat it
      ch = Peek();
      if (IsWhitespace(ch)) {
        (void) Read();
      }
    }
    NS_ASSERTION(rv >= 0, "How did rv become negative?");
    // "[at most six hexadecimal digits following a backslash] stand
    // for the ISO 10646 character with that number, which must not be
    // zero. (It is undefined in CSS 2.1 what happens if a style sheet
    // does contain a character with Unicode codepoint zero.)"
    //   -- CSS2.1 section 4.1.3
    //
    // Silently deleting \0 opens a content-filtration loophole (see
    // bug 228856), so what we do instead is pretend the "cancels the
    // meaning of special characters" rule applied.
    if (rv > 0) {
      AppendUCS4ToUTF16(ENSURE_VALID_CHAR(rv), aOutput);
    } else {
      while (i--)
        aOutput.Append('0');
      if (IsWhitespace(ch))
        Pushback(ch);
    }
    return true;
  }
  // "Any character except a hexadecimal digit can be escaped to
  // remove its special meaning by putting a backslash in front"
  // -- CSS1 spec section 7.1
  if (ch == '\n') {
    if (!aInString) {
      // Outside of strings (which includes url() that contains a
      // string), escaped newlines aren't special, and just tokenize as
      // eCSSToken_Symbol (DELIM).
      Pushback(ch);
      return false;
    }
    // In strings (and in url() containing a string), escaped newlines
    // are just dropped to allow splitting over multiple lines.
  } else {
    aOutput.Append(ch);
  }

  return true;
}

/**
 * Gather up the characters in an identifier. The identfier was
 * started by "aChar" which will be appended to aIdent. The result
 * will be aIdent with all of the identifier characters appended
 * until the first non-identifier character is seen. The termination
 * character is unread for the future re-reading.
 *
 * Returns failure when the character sequence does not form an ident at
 * all, in which case the caller is responsible for pushing back or
 * otherwise handling aChar.  (This occurs only when aChar is '\'.)
 */
bool
nsCSSScanner::GatherIdent(int32_t aChar, nsString& aIdent)
{
  if (aChar == '\\') {
    if (!ParseAndAppendEscape(aIdent, false)) {
      return false;
    }
  } else {
    MOZ_ASSERT(aChar > 0);
    aIdent.Append(aChar);
  }
  for (;;) {
    // If nothing in pushback, first try to get as much as possible in one go
    if (!mPushbackCount && mOffset < mCount) {
      // See how much we can consume and append in one go
      uint32_t n = mOffset;
      // Count number of Ident characters that can be processed
      while (n < mCount && IsIdent(mReadPointer[n])) {
        ++n;
      }
      // Add to the token what we have so far
      if (n > mOffset) {
        aIdent.Append(&mReadPointer[mOffset], n - mOffset);
        mOffset = n;
      }
    }

    aChar = Read();
    if (aChar < 0) break;
    if (aChar == '\\') {
      if (!ParseAndAppendEscape(aIdent, false)) {
        Pushback(aChar);
        break;
      }
    } else if (IsIdent(aChar)) {
      aIdent.Append(PRUnichar(aChar));
    } else {
      Pushback(aChar);
      break;
    }
  }
  MOZ_ASSERT(aIdent.Length() > 0);
  return true;
}

bool
nsCSSScanner::ParseRef(int32_t aChar, nsCSSToken& aToken)
{
  // Fall back for when we don't have name characters following:
  aToken.mType = eCSSToken_Symbol;
  aToken.mSymbol = aChar;

  int32_t ch = Read();
  if (ch < 0) {
    return true;
  }
  if (IsIdent(ch) || ch == '\\') {
    // First char after the '#' is a valid ident char (or an escape),
    // so it makes sense to keep going
    nsCSSTokenType type =
      StartsIdent(ch, Peek()) ? eCSSToken_ID : eCSSToken_Ref;
    aToken.mIdent.SetLength(0);
    if (GatherIdent(ch, aToken.mIdent)) {
      aToken.mType = type;
      return true;
    }
  }

  // No ident chars after the '#'.  Just unread |ch| and get out of here.
  Pushback(ch);
  return true;
}

bool
nsCSSScanner::ParseIdent(int32_t aChar, nsCSSToken& aToken)
{
  nsString& ident = aToken.mIdent;
  ident.SetLength(0);
  if (!GatherIdent(aChar, ident)) {
    aToken.mType = eCSSToken_Symbol;
    aToken.mSymbol = aChar;
    return true;
  }

  nsCSSTokenType tokenType = eCSSToken_Ident;
  // look for functions (ie: "ident(")
  if (Peek() == PRUnichar('(')) {
    Read();
    tokenType = eCSSToken_Function;

    if (ident.LowerCaseEqualsLiteral("url")) {
      NextURL(aToken); // ignore return value, since *we* read something
      return true;
    }
  }

  aToken.mType = tokenType;
  return true;
}

bool
nsCSSScanner::ParseAtKeyword(nsCSSToken& aToken)
{
  int32_t ch = Read();
  if (StartsIdent(ch, Peek())) {
    aToken.mIdent.SetLength(0);
    aToken.mType = eCSSToken_AtKeyword;
    if (GatherIdent(ch, aToken.mIdent)) {
      return true;
    }
  }
  if (ch >= 0) {
    Pushback(ch);
  }
  aToken.mType = eCSSToken_Symbol;
  aToken.mSymbol = PRUnichar('@');
  return true;
}

bool
nsCSSScanner::ParseNumber(int32_t c, nsCSSToken& aToken)
{
  NS_PRECONDITION(c == '.' || c == '+' || c == '-' || IsDigit(c),
                  "Why did we get called?");
  aToken.mHasSign = (c == '+' || c == '-');

  // Our sign.
  int32_t sign = c == '-' ? -1 : 1;
  // Absolute value of the integer part of the mantissa.  This is a double so
  // we don't run into overflow issues for consumers that only care about our
  // floating-point value while still being able to express the full int32_t
  // range for consumers who want integers.
  double intPart = 0;
  // Fractional part of the mantissa.  This is a double so that when we convert
  // to float at the end we'll end up rounding to nearest float instead of
  // truncating down (as we would if fracPart were a float and we just
  // effectively lost the last several digits).
  double fracPart = 0;
  // Absolute value of the power of 10 that we should multiply by (only
  // relevant for numbers in scientific notation).  Has to be a signed integer,
  // because multiplication of signed by unsigned converts the unsigned to
  // signed, so if we plan to actually multiply by expSign...
  int32_t exponent = 0;
  // Sign of the exponent.
  int32_t expSign = 1;

  if (aToken.mHasSign) {
    NS_ASSERTION(c != '.', "How did that happen?");
    c = Read();
  }

  bool gotDot = (c == '.');

  if (!gotDot) {
    // Parse the integer part of the mantisssa
    NS_ASSERTION(IsDigit(c), "Why did we get called?");
    do {
      intPart = 10*intPart + DecimalDigitValue(c);
      c = Read();
      // The IsDigit check will do the right thing even if Read() returns < 0
    } while (IsDigit(c));

    gotDot = (c == '.') && IsDigit(Peek());
  }

  if (gotDot) {
    // Parse the fractional part of the mantissa.
    c = Read();
    NS_ASSERTION(IsDigit(c), "How did we get here?");
    // Power of ten by which we need to divide our next digit
    float divisor = 10;
    do {
      fracPart += DecimalDigitValue(c) / divisor;
      divisor *= 10;
      c = Read();
      // The IsDigit check will do the right thing even if Read() returns < 0
    } while (IsDigit(c));
  }

  bool gotE = false;
  if (IsSVGMode() && (c == 'e' || c == 'E')) {
    int32_t nextChar = Peek();
    int32_t expSignChar = 0;
    if (nextChar == '-' || nextChar == '+') {
      expSignChar = Read();
      nextChar = Peek();
    }
    if (IsDigit(nextChar)) {
      gotE = true;
      if (expSignChar == '-') {
        expSign = -1;
      }

      c = Read();
      NS_ASSERTION(IsDigit(c), "Peek() must have lied");
      do {
        exponent = 10*exponent + DecimalDigitValue(c);
        c = Read();
        // The IsDigit check will do the right thing even if Read() returns < 0
      } while (IsDigit(c));
    } else {
      if (expSignChar) {
        Pushback(expSignChar);
      }
    }
  }

  nsCSSTokenType type = eCSSToken_Number;

  // Set mIntegerValid for all cases (except %, below) because we need
  // it for the "2n" in :nth-child(2n).
  aToken.mIntegerValid = false;

  // Time to reassemble our number.
  float value = float(sign * (intPart + fracPart));
  if (gotE) {
    // pow(), not powf(), because at least wince doesn't have the latter.
    // And explicitly cast everything to doubles to avoid issues with
    // overloaded pow() on Windows.
    value *= pow(10.0, double(expSign * exponent));
  } else if (!gotDot) {
    // Clamp values outside of integer range.
    if (sign > 0) {
      aToken.mInteger = int32_t(std::min(intPart, double(INT32_MAX)));
    } else {
      aToken.mInteger = int32_t(std::max(-intPart, double(INT32_MIN)));
    }
    aToken.mIntegerValid = true;
  }

  nsString& ident = aToken.mIdent;
  ident.Truncate();

  // Look at character that terminated the number
  if (c >= 0) {
    if (StartsIdent(c, Peek())) {
      if (GatherIdent(c, ident)) {
        type = eCSSToken_Dimension;
      }
    } else if ('%' == c) {
      type = eCSSToken_Percentage;
      value = value / 100.0f;
      aToken.mIntegerValid = false;
    } else {
      // Put back character that stopped numeric scan
      Pushback(c);
    }
  }
  aToken.mNumber = value;
  aToken.mType = type;
  return true;
}

bool
nsCSSScanner::SkipCComment()
{
  for (;;) {
    int32_t ch = Read();
    if (ch < 0) break;
    if (ch == '*') {
      if (LookAhead('/')) {
        return true;
      }
    }
  }

  mReporter->ReportUnexpectedEOF("PECommentEOF");
  return false;
}

bool
nsCSSScanner::ParseString(int32_t aStop, nsCSSToken& aToken)
{
  aToken.mIdent.SetLength(0);
  aToken.mType = eCSSToken_String;
  aToken.mSymbol = PRUnichar(aStop); // remember how it's quoted
  for (;;) {
    // If nothing in pushback, first try to get as much as possible in one go
    if (!mPushbackCount && mOffset < mCount) {
      // See how much we can consume and append in one go
      uint32_t n = mOffset;
      // Count number of characters that can be processed
      for (;n < mCount; ++n) {
        PRUnichar nextChar = mReadPointer[n];
        if ((nextChar == aStop) || (nextChar == '\\') ||
            (nextChar == '\n') || (nextChar == '\r') || (nextChar == '\f')) {
          break;
        }
      }
      // Add to the token what we have so far
      if (n > mOffset) {
        aToken.mIdent.Append(&mReadPointer[mOffset], n - mOffset);
        mOffset = n;
      }
    }
    int32_t ch = Read();
    if (ch < 0 || ch == aStop) {
      break;
    }
    if (ch == '\n') {
      aToken.mType = eCSSToken_Bad_String;
      mReporter->ReportUnexpected("SEUnterminatedString", aToken);
      break;
    }
    if (ch == '\\') {
      if (!ParseAndAppendEscape(aToken.mIdent, true)) {
        aToken.mType = eCSSToken_Bad_String;
        Pushback(ch);
        // For strings, the only case where ParseAndAppendEscape will
        // return false is when there's a backslash to start an escape
        // immediately followed by end-of-stream.  In that case, the
        // correct tokenization is badstring *followed* by a DELIM for
        // the backslash, but as far as the author is concerned, it
        // works pretty much the same as an unterminated string, so we
        // use the same error message.
        mReporter->ReportUnexpected("SEUnterminatedString", aToken);
        break;
      }
    } else {
      aToken.mIdent.Append(ch);
    }
  }
  return true;
}

// UNICODE-RANGE tokens match the regular expression
//
//     u\+[0-9a-f?]{1,6}(-[0-9a-f]{1,6})?
//
// However, some such tokens are "invalid".  There are three valid forms:
//
//     u+[0-9a-f]{x}              1 <= x <= 6
//     u+[0-9a-f]{x}\?{y}         1 <= x+y <= 6
//     u+[0-9a-f]{x}-[0-9a-f]{y}  1 <= x <= 6, 1 <= y <= 6
//
// All unicode-range tokens have their text recorded in mIdent; valid ones
// are also decoded into mInteger and mInteger2, and mIntegerValid is set.

bool
nsCSSScanner::ParseURange(int32_t aChar, nsCSSToken& aResult)
{
  int32_t intro2 = Read();
  int32_t ch = Peek();

  // We should only ever be called if these things are true.
  NS_ASSERTION(aChar == 'u' || aChar == 'U',
               "unicode-range called with improper introducer (U)");
  NS_ASSERTION(intro2 == '+',
               "unicode-range called with improper introducer (+)");

  // If the character immediately after the '+' is not a hex digit or
  // '?', this is not really a unicode-range token; push everything
  // back and scan the U as an ident.
  if (!IsHexDigit(ch) && ch != '?') {
    Pushback(intro2);
    Pushback(aChar);
    return ParseIdent(aChar, aResult);
  }

  aResult.mIdent.Truncate();
  aResult.mIdent.Append(aChar);
  aResult.mIdent.Append(intro2);

  bool valid = true;
  bool haveQues = false;
  uint32_t low = 0;
  uint32_t high = 0;
  int i = 0;

  for (;;) {
    ch = Read();
    i++;
    if (i == 7 || !(IsHexDigit(ch) || ch == '?')) {
      break;
    }

    aResult.mIdent.Append(ch);
    if (IsHexDigit(ch)) {
      if (haveQues) {
        valid = false; // all question marks should be at the end
      }
      low = low*16 + HexDigitValue(ch);
      high = high*16 + HexDigitValue(ch);
    } else {
      haveQues = true;
      low = low*16 + 0x0;
      high = high*16 + 0xF;
    }
  }

  if (ch == '-' && IsHexDigit(Peek())) {
    if (haveQues) {
      valid = false;
    }

    aResult.mIdent.Append(ch);
    high = 0;
    i = 0;
    for (;;) {
      ch = Read();
      i++;
      if (i == 7 || !IsHexDigit(ch)) {
        break;
      }
      aResult.mIdent.Append(ch);
      high = high*16 + HexDigitValue(ch);
    }
  }
  Pushback(ch);

  aResult.mInteger = low;
  aResult.mInteger2 = high;
  aResult.mIntegerValid = valid;
  aResult.mType = eCSSToken_URange;
  return true;
}

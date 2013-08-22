/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/* tokenization of CSS style sheets */

#include "nsCSSScanner.h"
#include "nsStyleUtil.h"
#include "mozilla/css/ErrorReporter.h"
#include "mozilla/Likely.h"
#include "mozilla/Util.h"
#include <algorithm>

using mozilla::ArrayLength;

/* Character class tables and related helper functions. */

static const uint8_t IS_HEX_DIGIT  = 0x01;
static const uint8_t IS_IDSTART    = 0x02;
static const uint8_t IS_IDCHAR     = 0x04;
static const uint8_t IS_URL_CHAR   = 0x08;
static const uint8_t IS_HSPACE     = 0x10;
static const uint8_t IS_VSPACE     = 0x20;
static const uint8_t IS_SPACE      = IS_HSPACE|IS_VSPACE;
static const uint8_t IS_STRING     = 0x40;

#define H    IS_HSPACE
#define V    IS_VSPACE
#define I    IS_IDCHAR
#define J    IS_IDSTART
#define U    IS_URL_CHAR
#define S    IS_STRING
#define X    IS_HEX_DIGIT

#define SH    S|H
#define SU    S|U
#define SUI   S|U|I
#define SUIJ  S|U|I|J
#define SUIX  S|U|I|X
#define SUIJX S|U|I|J|X

static const uint8_t gLexTable[] = {
// 00    01    02    03    04    05    06    07
    0,    S,    S,    S,    S,    S,    S,    S,
// 08   TAB    LF    0B    FF    CR    0E    0F
    S,   SH,    V,    S,    V,    V,    S,    S,
// 10    11    12    13    14    15    16    17
    S,    S,    S,    S,    S,    S,    S,    S,
// 18    19    1A    1B    1C    1D    1E    1F
    S,    S,    S,    S,    S,    S,    S,    S,
//SPC     !     "     #     $     %     &     '
   SH,   SU,    0,   SU,   SU,   SU,   SU,    0,
//  (     )     *     +     ,     -     .     /
    S,    S,   SU,   SU,   SU,  SUI,   SU,   SU,
//  0     1     2     3     4     5     6     7
 SUIX, SUIX, SUIX, SUIX, SUIX, SUIX, SUIX, SUIX,
//  8     9     :     ;     <     =     >     ?
 SUIX, SUIX,   SU,   SU,   SU,   SU,   SU,   SU,
//  @     A     B     C     D     E     F     G
   SU,SUIJX,SUIJX,SUIJX,SUIJX,SUIJX,SUIJX, SUIJ,
//  H     I     J     K     L     M     N     O
 SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ,
//  P     Q     R     S     T     U     V     W
 SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ,
//  X     Y     Z     [     \     ]     ^     _
 SUIJ, SUIJ, SUIJ,   SU,    J,   SU,   SU, SUIJ,
//  `     a     b     c     d     e     f     g
   SU,SUIJX,SUIJX,SUIJX,SUIJX,SUIJX,SUIJX, SUIJ,
//  h     i     j     k     l     m     n     o
 SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ,
//  p     q     r     s     t     u     v     w
 SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ,
//  x     y     z     {     |     }     ~    7F
 SUIJ, SUIJ, SUIJ,   SU,   SU,   SU,   SU,    S,
};

static_assert(MOZ_ARRAY_LENGTH(gLexTable) == 128,
              "gLexTable expected to cover all 128 ASCII characters");

#undef I
#undef J
#undef U
#undef S
#undef X
#undef SH
#undef SU
#undef SUI
#undef SUIJ
#undef SUIX
#undef SUIJX

/**
 * True if 'ch' is in character class 'cls', which should be one of
 * the constants above or some combination of them.  All characters
 * above U+007F are considered to be in 'cls'.  EOF is never in 'cls'.
 */
static inline bool
IsOpenCharClass(int32_t ch, uint8_t cls) {
  return ch >= 0 && (ch >= 128 || (gLexTable[ch] & cls) != 0);
}

/**
 * True if 'ch' is in character class 'cls', which should be one of
 * the constants above or some combination of them.  No characters
 * above U+007F are considered to be in 'cls'. EOF is never in 'cls'.
 */
static inline bool
IsClosedCharClass(int32_t ch, uint8_t cls) {
  return uint32_t(ch) < 128 && (gLexTable[ch] & cls) != 0;
}

/**
 * True if 'ch' is CSS whitespace, i.e. any of the ASCII characters
 * TAB, LF, FF, CR, or SPC.
 */
static inline bool
IsWhitespace(int32_t ch) {
  return IsClosedCharClass(ch, IS_SPACE);
}

/**
 * True if 'ch' is horizontal whitespace, i.e. TAB or SPC.
 */
static inline bool
IsHorzSpace(int32_t ch) {
  return IsClosedCharClass(ch, IS_HSPACE);
}

/**
 * True if 'ch' is vertical whitespace, i.e. LF, FF, or CR.  Vertical
 * whitespace requires special handling when consumed, see AdvanceLine.
 */
static inline bool
IsVertSpace(int32_t ch) {
  return IsClosedCharClass(ch, IS_VSPACE);
}

/**
 * True if 'ch' is a character that can appear in the middle of an identifier.
 * This includes U+0000 since it is handled as U+FFFD, but for purposes of
 * GatherText it should not be included in IsOpenCharClass.
 */
static inline bool
IsIdentChar(int32_t ch) {
  return IsOpenCharClass(ch, IS_IDCHAR) || ch == 0;
}

/**
 * True if 'ch' is a character that by itself begins an identifier.
 * This includes U+0000 since it is handled as U+FFFD, but for purposes of
 * GatherText it should not be included in IsOpenCharClass.
 * (This is a subset of IsIdentChar.)
 */
static inline bool
IsIdentStart(int32_t ch) {
  return IsOpenCharClass(ch, IS_IDSTART) || ch == 0;
}

/**
 * True if the two-character sequence aFirstChar+aSecondChar begins an
 * identifier.
 */
static inline bool
StartsIdent(int32_t aFirstChar, int32_t aSecondChar)
{
  return IsIdentStart(aFirstChar) ||
    (aFirstChar == '-' && IsIdentStart(aSecondChar));
}

/**
 * True if 'ch' is a decimal digit.
 */
static inline bool
IsDigit(int32_t ch) {
  return (ch >= '0') && (ch <= '9');
}

/**
 * True if 'ch' is a hexadecimal digit.
 */
static inline bool
IsHexDigit(int32_t ch) {
  return IsClosedCharClass(ch, IS_HEX_DIGIT);
}

/**
 * Assuming that 'ch' is a decimal digit, return its numeric value.
 */
static inline uint32_t
DecimalDigitValue(int32_t ch)
{
  return ch - '0';
}

/**
 * Assuming that 'ch' is a hexadecimal digit, return its numeric value.
 */
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

/**
 * If 'ch' can be the first character of a two-character match operator
 * token, return the token type code for that token, otherwise return
 * eCSSToken_Symbol to indicate that it can't.
 */
static inline nsCSSTokenType
MatchOperatorType(int32_t ch)
{
  switch (ch) {
  case '~': return eCSSToken_Includes;
  case '|': return eCSSToken_Dashmatch;
  case '^': return eCSSToken_Beginsmatch;
  case '$': return eCSSToken_Endsmatch;
  case '*': return eCSSToken_Containsmatch;
  default:  return eCSSToken_Symbol;
  }
}

/* Out-of-line nsCSSToken methods. */

/**
 * Append the textual representation of |this| to |aBuffer|.
 */
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
    case eCSSToken_Hash:
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

    case eCSSToken_Whitespace:
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

/* nsCSSScanner methods. */

nsCSSScanner::nsCSSScanner(const nsAString& aBuffer, uint32_t aLineNumber)
  : mBuffer(aBuffer.BeginReading())
  , mOffset(0)
  , mCount(aBuffer.Length())
  , mLineNumber(aLineNumber)
  , mLineOffset(0)
  , mTokenLineNumber(aLineNumber)
  , mTokenLineOffset(0)
  , mTokenOffset(0)
  , mRecordStartOffset(0)
  , mReporter(nullptr)
  , mSVGMode(false)
  , mRecording(false)
  , mSeenBadToken(false)
{
  MOZ_COUNT_CTOR(nsCSSScanner);
}

nsCSSScanner::~nsCSSScanner()
{
  MOZ_COUNT_DTOR(nsCSSScanner);
}

void
nsCSSScanner::StartRecording()
{
  MOZ_ASSERT(!mRecording, "already started recording");
  mRecording = true;
  mRecordStartOffset = mOffset;
}

void
nsCSSScanner::StopRecording()
{
  MOZ_ASSERT(mRecording, "haven't started recording");
  mRecording = false;
}

void
nsCSSScanner::StopRecording(nsString& aBuffer)
{
  MOZ_ASSERT(mRecording, "haven't started recording");
  mRecording = false;
  aBuffer.Append(mBuffer + mRecordStartOffset,
                 mOffset - mRecordStartOffset);
}

nsDependentSubstring
nsCSSScanner::GetCurrentLine() const
{
  uint32_t end = mTokenOffset;
  while (end < mCount && !IsVertSpace(mBuffer[end])) {
    end++;
  }
  return nsDependentSubstring(mBuffer + mTokenLineOffset,
                              mBuffer + end);
}

/**
 * Return the raw UTF-16 code unit at position |mOffset + n| within
 * the read buffer.  If that is beyond the end of the buffer, returns
 * -1 to indicate end of input.
 */
inline int32_t
nsCSSScanner::Peek(uint32_t n)
{
  if (mOffset + n >= mCount) {
    return -1;
  }
  return mBuffer[mOffset + n];
}

/**
 * Advance |mOffset| over |n| code units.  Advance(0) is a no-op.
 * If |n| is greater than the distance to end of input, will silently
 * stop at the end.  May not be used to advance over a line boundary;
 * AdvanceLine() must be used instead.
 */
inline void
nsCSSScanner::Advance(uint32_t n)
{
#ifdef DEBUG
  while (mOffset < mCount && n > 0) {
    MOZ_ASSERT(!IsVertSpace(mBuffer[mOffset]),
               "may not Advance() over a line boundary");
    mOffset++;
    n--;
  }
#else
  if (mOffset + n >= mCount || mOffset + n < mOffset)
    mOffset = mCount;
  else
    mOffset += n;
#endif
}

/**
 * Advance |mOffset| over a line boundary.
 */
void
nsCSSScanner::AdvanceLine()
{
  MOZ_ASSERT(IsVertSpace(mBuffer[mOffset]),
             "may not AdvanceLine() over a horizontal character");
  // Advance over \r\n as a unit.
  if (mBuffer[mOffset]   == '\r' && mOffset + 1 < mCount &&
      mBuffer[mOffset+1] == '\n')
    mOffset += 2;
  else
    mOffset += 1;
  // 0 is a magical line number meaning that we don't know (i.e., script)
  if (mLineNumber != 0)
    mLineNumber++;
  mLineOffset = mOffset;
}

/**
 * Back up |mOffset| over |n| code units.  Backup(0) is a no-op.
 * If |n| is greater than the distance to beginning of input, will
 * silently stop at the beginning.  May not be used to back up over a
 * line boundary.
 */
void
nsCSSScanner::Backup(uint32_t n)
{
#ifdef DEBUG
  while (mOffset > 0 && n > 0) {
    MOZ_ASSERT(!IsVertSpace(mBuffer[mOffset-1]),
               "may not Backup() over a line boundary");
    mOffset--;
    n--;
  }
#else
  if (mOffset < n)
    mOffset = 0;
  else
    mOffset -= n;
#endif
}

/**
 * Skip over a sequence of whitespace characters (vertical or
 * horizontal) starting at the current read position.
 */
void
nsCSSScanner::SkipWhitespace()
{
  for (;;) {
    int32_t ch = Peek();
    if (!IsWhitespace(ch)) { // EOF counts as non-whitespace
      break;
    }
    if (IsVertSpace(ch)) {
      AdvanceLine();
    } else {
      Advance();
    }
  }
}

/**
 * Skip over one CSS comment starting at the current read position.
 */
void
nsCSSScanner::SkipComment()
{
  MOZ_ASSERT(Peek() == '/' && Peek(1) == '*', "should not have been called");
  Advance(2);
  for (;;) {
    int32_t ch = Peek();
    if (ch < 0) {
      mReporter->ReportUnexpectedEOF("PECommentEOF");
      return;
    }
    if (ch == '*' && Peek(1) == '/') {
      Advance(2);
      return;
    }
    if (IsVertSpace(ch)) {
      AdvanceLine();
    } else {
      Advance();
    }
  }
}

/**
 * If there is a valid escape sequence starting at the current read
 * position, consume it, decode it, append the result to |aOutput|,
 * and return true.  Otherwise, consume nothing, leave |aOutput|
 * unmodified, and return false.  If |aInString| is true, accept the
 * additional form of escape sequence allowed within string-like tokens.
 */
bool
nsCSSScanner::GatherEscape(nsString& aOutput, bool aInString)
{
  MOZ_ASSERT(Peek() == '\\', "should not have been called");
  int32_t ch = Peek(1);
  if (ch < 0) {
    // If we are in a string (or a url() containing a string), we want to drop
    // the backslash on the floor.  Otherwise, we want to treat it as a U+FFFD
    // character.
    Advance();
    if (!aInString) {
      aOutput.Append(UCS2_REPLACEMENT_CHAR);
    }
    return true;
  }
  if (IsVertSpace(ch)) {
    if (aInString) {
      // In strings (and in url() containing a string), escaped
      // newlines are completely removed, to allow splitting over
      // multiple lines.
      Advance();
      AdvanceLine();
      return true;
    }
    // Outside of strings, backslash followed by a newline is not an escape.
    return false;
  }

  if (!IsHexDigit(ch)) {
    // "Any character (except a hexadecimal digit, linefeed, carriage
    // return, or form feed) can be escaped with a backslash to remove
    // its special meaning." -- CSS2.1 section 4.1.3
    Advance(2);
    if (ch == 0) {
      aOutput.Append(UCS2_REPLACEMENT_CHAR);
    } else {
      aOutput.Append(ch);
    }
    return true;
  }

  // "[at most six hexadecimal digits following a backslash] stand
  // for the ISO 10646 character with that number, which must not be
  // zero. (It is undefined in CSS 2.1 what happens if a style sheet
  // does contain a character with Unicode codepoint zero.)"
  //   -- CSS2.1 section 4.1.3

  // At this point we know we have \ followed by at least one
  // hexadecimal digit, therefore the escape sequence is valid and we
  // can go ahead and consume the backslash.
  Advance();
  uint32_t val = 0;
  int i = 0;
  do {
    val = val * 16 + HexDigitValue(ch);
    i++;
    Advance();
    ch = Peek();
  } while (i < 6 && IsHexDigit(ch));

  // "Interpret the hex digits as a hexadecimal number. If this number is zero,
  // or is greater than the maximum allowed codepoint, return U+FFFD
  // REPLACEMENT CHARACTER" -- CSS Syntax Level 3
  if (MOZ_UNLIKELY(val == 0)) {
    aOutput.Append(UCS2_REPLACEMENT_CHAR);
  } else {
    AppendUCS4ToUTF16(ENSURE_VALID_CHAR(val), aOutput);
  }

  // Consume exactly one whitespace character after a
  // hexadecimal escape sequence.
  if (IsVertSpace(ch)) {
    AdvanceLine();
  } else if (IsHorzSpace(ch)) {
    Advance();
  }
  return true;
}

/**
 * Consume a run of "text" beginning with the current read position,
 * consisting of characters in the class |aClass| (which must be a
 * suitable argument to IsOpenCharClass) plus escape sequences.
 * Append the text to |aText|, after decoding escape sequences.
 *
 * Returns true if at least one character was appended to |aText|,
 * false otherwise.
 */
bool
nsCSSScanner::GatherText(uint8_t aClass, nsString& aText)
{
  // This is all of the character classes currently used with
  // GatherText.  If you have a need to use this function with a
  // different class, go ahead and add it.
  MOZ_ASSERT(aClass == IS_STRING ||
             aClass == IS_IDCHAR ||
             aClass == IS_URL_CHAR,
             "possibly-inappropriate character class");

  uint32_t start = mOffset;
  bool inString = aClass == IS_STRING;

  for (;;) {
    // Consume runs of unescaped characters in one go.
    uint32_t n = mOffset;
    while (n < mCount && IsOpenCharClass(mBuffer[n], aClass)) {
      n++;
    }
    if (n > mOffset) {
      aText.Append(&mBuffer[mOffset], n - mOffset);
      mOffset = n;
    }
    if (n == mCount) {
      break;
    }

    int32_t ch = Peek();
    MOZ_ASSERT(!IsOpenCharClass(ch, aClass),
               "should not have exited the inner loop");
    if (ch == 0) {
      Advance();
      aText.Append(UCS2_REPLACEMENT_CHAR);
      continue;
    }

    if (ch != '\\') {
      break;
    }
    if (!GatherEscape(aText, inString)) {
      break;
    }
  }

  return mOffset > start;
}

/**
 * Scan an Ident token.  This also handles Function and URL tokens,
 * both of which begin indistinguishably from an identifier.  It can
 * produce a Symbol token when an apparent identifier actually led
 * into an invalid escape sequence.
 */
bool
nsCSSScanner::ScanIdent(nsCSSToken& aToken)
{
  if (MOZ_UNLIKELY(!GatherText(IS_IDCHAR, aToken.mIdent))) {
    aToken.mSymbol = Peek();
    Advance();
    return true;
  }

  if (MOZ_LIKELY(Peek() != '(')) {
    aToken.mType = eCSSToken_Ident;
    return true;
  }

  Advance();
  aToken.mType = eCSSToken_Function;
  if (aToken.mIdent.LowerCaseEqualsLiteral("url")) {
    NextURL(aToken);
  }
  return true;
}

/**
 * Scan an AtKeyword token.  Also handles production of Symbol when
 * an '@' is not followed by an identifier.
 */
bool
nsCSSScanner::ScanAtKeyword(nsCSSToken& aToken)
{
  MOZ_ASSERT(Peek() == '@', "should not have been called");

  // Fall back for when '@' isn't followed by an identifier.
  aToken.mSymbol = '@';
  Advance();

  int32_t ch = Peek();
  if (StartsIdent(ch, Peek(1))) {
    if (GatherText(IS_IDCHAR, aToken.mIdent)) {
       aToken.mType = eCSSToken_AtKeyword;
     }
  }
  return true;
}

/**
 * Scan a Hash token.  Handles the distinction between eCSSToken_ID
 * and eCSSToken_Hash, and handles production of Symbol when a '#'
 * is not followed by identifier characters.
 */
bool
nsCSSScanner::ScanHash(nsCSSToken& aToken)
{
  MOZ_ASSERT(Peek() == '#', "should not have been called");

  // Fall back for when '#' isn't followed by identifier characters.
  aToken.mSymbol = '#';
  Advance();

  int32_t ch = Peek();
  if (IsIdentChar(ch) || ch == '\\') {
    nsCSSTokenType type =
      StartsIdent(ch, Peek(1)) ? eCSSToken_ID : eCSSToken_Hash;
    aToken.mIdent.SetLength(0);
    if (GatherText(IS_IDCHAR, aToken.mIdent)) {
      aToken.mType = type;
    }
  }

  return true;
}

/**
 * Scan a Number, Percentage, or Dimension token (all of which begin
 * like a Number).  Can produce a Symbol when a '.' is not followed by
 * digits, or when '+' or '-' are not followed by either a digit or a
 * '.' and then a digit.  Can also produce a HTMLComment when it
 * encounters '-->'.
 */
bool
nsCSSScanner::ScanNumber(nsCSSToken& aToken)
{
  int32_t c = Peek();
#ifdef DEBUG
  {
    int32_t c2 = Peek(1);
    int32_t c3 = Peek(2);
    MOZ_ASSERT(IsDigit(c) ||
               (IsDigit(c2) && (c == '.' || c == '+' || c == '-')) ||
               (IsDigit(c3) && (c == '+' || c == '-') && c2 == '.'),
               "should not have been called");
  }
#endif

  // Sign of the mantissa (-1 or 1).
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

  aToken.mHasSign = (c == '+' || c == '-');
  if (aToken.mHasSign) {
    Advance();
    c = Peek();
  }

  bool gotDot = (c == '.');

  if (!gotDot) {
    // Scan the integer part of the mantissa.
    MOZ_ASSERT(IsDigit(c), "should have been excluded by logic above");
    do {
      intPart = 10*intPart + DecimalDigitValue(c);
      Advance();
      c = Peek();
    } while (IsDigit(c));

    gotDot = (c == '.') && IsDigit(Peek(1));
  }

  if (gotDot) {
    // Scan the fractional part of the mantissa.
    Advance();
    c = Peek();
    MOZ_ASSERT(IsDigit(c), "should have been excluded by logic above");
    // Power of ten by which we need to divide our next digit
    double divisor = 10;
    do {
      fracPart += DecimalDigitValue(c) / divisor;
      divisor *= 10;
      Advance();
      c = Peek();
    } while (IsDigit(c));
  }

  bool gotE = false;
  if (IsSVGMode() && (c == 'e' || c == 'E')) {
    int32_t expSignChar = Peek(1);
    int32_t nextChar = Peek(2);
    if (IsDigit(expSignChar) ||
        ((expSignChar == '-' || expSignChar == '+') && IsDigit(nextChar))) {
      gotE = true;
      if (expSignChar == '-') {
        expSign = -1;
      }
      Advance(); // consumes the E
      if (expSignChar == '-' || expSignChar == '+') {
        Advance();
        c = nextChar;
      } else {
        c = expSignChar;
      }
      MOZ_ASSERT(IsDigit(c), "should have been excluded by logic above");
      do {
        exponent = 10*exponent + DecimalDigitValue(c);
        Advance();
        c = Peek();
      } while (IsDigit(c));
    }
  }

  nsCSSTokenType type = eCSSToken_Number;

  // Set mIntegerValid for all cases (except %, below) because we need
  // it for the "2n" in :nth-child(2n).
  aToken.mIntegerValid = false;

  // Time to reassemble our number.
  // Do all the math in double precision so it's truncated only once.
  double value = sign * (intPart + fracPart);
  if (gotE) {
    // Explicitly cast expSign*exponent to double to avoid issues with
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

  // Check for Dimension and Percentage tokens.
  if (c >= 0) {
    if (StartsIdent(c, Peek(1))) {
      if (GatherText(IS_IDCHAR, ident)) {
        type = eCSSToken_Dimension;
      }
    } else if (c == '%') {
      Advance();
      type = eCSSToken_Percentage;
      value = value / 100.0f;
      aToken.mIntegerValid = false;
    }
  }
  aToken.mNumber = value;
  aToken.mType = type;
  return true;
}

/**
 * Scan a string constant ('foo' or "foo").  Will always produce
 * either a String or a Bad_String token; the latter occurs when the
 * close quote is missing.  Always returns true (for convenience in Next()).
 */
bool
nsCSSScanner::ScanString(nsCSSToken& aToken)
{
  int32_t aStop = Peek();
  MOZ_ASSERT(aStop == '"' || aStop == '\'', "should not have been called");
  aToken.mType = eCSSToken_String;
  aToken.mSymbol = PRUnichar(aStop); // Remember how it's quoted.
  Advance();

  for (;;) {
    GatherText(IS_STRING, aToken.mIdent);

    int32_t ch = Peek();
    if (ch == -1) {
      break; // EOF ends a string token with no error.
    }
    if (ch == aStop) {
      Advance();
      break;
    }
    // Both " and ' are excluded from IS_STRING.
    if (ch == '"' || ch == '\'') {
      aToken.mIdent.Append(ch);
      Advance();
      continue;
    }

    mSeenBadToken = true;
    aToken.mType = eCSSToken_Bad_String;
    mReporter->ReportUnexpected("SEUnterminatedString", aToken);
    break;
  }
  return true;
}

/**
 * Scan a unicode-range token.  These match the regular expression
 *
 *     u\+[0-9a-f?]{1,6}(-[0-9a-f]{1,6})?
 *
 * However, some such tokens are "invalid".  There are three valid forms:
 *
 *     u+[0-9a-f]{x}              1 <= x <= 6
 *     u+[0-9a-f]{x}\?{y}         1 <= x+y <= 6
 *     u+[0-9a-f]{x}-[0-9a-f]{y}  1 <= x <= 6, 1 <= y <= 6
 *
 * All unicode-range tokens have their text recorded in mIdent; valid ones
 * are also decoded into mInteger and mInteger2, and mIntegerValid is set.
 * Note that this does not validate the numeric range, only the syntactic
 * form.
 */
bool
nsCSSScanner::ScanURange(nsCSSToken& aResult)
{
  int32_t intro1 = Peek();
  int32_t intro2 = Peek(1);
  int32_t ch = Peek(2);

  MOZ_ASSERT((intro1 == 'u' || intro1 == 'U') &&
             intro2 == '+' &&
             (IsHexDigit(ch) || ch == '?'),
             "should not have been called");

  aResult.mIdent.Append(intro1);
  aResult.mIdent.Append(intro2);
  Advance(2);

  bool valid = true;
  bool haveQues = false;
  uint32_t low = 0;
  uint32_t high = 0;
  int i = 0;

  do {
    aResult.mIdent.Append(ch);
    if (IsHexDigit(ch)) {
      if (haveQues) {
        valid = false; // All question marks should be at the end.
      }
      low = low*16 + HexDigitValue(ch);
      high = high*16 + HexDigitValue(ch);
    } else {
      haveQues = true;
      low = low*16 + 0x0;
      high = high*16 + 0xF;
    }

    i++;
    Advance();
    ch = Peek();
  } while (i < 6 && (IsHexDigit(ch) || ch == '?'));

  if (ch == '-' && IsHexDigit(Peek(1))) {
    if (haveQues) {
      valid = false;
    }

    aResult.mIdent.Append(ch);
    Advance();
    ch = Peek();
    high = 0;
    i = 0;
    do {
      aResult.mIdent.Append(ch);
      high = high*16 + HexDigitValue(ch);

      i++;
      Advance();
      ch = Peek();
    } while (i < 6 && IsHexDigit(ch));
  }

  aResult.mInteger = low;
  aResult.mInteger2 = high;
  aResult.mIntegerValid = valid;
  aResult.mType = eCSSToken_URange;
  return true;
}

/**
 * Consume the part of an URL token after the initial 'url('.  Caller
 * is assumed to have consumed 'url(' already.  Will always produce
 * either an URL or a Bad_URL token.
 *
 * Exposed for use by nsCSSParser::ParseMozDocumentRule, which applies
 * the special lexical rules for URL tokens in a nonstandard context.
 */
bool
nsCSSScanner::NextURL(nsCSSToken& aToken)
{
  SkipWhitespace();

  int32_t ch = Peek();
  if (ch < 0) {
    return false;
  }

  // aToken.mIdent may be "url" at this point; clear that out
  aToken.mIdent.Truncate();

  // Do we have a string?
  if (ch == '"' || ch == '\'') {
    ScanString(aToken);
    if (MOZ_UNLIKELY(aToken.mType == eCSSToken_Bad_String)) {
      aToken.mType = eCSSToken_Bad_URL;
      return true;
    }
    MOZ_ASSERT(aToken.mType == eCSSToken_String, "unexpected token type");

  } else {
    // Otherwise, this is the start of a non-quoted url (which may be empty).
    aToken.mSymbol = PRUnichar(0);
    GatherText(IS_URL_CHAR, aToken.mIdent);
  }

  // Consume trailing whitespace and then look for a close parenthesis.
  SkipWhitespace();
  ch = Peek();
  if (MOZ_LIKELY(ch < 0 || ch == ')')) {
    Advance();
    aToken.mType = eCSSToken_URL;
  } else {
    mSeenBadToken = true;
    aToken.mType = eCSSToken_Bad_URL;
  }
  return true;
}

/**
 * Primary scanner entry point.  Consume one token and fill in
 * |aToken| accordingly.  Will skip over any number of comments first,
 * and will also skip over rather than return whitespace tokens if
 * |aSkipWS| is true.
 *
 * Returns true if it successfully consumed a token, false if EOF has
 * been reached.  Will always advance the current read position by at
 * least one character unless called when already at EOF.
 */
bool
nsCSSScanner::Next(nsCSSToken& aToken, bool aSkipWS)
{
  int32_t ch;

  // do this here so we don't have to do it in dozens of other places
  aToken.mIdent.Truncate();
  aToken.mType = eCSSToken_Symbol;

  for (;;) {
    // Consume any number of comments, and possibly also whitespace tokens,
    // in between other tokens.
    mTokenOffset = mOffset;
    mTokenLineOffset = mLineOffset;
    mTokenLineNumber = mLineNumber;

    ch = Peek();
    if (IsWhitespace(ch)) {
      SkipWhitespace();
      if (!aSkipWS) {
        aToken.mType = eCSSToken_Whitespace;
        return true;
      }
      continue; // start again at the beginning
    }
    if (ch == '/' && !IsSVGMode() && Peek(1) == '*') {
      // FIXME: Editor wants comments to be preserved (bug 60290).
      SkipComment();
      continue; // start again at the beginning
    }
    break;
  }

  // EOF
  if (ch < 0) {
    return false;
  }

  // 'u' could be UNICODE-RANGE or an identifier-family token
  if (ch == 'u' || ch == 'U') {
    int32_t c2 = Peek(1);
    int32_t c3 = Peek(2);
    if (c2 == '+' && (IsHexDigit(c3) || c3 == '?')) {
      return ScanURange(aToken);
    }
    return ScanIdent(aToken);
  }

  // identifier family
  if (IsIdentStart(ch)) {
    return ScanIdent(aToken);
  }

  // number family
  if (IsDigit(ch)) {
    return ScanNumber(aToken);
  }

  if (ch == '.' && IsDigit(Peek(1))) {
    return ScanNumber(aToken);
  }

  if (ch == '+') {
    int32_t c2 = Peek(1);
    if (IsDigit(c2) || (c2 == '.' && IsDigit(Peek(2)))) {
      return ScanNumber(aToken);
    }
  }

  // '-' can start an identifier-family token, a number-family token,
  // or an HTML-comment
  if (ch == '-') {
    int32_t c2 = Peek(1);
    int32_t c3 = Peek(2);
    if (IsIdentStart(c2)) {
      return ScanIdent(aToken);
    }
    if (IsDigit(c2) || (c2 == '.' && IsDigit(c3))) {
      return ScanNumber(aToken);
    }
    if (c2 == '-' && c3 == '>') {
      Advance(3);
      aToken.mType = eCSSToken_HTMLComment;
      aToken.mIdent.AssignLiteral("-->");
      return true;
    }
  }

  // the other HTML-comment token
  if (ch == '<' && Peek(1) == '!' && Peek(2) == '-' && Peek(3) == '-') {
    Advance(4);
    aToken.mType = eCSSToken_HTMLComment;
    aToken.mIdent.AssignLiteral("<!--");
    return true;
  }

  // AT_KEYWORD
  if (ch == '@') {
    return ScanAtKeyword(aToken);
  }

  // HASH
  if (ch == '#') {
    return ScanHash(aToken);
  }

  // STRING
  if (ch == '"' || ch == '\'') {
    return ScanString(aToken);
  }

  // Match operators: ~= |= ^= $= *=
  nsCSSTokenType opType = MatchOperatorType(ch);
  if (opType != eCSSToken_Symbol && Peek(1) == '=') {
    aToken.mType = opType;
    Advance(2);
    return true;
  }

  // Otherwise, a symbol (DELIM).
  aToken.mSymbol = ch;
  Advance();
  return true;
}

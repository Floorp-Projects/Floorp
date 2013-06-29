/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* tokenization of CSS style sheets */

#ifndef nsCSSScanner_h___
#define nsCSSScanner_h___

#include "nsString.h"

namespace mozilla {
namespace css {
class ErrorReporter;
}
}

// Token types; in close but not perfect correspondence to the token
// categorization in section 4.1.1 of CSS2.1.  (The deviations are all
// the fault of css3-selectors, which has requirements that can only be
// met by changing the generic tokenization.)  The comment on each line
// illustrates the form of each identifier.

enum nsCSSTokenType {
  // White space of any kind.  No value fields are used.  Note that
  // comments do *not* count as white space; comments separate tokens
  // but are not themselves tokens.
  eCSSToken_Whitespace,     //

  // Identifier-like tokens.  mIdent is the text of the identifier.
  // The difference between ID and Hash is: if the text after the #
  // would have been a valid Ident if the # hadn't been there, the
  // scanner produces an ID token.  Otherwise it produces a Hash token.
  // (This distinction is required by css3-selectors.)
  eCSSToken_Ident,          // word
  eCSSToken_Function,       // word(
  eCSSToken_AtKeyword,      // @word
  eCSSToken_ID,             // #word
  eCSSToken_Hash,           // #0word

  // Numeric tokens.  mNumber is the floating-point value of the
  // number, and mHasSign indicates whether there was an explicit sign
  // (+ or -) in front of the number.  If mIntegerValid is true, the
  // number had the lexical form of an integer, and mInteger is its
  // integer value.  Lexically integer values outside the range of a
  // 32-bit signed number are clamped to the maximum values; mNumber
  // will indicate a 'truer' value in that case.  Percentage tokens
  // are always considered not to be integers, even if their numeric
  // value is integral (100% => mNumber = 1.0).  For Dimension
  // tokens, mIdent holds the text of the unit.
  eCSSToken_Number,         // 1 -5 +2e3 3.14159 7.297352e-3
  eCSSToken_Dimension,      // 24px 8.5in
  eCSSToken_Percentage,     // 85% 1280.4%

  // String-like tokens.  In all cases, mIdent holds the text
  // belonging to the string, and mSymbol holds the delimiter
  // character, which may be ', ", or zero (only for unquoted URLs).
  // Bad_String and Bad_URL tokens are emitted when the closing
  // delimiter or parenthesis was missing.
  eCSSToken_String,         // 'foo bar' "foo bar"
  eCSSToken_Bad_String,     // 'foo bar
  eCSSToken_URL,            // url(foobar) url("foo bar")
  eCSSToken_Bad_URL,        // url(foo

  // Any one-character symbol.  mSymbol holds the character.
  eCSSToken_Symbol,         // . ; { } ! *

  // Match operators.  These are single tokens rather than pairs of
  // Symbol tokens because css3-selectors forbids the presence of
  // comments between the two characters.  No value fields are used;
  // the token type indicates which operator.
  eCSSToken_Includes,       // ~=
  eCSSToken_Dashmatch,      // |=
  eCSSToken_Beginsmatch,    // ^=
  eCSSToken_Endsmatch,      // $=
  eCSSToken_Containsmatch,  // *=

  // Unicode-range token: currently used only in @font-face.
  // The lexical rule for this token includes several forms that are
  // semantically invalid.  Therefore, mIdent always holds the
  // complete original text of the token (so we can print it
  // accurately in diagnostics), and mIntegerValid is true iff the
  // token is semantically valid.  In that case, mInteger holds the
  // lowest value included in the range, and mInteger2 holds the
  // highest value included in the range.
  eCSSToken_URange,         // U+007e U+01?? U+2000-206F

  // HTML comment delimiters, ignored as a unit when they appear at
  // the top level of a style sheet, for compatibility with websites
  // written for compatibility with pre-CSS browsers.  This token type
  // subsumes the css2.1 CDO and CDC tokens, which are always treated
  // the same by the parser.  mIdent holds the text of the token, for
  // diagnostics.
  eCSSToken_HTMLComment,    // <!-- -->
};

// A single token returned from the scanner.  mType is always
// meaningful; comments above describe which other fields are
// meaningful for which token types.
struct nsCSSToken {
  nsAutoString    mIdent;
  float           mNumber;
  int32_t         mInteger;
  int32_t         mInteger2;
  nsCSSTokenType  mType;
  PRUnichar       mSymbol;
  bool            mIntegerValid;
  bool            mHasSign;

  nsCSSToken()
    : mNumber(0), mInteger(0), mInteger2(0), mType(eCSSToken_Whitespace),
      mSymbol('\0'), mIntegerValid(false), mHasSign(false)
  {}

  bool IsSymbol(PRUnichar aSymbol) const {
    return mType == eCSSToken_Symbol && mSymbol == aSymbol;
  }

  void AppendToString(nsString& aBuffer) const;
};

// nsCSSScanner tokenizes an input stream using the CSS2.1 forward
// compatible tokenization rules.  Used internally by nsCSSParser;
// not available for use by other code.
class nsCSSScanner {
  public:
  // |aLineNumber == 1| is the beginning of a file, use |aLineNumber == 0|
  // when the line number is unknown.
  nsCSSScanner(const nsAString& aBuffer, uint32_t aLineNumber);
  ~nsCSSScanner();

  void SetErrorReporter(mozilla::css::ErrorReporter* aReporter) {
    mReporter = aReporter;
  }
  // Set whether or not we are processing SVG
  void SetSVGMode(bool aSVGMode) {
    mSVGMode = aSVGMode;
  }
  bool IsSVGMode() const {
    return mSVGMode;
  }

  // Reset or check whether a BAD_URL or BAD_STRING token has been seen.
  void ClearSeenBadToken() {
    mSeenBadToken = false;
  }

  bool SeenBadToken() const {
    return mSeenBadToken;
  }

  // Get the 1-based line number of the last character of
  // the most recently processed token.
  uint32_t GetLineNumber() const { return mTokenLineNumber; }

  // Get the 0-based column number of the first character of
  // the most recently processed token.
  uint32_t GetColumnNumber() const
  { return mTokenOffset - mTokenLineOffset; }

  // Get the text of the line containing the first character of
  // the most recently processed token.
  nsDependentSubstring GetCurrentLine() const;

  // Get the next token.  Return false on EOF.  aTokenResult is filled
  // in with the data for the token.  If aSkipWS is true, skip over
  // eCSSToken_Whitespace tokens rather than returning them.
  bool Next(nsCSSToken& aTokenResult, bool aSkipWS);

  // Get the body of an URL token (everything after the 'url(').
  // This is exposed for use by nsCSSParser::ParseMozDocumentRule,
  // which, for historical reasons, must make additional function
  // tokens behave like url().  Please do not add new uses to the
  // parser.
  bool NextURL(nsCSSToken& aTokenResult);

  // This is exposed for use by nsCSSParser::ParsePseudoClassWithNthPairArg,
  // because "2n-1" is a single DIMENSION token, and "n-1" is a single
  // IDENT token, but the :nth() selector syntax wants to interpret
  // them the same as "2n -1" and "n -1" respectively.  Please do not
  // add new uses to the parser.
  //
  // Note: this function may not be used to back up over a line boundary.
  void Backup(uint32_t n);

  // Starts recording the input stream from the current position.
  void StartRecording();

  // Abandons recording of the input stream.
  void StopRecording();

  // Stops recording of the input stream and appends the recorded
  // input to aBuffer.
  void StopRecording(nsString& aBuffer);

protected:
  int32_t Peek(uint32_t n = 0);
  void Advance(uint32_t n = 1);
  void AdvanceLine();

  void SkipWhitespace();
  void SkipComment();

  bool GatherEscape(nsString& aOutput, bool aInString);
  bool GatherText(uint8_t aClass, nsString& aIdent);

  bool ScanIdent(nsCSSToken& aResult);
  bool ScanAtKeyword(nsCSSToken& aResult);
  bool ScanHash(nsCSSToken& aResult);
  bool ScanNumber(nsCSSToken& aResult);
  bool ScanString(nsCSSToken& aResult);
  bool ScanURange(nsCSSToken& aResult);

  const PRUnichar *mBuffer;
  uint32_t mOffset;
  uint32_t mCount;

  uint32_t mLineNumber;
  uint32_t mLineOffset;

  uint32_t mTokenLineNumber;
  uint32_t mTokenLineOffset;
  uint32_t mTokenOffset;

  uint32_t mRecordStartOffset;

  mozilla::css::ErrorReporter *mReporter;

  // True if we are in SVG mode; false in "normal" CSS
  bool mSVGMode;
  bool mRecording;
  bool mSeenBadToken;
};

#endif /* nsCSSScanner_h___ */

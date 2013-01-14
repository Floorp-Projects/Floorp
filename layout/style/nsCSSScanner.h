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

// Token types
enum nsCSSTokenType {
  // A css identifier (e.g. foo)
  eCSSToken_Ident,          // mIdent

  // A css at keyword (e.g. @foo)
  eCSSToken_AtKeyword,      // mIdent

  // A css number without a percentage or dimension; with percentage;
  // without percentage but with a dimension
  eCSSToken_Number,         // mNumber
  eCSSToken_Percentage,     // mNumber
  eCSSToken_Dimension,      // mNumber + mIdent

  // A css string (e.g. "foo" or 'foo')
  eCSSToken_String,         // mSymbol + mIdent + mSymbol

  // Whitespace (e.g. " " or "/* abc */")
  eCSSToken_WhiteSpace,     // mIdent

  // A css symbol (e.g. ':', ';', '+', etc.)
  eCSSToken_Symbol,         // mSymbol

  // A css1 id (e.g. #foo3)
  eCSSToken_ID,             // mIdent
  // Just like eCSSToken_ID, except the part following the '#' is not
  // a valid CSS identifier (eg. starts with a digit, is the empty
  // string, etc).
  eCSSToken_Ref,            // mIdent

  eCSSToken_Function,       // mIdent

  eCSSToken_URL,            // mIdent + mSymbol
  eCSSToken_Bad_URL,        // mIdent + mSymbol

  eCSSToken_HTMLComment,    // "<!--" or "-->"

  eCSSToken_Includes,       // "~="
  eCSSToken_Dashmatch,      // "|="
  eCSSToken_Beginsmatch,    // "^="
  eCSSToken_Endsmatch,      // "$="
  eCSSToken_Containsmatch,  // "*="

  eCSSToken_URange,         // Low in mInteger, high in mInteger2;
                            // mIntegerValid is true if the token is a
                            // valid range; mIdent preserves the textual
                            // form of the token for error reporting

  // An unterminated string, which is always an error.
  eCSSToken_Bad_String      // mSymbol + mIdent
};

struct nsCSSToken {
  nsAutoString    mIdent NS_OKONHEAP;
  float           mNumber;
  int32_t         mInteger;
  int32_t         mInteger2;
  nsCSSTokenType  mType;
  PRUnichar       mSymbol;
  bool            mIntegerValid; // for number, dimension, urange
  bool            mHasSign; // for number, percentage, and dimension

  nsCSSToken();

  bool IsSymbol(PRUnichar aSymbol) {
    return bool((eCSSToken_Symbol == mType) && (mSymbol == aSymbol));
  }

  void AppendToString(nsString& aBuffer) const;
};

// CSS Scanner API. Used to tokenize an input stream using the CSS
// forward compatible tokenization rules. This implementation is
// private to this package and is only used internally by the css
// parser.
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

  // Get the next token. Return false on EOF. aTokenResult
  // is filled in with the data for the token.
  bool Next(nsCSSToken& aTokenResult);

  // Get the next token that may be a string or unquoted URL
  bool NextURL(nsCSSToken& aTokenResult);

  // It's really ugly that we have to expose this, but it's the easiest
  // way to do :nth-child() parsing sanely.  (In particular, in
  // :nth-child(2n-1), "2n-1" is a dimension, and we need to push the
  // "-1" back so we can read it again as a number.)
  void Pushback(PRUnichar aChar);

  // Starts recording the input stream from the current position.
  void StartRecording();

  // Abandons recording of the input stream.
  void StopRecording();

  // Stops recording of the input stream and appends the recorded
  // input to aBuffer.
  void StopRecording(nsString& aBuffer);

protected:
  int32_t Read();
  int32_t Peek();
  bool LookAhead(PRUnichar aChar);
  bool LookAheadOrEOF(PRUnichar aChar); // expect either aChar or EOF
  void EatWhiteSpace();

  bool ParseAndAppendEscape(nsString& aOutput, bool aInString);
  bool ParseIdent(int32_t aChar, nsCSSToken& aResult);
  bool ParseAtKeyword(nsCSSToken& aResult);
  bool ParseNumber(int32_t aChar, nsCSSToken& aResult);
  bool ParseRef(int32_t aChar, nsCSSToken& aResult);
  bool ParseString(int32_t aChar, nsCSSToken& aResult);
  bool ParseURange(int32_t aChar, nsCSSToken& aResult);
  bool SkipCComment();

  bool GatherIdent(int32_t aChar, nsString& aIdent);

  const PRUnichar *mReadPointer;
  uint32_t mOffset;
  uint32_t mCount;

  PRUnichar* mPushback;
  int32_t mPushbackCount;
  int32_t mPushbackSize;
  PRUnichar mLocalPushback[4];

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
};

#endif /* nsCSSScanner_h___ */

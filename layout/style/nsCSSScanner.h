/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* tokenization of CSS style sheets */

#ifndef nsCSSScanner_h___
#define nsCSSScanner_h___

#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/css/Loader.h"
#include "nsCSSStyleSheet.h"

// XXX turn this off for minimo builds
#define CSS_REPORT_PARSE_ERRORS

// for #ifdef CSS_REPORT_PARSE_ERRORS
#include "nsXPIDLString.h"
class nsIURI;

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
  PRInt32         mInteger;
  PRInt32         mInteger2;
  nsCSSTokenType  mType;
  PRUnichar       mSymbol;
  bool            mIntegerValid; // for number, dimension, urange
  bool            mHasSign; // for number, percentage, and dimension

  nsCSSToken();

  bool IsSymbol(PRUnichar aSymbol) {
    return bool((eCSSToken_Symbol == mType) && (mSymbol == aSymbol));
  }

  void AppendToString(nsString& aBuffer);
};

// CSS Scanner API. Used to tokenize an input stream using the CSS
// forward compatible tokenization rules. This implementation is
// private to this package and is only used internally by the css
// parser.
class nsCSSScanner {
  public:
  nsCSSScanner();
  ~nsCSSScanner();

  // Init the scanner.
  // |aLineNumber == 1| is the beginning of a file, use |aLineNumber == 0|
  // when the line number is unknown.
  void Init(const nsAString& aBuffer,
            nsIURI* aURI, PRUint32 aLineNumber,
            nsCSSStyleSheet* aSheet, mozilla::css::Loader* aLoader);
  void Close();

  static bool InitGlobals();
  static void ReleaseGlobals();

  // Set whether or not we are processing SVG
  void SetSVGMode(bool aSVGMode) {
    mSVGMode = aSVGMode;
  }
  bool IsSVGMode() const {
    return mSVGMode;
  }

#ifdef CSS_REPORT_PARSE_ERRORS
  void AddToError(const nsSubstring& aErrorText);
  void OutputError();
  void ClearError();

  // aMessage must take no parameters
  void ReportUnexpected(const char* aMessage);
  
private:
  void ReportUnexpectedParams(const char* aMessage,
                              const PRUnichar** aParams,
                              PRUint32 aParamsLength);

public:
  template<PRUint32 N>                           
  void ReportUnexpectedParams(const char* aMessage,
                              const PRUnichar* (&aParams)[N])
    {
      return ReportUnexpectedParams(aMessage, aParams, N);
    }
  // aLookingFor is a plain string, not a format string
  void ReportUnexpectedEOF(const char* aLookingFor);
  // aLookingFor is a single character
  void ReportUnexpectedEOF(PRUnichar aLookingFor);
  // aMessage must take 1 parameter (for the string representation of the
  // unexpected token)
  void ReportUnexpectedToken(nsCSSToken& tok, const char *aMessage);
  // aParams's first entry must be null, and we'll fill in the token
  void ReportUnexpectedTokenParams(nsCSSToken& tok,
                                   const char* aMessage,
                                   const PRUnichar **aParams,
                                   PRUint32 aParamsLength);
#endif

  PRUint32 GetLineNumber() { return mLineNumber; }

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
  PRInt32 Read();
  PRInt32 Peek();
  bool LookAhead(PRUnichar aChar);
  bool LookAheadOrEOF(PRUnichar aChar); // expect either aChar or EOF
  void EatWhiteSpace();

  bool ParseAndAppendEscape(nsString& aOutput, bool aInString);
  bool ParseIdent(PRInt32 aChar, nsCSSToken& aResult);
  bool ParseAtKeyword(PRInt32 aChar, nsCSSToken& aResult);
  bool ParseNumber(PRInt32 aChar, nsCSSToken& aResult);
  bool ParseRef(PRInt32 aChar, nsCSSToken& aResult);
  bool ParseString(PRInt32 aChar, nsCSSToken& aResult);
  bool ParseURange(PRInt32 aChar, nsCSSToken& aResult);
  bool SkipCComment();

  bool GatherIdent(PRInt32 aChar, nsString& aIdent);

  const PRUnichar *mReadPointer;
  PRUint32 mOffset;
  PRUint32 mCount;
  PRUnichar* mPushback;
  PRInt32 mPushbackCount;
  PRInt32 mPushbackSize;
  PRUnichar mLocalPushback[4];

  PRUint32 mLineNumber;
  // True if we are in SVG mode; false in "normal" CSS
  bool mSVGMode;
  bool mRecording;
  PRUint32 mRecordStartOffset;

#ifdef CSS_REPORT_PARSE_ERRORS
  nsXPIDLCString mFileName;
  nsCOMPtr<nsIURI> mURI;  // Cached so we know to not refetch mFileName
  PRUint32 mErrorLineNumber, mColNumber, mErrorColNumber;
  nsFixedString mError;
  PRUnichar mErrorBuf[200];
  PRUint64 mInnerWindowID;
  bool mWindowIDCached;
  nsCSSStyleSheet* mSheet;
  mozilla::css::Loader* mLoader;
#endif
};

#endif /* nsCSSScanner_h___ */

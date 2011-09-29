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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>
 *   Daniel Glazman <glazman@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* tokenization of CSS style sheets */

#ifndef nsCSSScanner_h___
#define nsCSSScanner_h___

#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/css/Loader.h"
#include "nsCSSStyleSheet.h"

class nsIUnicharInputStream;

// XXX turn this off for minimo builds
#define CSS_REPORT_PARSE_ERRORS

#define CSS_BUFFER_SIZE 256

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
  // Either aInput or (aBuffer and aCount) must be set.
  void Init(nsIUnicharInputStream* aInput, 
            const PRUnichar *aBuffer, PRUint32 aCount,
            nsIURI* aURI, PRUint32 aLineNumber,
            nsCSSStyleSheet* aSheet, mozilla::css::Loader* aLoader);
  void Close();

  static bool InitGlobals();
  static void ReleaseGlobals();

  // Set whether or not we are processing SVG
  void SetSVGMode(bool aSVGMode) {
    NS_ASSERTION(aSVGMode == PR_TRUE || aSVGMode == PR_FALSE,
                 "bad bool value");
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
  void ReportUnexpectedParams(const char* aMessage,
                              const PRUnichar **aParams,
                              PRUint32 aParamsLength);
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

  // Get the next token. Return PR_FALSE on EOF. aTokenResult
  // is filled in with the data for the token.
  bool Next(nsCSSToken& aTokenResult);

  // Get the next token that may be a string or unquoted URL
  bool NextURL(nsCSSToken& aTokenResult);

  // It's really ugly that we have to expose this, but it's the easiest
  // way to do :nth-child() parsing sanely.  (In particular, in
  // :nth-child(2n-1), "2n-1" is a dimension, and we need to push the
  // "-1" back so we can read it again as a number.)
  void Pushback(PRUnichar aChar);

  // Reports operating-system level errors, e.g. read failures and
  // out of memory.
  nsresult GetLowLevelError();

  // sometimes the parser wants to make note of a low-level error
  void SetLowLevelError(nsresult aErrorCode);
  
protected:
  bool EnsureData();
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

  // Only used when input is a stream
  nsCOMPtr<nsIUnicharInputStream> mInputStream;
  PRUnichar mBuffer[CSS_BUFFER_SIZE];

  const PRUnichar *mReadPointer;
  PRUint32 mOffset;
  PRUint32 mCount;
  PRUnichar* mPushback;
  PRInt32 mPushbackCount;
  PRInt32 mPushbackSize;
  PRUnichar mLocalPushback[4];
  nsresult mLowLevelError;

  PRUint32 mLineNumber;
  // True if we are in SVG mode; false in "normal" CSS
  bool mSVGMode;
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

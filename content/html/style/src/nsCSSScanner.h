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
#ifndef nsCSSScanner_h___
#define nsCSSScanner_h___

#include "nsString.h"
class nsIUnicharInputStream;

// XXX turn this off for minimo builds
#define CSS_REPORT_PARSE_ERRORS

// for #ifdef CSS_REPORT_PARSE_ERRORS
#include "nsXPIDLString.h"
class nsIURI;

// Token types
enum nsCSSTokenType {
  // A css identifier (e.g. foo)
  eCSSToken_Ident = 0,          // mIdent

  // A css at keyword (e.g. @foo)
  eCSSToken_AtKeyword = 1,      // mIdent

  // A css number without a percentage or dimension; with percentage;
  // without percentage but with a dimension
  eCSSToken_Number = 2,         // mNumber
  eCSSToken_Percentage = 3,     // mNumber
  eCSSToken_Dimension = 4,      // mNumber + mIdent

  // A css string (e.g. "foo" or 'foo')
  eCSSToken_String = 5,         // mSymbol + mIdent + mSymbol

  // Whitespace (e.g. " " or "/* abc */")
  eCSSToken_WhiteSpace = 6,     // mIdent

  // A css symbol (e.g. ':', ';', '+', etc.)
  eCSSToken_Symbol = 7,         // mSymbol

  // A css1 id (e.g. #foo3)
  eCSSToken_ID = 8,             // mIdent

  eCSSToken_Function = 9,       // mIdent

  eCSSToken_URL = 10,           // mIdent
  eCSSToken_InvalidURL = 11,    // doesn't matter

  eCSSToken_HTMLComment = 12,    // "<!--" or "-->"

  eCSSToken_Includes = 13,      // "~="
  eCSSToken_Dashmatch = 14,     // "|="
  eCSSToken_Beginsmatch = 15,   // "^="
  eCSSToken_Endsmatch = 16,     // "$="
  eCSSToken_Containsmatch = 17  // "*="

};

struct nsCSSToken {
  nsCSSTokenType  mType;
  PRPackedBool    mIntegerValid;
  nsAutoString    mIdent;
  float           mNumber;
  PRInt32         mInteger;
  PRUnichar       mSymbol;

  nsCSSToken();

  PRBool IsDimension() {
    return PRBool((eCSSToken_Dimension == mType) ||
                  ((eCSSToken_Number == mType) && (mNumber == 0.0f)));
  }

  PRBool IsSymbol(PRUnichar aSymbol) {
    return PRBool((eCSSToken_Symbol == mType) && (mSymbol == aSymbol));
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
  void Init(nsIUnicharInputStream* aInput, nsIURI* aURI, PRUint32 aLineNumber);

  static PRBool InitGlobals();
  static void ReleaseGlobals();

#ifdef CSS_REPORT_PARSE_ERRORS
  NS_HIDDEN_(void) AddToError(const nsSubstring& aErrorText);
  NS_HIDDEN_(void) OutputError();
  NS_HIDDEN_(void) ClearError();

  // aMessage must take no parameters
  NS_HIDDEN_(void) ReportUnexpected(const char* aMessage);
  NS_HIDDEN_(void) ReportUnexpectedParams(const char* aMessage,
                                          const PRUnichar **aParams,
                                          PRUint32 aParamsLength);
  // aMessage must take no parameters
  NS_HIDDEN_(void) ReportUnexpectedEOF(const char* aLookingFor);
  // aMessage must take 1 parameter (for the string representation of the
  // unexpected token)
  NS_HIDDEN_(void) ReportUnexpectedToken(nsCSSToken& tok,
                                         const char *aMessage);
  // aParams's first entry must be null, and we'll fill in the token
  NS_HIDDEN_(void) ReportUnexpectedTokenParams(nsCSSToken& tok,
                                               const char* aMessage,
                                               const PRUnichar **aParams,
                                               PRUint32 aParamsLength);
#endif

  PRUint32 GetLineNumber() { return mLineNumber; }

  // Get the next token. Return nsfalse on EOF or ERROR. aTokenResult
  // is filled in with the data for the token.
  PRBool Next(nsresult& aErrorCode, nsCSSToken& aTokenResult);

  // Get the next token that may be a string or unquoted URL or whitespace
  PRBool NextURL(nsresult& aErrorCode, nsCSSToken& aTokenResult);

  static inline PRBool
  IsIdentStart(PRInt32 aChar, const PRUint8* aLexTable)
  {
    return aChar >= 0 &&
      (aChar >= 256 || (aLexTable[aChar] & START_IDENT) != 0);
  }

  static inline PRBool
  StartsIdent(PRInt32 aFirstChar, PRInt32 aSecondChar,
              const PRUint8* aLexTable)
  {
    return IsIdentStart(aFirstChar, aLexTable) ||
      (aFirstChar == '-' && IsIdentStart(aSecondChar, aLexTable));
  }

  static inline const PRUint8* GetLexTable() {
    return gLexTable;
  }
  
protected:
  void Close();
  PRInt32 Read(nsresult& aErrorCode);
  PRInt32 Peek(nsresult& aErrorCode);
  void Unread();
  void Pushback(PRUnichar aChar);
  PRBool LookAhead(nsresult& aErrorCode, PRUnichar aChar);
  PRBool EatWhiteSpace(nsresult& aErrorCode);
  PRBool EatNewline(nsresult& aErrorCode);

  PRInt32 ParseEscape(nsresult& aErrorCode);
  PRBool ParseIdent(nsresult& aErrorCode, PRInt32 aChar, nsCSSToken& aResult);
  PRBool ParseAtKeyword(nsresult& aErrorCode, PRInt32 aChar,
                        nsCSSToken& aResult);
  PRBool ParseNumber(nsresult& aErrorCode, PRInt32 aChar, nsCSSToken& aResult);
  PRBool ParseID(nsresult& aErrorCode, PRInt32 aChar, nsCSSToken& aResult);
  PRBool ParseString(nsresult& aErrorCode, PRInt32 aChar, nsCSSToken& aResult);
#if 0
  PRBool ParseEOLComment(nsresult& aErrorCode, nsCSSToken& aResult);
  PRBool ParseCComment(nsresult& aErrorCode, nsCSSToken& aResult);
#endif
  PRBool SkipCComment(nsresult& aErrorCode);

  PRBool GatherString(nsresult& aErrorCode, PRInt32 aStop,
                      nsString& aString);
  PRBool GatherIdent(nsresult& aErrorCode, PRInt32 aChar, nsString& aIdent);

  nsIUnicharInputStream* mInput;
  PRUnichar* mBuffer;
  PRInt32 mOffset;
  PRInt32 mCount;
  PRUnichar* mPushback;
  PRInt32 mPushbackCount;
  PRInt32 mPushbackSize;
  PRInt32 mLastRead;
  PRUnichar mLocalPushback[4];

  PRUint32 mLineNumber;
#ifdef CSS_REPORT_PARSE_ERRORS
  nsXPIDLCString mFileName;
  PRUint32 mErrorLineNumber, mColNumber, mErrorColNumber;
  nsFixedString mError;
  PRUnichar mErrorBuf[200];
#endif

  static const PRUint8 IS_DIGIT;
  static const PRUint8 IS_HEX_DIGIT;
  static const PRUint8 IS_ALPHA;
  static const PRUint8 START_IDENT;
  static const PRUint8 IS_IDENT;
  static const PRUint8 IS_WHITESPACE;

  static PRUint8 gLexTable[256];
  static void BuildLexTable();
  static PRBool CheckLexTable(PRInt32 aChar, PRUint8 aBit, PRUint8* aLexTable);
};

#endif /* nsCSSScanner_h___ */

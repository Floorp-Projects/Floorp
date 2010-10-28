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
#include <math.h>

/* tokenization of CSS style sheets */

#include "nsCSSScanner.h"
#include "nsIFactory.h"
#include "nsIInputStream.h"
#include "nsIUnicharInputStream.h"
#include "nsString.h"
#include "nsCRT.h"

// for #ifdef CSS_REPORT_PARSE_ERRORS
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsContentUtils.h"
#include "mozilla/Services.h"

#ifdef CSS_REPORT_PARSE_ERRORS
static PRBool gReportErrors = PR_TRUE;
static nsIConsoleService *gConsoleService;
static nsIFactory *gScriptErrorFactory;
static nsIStringBundle *gStringBundle;
#endif

// Don't bother collecting whitespace characters in token's mIdent buffer
#undef COLLECT_WHITESPACE

// Table of character classes
static const PRUnichar CSS_ESCAPE  = PRUnichar('\\');

static const PRUint8 IS_HEX_DIGIT  = 0x01;
static const PRUint8 START_IDENT   = 0x02;
static const PRUint8 IS_IDENT      = 0x04;
static const PRUint8 IS_WHITESPACE = 0x08;

#define W   IS_WHITESPACE
#define I   IS_IDENT
#define S            START_IDENT
#define SI  IS_IDENT|START_IDENT
#define XI  IS_IDENT            |IS_HEX_DIGIT
#define XSI IS_IDENT|START_IDENT|IS_HEX_DIGIT

static const PRUint8 gLexTable[256] = {
//                                     TAB LF      FF  CR
   0,  0,  0,  0,  0,  0,  0,  0,  0,  W,  W,  0,  W,  W,  0,  0,
//
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
// SPC !   "   #   $   %   &   '   (   )   *   +   ,   -   .   /
   W,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  I,  0,  0,
// 0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ?
   XI, XI, XI, XI, XI, XI, XI, XI, XI, XI, 0,  0,  0,  0,  0,  0,
// @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O
   0,  XSI,XSI,XSI,XSI,XSI,XSI,SI, SI, SI, SI, SI, SI, SI, SI, SI,
// P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _
   SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, 0,  S,  0,  0,  SI,
// `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o
   0,  XSI,XSI,XSI,XSI,XSI,XSI,SI, SI, SI, SI, SI, SI, SI, SI, SI,
// p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~
   SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, 0,  0,  0,  0,  0,
//
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
//
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
// NBSP¡   ¢   £   ¤   ¥   ¦   §   ¨   ©   ª   «   ¬   ­   ®   ¯
   SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI,
// °   ±   ²   ³   ´   µ   ¶   ·   ¸   ¹   º   »   ¼   ½   ¾   ¿
   SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI,
// À   Á   Â   Ã   Ä   Å   Æ   Ç   È   É   Ê   Ë   Ì   Í   Î   Ï
   SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI,
// Ð   Ñ   Ò   Ó   Ô   Õ   Ö   ×   Ø   Ù   Ú   Û   Ü   Ý   Þ   ß
   SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI,
// à   á   â   ã   ä   å   æ   ç   è   é   ê   ë   ì   í   î   ï
   SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI,
// ð   ñ   ò   ó   ô   õ   ö   ÷   ø   ù   ú   û   ü   ý   þ   ÿ
   SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI, SI,
};

#undef W
#undef S
#undef I
#undef XI
#undef SI
#undef XSI

static inline PRBool
IsIdentStart(PRInt32 aChar)
{
  return aChar >= 0 &&
    (aChar >= 256 || (gLexTable[aChar] & START_IDENT) != 0);
}

static inline PRBool
StartsIdent(PRInt32 aFirstChar, PRInt32 aSecondChar)
{
  return IsIdentStart(aFirstChar) ||
    (aFirstChar == '-' && IsIdentStart(aSecondChar));
}

static inline PRBool
IsWhitespace(PRInt32 ch) {
  return PRUint32(ch) < 256 && (gLexTable[ch] & IS_WHITESPACE) != 0;
}

static inline PRBool
IsDigit(PRInt32 ch) {
  return (ch >= '0') && (ch <= '9');
}

static inline PRBool
IsHexDigit(PRInt32 ch) {
  return PRUint32(ch) < 256 && (gLexTable[ch] & IS_HEX_DIGIT) != 0;
}

static inline PRBool
IsIdent(PRInt32 ch) {
  return ch >= 0 && (ch >= 256 || (gLexTable[ch] & IS_IDENT) != 0);
}

static inline PRUint32
DecimalDigitValue(PRInt32 ch)
{
  return ch - '0';
}

static inline PRUint32
HexDigitValue(PRInt32 ch)
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
nsCSSToken::AppendToString(nsString& aBuffer)
{
  switch (mType) {
    case eCSSToken_AtKeyword:
      aBuffer.Append(PRUnichar('@')); // fall through intentional
    case eCSSToken_Ident:
    case eCSSToken_WhiteSpace:
    case eCSSToken_Function:
    case eCSSToken_URL:
    case eCSSToken_InvalidURL:
    case eCSSToken_HTMLComment:
    case eCSSToken_URange:
      aBuffer.Append(mIdent);
      if (mType == eCSSToken_Function)
        aBuffer.Append(PRUnichar('('));
      break;
    case eCSSToken_Number:
      if (mIntegerValid) {
        aBuffer.AppendInt(mInteger, 10);
      }
      else {
        aBuffer.AppendFloat(mNumber);
      }
      break;
    case eCSSToken_Percentage:
      NS_ASSERTION(!mIntegerValid, "How did a percentage token get this set?");
      aBuffer.AppendFloat(mNumber * 100.0f);
      aBuffer.Append(PRUnichar('%')); // STRING USE WARNING: technically, this should be |AppendWithConversion|
      break;
    case eCSSToken_Dimension:
      if (mIntegerValid) {
        aBuffer.AppendInt(mInteger, 10);
      }
      else {
        aBuffer.AppendFloat(mNumber);
      }
      aBuffer.Append(mIdent);
      break;
    case eCSSToken_String:
      aBuffer.Append(mSymbol);
      aBuffer.Append(mIdent); // fall through intentional
    case eCSSToken_Symbol:
      aBuffer.Append(mSymbol);
      break;
    case eCSSToken_ID:
    case eCSSToken_Ref:
      aBuffer.Append(PRUnichar('#'));
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
    case eCSSToken_Error:
      aBuffer.Append(mSymbol);
      aBuffer.Append(mIdent);
      break;
    default:
      NS_ERROR("invalid token type");
      break;
  }
}

nsCSSScanner::nsCSSScanner()
  : mInputStream(nsnull)
  , mReadPointer(nsnull)
  , mLowLevelError(NS_OK)
#ifdef MOZ_SVG
  , mSVGMode(PR_FALSE)
#endif
#ifdef CSS_REPORT_PARSE_ERRORS
  , mError(mErrorBuf, NS_ARRAY_LENGTH(mErrorBuf), 0)
#endif
{
  MOZ_COUNT_CTOR(nsCSSScanner);
  mPushback = mLocalPushback;
  mPushbackSize = NS_ARRAY_LENGTH(mLocalPushback);
  // No need to init the other members, since they represent state
  // which can get cleared.  We'll init them every time Init() is
  // called.
}

nsCSSScanner::~nsCSSScanner()
{
  MOZ_COUNT_DTOR(nsCSSScanner);
  Close();
  if (mLocalPushback != mPushback) {
    delete [] mPushback;
  }
}

nsresult
nsCSSScanner::GetLowLevelError()
{
  return mLowLevelError;
}

void
nsCSSScanner::SetLowLevelError(nsresult aErrorCode)
{
  NS_ASSERTION(aErrorCode != NS_OK, "SetLowLevelError() used to clear error");
  NS_ASSERTION(mLowLevelError == NS_OK, "there is already a low-level error");
  mLowLevelError = aErrorCode;
}

#ifdef CSS_REPORT_PARSE_ERRORS
#define CSS_ERRORS_PREF "layout.css.report_errors"

static int
CSSErrorsPrefChanged(const char *aPref, void *aClosure)
{
  gReportErrors = nsContentUtils::GetBoolPref(CSS_ERRORS_PREF, PR_TRUE);
  return NS_OK;
}
#endif

/* static */ PRBool
nsCSSScanner::InitGlobals()
{
#ifdef CSS_REPORT_PARSE_ERRORS
  if (gConsoleService && gScriptErrorFactory)
    return PR_TRUE;
  
  nsresult rv = CallGetService(NS_CONSOLESERVICE_CONTRACTID, &gConsoleService);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  rv = CallGetClassObject(NS_SCRIPTERROR_CONTRACTID, &gScriptErrorFactory);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  NS_ASSERTION(gConsoleService && gScriptErrorFactory,
               "unexpected null pointer without failure");

  nsContentUtils::RegisterPrefCallback(CSS_ERRORS_PREF, CSSErrorsPrefChanged, nsnull);
  CSSErrorsPrefChanged(CSS_ERRORS_PREF, nsnull);
#endif
  return PR_TRUE;
}

/* static */ void
nsCSSScanner::ReleaseGlobals()
{
#ifdef CSS_REPORT_PARSE_ERRORS
  nsContentUtils::UnregisterPrefCallback(CSS_ERRORS_PREF, CSSErrorsPrefChanged, nsnull);
  NS_IF_RELEASE(gConsoleService);
  NS_IF_RELEASE(gScriptErrorFactory);
  NS_IF_RELEASE(gStringBundle);
#endif
}

void
nsCSSScanner::Init(nsIUnicharInputStream* aInput, 
                   const PRUnichar * aBuffer, PRUint32 aCount, 
                   nsIURI* aURI, PRUint32 aLineNumber)
{
  NS_PRECONDITION(!mInputStream, "Should not have an existing input stream!");
  NS_PRECONDITION(!mReadPointer, "Should not have an existing input buffer!");

  // Read from stream via my own buffer
  if (aInput) {
    NS_PRECONDITION(!aBuffer, "Shouldn't have both input and buffer!");
    NS_PRECONDITION(aCount == 0, "Shouldn't have count with a stream");
    mInputStream = aInput;
    mReadPointer = mBuffer;
    mCount = 0;
  } else {
    NS_PRECONDITION(aBuffer, "Either aInput or aBuffer must be set");
    // Read directly from the provided buffer
    mInputStream = nsnull;
    mReadPointer = aBuffer;
    mCount = aCount;
  }

#ifdef CSS_REPORT_PARSE_ERRORS
  // If aURI is the same as mURI, no need to reget mFileName -- it
  // shouldn't have changed.
  if (aURI != mURI) {
    mURI = aURI;
    if (aURI) {
      aURI->GetSpec(mFileName);
    } else {
      mFileName.Adopt(NS_strdup("from DOM"));
    }
  }
#endif // CSS_REPORT_PARSE_ERRORS
  mLineNumber = aLineNumber;

  // Reset variables that we use to keep track of our progress through the input
  mOffset = 0;
  mPushbackCount = 0;
  mLowLevelError = NS_OK;

#ifdef CSS_REPORT_PARSE_ERRORS
  mColNumber = 0;
#endif
}

#ifdef CSS_REPORT_PARSE_ERRORS

// @see REPORT_UNEXPECTED_EOF in nsCSSParser.cpp
#define REPORT_UNEXPECTED_EOF(lf_) \
  ReportUnexpectedEOF(#lf_)

void
nsCSSScanner::AddToError(const nsSubstring& aErrorText)
{
  if (mError.IsEmpty()) {
    mErrorLineNumber = mLineNumber;
    mErrorColNumber = mColNumber;
    mError = aErrorText;
  } else {
    mError.Append(NS_LITERAL_STRING("  ") + aErrorText);
  }
}

void
nsCSSScanner::ClearError()
{
  mError.Truncate();
}

void
nsCSSScanner::OutputError()
{
  if (mError.IsEmpty()) return;
 
  // Log it to the Error console

  if (InitGlobals() && gReportErrors) {
    nsresult rv;
    nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(gScriptErrorFactory, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = errorObject->Init(mError.get(),
                             NS_ConvertUTF8toUTF16(mFileName).get(),
                             EmptyString().get(),
                             mErrorLineNumber,
                             mErrorColNumber,
                             nsIScriptError::warningFlag,
                             "CSS Parser");
      if (NS_SUCCEEDED(rv))
        gConsoleService->LogMessage(errorObject);
    }
  }
  ClearError();
}

static PRBool
InitStringBundle()
{
  if (gStringBundle)
    return PR_TRUE;

  nsCOMPtr<nsIStringBundleService> sbs =
    mozilla::services::GetStringBundleService();
  if (!sbs)
    return PR_FALSE;

  nsresult rv = 
    sbs->CreateBundle("chrome://global/locale/css.properties", &gStringBundle);
  if (NS_FAILED(rv)) {
    gStringBundle = nsnull;
    return PR_FALSE;
  }

  return PR_TRUE;
}

#define ENSURE_STRINGBUNDLE \
  PR_BEGIN_MACRO if (!InitStringBundle()) return; PR_END_MACRO

// aMessage must take no parameters
void nsCSSScanner::ReportUnexpected(const char* aMessage)
{
  ENSURE_STRINGBUNDLE;

  nsXPIDLString str;
  gStringBundle->GetStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                   getter_Copies(str));
  AddToError(str);
}
  
void
nsCSSScanner::ReportUnexpectedParams(const char* aMessage,
                                     const PRUnichar **aParams,
                                     PRUint32 aParamsLength)
{
  NS_PRECONDITION(aParamsLength > 0, "use the non-params version");
  ENSURE_STRINGBUNDLE;

  nsXPIDLString str;
  gStringBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                      aParams, aParamsLength,
                                      getter_Copies(str));
  AddToError(str);
}

// aLookingFor is a plain string, not a format string
void
nsCSSScanner::ReportUnexpectedEOF(const char* aLookingFor)
{
  ENSURE_STRINGBUNDLE;

  nsXPIDLString innerStr;
  gStringBundle->GetStringFromName(NS_ConvertASCIItoUTF16(aLookingFor).get(),
                                   getter_Copies(innerStr));

  const PRUnichar *params[] = {
    innerStr.get()
  };
  nsXPIDLString str;
  gStringBundle->FormatStringFromName(NS_LITERAL_STRING("PEUnexpEOF2").get(),
                                      params, NS_ARRAY_LENGTH(params),
                                      getter_Copies(str));
  AddToError(str);
}

// aLookingFor is a single character
void
nsCSSScanner::ReportUnexpectedEOF(PRUnichar aLookingFor)
{
  ENSURE_STRINGBUNDLE;

  const PRUnichar lookingForStr[] = {
    PRUnichar('\''), aLookingFor, PRUnichar('\''), PRUnichar(0)
  };
  const PRUnichar *params[] = { lookingForStr };
  nsXPIDLString str;
  gStringBundle->FormatStringFromName(NS_LITERAL_STRING("PEUnexpEOF2").get(),
                                      params, NS_ARRAY_LENGTH(params),
                                      getter_Copies(str));
  AddToError(str);
}

// aMessage must take 1 parameter (for the string representation of the
// unexpected token)
void
nsCSSScanner::ReportUnexpectedToken(nsCSSToken& tok,
                                    const char *aMessage)
{
  ENSURE_STRINGBUNDLE;
  
  nsAutoString tokenString;
  tok.AppendToString(tokenString);

  const PRUnichar *params[] = {
    tokenString.get()
  };

  ReportUnexpectedParams(aMessage, params, NS_ARRAY_LENGTH(params));
}

// aParams's first entry must be null, and we'll fill in the token
void
nsCSSScanner::ReportUnexpectedTokenParams(nsCSSToken& tok,
                                          const char* aMessage,
                                          const PRUnichar **aParams,
                                          PRUint32 aParamsLength)
{
  NS_PRECONDITION(aParamsLength > 1, "use the non-params version");
  NS_PRECONDITION(aParams[0] == nsnull, "first param should be empty");

  ENSURE_STRINGBUNDLE;
  
  nsAutoString tokenString;
  tok.AppendToString(tokenString);
  aParams[0] = tokenString.get();

  ReportUnexpectedParams(aMessage, aParams, aParamsLength);
}

#else

#define REPORT_UNEXPECTED_EOF(lf_)

#endif // CSS_REPORT_PARSE_ERRORS

void
nsCSSScanner::Close()
{
  mInputStream = nsnull;
  mReadPointer = nsnull;

  // Clean things up so we don't hold on to memory if our parser gets recycled.
#ifdef CSS_REPORT_PARSE_ERRORS
  mFileName.Truncate();
  mURI = nsnull;
  mError.Truncate();
#endif
  if (mPushback != mLocalPushback) {
    delete [] mPushback;
    mPushback = mLocalPushback;
    mPushbackSize = NS_ARRAY_LENGTH(mLocalPushback);
  }
}

#ifdef CSS_REPORT_PARSE_ERRORS
#define TAB_STOP_WIDTH 8
#endif

PRBool
nsCSSScanner::EnsureData()
{
  if (mOffset < mCount)
    return PR_TRUE;

  if (!mInputStream)
    return PR_FALSE;

  mOffset = 0;
  nsresult rv = mInputStream->Read(mBuffer, CSS_BUFFER_SIZE, &mCount);

  if (NS_FAILED(rv)) {
    mCount = 0;
    SetLowLevelError(rv);
    return PR_FALSE;
  }

  return mCount > 0;
}

// Returns -1 on error or eof
PRInt32
nsCSSScanner::Read()
{
  PRInt32 rv;
  if (0 < mPushbackCount) {
    rv = PRInt32(mPushback[--mPushbackCount]);
  } else {
    if (mOffset == mCount && !EnsureData()) {
      return -1;
    }
    rv = PRInt32(mReadPointer[mOffset++]);
    // There are four types of newlines in CSS: "\r", "\n", "\r\n", and "\f".
    // To simplify dealing with newlines, they are all normalized to "\n" here
    if (rv == '\r') {
      if (EnsureData() && mReadPointer[mOffset] == '\n') {
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
#ifdef CSS_REPORT_PARSE_ERRORS
      mColNumber = 0;
#endif
    } 
#ifdef CSS_REPORT_PARSE_ERRORS
    else if (rv == '\t') {
      mColNumber = ((mColNumber - 1 + TAB_STOP_WIDTH) / TAB_STOP_WIDTH)
                   * TAB_STOP_WIDTH;
    } else if (rv != '\n') {
      mColNumber++;
    }
#endif
  }
//printf("Read => %x\n", rv);
  return rv;
}

PRInt32
nsCSSScanner::Peek()
{
  if (0 == mPushbackCount) {
    PRInt32 ch = Read();
    if (ch < 0) {
      return -1;
    }
    mPushback[0] = PRUnichar(ch);
    mPushbackCount++;
  }
//printf("Peek => %x\n", mLookAhead);
  return PRInt32(mPushback[mPushbackCount - 1]);
}

void
nsCSSScanner::Pushback(PRUnichar aChar)
{
  if (mPushbackCount == mPushbackSize) { // grow buffer
    PRUnichar*  newPushback = new PRUnichar[mPushbackSize + 4];
    if (nsnull == newPushback) {
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

PRBool
nsCSSScanner::LookAhead(PRUnichar aChar)
{
  PRInt32 ch = Read();
  if (ch < 0) {
    return PR_FALSE;
  }
  if (ch == aChar) {
    return PR_TRUE;
  }
  Pushback(ch);
  return PR_FALSE;
}

void
nsCSSScanner::EatWhiteSpace()
{
  for (;;) {
    PRInt32 ch = Read();
    if (ch < 0) {
      break;
    }
    if ((ch != ' ') && (ch != '\n') && (ch != '\t')) {
      Pushback(ch);
      break;
    }
  }
}

PRBool
nsCSSScanner::Next(nsCSSToken& aToken)
{
  for (;;) { // Infinite loop so we can restart after comments.
    PRInt32 ch = Read();
    if (ch < 0) {
      return PR_FALSE;
    }

    // UNICODE-RANGE
    if ((ch == 'u' || ch == 'U') && Peek() == '+')
      return ParseURange(ch, aToken);

    // IDENT
    if (StartsIdent(ch, Peek()))
      return ParseIdent(ch, aToken);

    // AT_KEYWORD
    if (ch == '@') {
      PRInt32 nextChar = Read();
      if (nextChar >= 0) {
        PRInt32 followingChar = Peek();
        Pushback(nextChar);
        if (StartsIdent(nextChar, followingChar))
          return ParseAtKeyword(ch, aToken);
      }
    }

    // NUMBER or DIM
    if ((ch == '.') || (ch == '+') || (ch == '-')) {
      PRInt32 nextChar = Peek();
      if (IsDigit(nextChar)) {
        return ParseNumber(ch, aToken);
      }
      else if (('.' == nextChar) && ('.' != ch)) {
        nextChar = Read();
        PRInt32 followingChar = Peek();
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
      return PR_TRUE;
    }
    if (ch == '/') {
      PRInt32 nextChar = Peek();
      if (nextChar == '*') {
        (void) Read();
#if 0
        // If we change our storage data structures such that comments are
        // stored (for Editor), we should reenable this code, condition it
        // on being in editor mode, and apply glazou's patch from bug
        // 60290.
        aToken.mIdent.SetCapacity(2);
        aToken.mIdent.Assign(PRUnichar(ch));
        aToken.mIdent.Append(PRUnichar(nextChar));
        return ParseCComment(aToken);
#endif
        if (!SkipCComment()) {
          return PR_FALSE;
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
            return PR_TRUE;
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
          return PR_TRUE;
        }
        Pushback('-');
      }
    }

    // INCLUDES ("~=") and DASHMATCH ("|=")
    if (( ch == '|' ) || ( ch == '~' ) || ( ch == '^' ) ||
        ( ch == '$' ) || ( ch == '*' )) {
      PRInt32 nextChar = Read();
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
        return PR_TRUE;
      } else if (nextChar >= 0) {
        Pushback(nextChar);
      }
    }
    aToken.mType = eCSSToken_Symbol;
    aToken.mSymbol = ch;
    return PR_TRUE;
  }
}

PRBool
nsCSSScanner::NextURL(nsCSSToken& aToken)
{
  PRInt32 ch = Read();
  if (ch < 0) {
    return PR_FALSE;
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
    return PR_TRUE;
  }

  // Process a url lexical token. A CSS1 url token can contain
  // characters beyond identifier characters (e.g. '/', ':', etc.)
  // Because of this the normal rules for tokenizing the input don't
  // apply very well. To simplify the parser and relax some of the
  // requirements on the scanner we parse url's here. If we find a
  // malformed URL then we emit a token of type "InvalidURL" so that
  // the CSS1 parser can ignore the invalid input.  The parser must
  // treat an InvalidURL token like a Function token, and process
  // tokens until a matching parenthesis.

  aToken.mType = eCSSToken_InvalidURL;
  nsString& ident = aToken.mIdent;
  ident.SetLength(0);

  Pushback(ch);

  // start of a non-quoted url (which may be empty)
  PRBool ok = PR_TRUE;
  for (;;) {
    ch = Read();
    if (ch < 0) break;
    if (ch == CSS_ESCAPE) {
      ParseAndAppendEscape(ident);
    } else if ((ch == '"') || (ch == '\'') || (ch == '(')) {
      // This is an invalid URL spec
      ok = PR_FALSE;
      Pushback(ch); // push it back so the parser can match tokens and
                    // then closing parenthesis
      break;
    } else if (IsWhitespace(ch)) {
      // Whitespace is allowed at the end of the URL
        EatWhiteSpace();
        if (LookAhead(')')) {
        Pushback(')');  // leave the closing symbol
        // done!
        break;
      }
      // Whitespace is followed by something other than a
      // ")". This is an invalid url spec.
      ok = PR_FALSE;
      break;
    } else if (ch == ')') {
      Pushback(ch);
      // All done
      break;
    } else {
      // A regular url character.
      ident.Append(PRUnichar(ch));
    }
  }

  // If the result of the above scanning is ok then change the token
  // type to a useful one.
  if (ok) {
    aToken.mType = eCSSToken_URL;
  }
  return PR_TRUE;
}


void
nsCSSScanner::ParseAndAppendEscape(nsString& aOutput)
{
  PRInt32 ch = Peek();
  if (ch < 0) {
    aOutput.Append(CSS_ESCAPE);
    return;
  }
  if (IsHexDigit(ch)) {
    PRInt32 rv = 0;
    int i;
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
    return;
  } 
  // "Any character except a hexidecimal digit can be escaped to
  // remove its special meaning by putting a backslash in front"
  // -- CSS1 spec section 7.1
  ch = Read();  // Consume the escaped character
  if ((ch > 0) && (ch != '\n')) {
    aOutput.Append(ch);
  }
}

/**
 * Gather up the characters in an identifier. The identfier was
 * started by "aChar" which will be appended to aIdent. The result
 * will be aIdent with all of the identifier characters appended
 * until the first non-identifier character is seen. The termination
 * character is unread for the future re-reading.
 */
PRBool
nsCSSScanner::GatherIdent(PRInt32 aChar, nsString& aIdent)
{
  if (aChar == CSS_ESCAPE) {
    ParseAndAppendEscape(aIdent);
  }
  else if (0 < aChar) {
    aIdent.Append(aChar);
  }
  for (;;) {
    // If nothing in pushback, first try to get as much as possible in one go
    if (!mPushbackCount && EnsureData()) {
      // See how much we can consume and append in one go
      PRUint32 n = mOffset;
      // Count number of Ident characters that can be processed
      while (n < mCount && IsIdent(mReadPointer[n])) {
        ++n;
      }
      // Add to the token what we have so far
      if (n > mOffset) {
#ifdef CSS_REPORT_PARSE_ERRORS
        mColNumber += n - mOffset;
#endif
        aIdent.Append(&mReadPointer[mOffset], n - mOffset);
        mOffset = n;
      }
    }

    aChar = Read();
    if (aChar < 0) break;
    if (aChar == CSS_ESCAPE) {
      ParseAndAppendEscape(aIdent);
    } else if (IsIdent(aChar)) {
      aIdent.Append(PRUnichar(aChar));
    } else {
      Pushback(aChar);
      break;
    }
  }
  return PR_TRUE;
}

PRBool
nsCSSScanner::ParseRef(PRInt32 aChar, nsCSSToken& aToken)
{
  aToken.mIdent.SetLength(0);
  aToken.mType = eCSSToken_Ref;
  PRInt32 ch = Read();
  if (ch < 0) {
    return PR_FALSE;
  }
  if (IsIdent(ch) || ch == CSS_ESCAPE) {
    // First char after the '#' is a valid ident char (or an escape),
    // so it makes sense to keep going
    if (StartsIdent(ch, Peek())) {
      aToken.mType = eCSSToken_ID;
    }
    return GatherIdent(ch, aToken.mIdent);
  }

  // No ident chars after the '#'.  Just unread |ch| and get out of here.
  Pushback(ch);
  return PR_TRUE;
}

PRBool
nsCSSScanner::ParseIdent(PRInt32 aChar, nsCSSToken& aToken)
{
  nsString& ident = aToken.mIdent;
  ident.SetLength(0);
  if (!GatherIdent(aChar, ident)) {
    return PR_FALSE;
  }

  nsCSSTokenType tokenType = eCSSToken_Ident;
  // look for functions (ie: "ident(")
  if (Peek() == PRUnichar('(')) {
    Read();
    tokenType = eCSSToken_Function;
  }

  aToken.mType = tokenType;
  return PR_TRUE;
}

PRBool
nsCSSScanner::ParseAtKeyword(PRInt32 aChar, nsCSSToken& aToken)
{
  aToken.mIdent.SetLength(0);
  aToken.mType = eCSSToken_AtKeyword;
  return GatherIdent(0, aToken.mIdent);
}

PRBool
nsCSSScanner::ParseNumber(PRInt32 c, nsCSSToken& aToken)
{
  NS_PRECONDITION(c == '.' || c == '+' || c == '-' || IsDigit(c),
                  "Why did we get called?");
  aToken.mHasSign = (c == '+' || c == '-');

  // Our sign.
  PRInt32 sign = c == '-' ? -1 : 1;
  // Absolute value of the integer part of the mantissa.  This is a double so
  // we don't run into overflow issues for consumers that only care about our
  // floating-point value while still being able to express the full PRInt32
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
  PRInt32 exponent = 0;
  // Sign of the exponent.
  PRInt32 expSign = 1;

  if (aToken.mHasSign) {
    NS_ASSERTION(c != '.', "How did that happen?");
    c = Read();
  }

  PRBool gotDot = (c == '.');

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

  PRBool gotE = PR_FALSE;
#ifdef MOZ_SVG
  if (IsSVGMode() && (c == 'e' || c == 'E')) {
    PRInt32 nextChar = Peek();
    PRInt32 expSignChar = 0;
    if (nextChar == '-' || nextChar == '+') {
      expSignChar = Read();
      nextChar = Peek();
    }
    if (IsDigit(nextChar)) {
      gotE = PR_TRUE;
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
#endif

  nsCSSTokenType type = eCSSToken_Number;

  // Set mIntegerValid for all cases (except %, below) because we need
  // it for the "2n" in :nth-child(2n).
  aToken.mIntegerValid = PR_FALSE;

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
      aToken.mInteger = PRInt32(NS_MIN(intPart, double(PR_INT32_MAX)));
    } else {
      aToken.mInteger = PRInt32(NS_MAX(-intPart, double(PR_INT32_MIN)));
    }
    aToken.mIntegerValid = PR_TRUE;
  }

  nsString& ident = aToken.mIdent;
  ident.Truncate();

  // Look at character that terminated the number
  if (c >= 0) {
    if (StartsIdent(c, Peek())) {
      if (!GatherIdent(c, ident)) {
        return PR_FALSE;
      }
      type = eCSSToken_Dimension;
    } else if ('%' == c) {
      type = eCSSToken_Percentage;
      value = value / 100.0f;
      aToken.mIntegerValid = PR_FALSE;
    } else {
      // Put back character that stopped numeric scan
      Pushback(c);
    }
  }
  aToken.mNumber = value;
  aToken.mType = type;
  return PR_TRUE;
}

PRBool
nsCSSScanner::SkipCComment()
{
  for (;;) {
    PRInt32 ch = Read();
    if (ch < 0) break;
    if (ch == '*') {
      if (LookAhead('/')) {
        return PR_TRUE;
      }
    }
  }

  REPORT_UNEXPECTED_EOF(PECommentEOF);
  return PR_FALSE;
}

PRBool
nsCSSScanner::ParseString(PRInt32 aStop, nsCSSToken& aToken)
{
  aToken.mIdent.SetLength(0);
  aToken.mType = eCSSToken_String;
  aToken.mSymbol = PRUnichar(aStop); // remember how it's quoted
  for (;;) {
    // If nothing in pushback, first try to get as much as possible in one go
    if (!mPushbackCount && EnsureData()) {
      // See how much we can consume and append in one go
      PRUint32 n = mOffset;
      // Count number of characters that can be processed
      for (;n < mCount; ++n) {
        PRUnichar nextChar = mReadPointer[n];
        if ((nextChar == aStop) || (nextChar == CSS_ESCAPE) ||
            (nextChar == '\n') || (nextChar == '\r') || (nextChar == '\f')) {
          break;
        }
#ifdef CSS_REPORT_PARSE_ERRORS
        if (nextChar == '\t') {
          mColNumber = ((mColNumber - 1 + TAB_STOP_WIDTH) / TAB_STOP_WIDTH)
                       * TAB_STOP_WIDTH;
        } else {
          ++mColNumber;
        }
#endif
      }
      // Add to the token what we have so far
      if (n > mOffset) {
        aToken.mIdent.Append(&mReadPointer[mOffset], n - mOffset);
        mOffset = n;
      }
    }
    PRInt32 ch = Read();
    if (ch < 0 || ch == aStop) {
      break;
    }
    if (ch == '\n') {
      aToken.mType = eCSSToken_Error;
#ifdef CSS_REPORT_PARSE_ERRORS
      ReportUnexpectedToken(aToken, "SEUnterminatedString");
#endif
      break;
    }
    if (ch == CSS_ESCAPE) {
      ParseAndAppendEscape(aToken.mIdent);
    } else {
      aToken.mIdent.Append(ch);
    }
  }
  return PR_TRUE;
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

PRBool
nsCSSScanner::ParseURange(PRInt32 aChar, nsCSSToken& aResult)
{
  PRInt32 intro2 = Read();
  PRInt32 ch = Peek();

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

  PRBool valid = PR_TRUE;
  PRBool haveQues = PR_FALSE;
  PRUint32 low = 0;
  PRUint32 high = 0;
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
        valid = PR_FALSE; // all question marks should be at the end
      }
      low = low*16 + HexDigitValue(ch);
      high = high*16 + HexDigitValue(ch);
    } else {
      haveQues = PR_TRUE;
      low = low*16 + 0x0;
      high = high*16 + 0xF;
    }
  }

  if (ch == '-' && IsHexDigit(Peek())) {
    if (haveQues) {
      valid = PR_FALSE;
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
  return PR_TRUE;
}

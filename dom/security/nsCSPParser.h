/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSPParser_h___
#define nsCSPParser_h___

#include "nsCSPUtils.h"
#include "nsIURI.h"
#include "nsString.h"

/**
 * How does the parsing work?
 *
 * We generate tokens by splitting the policy-string by whitespace and semicolon.
 * Interally the tokens are represented as an array of string-arrays:
 *
 *  [
 *    [ name, src, src, src, ... ],
 *    [ name, src, src, src, ... ],
 *    [ name, src, src, src, ... ]
 *  ]
 *
 * for example:
 *  [
 *    [ img-src, http://www.example.com, http:www.test.com ],
 *    [ default-src, 'self'],
 *    [ script-src, 'unsafe-eval', 'unsafe-inline' ],
 *  ]
 *
 * The first element of each array has to be a valid directive-name, otherwise we can
 * ignore the remaining elements of the array. Also, if the
 * directive already exists in the current policy, we can ignore
 * the remaining elements of that array. (http://www.w3.org/TR/CSP/#parsing)
 */

typedef nsTArray< nsTArray<nsString> > cspTokens;

class nsCSPTokenizer {

  public:
    static void tokenizeCSPPolicy(const nsAString &aPolicyString, cspTokens& outTokens);

  private:
    nsCSPTokenizer(const char16_t* aStart, const char16_t* aEnd);
    ~nsCSPTokenizer();

    inline bool atEnd()
    {
      return mCurChar >= mEndChar;
    }

    inline void skipWhiteSpace()
    {
      while (mCurChar < mEndChar && *mCurChar == ' ') {
        mCurToken.Append(*mCurChar++);
      }
      mCurToken.Truncate();
    }

    inline void skipWhiteSpaceAndSemicolon()
    {
      while (mCurChar < mEndChar && (*mCurChar == ' ' || *mCurChar == ';')) {
        mCurToken.Append(*mCurChar++);
      }
      mCurToken.Truncate();
    }

    inline bool accept(char16_t aChar)
    {
      NS_ASSERTION(mCurChar < mEndChar, "Trying to dereference mEndChar");
      if (*mCurChar == aChar) {
        mCurToken.Append(*mCurChar++);
        return true;
      }
      return false;
    }

    void generateNextToken();
    void generateTokens(cspTokens& outTokens);

    const char16_t* mCurChar;
    const char16_t* mEndChar;
    nsString        mCurToken;
};


class nsCSPParser {

  public:
    /**
     * The CSP parser only has one publicly accessible function, which is parseContentSecurityPolicy.
     * Internally the input string is separated into string tokens and policy() is called, which starts
     * parsing the policy. The parser calls one function after the other according the the source-list
     * from http://www.w3.org/TR/CSP11/#source-list. E.g., the parser can only call port() after the parser
     * has already processed any possible host in host(), similar to a finite state machine.
     */
    static nsCSPPolicy* parseContentSecurityPolicy(const nsAString &aPolicyString,
                                                   nsIURI *aSelfURI,
                                                   bool aReportOnly,
                                                   nsCSPContext* aCSPContext);

  private:
    nsCSPParser(cspTokens& aTokens,
                nsIURI* aSelfURI,
                nsCSPContext* aCSPContext);
    ~nsCSPParser();


    // Parsing the CSP using the source-list from http://www.w3.org/TR/CSP11/#source-list
    nsCSPPolicy*    policy();
    void            directive();
    nsCSPDirective* directiveName();
    void            directiveValue(nsTArray<nsCSPBaseSrc*>& outSrcs);
    void            referrerDirectiveValue();
    void            sourceList(nsTArray<nsCSPBaseSrc*>& outSrcs);
    nsCSPBaseSrc*   sourceExpression();
    nsCSPSchemeSrc* schemeSource();
    nsCSPHostSrc*   hostSource();
    nsCSPBaseSrc*   keywordSource();
    nsCSPNonceSrc*  nonceSource();
    nsCSPHashSrc*   hashSource();
    nsCSPHostSrc*   appHost(); // helper function to support app specific hosts
    nsCSPHostSrc*   host();
    bool            hostChar();
    bool            schemeChar();
    bool            port();
    bool            path(nsCSPHostSrc* aCspHost);

    bool subHost();                                       // helper function to parse subDomains
    bool atValidUnreservedChar();                         // helper function to parse unreserved
    bool atValidSubDelimChar();                           // helper function to parse sub-delims
    bool atValidPctEncodedChar();                         // helper function to parse pct-encoded
    bool subPath(nsCSPHostSrc* aCspHost);                 // helper function to parse paths
    void reportURIList(nsTArray<nsCSPBaseSrc*>& outSrcs); // helper function to parse report-uris
    void percentDecodeStr(const nsAString& aEncStr,       // helper function to percent-decode
                          nsAString& outDecStr);

    inline bool atEnd()
    {
      return mCurChar >= mEndChar;
    }

    inline bool accept(char16_t aSymbol)
    {
      if (atEnd()) { return false; }
      return (*mCurChar == aSymbol) && advance();
    }

    inline bool accept(bool (*aClassifier) (char16_t))
    {
      if (atEnd()) { return false; }
      return (aClassifier(*mCurChar)) && advance();
    }

    inline bool peek(char16_t aSymbol)
    {
      if (atEnd()) { return false; }
      return *mCurChar == aSymbol;
    }

    inline bool peek(bool (*aClassifier) (char16_t))
    {
      if (atEnd()) { return false; }
      return aClassifier(*mCurChar);
    }

    inline bool advance()
    {
      if (atEnd()) { return false; }
      mCurValue.Append(*mCurChar++);
      return true;
    }

    inline void resetCurValue()
    {
      mCurValue.Truncate();
    }

    bool atEndOfPath();
    bool atValidPathChar();

    void resetCurChar(const nsAString& aToken);

    void logWarningErrorToConsole(uint32_t aSeverityFlag,
                                  const char* aProperty,
                                  const char16_t* aParams[],
                                  uint32_t aParamsLength);

/**
 * When parsing the policy, the parser internally uses the following helper
 * variables/members which are used/reset during parsing. The following
 * example explains how they are used.
 * The tokenizer separats all input into arrays of arrays of strings, which
 * are stored in mTokens, for example:
 *   mTokens = [ [ script-src, http://www.example.com, 'self' ], ... ]
 *
 * When parsing starts, mCurdir always holds the currently processed array of strings.
 * In our example:
 *   mCurDir = [ script-src, http://www.example.com, 'self' ]
 *
 * During parsing, we process/consume one string at a time of that array.
 * We set mCurToken to the string we are currently processing; in the first case
 * that would be:
 *   mCurToken = script-src
 * which allows to do simple string comparisons to see if mCurToken is a valid directive.
 *
 * Continuing parsing, the parser consumes the next string of that array, resetting:
 *   mCurToken = "http://www.example.com"
 *                ^                     ^
 *                mCurChar              mEndChar (points *after* the 'm')
 *   mCurValue = ""
 *
 * After calling advance() the first time, helpers would hold the following values:
 *   mCurToken = "http://www.example.com"
 *                 ^                    ^
 *                 mCurChar             mEndChar (points *after* the 'm')
 *  mCurValue = "h"
 *
 * We continue parsing till all strings of one directive are consumed, then we reset
 * mCurDir to hold the next array of strings and start the process all over.
 */

    const char16_t*    mCurChar;
    const char16_t*    mEndChar;
    nsString           mCurValue;
    nsString           mCurToken;
    nsTArray<nsString> mCurDir;

    // cache variables to ignore unsafe-inline if hash or nonce is specified
    bool               mHasHashOrNonce; // false, if no hash or nonce is defined
    nsCSPKeywordSrc*   mUnsafeInlineKeywordSrc; // null, otherwise invlidate()

    // cache variables for child-src and frame-src directive handling.
    // frame-src is deprecated in favor of child-src, however if we
    // see a frame-src directive, it takes precedence for frames and iframes.
    // At the end of parsing, if we have a child-src directive, we need to
    // decide whether it will handle frames, or if there is a frame-src we
    // should honor instead.
    nsCSPChildSrcDirective* mChildSrc;
    nsCSPDirective*         mFrameSrc;

    cspTokens          mTokens;
    nsIURI*            mSelfURI;
    nsCSPPolicy*       mPolicy;
    nsCSPContext*      mCSPContext; // used for console logging
};

#endif /* nsCSPParser_h___ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PolicyTokenizer_h___
#define PolicyTokenizer_h___

#include "nsContentUtils.h"
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
 */

typedef nsTArray< nsTArray<nsString> > policyTokens;

class PolicyTokenizer {

  public:
    static void tokenizePolicy(const nsAString &aPolicyString, policyTokens& outTokens);

  private:
    PolicyTokenizer(const char16_t* aStart, const char16_t* aEnd);
    ~PolicyTokenizer();

    inline bool atEnd()
    {
      return mCurChar >= mEndChar;
    }

    inline void skipWhiteSpace()
    {
      while (mCurChar < mEndChar &&
             nsContentUtils::IsHTMLWhitespace(*mCurChar)) {
        mCurToken.Append(*mCurChar++);
      }
      mCurToken.Truncate();
    }

    inline void skipWhiteSpaceAndSemicolon()
    {
      while (mCurChar < mEndChar && (*mCurChar == ';' ||
             nsContentUtils::IsHTMLWhitespace(*mCurChar))){
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
    void generateTokens(policyTokens& outTokens);

    const char16_t* mCurChar;
    const char16_t* mEndChar;
    nsString        mCurToken;
};

#endif /* PolicyTokenizer_h___ */

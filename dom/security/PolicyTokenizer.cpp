/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PolicyTokenizer.h"

#include "mozilla/Logging.h"

static mozilla::LogModule* GetPolicyTokenizerLog() {
  static mozilla::LazyLogModule gPolicyTokenizerPRLog("PolicyTokenizer");
  return gPolicyTokenizerPRLog;
}

#define POLICYTOKENIZERLOG(args) \
  MOZ_LOG(GetPolicyTokenizerLog(), mozilla::LogLevel::Debug, args)

static const char16_t SEMICOL = ';';

PolicyTokenizer::PolicyTokenizer(const char16_t* aStart, const char16_t* aEnd)
    : mCurChar(aStart), mEndChar(aEnd) {
  POLICYTOKENIZERLOG(("PolicyTokenizer::PolicyTokenizer"));
}

PolicyTokenizer::~PolicyTokenizer() {
  POLICYTOKENIZERLOG(("PolicyTokenizer::~PolicyTokenizer"));
}

void PolicyTokenizer::generateNextToken() {
  skipWhiteSpaceAndSemicolon();
  MOZ_ASSERT(mCurToken.Length() == 0);
  const char16_t* const start = mCurChar;
  while (!atEnd() && !nsContentUtils::IsHTMLWhitespace(*mCurChar) &&
         *mCurChar != SEMICOL) {
    mCurChar++;
  }
  if (start != mCurChar) {
    mCurToken.Append(start, mCurChar - start);
  }
  POLICYTOKENIZERLOG(("PolicyTokenizer::generateNextToken: %s",
                      NS_ConvertUTF16toUTF8(mCurToken).get()));
}

void PolicyTokenizer::generateTokens(policyTokens& outTokens) {
  POLICYTOKENIZERLOG(("PolicyTokenizer::generateTokens"));

  // dirAndSrcs holds one set of [ name, src, src, src, ... ]
  nsTArray<nsString> dirAndSrcs;

  while (!atEnd()) {
    generateNextToken();
    dirAndSrcs.AppendElement(mCurToken);
    skipWhiteSpace();
    if (atEnd() || accept(SEMICOL)) {
      outTokens.AppendElement(std::move(dirAndSrcs));
      dirAndSrcs.ClearAndRetainStorage();
    }
  }
}

void PolicyTokenizer::tokenizePolicy(const nsAString& aPolicyString,
                                     policyTokens& outTokens) {
  POLICYTOKENIZERLOG(("PolicyTokenizer::tokenizePolicy"));

  PolicyTokenizer tokenizer(aPolicyString.BeginReading(),
                            aPolicyString.EndReading());

  tokenizer.generateTokens(outTokens);
}

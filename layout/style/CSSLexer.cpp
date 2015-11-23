/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSLexer.h"
#include "js/Value.h"
#include "mozilla/dom/CSSLexerBinding.h"
#include "mozilla/dom/ToJSValue.h"

namespace mozilla {
namespace dom {

// Ensure that constants are consistent.

#define CHECK(X, Y) \
  static_assert(static_cast<int>(X) == static_cast<int>(Y),       \
                "nsCSSToken and CSSTokenType should have identical values")

CHECK(eCSSToken_Whitespace, CSSTokenType::Whitespace);
CHECK(eCSSToken_Comment, CSSTokenType::Comment);
CHECK(eCSSToken_Ident, CSSTokenType::Ident);
CHECK(eCSSToken_Function, CSSTokenType::Function);
CHECK(eCSSToken_AtKeyword, CSSTokenType::At);
CHECK(eCSSToken_ID, CSSTokenType::Id);
CHECK(eCSSToken_Hash, CSSTokenType::Hash);
CHECK(eCSSToken_Number, CSSTokenType::Number);
CHECK(eCSSToken_Dimension, CSSTokenType::Dimension);
CHECK(eCSSToken_Percentage, CSSTokenType::Percentage);
CHECK(eCSSToken_String, CSSTokenType::String);
CHECK(eCSSToken_Bad_String, CSSTokenType::Bad_string);
CHECK(eCSSToken_URL, CSSTokenType::Url);
CHECK(eCSSToken_Bad_URL, CSSTokenType::Bad_url);
CHECK(eCSSToken_Symbol, CSSTokenType::Symbol);
CHECK(eCSSToken_Includes, CSSTokenType::Includes);
CHECK(eCSSToken_Dashmatch, CSSTokenType::Dashmatch);
CHECK(eCSSToken_Beginsmatch, CSSTokenType::Beginsmatch);
CHECK(eCSSToken_Endsmatch, CSSTokenType::Endsmatch);
CHECK(eCSSToken_Containsmatch, CSSTokenType::Containsmatch);
CHECK(eCSSToken_URange, CSSTokenType::Urange);
CHECK(eCSSToken_HTMLComment, CSSTokenType::Htmlcomment);

#undef CHECK

CSSLexer::CSSLexer(const nsAString& aText)
  : mInput(aText)
  , mScanner(mInput, 1)
{
}

CSSLexer::~CSSLexer()
{
}

bool
CSSLexer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                     JS::MutableHandle<JSObject*> aReflector)
{
  return CSSLexerBinding::Wrap(aCx, this, aGivenProto, aReflector);
}

uint32_t
CSSLexer::LineNumber()
{
  // The scanner uses 1-based line numbers, but our callers expect
  // 0-based.
  return mScanner.GetLineNumber() - 1;
}

uint32_t
CSSLexer::ColumnNumber()
{
  return mScanner.GetColumnNumber();
}

void
CSSLexer::PerformEOFFixup(const nsAString& aInputString, bool aPreserveBackslash,
                          nsAString& aResult)
{
  aResult.Append(aInputString);
  uint32_t eofChars = mScanner.GetEOFCharacters();

  if (aPreserveBackslash &&
      (eofChars & (nsCSSScanner::eEOFCharacters_DropBackslash |
                   nsCSSScanner::eEOFCharacters_ReplacementChar)) != 0) {
    eofChars &= ~(nsCSSScanner::eEOFCharacters_DropBackslash |
                  nsCSSScanner::eEOFCharacters_ReplacementChar);
    aResult.Append('\\');
  }

  if ((eofChars & nsCSSScanner::eEOFCharacters_DropBackslash) != 0 &&
      aResult.Length() > 0 && aResult.Last() == '\\') {
    aResult.Truncate(aResult.Length() - 1);
  }

  nsCSSScanner::AppendImpliedEOFCharacters(nsCSSScanner::EOFCharacters(eofChars),
                                           aResult);
}

void
CSSLexer::NextToken(Nullable<CSSToken>& aResult)
{
  nsCSSToken token;
  if (!mScanner.Next(token, eCSSScannerExclude_None)) {
    return;
  }

  CSSToken& resultToken(aResult.SetValue());

  resultToken.mTokenType = static_cast<CSSTokenType>(token.mType);
  resultToken.mStartOffset = mScanner.GetTokenOffset();
  resultToken.mEndOffset = mScanner.GetTokenEndOffset();

  switch (token.mType) {
    case eCSSToken_Whitespace:
      break;

    case eCSSToken_Ident:
    case eCSSToken_Function:
    case eCSSToken_AtKeyword:
    case eCSSToken_ID:
    case eCSSToken_Hash:
      resultToken.mText.Construct(token.mIdent);
      break;

    case eCSSToken_Dimension:
      resultToken.mText.Construct(token.mIdent);
      MOZ_FALLTHROUGH;
    case eCSSToken_Number:
    case eCSSToken_Percentage:
      resultToken.mNumber.Construct(token.mNumber);
      resultToken.mHasSign.Construct(token.mHasSign);
      resultToken.mIsInteger.Construct(token.mIntegerValid);
      break;

    case eCSSToken_String:
    case eCSSToken_Bad_String:
    case eCSSToken_URL:
    case eCSSToken_Bad_URL:
      resultToken.mText.Construct(token.mIdent);
      /* Don't bother emitting the delimiter, as it is readily extracted
         from the source string when needed.  */
      break;

    case eCSSToken_Symbol:
      resultToken.mText.Construct(nsString(&token.mSymbol, 1));
      break;

    case eCSSToken_Includes:
    case eCSSToken_Dashmatch:
    case eCSSToken_Beginsmatch:
    case eCSSToken_Endsmatch:
    case eCSSToken_Containsmatch:
    case eCSSToken_URange:
      break;

    case eCSSToken_Comment:
    case eCSSToken_HTMLComment:
      /* The comment text is easily extracted from the source string,
         and is rarely useful.  */
      break;
  }
}

} // namespace dom
} // namespace mozilla

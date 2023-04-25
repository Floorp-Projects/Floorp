/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImportScanner.h"

#include "mozilla/ServoBindings.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsContentUtils.h"

namespace mozilla {

static inline bool IsWhitespace(char16_t aChar) {
  return nsContentUtils::IsHTMLWhitespace(aChar);
}

static inline bool OptionalSupportsMatches(const nsAString& aAfterRuleValue) {
  // Ensure pref for @import supports() is enabled before wanting to check.
  if (!StaticPrefs::layout_css_import_supports_enabled()) {
    return true;
  }

  // Empty, don't bother checking.
  if (aAfterRuleValue.IsEmpty()) {
    return true;
  }

  NS_ConvertUTF16toUTF8 value(aAfterRuleValue);
  return Servo_CSSSupportsForImport(&value);
}

void ImportScanner::ResetState() {
  mInImportRule = false;
  // We try to avoid freeing the buffers here.
  mRuleName.Truncate(0);
  mRuleValue.Truncate(0);
  mAfterRuleValue.Truncate(0);
}

void ImportScanner::Start() {
  Stop();
  mState = State::Idle;
}

void ImportScanner::EmitUrl() {
  MOZ_ASSERT(mState == State::AfterRuleValue);
  if (mInImportRule) {
    // Trim trailing whitespace from an unquoted URL.
    if (mUrlValueDelimiterClosingChar == ')') {
      // FIXME: Add a convenience function in nsContentUtils or something?
      mRuleValue.Trim(" \t\n\r\f", false);
    }

    // If a supports(...) condition is given as part of import conditions,
    // only emit the URL if it matches, as there is no use preloading
    // imports for features we do not support, as this cannot change
    // mid-page.
    if (OptionalSupportsMatches(mAfterRuleValue)) {
      mUrlsFound.AppendElement(std::move(mRuleValue));
    }
  }
  ResetState();
  MOZ_ASSERT(mRuleValue.IsEmpty());
}

nsTArray<nsString> ImportScanner::Stop() {
  if (mState == State::AfterRuleValue) {
    EmitUrl();
  }
  mState = State::OutsideOfStyleElement;
  ResetState();
  return std::move(mUrlsFound);
}

nsTArray<nsString> ImportScanner::Scan(Span<const char16_t> aFragment) {
  MOZ_ASSERT(ShouldScan());

  for (char16_t c : aFragment) {
    mState = Scan(c);
    if (mState == State::Done) {
      break;
    }
  }

  return std::move(mUrlsFound);
}

auto ImportScanner::Scan(char16_t aChar) -> State {
  switch (mState) {
    case State::OutsideOfStyleElement:
    case State::Done:
      MOZ_ASSERT_UNREACHABLE("How?");
      return mState;
    case State::Idle: {
      // TODO(emilio): Maybe worth caring about html-style comments like:
      // <style>
      // <!--
      //   @import url(stuff);
      // -->
      // </style>
      if (IsWhitespace(aChar)) {
        return mState;
      }
      if (aChar == '/') {
        return State::MaybeAtCommentStart;
      }
      if (aChar == '@') {
        MOZ_ASSERT(mRuleName.IsEmpty());
        return State::AtRuleName;
      }
      return State::Done;
    }
    case State::MaybeAtCommentStart: {
      return aChar == '*' ? State::AtComment : State::Done;
    }
    case State::AtComment: {
      return aChar == '*' ? State::MaybeAtCommentEnd : mState;
    }
    case State::MaybeAtCommentEnd: {
      return aChar == '/' ? State::Idle : State::AtComment;
    }
    case State::AtRuleName: {
      if (IsAsciiAlpha(aChar)) {
        if (mRuleName.Length() > kMaxRuleNameLength - 1) {
          return State::Done;
        }
        mRuleName.Append(aChar);
        return mState;
      }
      if (IsWhitespace(aChar)) {
        mInImportRule = mRuleName.LowerCaseEqualsLiteral("import");
        if (mInImportRule) {
          return State::AtRuleValue;
        }
        // Ignorable rules, we skip until the next semi-colon for these.
        if (mRuleName.LowerCaseEqualsLiteral("charset") ||
            mRuleName.LowerCaseEqualsLiteral("layer")) {
          MOZ_ASSERT(mRuleValue.IsEmpty());
          return State::AfterRuleValue;
        }
      }
      return State::Done;
    }
    case State::AtRuleValue: {
      MOZ_ASSERT(mInImportRule, "Should only get to this state for @import");
      if (mRuleValue.IsEmpty()) {
        if (IsWhitespace(aChar)) {
          return mState;
        }
        if (aChar == '"' || aChar == '\'') {
          mUrlValueDelimiterClosingChar = aChar;
          return State::AtRuleValueDelimited;
        }
        if (aChar == 'u' || aChar == 'U') {
          mRuleValue.Append('u');
          return mState;
        }
        return State::Done;
      }
      if (mRuleValue.Length() == 1) {
        MOZ_ASSERT(mRuleValue.EqualsLiteral("u"));
        if (aChar == 'r' || aChar == 'R') {
          mRuleValue.Append('r');
          return mState;
        }
        return State::Done;
      }
      if (mRuleValue.Length() == 2) {
        MOZ_ASSERT(mRuleValue.EqualsLiteral("ur"));
        if (aChar == 'l' || aChar == 'L') {
          mRuleValue.Append('l');
        }
        return mState;
      }
      if (mRuleValue.Length() == 3) {
        MOZ_ASSERT(mRuleValue.EqualsLiteral("url"));
        if (aChar == '(') {
          mUrlValueDelimiterClosingChar = ')';
          mRuleValue.Truncate(0);
          return State::AtRuleValueDelimited;
        }
        return State::Done;
      }
      MOZ_ASSERT_UNREACHABLE(
          "How? We should find a paren or a string delimiter");
      return State::Done;
    }
    case State::AtRuleValueDelimited: {
      MOZ_ASSERT(mInImportRule, "Should only get to this state for @import");
      if (aChar == mUrlValueDelimiterClosingChar) {
        return State::AfterRuleValue;
      }
      if (mUrlValueDelimiterClosingChar == ')' && mRuleValue.IsEmpty()) {
        if (IsWhitespace(aChar)) {
          return mState;
        }
        if (aChar == '"' || aChar == '\'') {
          // Handle url("") and url('').
          mUrlValueDelimiterClosingChar = aChar;
          return mState;
        }
      }
      if (!mRuleValue.Append(aChar, mozilla::fallible)) {
        mRuleValue.Truncate(0);
        return State::Done;
      }
      return mState;
    }
    case State::AfterRuleValue: {
      if (aChar == ';') {
        EmitUrl();
        return State::Idle;
      }
      // If there's a selector here and the import was unterminated, just give
      // up.
      if (aChar == '{') {
        return State::Done;
      }

      if (!mAfterRuleValue.Append(aChar, mozilla::fallible)) {
        mAfterRuleValue.Truncate(0);
        return State::Done;
      }

      return mState;  // There can be all sorts of stuff here like media
                      // queries or what not.
    }
  }
  MOZ_ASSERT_UNREACHABLE("Forgot to handle a state?");
  return State::Done;
}

}  // namespace mozilla

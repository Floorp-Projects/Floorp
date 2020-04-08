/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ImportScanner_h
#define mozilla_ImportScanner_h

/* A simple best-effort scanner for @import rules for the HTML parser */

#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

struct ImportScanner final {
  ImportScanner() = default;

  // Called when a <style> element starts.
  //
  // Note that this function cannot make assumptions about the internal state,
  // as you can nest <svg:style> elements.
  void Start();

  // Called when a <style> element ends. Returns the list of URLs scanned.
  nsTArray<nsString> Stop();

  // Whether Scan() should be called.
  bool ShouldScan() const {
    return mState != State::OutsideOfStyleElement && mState != State::Done;
  }

  // Scan() should be called when text content is parsed, and returns an array
  // of found URLs, if any.
  //
  // Asserts ShouldScan() returns true.
  nsTArray<nsString> Scan(Span<const char16_t> aFragment);

 private:
  enum class State {
    // Initial state, doesn't scan anything until Start() is called.
    OutsideOfStyleElement,
    // In an idle state during the stylesheet scanning, either at the
    // beginning or after parsing a rule.
    Idle,
    // We've seen a '/' character, but not the '*' yet, so we don't know if
    // it's a comment.
    MaybeAtCommentStart,
    // We're inside a comment.
    AtComment,
    // We've seen a '*' while we're in a comment, but we don't now yet whether
    // '/' comes afterwards (thus ending the comment).
    MaybeAtCommentEnd,
    // We're parsing the '@' rule name.
    AtRuleName,
    // We're parsing the '@' rule value.
    AtRuleValue,
    // We're parsing the '@' rule value and we've seen the delimiter (quote or
    // url() function) that encloses the url.
    AtRuleValueDelimited,
    // We've seen the url, but haven't seen the ';' finishing the rule yet.
    AfterRuleValue,
    // We've seen anything that is not an @import or a @charset rule, and thus
    // further @import / @charset should not be parsed.
    Done,
  };

  void EmitUrl();
  [[nodiscard]] State Scan(char16_t aChar);

  static constexpr const uint32_t kMaxRuleNameLength = 7;  // (charset, import)

  State mState = State::OutsideOfStyleElement;
  nsAutoStringN<kMaxRuleNameLength> mRuleName;
  nsAutoStringN<128> mRuleValue;
  nsTArray<nsString> mUrlsFound;

  // This is conceptually part of the AtRuleValue* / AfterRuleValue states,
  // and serves to differentiate between @import (where we actually care about
  // the value) and @charset (where we don't). It's just more convenient this
  // way than having separate states for them.
  bool mInImportRule = false;
  // If we're in the AtRuleValueDelimited state, what is the closing character
  // that will end the value. This is either a parenthesis (for unquoted
  // urls), or a quote, either single or double.
  char16_t mUrlValueDelimiterClosingChar = 0;
};

}  // namespace mozilla

#endif

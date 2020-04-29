/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowFeatures.h"

#include "nsINode.h"               // IsSpaceCharacter
#include "nsContentUtils.h"        // nsContentUtils
#include "nsDependentSubstring.h"  // Substring
#include "nsReadableUtils.h"       // ToLowerCase

using mozilla::dom::IsSpaceCharacter;
using mozilla::dom::WindowFeatures;

#ifdef DEBUG
/* static */
bool WindowFeatures::IsLowerCase(const char* text) {
  nsAutoCString before(text);
  nsAutoCString after;
  ToLowerCase(before, after);
  return before == after;
}
#endif

static bool IsFeatureSeparator(char aChar) {
  // https://html.spec.whatwg.org/multipage/window-object.html#feature-separator
  // A code point is a feature separator if it is ASCII whitespace, U+003D (=),
  // or U+002C (,).
  return IsSpaceCharacter(aChar) || aChar == '=' || aChar == ',';
}

template <class IterT, class CondT>
void AdvanceWhile(IterT& aPosition, const IterT& aEnd, CondT aCondition) {
  // https://infra.spec.whatwg.org/#collect-a-sequence-of-code-points
  //
  // Step 2. While `position` doesn’t point past the end of `input` and the
  // code point at `position` within `input` meets the condition condition:
  while (aCondition(*aPosition) && aPosition < aEnd) {
    // Step 2.1. Append that code point to the end of `result`.
    // (done by caller)

    // Step 2.2. Advance `position` by 1.
    ++aPosition;
  }
}

template <class IterT, class CondT>
nsTDependentSubstring<char> CollectSequence(IterT& aPosition, const IterT& aEnd,
                                            CondT aCondition) {
  // https://infra.spec.whatwg.org/#collect-a-sequence-of-code-points
  // To collect a sequence of code points meeting a condition `condition` from
  // a string `input`, given a position variable `position` tracking the
  // position of the calling algorithm within `input`:

  // Step 1. Let `result` be the empty string.
  auto start = aPosition;

  // Step 2.
  AdvanceWhile(aPosition, aEnd, aCondition);

  // Step 3. Return `result`.
  return Substring(start, aPosition);
}

static void NormalizeName(nsAutoCString& aName) {
  // https://html.spec.whatwg.org/multipage/window-object.html#normalizing-the-feature-name
  if (aName == "screenx") {
    aName = "left";
  } else if (aName == "screeny") {
    aName = "top";
  } else if (aName == "innerwidth") {
    aName = "width";
  } else if (aName == "innerheight") {
    aName = "height";
  }
}

/* static */
int32_t WindowFeatures::ParseIntegerWithFallback(const nsCString& aValue) {
  // https://html.spec.whatwg.org/multipage/window-object.html#concept-window-open-features-parse-boolean
  //
  // Step 3. Let `parsed` be the result of parsing value as an integer.
  nsContentUtils::ParseHTMLIntegerResultFlags parseResult;
  int32_t parsed = nsContentUtils::ParseHTMLInteger(aValue, &parseResult);

  // Step 4. If `parsed` is an error, then set it to 0.
  //
  // https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#rules-for-parsing-integers
  //
  // eParseHTMLInteger_NonStandard is not tested given:
  //   * Step 4 allows leading whitespaces
  //   * Step 6 allows a plus sign
  //   * Step 8 does not disallow leading zeros
  //   * Steps 6 and 9 allow `-0`
  //
  // eParseHTMLInteger_DidNotConsumeAllInput is not tested given:
  //   * Step 8 collects digits and ignores remaining part
  //
  if (parseResult & nsContentUtils::eParseHTMLInteger_Error) {
    parsed = 0;
  }

  return parsed;
}

/* static */
bool WindowFeatures::ParseBool(const nsCString& aValue) {
  // https://html.spec.whatwg.org/multipage/window-object.html#concept-window-open-features-parse-boolean
  // To parse a boolean feature given a string `value`:

  // Step 1. If `value` is the empty string, then return true.
  if (aValue.IsEmpty()) {
    return true;
  }

  // Step 2. If `value` is a case-sensitive match for "yes", then return true.
  if (aValue == "yes") {
    return true;
  }

  // Steps 3-4.
  int32_t parsed = ParseIntegerWithFallback(aValue);

  // Step 5. Return false if `parsed` is 0, and true otherwise.
  return parsed != 0;
}

bool WindowFeatures::Tokenize(const nsACString& aFeatures) {
  // https://html.spec.whatwg.org/multipage/window-object.html#concept-window-open-features-tokenize
  // To tokenize the `features` argument:

  // Step 1. Let `tokenizedFeatures` be a new ordered map.
  // (implicit)

  // Step 2. Let `position` point at the first code point of features.
  auto position = aFeatures.BeginReading();

  // Step 3. While `position` is not past the end of `features`:
  auto end = aFeatures.EndReading();
  while (position < end) {
    // Step 3.1. Let `name` be the empty string.
    // (implicit)

    // Step 3.2. Let `value` be the empty string.
    nsAutoCString value;

    // Step 3.3. Collect a sequence of code points that are feature separators
    // from `features` given `position`. This skips past leading separators
    // before the name.
    //
    // NOTE: Do not collect given this value is unused.
    AdvanceWhile(position, end, IsFeatureSeparator);

    // Step 3.4. Collect a sequence of code points that are not feature
    // separators from `features` given `position`. Set `name` to the collected
    // characters, converted to ASCII lowercase.
    nsAutoCString name(CollectSequence(
        position, end, [](char c) { return !IsFeatureSeparator(c); }));
    ToLowerCase(name);

    // Step 3.5. Set `name` to the result of normalizing the feature name
    // `name`.
    NormalizeName(name);

    // Step 3.6. While `position` is not past the end of `features` and the
    // code point at `position` in features is not U+003D (=):
    //
    // Step 3.6.1. If the code point at `position` in features is U+002C (,),
    // or if it is not a feature separator, then break.
    //
    // Step 3.6.2. Advance `position` by 1.
    //
    // NOTE: This skips to the first U+003D (=) but does not skip past a U+002C
    //       (,) or a non-separator.
    //
    // The above means skip all whitespaces.
    AdvanceWhile(position, end, [](char c) { return IsSpaceCharacter(c); });

    // Step 3.7. If the code point at `position` in `features` is a feature
    // separator:
    if (position < end && IsFeatureSeparator(*position)) {
      // Step 3.7.1. While `position` is not past the end of `features` and the
      // code point at `position` in `features` is a feature separator:
      //
      // Step 3.7.1.1. If the code point at `position` in `features` is
      // U+002C (,), then break.
      //
      // Step 3.7.1.2. Advance `position` by 1.
      //
      // NOTE: This skips to the first non-separator but does not skip past a
      // U+002C (,).
      AdvanceWhile(position, end,
                   [](char c) { return IsFeatureSeparator(c) && c != ','; });

      // Step 3.7.2. Collect a sequence of code points that are not feature
      // separators code points from `features` given `position`. Set `value` to
      // the collected code points, converted to ASCII lowercase.
      value = CollectSequence(position, end,
                              [](char c) { return !IsFeatureSeparator(c); });
      ToLowerCase(value);
    }

    // Step 3.8. If `name` is not the empty string, then set
    // `tokenizedFeatures[name]` to `value`.
    if (!name.IsEmpty()) {
      if (!tokenizedFeatures_.put(name, value)) {
        return false;
      }
    }
  }

  // Step 4. Return `tokenizedFeatures`.
  return true;
}

void WindowFeatures::Stringify(nsAutoCString& aOutput) {
  MOZ_ASSERT(aOutput.IsEmpty());

  for (auto r = tokenizedFeatures_.all(); !r.empty(); r.popFront()) {
    if (!aOutput.IsEmpty()) {
      aOutput.Append(',');
    }

    const nsCString& name = r.front().key();
    const nsCString& value = r.front().value();

    aOutput.Append(name);

    if (!value.IsEmpty()) {
      aOutput.Append('=');
      aOutput.Append(value);
    }
  }
}

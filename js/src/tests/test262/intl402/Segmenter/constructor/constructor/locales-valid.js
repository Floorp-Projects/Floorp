// |reftest| skip-if(!Intl.hasOwnProperty('Segmenter')) -- Intl.Segmenter is not enabled unconditionally
// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter
description: Checks various cases for the locales argument to the Segmenter constructor.
info: |
    Intl.Segmenter ([ locales [ , options ]])

    3. Let _requestedLocales_ be ? CanonicalizeLocaleList(_locales_).
includes: [testIntl.js]
features: [Intl.Segmenter]
---*/

const defaultLocale = new Intl.Segmenter().resolvedOptions().locale;

const tests = [
  [undefined, defaultLocale, "undefined"],
  ["EN", "en", "Single value"],
  [[], defaultLocale, "Empty array"],
  [["en-GB-oed"], "en-GB", "Grandfathered"],
  [["x-private"], defaultLocale, "Private", ["lookup"]],
  [["en", "EN"], "en", "Duplicate value (canonical first)"],
  [["EN", "en"], "en", "Duplicate value (canonical last)"],
  [{ 0: "DE", length: 0 }, defaultLocale, "Object with zero length"],
  [{ 0: "DE", length: 1 }, "de", "Object with length"],
];

for (const [locales, expected, name, matchers = ["best fit", "lookup"]] of tests) {
  for (const localeMatcher of matchers) {
    const segmenter = new Intl.Segmenter(locales, { localeMatcher });
    assert.sameValue(segmenter.resolvedOptions().locale, expected, name);
  }
}

reportCompare(0, 0);

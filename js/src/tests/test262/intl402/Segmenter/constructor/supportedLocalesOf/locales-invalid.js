// |reftest| skip-if(!Intl.Segmenter) -- Intl.Segmenter is not enabled unconditionally
// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter.supportedLocalesOf
description: Checks error cases for the locales argument to the supportedLocalesOf function.
info: |
    Intl.Segmenter.supportedLocalesOf ( locales [, options ])

    2. Let requestedLocales be CanonicalizeLocaleList(locales).
includes: [testIntl.js]
features: [Intl.Segmenter]
---*/

assert.sameValue(typeof Intl.Segmenter.supportedLocalesOf, "function",
                 "Should support Intl.Segmenter.supportedLocalesOf.");

for (const [locales, expectedError] of getInvalidLocaleArguments()) {
    assert.throws(expectedError, () => Intl.Segmenter.supportedLocalesOf(locales));
}

reportCompare(0, 0);

// |reftest| skip-if(!Intl.hasOwnProperty('Segmenter')) -- Intl.Segmenter is not enabled unconditionally
// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Segmenter.supportedLocalesOf
description: >
    Verifies there's no branding check for Intl.Segmenter.supportedLocalesOf().
info: |
    Intl.Segmenter.supportedLocalesOf ( locales [, options ])
features: [Intl.Segmenter]
---*/

const fn = Intl.Segmenter.supportedLocalesOf;
const thisValues = [
  undefined,
  null,
  true,
  "",
  Symbol(),
  1,
  {},
  Intl.Segmenter,
  Intl.Segmenter.prototype,
];

for (const thisValue of thisValues) {
  const result = fn.call(thisValue);
  assert.sameValue(Array.isArray(result), true);
}

reportCompare(0, 0);

// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-InitializeRelativeTimeFormat
description: Checks the propagation of exceptions from the options for the RelativeTimeFormat constructor.
features: [Intl.RelativeTimeFormat]
---*/

function CustomError() {}

const options = [
  "localeMatcher",
  "style",
  "numeric",
];

for (const option of options) {
  assert.throws(CustomError, () => {
    new Intl.RelativeTimeFormat("en", {
      get [option]() {
        throw new CustomError();
      }
    });
  }, `Exception from ${option} getter should be propagated`);
}

reportCompare(0, 0);

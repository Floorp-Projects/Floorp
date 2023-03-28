// |reftest| shell-option(--enable-array-from-async) skip-if(!Array.fromAsync||!xulRuntime.shell) async -- Array.fromAsync is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.fromasync
description: >
  Array.fromAsync iterates over a string
includes: [asyncHelpers.js, compareArray.js]
flags: [async]
features: [Array.fromAsync]
---*/

asyncTest(async function () {
  const result = await Array.fromAsync("test");
  assert.compareArray(result, ["t", "e", "s", "t"]);
});

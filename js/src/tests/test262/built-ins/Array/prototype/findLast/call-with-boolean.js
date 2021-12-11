// |reftest| skip -- array-find-from-last is not supported
// Copyright (C) 2021 Microsoft. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.findlast
description: Array.prototype.findLast applied to boolean primitive.
features: [array-find-from-last]
---*/

assert.sameValue(
  Array.prototype.findLast.call(true, () => {}),
  undefined,
  'Array.prototype.findLast.call(true, () => {}) must return undefined'
);
assert.sameValue(
  Array.prototype.findLast.call(false, () => {}),
  undefined,
  'Array.prototype.findLast.call(false, () => {}) must return undefined'
);

reportCompare(0, 0);

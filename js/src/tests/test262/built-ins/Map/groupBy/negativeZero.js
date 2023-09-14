// |reftest| shell-option(--enable-array-grouping) skip-if(!Object.groupBy||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2023 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-map.groupby
description: Map.groupBy normalizes 0 for Map key
info: |
  Map.groupBy ( items, callbackfn )

  ...

  GroupBy ( items, callbackfn, coercion )

  6. Repeat,
    h. Else,
      i. Assert: coercion is zero.
      ii. If key is -0𝔽, set key to +0𝔽.

  ...
includes: [compareArray.js]
features: [array-grouping, Map]
---*/


const arr = [-0, +0];

const map = Map.groupBy(arr, function (i) { return i; });

assert.sameValue(map.size, 1);
assert.compareArray(map.get(0), [-0, 0]);

reportCompare(0, 0);

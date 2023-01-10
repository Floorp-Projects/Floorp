// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap normalize 0 for Map key
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  6.d. If key is -0𝔽, set key to +0𝔽.

  ...
includes: [compareArray.js]
features: [array-grouping, Map]
---*/


const arr = [-0, +0];

const map = arr.groupToMap(function (i) { return i; });

assert.sameValue(map.size, 1);
assert.compareArray(map.get(0), [-0, 0]);

reportCompare(0, 0);

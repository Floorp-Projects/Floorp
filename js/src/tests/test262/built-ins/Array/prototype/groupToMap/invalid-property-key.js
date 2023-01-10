// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap does not error when return value cannot be converted to a property key.
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    c. Let propertyKey be ? ToPropertyKey(? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »)).

  ...
includes: [compareArray.js]
features: [array-grouping, Map, Symbol.iterator]
---*/

const key = {
  toString() {
    throw new Test262Error('not a property key');
  }
};

const map = [1].groupToMap(function() {
  return key;
});

assert.compareArray(Array.from(map.keys()), [key]);
assert.compareArray(map.get(key), [1]);

reportCompare(0, 0);

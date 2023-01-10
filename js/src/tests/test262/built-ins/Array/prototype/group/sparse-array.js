// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group adds undefined to the group for sparse arrays
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    a. Let Pk be ! ToString(𝔽(k)).
    b. Let kValue be ? Get(O, Pk).
    c. Let propertyKey be ? ToPropertyKey(? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »)).
    d. Perform AddValueToKeyedGroup(groups, propertyKey, kValue).

  ...
includes: [compareArray.js]
features: [array-grouping]
---*/

let calls = 0;
const array = [, , ,];

const obj = array.group(function () {
  calls++;
  return 'key';
});

assert.sameValue(calls, 3);

assert(0 in obj['key'], 'group has a first element');
assert(1 in obj['key'], 'group has a second element');
assert(2 in obj['key'], 'group has a third element');
assert.compareArray(obj['key'], [void 0, void 0, void 0]);

reportCompare(0, 0);

// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group errors when return value cannot be converted to a property key.
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    c. Let propertyKey be ? ToPropertyKey(? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »)).

  ...
features: [array-grouping]
---*/

assert.throws(Test262Error, function() {
  const array = [1];
  array.group(function() {
    throw new Test262Error('throw in callback');
  })
});

reportCompare(0, 0);

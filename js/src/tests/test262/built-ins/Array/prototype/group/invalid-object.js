// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group applied to null/undefined throws a TypeError
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  1. Let O be ? ToObject(this value).

  ...
features: [array-grouping]
---*/

const throws = function() {
  throw new Test262Error('callback function should not be called')
};

assert.throws(TypeError, function() {
  Array.prototype.group.call(undefined, throws);
}, 'undefined this value');

assert.throws(TypeError, function() {
  Array.prototype.group.call(undefined, throws);
}, 'null this value');

reportCompare(0, 0);

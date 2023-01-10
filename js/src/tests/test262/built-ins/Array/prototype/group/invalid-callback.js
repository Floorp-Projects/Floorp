// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group called with non-callable throws TypeError
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  3. If IsCallable(callbackfn) is false, throw a TypeError exception.

  ...
features: [array-grouping]
---*/


assert.throws(TypeError, function() {
  [].group(null)
}, "null callback throws TypeError");

assert.throws(TypeError, function() {
  [].group(undefined)
}, "undefined callback throws TypeError");

assert.throws(TypeError, function() {
  [].group({})
}, "object callback throws TypeError");

reportCompare(0, 0);

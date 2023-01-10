// |reftest| shell-option(--enable-change-array-by-copy) skip-if(!Array.prototype.with||!xulRuntime.shell) -- change-array-by-copy is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.toReversed
description: >
  Array.prototype.toReversed returns a new array even if it has zero or one elements
features: [change-array-by-copy]
includes: [compareArray.js]
---*/

var zero = [];
var zeroReversed = zero.toReversed();
assert.notSameValue(zero, zeroReversed);
assert.compareArray(zero, zeroReversed);

var one = [1];
var oneReversed = one.toReversed();
assert.notSameValue(one, oneReversed);
assert.compareArray(one, oneReversed);

reportCompare(0, 0);

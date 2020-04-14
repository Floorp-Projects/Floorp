// |reftest| skip-if(release_or_beta) -- AggregateError is not released yet
// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-aggregate-error.prototype
description: >
  Property descriptor of AggregateError.prototype
info: |
  AggregateError.prototype

  The initial value of AggregateError.prototype is the intrinsic object %AggregateErrorPrototype%.

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
includes: [propertyHelper.js]
features: [AggregateError]
---*/

assert.sameValue(typeof AggregateError.prototype, 'object');

verifyProperty(AggregateError, 'prototype', {
  enumerable: false,
  writable: false,
  configurable: false
});

reportCompare(0, 0);

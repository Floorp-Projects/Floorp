// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-getownproperty-p
description: >
  Returns a descriptor object from an index property
info: |
  9.4.5.1 [[GetOwnProperty]] ( P )

  ...
  3. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      ...
      iii. Return a PropertyDescriptor{[[Value]]: value, [[Writable]]: true,
      [[Enumerable]]: true, [[Configurable]]: false}.
  ...
includes: [testTypedArray.js, propertyHelper.js]
features: [align-detached-buffer-semantics-with-web-reality, TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA([42, 43]);

  verifyProperty(sample, "0", {
    value: 42,
    configurable: true,
    enumerable: true,
    writable: true,
  });

  verifyProperty(sample, "1", {
    value: 43,
    configurable: true,
    enumerable: true,
    writable: true,
  });
});

reportCompare(0, 0);

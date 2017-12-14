// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.getbiguint64
description: >
  Boolean littleEndian argument coerced in ToBoolean
includes: [typeCoercion.js]
features: [DataView, ArrayBuffer, DataView.prototype.setUint8, BigInt, Symbol, Symbol.toPrimitive]
---*/

var buffer = new ArrayBuffer(8);
var sample = new DataView(buffer, 0);

sample.setUint8(7, 0xff);

// False
assert.sameValue(sample.getBigUint64(0), 0xffn, "no argument");
testCoercibleToBooleanFalse(function (x) {
  assert.sameValue(sample.getBigUint64(0, x), 0xffn);
});

// True
testCoercibleToBooleanTrue(function (x) {
  assert.sameValue(sample.getBigUint64(0, x), 0xff00000000000000n);
});

reportCompare(0, 0);

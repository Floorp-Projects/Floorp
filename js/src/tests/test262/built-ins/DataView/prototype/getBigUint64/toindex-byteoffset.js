// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.getbiguint64
description: >
  ToIndex conversions on byteOffset
includes: [typeCoercion.js]
features: [DataView, ArrayBuffer, DataView.prototype.setUint8, BigInt, Symbol, Symbol.toPrimitive]
---*/

var buffer = new ArrayBuffer(12);
var sample = new DataView(buffer, 0);

sample.setUint8(0, 0x27);
sample.setUint8(1, 0x02);
sample.setUint8(2, 0x06);
sample.setUint8(3, 0x02);
sample.setUint8(4, 0x80);
sample.setUint8(5, 0x00);
sample.setUint8(6, 0x80);
sample.setUint8(7, 0x01);
sample.setUint8(8, 0x7f);
sample.setUint8(9, 0x00);
sample.setUint8(10, 0x01);
sample.setUint8(11, 0x02);

testCoercibleToIndexZero(function (x) {
  assert.sameValue(sample.getBigUint64(x), 0x2702060280008001n);
});

testCoercibleToIndexOne(function (x) {
  assert.sameValue(sample.getBigUint64(x), 0x20602800080017fn);
});

testCoercibleToIndexFromIndex(2, function (x) {
  assert.sameValue(sample.getBigUint64(x), 0x602800080017F00n);
});

testCoercibleToIndexFromIndex(3, function (x) {
  assert.sameValue(sample.getBigUint64(x), 0x2800080017F0001n);
});

reportCompare(0, 0);

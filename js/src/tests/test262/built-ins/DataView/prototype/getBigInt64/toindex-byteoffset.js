// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.getbigint64
description: >
  ToIndex conversions on byteOffset
info: |
  DataView.prototype.getBigInt64 ( byteOffset [ , littleEndian ] )

  1. Let v be the this value.
  2. If littleEndian is not present, let littleEndian be undefined.
  3. Return ? GetViewValue(v, byteOffset, littleEndian, "Int64").

  24.3.1.1 GetViewValue ( view, requestIndex, isLittleEndian, type )

  ...
  4. Let getIndex be ? ToIndex(requestIndex).
  ...
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
  assert.sameValue(sample.getBigInt64(x), 0x2702060280008001n);
});

testCoercibleToIndexOne(function (x) {
  assert.sameValue(sample.getBigInt64(x), 0x20602800080017fn);
});

testCoercibleToIndexFromIndex(2, function (x) {
  assert.sameValue(sample.getBigInt64(x), 0x602800080017F00n);
});

testCoercibleToIndexFromIndex(3, function (x) {
  assert.sameValue(sample.getBigInt64(x), 0x2800080017F0001n);
});

reportCompare(0, 0);

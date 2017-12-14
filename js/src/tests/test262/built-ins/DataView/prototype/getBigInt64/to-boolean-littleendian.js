// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.getbigint64
description: >
  Boolean littleEndian argument coerced in ToBoolean
info: |
  DataView.prototype.getBigInt64 ( byteOffset [ , littleEndian ] )

  1. Let v be the this value.
  2. If littleEndian is not present, let littleEndian be undefined.
  3. Return ? GetViewValue(v, byteOffset, littleEndian, "Int64").

  24.3.1.1 GetViewValue ( view, requestIndex, isLittleEndian, type )

  ...
  5. Set isLittleEndian to ToBoolean(isLittleEndian).
  ...
  12. Let bufferIndex be getIndex + viewOffset.
  13. Return GetValueFromBuffer(buffer, bufferIndex, type, false,
  "Unordered", isLittleEndian).

  24.1.1.6 GetValueFromBuffer ( arrayBuffer, byteIndex, type,
  isTypedArray, order [ , isLittleEndian ] )

  ...
  9. Return RawBytesToNumber(type, rawValue, isLittleEndian).

  24.1.1.5 RawBytesToNumber( type, rawBytes, isLittleEndian )

  ...
  2. If isLittleEndian is false, reverse the order of the elements of rawBytes.
  ...
includes: [typeCoercion.js]
features: [DataView, ArrayBuffer, DataView.prototype.setUint8, BigInt, Symbol, Symbol.toPrimitive]
---*/

var buffer = new ArrayBuffer(8);
var sample = new DataView(buffer, 0);

sample.setUint8(7, 0xff);

// False
assert.sameValue(sample.getBigInt64(0), 0xffn, "no argument");
testCoercibleToBooleanFalse(function (x) {
  assert.sameValue(sample.getBigInt64(0, x), 0xffn);
});

// True
testCoercibleToBooleanTrue(function (x) {
  assert.sameValue(sample.getBigInt64(0, x), -0x100000000000000n);
});

reportCompare(0, 0);

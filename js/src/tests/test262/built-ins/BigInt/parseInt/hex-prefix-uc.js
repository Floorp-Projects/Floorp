// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: String argument with hexadecimal prefix and zero radix
info: >
  BigInt.parseInt ( string, radix )

  The parseInt function produces a BigInt value dictated by
  interpretation of the contents of the string argument according to
  the specified radix.

  The algorithm is the same as 18.2.5 but with the following edits:

  * For all cases which result in returning NaN, throw a SyntaxError
    exception.
  * For all cases which result in returning -0, return 0n.
  * Replace the second to last step, which casts mathInt to a Number,
    with casting mathInt to a BigInt.

  18.2.5 parseInt ( string, radix )

  The parseInt function produces an integer value dictated by
  interpretation of the contents of the string argument according to
  the specified radix. Leading white space in string is ignored. If
  radix is undefined or 0, it is assumed to be 10 except when the
  number begins with the code unit pairs 0x or 0X, in which case a
  radix of 16 is assumed. If radix is 16, the number may also
  optionally begin with the code unit pairs 0x or 0X.
features: [BigInt]
---*/

assert.sameValue(BigInt.parseInt("0X0", 0), 0x0n);

assert.sameValue(BigInt.parseInt("0X1"), 0x1n);

assert.sameValue(BigInt.parseInt("0X2"), 0x2n);

assert.sameValue(BigInt.parseInt("0X3"), 0x3n);

assert.sameValue(BigInt.parseInt("0X4"), 0x4n);

assert.sameValue(BigInt.parseInt("0X5"), 0x5n);

assert.sameValue(BigInt.parseInt("0X6"), 0x6n);

assert.sameValue(BigInt.parseInt("0X7"), 0x7n);

assert.sameValue(BigInt.parseInt("0X8"), 0x8n);

assert.sameValue(BigInt.parseInt("0X9"), 0x9n);

assert.sameValue(BigInt.parseInt("0XA"), 0xAn);

assert.sameValue(BigInt.parseInt("0XB"), 0xBn);

assert.sameValue(BigInt.parseInt("0XC"), 0xCn);

assert.sameValue(BigInt.parseInt("0XD"), 0xDn);

assert.sameValue(BigInt.parseInt("0XE"), 0xEn);

assert.sameValue(BigInt.parseInt("0XF"), 0xFn);

assert.sameValue(BigInt.parseInt("0XE"), 0xEn);

assert.sameValue(BigInt.parseInt("0XABCDEF"), 0xABCDEFn);

reportCompare(0, 0);

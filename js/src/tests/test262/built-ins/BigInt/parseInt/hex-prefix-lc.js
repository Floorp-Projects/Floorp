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

assert.sameValue(BigInt.parseInt("0x0", 0), 0x0n);

assert.sameValue(BigInt.parseInt("0x1", 0), 0x1n);

assert.sameValue(BigInt.parseInt("0x2", 0), 0x2n);

assert.sameValue(BigInt.parseInt("0x3", 0), 0x3n);

assert.sameValue(BigInt.parseInt("0x4", 0), 0x4n);

assert.sameValue(BigInt.parseInt("0x5", 0), 0x5n);

assert.sameValue(BigInt.parseInt("0x6", 0), 0x6n);

assert.sameValue(BigInt.parseInt("0x7", 0), 0x7n);

assert.sameValue(BigInt.parseInt("0x8", 0), 0x8n);

assert.sameValue(BigInt.parseInt("0x9", 0), 0x9n);

assert.sameValue(BigInt.parseInt("0xA", 0), 0xAn);

assert.sameValue(BigInt.parseInt("0xB", 0), 0xBn);

assert.sameValue(BigInt.parseInt("0xC", 0), 0xCn);

assert.sameValue(BigInt.parseInt("0xD", 0), 0xDn);

assert.sameValue(BigInt.parseInt("0xE", 0), 0xEn);

assert.sameValue(BigInt.parseInt("0xF", 0), 0xFn);

assert.sameValue(BigInt.parseInt("0xE", 0), 0xEn);

assert.sameValue(BigInt.parseInt("0xABCDEF", 0), 0xABCDEFn);

reportCompare(0, 0);

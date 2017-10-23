// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Invalid radix (1)
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

  [...]
  8. If R ≠ 0, then
     a. If R < 2 or R > 36, return NaN.
features: [BigInt, arrow-function]
---*/

assert.throws(SyntaxError, () => BigInt.parseInt("0", 1), "0");
assert.throws(SyntaxError, () => BigInt.parseInt("1", 1), "1");
assert.throws(SyntaxError, () => BigInt.parseInt("2", 1), "2");
assert.throws(SyntaxError, () => BigInt.parseInt("3", 1), "3");
assert.throws(SyntaxError, () => BigInt.parseInt("4", 1), "4");
assert.throws(SyntaxError, () => BigInt.parseInt("5", 1), "5");
assert.throws(SyntaxError, () => BigInt.parseInt("6", 1), "6");
assert.throws(SyntaxError, () => BigInt.parseInt("7", 1), "7");
assert.throws(SyntaxError, () => BigInt.parseInt("8", 1), "8");
assert.throws(SyntaxError, () => BigInt.parseInt("9", 1), "9");
assert.throws(SyntaxError, () => BigInt.parseInt("10", 1), "10");
assert.throws(SyntaxError, () => BigInt.parseInt("11", 1), "11");

reportCompare(0, 0);

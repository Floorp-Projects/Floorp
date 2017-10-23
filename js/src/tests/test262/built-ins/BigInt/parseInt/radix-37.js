// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Invalid radix (37)
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

assert.throws(SyntaxError, () => BigInt.parseInt("0", 37), "0");
assert.throws(SyntaxError, () => BigInt.parseInt("1", 37), "1");
assert.throws(SyntaxError, () => BigInt.parseInt("2", 37), "2");
assert.throws(SyntaxError, () => BigInt.parseInt("3", 37), "3");
assert.throws(SyntaxError, () => BigInt.parseInt("4", 37), "4");
assert.throws(SyntaxError, () => BigInt.parseInt("5", 37), "5");
assert.throws(SyntaxError, () => BigInt.parseInt("6", 37), "6");
assert.throws(SyntaxError, () => BigInt.parseInt("7", 37), "7");
assert.throws(SyntaxError, () => BigInt.parseInt("8", 37), "8");
assert.throws(SyntaxError, () => BigInt.parseInt("9", 37), "9");
assert.throws(SyntaxError, () => BigInt.parseInt("10", 37), "10");
assert.throws(SyntaxError, () => BigInt.parseInt("11", 37), "11");

reportCompare(0, 0);

// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Leading whitespace (U+0009)
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
features: [BigInt, arrow-function]
---*/

assert.sameValue(BigInt.parseInt("\u00091"), 1n);
assert.sameValue(BigInt.parseInt("\u0009\u0009-1"), -1n);
assert.sameValue(BigInt.parseInt("	1"), 1n);
assert.sameValue(BigInt.parseInt("			1"), 1n);
assert.sameValue(BigInt.parseInt("			\u0009			\u0009-1"), -1n);
assert.throws(SyntaxError, () => BigInt.parseInt("\u0009"));

reportCompare(0, 0);

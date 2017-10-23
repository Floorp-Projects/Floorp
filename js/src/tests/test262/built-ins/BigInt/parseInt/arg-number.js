// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Number argument
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

  1. Let inputString be ? ToString(string).
features: [BigInt, arrow-function]
---*/

assert.sameValue(BigInt.parseInt(-1), -1n);
assert.throws(SyntaxError, () => BigInt.parseInt(Infinity));
assert.throws(SyntaxError, () => BigInt.parseInt(NaN));
assert.sameValue(BigInt.parseInt(-0), 0n);
assert.sameValue(BigInt.parseInt(new Number(-1)), -1n);
assert.throws(SyntaxError, () => BigInt.parseInt(new Number(Infinity)));
assert.throws(SyntaxError, () => BigInt.parseInt(new Number(NaN)));
assert.sameValue(BigInt.parseInt(new Number(-0)), 0n);

reportCompare(0, 0);

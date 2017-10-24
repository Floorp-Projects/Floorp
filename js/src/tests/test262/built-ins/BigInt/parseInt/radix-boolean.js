// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Boolean radix
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
  6. Let R be ? ToInt32(radix).
  7. Let stripPrefix be true.
  8. If R ≠ 0, then
     a. If R < 2 or R > 36, return NaN.
     b. If R ≠ 16, let stripPrefix be false.
  9. Else R = 0,
     a. Let R be 10.
  [...]
features: [BigInt, arrow-function]
---*/

assert.sameValue(BigInt.parseInt("11", false), 11n);
assert.throws(SyntaxError, () => BigInt.parseInt("11", true));
assert.sameValue(BigInt.parseInt("11", new Boolean(false)), 11n);
assert.throws(SyntaxError, () => BigInt.parseInt("11", new Boolean(true)));

reportCompare(0, 0);

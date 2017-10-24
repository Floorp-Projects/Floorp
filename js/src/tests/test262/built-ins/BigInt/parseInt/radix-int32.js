// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Radix converted using ToInt32
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
features: [BigInt]
---*/

assert.sameValue(BigInt.parseInt("11", 2.1), 0b11n);
assert.sameValue(BigInt.parseInt("11", 2.5), 0b11n);
assert.sameValue(BigInt.parseInt("11", 2.9), 0b11n);
assert.sameValue(BigInt.parseInt("11", 2.000000000001), 0b11n);
assert.sameValue(BigInt.parseInt("11", 2.999999999999), 0b11n);
assert.sameValue(BigInt.parseInt("11", 4294967298), 0b11n);
assert.sameValue(BigInt.parseInt("11", 4294967296), 11n);
assert.throws(SyntaxError, () => BigInt.parseInt("11", -2147483650), "-2147483650");
assert.sameValue(BigInt.parseInt("11", -4294967294), 0b11n);
assert.sameValue(BigInt.parseInt("11", NaN), 11n);
assert.sameValue(BigInt.parseInt("11", +0), 11n);
assert.sameValue(BigInt.parseInt("11", -0), 11n);
assert.sameValue(BigInt.parseInt("11", Number.POSITIVE_INFINITY), 11n);
assert.sameValue(BigInt.parseInt("11", Number.NEGATIVE_INFINITY), 11n);

reportCompare(0, 0);

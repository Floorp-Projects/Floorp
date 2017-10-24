// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Radix is undefined
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
  [...]
  9. Else R = 0,
     a. Let R be 10.
features: [BigInt]
---*/

assert.sameValue(BigInt.parseInt("0"), 0n);

assert.sameValue(BigInt.parseInt("1"), 1n);

assert.sameValue(BigInt.parseInt("2"), 2n);

assert.sameValue(BigInt.parseInt("3"), 3n);

assert.sameValue(BigInt.parseInt("4"), 4n);

assert.sameValue(BigInt.parseInt("5"), 5n);

assert.sameValue(BigInt.parseInt("6"), 6n);

assert.sameValue(BigInt.parseInt("7"), 7n);

assert.sameValue(BigInt.parseInt("8"), 8n);

assert.sameValue(BigInt.parseInt("9"), 9n);

assert.sameValue(BigInt.parseInt("10"), 10n);

assert.sameValue(BigInt.parseInt("11"), 11n);

assert.sameValue(BigInt.parseInt("9999"), 9999n);

reportCompare(0, 0);

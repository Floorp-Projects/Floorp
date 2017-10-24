// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Negative decimal string argument with radix 10
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
  4. If S is not empty and the first code unit of S is the code unit
     0x002D (HYPHEN-MINUS), let sign be -1.
  [...]
  16. Return sign × number.
features: [BigInt]
---*/

assert.sameValue(BigInt.parseInt("-1", 10), -1n);
assert.sameValue(BigInt.parseInt("-10", 10), -10n);
assert.sameValue(BigInt.parseInt("-100", 10), -100n);
assert.sameValue(BigInt.parseInt("-1000", 10), -1000n);
assert.sameValue(BigInt.parseInt("-10000", 10), -10000n);
assert.sameValue(BigInt.parseInt("-100000", 10), -100000n);
assert.sameValue(BigInt.parseInt("-1000000", 10), -1000000n);
assert.sameValue(BigInt.parseInt("-10000000", 10), -10000000n);
assert.sameValue(BigInt.parseInt("-100000000", 10), -100000000n);
assert.sameValue(BigInt.parseInt("-1000000000", 10), -1000000000n);
assert.sameValue(BigInt.parseInt("-10000000000", 10), -10000000000n);
assert.sameValue(BigInt.parseInt("-100000000000", 10), -100000000000n);
assert.sameValue(BigInt.parseInt("-1000000000000", 10), -1000000000000n);
assert.sameValue(BigInt.parseInt("-10000000000000", 10), -10000000000000n);
assert.sameValue(BigInt.parseInt("-100000000000000", 10), -100000000000000n);
assert.sameValue(BigInt.parseInt("-1000000000000000", 10), -1000000000000000n);
assert.sameValue(BigInt.parseInt("-10000000000000000", 10), -10000000000000000n);
assert.sameValue(BigInt.parseInt("-100000000000000000", 10), -100000000000000000n);
assert.sameValue(BigInt.parseInt("-1000000000000000000", 10), -1000000000000000000n);
assert.sameValue(BigInt.parseInt("-10000000000000000000", 10), -10000000000000000000n);

reportCompare(0, 0);

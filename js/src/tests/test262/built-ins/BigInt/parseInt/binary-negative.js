// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Negative binary argument with radix 2
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

  The parseInt function is the %parseInt% intrinsic object. When the
  parseInt function is called, the following steps are taken:

  [...]
  4. If S is not empty and the first code unit of S is the code unit
     0x002D (HYPHEN-MINUS), let sign be -1.
  [...]
  16. Return sign × number.
features: [BigInt]
---*/

assert.sameValue(BigInt.parseInt("-1", 2), -1n);
assert.sameValue(BigInt.parseInt("-11", 2), -3n);
assert.sameValue(BigInt.parseInt("-111", 2), -7n);
assert.sameValue(BigInt.parseInt("-1111", 2), -15n);
assert.sameValue(BigInt.parseInt("-11111", 2), -31n);
assert.sameValue(BigInt.parseInt("-111111", 2), -63n);
assert.sameValue(BigInt.parseInt("-1111111", 2), -127n);
assert.sameValue(BigInt.parseInt("-11111111", 2), -255n);
assert.sameValue(BigInt.parseInt("-111111111", 2), -511n);
assert.sameValue(BigInt.parseInt("-1111111111", 2), -1023n);
assert.sameValue(BigInt.parseInt("-11111111111", 2), -2047n);
assert.sameValue(BigInt.parseInt("-111111111111", 2), -4095n);
assert.sameValue(BigInt.parseInt("-1111111111111", 2), -8191n);
assert.sameValue(BigInt.parseInt("-11111111111111", 2), -16383n);
assert.sameValue(BigInt.parseInt("-111111111111111", 2), -32767n);
assert.sameValue(BigInt.parseInt("-1111111111111111", 2), -65535n);
assert.sameValue(BigInt.parseInt("-11111111111111111", 2), -131071n);
assert.sameValue(BigInt.parseInt("-111111111111111111", 2), -262143n);
assert.sameValue(BigInt.parseInt("-1111111111111111111", 2), -524287n);
assert.sameValue(BigInt.parseInt("-11111111111111111111", 2), -1048575n);

reportCompare(0, 0);

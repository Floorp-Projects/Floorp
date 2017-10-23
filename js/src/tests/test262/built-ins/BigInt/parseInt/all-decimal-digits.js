// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: 01234567890 parsed in different radices
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
  interpretation of the contents of the string argument according to the
  specified radix. Leading white space in string is ignored. If radix is
  undefined or 0, it is assumed to be 10 except when the number begins
  with the code unit pairs 0x or 0X, in which case a radix of 16 is
  assumed. If radix is 16, the number may also optionally begin with the
  code unit pairs 0x or 0X.

  [...]

  NOTE: parseInt may interpret only a leading portion of string as an
  integer value; it ignores any code units that cannot be interpreted as
  part of the notation of an integer, and no indication is given that
  any such code units were ignored.
features: [BigInt]
---*/

assert.sameValue(BigInt.parseInt("01234567890", 2), 1n);
assert.sameValue(BigInt.parseInt("01234567890", 3), 5n);
assert.sameValue(BigInt.parseInt("01234567890", 4), 27n);
assert.sameValue(BigInt.parseInt("01234567890", 5), 194n);
assert.sameValue(BigInt.parseInt("01234567890", 6), 1865n);
assert.sameValue(BigInt.parseInt("01234567890", 7), 22875n);
assert.sameValue(BigInt.parseInt("01234567890", 8), 342391n);
assert.sameValue(BigInt.parseInt("01234567890", 9), 6053444n);
assert.sameValue(BigInt.parseInt("01234567890", 10), 1234567890n);

reportCompare(0, 0);

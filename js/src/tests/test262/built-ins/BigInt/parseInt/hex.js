// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Hexadecimal string argument with radix 16
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
features: [BigInt]
---*/

assert.sameValue(BigInt.parseInt("0x1", 16), 1n);
assert.sameValue(BigInt.parseInt("0X10", 16), 16n);
assert.sameValue(BigInt.parseInt("0x100", 16), 256n);
assert.sameValue(BigInt.parseInt("0X1000", 16), 4096n);
assert.sameValue(BigInt.parseInt("0x10000", 16), 65536n);
assert.sameValue(BigInt.parseInt("0X100000", 16), 1048576n);
assert.sameValue(BigInt.parseInt("0x1000000", 16), 16777216n);
assert.sameValue(BigInt.parseInt("0x10000000", 16), 268435456n);
assert.sameValue(BigInt.parseInt("0x100000000", 16), 4294967296n);
assert.sameValue(BigInt.parseInt("0x1000000000", 16), 68719476736n);
assert.sameValue(BigInt.parseInt("0x10000000000", 16), 1099511627776n);
assert.sameValue(BigInt.parseInt("0x100000000000", 16), 17592186044416n);
assert.sameValue(BigInt.parseInt("0x1000000000000", 16), 281474976710656n);
assert.sameValue(BigInt.parseInt("0x10000000000000", 16), 4503599627370496n);
assert.sameValue(BigInt.parseInt("0x100000000000000", 16), 72057594037927936n);
assert.sameValue(BigInt.parseInt("0x1000000000000000", 16), 1152921504606846976n);
assert.sameValue(BigInt.parseInt("0x10000000000000000", 16), 18446744073709551616n);
assert.sameValue(BigInt.parseInt("0x100000000000000000", 16), 295147905179352825856n);
assert.sameValue(BigInt.parseInt("0x1000000000000000000", 16), 4722366482869645213696n);
assert.sameValue(BigInt.parseInt("0x10000000000000000000", 16), 75557863725914323419136n);

reportCompare(0, 0);

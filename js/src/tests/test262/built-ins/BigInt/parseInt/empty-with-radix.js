// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: SyntaxError thrown for invalid strings
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
  2. Let S be a newly created substring of inputString consisting of
     the first code unit that is not a StrWhiteSpaceChar and all code
     units following that code unit. (In other words, remove leading
     white space.) If inputString does not contain any such code unit,
     let S be the empty string.
  [...]
  11. If S contains a code unit that is not a radix-R digit, let Z be
      the substring of S consisting of all code units before the first
      such code unit; otherwise, let Z be S.
  12. If Z is empty, return NaN.
features: [BigInt, arrow-function]
---*/

for (var i = 2; i <= 36; i++) {
  assert.throws(SyntaxError, () => BigInt.parseInt("$0x", i), "$0x radix " + i);
  assert.throws(SyntaxError, () => BigInt.parseInt("$0X", i), "$0X radix " + i);
  assert.throws(SyntaxError, () => BigInt.parseInt("$$$", i), "$$$ radix " + i);
  assert.throws(SyntaxError, () => BigInt.parseInt("", i), "the empty string, radix " + i);
  assert.throws(SyntaxError, () => BigInt.parseInt(" ", i), "a string with a single space, radix " + i);
}

reportCompare(0, 0);

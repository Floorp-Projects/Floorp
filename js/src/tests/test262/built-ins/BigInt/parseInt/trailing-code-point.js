// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: Trailing non-digit UTF-16 code point
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
includes: [decimalToHexString.js]
features: [BigInt]
---*/

var errorCount = 0;
var count = 0;
var indexP;
var indexO = 0;
for (var index = 0; index <= 65535; index++) {
  if ((index < 0x0030) || (index > 0x0039) &&
      (index < 0x0041) || (index > 0x005A) &&
      (index < 0x0061) || (index > 0x007A)) {
    var hex = decimalToHexString(index);
    if (BigInt.parseInt("1Z" + String.fromCharCode(index), 36) !== 71n) {
      if (indexO === 0) {
        indexO = index;
      } else {
        if ((index - indexP) !== 1) {
          if ((indexP - indexO) !== 0) {
            var hexP = decimalToHexString(indexP);
            var hexO = decimalToHexString(indexO);
            $ERROR('#' + hexO + '-' + hexP + ' ');
          }
          else {
            var hexP = decimalToHexString(indexP);
            $ERROR('#' + hexP + ' ');
          }
          indexO = index;
        }
      }
      indexP = index;
      errorCount++;
    }
    count++;
  }
}

if (errorCount > 0) {
  if ((indexP - indexO) !== 0) {
    var hexP = decimalToHexString(indexP);
    var hexO = decimalToHexString(indexO);
    $ERROR('#' + hexO + '-' + hexP + ' ');
  } else {
    var hexP = decimalToHexString(indexP);
    $ERROR('#' + hexP + ' ');
  }
  $ERROR('Total error: ' + errorCount + ' bad Unicode character in ' + count + ' ');
}

reportCompare(0, 0);

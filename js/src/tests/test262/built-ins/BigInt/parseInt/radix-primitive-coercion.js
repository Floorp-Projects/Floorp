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
features: [BigInt, arrow-function]
---*/

var object = {valueOf() {return 2}};
assert.sameValue(BigInt.parseInt("11", object), 0b11n);

var object = {valueOf() {return 2}, toString() {return 1}};
assert.sameValue(BigInt.parseInt("11", object), 0b11n);

var object = {valueOf() {return 2}, toString() {return {}}};
assert.sameValue(BigInt.parseInt("11", object), 0b11n);

var object = {valueOf() {return 2}, toString() {throw new Test262Error()}};
assert.sameValue(BigInt.parseInt("11", object), 0b11n);

var object = {toString() {return 2}};
assert.sameValue(BigInt.parseInt("11", object), 0b11n);

var object = {valueOf() {return {}}, toString() {return 2}}
assert.sameValue(BigInt.parseInt("11", object), 0b11n);

var object = {valueOf() {throw new Test262Error()}, toString() {return 2}};
assert.throws(Test262Error, () => BigInt.parseInt("11", object));

var object = {valueOf() {return {}}, toString() {return {}}};
assert.throws(TypeError, () => BigInt.parseInt("11", object));

reportCompare(0, 0);

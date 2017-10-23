// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: BigInt.parseInt.prototype is undefined
  BigInt.parseInt ( string , radix )

  The parseInt function produces a BigInt value dictated by
  interpretation of the contents of the string argument according to
  the specified radix.

  9.3 Built-in Function Objects

  Built-in functions that are not constructors do not have a prototype
  property unless otherwise specified in the description of a
  particular function.
features: [BigInt]
---*/

assert.sameValue(BigInt.parseInt.prototype, undefined);

reportCompare(0, 0);

// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint-parseint-string-radix
description: BigInt.parseInt is not a constructor
info: >
  BigInt.parseInt ( string , radix )

  The parseInt function produces a BigInt value dictated by
  interpretation of the contents of the string argument according to
  the specified radix.

  9.3 Built-in Function Objects

  Built-in function objects that are not identified as constructors do
  not implement the [[Construct]] internal method unless otherwise
  specified in the description of a particular function.
features: [BigInt, arrow-function]
---*/

assert.throws(TypeError, () => new BigInt.parseInt());

reportCompare(0, 0);

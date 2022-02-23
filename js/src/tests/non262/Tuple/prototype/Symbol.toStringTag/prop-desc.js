// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-%Tuple%.prototype-@@tostringtag
description: >
info: |
  8.2.3.4 %Tuple%.prototype [ @@toStringTag ]

The initial value of Tuple.prototype[@@toStringTag] is the String value "Tuple".

This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.

features: [Symbol.toStringTag, Tuple]
---*/

var TuplePrototype = Tuple.prototype;
var desc = Object.getOwnPropertyDescriptor(
  TuplePrototype, Symbol.toStringTag
);

assertEq(desc.value, "Tuple");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(desc.set, undefined);
assertEq(desc.get, undefined);

reportCompare(0, 0);

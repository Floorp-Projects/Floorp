// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*---
esid: sec-get-%tuple%.prototype.length
description: >
info: |
  get %Tuple%.prototype.length

Tuple.prototype.length is an accessor property whose set accessor function is undefined. Its get accessor function performs the following steps:

1. Let T be ? thisTupleValue(this value).
2. Let size be the length of T.[[Sequence]].
3. Return size.

features: [Tuple]
---*/

/* Section 8.2.3.2 */

let TuplePrototype = Tuple.prototype;
let desc = Object.getOwnPropertyDescriptor(TuplePrototype, "length");

assertEq(typeof desc.get, "function");

assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(desc.set, undefined);

let values = [[#[1,2,3], 3], [#[1], 1], [#[], 0]];

for (pair of values) {
    let tup = pair[0];
    let len = pair[1];
    assertEq(desc.get.call(tup), len);
    assertEq(desc.get.call(Object(tup)), len);
    assertEq(tup["length"], len);
}

assertThrowsInstanceOf(() => desc.get.call("monkeys"), TypeError, "get length method called on incompatible string");
assertThrowsInstanceOf(() => desc.get.call(new Object()), TypeError, "get length method called on incompatible Object");

reportCompare(0, 0);

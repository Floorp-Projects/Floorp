// Array.keys() and .entries() on an empty array produce empty iterators

var arr = [];
var ki = arr.keys(), ei = arr.entries();
var p = Object.getPrototypeOf(ki);
assertEq(Object.getPrototypeOf(ei), p);

for (let k of ki)
  throw "FAIL";
for (let [k, v] of ei)
  throw "FAIL";

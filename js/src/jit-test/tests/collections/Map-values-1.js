// map.keys(), .values(), and .entries() on an empty map produce empty iterators.

var m = Map();
var ki = m.keys(), vi = m.values(), ei = m.entries();
var p = Object.getPrototypeOf(ki)
assertEq(Object.getPrototypeOf(vi), p);
assertEq(Object.getPrototypeOf(ei), p);

for (let k of ki)
    throw "FAIL";
for (let v of vi)
    throw "FAIL";
for (let [k, v] of ei)
    throw "FAIL";

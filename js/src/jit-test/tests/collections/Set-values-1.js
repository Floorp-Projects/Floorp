// set.keys(), .values(), and .entries() on an empty set produce empty iterators

var s = new Set();
var ki = s.keys(), vi = s.values(), ei = s.entries();
var p = Object.getPrototypeOf(ki);
assertEq(Object.getPrototypeOf(vi), p);
assertEq(Object.getPrototypeOf(ei), p);

for (let k of ki)
	throw "FAIL";
for (let v of vi)
	throw "FAIL";
for (let [k, v] of ei)
	throw "FAIL";

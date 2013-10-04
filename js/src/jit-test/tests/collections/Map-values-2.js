// map.keys() and map.values() return iterators over the key or the value,
// respectively, of each key-value pair in the map.

load(libdir + "iteration.js");

var data = [["one", 1], ["two", 2], ["three", 3], ["four", 4]];
var m = Map(data);

var ki = m.keys();
assertIteratorResult(ki.next(), "one", false);
assertIteratorResult(ki.next(), "two", false);
assertIteratorResult(ki.next(), "three", false);
assertIteratorResult(ki.next(), "four", false);
assertIteratorResult(ki.next(), undefined, true);

assertEq([k for (k of m.keys())].toSource(), ["one", "two", "three", "four"].toSource());
assertEq([k for (k of m.values())].toSource(), [1, 2, 3, 4].toSource());
assertEq([k for (k of m.entries())].toSource(), data.toSource());

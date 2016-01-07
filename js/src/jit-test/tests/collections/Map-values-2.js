// map.keys() and map.values() return iterators over the key or the value,
// respectively, of each key-value pair in the map.

load(libdir + "iteration.js");

var data = [["one", 1], ["two", 2], ["three", 3], ["four", 4]];
var m = new Map(data);

var ki = m.keys();
assertIteratorNext(ki, "one");
assertIteratorNext(ki, "two");
assertIteratorNext(ki, "three");
assertIteratorNext(ki, "four");
assertIteratorDone(ki, undefined);

assertEq([...m.keys()].toSource(), ["one", "two", "three", "four"].toSource());
assertEq([...m.values()].toSource(), [1, 2, 3, 4].toSource());
assertEq([...m.entries()].toSource(), data.toSource());

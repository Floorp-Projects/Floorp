// set.keys() and set.values() return iterators over the elements
// and set.entries() returns an iterator that yields pairs [e, e].

load(libdir + "iteration.js");

var data = [1, 2, 3, 4];
var s = Set(data);

var ki = s.keys();
assertIteratorResult(ki.next(), 1, false);
assertIteratorResult(ki.next(), 2, false);
assertIteratorResult(ki.next(), 3, false);
assertIteratorResult(ki.next(), 4, false);
assertIteratorResult(ki.next(), undefined, true);

assertEq([...s.keys()].toSource(), data.toSource());
assertEq([...s.values()].toSource(), data.toSource());
assertEq([...s.entries()].toSource(), [[1, 1], [2, 2], [3, 3], [4, 4]].toSource());

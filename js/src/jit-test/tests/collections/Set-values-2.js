// set.keys() and set.values() return iterators over the elements
// and set.entries() returns an iterator that yields pairs [e, e].

load(libdir + "iteration.js");

var data = [1, 2, 3, 4];
var s = new Set(data);

var ki = s.keys();
assertIteratorNext(ki, 1);
assertIteratorNext(ki, 2);
assertIteratorNext(ki, 3);
assertIteratorNext(ki, 4);
assertIteratorDone(ki, undefined);

assertDeepEq([...s.keys()], data);
assertDeepEq([...s.values()], data);
assertDeepEq([...s.entries()], [[1, 1], [2, 2], [3, 3], [4, 4]]);

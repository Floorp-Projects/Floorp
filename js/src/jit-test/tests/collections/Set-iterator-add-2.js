// A Set iterator does not iterate over new entries added after it throws StopIteration.

load(libdir + "iteration.js");

var set = new Set();
var iter0 = set[Symbol.iterator](), iter1 = set[Symbol.iterator]();
assertIteratorDone(iter0, undefined);  // closes iter0
set.add("x");
assertIteratorDone(iter0, undefined);  // already closed
assertIteratorNext(iter1, "x");  // was not yet closed

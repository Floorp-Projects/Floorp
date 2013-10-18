// A Set iterator does not iterate over new entries added after it throws StopIteration.

load(libdir + "iteration.js");

var set = Set();
var iter0 = set[std_iterator](), iter1 = set[std_iterator]();
assertIteratorDone(iter0, undefined);  // closes iter0
set.add("x");
assertIteratorDone(iter0, undefined);  // already closed
assertIteratorNext(iter1, "x");  // was not yet closed

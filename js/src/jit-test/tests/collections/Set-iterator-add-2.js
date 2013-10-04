// A Set iterator does not iterate over new entries added after it throws StopIteration.

load(libdir + "iteration.js");

var set = Set();
var iter0 = set[std_iterator](), iter1 = set[std_iterator]();
assertIteratorResult(iter0.next(), undefined, true);  // closes iter0
set.add("x");
assertIteratorResult(iter0.next(), undefined, true);  // already closed
assertIteratorResult(iter1.next(), "x", false);  // was not yet closed

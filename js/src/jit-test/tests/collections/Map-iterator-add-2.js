// A Map iterator does not iterate over new entries added after it throws StopIteration.

load(libdir + "iteration.js");

var map = Map();
var iter0 = map[std_iterator](), iter1 = map[std_iterator]();
assertIteratorResult(iter0.next(), undefined, true);  // closes iter0
map.set(1, 2);
assertIteratorResult(iter0.next(), undefined, true);  // already closed
assertIteratorResult(iter1.next(), [1, 2], false);  // was not yet closed

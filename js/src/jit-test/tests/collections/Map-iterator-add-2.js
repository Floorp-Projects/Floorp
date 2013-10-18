// A Map iterator does not iterate over new entries added after it throws StopIteration.

load(libdir + "iteration.js");

var map = Map();
var iter0 = map[std_iterator](), iter1 = map[std_iterator]();
assertIteratorDone(iter0, undefined);  // closes iter0
map.set(1, 2);
assertIteratorDone(iter0, undefined);  // already closed
assertIteratorNext(iter1, [1, 2]);     // was not yet closed

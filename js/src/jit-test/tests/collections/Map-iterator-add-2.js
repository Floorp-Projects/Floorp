// A Map iterator does not iterate over new entries added after it throws StopIteration.

load(libdir + "iteration.js");

var map = new Map();
var iter0 = map[Symbol.iterator](), iter1 = map[Symbol.iterator]();
assertIteratorDone(iter0, undefined);  // closes iter0
map.set(1, 2);
assertIteratorDone(iter0, undefined);  // already closed
assertIteratorNext(iter1, [1, 2]);     // was not yet closed

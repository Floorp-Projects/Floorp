// A closed Map iterator does not visit new entries added after a clear().

load(libdir + "iteration.js");

var m = Map();
var it = m[std_iterator]();
assertIteratorResult(it.next(), undefined, true);  // close the iterator
m.clear();
m.set("a", 1);
assertIteratorResult(it.next(), undefined, true);  // iterator still closed

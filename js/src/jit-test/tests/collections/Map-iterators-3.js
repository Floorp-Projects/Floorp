// A closed Map iterator does not visit new entries added after a clear().

load(libdir + "iteration.js");

var m = Map();
var it = m[std_iterator]();
assertIteratorDone(it, undefined);  // close the iterator
m.clear();
m.set("a", 1);
assertIteratorDone(it, undefined);  // iterator still closed

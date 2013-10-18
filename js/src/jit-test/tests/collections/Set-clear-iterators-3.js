// A closed Set iterator does not visit new entries added after a clear().

load(libdir + "iteration.js");

var s = Set();
var it = s[std_iterator]();
assertIteratorDone(it, undefined);  // close the iterator
s.clear();
s.add("a");
assertIteratorDone(it, undefined);

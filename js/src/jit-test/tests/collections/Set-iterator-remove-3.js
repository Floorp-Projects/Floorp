// A set iterator can cope with removing the next entry, then the current entry.

load(libdir + "iteration.js");

var set = Set("abcd");
var iter = set[std_iterator]();
assertIteratorNext(iter, "a");
assertIteratorNext(iter, "b");
set.delete("c");
set.delete("b");
assertIteratorNext(iter, "d");
assertIteratorDone(iter, undefined);

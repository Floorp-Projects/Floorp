// A Map iterator continues to visit entries added after a clear().

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var m = Map([["a", 1]]);
var it = m[std_iterator]();
assertIteratorResult(it.next(), ["a", 1], false);
m.clear();
m.set("b", 2);
assertIteratorResult(it.next(), ["b", 2], false);
assertIteratorResult(it.next(), undefined, true);

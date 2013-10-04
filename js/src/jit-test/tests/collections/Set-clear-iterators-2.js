// A Set iterator continues to visit entries added after a clear().

load(libdir + "iteration.js");

var s = Set(["a"]);
var it = s[std_iterator]();
assertIteratorResult(it.next(), "a", false);
s.clear();
s.add("b");
assertIteratorResult(it.next(), "b", false);
assertIteratorResult(it.next(), undefined, true);

// A Set iterator continues to visit entries added after a clear().

load(libdir + "asserts.js");

var s = Set(["a"]);
var it = s.iterator();
assertEq(it.next(), "a");
s.clear();
s.add("b");
assertEq(it.next(), "b");
assertThrowsValue(it.next.bind(it), StopIteration);

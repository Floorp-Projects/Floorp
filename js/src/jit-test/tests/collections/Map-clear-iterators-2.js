// A Map iterator continues to visit entries added after a clear().

load(libdir + "asserts.js");

var m = Map([["a", 1]]);
var it = m.iterator();
assertEq(it.next()[0], "a");
m.clear();
m.set("b", 2);
var pair = it.next()
assertEq(pair[0], "b");
assertEq(pair[1], 2);
assertThrowsValue(it.next.bind(it), StopIteration);



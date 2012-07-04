// A set iterator can cope with removing the next entry, then the current entry.

load(libdir + "asserts.js");

var set = Set("abcd");
var iter = set.iterator();
assertEq(iter.next(), "a");
assertEq(iter.next(), "b");
set.delete("c");
set.delete("b");
assertEq(iter.next(), "d");
assertThrowsValue(function () { iter.next(); }, StopIteration);

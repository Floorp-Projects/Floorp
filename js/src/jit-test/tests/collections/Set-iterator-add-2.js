// A Set iterator does not iterate over new entries added after it throws StopIteration.

load(libdir + "asserts.js");

var set = Set();
var iter0 = set.iterator(), iter1 = set.iterator();
assertThrowsValue(function () { iter0.next(); }, StopIteration);  // closes iter0
set.add("x");
assertThrowsValue(function () { iter0.next(); }, StopIteration);  // already closed
assertEq(iter1.next(), "x");  // was not yet closed

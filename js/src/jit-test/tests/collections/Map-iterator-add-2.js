// A Map iterator does not iterate over new entries added after it throws StopIteration.

load(libdir + "asserts.js");
load(libdir + "eqArrayHelper.js");

var map = Map();
var iter0 = map.iterator(), iter1 = map.iterator();
assertThrowsValue(function () { iter0.next(); }, StopIteration);  // closes iter0
map.set(1, 2);
assertThrowsValue(function () { iter0.next(); }, StopIteration);  // already closed
assertEqArray(iter1.next(), [1, 2]);  // was not yet closed

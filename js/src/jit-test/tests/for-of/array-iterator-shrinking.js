// If an array is truncated to the left of an iterator it, it.next() throws StopIteration.

load(libdir + "asserts.js");
var arr = [0, 1, 2];
var it = arr.iterator();
it.next();
it.next();
arr.length = 1;
assertThrowsValue(function () { it.next(); }, StopIteration);

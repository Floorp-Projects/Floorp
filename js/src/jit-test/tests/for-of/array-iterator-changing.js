// Array iterators reflect changes to elements of the underlying array.

load(libdir + "asserts.js");
var arr = [0, 1, 2];
var it = arr.iterator();
arr[0] = 1000;
arr[2] = 2000;
assertEq(it.next(), 1000);
assertEq(it.next(), 1);
assertEq(it.next(), 2000);
assertThrowsValue(function () { it.next(); }, StopIteration);

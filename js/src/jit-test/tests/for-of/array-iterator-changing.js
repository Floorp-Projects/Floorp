// Array iterators reflect changes to elements of the underlying array.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var arr = [0, 1, 2];
var it = arr[std_iterator]();
arr[0] = 1000;
arr[2] = 2000;
assertIteratorResult(it.next(), 1000, false);
assertIteratorResult(it.next(), 1, false);
assertIteratorResult(it.next(), 2000, false);
assertIteratorResult(it.next(), undefined, true);

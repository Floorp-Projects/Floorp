// If an array is truncated to the left of an iterator it, it.next() returns { done: true }.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var arr = [0, 1, 2];
var it = arr[std_iterator]();
assertIteratorResult(it.next(), 0, false);
assertIteratorResult(it.next(), 1, false);
arr.length = 1;
assertIteratorResult(it.next(), undefined, true);

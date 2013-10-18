// If an array is truncated to the left of an iterator it, it.next() returns { done: true }.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var arr = [0, 1, 2];
var it = arr[std_iterator]();
assertIteratorNext(it, 0);
assertIteratorNext(it, 1);
arr.length = 1;
assertIteratorDone(it, undefined);

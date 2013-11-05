// If an array is truncated to the left of an iterator it, it.next() returns { done: true }.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var arr = [0, 1, 2];
var it = arr[std_iterator]();
var ki = arr.keys();
var ei = arr.entries();

assertIteratorNext(it, 0);
assertIteratorNext(it, 1);
assertIteratorNext(ki, 0);
assertIteratorNext(ki, 1);
assertIteratorNext(ei, [0, 0]);
assertIteratorNext(ei, [1, 1]);
arr.length = 1;
assertIteratorDone(it, undefined);
assertIteratorDone(ki, undefined);
assertIteratorDone(ei, undefined);

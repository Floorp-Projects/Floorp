// Array iterators reflect changes to elements of the underlying array.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var arr = [0, 1, 2];
var it = arr[std_iterator]();
arr[0] = 1000;
arr[2] = 2000;
assertIteratorNext(it, 1000);
assertIteratorNext(it, 1);
assertIteratorNext(it, 2000);
assertIteratorDone(it, undefined);

// test that .keys() and .entries() have the same behaviour

arr = [0, 1, 2];
var ki = arr.keys();
var ei = arr.entries();
arr[0] = 1000;
arr[2] = 2000;
assertIteratorNext(ki, 0);
assertIteratorNext(ei, [0, 1000]);
assertIteratorNext(ki, 1);
assertIteratorNext(ei, [1, 1]);
assertIteratorNext(ki, 2);
assertIteratorNext(ei, [2, 2000]);
assertIteratorDone(ki, undefined);
assertIteratorDone(ei, undefined);

// If an array with an active iterator is lengthened, the iterator visits the new elements.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var arr = [0, 1];
var it = arr[std_iterator]();
var ki = arr.keys();
var ei = arr.entries();
assertIteratorNext(it, 0);
assertIteratorNext(ki, 0);
assertIteratorNext(ei, [0, 0]);
assertIteratorNext(it, 1);
assertIteratorNext(ki, 1);
assertIteratorNext(ei, [1, 1]);
arr[2] = 2;
arr.length = 4;
assertIteratorNext(it, 2);
assertIteratorNext(ki, 2);
assertIteratorNext(ei, [2, 2]);
assertIteratorNext(it, undefined);
assertIteratorNext(ki, 3);
assertIteratorNext(ei, [3, undefined]);
assertIteratorDone(it, undefined);
assertIteratorDone(ki, undefined);
assertIteratorDone(ei, undefined);

// If an array with an active iterator is lengthened, the iterator visits the new elements.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var arr = [0, 1];
var it = arr[std_iterator]();
assertIteratorNext(it, 0);
assertIteratorNext(it, 1);
arr[2] = 2;
arr.length = 4;
assertIteratorNext(it, 2);
assertIteratorNext(it, undefined);
assertIteratorDone(it, undefined);

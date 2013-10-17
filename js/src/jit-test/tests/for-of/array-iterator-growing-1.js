// If an array with an active iterator is lengthened, the iterator visits the new elements.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var arr = [0, 1];
var it = arr[std_iterator]();
assertIteratorResult(it.next(), 0, false);
assertIteratorResult(it.next(), 1, false);
arr[2] = 2;
arr.length = 4;
assertIteratorResult(it.next(), 2, false);
assertIteratorResult(it.next(), undefined, false);
assertIteratorResult(it.next(), undefined, true);

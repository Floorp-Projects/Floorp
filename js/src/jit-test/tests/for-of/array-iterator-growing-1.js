// If an array with an active iterator is lengthened, the iterator visits the new elements.

load(libdir + "asserts.js");
var arr = [0, 1];
var it = arr.iterator();
it.next();
it.next();
arr[2] = 2;
arr.length = 4;
assertEq(it.next(), 2);
assertEq(it.next(), undefined);
assertThrowsValue(function () { it.next(); }, StopIteration);

// for-of in comprehensions does not trigger the JS 1.7 for-in destructuring special case.

version(170);
load(libdir + "asserts.js");

var data = [[1, 2, 3], [4]];
var arr = eval("[a for ([a] of data)]");
assertEq(arr.length, 2);
assertEq(arr[0], 1);
assertEq(arr[1], 4);

arr = eval("[length for ({length} of data)]");
assertEq(arr.length, 2);
assertEq(arr[0], 3);
assertEq(arr[1], 1);

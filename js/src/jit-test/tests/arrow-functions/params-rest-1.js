// Rest parameters are allowed in arrow functions.

load(libdir + "asserts.js");

var A = (...x) => x;
assertDeepEq(A(), []);
assertEq("" + A(3, 4, 5), "3,4,5");

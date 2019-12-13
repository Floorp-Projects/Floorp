// Parameter default values work in arrow functions

load(libdir + "asserts.js");

var f = (a=1, b=2, ...rest) => [a, b, rest];
assertDeepEq(f(), [1, 2, []]);
assertDeepEq(f(0, 0), [0, 0, []]);
assertDeepEq(f(0, 1, 1, 2, 3, 5), [0, 1, [1, 2, 3, 5]]);

// Rest parameters work in arrow functions

load(libdir + "asserts.js");

var f = (a, b, ...rest) => [a, b, rest];
assertDeepEq(f(), [(void 0), (void 0), []]);
assertDeepEq(f(1, 2, 3, 4), [1, 2, [3, 4]]);

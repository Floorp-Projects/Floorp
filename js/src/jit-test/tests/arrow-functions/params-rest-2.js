// Rest parameters work in arrow functions

var f = (a, b, ...rest) => [a, b, rest];
assertEq(f().toSource(), "[(void 0), (void 0), []]");
assertEq(f(1, 2, 3, 4).toSource(), "[1, 2, [3, 4]]");

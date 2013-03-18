// Parameter default values work in arrow functions

var f = (a=1, b=2, ...rest) => [a, b, rest];
assertEq(f().toSource(), "[1, 2, []]");
assertEq(f(0, 0).toSource(), "[0, 0, []]");
assertEq(f(0, 1, 1, 2, 3, 5).toSource(), "[0, 1, [1, 2, 3, 5]]");

// The argument to Set can be a generator-expression.

var s = new Set(k * k for (k of [1, 2, 3, 4]));
assertEq(s.size, 4);
assertEq(s.has(1), true);
assertEq(s.has(4), true);
assertEq(s.has(9), true);
assertEq(s.has(16), true);

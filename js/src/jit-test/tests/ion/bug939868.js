function f(x, y) { return x || Math.fround(y); }
assertEq(f(0, 0), 0);
assertEq(f(0xfffffff, 0), 0xfffffff);

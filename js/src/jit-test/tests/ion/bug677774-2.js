function f(a, b) { return a > b; }

assertEq(f(5, 6), false)
assertEq(f(1337, 42), true)
assertEq(f(-12, 6), false)
assertEq(f(5, -6), true)
assertEq(f(-3, -2), false)
assertEq(f(-5, -6), true)

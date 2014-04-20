var intMin = -2147483648;

assertEq(intMin % (-2147483648), -0);
assertEq(intMin % (-3), -2);
assertEq(intMin % (-1), -0);
assertEq(intMin % 1, -0);
assertEq(intMin % 3, -2);
assertEq(intMin % 10, -8);
assertEq(intMin % 2147483647, -1);

assertEq((-2147483648) % (-2147483648), -0);
for (var i = -10; i <= 10; ++i)
    assertEq(i % (-2147483648), i);
assertEq(2147483647 % (-2147483648), 2147483647);

assertEq((-2147483648) / (-2147483648), 1);
assertEq(0 / (-2147483648), -0);
assertEq((-2147483648) / (-1), 2147483648);
assertEq((-0) / (-2147483648), 0);

let ToLength = getSelfHostedValue('ToLength');

assertEq(ToLength(NaN), 0);
assertEq(ToLength(-0), 0);
assertEq(ToLength(0), 0);
assertEq(ToLength(-Infinity), 0);
assertEq(ToLength(-Math.pow(2, 31)), 0);

const MAX = Math.pow(2, 53) - 1;
assertEq(ToLength(Infinity), MAX);
assertEq(ToLength(MAX + 1), MAX);
assertEq(ToLength(3), 3);
assertEq(ToLength(40.5), 40);

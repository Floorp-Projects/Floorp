assertEq(Math.pow(1, undefined), NaN);
assertEq(Math.pow(1, null), 1);
assertEq(Math.pow(1, true), 1);
assertEq(Math.pow(1, false), 1);
assertEq(Math.pow(1, 0), 1);
assertEq(Math.pow(1, -0), 1);
assertEq(Math.pow(1, NaN), NaN);
assertEq(Math.pow(1, {}), NaN);
assertEq(Math.pow(1, {valueOf: function() { return undefined; }}), NaN);

x = 2.2;
assertEq(Math.pow(x - 1.2, undefined), NaN);

var y;
assertEq(Math.pow(1, y), NaN);


// Properties of Math.acosh that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.acosh(NaN), NaN);

// If x is less than 1, the result is NaN.
assertEq(Math.acosh(ONE_MINUS_EPSILON), NaN);
assertEq(Math.acosh(Number.MIN_VALUE), NaN);
assertEq(Math.acosh(+0), NaN);
assertEq(Math.acosh(-0), NaN);
assertEq(Math.acosh(-Number.MIN_VALUE), NaN);
assertEq(Math.acosh(-1), NaN);
assertEq(Math.acosh(-Number.MAX_VALUE), NaN);
assertEq(Math.acosh(-Infinity), NaN);

for (var i = -20; i < 1; i++)
    assertEq(Math.acosh(i), NaN);

// If x is 1, the result is +0.
assertEq(Math.acosh(1), +0);

// If x is +∞, the result is +∞.
assertEq(Math.acosh(Number.POSITIVE_INFINITY), Number.POSITIVE_INFINITY);


reportCompare(0, 0, "ok");

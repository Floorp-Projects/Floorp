// Properties of Math.atanh that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.atanh(NaN), NaN);

// If x is less than −1, the result is NaN.
assertEq(Math.atanh(-ONE_PLUS_EPSILON), NaN);
assertEq(Math.atanh(-Number.MAX_VALUE), NaN);
assertEq(Math.atanh(-Infinity), NaN);

for (var i = -5; i < -1; i += 0.1)
    assertEq(Math.atanh(i), NaN);

// If x is greater than 1, the result is NaN.
assertEq(Math.atanh(ONE_PLUS_EPSILON), NaN);
assertEq(Math.atanh(Number.MAX_VALUE), NaN);
assertEq(Math.atanh(Infinity), NaN);

for (var i = +5; i > +1; i -= 0.1)
    assertEq(Math.atanh(i), NaN);

// If x is −1, the result is −∞.
assertEq(Math.atanh(-1), -Infinity);

// If x is +1, the result is +∞.
assertEq(Math.atanh(+1), Infinity);

// If x is +0, the result is +0.
assertEq(Math.atanh(+0), +0);

// If x is −0, the result is −0.
assertEq(Math.atanh(-0), -0);


reportCompare(0, 0, "ok");

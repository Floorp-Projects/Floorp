// Properties of Math.log10 that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.log10(NaN), NaN);

// If x is less than 0, the result is NaN.
assertEq(Math.log10(-1e-10), NaN);
assertEq(Math.log10(-1e-5), NaN);
assertEq(Math.log10(-1e-1), NaN);
assertEq(Math.log10(-Number.MIN_VALUE), NaN);
assertEq(Math.log10(-Number.MAX_VALUE), NaN);
assertEq(Math.log10(-Infinity), NaN);

for (var i = -1; i > -10; i--)
    assertEq(Math.log10(i), NaN);

// If x is +0, the result is −∞.
assertEq(Math.log10(+0), -Infinity);

// If x is −0, the result is −∞.
assertEq(Math.log10(-0), -Infinity);

// If x is 1, the result is +0.
assertEq(Math.log10(1), +0);

// If x is +∞, the result is +∞.
assertEq(Math.log10(Infinity), Infinity);


reportCompare(0, 0, 'ok');

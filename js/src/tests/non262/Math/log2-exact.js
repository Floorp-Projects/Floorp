// Properties of Math.log2 that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.log2(NaN), NaN);

// If x is less than 0, the result is NaN.
assertEq(Math.log2(-1e-10), NaN);
assertEq(Math.log2(-1e-5), NaN);
assertEq(Math.log2(-1e-1), NaN);
assertEq(Math.log2(-Number.MIN_VALUE), NaN);
assertEq(Math.log2(-Number.MAX_VALUE), NaN);
assertEq(Math.log2(-Infinity), NaN);

for (var i = -1; i > -10; i--)
    assertEq(Math.log2(i), NaN);

// If x is +0, the result is −∞.
assertEq(Math.log2(+0), -Infinity);

// If x is −0, the result is −∞.
assertEq(Math.log2(-0), -Infinity);

// If x is 1, the result is +0.
assertEq(Math.log2(1), +0);

// If x is +∞, the result is +∞.
assertEq(Math.log2(Infinity), Infinity);


reportCompare(0, 0, 'ok');

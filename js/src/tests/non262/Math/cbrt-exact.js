// Properties of Math.cbrt that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.cbrt(NaN), NaN);

// If x is +0, the result is +0.
assertEq(Math.cbrt(+0), +0);

// If x is −0, the result is −0.
assertEq(Math.cbrt(-0), -0);

// If x is +∞, the result is +∞.
assertEq(Math.cbrt(Infinity), Infinity);

// If x is −∞, the result is −∞.
assertEq(Math.cbrt(-Infinity), -Infinity);


reportCompare(0, 0, "ok");

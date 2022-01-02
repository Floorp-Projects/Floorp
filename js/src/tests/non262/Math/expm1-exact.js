// Properties of Math.expm1 that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.expm1(NaN), NaN);

// If x is +0, the result is +0.
assertEq(Math.expm1(+0), +0);

// If x is −0, the result is −0.
assertEq(Math.expm1(-0), -0);

// If x is +∞, the result is +∞.
assertEq(Math.expm1(Infinity), Infinity);

// If x is −∞, the result is -1.
assertEq(Math.expm1(-Infinity), -1);


reportCompare(0, 0, "ok");


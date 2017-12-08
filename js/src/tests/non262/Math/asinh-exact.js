// Properties of Math.asinh that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.asinh(NaN), NaN);

// If x is +0, the result is +0.
assertEq(Math.asinh(+0), +0);

// If x is −0, the result is −0.
assertEq(Math.asinh(-0), -0);

// If x is +∞, the result is +∞.
assertEq(Math.asinh(Infinity), Infinity);

// If x is −∞, the result is −∞.
assertEq(Math.asinh(-Infinity), -Infinity);


reportCompare(0, 0, "ok");

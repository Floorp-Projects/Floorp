// Properties of Math.cosh that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.cosh(NaN), NaN);

// If x is +0, the result is 1.
assertEq(Math.cosh(+0), 1);

// If x is −0, the result is 1.
assertEq(Math.cosh(-0), 1);

// If x is +∞, the result is +∞.
assertEq(Math.cosh(Infinity), Infinity);

// If x is −∞, the result is +∞.
assertEq(Math.cosh(-Infinity), Infinity);


reportCompare(0, 0, "ok");

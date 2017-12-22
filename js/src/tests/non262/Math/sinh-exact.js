// Properties of Math.sinh that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.sinh(NaN), NaN);

// If x is +0, the result is +0.
assertEq(Math.sinh(+0), +0);

// If x is −0, the result is −0.
assertEq(Math.sinh(-0), -0);

// If x is +∞, the result is +∞.
assertEq(Math.sinh(Infinity), Infinity);

// If x is −∞, the result is −∞.
assertEq(Math.sinh(-Infinity), -Infinity);


reportCompare(0, 0, "ok");

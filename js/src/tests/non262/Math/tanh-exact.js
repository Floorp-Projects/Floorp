// Properties of Math.tanh that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.tanh(NaN), NaN);

// If x is +0, the result is +0.
assertEq(Math.tanh(+0), +0);

// If x is −0, the result is −0.
assertEq(Math.tanh(-0), -0);

// If x is +∞, the result is +1.
assertEq(Math.tanh(Number.POSITIVE_INFINITY), +1);

// If x is −∞, the result is -1.
assertEq(Math.tanh(Number.NEGATIVE_INFINITY), -1);


reportCompare(0, 0, "ok");

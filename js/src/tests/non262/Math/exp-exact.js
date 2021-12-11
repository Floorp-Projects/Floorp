// Properties of Math.exp that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assertEq(Math.exp(NaN), NaN);

// If x is +0, the result is 1.
assertEq(Math.exp(+0), 1);

// If x is −0, the result is 1.
assertEq(Math.exp(-0), 1);

// If x is +∞, the result is +∞.
assertEq(Math.exp(Infinity), Infinity);

// If x is −∞, the result is +0.
assertEq(Math.exp(-Infinity), +0);


// Not guaranteed by the specification, but generally assumed to hold.

// If x is 1, the result is Math.E.
assertEq(Math.exp(1), Math.E);

// If x is -1, the result is 1/Math.E.
assertEq(Math.exp(-1), 1 / Math.E);


if (typeof reportCompare === "function")
    reportCompare(0, 0);

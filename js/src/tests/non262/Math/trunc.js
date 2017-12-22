// If x is NaN, the result is NaN.
assertEq(Math.trunc(NaN), NaN);

// If x is −0, the result is −0.
assertEq(Math.trunc(-0), -0);

// If x is +0, the result is +0.
assertEq(Math.trunc(+0), +0);

// If x is +∞, the result is +∞.
assertEq(Math.trunc(Infinity), Infinity);

// If x is −∞, the result is −∞.
assertEq(Math.trunc(-Infinity), -Infinity);

// Other boundary cases.
var MAX_NONINTEGER_VALUE       = 4503599627370495.5;
var TRUNC_MAX_NONINTEGER_VALUE = 4503599627370495;

assertEq(Math.trunc(Number.MIN_VALUE), +0);
assertEq(Math.trunc(ONE_MINUS_EPSILON), +0);
assertEq(Math.trunc(ONE_PLUS_EPSILON), 1);
assertEq(Math.trunc(MAX_NONINTEGER_VALUE), TRUNC_MAX_NONINTEGER_VALUE);
assertEq(Math.trunc(Number.MAX_VALUE), Number.MAX_VALUE);

assertEq(Math.trunc(-Number.MIN_VALUE), -0);
assertEq(Math.trunc(-ONE_MINUS_EPSILON), -0);
assertEq(Math.trunc(-ONE_PLUS_EPSILON), -1);
assertEq(Math.trunc(-MAX_NONINTEGER_VALUE), -TRUNC_MAX_NONINTEGER_VALUE);
assertEq(Math.trunc(-Number.MAX_VALUE), -Number.MAX_VALUE);

// Other cases.
for (var i = 1, f = 1.1; i < 20; i++, f += 1.0)
    assertEq(Math.trunc(f), i);

for (var i = -1, f = -1.1; i > -20; i--, f -= 1.0)
    assertEq(Math.trunc(f), i);

assertEq(Math.trunc(1e40 + 0.5), 1e40);

assertEq(Math.trunc(1e300), 1e300);
assertEq(Math.trunc(-1e300), -1e300);
assertEq(Math.trunc(1e-300), 0);
assertEq(Math.trunc(-1e-300), -0);

assertEq(Math.trunc(+0.9999), +0);
assertEq(Math.trunc(-0.9999), -0);


reportCompare(0, 0, "ok");

// Properties of Math.hypot that are guaranteed by the spec.

// If no arguments are passed, the result is +0.
assertEq(Math.hypot(), +0);

// If any argument is +∞, the result is +∞.
// If any argument is −∞, the result is +∞.
for (var inf of [Infinity, -Infinity]) {
    assertEq(Math.hypot(inf, 0), Infinity);
    assertEq(Math.hypot(0, inf), Infinity);
    assertEq(Math.hypot(inf, inf), Infinity);
    assertEq(Math.hypot(inf, -inf), Infinity);

    assertEq(Math.hypot(inf, -0), Infinity);
    assertEq(Math.hypot(-0, inf), Infinity);
    assertEq(Math.hypot(inf, Math.MIN_VALUE), Infinity);
    assertEq(Math.hypot(Math.MIN_VALUE, inf), Infinity);
    assertEq(Math.hypot(inf, 1), Infinity);
    assertEq(Math.hypot(1, inf), Infinity);

    assertEq(Math.hypot(inf, 0, 0), Infinity);
    assertEq(Math.hypot(0, inf, 0), Infinity);
    assertEq(Math.hypot(0, 0, inf), Infinity);

    assertEq(Math.hypot(inf, NaN), Infinity);
    assertEq(Math.hypot(NaN, inf), Infinity);

    assertEq(Math.hypot(inf, NaN, NaN), Infinity);
    assertEq(Math.hypot(NaN, inf, NaN), Infinity);
    assertEq(Math.hypot(NaN, NaN, inf), Infinity);
}

// If no argument is +∞ or −∞, and any argument is NaN, the result is NaN.
assertEq(Math.hypot(NaN), NaN);

assertEq(Math.hypot(NaN, 0), NaN);
assertEq(Math.hypot(0, NaN), NaN);

assertEq(Math.hypot(NaN, NaN), NaN);

assertEq(Math.hypot(NaN, 0, 0), NaN);
assertEq(Math.hypot(0, NaN, 0), NaN);
assertEq(Math.hypot(0, 0, NaN), NaN);

assertEq(Math.hypot(Number.MAX_VALUE, Number.MIN_VALUE, NaN), NaN);

// If all arguments are either +0 or -0, the result is +0.
assertEq(Math.hypot(-0, -0), +0);
assertEq(Math.hypot(+0, -0), +0);

assertEq(Math.hypot(-0, -0, -0), +0);
assertEq(Math.hypot(+0, -0, -0), +0);
assertEq(Math.hypot(-0, +0, -0), +0);
assertEq(Math.hypot(+0, +0, -0), +0);

// The length property of the hypot function is 2.
assertEq(Math.hypot.length, 2);


reportCompare(0, 0, "ok");

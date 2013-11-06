for (var i = -20; i < 20; i++) {
    assertEq(Math.hypot(+0, i), Math.abs(i));
    assertEq(Math.hypot(-0, i), Math.abs(i));
}

// The implementation must avoid underlow.
// The implementation must avoid overflow, where possible.
// The implementation must minimise rounding errors.

assertNear(Math.hypot(1e-300, 1e-300), 1.414213562373095e-300);
assertNear(Math.hypot(1e-300, 1e-300, 1e-300), 1.732050807568877e-300);

assertNear(Math.hypot(1e-3, 1e-3, 1e-3), 0.0017320508075688772);

assertNear(Math.hypot(1e300, 1e300), 1.4142135623730952e+300);
assertNear(Math.hypot(1e100, 1e200, 1e300), 1e300);

assertNear(Math.hypot(1e3, 1e-3), 1000.0000000005);
assertNear(Math.hypot(1e-300, 1e300), 1e300);
assertNear(Math.hypot(1e3, 1e-3, 1e3, 1e-3), 1414.2135623738021555);

for (var i = 1, j = 1; i < 2; i += 0.05, j += 0.05)
    assertNear(Math.hypot(i, j), Math.sqrt(i * i + j * j));

reportCompare(0, 0, "ok");

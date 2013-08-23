// |reftest| skip
// Math.hypot is disabled pending the resolution of spec issues (bug 896264).

for (var i = -20; i < 20; i++)
    assertEq(Math.hypot(+0, i), Math.abs(i));

for (var i = -20; i < 20; i++)
    assertEq(Math.hypot(-0, i), Math.abs(i));

for (var i = 1, j = 1; i < 2; i += 0.05, j += 0.05)
    assertNear(Math.hypot(i, j), Math.sqrt(i * i + j * j));

assertNear(Math.hypot(1e300, 1e300), 1.4142135623730952e+300);
assertNear(Math.hypot(1e-300, 1e-300), 1.414213562373095e-300);
assertNear(Math.hypot(1e3, 1e-3), 1000.0000000005);

reportCompare(0, 0, "ok");

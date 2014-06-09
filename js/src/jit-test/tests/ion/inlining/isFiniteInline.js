/* Test inlining of Number.isFinite() */

for (var i = 0; i < 200000; i++) {
    assertEq(Number.isFinite(NaN), false);
    assertEq(Number.isFinite(-NaN), false);
    assertEq(Number.isFinite(+Infinity), false);
    assertEq(Number.isFinite(-Infinity), false);
    assertEq(Number.isFinite(3), true);
    assertEq(Number.isFinite(3.141592654), true);
    assertEq(Number.isFinite(+0), true);
    assertEq(Number.isFinite(-0), true);
    assertEq(Number.isFinite(-3), true);
    assertEq(Number.isFinite(-3.141592654), true);
    assertEq(Number.isFinite({}), false);
}

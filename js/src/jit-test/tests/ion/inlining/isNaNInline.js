/* Test inlining of Number.isNaN() */

for (var i = 0; i < 200000; i++) {
    assertEq(Number.isNaN(NaN), true);
    assertEq(Number.isNaN(-NaN), true);
    assertEq(Number.isNaN(+Infinity), false);
    assertEq(Number.isNaN(-Infinity), false);
    assertEq(Number.isNaN(3.14159), false);
    assertEq(Number.isNaN(-3.14159), false);
    assertEq(Number.isNaN(3), false);
    assertEq(Number.isNaN(-3), false);
    assertEq(Number.isNaN(+0), false);
    assertEq(Number.isNaN(-0), false);
    assertEq(Number.isNaN({}), false);
}

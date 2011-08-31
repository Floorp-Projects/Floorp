
assertEq(Math.round(3.14), 3);
assertEq(Math.round(0.5), 1);
assertEq(Math.round(-0), -0);
assertEq(Math.round(0), 0);
assertEq(Math.round(-1.03), -1);
assertEq(Math.round(2147483649), 2147483649);
assertEq(Math.round(2147483647.5), 2147483648);
assertEq(Math.floor(2147483647.1), 2147483647);

/* Inferred as round(double). */
function round1(x) {
    return Math.round(x);
}
assertEq(round1(10.3), 10);
assertEq(round1(-3.14), -3);
assertEq(round1(-3.5), -3);
assertEq(round1(-3.6), -4);
assertEq(round1(3.5), 4);
assertEq(round1(3.6), 4);
assertEq(round1(0), 0);
assertEq(round1(-0), -0); // recompile to return double
assertEq(round1(12345), 12345);
assertEq(round1(654.6), 655);

/* Inferred as round(double). */
function round2(x) {
    return Math.round(x);
}
assertEq(round2(1234.5), 1235);
assertEq(round2(NaN), NaN); // recompile to return double
assertEq(round2(4.6), 5);

/* Inferred as round(int). */
function round3(x) {
    return Math.round(x);
}
assertEq(round3(4), 4);
assertEq(round3(-5), -5);
assertEq(round3(0), 0);


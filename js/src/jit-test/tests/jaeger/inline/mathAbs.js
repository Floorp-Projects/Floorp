
assertEq(Math.abs(-10), 10);
assertEq(Math.abs(-2147483648), 2147483648);
assertEq(Math.abs(2147483648), 2147483648);
assertEq(Math.abs(-0), 0);
assertEq(Math.abs(0), 0);
assertEq(Math.abs(-3.14), 3.14);
assertEq(Math.abs(NaN), NaN);

/* Inferred as abs(int). */
function abs1(x) {
    return Math.abs(x);
}
assertEq(abs1(1), 1);
assertEq(abs1(-1), 1);
assertEq(abs1(0), 0);
assertEq(abs1(-123) + abs1(234), 357);
assertEq(abs1(-2147483648), 2147483648); // recompile to return double
assertEq(abs1(-2), 2);

/* Inferred as abs(double). */
function abs2(x) {
    return Math.abs(x);
}
assertEq(abs2(-2.2), 2.2);
assertEq(abs2(123), 123);
assertEq(abs2(-456), 456);
assertEq(abs2(-0), 0);
assertEq(abs2(1.3), 1.3);
assertEq(abs2(NaN), NaN);


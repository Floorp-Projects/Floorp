
assertEq(Math.floor(3.14), 3);
assertEq(Math.floor(-0), -0);
assertEq(Math.floor(0), 0);
assertEq(Math.floor(-1.23), -2);
assertEq(Math.floor(2147483649), 2147483649);
assertEq(Math.floor(2147483648.5), 2147483648);
assertEq(Math.floor(2147483647.1), 2147483647);

/* Inferred as floor(double). */
function floor1(x) {
    return Math.floor(x);
}
assertEq(floor1(10.3), 10);
assertEq(floor1(-3.14), -4);
assertEq(floor1(-0), -0); // recompile to return double
assertEq(floor1(678.3), 678);

/* Inferred as floor(double). */
function floor2(x) {
    return Math.floor(x);
}
assertEq(floor2(3.4), 3);
assertEq(floor2(NaN), NaN); // recompile to return double
assertEq(floor2(-4.4), -5);

/* Inferred as floor(int). */
function floor3(x) {
    return Math.floor(x);
}
assertEq(floor3(4), 4);
assertEq(floor3(-5), -5);
assertEq(floor3(0), 0);



assertEq(Math.sqrt(-Infinity), NaN);
assertEq(Math.sqrt(-3.14), NaN);
assertEq(Math.sqrt(-2), NaN);
assertEq(Math.sqrt(-0), -0);
assertEq(Math.sqrt(0), 0);
assertEq(Math.sqrt(2), Math.SQRT2);
assertEq(Math.sqrt(49), 7);
assertEq(Math.sqrt(Infinity), Infinity);

/* Inferred as sqrt(double). */
function sqrt1(x) {
    return Math.sqrt(x);
}
assertEq(sqrt1(NaN), NaN);
assertEq(sqrt1(-Infinity), NaN);
assertEq(sqrt1(Infinity), Infinity);
assertEq(sqrt1(-0), -0);
assertEq(sqrt1(2), Math.SQRT2);
assertEq(sqrt1(16), 4);

/* Inferred as sqrt(int). */
function sqrt2(x) {
    return Math.sqrt(x);
}
assertEq(sqrt2(4), 2);
assertEq(sqrt2(169), 13);
assertEq(sqrt2(0), 0);



assertEq(Math.pow(100, 2), 10000);
assertEq(Math.pow(-Infinity, -0.5), 0);
assertEq(Math.pow(-Infinity,  0.5), Infinity);
assertEq(Math.pow(Infinity, -0.5), 0);
assertEq(Math.pow(Infinity,  0.5), Infinity);
assertEq(Math.pow(NaN, -0.5), NaN);
assertEq(Math.pow(NaN,  0.5), NaN);
assertEq(Math.pow(-3.14, -0.5), NaN);
assertEq(Math.pow(-1.23,  0.5), NaN);
assertEq(Math.pow(-0, -0.5), Infinity);
assertEq(Math.pow(-0,  0.5), 0);
assertEq(Math.pow(-1, -0.5), NaN);
assertEq(Math.pow(-1,  0.5), NaN);
assertEq(Math.pow(0, -0.5), Infinity);
assertEq(Math.pow(0,  0.5), 0);
assertEq(Math.pow(1, -0.5), 1);
assertEq(Math.pow(1,  0.5), 1);
assertEq(Math.pow(100, -0.5), 0.1);
assertEq(Math.pow(100,  0.5), 10);

/* Inferred as pow(double, double). */
function pow1(x) {
    return Math.pow(x, 0.5);
}
assertEq(pow1(100), 10);
assertEq(pow1(144), 12);
assertEq(pow1(-0), 0);
assertEq(pow1(0), 0);
assertEq(pow1(1), 1);
assertEq(pow1(-1), NaN);
assertEq(pow1(NaN), NaN);
assertEq(pow1(-Infinity), Infinity);
assertEq(pow1(Infinity), Infinity);


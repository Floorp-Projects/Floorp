function f1(x) {
    return Math.round(x);
}
assertEq(f1(3.3), 3);
assertEq(f1(-2.842170943040401e-14), -0);

function f2(x) {
    return Math.round(x);
}
assertEq(f2(3.3), 3);
assertEq(f2(-1.3), -1);
assertEq(f2(-1.8), -2);
assertEq(f2(-0.9), -1);
assertEq(f2(-0.6), -1);
assertEq(f2(-0.4), -0);

function f3(x) {
    return Math.round(x);
}
assertEq(f3(0.1), 0);
assertEq(f3(-0.5), -0);

function f4(x) {
    return Math.round(x);
}
assertEq(f4(0.1), 0);
assertEq(f4(-0), -0);

function f5(x) {
    return Math.round(x);
}
assertEq(f5(2.9), 3);
assertEq(f5(NaN), NaN);

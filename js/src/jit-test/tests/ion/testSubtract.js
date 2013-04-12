// vim: set ts=4 sw=4 tw=99 et:
function f_int(x, y) {
    return x - y;
}
function f_double(x, y) {
    return x - y;
}

for (var i = 0; i < 1000; i++) {
    assertEq(f_int(5, 3), 2);
    assertEq(f_int(3, 5), -2);
    assertEq(f_int(-2147483648, 1), -2147483649);
}


for (var i = 0; i < 1000; i++) {
    assertEq(f_double(5.5, 3.2), 2.3);
    assertEq(f_double(2.5, 3.0), -0.5);
}


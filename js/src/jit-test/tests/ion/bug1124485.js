function f(x) {
    var y = Math.fround(x);
    assertEq(y, Math.pow(y, 1));
}

f(0);
f(2147483647);

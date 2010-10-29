function f(v, e) {
    for (var i = 0; i < 9; i++)
        v %= e;
    return v;
}
f(0, 1);
assertEq(f(-2, 2), -0);

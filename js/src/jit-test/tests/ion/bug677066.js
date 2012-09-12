function f0() {
    var v0 = 1;
    if (v0 | 0) {
    } else {
        v0 = v0 + v0;
    }
    return v0;
}
assertEq(f0(), 1);

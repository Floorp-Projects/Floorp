function f0() {
    var v0 = 5000;
    if (v0) {
        if (v0) {
            v0 = v0 * v0;
        } else {
            return;
        }
        v0 = v0 * v0;
    }
    return v0;
}
assertEq(f0(), 625000000000000);

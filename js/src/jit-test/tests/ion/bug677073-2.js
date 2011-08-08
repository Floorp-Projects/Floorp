// Don't assert.
function f0() {
    var v0;
    var v1;
    while (v1) {
        v1 = v0 + v1;
        v0 = v1 + v1;
    }
    return v0;
}
assertEq(f0(), undefined);

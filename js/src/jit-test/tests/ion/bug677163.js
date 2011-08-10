function f0() {
    var v0;
    do {
        v0 = (v0 & v0) + v0;
        continue;
    } while (v0);
}
assertEq(f0(), undefined);


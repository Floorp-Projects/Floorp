function f0(p0) {
    var v0 = 0;
    var v1;
    var v2;
    if (v1)
        v2 = v1 + v1;
    v1 | v2;
    if (v0) {
        while (p0)
            v2 = v1 + v2;
        v2 = v0;
    }
}
assertEq(f0(1), undefined);

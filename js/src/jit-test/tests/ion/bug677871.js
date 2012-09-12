function f0(p0,p1,p2,p3,p4,p5,p6) {
    var v0;
    var v1;
    if (v1) {
        do {
            v0 = v0 + p3;
            v1 = p3 + p3 + v1 + v0;
            if (v1) {
                v0 = v0 + p1 + p4 + p2 + p0 + v1;
                continue;
            }
            break;
        } while (v0);
    }
    v0 + v1;
}
assertEq(f0(1,2,3,4,5,6), undefined);


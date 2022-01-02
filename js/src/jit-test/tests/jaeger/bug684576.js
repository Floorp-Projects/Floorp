// |jit-test| error: TypeError
function f0(p0,p1) {
    var v3;
    do {
        p1 > v3
        v3=1.7
    } while (((p0[p1][5]==1)||(p0[p1][5]==2)||(p0[p1][5] == 3)) + 0 > p0);
    + (v3(f0));
}
f0(4105,8307);

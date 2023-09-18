// |jit-test| skip-if: getBuildConfiguration("wasi")
timeout(5);
function f0(p0,p1,p2,p3,p4,p5,p6,p7,p8) {
    var v0;
    p0;
    v0 = p8;
    if (v0) {
        v0;
        v0 = (p5 ^ (p2 ^ p6));
        if (p1) {
        } else {
            v0 = (((v0 & p6) ^ v0) + (v0 | p3));
        }
        ((v0 + v0) + v0);
        (v0 + ((p1 + ((v0 & v0) & p1)) & v0));
        p4;
        p2;
        v0 = v0;
    }
    p4;
    while ((v0 ^ p0)) {
        break;
        (v0 ^ (p1 + p4));
        continue;
        v0;
    }
    v0;
    do {
        continue;
        v0 = p5;
        break;
    } while (v0);
    v0 = v0;
}
f0(0,5695,59,475,4562,6803,6440,6004,0);


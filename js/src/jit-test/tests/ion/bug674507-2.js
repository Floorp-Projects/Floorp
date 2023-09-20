// |jit-test| skip-if: getBuildConfiguration()['wasi']
timeout(5);
function f0(p0,p1,p2,p3,p4,p5,p6,p7,p8,p9) {
    var v0;
    var v1;
    do {
        if (p1) {
            break;
            continue;
        } else {
        }
        v0 = (p0 | p7);
    } while (v0);
    if (((p5 + p3) + (p3 & (v0 | v0)))) {
        v1 = p6;
        v1 = p4;
        v0 = (v1 ^ v1);
        (v0 + ((v0 & p5) | v0));
    }
}
f0(2204,465,7905,3902,4658,4110,5703,2199,2681,5291);

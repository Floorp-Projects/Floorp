// |jit-test| skip-if: getBuildConfiguration("wasi")
timeout(1);
function f0(p0) {
    var v0;
    var v1;
    var v2;
    while (v0) {
        do {
            if (p0) {
                if (v0 ^ p0) {
                    v1 = v2;
                    continue;
                }
                break;
            }
        } while (p0);
    }
}
f0(0);

/* Don't assert */


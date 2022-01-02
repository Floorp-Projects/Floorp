// |jit-test| skip-if: getBuildConfiguration()['wasi']
timeout(5);
function f0() {
    var v0;
    v0 = v0;
    v0 = v0;
    v0;
    while ((v0 + v0)) {
        v0 = (v0 | v0);
    }
    v0 = (v0 + (v0 + ((v0 + ((v0 ^ v0) & (v0 | v0))) + v0)));
    while (v0) {
        v0 = v0;
        break;
    }
}
f0();


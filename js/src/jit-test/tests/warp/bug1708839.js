function f(a) {
    let m = new Uint32Array([-1]);
    let h = m[0];
    let r = m[0];
    if (a) {
        h = undefined;
        r = 0xff;
    }
    return h > r;
};

assertEq(f(false), false);
for (let i = 0; i < 100; ++i) {
    f(true);
}
assertEq(f(false), false);

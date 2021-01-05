function f(x, y) {
    (Math.log() ? 0 : Math.abs(~y)) ^ x ? x : x;
}
for (let i = 0; i < 52; i++) {
    f(0, -2147483649);
}

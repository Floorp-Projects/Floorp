// |jit-test| --fast-warmup
function main() {
    for (var i = 0; i < 100; i++) {
        Math.fround(~16);
        var v = Math.min(-9223372036854775808, -2.220446049250313e-16);
        v % v;
    }
}
main();

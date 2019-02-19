function getarg(x) {
    return arguments[x]
}
function f() {
    var r = 0n;
    for (var i=0;i<2000;++i) {
        r += getarg(1+(i & 1), BigInt(0), 1n);
    }
    return r;
}
for (var i=0;i<2;++i) print(f())

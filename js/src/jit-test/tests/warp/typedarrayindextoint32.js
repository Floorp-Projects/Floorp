function f(ta, i) {
    var x = i + 0.2;
    return ta[i] + ta[i | 0] + ta[x - 0.2];
}

var ta = new Int32Array(10);
var xs = [0, 1, 2, -1];
for (var i = 0; i < 100_000; ++i) {
    assertEq(f(ta, xs[i & 3]), (i & 3) == 3 ? NaN : 0);
}

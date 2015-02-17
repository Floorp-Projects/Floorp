function f(x) {
    return Math.round((x >>> 0) / 2) >> 0;
}
f(2);
assertEq(f(1), 1);

function g(x, y) {
    var x1 = 0;
    var x2 = Math.tan(y);
    var t1 = (x1 >>> 0);
    var t2 = (x2 >>> 0);
    var a = t1 / t2;
    var sub = 1 - a;
    var e = sub | 0;
    return e;
}
g(8, 4)
assertEq(g(1, 0), 0);

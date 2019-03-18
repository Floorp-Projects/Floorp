function f() {}
function g(x) {
    var a = 0, b = NaN, c = 1, d = 0, e = 0;
    a = (x >> 0);
    b = f();
    b = +b;
    c = Math.round(1);
    d = Math.imul(b, c);
    e = e + a;
    e = e + d;
    return e;
}
for (let i = 0; i < 2; ++i) {
    assertEq(g(), 0);
}

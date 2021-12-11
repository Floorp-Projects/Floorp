function f(a1, a2, a3, a4) {
}
function g(a1, a2) {
    var d = new Date(0);
    f();
    assertEq(typeof d, 'object');
}
g();
gc();
f(2, 2, 2, f(2, 2, 2, 12 === 12));
g(false, false);

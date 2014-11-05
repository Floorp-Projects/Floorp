function f(y) {
    var a = Math.fround(-0);
    var b = ~Math.hypot(y > 0, 5);
    assertEq(a, -0);
    assertEq(b, -6);
}
f(-0);
f(1);

function g(y, z) {
    if (z == 0) {
        var a = Math.fround(z);
        var b = ~Math.hypot(y > 0, 5);
        assertEq(a, -0);
        assertEq(b, -6);
    }
}
g(-0, -0);
g(1, -0);

function h(y, z) {
    if (z == -0) {
        var a = Math.fround(z);
        var b = ~Math.hypot(y > 0, 5);
        assertEq(a, -0);
        assertEq(b, -6);
    }
}
h(-0, -0);
h(1, -0);

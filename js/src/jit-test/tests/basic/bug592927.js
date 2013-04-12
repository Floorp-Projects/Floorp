// vim: set ts=8 sts=4 et sw=4 tw=99:
function f(x, y) {
    x(f);
    assertEq(y, "hello");
}

function g(x) {
    assertEq(x.arguments[1], "hello");
    x.arguments[1] = "bye";
    assertEq(x.arguments[1], "hello");
}

function f2(x, y) {
    arguments;
    x(f2);
    assertEq(y, "hello");
}

function g2(x) {
    assertEq(x.arguments[1], "hello");
    x.arguments[1] = "bye";
    assertEq(x.arguments[1], "hello");
}

f(g, "hello");
f2(g2, "hello");


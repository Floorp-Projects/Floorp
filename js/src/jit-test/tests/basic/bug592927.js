// vim: set ts=4 sw=4 tw=99 et:
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
    assertEq(y, "bye");
}

function g2(x) {
    assertEq(x.arguments[1], "hello");
    x.arguments[1] = "bye";
    assertEq(x.arguments[1], "bye");
}

f(g, "hello");
f2(g2, "hello");


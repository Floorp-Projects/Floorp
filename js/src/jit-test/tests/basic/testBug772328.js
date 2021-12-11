function f(x) {
    function x() {}
    arguments[0] = 42;
    return x;
}
assertEq(f(0), 42);

function g(x) {
    function x() {}
    assertEq(arguments[0], x);
}
g(0);

var caught = false;
try {
    (function h(x) { function x() {} }).blah.baz;
} catch (e) {
    caught = true;
}
assertEq(caught, true);

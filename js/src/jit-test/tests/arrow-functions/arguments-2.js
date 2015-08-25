// 'arguments' is lexically scoped in arrow functions

var args, g;
function f() {
    g = () => arguments;
    args = arguments;
}
f();
assertEq(g(), args);

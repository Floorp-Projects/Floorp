
/* Resolve 'arguments' and the name of the function itself in the presence of such local variables. */

function f() {
    return typeof arguments;
    function arguments() {
        return 7;
    }
}
assertEq(f(), "function");

function g() {
    var arguments = 0;
    return typeof arguments;
}
assertEq(g(), "number");

function h() {
    return typeof h;
    function h() {
        return 7;
    }
}
assertEq(h(), "function");

function i() {
    return typeof i;
    var i;
}
assertEq(i(), "undefined");

function j() {
    return typeof j;
}
assertEq(j(), "function");

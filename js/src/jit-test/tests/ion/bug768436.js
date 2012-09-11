function f() {
    return new ({});
}
function g() {
    return ({})();
}
try {
    f();
    assertEq(0, 1);
} catch (e) {
    assertEq(e instanceof TypeError, true);
}
try {
    g();
    assertEq(0, 1);
} catch (e) {
    assertEq(e instanceof TypeError, true);
}

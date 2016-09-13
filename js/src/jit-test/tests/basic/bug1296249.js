function f(x) {
    new Int32Array(x);
}
f(0);
try {
    f(2147483647);
} catch(e) {
    assertEq(e instanceof InternalError, true,
             "expected InternalError, instead threw: " + e);
}

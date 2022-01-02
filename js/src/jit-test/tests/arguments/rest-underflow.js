function f(a, b, c, ...rest) {
    assertEq(a, 1);
    assertEq(b, undefined);
    assertEq(c, undefined);
    assertEq(Array.isArray(rest), true);
    assertEq(rest.length, 0);
    assertEq(Object.getPrototypeOf(rest), Array.prototype);
}
f(1);

function f(x) {
    return Math.imul(1, x >>> 0) / 9 | 0;
}
function g(x) {
    return 1 * (x >>> 0) / 9 | 0;
}
function h(x) {
    return (x >>> 0) / 9 | 0;
}

assertEq(0, f(4294967296));
assertEq(-238609294, f(2147483648));

assertEq(0, g(4294967296));
assertEq(238609294, g(2147483648));

assertEq(0, h(4294967296));
assertEq(238609294, h(2147483648));

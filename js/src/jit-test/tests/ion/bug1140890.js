function isNegativeZero(x) {
    return x===0 && (1/x)===-Infinity;
}
function f(y) {
    return -(0 != 1 / y) - -Math.imul(1, !y)
}
x = [-0, Infinity]
for (var k = 0; k < 2; ++k) {
    assertEq(isNegativeZero(f(x[k])), false);
}


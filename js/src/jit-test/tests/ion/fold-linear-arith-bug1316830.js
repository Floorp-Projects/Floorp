function f(x) {
    return (x | 0) - (-4294967295 | 0) + -2147483647
}
x = [1, 4294967295]
for (var j = 0; j < 2; ++j) {
    for (var k = 0; k < 3; ++k) {
        assertEq(f(x[j]), -2147483647 - 2 * j);
    }
}

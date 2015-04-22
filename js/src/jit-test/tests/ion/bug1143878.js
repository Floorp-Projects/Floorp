function f(x, y) {
    if (!y)
        assertEq(x == x + x, false)
}
m = [true, null]
for (var j = 0; j < 2; ++j) {
    for (var k = 0; k < 2; ++k) {
        f(m[j], m[k])
    }
}

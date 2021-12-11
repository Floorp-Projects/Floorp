function f(y) {
    return ((y ? y : 0) ? 0 : y)
}
m = [0xf]
f(m[0])
assertEq(f(m[0]), 0)

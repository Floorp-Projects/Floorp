function f(x) {
        return Math.cos(~(~Math.pow(Number.MAX_VALUE, x)))
}
f(-0)
assertEq(f(undefined - undefined), 1)

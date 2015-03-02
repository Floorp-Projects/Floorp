function j(x) {
    return Math.pow(x, x) !== Math.pow(x, x)
}
j(-0)
assertEq(j(-undefined), true)

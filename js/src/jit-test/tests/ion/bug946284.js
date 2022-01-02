function f(x) {
    return x.length / 2
}
f("")
assertEq(f(undefined + ""), 4.5);

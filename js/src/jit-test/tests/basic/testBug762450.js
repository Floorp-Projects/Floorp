function f(a, b, c) {
    arguments[0] = 3;
    return (c--) + 1;
}
var r = f();
print(r);
assertEq(r !== r, true);

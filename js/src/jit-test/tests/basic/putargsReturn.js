function f(a) {
    x = arguments;
    return 99;
}
for (var i = 0; i < 9; i++)
    f(123);
assertEq(x[0], 123);

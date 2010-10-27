function f(a) {
    x = arguments;
}
for (var i = 0; i < 9; i++)
    f(123);
assertEq(x[0], 123);

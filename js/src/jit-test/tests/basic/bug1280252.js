function f() {
    x = arguments;
    delete x[1];
}
f(0, 1);
gc();
assertEq(x.length, 2);
assertEq(0 in x, true);
assertEq(1 in x, false);

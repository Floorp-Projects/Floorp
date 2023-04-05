function f(arg) {
    eval(`var y = 1; function f() {return y}; f();`);
    delete y;
    arguments[0] = 5;
    assertEq(arg, 5);
}
f(0);

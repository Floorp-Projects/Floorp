function f() {
    var x = -0;
    x++;
    if (3 > 2) {};
    var y = x + 2.14;
    assertEq(y, 3.14);
}
f();

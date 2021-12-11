function f() {
    var x = Object(2);
    var y = 3.14;
    assertEq(true && x < y, true);
}
f();

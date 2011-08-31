function f() {
    var x = Object.prototype.hasOwnProperty.call(1);
    assertEq(x, false);
    isNaN(2);
}
f();

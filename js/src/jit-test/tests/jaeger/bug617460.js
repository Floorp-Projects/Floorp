
function f() {
    var x = NaN;
    if (2 > 0) {}
    var y = {};
    var z = (1234 - x);
    y.foo = z;
    assertEq(x, NaN);
}
f();

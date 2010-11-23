
function f() {
    var i = 1000;
    var rest = i % 3;
    var div = (i - rest) / 3;
    assertEq(div, 333);
}
f();
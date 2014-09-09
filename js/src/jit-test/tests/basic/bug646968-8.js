var g = newGlobal();
var dbg = new g.Debugger(this);

var x = 5;
let (x = eval("x++")) {
    assertEq(evalInFrame(0, "x"), 5);
}
assertEq(x, 6);

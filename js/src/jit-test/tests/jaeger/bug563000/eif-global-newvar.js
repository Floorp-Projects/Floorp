var g = newGlobal();
var dbg = new g.Debugger(this);

function callee() {
  evalInFrame(1, "var x = 'success'");
}
callee();
assertEq(x, "success");

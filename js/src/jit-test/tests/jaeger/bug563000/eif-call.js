var g = newGlobal();
var dbg = new g.Debugger(this);

function callee() {
  evalInFrame(1, "x = 'success'");
}
function caller() {
  var x = "failure";
  callee();
  return x;
}
assertEq(caller(), "success");

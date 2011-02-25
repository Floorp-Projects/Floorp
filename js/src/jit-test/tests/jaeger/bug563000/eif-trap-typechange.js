// |jit-test| mjitalways;debug
setDebug(true);

function nop(){}
function caller(obj) {
  assertJit();
  var x = ({ dana : "zuul" });
  return x;
}
// 0 is the pc of "assertJit()", we want the pc of "return x", 2 lines below.
var pc = line2pc(caller, pc2line(caller, 0) + 2);
trap(caller, pc, "x = 'success'; nop()");
assertEq(caller(this), "success");

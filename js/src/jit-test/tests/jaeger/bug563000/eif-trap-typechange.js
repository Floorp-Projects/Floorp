// |jit-test| mjitalways;debug
setDebug(true);

function nop(){}
function caller(obj) {
  var x = ({ dana : "zuul" });
  return x;
}
// 0 is the pc of the first line, we want the pc of "return x", on the next line.
var pc = line2pc(caller, pc2line(caller, 0) + 1);
trap(caller, pc, "x = 'success'; nop()");
assertEq(caller(this), "success");

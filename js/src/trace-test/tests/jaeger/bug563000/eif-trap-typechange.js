setDebug(true);

function nop(){}
function caller(obj) {
  assertJit();
  var x = ({ dana : "zuul" });
  return x;
}
trap(caller, 22, "x = 'success'; nop()");
assertEq(caller(this), "success");

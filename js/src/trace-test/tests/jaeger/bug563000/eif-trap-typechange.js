setDebug(true);

function nop(){}
function caller(obj) {
  assertJit();
  var x = ({ dana : "zuul" });
  return x;
}
trap(caller, 20, "x = 'success'; nop()");
assertEq(caller(this), "success");

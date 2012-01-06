// |jit-test| mjitalways;debug
setDebug(true);

function nop(){}
function caller(obj) {
  assertJit();
  var x = "failure";
  return x;
}
trap(caller, 14, "x = 'success'; nop()");
assertEq(caller(this), "success");

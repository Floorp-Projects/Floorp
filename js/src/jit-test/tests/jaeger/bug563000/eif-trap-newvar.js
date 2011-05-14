// |jit-test| mjitalways;debug
setDebug(true);

function nop(){}
function caller(obj) {
  assertJit();
  return x;
}
trap(caller, 9, "var x = 'success'; nop()");
assertEq(caller(this), "success");

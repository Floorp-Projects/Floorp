// |jit-test| mjitalways;debug
setDebug(true);

function nop(){}
function caller(obj) {
  eval();
  return x;
}
trap(caller, 13, "var x = 'success'; nop()");
assertEq(caller(this), "success");

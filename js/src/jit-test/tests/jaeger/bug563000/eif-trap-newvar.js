// |jit-test| mjitalways;debug
setDebug(true);

function nop(){}
function caller(code, obj) {
  eval(code); // Make the compiler give up on binding analysis.
  return x;
}
trap(caller, 15, "var x = 'success'; nop()");
assertEq(caller("var y = 'ignominy'", this), "success");

// |jit-test| mjitalways;debug
setDebug(true);

function callee() {
  assertJit();
  evalInFrame(1, "var x = 'success'");
}
function caller(code) {
  assertJit();
  eval(code);
  callee();
  return x;
}
assertEq(caller('var y = "ignominy"'), "success");
assertEq(typeof x, "undefined");

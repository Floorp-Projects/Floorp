// |jit-test| mjitalways;debug
setDebug(true);

function callee() {
  assertJit();
  evalInFrame(1, "var x = 'success'");
}
function caller() {
  assertJit();
  callee();
  return x;
}
assertEq(caller(), "success");
assertEq(typeof x, "undefined");

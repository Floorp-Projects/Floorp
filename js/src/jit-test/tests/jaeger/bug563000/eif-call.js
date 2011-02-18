// |jit-test| mjitalways;debug
setDebug(true);

function callee() {
  assertJit();
  evalInFrame(1, "x = 'success'");
}
function caller() {
  assertJit();
  var x = "failure";
  callee();
  return x;
}
assertEq(caller(), "success");

// |jit-test| debug
setDebug(true);

function callee() {
  evalInFrame(1, "var x = 'success'");
}
callee();
assertEq(x, "success");

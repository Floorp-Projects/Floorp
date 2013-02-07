// |jit-test| mjitalways;debug
setDebug(true);

function callee() {
  evalInFrame(1, "x = 'success'");
}
function caller() {
  var x = ({ dana : "zuul" });
  callee();
  return x;
}
assertEq(caller(), "success");

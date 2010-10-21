setDebug(true);

function callee() {
  assertJit();
  evalInFrame(1, "x = 'success'");
}
function caller() {
  assertJit();
  var x = ({ dana : "zuul" });
  callee();
  return x;
}
assertEq(caller(), "success");

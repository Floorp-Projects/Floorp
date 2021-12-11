load(libdir + "evalInFrame.js");

function callee() {
  evalInFrame(1, "x = 'success'");
}
function caller() {
  var x = "failure";
  callee();
  return x;
}
assertEq(caller(), "success");

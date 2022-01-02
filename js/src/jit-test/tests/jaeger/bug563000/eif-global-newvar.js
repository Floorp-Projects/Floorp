load(libdir + "evalInFrame.js");

function callee() {
  evalInFrame(1, "var x = 'success'");
}
callee();
assertEq(x, "success");

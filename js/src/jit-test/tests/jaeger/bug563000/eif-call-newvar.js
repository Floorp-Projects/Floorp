load(libdir + "evalInFrame.js");

function callee() {
  evalInFrame(1, "var x = 'success'");
}
function caller(code) {
  eval(code);
  callee();
  return x;
}
assertEq(caller('var y = "ignominy"'), "success");
assertEq(typeof x, "undefined");

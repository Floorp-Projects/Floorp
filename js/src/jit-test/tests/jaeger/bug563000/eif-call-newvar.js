// |jit-test| mjitalways;debug
setDebug(true);

function callee() {
  var caught = false;
  try {
    // attempting to add a new binding to an existing scope is a dynamic error
    evalInFrame(1, "var y = 'success'");
  } catch (e) {
    assertEq(String.prototype.indexOf.call(e, "TypeError"), 0);
    caught = true;
  }
  assertEq(caught, true);
}
function caller(code) {
  eval(code);
  callee();
  return x;
}
assertEq(caller('var x = "success"'), "success");
assertEq(typeof x, "undefined");

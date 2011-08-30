// |jit-test| mjitalways;debug
setDebug(true);

this.__defineGetter__("someProperty", function () { evalInFrame(1, "var x = 'success'"); });
function caller(code, obj) {
  eval(code); // Make the compiler give up on binding analysis.
  obj.someProperty;
  return x;
}
assertEq(caller("var y = 'ignominy'", this), "success");

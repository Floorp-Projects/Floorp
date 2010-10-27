setDebug(true);

__defineGetter__("someProperty", function () { evalInFrame(1, "x = 'success'"); });
function caller(obj) {
  assertJit();
  var x = "failure";
  obj.someProperty;
  return x;
}
assertEq(caller(this), "success");

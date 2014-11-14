load(libdir + "evalInFrame.js");

this.__defineGetter__("someProperty", function () { evalInFrame(1, "var x = 'success'"); });
function caller(obj) {
  var x = ({ dana : 'zuul' });
  obj.someProperty;
  return x;
}
assertEq(caller(this), "success");

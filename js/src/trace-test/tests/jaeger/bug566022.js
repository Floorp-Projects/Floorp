function f() {
  var a;
  var o = { valueOf: function () { x = 99; return x; } };
  var x = 2;
  return [x, o + x, x]
}
assertEq(f().join(", "), "2, 101, 99");


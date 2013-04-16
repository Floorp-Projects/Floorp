// vim: set ts=8 sts=4 et sw=4 tw=99:
function f([a, b, c, d]) {
  a = b;
  return function () { return a + b + c + d; };
}

var F = f(["a", "b", "c", "d"]);
assertEq(F(), "bbcd");

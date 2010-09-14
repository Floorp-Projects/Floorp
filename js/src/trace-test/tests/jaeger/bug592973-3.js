// vim: set ts=4 sw=4 tw=99 et:
function f([a, b, c, d]) {
  a = b;
  return function () { return a + b + c + d; };
}

var F = f(["a", "b", "c", "d"]);
assertEq(F(), "bbcd");

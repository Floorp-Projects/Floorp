/* Don't crash. */
function foo(y) {
  var x = y;
  if (x != x)
    return true;
  return false;
}
assertEq(foo("three"), false);
assertEq(foo(NaN), true);

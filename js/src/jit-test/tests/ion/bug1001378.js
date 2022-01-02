// Test that we don't incorrectly optimize out argument slots from resume
// points.

function boo() {
  return foo.arguments[0];
}
function foo(a,b,c) {
  if (a == 0) {
    a ^= "";
    return boo();
  }
}
function inlined() {
  return foo.apply({}, arguments);
}
assertEq(inlined(1,2,3), undefined);
assertEq(inlined(0,2,3), 0);

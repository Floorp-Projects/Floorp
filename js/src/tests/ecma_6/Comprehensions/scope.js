// The identifier of a ComprehensionFor is only bound within its tail.

function t() {
  var x = [0, 1, 2];
  return [for (x of x) x*2]
}
assertDeepEq(t(), [0, 2, 4]);

// Each iteration should create a fresh binding.  Unfortunately this is
// not currently the case, but bug 449811 will fix this.
function t2() {
  var x = [0, 1, 2];
  return [for (x of x) ()=>x]
}
// FIXME: Should be [0, 1, 2].
assertDeepEq([for (x of t2()) x()], [2, 2, 2]);

reportCompare(null, null, "test");

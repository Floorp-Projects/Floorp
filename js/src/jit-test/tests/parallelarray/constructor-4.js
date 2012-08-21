
function buildPA() {
  // Construct copying from PA
  var p1 = new ParallelArray([1,2,3,4]);
  var p2 = new ParallelArray(p1);
  assertEq(p1.toString(), p2.toString());
  var p1d = new ParallelArray([2,2], function(i,j) { return i + j; });
  var p2d = new ParallelArray(p1d);
  assertEq(p1d.toString(), p2d.toString());
}

buildPA();

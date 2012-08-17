
function buildMultidim() {
  // 2D comprehension
  var p = new ParallelArray([2,2], function (i,j) { return i + j; });
  assertEq(p.shape.toString(), [2,2].toString());
  assertEq(p.toString(), "<<0,1>,<1,2>>");
}

buildMultidim();

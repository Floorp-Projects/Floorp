
function testFilterAll() {
  // Test filtering everything (leaving everything in)
  var p = new ParallelArray([0,1,2,3,4]);
  var all = p.map(function (i) { return true; });
  var r = p.filter(all);
  assertEq(r.toString(), "<0,1,2,3,4>");
  var p = new ParallelArray([5,2], function(i,j) { return i+j; });
  var r = p.filter(all);
  assertEq(r.toString(), "<<0,1>,<1,2>,<2,3>,<3,4>,<4,5>>");
}

testFilterAll();

load(libdir + "parallelarray-helpers.js");

function testFlatten() {
  var p0 = new ParallelArray([0,1]);
  var p1 = new ParallelArray([2,3]);
  var p = new ParallelArray([p0, p1]);
  var p2 = new ParallelArray([0,1,2,3]);
  assertEqParallelArray(p.flatten(), p2);

  // Test flatten crossing packed boundary with non-shape uniform elements
  var blah = new ParallelArray([2,2], function() { return 0; });
  var pp = new ParallelArray([p0, p1, blah]);
  var p2 = new ParallelArray([0,1,2,3,blah[0],blah[1]]);
  assertEqParallelArray(pp.flatten(), p2);

  var p0 = new ParallelArray([2,2], function() { return 1; });
  var p1 = new ParallelArray([2,2], function() { return 2; });
  var p = new ParallelArray([p0, p1]);
  var p2 = new ParallelArray([p0[0],p0[1],p1[0],p1[1]]);
  assertEqParallelArray(p.flatten(), p2);
}

// FIXME(bug 844991) logical shape not implemented
// testFlatten();

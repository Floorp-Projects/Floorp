load(libdir + "parallelarray-helpers.js");

function testElement() {
  // Test getting element from higher dimension
  var p = new ParallelArray([2,2,2], function () { return 0; });
  var p0 = new ParallelArray([2,2], function () { return 0; });
  assertEqParallelArray(p[0], p0);
  // Should create new wrapper
  assertEq(p[0] !== p[0], true);
  // Test out of bounds
  assertEq(p[42], undefined);
  // Test getting element from 0-lengthed higher dimension
  var pp = new ParallelArray([0,0], function() { return 0; });
  assertEq(p[2], undefined);
}

testElement();

load(libdir + "parallelarray-helpers.js");

function testFilterMisc() {
  var p = new ParallelArray([0,1,2]);
  // Test array
  var r = p.filter([true, false, true]);
  assertEqParallelArray(r, new ParallelArray([p[0], p[2]]));
  // Test array-like
  var r = p.filter({ 0: true, 1: false, 2: true, length: 3 });
  assertEqParallelArray(r, new ParallelArray([p[0], p[2]]));
  // Test truthy
  var r = p.filter([1, "", {}]);
  assertEqParallelArray(r, new ParallelArray([p[0], p[2]]));
}

testFilterMisc();

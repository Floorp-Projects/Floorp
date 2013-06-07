load(libdir + "parallelarray-helpers.js");

function test() {
  // Test what happens if the length of the array is very short (i.e.,
  // less than the number of cores).  There used to be a bug in this
  // case that led to crashes or other undefined behavior. Note that
  // we don't necessarily expected this to *parallelize*.
  var makeadd1 = function (v) { return [v]; }
  var array = range(1, 3);
  var expected = array.map(makeadd1);
  var actual = new ParallelArray(array).map(makeadd1);
  assertStructuralEq(expected, actual);
}

if (getBuildConfiguration().parallelJS)
  test();

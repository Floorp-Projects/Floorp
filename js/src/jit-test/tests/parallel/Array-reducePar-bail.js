load(libdir + "parallelarray-helpers.js");

// Tests that reduce saves its intermediate state correctly by
// inducing a bailout in the middle of the reduction.  This test may
// fail to test what it is intended to test if the wrong number of
// worker threads etc are present.  It reproduced an existing bug when
// used with 8 worker threads.

function testReduce() {
  var aCounter = 0;
  function sum(a, b) {
    var r = a + b;
    if (r == 234) // occurs once per slice
      aCounter++;
    return r;
  }

  // We use a big array, to make sure that the test runs with 64 slices.
  var array = build(8 * 4096, function() { return 1; });
  var seqResult = array.reduce(sum);
  var seqCounter = aCounter;

  aCounter = 0;
  var parResult = array.reducePar(sum);
  var parCounter = aCounter;

  assertEq(true, parCounter >= seqCounter);
  assertStructuralEq(parResult, seqResult);
}

if (getBuildConfiguration().parallelJS)
  testReduce();

load(libdir + "parallelarray-helpers.js");

function testReduce() {
  // This test is interesting because during warmup v*p remains an
  // integer but this ceases to be true once real execution proceeds.
  // By the end, it will just be infinity. Note that this is a case
  // where the non-commutative of floating point becomes relevant,
  // so we must use assertAlmostEq.
  function mul(v, p) { return v*p; }
  var array = range(1, 513);
  compareAgainstArray(array, "reduce", mul, assertAlmostEq);
}

if (getBuildConfiguration().parallelJS) testReduce();

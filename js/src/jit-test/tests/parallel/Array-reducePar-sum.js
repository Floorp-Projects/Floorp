load(libdir + "parallelarray-helpers.js");

function testReduce() {
  function sum(v, p) { return v+p; }
  assertArraySeqParResultsEq(range(1, 513), "reduce", sum);
}

if (getBuildConfiguration().parallelJS)
  testReduce();

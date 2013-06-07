load(libdir + "parallelarray-helpers.js");

function testReduce() {
  function sum(v, p) { return v+p; }
  compareAgainstArray(range(1, minItemsTestingThreshold+1), "reduce", sum);
}

if (getBuildConfiguration().parallelJS) testReduce();

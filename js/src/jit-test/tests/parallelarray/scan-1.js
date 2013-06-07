load(libdir + "parallelarray-helpers.js");
function sum(a, b) { return a+b; }
if (getBuildConfiguration().parallelJS)
  testScan(range(0, minItemsTestingThreshold), sum);

load(libdir + "parallelarray-helpers.js");
function sum(a, b) { return a+b; }
if (getBuildConfiguration().parallelJS)
  testArrayScanPar(range(1, 1024), sum);

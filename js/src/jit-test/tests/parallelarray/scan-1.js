load(libdir + "parallelarray-helpers.js");
function sum(a, b) { return a+b; }
testScan(range(1, 1024), sum);

load(libdir + "parallelarray-helpers.js");

function factorial(n) {
  if (n == 0)
    return 1;
  return n * factorial(n - 1);
}

if (getBuildConfiguration().parallelJS)
  assertArraySeqParResultsEq(range(0, 64), "map", factorial);

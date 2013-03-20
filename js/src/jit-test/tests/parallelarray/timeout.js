// |jit-test| exitstatus: 6;

load(libdir + "parallelarray-helpers.js");

function iterate(x) {
  while (x == 2046) {
    // for exactly one index, this infinitely loops!
    // this ensures that the warmup doesn't loop.
  }
  return 22;
}

timeout(1);
if (getBuildConfiguration().parallelJS)
  new ParallelArray([2048], iterate);
else
  while (true);

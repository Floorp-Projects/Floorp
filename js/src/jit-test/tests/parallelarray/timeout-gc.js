// |jit-test| exitstatus: 6;

// One sneaky way to run GC during a parallel section is to invoke the
// gc() function during the parallel timeout!

load(libdir + "parallelarray-helpers.js");

function iterate(x) {
  while (x == 2046) {
    // for exactly one index, this infinitely loops!
    // this ensures that the warmup doesn't loop.
  }
  return 22;
}

function timeoutfunc() {
  print("Timed out, invoking the GC");
  gc();
  return false;
}

timeout(1, timeoutfunc);

if (getBuildConfiguration().parallelJS)
  new ParallelArray([2048], iterate);
else
  while(true);

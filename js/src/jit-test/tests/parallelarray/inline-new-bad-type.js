// |jit-test| error: TypeError
//
// Run with --ion-eager.
if (getBuildConfiguration().parallelJS) {
  function TestCase(n, d, e, a) {};
  function reportCompare() {
    var testcase = new TestCase("x", 0);
  }
  reportCompare();
  TestCase = ParallelArray;
  gczeal(6);
  try {
    reportCompare();
  } catch(exc1) {}
  reportCompare();
} else {
  throw new TypeError();
}

// |jit-test| error: RangeError
// 
// Run with --ion-eager.
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

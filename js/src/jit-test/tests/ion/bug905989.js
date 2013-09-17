if (typeof ParallelArray === "undefined")
  quit();

function TestCase(n, d, e, a) {};
function reportCompare() {
  new TestCase();
}
reportCompare();
TestCase = ParallelArray;
try {
  reportCompare();
} catch(exc1) {}
reportCompare();

// |jit-test| error: ReferenceError
function TestCase(e, a)
  this.passed = (e == a);
function reportCompare (expected, actual) {
  var expected_t = typeof expected;
  var actual_t = typeof actual;
  if (expected_t != actual_t)
    printStatus();
  new TestCase(expected, actual);
}
var expect = '';
reportCompare(expect, '');
try {
  test();
} catch(exc1) {}
function test() {
  var { expect }  = '';
  for (var a = 1; a < 2; ++a)
    reportCompare(expect, '');
}
test();

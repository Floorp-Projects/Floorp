function TestCase(n, d, e, a) {}
function reportCompare (expected, actual, description) {
  var testcase = new TestCase("unknown-test-name", description, expected, actual);
}
var status = 'Testing scope after changing obj.__proto__';
function test() {
  let ( actual = [ ]  ) TestCase   .__proto__ = null;
  reportCompare (expect, actual, status);
}
var actual = 'error';
var expect = 'error';
for (i = 0; i < 12000; i++)  {
  test();
}

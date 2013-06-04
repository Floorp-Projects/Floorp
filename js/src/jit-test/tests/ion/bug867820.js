
function AddTestCase(expect, actual) {
  new TestCase(expect, actual);
}
function TestCase(e, a) {
  this.expect = e;
  getTestCaseResult(e, a);
}
function getTestCaseResult(expected, actual) {
  if (actual != expected) {}
}
AddRegExpCases(false, Math.pow(2,31));
AddRegExpCases("", Math.pow(2,30) - 1);
function AddRegExpCases(m, l) {
  AddTestCase("");
  AddTestCase(m, true);
  AddTestCase(l, 0);
}

function TestCase(n, d, e, a) {
  this.name = n;
  this.description = d;
  this.expect = e;
  this.actual = a;
  this.passed = getTestCaseResult(e, a);
  this.reason = '';
}
function getTestCaseResult(expected, actual) {}
var obj = new Function();
for (var i = 0; i < 100; i++) {
        obj['-1'] = new Array();
    assertEq(obj['-1'] == null, false);
    obj = new TestCase();
}

// Binary: cache/js-dbg-64-9cf3ea112635-linux
// Flags: --ion-eager
//

var callStack = new Array();
function TestCase(n, d, e, a) {
  this.expect = e;
  this.actual = a;
  this.passed = getTestCaseResult(e, a);
  dump(+ this.path + ' ' + 'reason: ' + toPrinted(this.reason)+ '\n');
};
function reportCompare (expected, actual, description) {
  var testcase = new TestCase("unknown-test-name", description, expected, actual);
}
function enterFunc (funcName) {
  callStack.push(funcName);
  var lastFunc = callStack.pop();
  reportCompare(funcName, lastFunc, "Test driver failure wrong exit function ");
}
try {
var summary = 'String static methods';
var actual = '';
expect = '2';
reportCompare(expect, actual, summary + " String.toUpperCase(new Boolean(true))");
} catch(exc0) {}
try {
function TestCase(n, d, e, a) {}
enterFunc ('test');
reportCompare(expect, actual, summary);
} catch(exc2) {}

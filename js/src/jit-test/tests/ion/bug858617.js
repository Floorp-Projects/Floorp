function TestCase(e, a) {
  getTestCaseResult(e, a);
};
function reportCompare (expected, actual) {
  new TestCase(expected, actual);
}
function enterFunc() {}
function getTestCaseResult(expected, actual) {
  return actual == expected;
}
reportCompare('', '');
evaluate("\
test();\
function test() {\
  enterFunc();\
  reportCompare();\
}\
");

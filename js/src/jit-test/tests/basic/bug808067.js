function TestCase(n, d, e, a)
  this.reason = '';
function reportCompare (expected, actual, description) {
  var output = "";
  var testcase = new TestCase("unknown-test-name", description, expected, actual);
  testcase.reason = output;
}
gcPreserveCode();
var summary = 'return with argument and lazy generator detection';
expect = "generator function foo returns a value";
actual = (function (j)  {}).message;
reportCompare(expect, actual, summary + ": 1");
reportCompare(expect, actual, summary + ": 2");
gcslice(1);
gcslice(2);
gc();
var strings = [ (0), ];
for (var i = 0; i < strings.length; i++)
  reportCompare(expect, actual, summary + (5e1) + strings[i]);

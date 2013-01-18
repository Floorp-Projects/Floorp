// |jit-test| error: TypeError
function TestCase(n, d, e, a) {}
function reportCompare (expected, actual, description) {
  var testcase = new TestCase();
}
eval("(function() { \
var summary = 'Do not hang/crash |for each| over object with getter set to map';\
var actual = 'No Crash';\
var expect = 'No Crash';\
reportCompare(expect, actual, summary);\
})();");
eval("(function() { TestCase = Object.prototype.toString; })();");
eval("gc(); reportCompare();");

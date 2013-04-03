function TestCase(n, d, e, a) {};
function reportCompare (expected, actual) {
    var testcase = new TestCase("unknown-test-name", null, expected, actual);
}
reportCompare();
var b = eval(uneval((TestCase)));
reportCompare(true, true);
expect = actual = ''
reportCompare(expect, actual);

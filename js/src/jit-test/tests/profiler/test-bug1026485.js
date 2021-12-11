
function TestCase(n, d, e, a) {
  return TestCase.prototype.dump = function () {};
}
enableGeckoProfiling();
new TestCase(typeof Number(new Number()));
new TestCase(typeof Number(new Number(Number.NaN)));
test();
function test() {
    try {
        test();
    } catch (e) {
        new TestCase();
    }
}

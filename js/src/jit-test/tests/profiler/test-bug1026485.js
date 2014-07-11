
function TestCase(n, d, e, a)
  TestCase.prototype.dump = function () {}
enableSPSProfiling();
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

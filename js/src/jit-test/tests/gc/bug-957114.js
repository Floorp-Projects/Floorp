gczeal(7,1);
function TestCase(n) {
  this.name = '';
  this.description = '';
  this.expect = '';
  this.actual = '';
  this.reason = '';
  this.passed = '';
}
function test() { return new TestCase; }
test();
Object.defineProperty(Object.prototype, "name", {});
test();

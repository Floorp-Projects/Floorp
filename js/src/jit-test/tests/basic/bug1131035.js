
function TestCase() {
  this.passed = 'x';
}
result = "pass";
for (var i = 0; i < 100; i++)
    new TestCase(result);
function Gen2(value) {}
Gen2.prototype = new TestCase();

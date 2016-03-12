load(libdir + "immutable-prototype.js");

function TestCase(n, d, e, a) {
  this.name=n;
  return n;
}

function reportCompare (expected, actual, description) {
  new TestCase
}

reportCompare(true, "isGenerator" in Function, "Function.prototype.isGenerator present");
var p = new Proxy({}, {});
function test() {
    TestCase.prototype.__proto__=null
    if (new TestCase)
        TestCase.prototype.__proto__=p
}
test();
new TestCase;
test()

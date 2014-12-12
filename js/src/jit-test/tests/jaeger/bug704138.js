function TestCase(n, d, e, a)
  this.name=n;
function reportCompare (expected, actual, description) {
  new TestCase
}
reportCompare(true, "isGenerator" in Function, "Function.prototype.isGenerator present");
var p = Proxy.create({
    has : function(id) {}
});
function test() {
    Object.prototype.__proto__=null
    if (new TestCase)
        Object.prototype.__proto__=p
}
test();
new TestCase;
test()

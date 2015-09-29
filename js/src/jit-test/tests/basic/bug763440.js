load(libdir + "immutable-prototype.js");

var summary = '';
var actual = '';
gcPreserveCode()
function TestCase(n, d, e, a) {
  this.name=n;
}
function reportCompare (expected, actual, description) {
  new TestCase
}
reportCompare(true, eval++, "Dummy description.");
var p = Proxy.create({
    has : function(id) {},
    set : function() {}
});
if (globalPrototypeChainIsMutable())
    Object.prototype.__proto__ = p;
new TestCase;
var expect = '';
reportCompare(expect, actual, summary);
gczeal(4);
try {
  evalcx(".");
} catch (e) {}
reportCompare(expect, actual, summary);

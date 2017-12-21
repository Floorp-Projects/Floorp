const Cu = Components.utils;
function run_test() {

  var chromeSB = new Cu.Sandbox(this);
  var contentSB = new Cu.Sandbox('http://www.example.com');
  Cu.evalInSandbox('this.foo = {a: 2}', chromeSB);
  contentSB.foo = chromeSB.foo;
  Assert.equal(Cu.evalInSandbox('foo.a', contentSB), undefined, "Default deny with no __exposedProps__");
  Cu.evalInSandbox('this.foo.__exposedProps__ = {a: "r"}', chromeSB);
  Assert.equal(Cu.evalInSandbox('foo.a', contentSB), undefined, "Still not allowed with __exposedProps__");
}

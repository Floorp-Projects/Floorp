var G = 3;

const Cu = Components.utils;

function run_test()
{
  let s = Cu.Sandbox(this, {sandboxPrototype: this, writeToGlobalPrototype: true});

  Cu.evalInSandbox("a = 3", s);
  Cu.evalInSandbox("var b = 3", s);
  Cu.evalInSandbox("const c = 3", s);
  Cu.evalInSandbox("this.d = 3", s);
  Cu.evalInSandbox("function e() { return 3; }", s);

  Assert.equal(Cu.evalInSandbox("a", s), 3);
  Assert.equal(Cu.evalInSandbox("b", s), 3);
  Assert.equal(Cu.evalInSandbox("c", s), 3);
  Assert.equal(Cu.evalInSandbox("d", s), 3);
  Assert.equal(Cu.evalInSandbox("e()", s), 3);

  Assert.equal(a, 3);
  Assert.equal(b, 3);
  // c is a lexical binding and does not write to the global prototype
  Assert.equal(d, 3);
  Assert.equal(e(), 3);

  a = 12;
  Assert.equal(Cu.evalInSandbox("a", s), 12);
  b = 12;
  Assert.equal(Cu.evalInSandbox("b", s), 12);
  d = 12;
  Assert.equal(Cu.evalInSandbox("d", s), 12);

  this.q = 3;
  Assert.equal(Cu.evalInSandbox("q", s), 3);
  Cu.evalInSandbox("q = 12", s);
  Assert.equal(q, 12);

  Assert.equal(Cu.evalInSandbox("G", s), 3);
  Cu.evalInSandbox("G = 12", s);
  Assert.equal(G, 12);

  Cu.evalInSandbox("Object.defineProperty(this, 'x', {enumerable: false, value: 3})", s);
  Assert.equal(Cu.evalInSandbox("x", s), 3);
  Assert.equal(x, 3);
  for (var p in this) {
    Assert.notEqual(p, "x");
  }

  Cu.evalInSandbox("Object.defineProperty(this, 'y', {get: function() { this.gotten = true; return 3; }})", s);
  Assert.equal(y, 3);
  Assert.equal(Cu.evalInSandbox("gotten", s), true);
  Assert.equal(gotten, true);

  Cu.evalInSandbox("this.gotten = false", s);
  Assert.equal(Cu.evalInSandbox("y", s), 3);
  Assert.equal(Cu.evalInSandbox("gotten", s), true);
  Assert.equal(gotten, true);

  Cu.evalInSandbox("Object.defineProperty(this, 'z', {get: function() { this.gotten = true; return 3; }, set: function(v) { this.setTo = v; }})", s);
  z = 12;
  Assert.equal(setTo, 12);
  Assert.equal(z, 3);
  Assert.equal(gotten, true);
  Assert.equal(Cu.evalInSandbox("gotten", s), true);
  gotten = false;
  Assert.equal(Cu.evalInSandbox("gotten", s), false);

  Cu.evalInSandbox("z = 20", s);
  Assert.equal(setTo, 20);
  Assert.equal(Cu.evalInSandbox("z", s), 3);
  Assert.equal(gotten, true);
  Assert.equal(Cu.evalInSandbox("gotten", s), true);
  gotten = false;
  Assert.equal(Cu.evalInSandbox("gotten", s), false);
}

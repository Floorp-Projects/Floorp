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

  do_check_eq(Cu.evalInSandbox("a", s), 3);
  do_check_eq(Cu.evalInSandbox("b", s), 3);
  do_check_eq(Cu.evalInSandbox("c", s), 3);
  do_check_eq(Cu.evalInSandbox("d", s), 3);
  do_check_eq(Cu.evalInSandbox("e()", s), 3);

  do_check_eq(a, 3);
  do_check_eq(b, 3);
  // c is a lexical binding and does not write to the global prototype
  do_check_eq(d, 3);
  do_check_eq(e(), 3);

  a = 12;
  do_check_eq(Cu.evalInSandbox("a", s), 12);
  b = 12;
  do_check_eq(Cu.evalInSandbox("b", s), 12);
  d = 12;
  do_check_eq(Cu.evalInSandbox("d", s), 12);

  this.q = 3;
  do_check_eq(Cu.evalInSandbox("q", s), 3);
  Cu.evalInSandbox("q = 12", s);
  do_check_eq(q, 12);

  do_check_eq(Cu.evalInSandbox("G", s), 3);
  Cu.evalInSandbox("G = 12", s);
  do_check_eq(G, 12);

  Cu.evalInSandbox("Object.defineProperty(this, 'x', {enumerable: false, value: 3})", s);
  do_check_eq(Cu.evalInSandbox("x", s), 3);
  do_check_eq(x, 3);
  for (var p in this) {
    do_check_neq(p, "x");
  }

  Cu.evalInSandbox("Object.defineProperty(this, 'y', {get: function() { this.gotten = true; return 3; }})", s);
  do_check_eq(y, 3);
  do_check_eq(Cu.evalInSandbox("gotten", s), true);
  do_check_eq(gotten, true);

  Cu.evalInSandbox("this.gotten = false", s);
  do_check_eq(Cu.evalInSandbox("y", s), 3);
  do_check_eq(Cu.evalInSandbox("gotten", s), true);
  do_check_eq(gotten, true);

  Cu.evalInSandbox("Object.defineProperty(this, 'z', {get: function() { this.gotten = true; return 3; }, set: function(v) { this.setTo = v; }})", s);
  z = 12;
  do_check_eq(setTo, 12);
  do_check_eq(z, 3);
  do_check_eq(gotten, true);
  do_check_eq(Cu.evalInSandbox("gotten", s), true);
  gotten = false;
  do_check_eq(Cu.evalInSandbox("gotten", s), false);

  Cu.evalInSandbox("z = 20", s);
  do_check_eq(setTo, 20);
  do_check_eq(Cu.evalInSandbox("z", s), 3);
  do_check_eq(gotten, true);
  do_check_eq(Cu.evalInSandbox("gotten", s), true);
  gotten = false;
  do_check_eq(Cu.evalInSandbox("gotten", s), false);
}

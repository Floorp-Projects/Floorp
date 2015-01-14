// We can find into and from optimized out scopes.

var g = newGlobal();
var dbg = new Debugger;
dbg.addDebuggee(g);

g.eval("" + function f() {
  var x = 42;
  function g() { }
  g();
});

dbg.onEnterFrame = function (f) {
  if (f.callee && (f.callee.name === "g")) {
    genv = f.environment.parent;
    assertEq(genv.optimizedOut, true);
    assertEq(genv.find("f").type, "object");
    assertEq(f.environment.find("x"), genv);
  }
}

g.f();

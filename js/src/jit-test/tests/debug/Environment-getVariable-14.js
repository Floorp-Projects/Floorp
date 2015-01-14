// Debugger.Environment can reflect optimized out function scopes

var g = newGlobal();
var dbg = new Debugger;
dbg.addDebuggee(g);

g.eval("" + function f() {
  var x = 42;
  function g() { }
  g();
});

dbg.onEnterFrame = function (f) {
  if (f.callee && (f.callee.name === "g"))
    assertEq(f.environment.parent.getVariable("x").optimizedOut, true);
}

g.f();

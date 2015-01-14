// Optimized out scopes should have working names().

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
    var names = f.environment.parent.names();
    assertEq(names.indexOf("x") !== -1, true);
    assertEq(names.indexOf("g") !== -1, true);
    assertEq(names.length, 3); // x,g,arguments
  }
}

g.f();

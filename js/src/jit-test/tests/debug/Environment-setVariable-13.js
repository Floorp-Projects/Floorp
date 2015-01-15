// Debugger.Environment should throw trying to setVariable on optimized out scope.

load(libdir + "asserts.js");

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
    assertThrowsInstanceOf(function () { f.environment.parent.setVariable("x", 43) }, ReferenceError);
}

g.f();

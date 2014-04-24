// Turning debugger off global at a time.

var g1 = newGlobal();
var g2 = newGlobal();
var g3 = newGlobal();

g1.eval("" + function f() {
  var name = "f";
  g();
  return name;
});
g2.eval("" + function g() {
  var name = "g";
  h();
  return name;
});
g3.eval("" + function h() {
  var name = "h";
  toggle();
  return name;
});

g1.g = g2.g;
g2.h = g3.h;

function name(f) {
  return f.environment.getVariable("name");
}

var dbg = new Debugger;
g3.toggle = function () {
  var frame;

  // Add all globals.
  dbg.addDebuggee(g1);
  dbg.addDebuggee(g3);
  dbg.addDebuggee(g2);

  // Remove one at a time.
  dbg.removeDebuggee(g3);
  assertEq(name(dbg.getNewestFrame()), "g");
  dbg.removeDebuggee(g2);
  assertEq(name(dbg.getNewestFrame()), "f");
  dbg.removeDebuggee(g1);
};

g1.eval("(" + function () { f(); } + ")();");


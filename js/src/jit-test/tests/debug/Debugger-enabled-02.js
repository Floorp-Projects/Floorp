// Tests that hooks work if set while the Debugger is disabled.

var g = newGlobal();
var dbg = new Debugger(g);
var log = "";

g.eval("" + function f() { return 42; });

dbg.enabled = false;
dbg.onEnterFrame = function (frame) {
  log += "1";
};
dbg.enabled = true;

g.f();

assertEq(log, "1");

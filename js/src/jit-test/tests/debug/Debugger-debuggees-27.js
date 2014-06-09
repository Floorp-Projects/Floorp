// Test that we can OSR with the same script on the stack multiple times.

var g = newGlobal();
var dbg = new Debugger;

g.toggle = function toggle() {
  dbg.addDebuggee(g);
  var frame = dbg.getNewestFrame();
}

g.eval("" + function f(x) {
  if (x == 0) {
    toggle();
    return;
  }
  f(x - 1);
});

g.eval("f(3);");

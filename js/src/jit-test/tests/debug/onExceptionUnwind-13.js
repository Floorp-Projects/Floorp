// |jit-test| error: 4
//
// Test that we can handle doing debug mode OSR from onExceptionUnwind when
// settling on a pc without a Baseline ICEntry.

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onExceptionUnwind = function () {};

g.eval("" + function f(y) {
  if (y > 0) {
    throw 4;
  }
});
g.eval("f(0)");
g.eval("f(1)");

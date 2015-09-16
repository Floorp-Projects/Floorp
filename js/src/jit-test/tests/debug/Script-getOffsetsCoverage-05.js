var g = newGlobal();
var dbg = Debugger(g);
function f(x) {
  while (x) {
    interruptIf(true);
    x -= 1;
  }
}
g.eval(f.toSource());

// Toogle the debugger while the function f is running.
setInterruptCallback(toogleDebugger);
function toogleDebugger() {
  dbg.enabled = !dbg.enabled;
  return true;
}

dbg.collectCoverageInfo = false;
dbg.enabled = false;
g.eval("f(10);");

dbg.collectCoverageInfo = true;
dbg.enabled = false;
g.eval("f(10);");

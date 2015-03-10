enableSPSProfiling();
try {
  // Only the ARM simulator supports single step profiling.
  enableSingleStepProfiling();
} catch (e) {
  quit(0);
}
var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {};
g.eval("var line = new Error().lineNumber; debugger;");

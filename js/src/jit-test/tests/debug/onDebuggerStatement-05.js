// For perf reasons we don't recompile all a debuggee global's scripts when
// Debugger no longer needs to observe all execution for that global. Test that
// things don't crash if we try to run a script with a BaselineScript that was
// compiled with debug instrumentation when the global is no longer a debuggee.

var g = newGlobal();
var dbg = new Debugger(g);
var counter = 0;
dbg.onDebuggerStatement = function (frame) {
  counter++;
  if (counter == 15)
    dbg.onDebuggerStatement = undefined;
};

g.eval("" + function f() {
  {
    let inner = 42;
    debugger;
    inner++;
  }
});
g.eval("for (var i = 0; i < 20; i++) f()");

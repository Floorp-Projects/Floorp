var g = newGlobal();
g.eval("function f() {\n" +
       "    debugger;\n" +
       "}\n")

var dbg = new Debugger(g);
var handler = {};
dbg.onDebuggerStatement = function (frame) {
  frame.script.setBreakpoint(0, {});
};

// create breakpoint
g.f()

// drop our references to things
handler = undefined;
dbg.onDebuggerStatement = undefined;

dbg.removeAllDebuggees();

gc();

//create garbage to trigger a minor GC
var x;
for (var i = 0; i < 100; ++i)
    x = {};

gc();

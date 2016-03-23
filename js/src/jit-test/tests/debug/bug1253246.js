var g = newGlobal();
var dbg = new Debugger(g);

dbg.onDebuggerStatement = (frame) => { frame.eval("c = 42;"); };
g.evalReturningScope("'use strict'; debugger;");

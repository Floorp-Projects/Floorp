fullcompartmentchecks(true);
var dbg = new Debugger();
var g = newGlobal();
g.eval("function f(){}");
dbg.addDebuggee(g);
dbg.findScripts();

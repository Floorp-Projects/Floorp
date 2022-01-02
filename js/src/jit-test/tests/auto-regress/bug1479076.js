fullcompartmentchecks(true);
var dbg = new Debugger();
var g = newGlobal({newCompartment: true});
g.eval("function f(){}");
dbg.addDebuggee(g);
dbg.findScripts();

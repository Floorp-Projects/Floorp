var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var g = newGlobal({newCompartment: true});
g.evaluate("function f(x) { return x + 1; }");
var gw = dbg.addDebuggee(g);
gczeal(2, 1);
var s = dbg.findScripts();
gczeal(0);
